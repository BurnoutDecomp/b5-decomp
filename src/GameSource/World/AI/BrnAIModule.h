#pragma once

#include "types.hpp"
#include "GameSource/World/AI/BrnAIDebugComponent.h"
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "SharedClasses/BrnSharedConstants.h"   // BrnUpdateSet
#include "GameSource/World/AI/Route/BrnRouteMapModule.h"
#include "GameSource/World/AI/BrnAICar.h"                              // AICar (maAICars[35], BY VALUE)
#include "GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.h"   // ResetOnTrackManager
#include "GameSource/Director/Camera/Camera.h"                     // BrnDirector::Camera::Camera (mCamera, DWARF :354)
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

// ⚠️ CLASS KEY: BrnGameDataAllocatorList.h defines AllocatorList as a `class`, so it is
// forward-declared as one here. MSVC mangles the class key into the parameter type (V vs U): a
// `struct` here makes BrnAIModule.cpp emit ?Prepare@...PEAUAllocatorList... while every caller
// (compiled against the real header) asks for ...PEAVAllocatorList... -- an LNK2019. A silent
// trap for any TU that only ever saw this line.
namespace BrnResource { namespace GameDataIO { template <int N> class AllocatorListT; class AllocatorList; } }

namespace CgsModule { struct IOBufferStack; }
namespace BrnWorld { class WorldDebugComponent; }

// ---- ADDITIVE (aiwave lane A7, 2026-09-03) -- pointer-only parameter types of the eight
//      game-action handlers below. Declared, not included: BrnGameActions.h /
//      BrnGameModeParams.h are heavy GameState headers and this class only takes pointers.
//      BrnAIModule_Events.cpp includes both for the complete types it dereferences
//      (OnRaceCarReachedCheckpoint @0x8278A658 reads RaceCarReachedCheckpointAction's +0x04 /
//      +0x08 / +0x0C -- an ARTIST export hole, decoded whole for crash parity CC-10). ----
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
namespace RouteMapModuleIO { struct OutputBuffer; }       // UpdateCarRoutes arg: the transient "Route" OUTPUT buffer

// DWARF BrnAIModule.h:58/:59 -- the two roster caps the module's arrays and loops are sized by.
// The console bakes them as literals (35 / 8) into every GetAICar/GetAIDriver and loop bound.
const s32 KI_MAX_OUT_OF_RANGE_RACE_CARS = 35;
const s32 KI_MAX_ACTIVE_RACE_CARS       = 8;

struct Route;
struct AICar;
struct AISectionsData;
class RouteMapDebugComponent;

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
    friend class ::BrnWorld::WorldDebugComponent;
    friend class AIDebugComponent;   // the AI debug overlay reads the car array / buzz-by directly
    friend class RouteMapDebugComponent; // its original minimap readers walk the named AI-car and route state

public:
    // DWARF SetAIDrivesPlayer; inlined store in ARTIST WorldModule::Update @0x827D753C.
    void SetAIDrivesPlayer(bool lbEnabled) { mbAIDrivesPlayer = lbEnabled; }
    // DWARF BrnAIModule.h:423 `void SetCamera(Camera&)`; inlined in ARTIST WorldModule::Update as
    // Camera::operator=(this + 0x5DFD00 == mAIModule + 0x4EA00, this + 0x5E1CC0 == mLastCameraInput)
    // at 0x827D750C..0x827D7540 -- right after the SetAIDrivesPlayer store, right before the
    // AIModule::Update vtable call (0x827D7588). (crash parity G05-D3, 2026-09-22)
    void SetCamera(BrnDirector::Camera::Camera& lrCamera) { mCamera = lrCamera; }

        // X360 0x82794D08. Reached by the wired WorldModule::Construct @0x827CF540 fleet cascade.
        void Construct() ;
        // ---- ADDITIVE (attested by WorldModule::DestructWorld @0x827BD0F0) ----
        // X360 0x8276E380. Bodied in BrnAIModule.cpp (crash parity G04-D7); teardown-only.
        void Destruct() ;
        // ---- ADDITIVE (attested by WorldModule::ReleaseWorld @0x827BCE58) ----
        // Bodied in BrnAIModule_wS34_00.cpp.
        bool Release() ;

        // X360 0x82798070 -- the 6-stage prepare machine. NOT the base's virtual Prepare()
        // (different signature): the console calls this one directly from WorldModule::Prepare
        // stage 11 and it calls the base's inside its own stage 1.
        bool Prepare( BrnResource::GameDataIO::AllocatorList* lpAllocatorList,
                      AIModuleIO::OutputBuffer* lpOutputBuffer );

        // ---- ADDITIVE (called from WorldModule::Update). Update is bodied in
        //      BrnAIModule_ResetPump.cpp, PostPhysicsUpdate in WorldBridgeAIModule.cpp. ----
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

        // @0x8279A1E0. The SIM-PAUSED arm of Update: the console branches to it at
        // 0x8279B4B0 (`if (lUpdateSet & 1)`) INSTEAD of the running body, and it still
        // drains the per-frame queues -- HandleGameActions at 0x8279A348,
        // HandleManagementEvents at 0x8279A37C, the route round trip and
        // ProcessRequestInterface (0x8279A4C8, r6 = the lUpdateSet Update forwards in r8).
        void PausedUpdate( CgsModule::IOBufferStack* lpInputBufferStack,
                           CgsModule::IOBufferStack* lpOutputBufferStack,
                           const AIModuleIO::InputBuffer* lpInputBuffer,
                           AIModuleIO::OutputBuffer* lpOutputBuffer,
                           BrnUpdateSet lUpdateSet );

        // ---- the game-action handlers HandleGameActions calls (DWARF :251..:272) --------------
        void OnRaceCarReachedFinish( const BrnGameState::GameStateModuleIO::RaceCarReachedFinishAction* lpAction );      // @0x8277B8D0
        void OnRaceCarReachedCheckpoint( const BrnGameState::GameStateModuleIO::RaceCarReachedCheckpointAction* lpRaceCarReachedCheckpointAction ); // @0x8278A658 (ARTIST export hole, decoded with ppcdis -- CC-10)
        void OnModeFinished( const BrnGameState::GameStateModuleIO::FinishedModeNotifyAction* lpAction );                // @0x8277B970
        void OnPlayerTakedown( const BrnGameState::GameStateModuleIO::OnPlayerTakedownAction* lpAction );                // @0x8278A720
        void OnModeStart( const BrnGameState::GameModeParams* lpGameModeParams );                                        // @0x82791DB8
        void OnModeStartRacing( bool lbSkipPlayerCar );                                                                  // @0x8276E4B0
        void OnRollingStart();                                                                                           // @0x8276E5C8
        void OnModeEnd( bool lbRestoreDrivingInput );                                                                    // @0x8277BA80
        // declaration reference BrnAIModule.cpp. OnModeStart's one call: it builds a temporary
        // Array<RaceBalancingGraph,7> out of the mode's OpponentData and hands it to
        // RaceBalancingManager::OnRaceStart. Bodied in BrnAIModule_Events.cpp.
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

    // RouteMap RenderHUD reads the embedded master route (ARTIST +0x46B60).
    // Its heuristic label directly indexes the original table with the existing
    // RouteRequestManager enum (+0x424A8); no separate AIModule name getter exists.
    const Route* GetMasterRoute() const;                  // this + 0x46B60

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
    // X360 @0x827955F0 (127 insns) -- DWARF BrnAIModule.cpp:1495 `UpdateCarRoutes(OutputBuffer*,
    // const OutputBuffer*)`: r4 = the AI output buffer (event 117), r5 = the transient "Route"
    // output buffer (read-locked by Update at 0x8279B7D4). Hands each AI-owned RouteResponse to its
    // car, feeds the race balancer, and posts E_EVENT_PLAYER_ROUTE_UPDATED. Body in
    // BrnAIModule_Routes.cpp. (A member again since 2026-09-22 -- it reads the two player cursors
    // and mRaceBalancingManager, which the aiwave free-function form could not reach.)
    void UpdateCarRoutes( AIModuleIO::OutputBuffer* lpOutputBuffer,
                          const RouteMapModuleIO::OutputBuffer* lpRouteOutputBuffer );

    // ---- [PC HARNESS, NOT X360] BRN_AI_PAD_PLAYER=pursuit (BrnAIHarnessPad.h). Bodies at the end of
    //      BrnAIModule_Drive.cpp; UpdateDrivers calls HarnessAIPadPursuit once per frame, and it
    //      returns at once unless WorldModule armed the pad with the pursuit objective. No console
    //      function has either name. ----
    void HarnessAIPadPursuit( AICar* lpPlayerCar );
    void HarnessAIPadReleaseRoute( AICar* lpPlayerCar, const char* lpcReason );

    static void AssertNativeLayout();

    // ARTIST 0x827E3780 and 0x82794D08; member order corroborated by DecFIGS.
    AICar maAICars[35];
    AIDriver maAIDrivers[KI_MAX_ACTIVE_RACE_CARS];
    RaceBalancingManager mRaceBalancingManager;
    RouteRequestManager mRouteRequestManager;
    AIDebugComponent mAIDebugComponent;
    ResetOnTrackManager mResetOnTrackManager;
    Route mMasterRoute;
    EPrepareStage mePrepareStage;
    EReleaseStage meReleaseStage;
    ELoadMapDataStage meLoadMapDataStage;
    CgsNumeric::Random mRandom;
    CgsModule::EventReceiverQueue<1024, 16> mResourceReceiverQueue;
    CgsResource::ResourceHandle mMapDataHandle;
    CgsResource::ResourceHandle mLuaDataHandle;
    RouteMapModule mRouteMapModule;
    EActiveRaceCarIndex mePlayerActiveRaceCarIndex;
    EGlobalRaceCarIndex mePlayerGlobalRaceCarIndex;
    BrnDirector::Camera::Camera mCamera;
    s32 miLineUpdateTokenCounter;
    f32 mfProgressionRankAsRatio;
    BrnPhysics::ContactSpy::ContactSpyInterface mContactSpyInterface;
    EAISpeedSelectionMethod meSpeedSelectionMethod;
    ERouteFindingStyle meDefaultPlayerRouteFindingStyle;
    ERouteFindingStyle meDefaultAIRouteFindingStyle;
    u8 muNumAggressiveCars;
    bool mbDoInRangeCatchup;
    bool mbDoOutOfRangeCatchup;
    bool mbDoAggressiveDriving;
    bool mbEnableDrivingInput;
    bool mbIsInOnlineGameMode;
    bool mbIsInGameMode;
    bool mbFullRollingStart;
    bool mbDonutStart;
    bool mbHighTakenDownPenalty;
    bool mbAIDrivesPlayer;
    bool mbAIPlayerInvulnerable;
    Array<f32, 16> maWayPointDistance;
    EActiveRaceCarIndex meCurrentRoundRobin[E_ROUND_ROBIN_COUNT];
    EActiveRaceCarIndex meAggressorIndex;
    EActiveRaceCarIndex meVictimIndex;
    EGlobalRaceCarIndex meProximityGlobalRaceCarIndexRoundRobin;
    BuzzBy mBuzzBy;
    f32 mfClosestDistance;
    AICar* mpClosestCar;

};

// Free post-increment over the AI prepare-stage enum (DWARF BrnAIModule.h:417). X360 0x82765A10.
AIModule::EPrepareStage operator++(AIModule::EPrepareStage& leEnumIndex, int);
AIModule::EReleaseStage operator++(AIModule::EReleaseStage& leEnumIndex, int);   // the release-stage twin
}
