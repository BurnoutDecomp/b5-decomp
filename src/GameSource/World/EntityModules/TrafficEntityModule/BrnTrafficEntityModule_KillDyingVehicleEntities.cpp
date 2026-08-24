// ============================================================================
// BrnTrafficEntityModule_KillDyingVehicleEntities.cpp -- the REMOVE half of the traffic
// scene registration: KillDyingVehicleEntities @0x82741E40 and its only callee
// KillDyingVehicleEntity @0x8272EB40. DWARF home BrnTrafficEntityModule.cpp (:4410..:4546).
//
// WHY THIS TU EXISTS (user report 2026-08-24, "the traffic despawn, like it is anchored to
// the junkyard and doesn't follow the player"): offline param retirement is
//   KillParam -> param DYING + Vehicle::SetDead/SetOrphan (entity/collision/physics regs KEPT)
//   -> KillDyingVehicleEntities tears the registrations down       [was UNRECONSTRUCTED]
//   -> UpdateParams_UpdateDead sees (dying & !alive & !entity & !collidable)
//   -> ClearDying + PutParamInPurgatory -> 5 decision frames -> mFreeParams.
// Without the middle leg every driving car that ever registered a scene entity leaked its
// param id on death; mFreeParams fell monotonically (~1.4/s measured) and hit 0 a few
// minutes into a session, after which no traffic could generate anywhere for the rest of
// the run. The junkyard neighbourhood is filled at boot before the pool empties, so the
// user-visible symptom is "traffic exists only around the junkyard".
//
// Layout is host-native: every member is reached by name; console displacements in comments
// only attest which member a line resolves to (mVehicleSoaData arrays: mAliveVehicles
// +164560, mVehiclesWithEntities +164640, mCollidableVehicles +164720, mPhysicalVehicles
// +164800, mArticulatedVehicles +164880; maRecentlyRecoveredSlammedTraffic +0x580C0 ==
// maNewRemovedVehicles' +0x57F7C + 324, per the _wT3_02.cpp demotion-chain attestation;
// mTrailerPurgatoryList +256504; mbAllVehiclesDead +464908, beside mfBaseDensityScale
// +464912).
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h"

#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

namespace BrnTraffic
{
namespace
{
    // NAMED LEG GATE, file-local by the sibling partfiles' convention. NOT IN THE X360 BINARY.
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
                << "[T-recycle] TrafficEntityModule leg NOT RECONSTRUCTED, skipped: "
                << lpcLegNameAndReason << " [FLAG PC partial gate]\n";
        }
    }

    // The scene owner byte a traffic VolumeInstanceId carries -- the same two-line build as
    // _wT4_01.cpp's file-local MakeTrafficVolumeInstanceId (that helper is anonymous-namespace
    // there, so it is mirrored here rather than reached across TUs). The console builds the
    // id inline both times it needs one (0x8272ED60/0x8272ED88: lis/ori the owner word, then
    // VolumeInstanceId::SetEntityIDEntityIndex).
    const u8 KU8_TRAFFIC_ENTITY_OWNER = 2;   // E_ENTITYTYPE_TRAFFIC

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
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::KillDyingVehicleEntity  @ 0x8272EB40   (.cpp 4457..4546)
//
// Tears down every registration one dead vehicle still holds, one arm per SoA bit:
// articulation detach, physics demotion, collision volumes, scene entity; then, when
// anything was torn down and the vehicle is the trailer, parks the trailer id in its own
// purgatory (the same {index, 5 decision frames} record the param pool uses).
//
// The FastBitArray accessor asserts that pepper the console body (CgsFastBitArray.h:235/
// :250/:251/:282/:284/:314/:316/:374/:415) live inside the committed IsBitSet/Iterator
// bodies and are not repeated here.
// ----------------------------------------------------------------------------
void TrafficEntityModule::KillDyingVehicleEntity(
    u32 luVehicle,
    const CgsContainers::FastBitArray<VehicleSoaData::KU_MAX_VEHICLES>::Iterator& lrItVehicle,
    BrnTrafficIO::OutputBuffer_PreScene* lpOutput)
{
    CGS_ASSERT(luVehicle == static_cast<u32>(lrItVehicle.GetIndex()),
               "luVehicle == (uint32_t)lItVehicle.GetIndex()");                  // .cpp 4457
    CGS_ASSERT(lpOutput != 0, "lpOutput");                                       // .cpp 4463

    // 0x8272EC30 `lbz +10885 & 0x20` on the 128-byte vehicle record -- E_FLAG_ORPHAN.
    CGS_ASSERT((GetVehicle(luVehicle)->GetFlags() & Vehicle::E_FLAG_ORPHAN) == 0,
               "A vehicle died while still being an orphan");                    // .cpp 4464

    bool lbRemovedSomething = false;

    // ---- arm 1: articulation (mArticulatedVehicles, +164880) ---------------------------
    if (mVehicleSoaData.mArticulatedVehicles.IsBitSet(luVehicle))
    {
        // GATE: the cab/trailer detach pair (.cpp 4478..4508) -- Vehicle::DetachArticulation
        // @? has no body in this tree (same blocker KillParam's own trailer gate names at
        // _wT2_01.cpp). DEAD ON THIS BUILD: trailers are wave-gated out of FillNewHull /
        // GenerateNewVehicle, so nothing ever sets an mArticulatedVehicles bit.
        // DELETE-WHEN DetachArticulation lands with the trailer wave.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "KillDyingVehicleEntity articulation arm (.cpp 4478..4508) -- "
            "Vehicle::DetachArticulation has no body; dead here because trailers are "
            "wave-gated out of spawning");
    }

    // ---- arm 2: physics demotion (mPhysicalVehicles, +164800) --------------------------
    if (mVehicleSoaData.mPhysicalVehicles.IsBitSet(luVehicle))
    {
        // 0x8272F1D4: Array<u16,160>::EraseInstancesOf on this+0x580C0 ==
        // maRecentlyRecoveredSlammedTraffic (:683; the array after maNewRemovedVehicles'
        // attested +0x57F7C).
        maRecentlyRecoveredSlammedTraffic.EraseInstancesOf(static_cast<u16>(luVehicle));

        CGS_ASSERT(GetVehicle(luVehicle)->IsPhysical(),
                   "GetVehicle( luVehicle )->IsPhysical()");                     // .cpp 4513

        // `li r5, 0` -- the physics-removal half runs (maNewRemovedVehicles Append).
        StopVehicleBeingPhysical(luVehicle, false);
        lbRemovedSomething = true;
    }

    // ---- arm 3: collision volumes (mCollidableVehicles, +164720) -----------------------
    if (mVehicleSoaData.mCollidableVehicles.IsBitSet(luVehicle))
    {
        CGS_ASSERT(GetVehicle(luVehicle)->IsCollidable(),
                   "GetVehicle( luVehicle )->IsCollidable()");                   // .cpp 4526
        CGS_ASSERT(GetVehicle(luVehicle)->HasEntity(),
                   "GetVehicle( luVehicle )->HasEntity()");                      // .cpp 4527

        // The console rebuilds the id for each call (two SetEntityIDEntityIndex runs).
        lpOutput->GetSceneInputInterface()->RemoveForCollision(
            MakeTrafficVolumeInstanceId(luVehicle));
        lpOutput->GetSceneInputInterface()->RemoveVolumeInstance(
            MakeTrafficVolumeInstanceId(luVehicle));

        // SetCollidable takes the ITERATOR (its body ANDCs the iterator's cached mask into
        // the SoA word), matching the console's r5.
        GetVehicle(luVehicle)->SetCollidable(false, lrItVehicle, mVehicleSoaData);
        lbRemovedSomething = true;
    }

    // ---- arm 4: the scene entity (mVehiclesWithEntities, +164640) ----------------------
    if (mVehicleSoaData.mVehiclesWithEntities.IsBitSet(luVehicle))
    {
        CGS_ASSERT(GetVehicle(luVehicle)->HasEntity(),
                   "GetVehicle( luVehicle )->HasEntity()");                      // .cpp 4546

        const EntityId lTrafficEntityId = MakeTrafficEntityId(luVehicle);
        lpOutput->GetSceneInputInterface()->RemoveEntity(
            CgsSceneManager::EntityId(lTrafficEntityId.muValue), 0);

        GetVehicle(luVehicle)->SetHasEntity(false, luVehicle, mVehicleSoaData);
        lbRemovedSomething = true;
    }

    // ---- tail: the trailer's own purgatory ---------------------------------------------
    // 0x8272F7A8..0x8272F7E4: species 2 == E_SPECIES_TRAILER; the record is the param pool's
    // {index, 5 decision frames} PurgatoryInfo, appended to the 1-slot trailer purgatory
    // (+256504). The 5 is one emission for both KU_PURGATORY_TIME_* constants (both 5).
    if (lbRemovedSomething)
    {
        if (GetVehicleSpecies(luVehicle) == Vehicle::E_SPECIES_TRAILER)
        {
            PurgatoryInfo lInfo;
            lInfo.muIndex              = static_cast<u16>(luVehicle);
            lInfo.muDecisionFramesLeft = 5;
            mTrailerPurgatoryList.Append(lInfo);
        }
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::KillDyingVehicleEntities  @ 0x82741E40   (.cpp 4410..4438)
//
// Candidate set: NOT alive AND still holding any registration
// (articulated | collidable | physical | has-entity), walked with the FastBitArray
// iterator; each hit is handed to KillDyingVehicleEntity with the live iterator, exactly
// as UpdateParams_UpdateDead hands its own. The head also computes "no vehicle is alive
// at all" from mAliveVehicles' backing words and publishes it into mbAllVehiclesDead
// (+464908) at the tail -- the TEARING_DOWN state machine's exit condition.
// ----------------------------------------------------------------------------
void TrafficEntityModule::KillDyingVehicleEntities(BrnTrafficIO::OutputBuffer_PreScene* lpOutput)
{
    CGS_ASSERT(lpOutput != 0, "lpOutput != NULL");                               // .cpp 4410

    // 0x82741E7C..0x82741EA4: the ten-word scan of mAliveVehicles, before any teardown.
    const bool lbNoAliveVehicles = mVehicleSoaData.mAliveVehicles.IsZero();

    // 0x82741EAC..0x82741F58: ~alive & (articulated | collidable | physical | entities),
    // built on the stack.
    CgsContainers::FastBitArray<VehicleSoaData::KU_MAX_VEHICLES> lRegistered;
    lRegistered.SetOr(mVehicleSoaData.mArticulatedVehicles, mVehicleSoaData.mCollidableVehicles);
    lRegistered.SetOr(lRegistered, mVehicleSoaData.mPhysicalVehicles);
    lRegistered.SetOr(lRegistered, mVehicleSoaData.mVehiclesWithEntities);

    CgsContainers::FastBitArray<VehicleSoaData::KU_MAX_VEHICLES> lNotAlive;
    lNotAlive.SetInverse(mVehicleSoaData.mAliveVehicles);

    CgsContainers::FastBitArray<VehicleSoaData::KU_MAX_VEHICLES> lDeadWithRegistrations;
    lDeadWithRegistrations.SetAnd(lNotAlive, lRegistered);

    for (CgsContainers::FastBitArray<VehicleSoaData::KU_MAX_VEHICLES>::Iterator lItVehicle =
             lDeadWithRegistrations.Begin();
         lItVehicle != lDeadWithRegistrations.End();
         ++lItVehicle)
    {
        const u32 luVehicle = static_cast<u32>(lItVehicle.GetIndex());

        CGS_ASSERT(!GetVehicle(luVehicle)->IsAlive(),
                   "!GetVehicle( luVehicle )->IsAlive()");                       // .cpp 4434

        KillDyingVehicleEntity(luVehicle, lItVehicle, lpOutput);
    }

    // 0x82742018 `stb` into +464908.
    mbAllVehiclesDead = lbNoAliveVehicles;
}

}
