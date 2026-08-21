// ============================================================================
// BrnTrafficEntityModule_wT1_05.cpp  --  wave T1 (parked traffic cars) ROUND 4, item 2:
// SCENE PRESENCE.
//
// TWO FUNCTIONS LIVE HERE:
//   TrafficEntityModule::CreateNewVehicleEntities @0x8272FA30   (DWARF :1317)
//   TrafficEntityModule::IsVehiclesParamAZombie   @0x82715D70   (DWARF :1323)
//
// ⭐⭐ WHAT THIS FILE IS FOR. Round 3 closed the boot (asserts=0, phase=DRIVING, the whole
// render trio ticking) and left exactly one number at zero: nothing was ever registered with
// the scene manager, so `[T1-rinfo] count = 0` and `[T1-dispatch] visible 0 -> kept 0` every
// frame. CreateNewVehicleEntities IS that registration: it walks the vehicles that are alive
// but have no entity yet, computes each one's bounding sphere from its vehicle-TYPE runtime
// record, and posts an AddEntity into the pre-scene output buffer's InSceneUpdateInterface.
// Without it a traffic car exists in the traffic module and nowhere else.
//
// ⚠️ THE CALLER LEG IS UN-GATED IN THE SAME CHANGE, ON PURPOSE. CreateNewVehicleEntities'
// ONLY caller in the whole image is PreSceneUpdate @0x8274A968's E_STATE_RUNNING arm
// (confirmed from its `xrefs_to`, which names exactly one function). Landing the body without
// un-gating that leg would be dead code that looks like progress -- the exact failure shape
// this wave keeps recording. See BrnTrafficEntityModule_wT1_02.cpp.
//
// ---------------------------------------------------------------------------
// LEDGER MISATTRIBUTION, FOR THE RECORD (second instance of the same family)
//
// Round 3 parked this function on two "third-order blockers". Both were false-blockers of the
// same kind round 3 itself diagnosed for FindVehicleTypeAttribKey_EXPENSIVE:
//   * Vehicle::SetHasEntity @0x8270EB38 -- "no declaration anywhere"; its identity.json row
//     carries the GameShared/GameClasses/Development/CgsStrStream.h CATCH-ALL primary_file.
//     Its four baked assert strings all cite BrnTrafficVehicle.h. Landed this round in
//     BrnTrafficVehicle.h/.cpp.
//   * IsVehiclesParamAZombie @0x82715D70 -- parked as needing "a FOURTH-order dependency,
//     Vehicle::GetCabIndex, which has no declaration anywhere in the tree". GetCabIndex
//     @0x8270E4C8 is FOURTEEN INSTRUCTIONS (one species assert, one `lhz 2(this)`) and is
//     DWARF-declared at BrnTrafficVehicle.h:339. Landed this round too, which retires the
//     blocker AND the two banners elsewhere in the tree that cite it
//     (BrnTrafficEntityModule_Render.cpp:317-319, WorldLinkStubs.cpp:734).
// Its other two callees resolve to committed accessors: sub_82707698 is `GetParam` (its
// assert is "luParam < KU_MAX_PARAMS" and its arithmetic `((luParam+1291)<<7)+this` is
// &maParams[luParam]), and sub_82707BC0 is `GetVehicle` (`((luIndex+85)<<7)+this` ==
// &maVehicles[luIndex], the same base/stride BrnTrafficEntityModule.cpp already documents).
// Nothing here is a stand-in.
//
// ---------------------------------------------------------------------------
// WHAT IS REAL IN CreateNewVehicleEntities: ALL OF IT. There is no named gate in this
// function. Every branch of the console body has a counterpart below.
//
// ⚠️ THE VMX OPERAND-ORDER TRAP, RESOLVED AND WRITTEN DOWN. The bounding-sphere CENTRE is
// built at 0x8272FF04..0x8272FF44 by three fused multiply-adds over the vehicle transform:
//     vspltw    v9, v0, 0 / v8, v0, 1 / v0, v0, 2     ; the three lanes of mBBoxOffset
//     lvx128    v12, r11, 0x30   (wAxis)   lvx128 v13, r0, r11 (xAxis)
//     vmaddfp   v13, v9, v12, v13
//     lvx128    v11, r11, 0x10   (yAxis)   lvx128 v10, r11, 0x20 (zAxis)
//     vmaddfp   v13, v8, v13, v11
//     vmaddfp128 v13, v0,  v10, v13
// IDA prints `vmaddfp` in RAW ENCODING ORDER (D, A, B, C) but `vmaddfp128` in MNEMONIC ORDER
// (D, A, C, B), and the machine operation is A*C + B in both. Reading every one of them the
// same way produces `x*wAxis + xAxis`, then `y*(that) + yAxis`, which is not a transform of
// anything. Reading them per their actual printing gives
//     t = x*xAxis + wAxis ; t = y*yAxis + t ; t = z*zAxis + t
// -- the standard affine transform-point, which is also what the DecFIGS scope tree says the
// source wrote (it names `Matrix44Affine lTranslate` + `rw::math::vpu::operator*`, i.e. a
// translate-by-mBBoxOffset matrix composed with the vehicle transform, of whose product only
// the translation row is consumed; the X360 folded the other three rows away as dead). It is
// therefore spelled below as the SDK's own TransformPoint, and the mixed IDA printing is
// recorded here so nobody "corrects" it back.
//
// THE OTHER FOUR THINGS WORTH KEEPING:
//   (a) 0x488 is the traffic scene entity-TYPE flag word (`li r29, 0x488`), the exact sibling
//       of the prop (0x490) and race-car (0x484) flags. Named in BrnTrafficConstants.h.
//   (b) The radius is Length(mBBoxHalfSize) -- vmsum3fp128 (a 3-LANE dot, not 4) then
//       vrsqrtefp with TWO Newton refinements, and a `vcmpeqfp`/`vsel` pair that returns
//       ZERO for a zero-length input rather than a NaN. This tree's rw::math::vpu::Magnitude
//       is that same guarded shape, so it is called rather than transcribed.
//   (c) The candidate set is `mAliveVehicles AND NOT mVehiclesWithEntities`, computed into a
//       LOCAL FastBitArray<601> pair the DWARF names lVehicles_NoEntity /
//       lVehicles_Alive_And_NoEntity (BrnTrafficEntityModule.cpp:4529/:4530) through
//       SetInverse/SetAnd -- which is why those two combiners were added to CgsFastBitArray.h
//       this round (see that header's own banner; purely additive).
//   (d) A ZOMBIE param's vehicle is SKIPPED, and that `continue` is load-bearing: without it
//       every zombie gets a scene entity that KillDyingVehicleEntity would then have to
//       remove, and the SetHasEntity assert pair would fire the moment the two disagree.
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
    // ------------------------------------------------------------------------------------
    // DELETE-WHEN-STABLE bring-up probe plumbing, gated on this wave's own BRN_TRAFFIC_DIAG
    // knob. Identical to the sibling partfiles'. [DIAG] NOT IN THE X360 BINARY.
    // ------------------------------------------------------------------------------------
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
// "Is the PARAM that owns this vehicle a zombie?" -- a zombie param is one whose slot has
// been handed to a new spawn while the old vehicle is still finishing (the divorce path
// StaticVehicles_Generate takes online). The pool a vehicle's param lives in is chosen purely
// by the vehicle index's RANGE, which is what GetVehicleSpecies encodes:
//
//   E_SPECIES_STANDARD -> GetParam(luVehicle)->IsZombie()
//                         (`(*(param + 0x40) >> 5) & 1`; 0x40 == Param::mxFlags, bit 5 ==
//                          Param::E_FLAG_ZOMBIE. Standard vehicle index == param index.)
//   E_SPECIES_STATIC   -> GetStaticTrafficParamFro(luVehicle)->IsZombie()
//                         (`(*(param + 3) >> 5) & 1`; 0x03 == StaticTrafficParam::mxFlags.
//                          The FULL-index accessor, i.e. it subtracts KU_STATIC_TRAFFIC_OFFSET
//                          itself -- which is why the raw luVehicle is passed.)
//   E_SPECIES_TRAILER  -> follow the trailer to its CAB and ask the cab's param instead
//                         (a trailer has no param of its own).
//
// The default arm is a StrStream assert ("Encountered traffic vehicle with unknown species: ")
// that returns FALSE. It is unreachable through GetVehicleSpecies, which is a total function
// over [0, 600) -- the console tests the value anyway because the species also arrives from
// serialised data elsewhere. Reproduced as the plain assert this tree's CGS_ASSERT gives; the
// streamed operands are cosmetic.
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
        // .cpp 4786 -- the console streams the species and the vehicle index into the
        // message buffer first; the condition is what matters.
        CGS_ASSERT(false, "Encountered traffic vehicle with unknown species");
        return false;
    }

    // Both the STANDARD arm and the TRAILER arm's cab land here (the console shares the tail
    // through LABEL_11), reading the lane-param pool.
    return GetParam(luParamVehicle)->IsZombie();
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::CreateNewVehicleEntities  @ 0x8272FA30   (.cpp 4525)
//
// Console body, statement for statement:
//   assert(lpOutput != NULL);                                        ; .cpp 4631
//   FastBitArray<601> lVehicles_NoEntity;
//   lVehicles_NoEntity.SetInverse(mVehicleSoaData.mVehiclesWithEntities);
//   FastBitArray<601> lVehicles_Alive_And_NoEntity;
//   lVehicles_Alive_And_NoEntity.SetAnd(mVehicleSoaData.mAliveVehicles, lVehicles_NoEntity);
//   for (it = lVehicles_Alive_And_NoEntity.Begin(); it != End(); ++it)
//   {
//       luVehicle = it.GetIndex();
//       assert(luVehicle < KU_MAX_TOTAL_TRAFFIC);                    ; header 2459
//       lpVehicle = GetVehicle(luVehicle);
//       assert(lpVehicle->IsAlive());                                ; .cpp 4647
//       assert(!lpVehicle->HasEntity());                             ; .cpp 4648
//       if (IsVehiclesParamAZombie(luVehicle))  continue;
//       assert(lpVehicle->IsAlive());                                ; BrnTrafficVehicle.h 786
//       lpType   = GetVehicleTypeRuntime(lpVehicle->GetVehicleType());
//       lfRadius = Magnitude(lpType->GetBBoxHalfSize());
//       assert(luVehicle < KU_MAX_TOTAL_TRAFFIC);                    ; header 2483
//       lCentre  = TransformPoint(maVehicleTransforms[luVehicle], lpType->GetBBoxOffset());
//       lId      = MakeTrafficEntityId(luVehicle);                   ; CgsEntityId.h 116 assert
//       lpOutput->GetSceneInputInterface()->AddEntity(lId, 0x488, lCentre, lfRadius);
//       lpVehicle->SetHasEntity(true, luVehicle, mVehicleSoaData);
//   }
//
// ⚠️ THE TWO BOUNDS ASSERTS ARE NOT DUPLICATES. Header line 2459 is GetVehicle's own bound
// (the pool accessor) and line 2483 is the vehicle-TRANSFORM accessor's -- two different
// inlined accessors, both bounded by KU_MAX_TOTAL_TRAFFIC. GetVehicle carries its own copy,
// so only the transform one is written explicitly here; the maVehicleTransforms read is
// direct because the console inlines GetVehicleTransform and no declaration for it exists in
// this tree (the same choice, for the same reason, that
// BrnTrafficEntityModule_Render.cpp:275-278 already documents).
//
// ⚠️ SetHasEntity IS INSIDE THE LOOP, AFTER AddEntity, AND IS NOT OPTIONAL. It is what makes
// the vehicle drop out of the candidate set next frame. Hoisting it, or dropping it because
// "AddEntity already happened", would re-register every traffic car every single frame and
// trip its own `HasEntity() != lbHasEntity` assert on the second visit.
//
// ⚠️ NO PER-FRAME POSITION LEG BELONGS HERE, and the question is settled by the asm rather
// than assumed. AddEntity's fourth argument IS the world-space bounding-sphere centre (the
// TransformPoint above), so the entity enters the scene already positioned -- which is why a
// PARKED car needs nothing further: its transform never changes after
// StaticVehicles_CreateNewVehicles seats it via SetVehicleTransform. The console's per-frame
// mover is a DIFFERENT function, GenerateSceneUpdateEvents, called from PostPhysicsUpdate's
// RUNNING arm (0x8274E5xx) and NOT from here; it is a named gate in
// BrnTrafficEntityModule_wT1_06.cpp because it is the DRIVING half (it posts one update per
// vehicle that MOVED). Landing it for wave 1 would post redundant no-op updates for stationary
// cars, i.e. cost with no observable difference. Recorded so the driving wave knows where the
// seam is.
// ----------------------------------------------------------------------------
void TrafficEntityModule::CreateNewVehicleEntities(BrnTrafficIO::OutputBuffer_PreScene* lpOutput)
{
    CGS_ASSERT(lpOutput != 0, "lpOutput != NULL");   // .cpp 4631

    // The candidate set: alive AND without an entity. Two locals, both DWARF-named.
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

        if (IsVehiclesParamAZombie(luVehicle))
        {
            continue;
        }

        CGS_ASSERT(lpVehicle->IsAlive(), "IsAlive()");        // BrnTrafficVehicle.h:786

        const VehicleTypeRuntime* lpVehicleTypeRuntime =
            GetVehicleTypeRuntime(lpVehicle->GetVehicleType());

        // vmsum3fp128 + rsqrt-with-two-Newton-steps + the vsel zero guard.
        const f32 lfRadius = rw::math::vpu::Magnitude(lpVehicleTypeRuntime->GetBBoxHalfSize());

        CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC");  // header 2483
        const Vector3 lCentre = rw::math::vpu::TransformPoint(
            maVehicleTransforms[luVehicle], lpVehicleTypeRuntime->GetBBoxOffset());

        // (luVehicle << 10) | 0x02000000, with the CgsEntityId.h:116 `luEntityIndex <
        // (1U << KU_NUM_BITS_FOR_ENTITY_NUM)` assert the console fires at 0x8272FF1C.
        const EntityId lTrafficEntityId = MakeTrafficEntityId(luVehicle);

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
            // [T1-scene] FIRST AddEntity, then a value-latched running total. This is THE
            // line that answers round 4's question: a non-zero total here is the number the
            // scene manager should hand back through [T1-rinfo] / [T1-dispatch].
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
