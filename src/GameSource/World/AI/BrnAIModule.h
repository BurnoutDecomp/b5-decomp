#pragma once

#include "types.hpp"
#include "SharedClasses/BrnSharedConstants.h"   // BrnUpdateSet
#include "GameSource/World/AI/Route/BrnRouteMapModule.h"
#include "GameSource/World/AI/BrnAICar.h"                              // AICar (maAICars[35], BY VALUE)
#include "GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.h"   // ResetOnTrackManager
// ---- ADDITIVE (aiwave lane A1, 2026-09-03): the drive spine's embedded state ----
#include "GameSource/World/AI/BrnAIDriver.h"                          // AIDriver (maAIDrivers[8], BY VALUE)
#include "GameSource/World/AI/BrnAIBuzzBy.h"                          // BuzzBy (mBuzzBy, BY VALUE)
#include "GameSource/World/AI/BrnAISharedConstants.h"                 // ERoundRobinType
#include "GameSource/World/AI/RaceBalancing/BrnRaceBalancingManager.h"   // RaceBalancingManager (BY VALUE)
#include "GameSource/World/AI/BrnRouteRequestManager.h"             // RouteRequestManager (mRouteRequestManager, BY VALUE)
#include "GameSource/World/AI/Route/BrnRoute.h"                       // Route (mMasterRoute, BY VALUE)
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                 // CgsNumeric::Random (mRandom)
#include "GameSource/BurnoutConstants.h"                              // EActiveRaceCarIndex / EGlobalRaceCarIndex
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"     // the module base
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"   // EventReceiverQueue<1024,16>
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"  // CgsResource::ResourceHandle
#include "GameSource/Physics/ContactSpies/BrnContactSpyInterface.h"    // BrnPhysics::ContactSpy::ContactSpyInterface (mContactSpyInterface, DWARF :361)

#include <eathread/eathread_rwmutex.h>

// ⚠️ CLASS-KEY CORRECTED 2026-08-25 (aimodule wave): AllocatorList was forward-declared here
// as a `struct`, but BrnGameDataAllocatorList.h defines it as a `class`. MSVC mangles the class
// key into the parameter type (V vs U), so BrnAIModule.cpp emitted
// ?Prepare@...PEAUAllocatorList... while every caller (compiled against the real header) asked
// for ...PEAVAllocatorList... -- an LNK2019 the moment the body left WorldLinkStubs.cpp, where it
// had always been compiled against the real header. A silent trap for any TU that only ever saw
// this line.
namespace BrnResource { namespace GameDataIO { template <int N> class AllocatorListT; class AllocatorList; } }

namespace CgsModule { struct IOBufferStack; }

// ---- ADDITIVE (aiwave lane A7, 2026-09-03) -- pointer-only parameter types of the eight
//      game-action handlers below. Declared, not included: BrnGameActions.h /
//      BrnGameModeParams.h are heavy GameState headers and this class only takes pointers.
//      BrnAIModule_Events.cpp includes both for the complete types it dereferences.
//      RaceCarReachedCheckpointAction is DECLARED ONLY ANYWHERE -- its handler is a park
//      (BrnAI::AIModule::OnRaceCarReachedCheckpoint @0x8278A658 is an ARTIST export hole), so
//      nothing in the tree dereferences it yet. ----
namespace BrnGameState
{
    // CLASS KEY: BrnGameModeParams.h:138 defines it as a `class` -- MSVC mangles the key into the
    // parameter type (V vs U), so a `struct` forward declaration here would emit an OnModeStart
    // no caller can link against (the AllocatorList trap documented at the top of this file).
    class GameModeParams;
    namespace GameStateModuleIO
    {
        struct RaceCarReachedFinishAction;
        struct RaceCarReachedCheckpointAction;
        struct FinishedModeNotifyAction;
        struct OnPlayerTakedownAction;
    }
}

namespace BrnAI
{
namespace AIModuleIO { struct OutputBuffer; struct InputBuffer; struct InputBuffer_PostPhysics; }
namespace AIModuleIO { struct RaceCarAIInterface; }      // SetSuitabilityForAggression arg (BrnRaceCarAIInterfaces.h)
namespace RouteMapModuleIO { struct InputBuffer; }        // UpdateCars arg: the transient "Route" IO buffer (BrnRouteMapModuleIO.h:239)

// DWARF BrnAIModule.h:58/:59 -- the two roster caps the module's arrays and loops are sized by.
// The console bakes them as literals (35 / 8) into every GetAICar/GetAIDriver and loop bound.
const s32 KI_MAX_OUT_OF_RANGE_RACE_CARS = 35;
const s32 KI_MAX_ACTIVE_RACE_CARS       = 8;

struct Route;
struct AICar;
struct AISectionsData;

// ⭐⭐ 2026-08-25 (aimodule wave): AIModule NOW DERIVES FROM
// CgsModule::ModuleSingleBuffered, which is what the X360 has done all along --
// AIModule::Construct @0x82794D08 opens with
// `bl CgsModule::ModuleSingleBuffered::Construct(this)`, Prepare stage 1 is
// `ModuleSingleBuffered::Prepare(this)`, Release stage 1 is its Release and Destruct opens with
// its Destruct. The previous model stood a bare `u32 muBaseVTable = 0x820D0D98` in for the base
// and could not call any of them. This is the SAME defect class the crash-exit wave found one
// level up (a MODULE THAT DECLARES NONE OF ITS LIFECYCLE, so every lifecycle call lands on a
// base default that does nothing for it) -- here it was worse, because there was no base to
// bind to at all.
//
// The base's vtable slot order is this tree's (a virtual dtor at slot 0 -- see the banner in
// CgsModule.h). Nothing here indexes a module vtable numerically; the three console indirect
// calls this class reproduces (RouteMapModule slot 16 == its own Prepare(handle), slots 2/3 ==
// its Release/Destruct) are written BY NAME.
class AIModule : public CgsModule::ModuleSingleBuffered
{
public:
        // X360 0x82794D08. Reached by the wired WorldModule::Construct @0x827CF540 fleet cascade.
        void Construct() override;
        // ---- ADDITIVE (attested by WorldModule::DestructWorld @0x827BD0F0) ----
        // Still a boot gate in WorldLinkStubs.cpp -- see the note there.
        void Destruct() override;
        // ---- ADDITIVE (attested by WorldModule::ReleaseWorld @0x827BCE58) ----
        // Still a boot gate in WorldLinkStubs.cpp -- see the note there.
        bool Release() override;

        // X360 0x82798070 -- the 6-stage prepare machine. NOT the base's virtual Prepare()
        // (different signature): the console calls this one directly from WorldModule::Prepare
        // stage 11 and it calls the base's inside its own stage 1.
        bool Prepare( BrnResource::GameDataIO::AllocatorList* lpAllocatorList,
                      AIModuleIO::OutputBuffer* lpOutputBuffer );

        // ---- ADDITIVE (WorldModule::Update @0x827D63E8; DWARF BrnAIModule.h
        //      :232/:235). Declaration-only; bodies gated in WorldLinkStubs.cpp
        //      until this module's own TU lands. ----
        void Update( CgsModule::IOBufferStack* lpInputBufferStack,
                     CgsModule::IOBufferStack* lpOutputBufferStack,
                     const AIModuleIO::InputBuffer* lpInputBuffer,
                     AIModuleIO::OutputBuffer* lpOutputBuffer,
                     BrnUpdateSet lUpdateSet );
        void PostPhysicsUpdate( const AIModuleIO::InputBuffer_PostPhysics* lpInputBuffer );

        // ---- THE RESET-ON-TRACK PUMP, AI half (resetpump wave 2026-08-26). Bodies in
        //      BrnAIModule_ResetPump.cpp, alongside the Update slice that calls them. ----

        // X360 @0x8278A7A8 (185 insns). Drain the input buffer's ResetOnTrackRequest queue into
        // the manager's own 35-deep pending array.
        void ProcessRequestInterface( const AIModuleIO::InputBuffer* lpInputBuffer,
                                      AIModuleIO::OutputBuffer* lpOutputBuffer,
                                      BrnUpdateSet lUpdateSet );

        // X360 @0x8279ABB0 (192 insns). Run ResetOnTrackManager::Update for this frame and
        // apply its results to the AI cars.
        void UpdateResetOnTrackManager( AIModuleIO::AIModuleResultInterface* lpResults,
                                        f32 lfTime );

        // ---- THE DRIVE LEGS of the per-frame spine (aiwave lane A1, 2026-09-03). Bodies in
        //      BrnAIModule_Drive.cpp; the console-ordered call list of AIModule::Update is the
        //      banner of that file and scratch/aiwave/A1_update_spine.md. ----

        // X360 @0x8279A518 (89 insns) -- DWARF BrnAIModule.cpp:1118 `UpdateCars(float32_t,
        // InputBuffer*, OutputBuffer*)`: f1 = the frame's sim time step, r5 = the transient
        // RouteMapModuleIO "Route" input buffer, r6 = the AI's own output buffer. The body reads
        // NEITHER buffer (only this + f1); they are carried for the prototype.
        void UpdateCars( f32 lfTimeStep,
                         RouteMapModuleIO::InputBuffer* lpRouteInputBuffer,
                         AIModuleIO::OutputBuffer* lpOutputBuffer );
        // X360 @0x8279B148 (204 insns) -- DWARF :1374 `UpdateDrivers(const InputBuffer*,
        // OutputBuffer*, Vector3, float32_t)`: v1 = the player car position the caller read
        // from RaceCarAIInterface::GetPlayerCarPosition, f1 = the sim time step.
        void UpdateDrivers( const AIModuleIO::InputBuffer* lpInputBuffer,
                            AIModuleIO::OutputBuffer* lpOutputBuffer,
                            Vector3 lPlayerCarPosition,
                            f32 lfTimeStep );
        // X360 @0x827957F0 (392 insns) -- DWARF :1999. The INPUT side: copies each active
        // driver's race-car snapshot (matrix/velocity/speed/section/8 flags) into its AICar.
        void StoreDrivenCarData( const AIModuleIO::InputBuffer* lpInputBuffer );
        // X360 @0x8278A970 (131 insns) -- DWARF :2309. Nearby traffic + nearby AI -> each
        // driver's avoidance list.
        void SortTrafficIntoAICars( const AIModuleIO::InputBuffer* lpInputBuffer );
        // X360 @0x82795E10 (153 insns) -- DWARF :2130. THE OUTPUT SIDE: one BrnAIDriverControls
        // record per active-car slot into the output buffer's VehicleDriverInputInterface queue.
        void ProcessAIVehicleInputs( AIModuleIO::OutputBuffer* lpOutputBuffer );
        // X360 @0x8276E910 (66) / @0x8276EA18 (67) -- DWARF :2189 / :2224.
        void ProcessOutOfRangeVehicles( AIModuleIO::OutputBuffer* lpOutputBuffer );
        void ProcessInRangeVehicles( AIModuleIO::OutputBuffer* lpOutputBuffer );
        // X360 @0x8276EB28 (98 insns) -- DWARF :2264. Per-car distance-to-checkpoint + AI
        // section into the AICarOutputInterface, plus the player's route.
        void ExportCarData( AIModuleIO::OutputBuffer* lpOutputBuffer );

        // X360 @0x82765B90 -- DWARF BrnAIModule.h:450. `assert(index < 8)` ("Invalid driver
        // index: ") then `return this + 192080 + 7536 * index`, i.e. &maAIDrivers[index].
        AIDriver* GetAIDriver( EActiveRaceCarIndex leActiveRaceCarIndex );

        // ---- THE ACTIVATION / GAME-ACTION LEGS (aiwave lane A7, 2026-09-03). Bodies in
        //      BrnAIModule_Events.cpp. These are AIModule::Update rows #14 and #16 (see
        //      BrnAIModule_Drive.cpp's spine table) and the eight handlers row #14 dispatches to.
        //      HandleManagementEvents is the ONLY writer of AIDriver::mbIsActive on this build --
        //      without it every drive leg is a silent no-op. ----

        // X360 @0x82791FD0 (280 insns) -- DWARF BrnAIModule.cpp:1552 `HandleGameActions(const
        // InputBuffer*, OutputBuffer*, InputBuffer*)`. Drains lpInputBuffer->GetGameActionQueue()
        // (VariableEventQueue<13312,16>) and dispatches 16 action ids. r5 (the AI OUTPUT buffer)
        // is carried for the prototype -- the console body never touches it. r6 is the transient
        // RouteMapModuleIO "Route" INPUT buffer that action 50 appends a race-route request to;
        // the caller brackets this whole call in LockBuffersForIO/UnlockBuffersForIO on it.
        void HandleGameActions( const AIModuleIO::InputBuffer* lpInputBuffer,
                                AIModuleIO::OutputBuffer* lpOutputBuffer,
                                RouteMapModuleIO::InputBuffer* lpRouteInputBuffer );

        // X360 @0x82798620 (586 insns) -- DWARF BrnAIModule.cpp:1774 `HandleManagementEvents(
        // const InputBuffer*)`. Drains lpInputBuffer->GetRaceCarAIInterface()->mManagementQueue
        // (VariableEventQueue<16384,16> at console RaceCarAIInterface+0x2F8) and runs the eight
        // BrnAI::AIModuleIO::EEvent arms -- attach / activate / deactivate / detach / player taken
        // over / set up out of range / add car to mode / remove car from mode.
        void HandleManagementEvents( const AIModuleIO::InputBuffer* lpInputBuffer );

        // ---- the game-action handlers HandleGameActions calls (DWARF :251..:272) --------------
        void OnRaceCarReachedFinish( const BrnGameState::GameStateModuleIO::RaceCarReachedFinishAction* lpAction );      // @0x8277B8D0
        void OnRaceCarReachedCheckpoint( const BrnGameState::GameStateModuleIO::RaceCarReachedCheckpointAction* lpAction ); // @0x8278A658 (ARTIST export hole -- parked)
        void OnModeFinished( const BrnGameState::GameStateModuleIO::FinishedModeNotifyAction* lpAction );                // @0x8277B970
        void OnPlayerTakedown( const BrnGameState::GameStateModuleIO::OnPlayerTakedownAction* lpAction );                // @0x8278A720
        void OnModeStart( const BrnGameState::GameModeParams* lpGameModeParams );                                        // @0x82791DB8
        void OnModeStartRacing( bool lbSkipPlayerCar );                                                                  // @0x8276E4B0
        void OnRollingStart();                                                                                           // @0x8276E5C8
        void OnModeEnd( bool lbRestoreDrivingInput );                                                                    // @0x8277BA80
        // DWARF BrnAIModule.cpp:202. OnModeStart's one call; body is a NAMED PARK in
        // BrnAIModule_Events.cpp (it builds a temporary Array<RaceBalancingGraph,7> out of the
        // mode's OpponentData and hands it to RaceBalancingManager::OnRaceStart @0x82789AF8).
        void SetupRaceBalancingManager( const BrnGameState::GameModeParams* lpGameModeParams );                          // @0x8278A460

    AIModule();

    // The AI road network, once LoadMapData + RouteMapModule::Prepare have resolved it.
    // Non-null only from prepare stage 4 onward. (The X360's own GetAISectionsData
    // @0x8277BC00 builds a temporary ResourcePtr from mMapDataHandle and returns its
    // memory resource; this is that read, without the temporary's list churn.)
    const AISectionsData* GetLoadedAISectionsData() const;

    // Has the module finished its own prepare machine (stage 5)?
    bool IsPrepared() const { return mePrepareStage == E_PREPARESTAGE_DONE; }

    // DWARF-authoritative nested enum (BrnAIModule.h:83): the multi-frame prepare state
    // machine AIModule::Prepare steps through (advanced by the free post-increment operator++).
    enum EPrepareStage
    {
        E_PREPARESTAGE_START     = 0,
        E_PREPARESTAGE_MANAGER   = 1,
        E_PREPARESTAGE_RESOURCES = 2,
        E_PREPARESTAGE_ROUTEMAP  = 3,
        E_PREPARESTAGE_AICARS    = 4,
        E_PREPARESTAGE_DONE      = 5,
    };

    // The AI module's OWN release stage machine (X360 +294768, four states, read by
    // AIModule::Release @0x8276E270 and seeded to DONE by Construct exactly as
    // CrashModule::Construct seeds its own). Distinct from -- and shadowing nothing of --
    // the base's private EManagerReleaseStage.
    enum EReleaseStage
    {
        E_RELEASESTAGE_START    = 0,
        E_RELEASESTAGE_BASE     = 1,
        E_RELEASESTAGE_ROUTEMAP = 2,
        E_RELEASESTAGE_DONE     = 3,
    };

    // LoadMapData's own sub-state machine (X360 +294772, five states; the default arm's
    // baked text is "AIModule::LoadMapData in a weird state", BrnAIModule.cpp:491).
    enum ELoadMapDataStage
    {
        E_LOADMAPDATA_REQUEST_BUNDLE  = 0,
        E_LOADMAPDATA_AWAIT_BUNDLE    = 1,
        E_LOADMAPDATA_REQUEST_MAPDATA = 2,
        E_LOADMAPDATA_AWAIT_MAPDATA   = 3,
        E_LOADMAPDATA_DONE            = 4,
    };

    // Declared-only accessors needed by RouteMapDebugComponent::RenderHUD (its own TU). Bodies live
    // in the AIModule TU. X360 offsets (read in RouteMapDebugComponent::RenderHUD @0x827940A0):
    //   - the master route is a Route embedded at this+0x46B60 (289632); GetMasterRoute returns it.
    //   - the default A* distance-function enum is at this+0x424A8 (271528); the X360 indexes the
    //     KAPC_ASTAR_DISTANCE_FUNCTION_NAMES table with it -- GetDefaultDistanceFunctionName returns
    //     the matching name string.
    const Route* GetMasterRoute() const;                  // this + 0x46B60
    const char*  GetDefaultDistanceFunctionName() const;  // KAPC_..._NAMES[*(this + 0x424A8)]

    // Declared-only accessors needed by RaceBalancingDebugComponent (its own TU). Bodies live in the
    // AIModule TU (X360 standalone symbols BrnAI::AIModule::GetAICar @0x8276A5C8-region and
    // GetAISectionsData). GetAICar returns the per-slot AI car (FindAICar @0x827676F8 walks indices
    // 0..34); GetAISectionsData returns the loaded section data the speed read-outs query.
    AICar*          GetAICar(u32 luIndex) const;          // BrnAI::AIModule::GetAICar(this, index)
    AISectionsData* GetAISectionsData() const;            // BrnAI::AIModule::GetAISectionsData(this)

    // The current AI difficulty scalar (0..1), read by RaceBalancingDebugComponent::RenderHUD as the
    // float at this+0x4EBA4 (322404). Declared-only; body lives in the AIModule TU.
    f32             GetDifficulty() const;                // *(this + 0x4EBA4)

private:
    // X360 0x82795340 (167 insns) -- stage 2 of Prepare. LoadBundle("AI.dat") then acquire
    // "WorldMapData"; drains the reply into mMapDataHandle.
    bool LoadMapData( AIModuleIO::OutputBuffer* lpOutputBuffer );

    // ---- private drive helpers (aiwave lane A1). Bodies in BrnAIModule_Drive.cpp. ----
    // X360 @0x82798540 (55 insns) -- DWARF BrnAIModule.cpp:1324. Two RoundRobinDrivers passes.
    void DoRoundRobins();
    // X360 @0x82798408 (78 insns) -- DWARF :1168. Advances meCurrentRoundRobin[type] through the
    // active drivers, calling AIDriver::DoRoundRobinWork on up to liMaxWork of them.
    s32  RoundRobinDrivers( s32 liMaxWork, ERoundRobinType leType );
    // X360 @0x8276E660 (88 insns) -- DWARF :1054. One car per frame: its proximity rank among
    // the active cars + the module's closest-non-player car.
    void UpdateOneProximityIndex();
    // X360 @0x8276E7C0 (~70 insns) -- DWARF :1244.
    void SetSuitabilityForAggression( EActiveRaceCarIndex leActiveRaceCarIndex,
                                      const AIModuleIO::RaceCarAIInterface* lpCarInterface );

    struct RouteRequestSlot
    {
        s32 miRouteId;
        u8  mPad0[32];
    };

    EA::Thread::RWMutex mInputMutex;
    EA::Thread::RWMutex mOutputMutex;
    u8                  mPad0[252520];
    s32                 miActiveRouteRequest;
    u8                  mPad1[18088];
    s32                 miPendingRouteRequest;
    u8                  mPad2[12];
    u32                 muRouteRequestVTable;
    u8                  mPad3[56];
    RouteRequestSlot    maRouteRequestSlots[15];
    u8                  mPad4[8];
    u32                 muOpenListVTable;
    u8                  mPad5[68];
    u32                 muClosedListVTable;
    u8                  mPad6[7244];
    u32                 muScratchListVTable;
    u8                  mPad7[7832];
    s32                 miLastRouteId;
    u8                  mPad8[300];
    u32                 muAnchorState;
    u32                 muAnchorPrev;
    u32                 muAnchorNext;
    void*               mpAnchorHead;
    void*               mpAnchorTail;
    void*               mpAnchorCursor;
    u32                 muAnchorFlags;
    u8                  mPad9[452];
    u32                 muAllocatorVTable;
    u8                  mPad10[8420];
    RouteMapModule      mRouteMapModule;
    s32                 miWorldRouteRequest;

    // ================================================================================
    // ---- NAMED MEMBERS (aimodule wave 2026-08-25) ----------------------------------
    // ================================================================================
    // HOST LAYOUT IS NOT OFFSET-FAITHFUL AND NEVER WAS. The pad spine above was derived
    // from console WORD indices; on this LLP64 host EA::Thread::RWMutex, every pointer and
    // every embedded object is a different size, so no member of this class sits at its
    // X360 byte offset. The X360 offset recorded against each member below is therefore the
    // IDENTITY authority (it is what proves WHICH field a console body is touching), not a
    // placement instruction. Nothing outside this class addresses the module by offset --
    // verified by grep for `mAIModule` before the change.
    //
    // These are appended rather than carved out of the pads on purpose: carving would move
    // every existing pad-relative neighbour for no gain, since the offsets are already not
    // reproducible. The pads stay exactly as committed.

    // X360 +294764 (0x47F6C). The 6-stage prepare machine cursor.
    EPrepareStage       mePrepareStage;
    // X360 +294768 (0x47F70). The module's own release machine cursor; Construct seeds it
    // to DONE(3), Prepare's success arm resets it to START(0).
    EReleaseStage       meReleaseStage;
    // X360 +294772 (0x47F74). LoadMapData's sub-stage.
    ELoadMapDataStage   meLoadMapDataStage;

    // X360 +294832 (0x47FB0), backing buffer at +294856 with capacity 1024 / alignment 16
    // (Construct stores `*(this+294848) = 1024`, `*(this+294852) = 16`,
    // `*(this+294832) = this+294856` -- i.e. exactly EventReceiverQueue<1024,16>::Construct,
    // whose base Construct writes {buffer, capacity, alignment} then Clear()s. The three
    // console stores land at base+16 / base+20 / base+0, which is that layout exactly).
    CgsModule::EventReceiverQueue<1024, 16> mResourceReceiverQueue;

    // X360 +295880 (0x483C8). The 8-byte resource handle LoadMapData copies out of the
    // "WorldMapData" acquire reply and Prepare stage 3 hands to RouteMapModule::Prepare
    // (`ld r4, 0(this+295880)` -- ONE 64-bit load of the console's two 32-bit pointers,
    // i.e. the whole ResourceHandle passed by value in a single GPR).
    CgsResource::ResourceHandle mMapDataHandle;

    // X360 +286128 (0x45DB0). The reset-on-track manager. EMBEDDED BY VALUE, and its ONLY
    // construction site in the whole image is this module's Prepare stage 3.
    ResetOnTrackManager mResetOnTrackManager;

    // ⭐⭐⭐ X360 +560 (0x230). THE 35-ENTRY AI-CAR ARRAY (aicar_reset wave 2026-08-26).
    //
    // This is the member AIModule::Prepare stage 3 hands the reset-on-track manager as its
    // `lpaAICars`, and until this wave it handed it a NULL. It is not polish:
    // ResetOnTrackManager::Update walks all 35 entries EVERY frame (`for (i = 0; i < 191520;
    // i += 5472)` -- the console bakes the array's byte extent and the 5472 stride straight into
    // the loop) and ComputeInitialCoordinatesStandard dereferences GetAICar's result on the
    // FIRST request. A null array is an access violation the moment the pump runs.
    //
    // ⭐ THE STRIDE MATCHES BY MEASUREMENT, NOT BY HOPE: sizeof(AICar) == 5472 == 0x1560 on this
    // host, pinned by a static_assert in BrnAICar.h. 35 * 5472 == 191520, the console's own loop
    // bound. If that assert ever fires, GetAICar's console constant must become &maAICars[index].
    //
    // ⛔ IT IS NOT FULLY CONSTRUCTED -- SEE AIModule::Construct. AICar::Construct @0x82792620 is
    // an ARTIST export HOLE (no JSON; it is named only through AIModule::Construct's xrefs_from),
    // so Construct reproduces ONLY the one initialisation the reset-on-track path reads, from the
    // attested AICar::Reset @0x82792800. Everything else is the console's own pre-Construct .bss
    // zero. Flagged there, loudly.
    AICar maAICars[35];

    // X360 +322040 (0x4EB78) / +322044 (0x4EB7C) -- the two player-car cursors AIModule::Update
    // latches at the top of its body and every later leg reads back.
    //
    //   mePlayerActiveRaceCarIndex  <- lpInputBuffer->GetRaceCarAIInterface()
    //                                    ->GetPlayerActiveRaceCarIndex()  (asm 0x8279B5F8)
    //   mePlayerGlobalRaceCarIndex  <- the AI DRIVER chain: only written when the player's slot
    //                                  HAS an AIDriver whose +7529 flag is set, as
    //                                  `driver->mpAICar ? driver->mpAICar->miRaceCarIndex : -1`
    //                                  (asm 0x8279B640..0x8279B6A0).
    //
    // ⛔ [FLAG PC bring-up] NOTHING ON THIS BUILD WRITES THE SECOND ONE. AIModule::GetAIDriver
    // and the eight AIDriver objects are absent (AIDriver::Prepare is Prepare stage 4's parked
    // leg), so the console's writer arm cannot run. Its resting value here is the CONSOLE'S OWN
    // resting value: the module lives in .bss and the console never stores anything else into
    // +322044 on a free-burn drive, so E_GLOBAL_RACE_CAR_INDEX_0 (== 0) is what the console
    // reads too. It feeds only ResetOnTrackManager::Update's two range asserts and its
    // "player sent a non-STANDARD request" tripwire, both of which pass at 0.
    // ⚠️ It is NOT "the player's global race-car index" today, and no code here treats it as
    // one -- the request itself carries the real index (RCEM::SendResetOnTrackRequests writes
    // it), which is what ProcessResetOnTrackRequest actually resolves against.
    EActiveRaceCarIndex mePlayerActiveRaceCarIndex;   // X360 +322040
    EGlobalRaceCarIndex mePlayerGlobalRaceCarIndex;   // X360 +322044

    // ---- ADDITIVE (aiwave lane A4, 2026-09-03): DWARF BrnAIModule.h:361, X360 +322408
    //      (0x4EB68). The contact-spy handle AIModule::PostPhysicsUpdate @0x8276E428 latches
    //      every frame out of the post-physics input buffer (`addis r11,this,5 ; addi -0x1498 ;
    //      stw 0 ; lwz r10,4(r31) ; stw r10`). Body in Bridges/WorldBridgeAIModule.cpp until
    //      BrnAIModule.cpp takes it. DWARF order puts mCamera (:354) / miLineUpdateTokenCounter
    //      (:356) / mfProgressionRankAsRatio (:358) between the cursors above and this member;
    //      identity, not placement, as with everything in this block. ----
    // ---- ADDITIVE (aiwave lane A7, 2026-09-03). DWARF BrnAIModule.h:119, X360 +322404
    //      (0x4EBA4). AIModule::OnModeStart @0x82791E3C..0x82791E40 latches the mode's own
    //      `lfs f0, 4(lpGameModeParams)` -- GameModeParams::mfProgressionRankAsRatio -- into this
    //      seat, name for name. It is the same word the declared-only GetDifficulty() above says
    //      it returns (`*(this + 0x4EBA4)`), so that accessor's body is `return
    //      mfProgressionRankAsRatio;` when the AIModule TU lands it. ----
    f32                 mfProgressionRankAsRatio;   // X360 +322404, DWARF :119

    BrnPhysics::ContactSpy::ContactSpyInterface mContactSpyInterface;   // X360 +322408, DWARF :361

    // ================================================================================
    // ---- NAMED MEMBERS (aiwave lane A1, 2026-09-03) -- the DRIVE spine's state ----
    // ================================================================================
    // Same disposition as the block above: the X360 offset is the IDENTITY authority, not a
    // placement. Every offset here is read straight out of the drive legs' asm (the
    // `lis r11,4 ; ori r11,r11,0xEBxx ; lbzx/lwzx rN,this,r11` idiom), and the DWARF line is
    // the name authority.
    //
    // NONE OF THESE IS SEEDED BY AIModule::Construct / Prepare YET (BrnAIModule.cpp is not
    // this lane's file). The console's seeds, from Construct @0x82794D08 / Prepare @0x82798070:
    //     miLineUpdateTokenCounter = 0 (both)         muNumAggressiveCars = 3
    //     mbDoInRangeCatchup = mbDoOutOfRangeCatchup = mbDoAggressiveDriving = 1
    //     mbEnableDrivingInput = 1                    mbIsInOnlineGameMode = 0
    //     mbIsInGameMode = 0                          mbFullRollingStart = mbDonutStart = 0
    //     mbAIDrivesPlayer = 0                        mbAIPlayerInvulnerable = 1
    //     meCurrentRoundRobin[0..1] = 0 (Prepare)     meProximityGlobalRaceCarIndexRoundRobin = 0
    //     mfClosestDistance = FLT_MAX, mpClosestCar = 0 (Prepare)
    //     mRandom.Construct() (Construct, the +294784 prime, unrolled)
    //     mBuzzBy.Prepare(maAICars, &mResetOnTrackManager) (Prepare stage 4 tail)
    //     8x AIDriver::Construct (Construct) / 8x AIDriver::Prepare(sections, i, &mRandom) (Prepare stage 4)
    // The conductor lands those in BrnAIModule.cpp; until then the flags rest at the host's
    // zero-init, which is NOT the console's resting value for the five that default to 1.

    // X360 +192080 (0x2EE50), stride 7536 -- GetAIDriver @0x82765B90 bakes both. DWARF :327.
    // maAICars (35 * 5472 == 191520) ends exactly at 560 + 191520 == 192080, so this array
    // follows it in the console, as here.
    AIDriver            maAIDrivers[KI_MAX_ACTIVE_RACE_CARS];
    // X360 +252368 (0x3D9D0) -- UpdateCars hands `this + 0x3D9D0` to AICar::Update as its
    // RaceBalancingManager. DWARF :328. (8 * 7536 == 60288; 192080 + 60288 == 252368.)
    RaceBalancingManager mRaceBalancingManager;
    RouteRequestManager mRouteRequestManager;     // DWARF BrnAIModule.h:329, X360 +270952 (Construct @0x8278A3B0 from AIModule::Construct; Update row 22)
    // X360 +289632 (0x46B60) -- UpdateCars hands `this + 0x46B60` to AICar::Update as its
    // Route; RouteMapDebugComponent::RenderHUD reads the same seat. DWARF :337.
    Route               mMasterRoute;
    // X360 +294784 (0x47F80) -- UpdateDrivers hands `this + 0x47F80` to AIDriver::Update as
    // its Random. DWARF :343.
    CgsNumeric::Random  mRandom;
    // X360 +322400 (0x4EB60) -- UpdateDrivers: the driver whose slot equals this counter gets
    // `lbActive == true` this frame (asm `subf; cntlzw; extrwi ..,1,26` == (i == counter));
    // incremented mod 8 at the tail. DWARF :356.
    s32                 miLineUpdateTokenCounter;
    // ---- ADDITIVE (aiwave lane A7, 2026-09-03) -- the three per-mode AI style cursors
    //      OnModeStart @0x82791F18..0x82791F54 copies out of the GameModeParams and
    //      HandleManagementEvents' ADD_CAR_TO_MODE arm / HandleGameActions' SET_PLAYER_CAR_DRIVER
    //      arm read back. Offsets are the `lis r11,4 ; ori r11,r11,0xEB6C/0xEB70/0xEB74 ; lwzx`
    //      idiom; names are DWARF BrnAIModule.h:125/:128/:131 and the GameModeParams member each
    //      one is copied from is the same name (meAISpeedSelectionMethod /
    //      meDefaultPlayerRouteFindingStyle / meDefaultAIRouteFindingStyle). ----
    // X360 +322412 (0x4EB6C) -- AICar::OnModeStart's leSpeedSelectionMethod argument (r4).
    EAISpeedSelectionMethod meSpeedSelectionMethod;          // DWARF :125
    // X360 +322416 (0x4EB70) -- the style the PLAYER's AI car gets (AICar::mbIsPlayer arm of
    // ADD_CAR_TO_MODE, and the style SET_PLAYER_CAR_DRIVER restores when the entity module
    // takes the car back).
    ERouteFindingStyle  meDefaultPlayerRouteFindingStyle;     // DWARF :128
    // X360 +322420 (0x4EB74) -- the style every OTHER car gets; ADD_CAR_TO_MODE also gates
    // AICar::OnModeStart's lbCanDeviateFromRoute on `this == E_ROUTE_FINDING_RACE`.
    ERouteFindingStyle  meDefaultAIRouteFindingStyle;         // DWARF :131

    // X360 +322424 (0x4EB78, lbzx) -- UpdateOneProximityIndex's `miProximityIndex = this - n`.
    // DWARF :370.
    u8                  muNumAggressiveCars;
    // X360 +322425 (0x4EB79) -- UpdateDrivers passes it as AIDriver::Update's 5th arg (r7).
    bool                mbDoInRangeCatchup;         // DWARF :373
    bool                mbDoOutOfRangeCatchup;      // DWARF :374  +322426
    bool                mbDoAggressiveDriving;      // DWARF :375  +322427 (SetSuitabilityForAggression reads 0x4EB7B)
    // X360 +322428 (0x4EB7C) -- THE ProcessAIVehicleInputs GATE (and ProcessRequestInterface's
    // driver-sweep gate). OnModeStart clears it when the mode's +148 word is non-zero; OnModeEnd
    // restores 1. DWARF :376.
    bool                mbEnableDrivingInput;
    // X360 +322429 (0x4EB7D) -- UpdateDrivers / RoundRobinDrivers: "only the player's driver".
    // DWARF :377.
    bool                mbIsInOnlineGameMode;
    // X360 +322430 (0x4EB7E) -- UpdateCars: 8 cars (a mode) vs 35 (free burn). DWARF :378.
    bool                mbIsInGameMode;
    bool                mbFullRollingStart;         // DWARF :379  +322431 (OnModeStart: flags & 0x4000000)
    bool                mbDonutStart;               // DWARF :380  +322432 (OnModeStart: flags & 0x8000000)
    // +322433 (0x4EB81) is a bool the X360 build HAS and the PS3 DWARF does NOT: Construct
    // and Prepare zero it, and SetupRaceBalancingManager @0x8278A460 passes it as the third
    // argument of RaceBalancingManager::OnRaceStart. It has no name to give it, so it is not
    // declared; nothing in this tree reads it.
    // X360 +322434 (0x4EB82) -- StoreDrivenCarData: `isPlayer && !this` is the mbIsDrivenByPlayer
    // bool it hands AICar::UpdateInRangeData (asm 0x82795CB4..CD4; corrected 2026-09-05). Construct
    // stores 0 (r30) here -- the WorldDebugComponent's "AI drives player" toggle is the only setter.
    bool                mbAIDrivesPlayer;
    // X360 +322435 (0x4EB83) -- ProcessAIVehicleInputs: `isPlayer && this` -> the record's
    // mbIsInvulnerableToWorld, OR'd with AIDriver::IsInvulnerable() into
    // mbIsInvulnerableToVehicles. Construct seeds it to 1. DWARF :383.
    bool                mbAIPlayerInvulnerable;
    // X360 +322504 (0x4EBC8), one s32 per ERoundRobinType -- RoundRobinDrivers indexes it as
    // `4 * (type + 80626) + this`. DWARF :387.
    EActiveRaceCarIndex meCurrentRoundRobin[E_ROUND_ROBIN_COUNT];
    // X360 +322520 (0x4EBD8) -- UpdateOneProximityIndex's cursor (`this + 0x50000 - 0x1428`),
    // wrapped at 35. DWARF :392.
    EGlobalRaceCarIndex meProximityGlobalRaceCarIndexRoundRobin;
    // X360 +322524 (0x4EBDC) -- BuzzBy::Update's `this` and RouteRequestManager::Update's
    // last arg. DWARF :394.
    BuzzBy              mBuzzBy;
    // X360 +322828 (0x4ED0C) / +322832 (0x4ED10) -- UpdateOneProximityIndex rewrites both every
    // frame (FLT_MAX / NULL, then the nearest active non-player car); BuzzBy::Update reads
    // mpClosestCar. DWARF :396 / :397.
    f32                 mfClosestDistance;
    AICar*              mpClosestCar;
};

// Free post-increment over the AI prepare-stage enum (DWARF BrnAIModule.h:417). X360 0x82765A10.
AIModule::EPrepareStage operator++(AIModule::EPrepareStage& leEnumIndex, int);
}
