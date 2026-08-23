// ============================================================================
// BrnTrafficEntityModule_wT4_01.cpp
//
//   TrafficEntityModule::UpdateCollidableVehicles @0x827302C8   (~1030 insns)
//
// THE COLLISION HALF OF THE SCENE REGISTRATION -- the wave-4 root break.
// CreateNewVehicleEntities registers a traffic car as a scene ENTITY; nothing in the tree
// gave it a collision VOLUME, so the broad phase never emitted a race-car-vs-traffic overlap
// pair, nothing ever asked for a promotion, and the race car drove straight through. This
// function is the only producer of mVehicleSoaData.mCollidableVehicles and the only caller of
// AddVolumeInstance / AddForCollision for a traffic vehicle.
//
// MOUNT REQUIRED (conductor-owned): add
//   echo "%SRC%\GameSource\World\EntityModules\TrafficEntityModule\BrnTrafficEntityModule_wT4_01.cpp"
// to tools/build/build_game_exe.bat beside the other TrafficEntityModule mounts, in the SAME
// change that retires the gate in _wT1_02.cpp, or the exe link fails with LNK2019.
//
// SHAPE, off the export. The console is VMX-heavy (vperm lane splices into a
// struct-of-arrays packet, vrlimi128 masks, a vrefp + one Newton-Raphson step for the
// reciprocal). It is written here as the plain scalar loop the wave brief permits: behaviour
// parity is the bar, mCachedCollidableList's only other readers are DebugComponent::
// DrawAvoidance and the avoidance steering, and every lane assignment below is transcribed
// from the vperm control-mask index (mask A == component x, B == y, C == z) rather than
// guessed.
//
// THE THREE .data CONSTANTS, dumped from their dyn-init thunks (a dyn-init `unk_` reads ZERO
// off the section -- the initialiser is the source of truth):
//   unk_8300CEF0 = kfVehicle_AvoidRadiusSq_CollideRadiusSq_MaxFloat_W, seeded @0x82C66318..
//                  0x82C6635C from { flt_820BA858, flt_8200889C, flt_820BA23C, flt_82001CC0 }
//                  == { 2500.0f, 400.0f, FLT_MAX, 0.0f } -- a 50 m AVOID radius and a 20 m
//                  COLLIDE radius. The console's own name for it comes from the baked assert
//                  string at .cpp 4953.
//   unk_8300C980 = splat(flt_82013F90) == 0.001f. NOT a radius (the wave brief's scout guessed
//                  "collidable radius^2"): it is the epsilon that decides whether a car's
//                  velocity is worth caching or whether its facing direction should stand in.
//   flt_82001CC0 = 0.0f (already attested in-tree).
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicleTypeRuntime.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"

#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"  // InSceneUpdateInterface
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"            // VolumeInstanceId
#include "GameShared/GameClasses/SceneManager/CgsVolumeId.h"                    // VolumeId

#include "rw/physics/rigidbody.h"                                              // rw::physics::ACTIVE_BODY
#include "rw/math/vpu/matrix44affine_operation.h"
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/vpu/vector4_operation.h"

#include <cstdlib>   // getenv

namespace BrnTraffic
{
namespace
{
    // ---- DELETE-WHEN-STABLE bring-up probe plumbing, gated on BRN_TRAFFIC_DIAG.
    // [DIAG] NOT IN THE X360 BINARY.
    bool TrafficDiagEnabled()
    {
        static s32 siCached = -1;
        if (siCached < 0)
        {
            const char* lpcEnv = getenv("BRN_TRAFFIC_DIAG");
            siCached = (lpcEnv != 0 && lpcEnv[0] != '0') ? 1 : 0;
        }
        return siCached != 0;
    }

    CgsDev::Log::DebugPrint* TrafficDiagStream()
    {
        if (!TrafficDiagEnabled() || CgsDev::Log::gpDebugPrint == 0)
        {
            return 0;
        }
        return CgsDev::Log::gpDebugPrint;
    }

    // NAMED LEG GATE -- same shape as the sibling partfiles'. [DIAG] NOT IN THE X360 BINARY.
    inline void LogMissingLeg(bool& lrbAlreadyLogged, const char* lpcLegNameAndReason)
    {
        if (lrbAlreadyLogged)
        {
            return;
        }
        lrbAlreadyLogged = true;

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[T4-traffic-leg] TrafficEntityModule leg NOT RECONSTRUCTED, skipped: "
                << lpcLegNameAndReason << " [FLAG PC partial gate]\n";
        }
    }

    // ---- the two .data constants (see the banner for their dyn-init provenance) ----------

    // unk_8300CEF0. Lane 0 AvoidRadiusSq (50 m), lane 1 CollideRadiusSq (20 m), lane 2 the
    // FLT_MAX the nearest-source search is seeded with, lane 3 unused. The console's spelling,
    // straight off the baked assert string.
    const Vector4 kfVehicle_AvoidRadiusSq_CollideRadiusSq_MaxFloat_W =
        { 2500.0f, 400.0f, 3.4028234663852886e+38f, 0.0f };

    // unk_8300C980 == splat(0.001f).
    const f32 KF_VEHICLE_MOTION_EPSILON = 0.001f;

    // The scene owner byte a traffic VolumeInstanceId carries (E_ENTITYTYPE_TRAFFIC == 2). The
    // console seeds the whole 64-bit id ONCE at the head with `li r10,1 ; extldi r10,r10,64,57`
    // == 1 << 57 == owner 2 in the embedded entity word's high byte, then splices only the
    // entity index per vehicle. Same value MakeTrafficEntityId produces, shifted into the high
    // dword -- which is what GenerateSceneUpdateEvents already posts transforms against.
    const u8 KU8_TRAFFIC_ENTITY_OWNER = 2;

    // AddForCollision's three literal arguments (`li r5,2 ; li r6,4 ; li r7,2`). Culling group
    // 2 == KU8_CULLING_GROUP_CARS; the body state is a LIVE body, not the props' FROZEN_BODY.
    const u32 KU_TRAFFIC_CULLING_GROUP = 2;

    // ---- lane helpers ---------------------------------------------------------------------
    //
    // The console's packet is a struct-of-arrays: each Vector4 member holds ONE field for FOUR
    // vehicles, spliced in with vperm through three 16-byte control masks indexed by lane
    // (unk_8327F140/150/160 + lane*64). Mask A selects component x, B y, C z -- read off the
    // race-car fill, where v120 (mPosition_X) takes mask A, v119 (mPosition_Y) mask B and v118
    // (mPosition_Z) mask C from the SAME source vector. mHalfLengths takes mask C (the box's z
    // half-extent == half LENGTH) and mHalfWidths mask A (x == half WIDTH).
    // One lane of a Vector4, by name rather than by pointer arithmetic (the host Vector4 is a
    // plain x/y/z/w record; indexing it as f32[4] would be a layout assumption this file does
    // not need to make).
    inline void SetLane(Vector4& lrVector, u32 luLane, f32 lfValue)
    {
        switch (luLane)
        {
        case 0:  lrVector.x = lfValue; break;
        case 1:  lrVector.y = lfValue; break;
        case 2:  lrVector.z = lfValue; break;
        default: lrVector.w = lfValue; break;
        }
    }

    inline void SetPacketLane(CollidableVehicleInfo4& lrPacket,
                              u32 luLane,
                              Vector3 lPosition,
                              Vector3 lMotion,
                              Vector3 lHalfExtent)
    {
        CGS_ASSERT(luLane < 4u, "luLane < 4");

        SetLane(lrPacket.mPosition_X, luLane, lPosition.x);
        SetLane(lrPacket.mPosition_Y, luLane, lPosition.y);
        SetLane(lrPacket.mPosition_Z, luLane, lPosition.z);

        SetLane(lrPacket.mLinearVelocity_X, luLane, lMotion.x);
        SetLane(lrPacket.mLinearVelocity_Y, luLane, lMotion.y);
        SetLane(lrPacket.mLinearVelocity_Z, luLane, lMotion.z);

        SetLane(lrPacket.mHalfLengths, luLane, lHalfExtent.z);
        SetLane(lrPacket.mHalfWidths,  luLane, lHalfExtent.x);
    }

    // The velocity-or-facing pick, 0x82730524..0x827305A8 (race cars) and
    // 0x82731E5C..0x82731EFC (traffic), identical in both: fabs() the candidate motion vector
    // (the console's `vandc` against the sign mask, with the w lane rotated in from x so the
    // 4-lane "none true" flag is meaningful over x/y/z only), compare against the 0.001f splat,
    // and fall back on the facing direction when the car is effectively stationary. A zero
    // motion lane would make every stopped car's avoidance prediction degenerate.
    // The console seeds the whole 64-bit id ONCE at the head (`li r10,1 ;
    // extldi r10,r10,64,57` == 1 << 57 == owner 2 in the embedded entity word's high byte)
    // and splices only the entity index per vehicle. Same value MakeTrafficEntityId produces
    // shifted into the high dword -- which is what GenerateSceneUpdateEvents already posts
    // SetVolumeInstanceTransform against, so the ADD here and the per-frame MOVE there agree.
    inline CgsSceneManager::VolumeInstanceId MakeTrafficVolumeInstanceId(u32 luVehicle)
    {
        CgsSceneManager::VolumeInstanceId lVolumeInstanceId;
        lVolumeInstanceId.muId =
            static_cast<u64>(KU8_TRAFFIC_ENTITY_OWNER)
            << (CgsSceneManager::VolumeInstanceId::KU_ENTITY_ID_START_INDEX
                + CgsSceneManager::VolumeInstanceId::KU_OWNER_BASE);
        lVolumeInstanceId.SetEntityIDEntityIndex(luVehicle);
        return lVolumeInstanceId;
    }

    inline Vector3 PickMotionLane(Vector3 lMotion, Vector3 lFacing)
    {
        const f32 lfAbsX = (lMotion.x < 0.0f) ? -lMotion.x : lMotion.x;
        const f32 lfAbsY = (lMotion.y < 0.0f) ? -lMotion.y : lMotion.y;
        const f32 lfAbsZ = (lMotion.z < 0.0f) ? -lMotion.z : lMotion.z;

        if (lfAbsX > KF_VEHICLE_MOTION_EPSILON ||
            lfAbsY > KF_VEHICLE_MOTION_EPSILON ||
            lfAbsZ > KF_VEHICLE_MOTION_EPSILON)
        {
            return lMotion;
        }
        return lFacing;
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateCollidableVehicles  @ 0x827302C8   (.cpp 4807)
//
// Three passes, in the console's order:
//   1. build the collision SOURCE list -- every ACTIVE race car plus every ALIVE PHYSICAL
//      traffic car -- into a 33-slot stack Array<Vector3,33> (8 race cars + the 25-slot
//      physical-traffic budget), accumulating their average into mAveragePhysicalCentre and
//      splicing each into the 4-wide mCachedCollidableList packets;
//   2. walk (mAliveVehicles & mVehiclesWithEntities) and, for the half of the pool
//      mVehiclesToUpdateCollidables selects this frame plus every physical car, find the
//      nearest source and classify: inside 50 m == AVOIDABLE (cached for the avoidance
//      steering), inside 20 m == COLLIDABLE (a real scene volume);
//   3. flip mVehiclesToUpdateCollidables so the OTHER half of the pool is re-evaluated next
//      frame -- Construct seeds its first 300 bits (_wT1_01.cpp:2294..:2298), so the wholesale
//      `~` at the tail is a two-frame amortisation, not an on/off toggle.
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdateCollidableVehicles(
        const BrnTrafficIO::InputBuffer_PreScene* lpInput,
        BrnTrafficIO::OutputBuffer_PreScene* lpOutput)
{
    CGS_ASSERT(lpInput != 0, "lpInput != NULL");     // 0x82730320, baked .cpp 4807
    CGS_ASSERT(lpOutput != 0, "lpOutput != NULL");   // 0x82730344, baked .cpp 4808

    if (lpInput == 0 || lpOutput == 0)   // PC-safety guard, as in the sibling partfiles
    {
        return;
    }

    {
        // GATE: the console's PerfMonCpu Start/StopMonitor(miPerfMon_UpdateCollidableVehicles)
        // bracket (0x82730300 / 0x827329C0). The handle is never issued because Construct's
        // twenty AddMonitor registrations are gated. DELETE WHEN those registrations land.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "UpdateCollidableVehicles PerfMonCpu Start/StopMonitor bracket -- the handle is "
            "never issued because Construct's AddMonitor registrations are gated");
    }

    typedef CgsContainers::FastBitArray<VehicleSoaData::KU_MAX_VEHICLES> TrafficBitArray;
    typedef BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface
            ActiveRaceCarOutputInterface;

    // 0x82730388 `stwx r20, this, 0x72340` -- the count word of mCachedCollidableList.
    // Reset order matters: Reset() SetFullCount()s this array (_wT1_01.cpp:2256), so the
    // Clear() has to run before any GetLength() below (CgsArray.h:336 fires on the -1
    // sentinel, not on 16).
    mCachedCollidableList.Clear();

    // ================================================================================
    // PASS 1 -- the collision SOURCE list.
    // ================================================================================

    // 0x82730390 `stvx128 v122(zero), r0, r18` where r18 == this+0x725D0.
    mAveragePhysicalCentre.SetZero();

    // The console's own 33-slot stack list (Array<rw::math::vpu::Vector3,33>, the committed
    // CgsArrayVpuVector3_33.cpp instantiation): 8 active race cars + the 25-slot physical
    // traffic budget.
    ::Array<Vector3, KU_MAX_PHYSICAL_TRAFFIC_VEHICLES + E_ACTIVE_RACE_CAR_INDEX_COUNT>
        lSourcePositions;
    lSourcePositions.Clear();

    Vector3 lSourceSum;
    lSourceSum.SetZero();
    f32 lfSourceCount = 0.0f;   // v127, incremented by a splatted 1.0f per source

    CollidableVehicleInfo4 lPacket = {};   // var_590..var_520, the 4-lane staging packet
    u32 luPacketLane = 0;                  // r16 -- counts SOURCES, lane == luPacketLane & 3

    const ActiveRaceCarOutputInterface* lpActiveRaceCars =
        lpInput->GetActiveRaceCarOutputInterface();
    CGS_ASSERT(lpActiveRaceCars != 0, "lpActiveRaceCarOutputInterface");   // baked .cpp 4833

    if (lpActiveRaceCars != 0)
    {
        for (s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar)
        {
            // The two bounds asserts the console folds in from BurnoutConstants.h:0x356/0x357.
            CGS_ASSERT(liCar >= 0, "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_INVALID");
            CGS_ASSERT(liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                       "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

            const EActiveRaceCarIndex leCar = static_cast<EActiveRaceCarIndex>(liCar);
            if (!lpActiveRaceCars->IsRaceCarActive(leCar))
            {
                continue;   // 0x827304B8: the lane counter does NOT advance for an idle slot
            }

            const BrnPhysics::Vehicle::RaceCarState* lpState =
                lpActiveRaceCars->GetRaceCarState(leCar);

            // RaceCarState displacements, all reached BY NAME: +0x220 mTransform.wAxis,
            // +0x210 mTransform.zAxis, +0x330 mLinearVelocity, +0x350 mHalfExtent.
            const Vector3 lPosition = lpState->mTransform.Pos();

            lSourcePositions.Append(lPosition);
            lSourceSum = lSourceSum + lPosition;
            lfSourceCount += 1.0f;

            SetPacketLane(lPacket,
                          luPacketLane & 3u,
                          lPosition,
                          PickMotionLane(lpState->mLinearVelocity, lpState->mTransform.At()),
                          lpState->mHalfExtent);

            if ((luPacketLane & 3u) == 3u &&
                mCachedCollidableList.GetLength() != KU_MAX_COLLIDABLE_CACHED_TRAFFIC_ARRAY)
            {
                mCachedCollidableList.Append(lPacket);   // 0x82730630
            }
            ++luPacketLane;
        }
    }

    // The ALIVE PHYSICAL traffic cars are collision sources too: a physical traffic car can
    // hit another traffic car, so the cars around it must be solid as well.
    // 0x82730668..0x827306A8 == `this[(0x505A+i)*8] & this[(0x5078+i)*8]`, soa+0 & soa+240.
    TrafficBitArray lPhysicalAlive;
    lPhysicalAlive.SetAnd(mVehicleSoaData.mAliveVehicles, mVehicleSoaData.mPhysicalVehicles);

    for (TrafficBitArray::Iterator lIt = lPhysicalAlive.Begin();
         lIt != lPhysicalAlive.End();
         ++lIt)
    {
        const u32 luVehicle = static_cast<u32>(lIt.GetIndex());
        CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC");

        // 0x827308D4: this + luVehicle*64 + 0x1ECB0 == maVehicleTransforms[i].wAxis.
        const Vector3 lPosition = GetVehicleTransform(luVehicle).Pos();

        lSourcePositions.Append(lPosition);
        lSourceSum = lSourceSum + lPosition;
        lfSourceCount += 1.0f;
    }

    // 0x82730C80..0x82730CAC: vrefp + one Newton-Raphson step == the reciprocal of the source
    // count, times the accumulated sum.
    // BEHAVIOUR DELTA, deliberate: the console does NOT guard the zero case, so with no active
    // race car and no physical traffic it writes a NaN into mAveragePhysicalCentre. Guarded
    // here -- a NaN centre would propagate silently into every consumer of the member, and
    // "leave the placeholder zero" is the safe reading of an unreachable console path.
    if (lfSourceCount > 0.0f)
    {
        mAveragePhysicalCentre = lSourceSum * (1.0f / lfSourceCount);
    }

    {
        // GATE: the third source, 0x82730CB0..0x82730CE0 -- mCameraLastFrame's Pos row
        // (this+0x728C0) appended when bit 27 of the selector word at this+0x729D4 ==
        // mCameraLastFrame+0x144 is set. BLOCKER: that word is mCameraLastFrame's CameraState
        // current-flag set and the flag's MEANING is unnamed, exactly as the sibling DEBUG
        // sim-centre gate at _wT1_01.cpp:1376 already records. Omitting it can only make FEWER
        // cars collidable, and never the ones near a race car. DELETE-WHEN the flag is named.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "UpdateCollidableVehicles camera collision-source @0x82730CB0 -- the selector bit "
            "is mCameraLastFrame+0x144 bit 27, an unnamed CameraState flag (same blocker as "
            "the DEBUG sim-centre overrides). Race-car and physical-traffic sources are LIVE");
    }

    // ================================================================================
    // PASS 2 -- classify every alive vehicle that owns a scene entity.
    // ================================================================================

    // 0x82730CE4..0x82730D1C == `this[(0x5064+i)*8] & this[(0x505A+i)*8]`, soa+80 & soa+0.
    TrafficBitArray lAliveWithEntities;
    lAliveWithEntities.SetAnd(mVehicleSoaData.mVehiclesWithEntities,
                              mVehicleSoaData.mAliveVehicles);

    // 0x82730D20..0x82730D88 builds the candidate set as a third stack bit array,
    //   (mVehiclesToUpdateCollidables & mVehiclesWithEntities & mAliveVehicles) | (alive & physical)
    // and then walks (alive & withEntities). The `& lAliveWithEntities` term is redundant with
    // the walk itself, so the two remaining terms are tested per vehicle below instead of
    // materialising the array -- value-identical, and it sidesteps the fact that
    // mVehiclesToUpdateCollidables is a FastBitArray<600> while the SoA sets are
    // FastBitArray<601> (same ten fields, different C++ types).

    // ------------------------------------------------------------------------------------
    // [PC SAFETY] NOT IN THE X360 BINARY. Retire the collision volume of any vehicle that has
    // DIED while collidable.
    //
    // Vehicle::SetDead masks mxFlags with 0xDE -- it clears ALIVE and ORPHAN and deliberately
    // leaves E_FLAG_COLLIDABLE and the SoA bit alone, because on the console the remove half
    // (KillDyingVehicleEntities @0x82741E40 / RemoveVehicle @0x8272E370) tears the scene
    // registration down. BOTH are gated in this tree, and the main walk below only visits
    // (alive & withEntities), so a killed driving-traffic car would keep a live AddForCollision
    // registration at its last position for the rest of the session -- an invisible solid car.
    // KillParam (_wT2_01.cpp:653) reaches SetDead on a normal drive, so this is reachable, not
    // theoretical.
    // DELETE-WHEN KillDyingVehicleEntities or RemoveVehicle lands.
    // ------------------------------------------------------------------------------------
    {
        TrafficBitArray lStaleCollidable;
        lStaleCollidable.SetInverse(mVehicleSoaData.mAliveVehicles);
        lStaleCollidable.SetAnd(lStaleCollidable, mVehicleSoaData.mCollidableVehicles);

        for (TrafficBitArray::Iterator lIt = lStaleCollidable.Begin();
             lIt != lStaleCollidable.End();
             ++lIt)
        {
            const u32 luVehicle = static_cast<u32>(lIt.GetIndex());
            if (luVehicle >= KU_MAX_TOTAL_TRAFFIC)
            {
                continue;
            }

            const CgsSceneManager::VolumeInstanceId lVolumeInstanceId =
                MakeTrafficVolumeInstanceId(luVehicle);
            lpOutput->GetSceneInputInterface()->RemoveForCollision(lVolumeInstanceId);
            lpOutput->GetSceneInputInterface()->RemoveVolumeInstance(lVolumeInstanceId);
            GetVehicle(luVehicle)->SetCollidable(false, lIt, mVehicleSoaData);

            if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
            {
                static bool sbLogged = false;
                if (!sbLogged)
                {
                    sbLogged = true;
                    *lpDiag << "[T4-collide] FIRST stale collision volume retired for DEAD "
                               "vehicle " << static_cast<s32>(luVehicle)
                            << " -- the console's remove half is gated [DELETE-WHEN-STABLE]\n";
                }
            }
        }
    }

    u32 luDiagTurnedCollidable = 0;   // [T4-collide] DIAG

    for (TrafficBitArray::Iterator lIt = lAliveWithEntities.Begin();
         lIt != lAliveWithEntities.End();
         ++lIt)
    {
        const u32 luVehicle = static_cast<u32>(lIt.GetIndex());
        CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC");

        bool lbCollidable = false;   // r15
        bool lbAvoidable  = false;   // r25
        bool lbCandidate  = false;   // r16

        const bool lbPhysical = mVehicleSoaData.mPhysicalVehicles.IsBitSet(luVehicle);

        if (mVehiclesToUpdateCollidables.IsBitSet(luVehicle) || lbPhysical)
        {
            lbCandidate = true;

            if (lbPhysical)
            {
                // 0x827318F4: a physical car is unconditionally both, no distance test.
                lbCollidable = true;
                lbAvoidable  = true;
                mVehiclesAvoidableLastFrame.SetBit(luVehicle);
            }
            else
            {
                // 0x82731634..0x827318E8. Nearest source, seeded with the constant's FLT_MAX
                // lane so an empty source list leaves every car non-collidable.
                const Vector3 lPosition = GetVehicleTransform(luVehicle).Pos();

                f32 lfNearestSq = kfVehicle_AvoidRadiusSq_CollideRadiusSq_MaxFloat_W.z;
                for (u32 luSource = 0; luSource < lSourcePositions.GetLength(); ++luSource)
                {
                    const Vector3 lToSource = lPosition - lSourcePositions[luSource];
                    const f32 lfDistSq = rw::math::vpu::Dot(lToSource, lToSource);
                    if (lfDistSq < lfNearestSq)
                    {
                        lfNearestSq = lfDistSq;
                    }
                }

                if (kfVehicle_AvoidRadiusSq_CollideRadiusSq_MaxFloat_W.x > lfNearestSq)
                {
                    lbAvoidable = true;
                    mVehiclesAvoidableLastFrame.SetBit(luVehicle);

                    CGS_ASSERT(kfVehicle_AvoidRadiusSq_CollideRadiusSq_MaxFloat_W.y <
                               kfVehicle_AvoidRadiusSq_CollideRadiusSq_MaxFloat_W.x,
                               "kfVehicle_AvoidRadiusSq_CollideRadiusSq_MaxFloat_W[1] < "
                               "kfVehicle_AvoidRadiusSq_CollideRadiusSq_MaxFloat_W[0]");  // .cpp 4953

                    if (kfVehicle_AvoidRadiusSq_CollideRadiusSq_MaxFloat_W.y > lfNearestSq)
                    {
                        lbCollidable = true;   // 0x827318EC
                    }
                }
            }
        }

        // 0x82731D20: hidden traffic is never solid and never cached.
        if (mbTrafficIsHidden)
        {
            lbAvoidable  = false;
            lbCollidable = false;
        }

        // 0x82731D58: this + (luVehicle + 0x55)*128 == &maVehicles[luVehicle].
        Vehicle* lpVehicle = GetVehicle(luVehicle);
        CGS_ASSERT(lpVehicle->IsAlive(),    "lpVehicle->IsAlive()");     // baked .cpp 4987
        CGS_ASSERT(lpVehicle->HasEntity(),  "lpVehicle->HasEntity()");   // baked .cpp 4988

        if (lbAvoidable)
        {
            // 0x82731DE4..0x82731FF0 -- cache this car in the 4-wide packet list.
            const Matrix44Affine lTransform = GetVehicleTransform(luVehicle);
            const Vector3 lFacing   = lTransform.At();    // +0x20, the zAxis row
            const Vector3 lPosition = lTransform.Pos();   // +0x30

            CGS_ASSERT(lpVehicle->IsAlive(), "IsAlive()");   // BrnTrafficVehicle.h:0x26B
            const f32 lfSpeed = lpVehicle->GetSpeed().x;

            const VehicleTypeRuntime* lpVehicleTypeRuntime =
                GetVehicleTypeRuntime(lpVehicle->GetVehicleType());
            CGS_ASSERT(lpVehicleTypeRuntime != 0, "lpVehicleTypeRuntime");   // baked .cpp 5020

            SetPacketLane(lPacket,
                          luPacketLane & 3u,
                          lPosition,
                          PickMotionLane(lFacing * lfSpeed, lFacing),
                          lpVehicleTypeRuntime->GetBBoxHalfSize());

            if ((luPacketLane & 3u) == 3u &&
                mCachedCollidableList.GetLength() != KU_MAX_COLLIDABLE_CACHED_TRAFFIC_ARRAY)
            {
                mCachedCollidableList.Append(lPacket);   // 0x82731FE4
            }
            ++luPacketLane;
        }

        if (!lbCandidate && !mbTrafficIsHidden)
        {
            // 0x82731FF4..0x82732008: a car this frame's half does not cover keeps whatever
            // collision state it already has. Next frame's `~` covers it.
            continue;
        }

        // 0x8273222C: the CURRENT SoA bit, read through the iterator's own field/mask.
        const bool lbWasCollidable = mVehicleSoaData.mCollidableVehicles.IsBitSet(luVehicle);

        if (lbCollidable == lbWasCollidable)
        {
            // 0x8273228C: nothing to post; just tripwire the flag against the SoA bit.
            CGS_ASSERT(lpVehicle->IsCollidable() == lbCollidable,
                       "Collidable flag out of sync for vehicle");   // baked .cpp 5041
            continue;
        }

        // 0x82732300.
        const CgsSceneManager::VolumeInstanceId lVolumeInstanceId =
            MakeTrafficVolumeInstanceId(luVehicle);

        if (lbCollidable)
        {
            CGS_ASSERT(lpVehicle->IsAlive(), "IsAlive()");   // BrnTrafficVehicle.h:0x312

            const VehicleTypeRuntime* lpVehicleTypeRuntime =
                GetVehicleTypeRuntime(lpVehicle->GetVehicleType());

            // 0x82732360..0x82732424. The three axis rows are multiplied by a vector the
            // console builds out of vspltisw 1 -> vcsxwfp == 1.0f, i.e. the rotation is
            // UNCHANGED; the only real edit is the translation row, which becomes
            // TransformPoint(transform, mBBoxOffset). That is the same bbox-offset transform
            // GenerateSceneUpdateEvents' collidable arm publishes each frame -- the shared
            // per-TYPE BoxVolume Prepare stage 3 registers already carries the real
            // half-extents, so no scale belongs here.
            Matrix44Affine lTransform = GetVehicleTransform(luVehicle);
            const Vector3 lForward = lTransform.At();
            lTransform.wAxis = rw::math::vpu::TransformPoint(
                lTransform, lpVehicleTypeRuntime->GetBBoxOffset());

            CGS_ASSERT(lpVehicle->IsAlive(), "IsAlive()");   // BrnTrafficVehicle.h:0x312

            // 0x82732454 `addi r11, r11, 0x24` -- VolumeId(KU_HACK_BASE_VOLUME_ID + type), the
            // id space Prepare stage 3 (_wQ7_02.cpp:721-792) already registered one shared
            // rw::collision::BoxVolume per vehicle TYPE into.
            const CgsSceneManager::VolumeId lVolumeId(
                static_cast<u64>(36u + lpVehicle->GetVehicleType()));

            lpOutput->GetSceneInputInterface()->AddVolumeInstance(
                lVolumeInstanceId, lVolumeId, lTransform);

            // 0x8273248C..0x827324BC: the swept padding == the car's facing axis times the
            // distance it covers this frame (GetSpeed() * mfSimTimeStepVec, this+0x71410).
            const Vector3 lPadding = lForward * (lpVehicle->GetSpeed().x * mfSimTimeStepVec.x);

            lpOutput->GetSceneInputInterface()->AddForCollision(
                lVolumeInstanceId,
                static_cast<CgsSceneManager::SceneManagerIO::InEventAddForCollision::CullingGroup>(
                    KU_TRAFFIC_CULLING_GROUP),                            // li r5, 2
                rw::physics::ACTIVE_BODY,                                 // li r6, 4
                lPadding,
                CgsSceneManager::SceneManagerIO::E_DO_NOT_ADD_TO_CACHE_MANAGER);   // li r7, 2

            lpVehicle->SetCollidable(true, lIt, mVehicleSoaData);
            ++luDiagTurnedCollidable;

            if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
            {
                // [T4-collide] one-shot on the FIRST traffic volume instance ever added for
                // collision. A missing line means gate 1 never fired; a line with a plausible
                // id means the scene now has a traffic collision volume. DELETE-WHEN-STABLE.
                static bool sbLogged = false;
                if (!sbLogged)
                {
                    sbLogged = true;
                    *lpDiag << "[T4-collide] FIRST AddForCollision vehicle="
                            << static_cast<s32>(luVehicle)
                            << " type=" << static_cast<s32>(lpVehicle->GetVehicleType())
                            << " volumeId=" << static_cast<s32>(36u + lpVehicle->GetVehicleType())
                            << " volInstIdHi=" << static_cast<s32>(lVolumeInstanceId.muId >> 32)
                            << " sources=" << static_cast<s32>(lSourcePositions.GetLength())
                            << " [DELETE-WHEN-STABLE]\n";
                }
            }
        }
        else
        {
            // 0x82732508: the exact mirror -- collision first, then the volume instance.
            lpOutput->GetSceneInputInterface()->RemoveForCollision(lVolumeInstanceId);
            lpOutput->GetSceneInputInterface()->RemoveVolumeInstance(lVolumeInstanceId);
            lpVehicle->SetCollidable(false, lIt, mVehicleSoaData);
        }
    }

    // ================================================================================
    // PASS 3 -- flush the partial packet, then flip the half-pool selector.
    // ================================================================================

    // 0x8273284C..0x82732984. The unused lanes are padded with position == the constant's
    // FLT_MAX lane (so the avoidance search can never pick them) and velocity == 0. The two
    // half-extent lanes are deliberately left as they were: the console writes neither.
    const u32 luPartialLanes = luPacketLane & 3u;
    if (luPartialLanes != 0 &&
        mCachedCollidableList.GetLength() != KU_MAX_COLLIDABLE_CACHED_TRAFFIC_ARRAY)
    {
        const f32 lfFar = kfVehicle_AvoidRadiusSq_CollideRadiusSq_MaxFloat_W.z;
        for (u32 luLane = luPartialLanes; luLane < 4u; ++luLane)
        {
            SetLane(lPacket.mPosition_X, luLane, lfFar);
            SetLane(lPacket.mPosition_Y, luLane, lfFar);
            SetLane(lPacket.mPosition_Z, luLane, lfFar);

            SetLane(lPacket.mLinearVelocity_X, luLane, 0.0f);   // flt_82001CC0
            SetLane(lPacket.mLinearVelocity_Y, luLane, 0.0f);
            SetLane(lPacket.mLinearVelocity_Z, luLane, 0.0f);
        }
        mCachedCollidableList.Append(lPacket);
    }

    // 0x82732988..0x827329B4: `this[(0xE4A5+i)*8] = ~this[(0xE4A5+i)*8]` over all ten fields.
    // Construct seeds the first 300 bits, so this alternates which half of the 600-car pool is
    // re-evaluated. It is NOT a clear-and-rebuild: dropping it pins the sweep to one half.
    mVehiclesToUpdateCollidables.SetInverse(mVehiclesToUpdateCollidables);

    if (luDiagTurnedCollidable != 0)
    {
        if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
        {
            // [T4-collide] value-latched on the per-frame transition count, so the log shows
            // the population changing rather than one frame. DELETE-WHEN-STABLE.
            static s32 siLastCount = -1;
            if (siLastCount != static_cast<s32>(luDiagTurnedCollidable))
            {
                siLastCount = static_cast<s32>(luDiagTurnedCollidable);
                *lpDiag << "[T4-collide] turned collidable this frame: " << siLastCount
                        << " (cached packets " << static_cast<s32>(mCachedCollidableList.GetLength())
                        << ") [DELETE-WHEN-STABLE]\n";
            }
        }
    }
}

}
