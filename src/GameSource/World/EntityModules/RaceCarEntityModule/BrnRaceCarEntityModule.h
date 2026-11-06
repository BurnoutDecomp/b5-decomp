#pragma once

// Native RaceCar containment: ARTIST bounds and DecFIGS declaration order.
// Replay serialiser alignment is a constrained reconstruction; see its home.

#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"
#include "BrnCommonTypes.h"
#include "SharedClasses/BrnSharedConstants.h"   // BrnUpdateSet
#include "types.hpp"
#include "GameSource/BurnoutConstants.h"             // EActiveRaceCarIndex / EGlobalRaceCarIndex
#include "GameSource/GameState/BrnGameStateSharedIO.h" // BrnGameState::GameStateModuleIO::EPlayerScoringIndex
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "SharedClasses/Progression/BrnTrainingTypes.h" // BrnProgression::ETrainingType

#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"
#include "GameSource/Replays/Serialisers/BrnReplayRaceCarEntitySerialiser.h"
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h" // CgsModule::EventReceiverQueue<N,A>
#include "GameShared/GameClasses/Containers/CgsBitArray.h"           // CgsContainers::BitArray<8u> (mabResetThisFrame)
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"   // CgsResource::BaseResourcePtr
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarStreamer.h" // BrnWorld::RaceCarStreamer (by value)
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCar.h"         // BrnWorld::RaceCar (by value)
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.h"   // BrnWorld::ActiveRaceCar (by value)
#include "GameSource/Graphics/BrnCoronaManager.h"   // BrnCoronaManager::BrnSubmissionInterface (SubmitCoronasForRaceCar's 1st arg)
#include "GameSource/World/BrnPlaceOnTrackManager.h"                              // BrnWorld::PlaceOnTrackManager (by value, +0x17850)
#include "GameShared/GameClasses/World/CgsWorldMap2D.h"                           // CgsWorld::WorldMap2D (by value, +0x18300)
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"             // CgsResource::ResourceHandle (mDistrictMapResourceHandle, +0x23C)
#include "SharedClasses/World/BrnWorldRegion.h"                                   // BrnWorld::WorldRegion (mCurrentWorldRegion, +0x244)
#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostManager.h"                 // BrnWorld::BoostManager (by value, +0x17890)
#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostDebugComponent.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/NearMisses/BrnNearMissManager.h"         // BrnWorld::NearMissManager (by value, +0x17E68)
#include "GameSource/World/EntityModules/RaceCarEntityModule/AirTime/BrnAirTimeManager.h"             // BrnWorld::AirTimeManager (by value, +0x18098) [boost-ticker wave]
#include "GameSource/World/EntityModules/RaceCarEntityModule/TrafficCheck/BrnTrafficCheckManager.h"    // BrnWorld::TrafficCheckManager (by value, +0x180E8) [boost-wave2]
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"                     // BrnPhysics::Vehicle::AggressiveDrivingFlags (mAggressiveDrivingFlags, +0x1836C) [boost-wave2]
#include "GameSource/World/EntityModules/RaceCarEntityModule/CrashPlay/BrnCrashPlayDebugComponent.h"  // BrnWorld::CrashPlayManager (by value, +0x180F0)
#include "GameSource/World/EntityModules/RaceCarEntityModule/PowerParking/BrnPowerParkingManager.h"   // BrnWorld::PowerParkingManager (by value, +0x18250)
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnPlayerVehicleControls.h"
#include "GameSource/AttribSys/Generated/classes/surfacelist.h"        // Attrib::Gen::surfacelist mSurfaceList (DWARF :365)     // BrnWorld::PlayerVehicleControls (by value, +0x183A8)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                       // BrnNetwork::EPaybackType (meActivePaybackType)
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                             // CgsNumeric::Random (mNonDeterministicRandom, +0x18490)

#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleDebugComponent.h"

#include <cstddef>                                   // offsetof

namespace CgsGraphics { class DispatchFrame; }
// (BrnWorld::ShadowMap is a `struct` -- BrnShadowMap.h:65; never forward-declare it
//  as `class` here, the class key is part of the MSVC mangling.)
namespace BrnWorld { struct ShadowMap; }

// Writable ARTIST tailgate data, shared by detection, boost credit and debug cones.
namespace BrnWorld
{
    extern f32 KF_Z_OFFSET_FROM_CAR_CENTRE_TO_CONE_APEX;
    extern f32 KF_TAILGATING_CONE_HALF_ANGLE_RADS;
    extern f32 KF_TAILGATING_CONE_DEPTH;
    extern f32 KF_MIN_TAILGATE_DURATION;
    extern f32 KF_TAILGATING_MAX_RELATIVE_VELOCITY_MAGNITUDE;
    extern f32 KF_MIN_TAILGATEE_SPEED;
}

namespace CgsResource { struct ResourceHandle; }
namespace BrnResource { struct VehicleList; class WheelList; enum ECarType ; }
// The game-action consumer's two argument types (pointer-only here; the .cpp includes both
// owning headers). ResetPlayerCarAction is game action 0's 80-byte payload,
// RaceCarAIInterface is the AI publish surface SpawnRaceCar posts its attach event into.
namespace BrnGameState { namespace GameStateModuleIO {
    struct ResetPlayerCarAction;
        struct CarSelectionRequestStreamingAction;
    struct PrepareForModeAction;
    struct StopModeAction;          // game action 39 -- HandleStopModeAction's record
    struct AddRivalCarAction;       // game action 196 -- AddRivalCar's record
    struct SetupNetworkCarAction;   // game action 5 -- HandleSetupNetworkCarAction's record
} }
// [stuntrace start-grid wave] SetupOpponents / SetUpPlayerCarForMode take the mode's parameter
// block by pointer; the .cpp includes BrnGameModeParams.h for the members.
namespace BrnGameState { class GameModeParams; }
namespace BrnAI       { namespace AIModuleIO         { struct RaceCarAIInterface;   } }
namespace BrnPhysics  { namespace Vehicle            { struct VehicleInputInterface; } }
// DetachActiveRaceCar's fourth argument (DWARF spells it
// OutputBuffer_PreScene::SceneInputInterface, a typedef for this type). Pointer-only here;
// the .cpp reaches it through BrnRaceCarEntityModuleIO.h.
namespace CgsSceneManager { namespace SceneManagerIO { struct InSceneUpdateInterface; } }

namespace BrnWorld
{

// The three dispatch-list ids the console's WorldModule::GenerateDispatchLists hands the
// race-car module (`li r6, 0xC` / `li r7, 0x13` / `li r8, 0x14` @0x827D27AC..0x827D27B8):
// the dispatch-frame OBJECT list and the two race-car MESH lists. Compare the world's own
// 2 / 11 / 15 in BrnWorldModule.cpp.
const s32 KI_RACE_CAR_OBJECT_LIST           = 12;
const s32 KI_RACE_CAR_OPAQUE_MESH_LIST      = 19;
const s32 KI_RACE_CAR_TRANSPARENT_MESH_LIST = 20;

// X360-attested pending-training-request ring depth (DWARF BrnRaceCarEntityModule.h:66).
// AddTrainingRequest asserts miPendingRequestCount < this before appending.
const s32 KI_TRAINING_REQUEST_QUEUE_SIZE = 8;

// The active-race-car output interface (real home:
// SharedIO/BrnRaceCarEntityModuleOutputInterface.h). CopyActiveRaceCarToPlayerScoringMappingToOutput
// only takes a pointer to it, so a forward declaration suffices here.
namespace RaceCarEntityModuleIO { struct RCEntityActiveRaceCarOutputInterface; }
// Pointer-only dependency; the queue is owned by SharedIO/BrnRaceCarEntityModuleIOQueues.h.
namespace RaceCarEntityModuleIO { struct TakedownEventQueue; }
namespace RaceCarEntityModuleIO { struct GameEventQueue; class InputBuffer_PrePhysics; class OutputBuffer_PrePhysics; class InputBuffer_PostScene; class OutputBuffer_PostScene; class InputBuffer_GenerateDispatchLists; struct InputBuffer_PreScene; struct OutputBuffer_PreScene; struct InputBuffer_PostPhysics; struct OutputBuffer_PostPhysics; struct OutputBuffer_Prepare; }

// The "CarColours" palette resource LoadGlobalResources acquires (real home
// SharedClasses/Graphics/BrnGlobalColourPalette.h); held by pointer only here.
struct GlobalColourPalette;

// ⭐ ADDED 2026-08-11 (player-input wave). DWARF BrnRaceCarEntityModule.h:95-97 -- this header
// IS its home. One entry of the module's per-frame stomped/leaped car list
// (mStoredStompees[8], below), produced by ProcessLeapedAndStompedCars @0x822BD5B8 and drained
// by ProcessPlayerVehicleInput @0x822FFE30 into
// VehicleDriverInputInterface::AddTargetAssist(Vector3, EntityId) -- whose parameter pair is
// exactly these two members, in this order. The console loop stride is 0x20 with the id read at
// +0x10 (`lwz r18, 0x10(r31) ; lvx128 v127, r0, r31 ; ... ; addi r31, r31, 0x20`), which is what
// the Vector3's 16-byte alignment produces here too.
struct StoredStompeeData
{
    Vector3  mPosition;   // :96  @0x00
    EntityId mEntityId;   // :97  @0x10
};

// ⭐ ADDED 2026-09-05 (rival range-loop wave, lane W1). DWARF BrnRaceCarEntityModule.h:450
// declares the module's own pending array as `RaceCarNeedsHidingEvent[8] mHidingEvents`, whose
// element type is BrnGameState::GameStateModuleIO::RaceCarNeedsHidingEvent (dwarfdump
// BrnGameEvents.h:371-374: `int32_t miActiveRaceCarIndex; float32_t mfHiddenTime;`).
//
// [!] HEADER REQUEST -- interim TU-local mirror, exactly the carrier
// BrnRoadRageMode.cpp:161 already uses for the CONSUMER end of the same 8-byte record
// (its `RaceCarNeedsHidingEventRecord`), because GameSource/GameState/BrnGameEvents.h does
// not yet carry E_EVENT_RACE_CAR_NEEDS_HIDING or its GameEvent<> struct. The two mirrors
// are byte-identical by construction; see the report's header_requests.
// DELETE-WHEN BrnGameEvents.h grows the enumerator + struct and both mirrors can be dropped.
//
// SIZE PROOF: UpdateHidingEvents @0x822F5830 posts each record with `li r6, 8` and walks the
// array with `addi r30, r30, 8`; the array base (module + 0x186D4) and the count (module +
// 0x18714) are 0x40 == 8 * 8 bytes apart, which is the DWARF's array bound of 8.
struct RaceCarNeedsHidingEventRecord
{
    s32 miActiveRaceCarIndex;   // +0x00  (SetHiddenDelay: `stw` of RaceCar::miActiveRaceCarIndex)
    f32 mfHiddenTime;           // +0x04  (SetHiddenDelay: `stfs`)
};

// ---- element types (2026-07-31: THE ODR FORK IS GONE) ----------------------
// This header used to define its own opaque `class RaceCar { u8 [0xB0]; }` and
// `class ActiveRaceCar { u8 [0x1CD0]; }` so the two array accessors could compile
// against a known stride. Both real types are committed (BrnRaceCar.h /
// BrnActiveRaceCar.h) and are now included above, so those stand-ins -- a genuine
// two-definitions-of-one-class ODR violation the moment any TU saw both headers --
// are deleted. Consequence: on the x64 gate the element strides are the real
// sizeof()s (both types carry pointers the console stored in 4 bytes), so the
// CONSOLE array offsets 0x250 / 0x1A60 no longer hold for maActiveRaceCars and the
// offsetof pins below drop to the members whose offsets are still meaningful.
// Nothing reads this module by offset -- parity is by named member.

class RaceCarEntityModule : public CgsModule::ModuleSingleBuffered
{
    friend class WorldDebugComponent;
    friend class BoostDebugComponent;
    friend struct RaceCarEntityModuleDebugComponent;   // reads the module's debug switches/members directly
    friend class PlaceOnTrackManager; // writes the embedded diagnostic history

public:
        // ---- ADDITIVE (attested by WorldModule::Construct @0x827CF540, which
        //      virtual-dispatches the fleet lifecycle) ----
        // Bodied in BrnRaceCarEntityModule.cpp.
        void Construct();
        // ---- ADDITIVE (attested by WorldModule::DestructWorld @0x827BD0F0) ----
        // Bodied in BrnRaceCarEntityModule_wS34_00.cpp.
        void Destruct();
        // ---- ADDITIVE (attested by WorldModule::ReleaseWorld @0x827BCE58) ----
        // Bodied in BrnRaceCarEntityModule_wS34_00.cpp.
        bool Release();

        // ---- ADDITIVE (WorldModule::EntityModulePrePhysicsUpdate @0x827BD5B8) ----
        // Bodied in BrnRaceCarEntityModule.cpp.
        void PrePhysicsUpdate( RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpInput,
                               RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput,
                               BrnUpdateSet lUpdateSet );

        // ---- ADDITIVE (WorldModule::EntityModulePostSceneUpdate @0x827C3C58) ----
        void PostSceneUpdate( RaceCarEntityModuleIO::InputBuffer_PostScene* lpInput,
                              RaceCarEntityModuleIO::OutputBuffer_PostScene* lpOutput,
                              BrnUpdateSet lUpdateSet );

        // ---- THE CRASH EXIT, consumer side (2026-08-25). Body in
        //      BrnRaceCarEntityModule_CrashExit.cpp, alongside the PostSceneUpdate slice that
        //      calls it. X360 0x822F3FE0 (359 insns).
        void ProcessRaceCarCrashCompleteEvents(
            const RaceCarEntityModuleIO::InputBuffer_PostScene* lpInput );

        // X360 0x822BD5B8 (DWARF BrnRaceCarEntityModule.h:626). The Showtime leap/stomp target
        // list: while Showtime is on and the player is airborne, copy the traffic module's
        // potential stompees into mStoredStompees / miStoredStompeeCount, which
        // ProcessPlayerVehicleInput hands to AddTargetAssist. Called from PostSceneUpdate right
        // after ProcessRaceCarCrashCompleteEvents (bl @0x822FE4B8); the console passes lpOutput
        // (r5 @0x822FE4AC) and the body never reads it (crash parity CHAIN-STOMPEES c, 2026-09-24).
        void ProcessLeapedAndStompedCars( const RaceCarEntityModuleIO::InputBuffer_PostScene* lpInput,
                                          RaceCarEntityModuleIO::OutputBuffer_PostScene* lpOutput );

        // X360 0x822CDF10 (DWARF BrnRaceCarEntityModule.h:629, body :1420). The Power Parking
        // consumer on the post-scene tick: takes the traffic module's nearby-PARKED-traffic
        // measurements off the input buffer, ranks every in-world, not-crashing NETWORK race car
        // against the player on the same running minima (CheckVehicleForPowerPark), hands the
        // result to mPowerParkingManager while a park is in progress, and publishes
        // IsPowerParking() as RaceCarToTrafficInterface bit 0 -- the gate of the traffic module's
        // GenerateNearbyParkedTrafficOutput. Called from PostSceneUpdate @0x822FE588, gated
        // `!mbIsInGameMode || meGameModeType == E_MODE_ONLINE_FREE_BURN_LOBBY`. Body in
        // BrnRaceCarEntityModule_CrashExit.cpp beside its PostSceneUpdate neighbours
        // (crash parity FX-SCENEMGR item 4, 2026-09-24).
        void ProcessPowerParking( const RaceCarEntityModuleIO::InputBuffer_PostScene* lpInput,
                                  RaceCarEntityModuleIO::OutputBuffer_PostScene* lpOutput );

        // ---- THE CRASH ENTRY, consumer side (crash wave 2026-09-02). X360 0x822BD8B0
        //      (127 insns), DWARF :668. Body in BrnRaceCarEntityModule_CrashExit.cpp beside its
        //      exit twin. Drains the physics module's RaceCarCrashEvent queue every post-physics
        //      tick: for the PLAYER's first event of a crash it latches the crash-play manager's
        //      mPlayerCarVolumeInstanceID (the value CrashPlayManager::Update was starving on)
        //      and, in ROAD_RAGE / mode 8 / mode 10, sets mbIsWrecked on a primary crash; then
        //      every event reaches ActiveRaceCar::OnCrash. Called from PostPhysicsUpdate at the
        //      console's own slot (@0x823075E8, before ProcessCreateVehicleEvents).
        void ProcessRaceCarCrashEvents_PostPhysics(
            const RaceCarEntityModuleIO::InputBuffer_PostPhysics* lpInput,
            RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpOutput );

        // ---- THE RESET-ON-TRACK PUMP (resetpump wave 2026-08-26). All three bodies live in
        //      BrnRaceCarEntityModule_ResetPump.cpp. Together they are the round trip that was
        //      missing between "a heavy crash ends" and "the car is put back on the road":
        //        PreScene   WriteUpdatedAIData            -> the AI's per-frame car snapshot
        //                                                   (and mbPlayerDataSet, which gates
        //                                                   AIModule::Update's ENTIRE body)
        //        PostScene  SendResetOnTrackRequests      -> the ONLY reader of
        //                                                   RaceCar::mbToBeResetOnTrack
        //        PrePhysics ProcessResetOnTrackResultQueue-> ActiveRaceCar::RequestPlaceOnTrack

        // X360 @0x822D1FC8 (172 insns). Publish the player-car block and every ACTIVE car's
        // per-frame state into the pre-scene output buffer's RaceCarAIInterface.
        void WriteUpdatedAIData( RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput );

        // X360 @0x822CE178 (57 insns). Turn every global race car flagged mbToBeResetOnTrack
        // into a queued BrnAI::AIModuleIO::ResetOnTrackRequest on the post-scene output.
        void SendResetOnTrackRequests( RaceCarEntityModuleIO::OutputBuffer_PostScene* lpOutput );

        // ------------------------------------------------------------------------
        // X360 0x822CE9E0 (305 instrs) -- the OTHER producer of mbToBeResetOnTrack: the
        // per-frame watchdog that resets a car which has drowned, hit a super-fatal
        // surface, wedged itself in the world, been force-reset, fallen out of the world,
        // or hung in the air too long. Bodied in BrnRaceCarEntityModule_ResetPump.cpp.
        // ------------------------------------------------------------------------
        void CheckForResetOnTrackConditions();

        // X360 @0x822F4580 (192 insns). Drain the AI module's two result rings: the
        // reset-on-track RESULTS (success -> the AI's pose; failure -> the car's own
        // GetResetCoords) and the place-on-track REQUESTS, both into
        // ActiveRaceCar::RequestPlaceOnTrack.
        void ProcessResetOnTrackResultQueue(
            const RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpInput,
            RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput );

        // ---- ADDITIVE (WorldModule::EntityModulePreSceneUpdate @0x827BD1F0) ----
        // Bodied in BrnRaceCarEntityModule.cpp.
        void PreSceneUpdate( RaceCarEntityModuleIO::InputBuffer_PreScene* lpInput,
                             RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput,
                             BrnUpdateSet lUpdateSet );

        // ---- ADDITIVE (WorldModule::EntityModulePostPhysicsUpdate @0x827D3F10) ----
        // Bodied in BrnRaceCarEntityModule.cpp.
        void PostPhysicsUpdate( RaceCarEntityModuleIO::InputBuffer_PostPhysics* lpInput,
                                RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpOutput,
                                BrnUpdateSet lUpdateSet );

        // X360 0x822BDAB0; DecFIGS declares this const. Native PC submits this from
        // WorldModule's render pass; the original +0x18349 switch is checked inside.
        void DebugRenderPosition() const;

        // ⭐⭐ X360 0x822E87B8 -- THE PHYSICS READBACK (physics-return-path wave 2026-08-11).
        // The console's ONE producer of every active car's pose: it walks the post-physics
        // input buffer's VehicleOutputInterface and hands each published RaceCarState to
        // ActiveRaceCar::UpdatePhysicsState @0x822D4418, then drains the deformation
        // output. Its only caller is PostPhysicsUpdate @0x82307538 (the `bl` at 0x8230761C).
        // Signature from the asm prologue: r3 = this, r4 = the input buffer. See the .cpp
        // banner for the leg-by-leg landed/parked inventory.
        void ReadUpdatedActiveRaceCarDataFromPhysics(
                RaceCarEntityModuleIO::InputBuffer_PostPhysics* lpInput );

        // ====================================================================
        // THE RESET-ON-TRACK RING'S TWO DRIVERS (aicar_reset wave 2026-08-26).
        // Bodies in BrnRaceCarEntityModule_ResetOnTrack.cpp. Between them they make
        // ActiveRaceCar::mPrevTransforms -- the ring ActiveRaceCar::GetResetCoords reads --
        // hold real data on this build for the first time.
        // ====================================================================

        // X360 0x822D2280 (159). Per active car, per physics readback: take the collision tag
        // the above-ground ray hit under the car, hand its AI-SECTION half to
        // ActiveRaceCar::SetAISection, and (player only) run the wrong-way / traffic-direction
        // state machine. Called from ReadUpdatedActiveRaceCarDataFromPhysics, just BEFORE
        // ActiveRaceCar::UpdateDeformationState. See the .cpp for what is parked.
        void UpdateRaceCarCollisionTagging(
                s32 liActiveRaceCarIndex,
                const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState );

        // X360 0x822CF1A0 (a bare 0..7 loop). Run ActiveRaceCar::UpdateResetTransform on every
        // slot. Its only caller is PostPhysicsUpdate @0x82307538 (the `bl` at 0x82307688,
        // inside the sim-paused skip, between the physics readback and UpdateCurrentWorldRegion).
        void UpdateActiveRaceCarTransforms();

        // ====================================================================
        // THE THREE SCENE LEGS OF PostPhysicsUpdate (wave Q5, cluster G1 -- the car's
        // half of the scene volume-collision middle). All three are console functions
        // called only from PostPhysicsUpdate @0x82307538, at these exact `bl` sites:
        //     0x823075F8  ProcessCreateVehicleEvents(lpInput, lpOutput)
        //     0x82307658  GenerateSceneUpdateEvents(lpOutput)      [inside the paused skip]
        //     0x8230773C  SendRaceCarSceneUpdates(lpOutput)
        // Signatures are the DecFIGS DWARF's own (BrnRaceCarEntityModule.h entries for
        // .cpp:5070 / :5859 / :4980) and match the ARTIST asm register use one for one.
        // ====================================================================

        // X360 0x822FF620 (182). Drain the physics vehicle manager's create-vehicle result
        // queue: for every result whose VolumeInstanceId owner is E_ENTITYTYPE_RACECAR,
        // hand the freshly created handling body to the matching active slot
        // (ActiveRaceCar::OnHandlingModelAdded -> AddToScene, which is what registers the
        // car's BOX VOLUME with the scene), and -- for the player's car only -- publish the
        // new vehicle's attribute key + vehicle-list index to the director.
        // ⚠️ `const` on the input buffer is the DWARF's.
        void ProcessCreateVehicleEvents(
                const RaceCarEntityModuleIO::InputBuffer_PostPhysics* lpInput,
                RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpOutput );

        // X360 0x822F6B08 (56). For every ACTIVE slot, run the per-car post-physics scene
        // publish (ActiveRaceCar::SendSceneUpdatesPostPhysics -> UpdateCullingGroup plus the
        // late AddToCollision arm). Runs OUTSIDE the console's paused skip.
        void SendRaceCarSceneUpdates( RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpOutput );

        // X360 0x822D2500 (100). ⭐ THE CAR'S PER-FRAME SCENE-TRANSFORM PRODUCER: for every
        // ACTIVE slot push InSceneUpdateInterface::SetVolumeInstanceTransform( the slot's
        // handling-body VolumeInstanceId, mPhysicsState.mTransform ) and
        // SetEntityPosition( that id's entity word, the transform's translation ), then
        // SetPaddingForResetRaceCars. Without it a registered car volume never moves off its
        // spawn point, which is the exact signature the wave-Q5 scout names in §6.
        void GenerateSceneUpdateEvents( RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpOutput );

        // X360 0x822E85F0 (114). ⭐ THE CRASH-PLAY CONTACT PASS -- the ONLY producer of
        // CrashPlayManager::HandlePlayerToVehicleImpact, and therefore of OnCarCrash. Walks
        // the post-physics race-car contact queue for contacts whose A-side is the player's
        // body (part 0) and dispatches them by the B-side's OWNER byte.
        // ⚠️ SIGNATURE: the console call site at 0x82307674..0x82307680 loads THREE argument
        // registers (r3=this, r4=lpInput, r5=lpOutput) but the callee's prologue captures only
        // r3 and r4 (`mr r28, r3` / `mr r31, r4`) and never reads r5 anywhere in its 114
        // instructions -- a stale third argument at the call site. Declared with the two the
        // body actually uses.
        void UpdateCrashingPlayerContacts(
                const RaceCarEntityModuleIO::InputBuffer_PostPhysics* lpInput );

        // [boost-wave2 2026-09-14] X360 0x822F5A50 (122 instructions). The SIBLING contact pass
        // the console runs FIRST of the three (`bl` at 0x82307664). Walks the post-physics
        // race-car contact queue and, for every contact on the PLAYER's own body, logs the other
        // vehicle into the matching NearMissData "contacted" list -- which is what SUPPRESSES a
        // near-miss award for a car the player actually touched -- then marks the A-side car's
        // two "touching" bytes for the AI module.
        void UpdateRaceCarContacts(
                const RaceCarEntityModuleIO::InputBuffer_PostPhysics* lpInput );

        // [boost-wave2 2026-09-14] X360 0x822D2690 (48 instructions). The SECOND of the three
        // (`bl` at 0x82307670). Walks the contact-spy PROP queue and dispatches the boost
        // strategy's OnPropHit (vtable slot 16, +0x40) for every prop contact that began moving
        // this frame and belongs to the player's car -- i.e. it is the ONLY thing in the image
        // that pays BoostBurnout3::OnPropHit's mfStuntSmashEarning for smashing roadside props.
        void ProcessPropContactQueue(
                const RaceCarEntityModuleIO::InputBuffer_PostPhysics* lpInput );

        // X360 0x822CEEA8 (the tail call of GenerateSceneUpdateEvents). Walk
        // mabResetThisFrame; for every set bit whose slot IsActive(), clear that entity's
        // volume padding, then clear the whole bit array. DWARF declares the parameter as
        // `OutputBuffer_PreScene::SceneInputInterface*` -- the same InSceneUpdateInterface
        // type the PostPhysics output buffer hands out.
        void SetPaddingForResetRaceCars(
                CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface );

        // ---- X360 0x822E79F8 (called by WorldModule::GenerateDispatchLists @0x827D27C8) ----
        // ⚠ NOTE the argument list. The earlier PC declaration carried only the input
        // buffer, the visible-entity array and the three vector args -- it DROPPED the four
        // GPR arguments the console call site loads right before the branch:
        //     li r6, 0xC   li r7, 0x13   li r8, 0x14   li r9, 0
        // i.e. the dispatch-frame OBJECT list (12) and the two MESH lists (19 = race-car
        // opaque, 20 = race-car transparent) that RenderRaceCar forwards straight into
        // DispatchFrame::GetList / DrawRenderable::AddToBin, plus a bool. Without them the
        // render leg has nowhere to submit. (Same trap as the streamer wave's Prepare:
        // recover a stub's signature from the ASM, not from the existing declaration.)
        // The vector args: v1 = fog scattering, v2 = fog colour + white level,
        // v3 = the camera position (the `vsubfp128 v13, v123, v13` operand).
        void GenerateDispatchLists( RaceCarEntityModuleIO::InputBuffer_GenerateDispatchLists* lpInput,
                                    const Array<CgsSceneManager::EntityId, 32u>& lrVisibleEntities,
                                    s32  liObjectList,
                                    s32  liOpaqueMeshList,
                                    s32  liTransparentMeshList,
                                    bool lbEnvironmentMapPass,
                                    Vector4 lvFogScattering,
                                    Vector4 lvFogColourPlusWhiteLevel,
                                    Vector3 lvCameraPosition );

        // ---- X360 0x822CF6A0 -- submit ONE race car's dispatch entries ----
        // Signature recovered from the asm prologue (@0x822CF6C4..0x822CF764: r3->r20 this,
        // r5->r16 render params, r6->r25 graphics ptr, r7/r8/r9/r10 spilled to arg_64/6C/74/7C,
        // r4 spilled to arg_1C) and the call site @0x822E8550 (r5 = &activeCar + 0x7E0,
        // stack arg_84 = the shadow map, stack arg_8F = the "render attached geometry" bool).
        //   lfCameraDistance : v1 -- the camera->car DISTANCE the caller computes with
        //                      vmsum3fp128 + vrsqrtefp + vmulfp (a scalar broadcast into all
        //                      four lanes, NOT a direction vector).
        void RenderRaceCar( CgsGraphics::DispatchFrame* lpDispatchFrame,
                            rw::math::vpu::VecFloat lCameraDistance,
                            Vector4 lvFogScattering,
                            Vector4 lvFogColourPlusWhiteLevel,
                            const ActiveRaceCar::RenderParams* lpRenderParams,
                            const CgsResource::ResourcePtr<BrnVehicle::GraphicsSpec>& lrCarGraphics,
                            const CgsResource::ResourcePtr<BrnWheel::GraphicsSpec>& lrWheelGraphics,
                            s32 liObjectList,
                            s32 liOpaqueMeshList,
                            s32 liTransparentMeshList,
                            ShadowMap* lpShadowMap,
                            bool lbRenderAttachedGeometry ) const;

        // ---- X360 0x822D1600 -- ONE race car's LAMP FLARES (coronas) ----
        // Signature + `const` are the DWARF's (BrnRaceCarEntityModule.h:788, definition
        // BrnRaceCarEntityModule.cpp:3828) and they match the asm prologue one for one:
        //   r3 = this, r4 = the corona submission interface (asserted non-null, :3905),
        //   r5 = the car's STREAMED DEFORMATION SPEC resource pointer (dereferenced through
        //        the resource-pointer accessor at 0x822D1720 for its
        //        mCarModelSpaceToHandlingBodySpaceTransform at spec+1552),
        //   r6 = the car's RenderParams (asserted non-null, :3906),
        //   r7 = a BYTE (`clrlwi r11, r28, 24`): "this is the LOCAL PLAYER's car", which
        //        selects the eCoronaTypePlayerCar* archetype bank over the eCoronaTypeRaceCar*
        //        one. Its call site computes it as
        //        `(u16)mePlayerActiveRaceCarIndex == liActiveRaceCar` (@0x822E80A8..0x822E80B8).
        // Bodied in BrnRaceCarEntityModule_Render.cpp beside its only caller.
        void SubmitCoronasForRaceCar(
                BrnCoronaManager::BrnSubmissionInterface* lpCoronaSubmissionInterface,
                const CgsResource::ResourcePtr<BrnPhysics::Deformation::StreamedDeformationSpec>& lrPhysicsResource,
                ActiveRaceCar::RenderParams* lpRenderParams,
                bool lbIsPlayerCar ) const;

        // ---- X360 0x82303E78 (attested by WorldModule::Prepare @0x827D53B0 stage 6) ----
        // NOTE the argument list: the console signature is
        //   Prepare(this, OutputBuffer_Prepare* lpOutput, ResourceHandle lDistrictMapHandle)
        // (asm prologue: r4 -> the `lpOutput != NULL` assert at BrnRaceCarEntityModule.cpp:663,
        // r5 -> the district-map handle asserted at :671). The earlier PC declaration dropped
        // the output buffer, which is exactly the buffer LoadGlobalResources publishes its
        // resource requests into -- without it the module can never ask for anything.
        bool Prepare( RaceCarEntityModuleIO::OutputBuffer_Prepare* lpOutput,
                      CgsResource::ResourceHandle lDistrictMapHandle );

        // X360 0x82300730. The module's own resumable global-resource load: acquire
        // "CarColours" (pool 5), stream "Vehicles/VEHICLETEX.BIN" into the CarShared pool
        // (25), then GET the vehicle list and the wheel list. Returns false while waiting.
        bool LoadGlobalResources( RaceCarEntityModuleIO::OutputBuffer_Prepare* lpOutput );

        // ---- the per-frame STREAMING pump (race-car streamer wave 2026-07-31) ----

        // X360 0x822FEFE0 (DWARF BrnRaceCarEntityModule.h:563). Pump the five component
        // streamers for one frame, then sweep the eight active-car slots for cars whose
        // resources have just completed. Called from PreSceneUpdate when not in replay.
        void UpdateStreaming( const RaceCarEntityModuleIO::InputBuffer_PreScene* lpInput,
                              RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput );

        // X360 0x82304F70. Drain the five component streamers' own GameData request
        // queues onto the PostPhysics output buffer's resource-request interface -- the
        // ONLY way a race-car load request leaves this module. Called from PostPhysicsUpdate.
        void SendStreamerEvents( RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpOutput );

        // X360 0x822CF208 (DWARF BrnRaceCarEntityModule.h:650). THE per-frame PAINT PUBLISH:
        // for every ATTACHED active slot, resolve the paired global RaceCar's
        // {miColourPalette, miColourIndex} (both -1 -> 0) against mCarColoursResource and copy
        // the two Vector4 colours into the slot's render snapshot. It is the ONLY writer of
        // RenderParams::mPaintColour / mPearlescentColour anywhere in the image, and
        // RenderRaceCar uploads exactly those two as shader constants 20 (g_paintColour) and
        // 21 (g_pearlescentColour) -- the body panels' whole colour, since
        // Vehicle_Opaque_BodypartsSkin_EnvMapped_Default declares no diffuse sampler at all.
        // Called from PostPhysicsUpdate (its only caller), unconditionally: the console's
        // paused branch at 0x82307610 jumps to 0x823076C0, i.e. to the instruction that sets
        // up this call, so it runs paused or not.
        void UpdateActiveRaceCarColours();

        // X360 0x822D27B0 (DWARF BrnRaceCarEntityModule.h:824 --
        // `void ChangePlayerCarColour(uint32_t, uint32_t)`). Seed the PLAYER's global
        // RaceCar with the {palette, colour} pair game action 79
        // (CarSelectChangeColourAction) carries, after range-asserting both against
        // mCarColoursResource. It is the ONLY console writer of the pair on the
        // junkyard start-of-game path -- HandleResetPlayerCarAction spawns every fresh
        // car at 0/0 -- so without it every car renders in palette 0 / colour 0
        // regardless of the default the VehicleList authored (367 of 431 entries author
        // something else; PUSMC01 authors colour 13).
        void ChangePlayerCarColour( u32 luPaletteIndex, u32 luColourIndex );

        // X360 0x822F5170 (DWARF BrnRaceCarEntityModule.h:719). A freshly loaded car whose global
        // RaceCar has no colour yet (miColourIndex == -1) takes its authored default pair
        // (ActiveRaceCar::GetDefaultColourIndex / GetDefaultPaletteIndex, read by
        // OnResourcesLoaded); a non-player car in a game mode whose default colour is already on
        // the road gets a random free one instead, and SET_OPPONENTS_TO_COPS forces colour 6.
        // Called from OnRaceCarResourcesLoaded right after ActiveRaceCar::OnResourcesLoaded
        // (0x822FEDD0), its only caller (crash parity CHAIN-RECOLOUR, 2026-09-24).
        void SetupCarColour( EActiveRaceCarIndex leActiveRaceCarIndex );

        // X360 0x822EA088 (DWARF BrnRaceCarEntityModule.h:875). A colour for palette
        // liPaletteIndex nobody on the road wears: keep liColourIndex on a coin flip when it is
        // free, else walk the eight-entry table from a random start. Bodied in the Range TU with
        // the module RNG it draws from (0x82FAD2B0). Callers: SetupCarColour and the taken-down
        // re-colour legs of ProcessRaceCarCrashCompleteEvents.
        s32 GetRandomCarColour( s32 liPaletteIndex, s32 liColourIndex );

        // X360 0x822D2E68 (DWARF BrnRaceCarEntityModule.h:878). True when an attached car wears
        // colour liColourIndex (by index, any palette) or a paint within 0.22 (L1) of
        // maPalettes[liPaletteIndex]'s entry for it.
        bool IsCarColourInUse( s32 liPaletteIndex, s32 liColourIndex );

        // X360 0x822F5CF8. THE per-frame OUTPUT PUBLISH: copy every attached active slot's
        // live physics state + identity into the active-race-car output interface, and the
        // player's slot/engine-state into its player-scoped scalars. Called from both
        // PreSceneUpdate and PostPhysicsUpdate on the console; it is the ONLY producer of
        // RCEntityActiveRaceCarOutputInterface anywhere in the image, and therefore the head
        // of the chain that ends at the director's per-car VehicleInfo
        // (UpdateOutputInterfaces -> BridgeRaceCarEntityInfoToOutput_PostPhysics ->
        //  BrnGameModule::BridgeWorldToDirector -> DirectorIO::InputBuffer::SetRaceCarInfo).
        void UpdateOutputInterfaces(
                RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface,
                RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalCarInterface,
                RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpReplayActiveCarInterface,
                RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpReplayGlobalCarInterface );

        void TransmitCarsInRaceToQueryManager(
                RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface );

        // X360 0x822D1BD0 (PS3 named export 0x17F30C). THE per-frame BOOST publish: fill
        // one BoostOutputInfo per active-car slot -- the player's from the live
        // BoostStrategy + near-miss tracker + controls, every other attached car's
        // mbIsBoosting from its physics boosting timer / start-line boost -- and store the
        // eight records with SetBoostOutputInfoN. Called from PostPhysicsUpdate between
        // UpdateActiveRaceCarColours and UpdateOutputInterfaces (the console's own order).
        // The head of the chain that ends at the HUD boost bar (BridgeWorldVehicleDataToGui
        // -> GUI event 206 -> BoostBarRenderer).
        void UpdateOutputBoostInfo(
                RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface );

        // X360 0x822F4DB0. Bind lpRaceCar to an active-race-car slot and start its asset
        // load. leActiveRaceCarIndex may be E_ACTIVE_RACE_CAR_INDEX_INVALID (-1), in which
        // case the console re-uses the car's own previous slot if it has one and otherwise
        // takes the first slot whose muState is E_STATE_INACTIVE. Returns the slot used.
        //
        // ⚠️ NOT [VMX]: the PC FLAG INVENTORY files this function under [VMX]; the console
        // body contains no vector instruction at all.
        EActiveRaceCarIndex AttachActiveRaceCar( RaceCar* lpRaceCar,
                                                 EActiveRaceCarIndex leActiveRaceCarIndex );

        // ====================================================================
        // THE REMOVE / DETACH PAIR (ghost-car wave 2026-08-17). Both were ledger-`reviewed`
        // and ABSENT from the tree, which is why Car Select's re-spawn left the PREVIOUS
        // player car E_STATE_ACTIVE at the same position (two cars in the
        // `[racecar-lod] banded` probe, coarse-LOD wheel proxies drawing over the real ones,
        // and a ghost left in the junkyard when the player drove off).
        // Signatures are the DecFIGS DWARF's own (BrnRaceCarEntityModule.h:707 / :725) and
        // match the ARTIST asm's register use exactly.
        // ====================================================================

        // X360 0x82304440. Take a global race car out of the world: clear the module's player
        // slot if this WAS the player's car, detach its active slot (if it has one), detach AI
        // control, and RaceCar::RemoveFromWorld it. Eight console callers; see the .cpp banner
        // for which are live on this build.
        void RemoveRaceCar( EGlobalRaceCarIndex leGlobalRaceCarIndex,
                            RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput );

        // X360 0x822FEDF8. The inverse of AttachActiveRaceCar: release the slot's streamed
        // assets (RaceCarStreamer::RemoveVehicleData), tell the AI module the car is no longer
        // simulated, and ActiveRaceCar::Detach the slot -- which is what returns muState to
        // E_STATE_INACTIVE and stops GenerateDispatchLists drawing it.
        void DetachActiveRaceCar(
                RaceCar* lpRaceCar,
                BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInputInterface,
                BrnAI::AIModuleIO::RaceCarAIInterface* lpRaceCarAIInterface,
                CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInputInterface );

        // ====================================================================
        // THE GAME-ACTION CONSUMER (reset-player-car wave 2026-08-01).
        // ====================================================================

        // X360 0x8230BE08. Drain the pre-scene input buffer's game-action queue and apply
        // every action this module owns. PARTIAL SLICE -- see the .cpp banner for exactly
        // which of the ~100 cases are reproduced and why the rest are dropped rather than
        // paraphrased.
        //
        // ⚠️ THREE arguments. Hex-Rays renders one: the console prologue is
        //   r31 = r3 (this); r3 = r4 (the pre-scene INPUT buffer, whose GetGameActionQueue
        //   the very next instruction calls); r20 = r5 (the pre-scene OUTPUT buffer, which
        //   every case forwards to its handler). Recovered from the asm, not the pseudocode.
        void HandleSelectionRequestStreamingAction(const BrnGameState::GameStateModuleIO::CarSelectionRequestStreamingAction* lpAction);
        void HandleGameActions( RaceCarEntityModuleIO::InputBuffer_PreScene* lpInput,
                                RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput );

        // ARTIST 0x822A4700; DecFIGS supplies the BrnResource::ECarType shape.
        void HandleCarStatsUpdate(BrnResource::ECarType leCarType,
                                  s32 liBoostLevel,
                                  s32 liBoostLossLevel);

        // ARTIST 0x823092F0. The currently reconstructed body owns the common
        // non-Showtime mode/boost arming spine; see ModeArming.cpp.
        void HandlePrepareForModeAction(
            const BrnGameState::GameStateModuleIO::PrepareForModeAction* lpPFMAction,
            RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput);

        // X360 0x82304FE8 -- game action 0. Place (or re-spawn) the player's car. THE
        // record CarSelectManager posts to put the player in a junkyard.
        // ⚠️ THREE arguments again (this / lpAction / the pre-scene output buffer; r22/r16/r25
        // in the prologue).
        void HandleResetPlayerCarAction(
                const BrnGameState::GameStateModuleIO::ResetPlayerCarAction* lpAction,
                RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput );

        // [!] HEADER REQUEST (crash parity G68-D11, 2026-09-23): the X360 record of game action 170
        // (DWARF BrnGameState::GameStateModuleIO::SetBoostAction, BrnGameActions.h:1490; DWARF id
        // 162, X360 170), defined in BrnRaceCarEntityModule.cpp and declared here only because
        // HandleSetBoost takes it. DELETE-WHEN BrnGameActions.h grows the record.
        struct SetBoostActionRecord;

        // X360 0x822A4648 (DWARF BrnRaceCarEntityModule.h:758) -- game action 170, the event's
        // boost set (StuntAttackMode's full bar at the start; the online Stunt Run's infinite
        // boost). Acts only on the player's slot, through strategy slots 32/39/40. The console
        // body never reads lpOutput (the call passes only r3/r4).
        void HandleSetBoost( const SetBoostActionRecord* lpAction,
                             RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput );

        // [!] HEADER REQUEST (crash parity G68-D11): the record of game action 4 (DWARF
        // SetPlayerOpponentsAction { Array<CgsID,7u> maOpponents; }, BrnGameActions.h:726 -- DWARF
        // and X360 id 4, size 64), defined in BrnRaceCarEntityModule.cpp; declared here only because
        // HandleSetPlayerOpponentsAction takes it. DELETE-WHEN BrnGameActions.h grows the record.
        struct SetPlayerOpponentsActionRecord;

        // X360 0x822E96D8 (DWARF BrnRaceCarEntityModule.h:740) -- game action 4, posted by
        // GameStateModule::OnPlayerCarChange with the new car's opponent set: pre-stream each
        // opponent's model + default wheels into streamer slots 7, 6, 5, ...
        void HandleSetPlayerOpponentsAction( const SetPlayerOpponentsActionRecord* lpAction );

        // X360 0x822FE5D8. Find a free global race-car slot, Prepare + AddToWorld it at
        // lrTransform, resolve the wheel set when lWheelModelId is null, and publish the AI
        // module's AttachAIControlEvent. Returns the global slot used.
        //
        // ⚠️ The signature is EIGHT parameters and Hex-Rays renders TWENTY-TWO (it counts the
        // callee's own spills into the caller-provided home area). Recovered from the X360
        // home-area slot spacing (r3->sp+0x14, r4->+0x1C, r5->+0x24 ... r10->+0x4C, first
        // stack arg -> +0x54, which is exactly the slot HandleResetPlayerCarAction writes):
        //   r4 lpRaceCarAIInterface  r5 lrTransform  r6 leType  r7 lModelId
        //   r8 lbKeepResetSection    r9 lWheelModelId  r10 lpRivalId (NULLABLE)
        //   sp+0x54 liOpponentIndex
        // lbKeepResetSection is named by the DecFIGS DWARF: it is the fourth member of
        // AttachAIControlEvent (BrnRaceCarAIInterfaces.h:306), which this function is the
        // only producer of.
        EGlobalRaceCarIndex SpawnRaceCar(
                BrnAI::AIModuleIO::RaceCarAIInterface* lpRaceCarAIInterface,
                const Matrix44Affine& lrTransform,
                ERaceCarType leType,
                CgsID lModelId,
                bool lbKeepResetSection,
                CgsID lWheelModelId,
                const CgsID* lpRivalId,
                s32 liOpponentIndex );

        // [RETIRED 2026-08-18, wave Q5 finisher] PublishNewVehicleToDirectorWithoutPhysicsBringUp
        // was declared here. The real leg it stood in for -- ProcessCreateVehicleEvents
        // @0x822FF620 -- is complete as of that wave and publishes the director NewVehicle event
        // itself, on the console's own trigger, so the stand-in was deleted rather than left
        // beside it. See the .cpp banner at the function's old seat.

        // ====================================================================
        // THE ATTACHED -> WAITING -> ACTIVE CHAIN (drivable wave 2026-08-01).
        // These four are what retire PromoteAttachedCarToActiveBringUp.
        // ====================================================================

        // X360 0x822FEBF8. A car's five streamed resources have arrived: publish them into
        // the slot, colour it, and ask for it to be placed on the track.
        // ARTIST passes the AI and scene interfaces but this body does not use them.
        void OnRaceCarResourcesLoaded(
                EActiveRaceCarIndex leActiveRaceCarIndex,
                BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInputInterface,
                BrnAI::AIModuleIO::RaceCarAIInterface* lpRaceCarAIInterface,
                CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInputInterface );

        // X360 0x822CE588. Decide WHERE a freshly loaded car goes and post the request.
        // For a PLAYER car outside a game mode that is ActiveRaceCar::RequestPlaceOnTrack
        // at the car's own AddToWorld pose, which is the start-of-game path this build
        // drives; every other arm goes through RaceCar::RequestResetOnTrack.
        void PlaceRaceCarOnLoad( RaceCar* lpRaceCar );

        // X360 0x822F4880. ⭐ THE ONLY WRITER OF ActiveRaceCar::E_STATE_ACTIVE IN THE XEX.
        // Only caller: PlaceOnTrackManager::PrePhysicsUpdate.
        //
        // ⚠️ FOUR arguments + a VECTOR. Hex-Rays renders `(this, index, transform,
        // vehicleInterface)` and DROPS the velocity, which arrives in v1 (`vmr128 v127, v1`
        // at 0x822F4890 and `vmr128 v1, v127` right before each of the two callees).
        // Incident TEN of the dropped-argument rule.
        void ResetActiveRaceCar( EActiveRaceCarIndex leActiveRaceCarIndex,
                                 const Matrix44Affine& lrTransform,
                                 const Vector3& lrVelocity,
                                 BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInputInterface );

        // X360 inlined at PlaceOnTrackManager::PrePhysicsUpdate (`*(module + 100041) == 0`
        // -> lbIgnoreFatal). Named accessor rather than an offset read from another class.
        bool IsInCarSelectScreen() const { return mbInCarSelectScreen; }

        // X360 inlined at PlaceOnTrackManager::GetValuesForCarSelect @0x822D3470 (`*(module +
        // 100044)`, three reads). DWARF :446 meCarSelectResetType, a
        // ResetPlayerCarAction::CarSelectType (0 = seat on the ground, 1/2 = the junkyard DROP).
        s32  GetCarSelectResetType() const { return meCarSelectResetType; }

    // X360 0x822A34A8 -- &maActiveRaceCars[leActiveRaceCarIndex], in-range checked.
    inline ActiveRaceCar* GetActiveRaceCar(EActiveRaceCarIndex leActiveRaceCarIndex);
    // DecFIGS h:1128 / X360 clone 0x822A36E8, used by const DebugRenderPosition.
    inline const ActiveRaceCar* GetActiveRaceCar(EActiveRaceCarIndex leActiveRaceCarIndex) const;

    // The module's OWN player slot, +0x182F8 (99064) -- the word WorldModule::CalculateVehicleLODs
    // @0x827C3778 reads straight off this object (`lwzx r4, r3, r11`, r3 == WorldModule+0x280 ==
    // mRaceCarEntityModule, r11 == 0x182F8) to force the player's car to LOD 0. DWARF names no
    // accessor (the console reaches the private member directly); this inline is that read given a
    // name, for that one reader. It is NOT the WorldModule mirror meLocalPlayerActiveRaceCarIndex:
    // the mirror is published only for an ATTACHED slot (UpdateOutputInterfaces gate,
    // BrnRaceCarEntityModule.cpp ~:2171), so on the Car Select / junkyard screen -- player car
    // created (mePlayerActiveRaceCarIndex valid, :1699) but not attached to physics -- the mirror
    // was INVALID, nothing lifted RenderParams::mLOD off Reset's 4, and the wheels drew their
    // authored LOD-4 box proxy (car+lights step 1b, boot-verified).
    EActiveRaceCarIndex GetPlayerActiveRaceCarIndex() const { return mePlayerActiveRaceCarIndex; }

    // X360 0x822A3568 -- &maRaceCars[leGlobalRaceCarIndex], in-range checked.
    inline RaceCar* GetGlobalRaceCar(EGlobalRaceCarIndex leGlobalRaceCarIndex);

    // ------------------------------------------------------------------------
    // Active-race-car <-> player-scoring-slot mapping (online scoring). The module
    // keeps maActiveRaceCarForPlayerScoringIndex[player] == the active-race-car slot
    // that the given player is scoring as, or E_ACTIVE_RACE_CAR_INDEX_COUNT (8) as the
    // "no mapping" sentinel (that is the value the X360 stores -- `li r10,8` / `li r27,8`).
    // ------------------------------------------------------------------------

    // X360 0x822A3760 -- set every player slot to the "no mapping" sentinel (8).
    void ClearAllActiveRaceCarToPlayerScoringMappings();

    // X360 0x822BE0A0 -- walk all eight active-race-car slots and put every ATTACHED one
    // into the current game mode. The console INLINES RaceCar::SetInCurrentGameMode
    // @0x822B3F08 at the store pair (`stb r22,0xA6(r31)` / `stb r20,0xA7(r31)`, r22 == the
    // literal 1), so the in-game flag is a CONSTANT true here and only the car-select-allowed
    // flag comes from the argument -- HandlePrepareForModeAction passes its own
    // mbCarSelectAllowedInGameMode (+99143), NOT the online flag.
    void SetAllActiveCarsInGameMode(bool lbCarSelectAllowedInGameMode);

    // X360 0x822A4850 -- put every ATTACHED active-race-car slot into a race-start state.
    // lbIncludePlayer selects whether the PLAYER's own slot (mePlayerActiveRaceCarIndex,
    // +0x182F8) is included; SetupOpponents @0x82307DF0 always passes true.
    void SetAllCarsOnStartLine(ActiveRaceCar::ERaceStartState leRaceStartState,
                               bool lbIncludePlayer);

    // ------------------------------------------------------------------------
    // ⭐ [stuntrace start-grid wave 2026-08-27] THE MODE-START PLACEMENT PAIR.
    // HandlePrepareForModeAction @0x823092F0 -> SetupOpponents @0x82307DF0 ->
    // SetUpPlayerCarForMode @0x823058F8 is the console's "seat the player on the event's
    // start grid" chain. Bodies (and what is parked in each) in ModeArming.cpp.
    // ------------------------------------------------------------------------

    // X360 0x82307DF0. Read the player's current car/wheel ids and the event's first
    // checkpoint AI section, then drive the four mode-start legs. THREE arguments (this /
    // the mode params / the pre-scene OUTPUT buffer; r27/r30/r26 in the prologue).
    void SetupOpponents(const BrnGameState::GameModeParams* lpGameModeParams,
                        RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput);

    // HandlePrepareForModeAction's ONLINE arm (the params' mbIsOnline byte): clear the world of
    // race cars, then seat every roster car -- the local player's first, then each network player
    // -- on the event's start grid or, for the free-burn lobby, wherever the lobby puts it.
    // Bodies in ModeArming.cpp.
    //
    // RemoveRaceCar every global slot that is in the world; the PLAYER's car only when
    // lbRemovePlayerCar is set.
    void RemoveAllRaceCars(RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput,
                           bool lbRemovePlayerCar);

    // The network-car legs of HandleGameActions and PreSceneUpdate. Bodies in
    // BrnRaceCarEntityModule.cpp, next to HandleGameActions.
    //
    // Game action 5 (a remote player changed car): respawn that player's NETWORK car in the same
    // active slot with the new model / wheels at the record's pose, unless both ids are unchanged.
    void HandleSetupNetworkCarAction(
        const BrnGameState::GameStateModuleIO::SetupNetworkCarAction* lpSNCAction,
        RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput);
    // The pre-scene tail: raise / lower each slot's mbNotSendingNetworkUpdates from the input
    // buffer's lost-contact / regained-contact flags.
    void UpdateDisconnectedPlayers(const RaceCarEntityModuleIO::InputBuffer_PreScene* lpInput);

    // One roster car: its transform (the grid slot's start location, or in the free-burn lobby the
    // local car's own pose for the local player and the origin for a network player -- the first
    // update snaps it), the local car's boost strategy + stats from its vehicle-list entry, a
    // SpawnRaceCar (type PLAYER or NETWORK) + AttachActiveRaceCar unless the local car is kept,
    // its base deformation, the AI module's AddCarToCurrentModeEvent, colour + palette, the
    // scoring-slot mapping, the disconnected flag, and the team-boost setup of online modes 11/13.
    // NINE arguments: seven integer ones and the local car's position and direction vectors.
    void AddRaceCarToStartingGridOrFreeburnLobby(
        const BrnGameState::GameStateModuleIO::PrepareForModeAction* lpPFMAction,
        BrnAI::AIModuleIO::RaceCarAIInterface* lpRaceCarAIInterface,
        const BrnGameState::GameModeParams* lpGameModeParams,
        bool lbIsFreeburnLobby,
        bool lbRespawnLocalCar,
        s32 liGridPosition,
        Vector3 lLocalPosition,
        Vector3 lLocalDirection,
        EActiveRaceCarIndex leLocalPlayerActiveRaceCarIndex);

    // X360 0x823058F8. Put the PLAYER's car on start-grid slot 0 (offline: a place-on-track
    // request; online: a remove/respawn at the built transform), carry its colour across,
    // publish the AI module's AddCarToCurrentModeEvent, and map the player's scoring slot.
    // SIX arguments -- Hex-Rays renders twenty-two (the callee's own home-area spills).
    // SetupOpponents supplies lCarModelId / lWheelModelId / lu16StartAISectionIndex.
    void SetUpPlayerCarForMode(const BrnGameState::GameModeParams* lpGameModeParams,
                               RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput,
                               CgsID lCarModelId,
                               CgsID lWheelModelId,
                               u16 lu16StartAISectionIndex);

    // ------------------------------------------------------------------------
    // ⭐ [rival-spawn wave R, 2026-09-02] THE TWO RIVAL LEGS SetupOpponents was parked on.
    // Bodies in BrnRaceCarEntityModule_Rivals.cpp. Shapes are the DWARF's
    // (BrnRaceCarEntityModule.h:779 / :782) and match the prologues (r27/r25/r19 + r6 as a
    // halfword for SetUpAIForMode; r3/r4/r5 for RemoveRivals).
    // ------------------------------------------------------------------------

    // X360 0x82301620. For each of GetOpponentCount() (== miNumRivals) opponents: build the grid
    // transform from the params' StartLocation, SpawnRaceCar an E_RACE_CAR_TYPE_AI car with the
    // OpponentData's model id (or the cop car), AttachActiveRaceCar it, seed the AI module's
    // out-of-range record at the grid pose, publish AddCarToCurrentModeEvent, and map scoring
    // slot N to the new active slot. THREE arguments (this / the mode params / the pre-scene
    // OUTPUT buffer / the destination AI section as a u16 -- `sth r6` at entry).
    void SetUpAIForMode(const BrnGameState::GameModeParams* lpGameModeParams,
                        RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput,
                        u16 lu16DestinationSectionIndex);

    // X360 0x82305E00. RemoveRaceCar every global slot that is in the world and not network
    // driven; the PLAYER's car is skipped unless lbRemovePlayerCar (r5) is set. SetupOpponents
    // @0x82307EF4 passes false.
    void RemoveRivals(RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput,
                      bool lbRemovePlayerCar);

    // X360 0x82305F28. The game-action 195 (E_ACTION_REMOVE_ALL_RIVALS) consumer: RemoveRaceCar
    // every global slot that is in the world (muType != INACTIVE, BrnRaceCar.h:547) AND AI driven
    // (muType == E_RACE_CAR_TYPE_AI, :603). ProgressionManager::UpdateRivals @0x82396298 posts
    // it at the head of every rival refresh (the one ExitCurrentMode requests on leaving an
    // offline event), before re-adding the unlocked roaming rivals one per frame with 196.
    void RemoveAllRivalsFromWorld(RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput);

    // X360 0x82306028 (DWARF .cpp:9143). The game-action 27 / 41 (FINISH_MODE_FINAL_ONLINE /
    // QUIT_MODE_ONLINE) consumer: take every NETWORK-driven car in the world out of the mode and
    // RemoveRaceCar it. Crash parity G68-D10 (2026-09-24).
    void RemoveAllNetworkCarsFromWorld(RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput);

    // X360 0x82301A50. The game-action 196 (E_ACTION_ADD_RIVAL) consumer: spawn one roaming
    // freeburn rival -- an E_RACE_CAR_TYPE_AI car of the Rival's car -- looking down the record's
    // heading from its spawn position, then seed the AI module's out-of-range record for it at
    // the record's AI section with the rival's district and medals-to-unlock.
    void AddRivalCar(const BrnGameState::GameStateModuleIO::AddRivalCarAction* lpAction,
                     RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput);

    // X360 0x82307A30. The game-action 39 (E_ACTION_STOP_MODE) consumer -- the WORLD-side mode
    // teardown, one per mode exit: every car back to RACING, the scoring map cleared, the player's
    // base-deformation pair restored from its mode-change stash, infinite boost off, the player's
    // car out of the mode, crash play / showtime deactivated (unless the NEXT mode is showtime),
    // the rivals removed when the mode asked for it, the boost event modifier restored, the
    // indicators off, the eight mode words/bytes cleared, and the winner's car reset or placed
    // back on track.
    void HandleStopModeAction(const BrnGameState::GameStateModuleIO::StopModeAction* lpAction,
                              RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput);

    // X360 0x822A37C8 -- find the player slot currently mapped to leActiveRaceCarIndex
    // and reset it to the sentinel (8). If no slot maps to it, do nothing.
    void ClearActiveRaceCarToPlayerScoringMapping(EActiveRaceCarIndex leActiveRaceCarIndex);

    // X360 0x822A3888 -- map player scoring slot lePlayerScoringIndex to
    // leActiveRaceCarIndex.
    void SetActiveRaceCarForPlayerScoringIndex(
        BrnGameState::GameStateModuleIO::EPlayerScoringIndex lePlayerScoringIndex,
        EActiveRaceCarIndex leActiveRaceCarIndex);

    // X360 0x822A3918 -- copy the whole player-scoring mapping into the active-race-car
    // output interface (one SetActiveRaceCarIndex per slot).
    void CopyActiveRaceCarToPlayerScoringMappingToOutput(
        BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpOutputInterface);

    // X360 0x822A3A20 -- (mxGameModeFlags & lxFlagMask) != 0.
    bool GetGameModeFlag(u64 lxFlagMask) const;
    // X360 0x822A4A38 (DWARF :884). Global race cars in the world carrying persistent damage.
    s32 GetPersistentDamageCarCount() const;

    // ------------------------------------------------------------------------
    // Tail-state bookkeeping bodied in BrnRaceCarEntityModule.cpp. These touch only
    // the module's own scalar tail at attested offsets (plus, for UpdateTailgateTimer,
    // a forward-declared sibling query), so they reconstruct BY NAME without modelling
    // the un-homed RaceCar/ActiveRaceCar/manager interiors.
    // ------------------------------------------------------------------------

    // X360 0x822A47A8 -- append leTrainingType to mePendingTrainingRequestQueue (range +
    // capacity asserted), bumping miPendingRequestCount. No-op if the ring is full.
    void AddTrainingRequest(BrnProgression::ETrainingType leTrainingType);

    // ⭐ [tut-ticker] X360 0x822F6BE8 -- drain this module's pending game events into the
    // post-physics output buffer's game-event queue. Console call site: the tail of
    // PostPhysicsUpdate @0x82307938, immediately before SendStreamerEvents. PARTIAL: the
    // training-request drain (game event 113 per ring entry, LIFO) is whole; the leading
    // "player car changed" arm (event 9) reaches three un-homed tail members inside
    // maTailPadB0 and is named, not faked -- see the body.
    void SendGameEvents(RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpOutput);

    // ⭐ [tut-ticker] X360 0x822BE020 -- map the player car's BrnResource::ECarType to its
    // DRIVES_*_CAR training tip and queue it (the "this text explains the car" tutorial):
    // dword_82CDB4A4 = { 6, 7, 5 } == DANGER->TRAINING_DRIVES_DANGER_CAR,
    // AGGRESSION->TRAINING_DRIVES_AGGRESSION_CAR, STUNTS->TRAINING_DRIVES_STUNT_CAR
    // (table READ FROM THE IMAGE 2026-08-24; anchored against the recovered training
    // string table at 0x82CDBF40 -- see the wave log). Caller: HandleGameActions case 77.
    void HandleCarTypeTrainingMessage(u32 luCarType);

    // X360 0x822CE508 -- if the player car is tailgating any other race car, accumulate
    // lfDeltaTime into mfCurrentTailgateDuration; otherwise reset it to 0. Returns the
    // tailgating predicate. (Calls IsPlayerCarTailgatingOtherRaceCars, declared below.)
    bool UpdateTailgateTimer(f32 lfDeltaTime);

    // One frame of near-miss bookkeeping: publish the player's speed and crashing flag to
    // mNearMissManager (a crashing player also breaks the chain), remember every other active
    // race car that is crashing, drain the vehicle manager's fine crashing-traffic queue into
    // the traffic half, then step NearMissManager::Update. The console calls it from
    // PostPhysicsUpdate, inside the second sim-paused skip. Body in
    // BrnRaceCarEntityModule_NearMissTailgate.cpp.
    void UpdateNearMisses(RaceCarEntityModuleIO::InputBuffer_PostPhysics* lpInput,
                          RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpOutput);

    // X360 0x822FF5B0 (DWARF BrnRaceCarEntityModule.h:578, body :4543). One Power Parking scorer
    // step: mPowerParkingManager.Update(meGameModeType, mfTimeStep, the player's active car,
    // &mPlayerVehicleControls, lpOutput->GetGameEventQueue()). lpInput is never read. Called from
    // PostPhysicsUpdate @0x8230778C right after UpdateNearMisses, gated like ProcessPowerParking.
    // Body in BrnRaceCarEntityModule_NearMissTailgate.cpp (crash parity FX-SCENEMGR item 4).
    void UpdatePowerParking(const RaceCarEntityModuleIO::InputBuffer_PostPhysics* lpInput,
                            RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpOutput);

    // The PRODUCER of both near lists. Drains the traffic module's two proximity collections
    // out of the post-scene input buffer's traffic->race-car interface into mNearMissManager,
    // and tallies the traffic half for power parking while the module is out of a game mode.
    // The console calls it from PostSceneUpdate, first callee after the lock pair, skipped on
    // a network-catchup frame. Body in BrnRaceCarEntityModule_NearMissTailgate.cpp.
    void UpdateTrafficAndRaceCarNearMisses(RaceCarEntityModuleIO::InputBuffer_PostScene* lpInput);

private:
    // True when the player car sits inside the tailgate cone behind any other active race car
    // (reference point 2 m along that car's heading, 20 m along its reversed heading, a
    // 0.346 rad cone, the other car above 30 mph and the two velocities within 90 mph of each
    // other). Latches the slot into meIndexOfCarPlayerIsTailgating, or resets it to INVALID.
    // The parameter is the active-car array BASE, not the player's slot -- the console forms
    // the player's slot from it. Body in BrnRaceCarEntityModule_NearMissTailgate.cpp.
    bool IsPlayerCarTailgatingOtherRaceCars(
        EActiveRaceCarIndex lePlayerActiveRaceCarIndex,
        const ActiveRaceCar* lpPlayerActiveRaceCar);

    // Compiled-never-called offsetof layout lock (see definition below).
    void LockLayout_();

    // [H1 district wave 2026-08-25] X360 0x822F5718 -- sample the local player car's
    // position against the module's district map each post-physics tick; when the district
    // changes, latch the new WorldRegion into mCurrentWorldRegion and post the 8-byte
    // {county, district} pair as game event 115 into the caller's queue. The console
    // caller (PostPhysicsUpdate @0x8230769C) passes lpOutput->GetGameEventQueue().
    void UpdateCurrentWorldRegion(RaceCarEntityModuleIO::GameEventQueue* lpGameEventQueue);

    // ========================================================================
    // THE ROAD-RAGE RIVAL RANGE LOOP (rival range-loop wave, lane W1, 2026-09-05).
    // Bodies in BrnRaceCarEntityModule_Range.cpp; every signature below is from the X360
    // asm prologue, not the pseudocode (which renders all five with junk argument lists).
    // ========================================================================

    // X360 0x822FF8F8, called from PreSceneUpdate @0x8230E2C0 (r4 == the pre-scene OUTPUT
    // buffer -- the same r29 UpdateRaceCars_PreScene and UpdateStreaming receive). Two passes
    // over the car arrays: pass 1 pushes in-range rivals OUT (or wraps them), pass 2 pulls
    // out-of-range rivals back IN.
    void UpdateInAndOutOfRangeCars( RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput );

    // X360 0x822F5578 (62 insns). DWARF BrnRaceCarEntityModule.h:662 `void UpdateRaceCars_PreScene(
    // OutputBuffer_PreScene*)`. Its only caller is PreSceneUpdate @0x8230E288, in the un-paused arm
    // immediately before the rate-gated UpdateInAndOutOfRangeCars. ActiveRaceCar::Update_PreScene
    // for all eight slots, then the start-of-mode intro countdown (mfIntroTimer): when it runs out
    // the rivals are released onto a ROLLING start (the drive-by start). Body in
    // BrnRaceCarEntityModule.cpp beside PreSceneUpdate.
    void UpdateRaceCars_PreScene( RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput );

    // X360 0x822F5668 (44 insns). DWARF BrnRaceCarEntityModule.h:665 `void
    // UpdatePropBoundingBoxes_PreScene(OutputBuffer_PreScene*)` (crash parity CC-3 = G60-D3). Its
    // only caller is PreSceneUpdate @0x8230E408, between WriteUpdatedAIData and the output-interface
    // fetches: every slot whose car is in the scene and deformed this frame has its dynamic scene
    // volume replaced by ActiveRaceCar::GetPropCollisionBox. Body beside UpdateRaceCars_PreScene.
    void UpdatePropBoundingBoxes_PreScene( RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput );

    // X360 0x822F5830, called from PostPhysicsUpdate @0x823076B0 with
    // lpOutput->GetGameEventQueue() (the sub_822B67D0 accessor at 0x823076A4 -- the same one
    // UpdateCurrentWorldRegion is handed one call earlier). Posts every pending hiding record
    // as game event 40 and empties the array.
    void UpdateHidingEvents( RaceCarEntityModuleIO::GameEventQueue* lpGameEventQueue );

    // X360 0x822D2988. Queue one {slot, hiddenTime} record for lpRaceCar. Its ONLY caller is
    // IsRaceCarWrappable @0x822E9E18.
    void SetHiddenDelay( RaceCar* lpRaceCar );

    // X360 0x822E9E18. True when the car may be teleported ("wrapped") relative to the player
    // this frame; queues the hiding record as a side effect on the arms that say yes.
    bool IsRaceCarWrappable( RaceCar* lpRaceCar );

    // X360 0x822E9188, called from PostPhysicsUpdate @0x82307604 (r4 == the post-physics INPUT
    // buffer). Copies the AI's pose for every car it is simulating out of range into that car's
    // RaceCar slot, so a car that comes back in range comes back where the AI drove it to.
    void ReadOutOfRangeRaceCarDataFromAI( RaceCarEntityModuleIO::InputBuffer_PostPhysics* lpInput );

    // X360 0x822BA068. The global RaceCar behind the local player's active slot.
    RaceCar* GetPlayerRaceCar();

    void ProcessPlayerVehicleInput(f32 lfTimeStep,
        const RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpInput,
        RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput);
    void UpdateBoost(f32 lfTimeStep,
        const RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpInput,
        RaceCarEntityModuleIO::GameEventQueue* lpEventQueue);
    s32 GetDamagedCarCount() const;
    void ProcessTakedownEvents(const RaceCarEntityModuleIO::TakedownEventQueue* lpQueue);
    void UpdateActiveCars(f32 lfTimeStep, f32 lfTimeStepMultiplier,
        f32 lfAcceleration, f32 lfBraking,
        RaceCarEntityModuleIO::GameEventQueue* lpGameEvents,
        BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInput);
    void SendAddedForCollisionStateToPhysics(BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInput);
    void StorePlayerRoutePortalPositions(const RaceCarEntityModuleIO::InputBuffer_PostPhysics* lpInput);

    // Native containment follows DecFIGS declaration order, with ARTIST accesses
    // closing each constituent boundary. Existing source spellings are retained.
    // The leading ModuleSingleBuffered base is the actual native owner (0x228).
    f32 mfClippedTime;                              // ARTIST +0x228
    s32 mePrepareStage;                             // +0x22C
    s32 miPrepareCarIndex;                          // +0x230, meReleaseStage in DWARF
    s32 meLoadGlobalResourcesStage;                 // +0x234, meInitialLoadStage
    s32 miExpectedResponseCount;                    // +0x238, miResourceCount
    CgsResource::ResourceHandle mDistrictMapResourceHandle; // +0x23C
    BrnWorld::WorldRegion mCurrentWorldRegion;       // +0x244
    RaceCar maRaceCars[E_GLOBAL_RACE_CAR_INDEX_COUNT];       // +0x250
    ActiveRaceCar maActiveRaceCars[E_ACTIVE_RACE_CAR_INDEX_COUNT]; // +0x1A60
    CgsContainers::BitArray<8u> mabResetThisFrame;    // +0x100E0
    CgsModule::EventReceiverQueue<4096, 16> mReceiverQueue; // +0x100E8
    RaceCarStreamer mRaceCarStreamer;               // +0x11100
    PlaceOnTrackManager mPlaceOnTrackManager;       // +0x17850
    BoostManager mBoostManager;                    // +0x17890
    RaceCarEntityModuleDebugComponent mRCEntityModuleDebugComponent; // +0x17CF0
    BoostDebugComponent mBoostDebugComponent;       // +0x17E50
    NearMissManager mNearMissManager;               // +0x17E68
    AirTimeManager mAirTimeManager;                 // +0x180D8
    TrafficCheckManager mTrafficCheckManager;       // +0x180E8
    CrashPlayManager mCrashPlayManager;             // +0x180F0
    PowerParkingManager mPowerParkingManager;       // +0x18250
    f32 mfCurrentTailgateDuration;                  // +0x182F0
    EActiveRaceCarIndex meIndexOfCarPlayerIsTailgating; // +0x182F4
    EActiveRaceCarIndex mePlayerActiveRaceCarIndex;  // +0x182F8
    CgsWorld::WorldMap2D mWorldMap2D;                // +0x18300
    Attrib::Gen::surfacelist mSurfaceList;           // +0x18330
    s32 miOpponentCount;                           // +0x18340
    bool mbIsInGameMode;
    bool mbIsInOnlineGameMode;
    bool mbOnlineModeJustFinished;
    bool mbCarSelectAllowedInGameMode;
    bool mbWaitingForStreaming;
    bool mbDisplayPlayerCarPosition;
    bool mbRivalDebug;
    bool mbRenderCarsDuringCrash;
    bool mbRenderWheels;
    bool mbSixaxisSteeringEnabled;
    bool mbPaybackSixaxisSteering;
    bool mbRenderRaceCarCoronas;
    bool mbSpawnAIBehindStartGrid;
    bool mbPlayerRollsOnEventStart;
    bool mbPlayerDonutsOnEventStart;
    bool mbUseRankUpAISpawnSeparation;
    bool mbModeStartedPlaying;
    u64 mxGameModeFlags;                           // +0x18358
    bool mabCarSelectWaitForStreaming[8];           // +0x18360
    BrnGameState::GameStateModuleIO::EGameModeType meGameModeType; // +0x18368
    BrnPhysics::Vehicle::AggressiveDrivingFlags mAggressiveDrivingFlags; // +0x1836C
    bool mbSendStreamingComplete;
    BrnProgression::ETrainingType mePendingTrainingRequestQueue[KI_TRAINING_REQUEST_QUEUE_SIZE];
    s32 miPendingRequestCount;
    f32 mfTimeStep;                                // +0x18398, mfSimTimerTimeStep
    f32 mfIntroTimer;
    f32 mfSimTime;
    f32 mfTimeStepMultiplier;
    PlayerVehicleControls mPlayerVehicleControls;   // +0x183A8
    BrnNetwork::EPaybackType meActivePaybackType;
    EActiveRaceCarIndex meActivePaybackAggressor;
    Matrix44Affine mCameraTransform;               // +0x183F0
    bool mbDEBUGPlayerUsingReset;
    const BrnResource::VehicleList* mpVehicleList;
    const BrnResource::WheelList* mpWheelList;
    CgsResource::ResourcePtr<BrnWorld::GlobalColourPalette> mCarColoursResource;
    CgsNumeric::Random mDeterministicRandom;        // +0x18460
    CgsNumeric::Random mNonDeterministicRandom;     // +0x18490
    f32 mfLastPlayerCarSpeed;
    f32 mfRandomBoostTime;
    bool mbRandomBoostOn;
    f32 mfResetOnWaterHeight;
    s32 miPlayerBaseDeformationTypeMirror;
    s32 miPlayerBaseDeformationTypeSaved;
    f32 mfPlayerBaseDeformAmountMirror;
    bool mbPlayerBaseDeformRequestPending;
    f32 mfPlayerBaseDeformAmountSaved;
    StoredStompeeData mStoredStompees[E_ACTIVE_RACE_CAR_INDEX_COUNT]; // +0x184F0
    s32 miStoredStompeeCount;                       // +0x185F0
    bool mbInReplay;                               // +0x185F4
    bool mbReplayRestored;                         // +0x185F5

    // ARTIST calls the serialiser at 0x18600. Its constrained class alignment
    // closes the replay tail while preserving the genuine 64-byte matrix type.
    BrnReplays::RaceCarEntitySerialiser mReplaySerialiser;
    Matrix44Affine mReplayTransform;
    bool mbInvalidatedRaceCars;
    bool mabRaceCarsRendered[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    bool mbInCarSelectScreen;
    bool mbInCarModScreen;
    s32 meCarSelectResetType;
    bool mbCarSelectDontStreamAudio;
    bool mbHACK_ExitingCarSelectWaitForAudio;
    RaceCarNeedsHidingEventRecord mHidingEvents[8];
    s32 miHidingEvents;
    bool mbOncomingTimerActive;
    f32 mfOncomingNoClueTimer;
    Vector2 mPlayersCurrentRouteNodePosition;
    Vector2 mPlayersNextRouteNodePosition;
    bool DEBUG_mbSpawnDebugRaceCarThisFrame;
    bool DEBUG_mbCleanDebugRaceCarThisFrame;
    bool DEBUG_mbDebugRaceCarIsActive;
    Vector3 DEBUG_mDebugRaceCarPosition;
    Vector3 DEBUG_mDebugRaceCarDirection;
    f32 DEBUG_mfDebugRaceCarVelocity;
    RaceCar* DEBUG_mpDebugRaceCar;
    bool DEBUG_mbOverrideDamage;
    f32 DEBUG_mfVehicleScratchAmount;
    f32 DEBUG_mfVehicleDustAmount;
    f32 DEBUG_mfVehicleCrumpleAmount;
    f32 DEBUG_mfSelfIlluminationR;
    f32 DEBUG_mfSelfIlluminationG;
    f32 DEBUG_mfSelfIlluminationB;
    bool DEBUG_mbOverrideCarColor;
    f32 DEBUG_mfOverridePaintColorR;
    f32 DEBUG_mfOverridePaintColorG;
    f32 DEBUG_mfOverridePaintColorB;
    f32 DEBUG_mfOverridePearlColorR;
    f32 DEBUG_mfOverridePearlColorG;
    f32 DEBUG_mfOverridePearlColorB;
    bool DEBUG_mbOverrideCarPalette;
    s32 DEBUG_miPaletteIndex;
    s32 DEBUG_miColourIndex;
    EActiveRaceCarIndex maActiveRaceCarForPlayerScoringIndex[BrnGameState::GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT];
    bool mbSetRaceCarEliminated;
    EActiveRaceCarIndex mCarToCrashActiveIndex;
    s32 miPreSceneUpdatePM;
    s32 miPostSceneUpdatePM;
    s32 miPrePhysicsUpdatePM;
    s32 miPostPhysicsUpdatePM;

};

// X360 0x822A34A8. Asserts the index is in [E_ACTIVE_RACE_CAR_INDEX_0,
// E_ACTIVE_RACE_CAR_INDEX_COUNT) (the asm only emits the upper-bound branch since
// the lower bound on a non-negative enum is trivially true), then returns the
// element address: this + 6752 + 7376*index == &maActiveRaceCars[index].
inline ActiveRaceCar*
RaceCarEntityModule::GetActiveRaceCar(EActiveRaceCarIndex leActiveRaceCarIndex)
{
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    return &maActiveRaceCars[leActiveRaceCarIndex];
}

// X360 0x822A36E8 -- the DecFIGS const overload used by DebugRenderPosition.
inline const ActiveRaceCar*
RaceCarEntityModule::GetActiveRaceCar(EActiveRaceCarIndex leActiveRaceCarIndex) const
{
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    return &maActiveRaceCars[leActiveRaceCarIndex];
}

// X360 0x822A3568. Asserts the index is in [E_GLOBAL_RACE_CAR_INDEX_0,
// E_GLOBAL_RACE_CAR_INDEX_COUNT), then returns the element address:
// this + 592 + 176*index == &maRaceCars[index].
inline RaceCar*
RaceCarEntityModule::GetGlobalRaceCar(EGlobalRaceCarIndex leGlobalRaceCarIndex)
{
    CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0,
               "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");
    CGS_ASSERT(leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT,
               "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");

    return &maRaceCars[leGlobalRaceCarIndex];
}

// Layout lock. offsetof on the private array members needs member-scope access
// under MSVC, so the X360-proven offsets are asserted here (compiled, never
// called). maRaceCars @+0x250 and maActiveRaceCars @+0x1A60 are the offsets the
// accessor asm bakes in (this + 592 / this + 6752).
inline void RaceCarEntityModule::LockLayout_()
{
    // ARTIST closes the genuine base and the named constituent boundaries.
    // All replay/end bounds remain hard requirements of the constrained
    // declaration; preserve them when extending the module.
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mfClippedTime) == 0x228), "ARTIST clipped time");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mDistrictMapResourceHandle) == 0x23C), "ARTIST district handle");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mCurrentWorldRegion) == 0x244), "ARTIST world region");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mabResetThisFrame) == 0x100E0), "ARTIST reset bits");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mReceiverQueue) == 0x100E8), "ARTIST receiver queue");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mRaceCarStreamer) == 0x11100), "ARTIST streamer");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mPlaceOnTrackManager) == 0x17850), "ARTIST placement manager");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mBoostManager) == 0x17890), "ARTIST boost manager");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mRCEntityModuleDebugComponent) == 0x17CF0), "ARTIST debug component");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mBoostDebugComponent) == 0x17E50), "ARTIST boost debug component");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mNearMissManager) == 0x17E68), "ARTIST near-miss manager");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mAirTimeManager) == 0x180D8), "ARTIST air-time manager");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mTrafficCheckManager) == 0x180E8), "ARTIST traffic-check manager");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mCrashPlayManager) == 0x180F0), "ARTIST crash-play manager");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mPowerParkingManager) == 0x18250), "ARTIST power-parking manager");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mWorldMap2D) == 0x18300), "ARTIST district map");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mSurfaceList) == 0x18330), "ARTIST surface list");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, meGameModeType) == 0x18368), "ARTIST game mode");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mfTimeStep) == 0x18398), "ARTIST time step");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mPlayerVehicleControls) == 0x183A8), "ARTIST controls");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mCameraTransform) == 0x183F0), "ARTIST camera transform");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mCarColoursResource) == 0x1843C), "ARTIST palette resource");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mDeterministicRandom) == 0x18460), "ARTIST deterministic random");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mNonDeterministicRandom) == 0x18490), "ARTIST nondeterministic random");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mStoredStompees) == 0x184F0), "ARTIST stored stompees");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, miStoredStompeeCount) == 0x185F0), "ARTIST stompee count");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mbInReplay) == 0x185F4), "ARTIST replay flag");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mbReplayRestored) == 0x185F5), "ARTIST restored flag");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, maActiveRaceCars) == 0x1A60), "ARTIST active slots");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mfCurrentTailgateDuration) == 0x182F0), "ARTIST tailgate duration");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, meIndexOfCarPlayerIsTailgating) == 0x182F4), "ARTIST tailgate index");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mePlayerActiveRaceCarIndex) == 0x182F8), "ARTIST player index");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, maRaceCars) == 0x250),
                  "maRaceCars @+0x250 (== 592)");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mReplaySerialiser) == 0x18600), "ARTIST replay serialiser");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mbInvalidatedRaceCars) == 0x186C0), "ARTIST invalidated cars");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, mabRaceCarsRendered) == 0x186C1), "ARTIST rendered flags");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntityModule, DEBUG_mbOverrideDamage) == 0x18778), "ARTIST damage override");
    CGS_STATIC_ASSERT((sizeof(RaceCarEntityModule) == 0x18800), "ARTIST module size");
}

}
