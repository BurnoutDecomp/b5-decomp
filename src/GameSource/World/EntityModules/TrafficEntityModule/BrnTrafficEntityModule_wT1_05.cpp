// ============================================================================
// BrnTrafficEntityModule_wT1_05.cpp -- traffic scene presence.
//
//   TrafficEntityModule::CreateNewVehicleEntities @0x8272FA30  (DWARF :1317)
//   TrafficEntityModule::IsVehiclesParamAZombie   @0x82715D70  (DWARF :1323)
//
// CreateNewVehicleEntities registers alive-but-entity-less vehicles with the scene manager:
// bounding sphere from the vehicle-TYPE runtime record, then AddEntity into the pre-scene
// output buffer. Its only caller in the image is PreSceneUpdate @0x8274A968's
// E_STATE_RUNNING arm. Neither function has a gate; every console branch is reproduced.
//
// NO PER-FRAME POSITION LEG BELONGS HERE. AddEntity's fourth argument is the world-space
// sphere centre, so an entity enters the scene already positioned and a parked car needs
// nothing more. The per-frame mover is GenerateSceneUpdateEvents, called from
// PostPhysicsUpdate's RUNNING arm (0x8274E5xx); it is gated in
// BrnTrafficEntityModule_wT1_06.cpp.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h"   // MakeTrafficEntityId, KU_TRAFFIC_SCENE_ENTITY_TYPE_FLAG
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficParam.h"       // Param::IsZombie
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficStaticParam.h" // StaticTrafficParam::IsZombie
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.h"     // Vehicle, GetVehicleSpecies
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicleTypeRuntime.h"

#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"

#include "rw/math/vpu/matrix44affine_operation.h"   // TransformPoint
#include "rw/math/vpu/vector3_operation.h"          // Magnitude

#include <cstdlib>   // getenv

namespace BrnTraffic
{
namespace
{
    // DELETE-WHEN-STABLE bring-up probe plumbing, gated on BRN_TRAFFIC_DIAG. Same as the
    // sibling partfiles'. [DIAG] NOT IN THE X360 BINARY.
    bool TrafficDiagEnabled()
    {
        static const bool sbEnabled = (getenv("BRN_TRAFFIC_DIAG") != 0);
        return sbEnabled;
    }

    CgsDev::Log::DebugPrint* TrafficDiagStream()
    {
        if (!TrafficDiagEnabled() || CgsDev::Log::gpDebugPrint == 0)
        {
            return 0;
        }
        return CgsDev::Log::gpDebugPrint;
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::IsVehiclesParamAZombie  @ 0x82715D70   (.cpp 4656)
//
// Is the param that owns this vehicle a zombie? A zombie param is one whose slot went to a
// new spawn while the old vehicle is still finishing. Which pool the param lives in follows
// from the vehicle index range, i.e. from GetVehicleSpecies:
//
//   E_SPECIES_STANDARD -> GetParam(luVehicle)->IsZombie()
//                         (`(*(param + 0x40) >> 5) & 1`; 0x40 == Param::mxFlags, bit 5 ==
//                          Param::E_FLAG_ZOMBIE. Standard vehicle index == param index.)
//   E_SPECIES_STATIC   -> GetStaticTrafficParamFro(luVehicle)->IsZombie()
//                         (`(*(param + 3) >> 5) & 1`; 0x03 == StaticTrafficParam::mxFlags.
//                          The full-index accessor subtracts KU_STATIC_TRAFFIC_OFFSET itself,
//                          so the raw luVehicle is what it wants.)
//   E_SPECIES_TRAILER  -> a trailer has no param, so ask its cab's param instead.
//
// The default arm asserts and returns false. GetVehicleSpecies is total over [0, 600), so it
// is unreachable from here; the console still tests because the species also arrives from
// serialised data. The console streams the species and index into the message buffer, which
// CGS_ASSERT does not do; only the condition matters.
// ----------------------------------------------------------------------------
bool TrafficEntityModule::IsVehiclesParamAZombie(u32 luVehicle)
{
    const Vehicle::Species leSpecies = GetVehicleSpecies(luVehicle);

    u32 luParamVehicle = luVehicle;

    if (leSpecies == Vehicle::E_SPECIES_STATIC)
    {
        return GetStaticTrafficParamFro(luVehicle)->IsZombie();
    }

    if (leSpecies == Vehicle::E_SPECIES_TRAILER)
    {
        const u32 luCab = GetVehicle(luVehicle)->GetCabIndex();
        CGS_ASSERT(luCab != KU_INVALID_VEHICLE, "luCab != KU_INVALID_VEHICLE");   // .cpp 4780
        luParamVehicle = luCab;
    }
    else if (leSpecies != Vehicle::E_SPECIES_STANDARD)
    {
        CGS_ASSERT(false, "Encountered traffic vehicle with unknown species");   // .cpp 4786
        return false;
    }

    // Both the STANDARD arm and the TRAILER arm's cab land here, reading the lane-param pool.
    return GetParam(luParamVehicle)->IsZombie();
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::CreateNewVehicleEntities  @ 0x8272FA30   (.cpp 4525)
//
// The two bounds asserts are not duplicates: header 2459 is GetVehicle's own bound and header
// 2483 belongs to the vehicle-TRANSFORM accessor. GetVehicle carries its copy, so only the
// transform one is written out here; maVehicleTransforms is read directly because the console
// inlines GetVehicleTransform and this tree has no declaration for it.
//
// SetHasEntity must stay inside the loop, after AddEntity. It is what drops the vehicle out of
// the candidate set next frame; hoist or drop it and every traffic car re-registers every
// frame, tripping its own `HasEntity() != lbHasEntity` assert on the second visit.
// ----------------------------------------------------------------------------
void TrafficEntityModule::CreateNewVehicleEntities(BrnTrafficIO::OutputBuffer_PreScene* lpOutput)
{
    CGS_ASSERT(lpOutput != 0, "lpOutput != NULL");   // .cpp 4631

    // Candidate set: alive AND without an entity. Both locals are DWARF-named
    // (BrnTrafficEntityModule.cpp:4529/:4530); SetInverse/SetAnd exist in CgsFastBitArray.h
    // for this pair.
    CgsContainers::FastBitArray<VehicleSoaData::KU_MAX_VEHICLES> lVehicles_NoEntity;
    lVehicles_NoEntity.SetInverse(mVehicleSoaData.mVehiclesWithEntities);

    CgsContainers::FastBitArray<VehicleSoaData::KU_MAX_VEHICLES> lVehicles_Alive_And_NoEntity;
    lVehicles_Alive_And_NoEntity.SetAnd(mVehicleSoaData.mAliveVehicles, lVehicles_NoEntity);

    u32 luRegistered = 0;

    for (CgsContainers::FastBitArray<VehicleSoaData::KU_MAX_VEHICLES>::Iterator lItVehicle =
             lVehicles_Alive_And_NoEntity.Begin();
         lItVehicle != lVehicles_Alive_And_NoEntity.End();
         ++lItVehicle)
    {
        const u32 luVehicle = static_cast<u32>(lItVehicle.GetIndex());

        Vehicle* lpVehicle = GetVehicle(luVehicle);          // carries header-2459's bound

        CGS_ASSERT(lpVehicle->IsAlive(), "lpVehicle->IsAlive()");        // .cpp 4647
        CGS_ASSERT(!lpVehicle->HasEntity(), "!lpVehicle->HasEntity()");  // .cpp 4648

        // Skipping zombies is load-bearing: give one an entity and KillDyingVehicleEntity has
        // to remove it again, and the SetHasEntity assert pair fires as soon as they disagree.
        if (IsVehiclesParamAZombie(luVehicle))
        {
            continue;
        }

        CGS_ASSERT(lpVehicle->IsAlive(), "IsAlive()");        // BrnTrafficVehicle.h:786

        const VehicleTypeRuntime* lpVehicleTypeRuntime =
            GetVehicleTypeRuntime(lpVehicle->GetVehicleType());

        // The console inlines a 3-lane vmsum3fp128 dot, rsqrt with two Newton steps, and a
        // vcmpeqfp/vsel guard returning zero (not NaN) for zero length. Magnitude is that shape.
        const f32 lfRadius = rw::math::vpu::Magnitude(lpVehicleTypeRuntime->GetBBoxHalfSize());

        CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC");  // header 2483

        // OPERAND ORDER, 0x8272FF04..0x8272FF44: IDA prints `vmaddfp` as (D, A, B, C) but
        // `vmaddfp128` as (D, A, C, B) while both compute A*C + B. Read per its own printing,
        // the three fmas are t = x*xAxis + wAxis, t = y*yAxis + t, t = z*zAxis + t, i.e. an
        // affine transform-point. Reading them uniformly gives nonsense; do not "correct" this.
        const Vector3 lCentre = rw::math::vpu::TransformPoint(
            maVehicleTransforms[luVehicle], lpVehicleTypeRuntime->GetBBoxOffset());

        // (luVehicle << 10) | 0x02000000. Carries the CgsEntityId.h:116 bound assert the
        // console fires at 0x8272FF1C.
        const EntityId lTrafficEntityId = MakeTrafficEntityId(luVehicle);

        // `li r29, 0x488` is the traffic entity-type flag, sibling of prop 0x490 and race-car
        // 0x484.
        lpOutput->GetSceneInputInterface()->AddEntity(
            CgsSceneManager::EntityId(lTrafficEntityId.muValue),
            KU_TRAFFIC_SCENE_ENTITY_TYPE_FLAG,
            lCentre,
            lfRadius);

        lpVehicle->SetHasEntity(true, luVehicle, mVehicleSoaData);
        ++luRegistered;
    }

    if (luRegistered != 0)
    {
        if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
        {
            // [T1-scene] first AddEntity, then a running total. A non-zero total here is the
            // count the scene manager should hand back through [T1-rinfo] / [T1-dispatch].
            // DELETE-WHEN-STABLE.
            static u32 suTotalRegistered = 0;
            const bool lbFirst = (suTotalRegistered == 0);
            suTotalRegistered += luRegistered;

            if (lbFirst)
            {
                *lpDiag << "[T1-scene] FIRST CreateNewVehicleEntities AddEntity: registered "
                        << static_cast<s32>(luRegistered)
                        << " traffic entities (flag 0x488)\n";
            }
            else
            {
                *lpDiag << "[T1-scene] CreateNewVehicleEntities registered "
                        << static_cast<s32>(luRegistered) << " (total "
                        << static_cast<s32>(suTotalRegistered) << ")\n";
            }
        }
    }
}

}
