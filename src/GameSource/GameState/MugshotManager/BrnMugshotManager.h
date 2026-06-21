#pragma once

// ============================================================================
// b5-decomp/src/GameSource/GameState/MugshotManager/BrnMugshotManager.h
// ============================================================================
// Canonical home for BrnGameState::MugshotManager -- the photo / "mugshot" capture
// manager embedded by value in the GameStateModule (BrnGameStateModule::Construct
// calls MugshotManager::Construct). It runs two parallel little state machines:
//   * the CAPTURE machine (meMugshotCaptureState) drives capturing the LOCAL player's
//     own mugshot (after takedowns / road-rule beats / online wins);
//   * the SHOW machine   (meMugshotShowState)   drives capturing + showing a REMOTE
//     player's mugshot that arrived over the network.
// Both machines push GameActions onto the OutputBuffer's GameActionQueue
// (CgsModule::VariableEventQueue<13312,16>) to ask the image/camera layer to act.
//
// SHAPE is DWARF-authoritative
// (references/DecFIGS/dwarfdump/.../MugshotManager/BrnMugshotManager.h), gated against
// the X360 binary's Construct (0x82363A88) / OnRoundStart (0x82357A70) store offsets,
// which pin every member to its exact byte slot:
//   +0x00  CameraStatusData maCameraStatusData[8]   (8 * 8 == 64 bytes; each entry is
//                                                     {EActiveRaceCarIndex@+0, ECameraStatus@+4},
//                                                     Construct fills index=-1 / status=4)
//   +0x40  CgsID            mRoadRuleMugshotBeatenRoadID   (u64; Construct -> 0x800000000)
//   +0x48  EActiveRaceCarIndex meMugshotCaptureRaceCarIndex (-1)
//   +0x4C  EActiveRaceCarIndex meMugshotShowRaceCarIndex    (-1)
//   +0x50  f32              mfMugshotCaptureTimer          (-1.0f)
//   +0x54  f32              mfMugshotShowTimer             (-1.0f)
//   +0x58  EMugshotCaptureState meMugshotCaptureState      (0 == IDLE)
//   +0x5C  EMugshotShowState    meMugshotShowState         (0 == IDLE)
//   +0x60  GameStateModuleIO::EImageType meCaptureMugshotType (6 == COUNT)
//   +0x64  GameStateModuleIO::EImageType meShowMugshotType    (6 == COUNT)
//   +0x68  GameStateModule* mpGameStateModule              (Construct arg)
//   +0x6C  bool             mbIsAnythingPaused             (0)
// No vptr (Construct's first store is the maCameraStatusData[0] word at this+0).
//
// The X360 stores `0x800000000`/`0x600000000` into mRoadRuleMugshotBeatenRoadID as the
// cleared / round-reset value: it is a CgsID (u64) whose HIDWORD (the image-type word the
// StartMugshotCapture payload reads back via `HIDWORD(v7)`) is seeded to 8 (Construct) and 6
// (round reset == E_IMAGE_TYPE_COUNT). The LODWORD is the road id; cleared to 0.

#include "types.hpp"
#include "GameSource/BurnoutConstants.h"                                          // EActiveRaceCarIndex (global scope, used by callers)
#include "GameShared/GameClasses/Core/CgsAssert.h"                                // CGS_ASSERT
#include "GameSource/GameState/BrnGameStateSharedIO.h"                            // GameStateModuleIO::EImageType / EGameModeType / IsOnlineFreeBurnLobby
#include "GameSource/GameState/BrnGameStateModuleIO.h"                            // PreWorldInputBuffer / OutputBuffer / GameActionQueue / TimerStatusInterface
#include "GameSource/GameState/BrnGameStateModule.h"                              // GameStateModule (GetPlayerActiveRaceCarIndex / IsOnlineGameMode / GetModeManager)
#include "GameSource/GameState/ModeManager/BrnModeManager.h"                      // ModeManager::IsInPostEvent
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h" // BrnNetwork::ECameraStatus, InGamePlayerStatusInterface
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleIOQueues.h" // (TakedownEventQueue element note)
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                          // CgsModule::EventQueue<TakedownEvent,8> (Update's takedown-queue arg)
#include "GameSource/GameState/TakedownManager/BrnTakedownManagerTypes.h"         // BrnGameState::TakedownEvent, CgsID, BrnGameState::EActiveRaceCarIndex

namespace BrnGameState
{
// Forward decls for the network-image event payloads. These two events are produced by the
// network image manager (BrnNetworkImageManager) and consumed read-only here; their full
// layouts live with that subsystem's own TU. Only the leading words this TU reads are needed,
// so a minimal complete shape is modelled inline below rather than forward-declared, because
// ProcessImageReceivedEvent / ProcessAbortCaptureEvent dereference concrete fields.

// OnlineImageReceivedEvent (read by ProcessImageReceivedEvent @ 0x82363C90). The X360 reads:
//   *(a2+0)   -> a CgsID-low / capture-car word stored into mRoadRuleMugshotBeatenRoadID's low
//   *(a2+4)   -> the EImageType (validated <= 4) stored into meShowMugshotType
//   *(a2+8)   -> a u64 (CgsID image id) stored into mRoadRuleMugshotBeatenRoadID
// Modelled as the leading words the consumer touches; widen with the real BrnNetworkImageManager
// type when that TU lands (offsets must not move).
struct OnlineImageReceivedEvent
{
    s32                           miCaptureCarWord;   // +0x00 (stored to mRoadRuleMugshotBeatenRoadID low via meMugshotShowRaceCarIndex path)
    GameStateModuleIO::EImageType meImageType;        // +0x04 (validated in [0,4], -> meShowMugshotType)
    CgsID                         mImageId;            // +0x08 (-> mRoadRuleMugshotBeatenRoadID)
};

// OnlineImageCaptureAbortedEvent (read by ProcessAbortCaptureEvent @ 0x82363DE0). The X360 reads
// only the leading byte (*a2): true selects the CAPTURE machine reset, false the SHOW machine reset.
struct OnlineImageCaptureAbortedEvent
{
    bool mbAbortCaptureMachine;   // +0x00 (true -> reset capture FSM; false -> reset show FSM)
};

// ============================================================================
// MugshotManager (DWARF BrnMugshotManager.h:46)
// ============================================================================
struct MugshotManager
{
public:
    // Capture (local-player mugshot) FSM states. DWARF BrnMugshotManager.h:117.
    enum EMugshotCaptureState
    {
        E_MUGSHOT_CAPTURE_STATE_IDLE                = 0,
        E_MUGSHOT_CAPTURE_STATE_PREPARE_FOR_CAPTURE = 1,
        E_MUGSHOT_CAPTURE_STATE_CAPTURE_YOUR_MUGSHOT = 2,
        E_MUGSHOT_CAPTURE_STATE_TAKE_MUGSHOT        = 3,
    };

    // Show (remote-player mugshot) FSM states. DWARF BrnMugshotManager.h:134.
    enum EMugshotShowState
    {
        E_MUGSHOT_SHOW_STATE_IDLE                          = 0,
        E_MUGSHOT_SHOW_STATE_PREPARE_TO_CAPTURE_THEIR_MUGSHOT = 1,
        E_MUGSHOT_SHOW_STATE_CAPTURE_THEIR_MUGSHOT         = 2,
        E_MUGSHOT_SHOW_STATE_SHOW_THEIR_MUGSHOT            = 3,
    };

    // Per-active-race-car cached camera status (DWARF BrnMugshotManager.h:152). 8-byte entry:
    // the player's active-race-car slot and their network camera status. UpdateCameraStatusData
    // refreshes maCameraStatusData[] each frame from the network player-status interface.
    struct CameraStatusData
    {
        // Global-scope EActiveRaceCarIndex (BurnoutConstants.h) -- matches the value the network
        // player-status record (InGamePlayerStatusData::meActiveRaceCarIndex) and the GameStateModule
        // accessor (GetPlayerActiveRaceCarIndex) carry, both of which are the global enum.
        ::EActiveRaceCarIndex     meActiveRaceCarIndex;   // +0x00
        BrnNetwork::ECameraStatus meCameraStatus;         // +0x04

        // Reset to {INVALID, E_CAMERA_STATUS_COUNT}. The X360 cleared entry is {-1, 4}; 4 is the
        // "no camera / count" sentinel (BrnNetwork::KI_CAMERA_STATUS_DEFAULT). DoesPlayerHaveACamera
        // treats status 4 as the asserted-invalid sentinel and status 0 as "no camera".
        void Clear();
    };

public:
    // ---- lifecycle ----
    // X360 0x82363A88. Seeds every member to its cleared value and caches the owning module.
    void Construct(GameStateModule* lpGameStateModule);
    // Prepare/Release/Destruct are X360-inlined no-ops for this manager (the DWARF declares them,
    // but the X360 build emits no out-of-line body and GameStateModule's Prepare/Release/Destruct
    // do not call into them). Declared-only; bodies are trivial and land if ever de-inlined.
    bool Prepare();
    bool Release();
    void Destruct();

    // ---- per-frame + round hooks ----
    // X360 0x82391690 (BrnGameStateModule::PreWorldUpdate). Advances both FSMs.
    void Update(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                GameStateModuleIO::OutputBuffer* lpOutput,
                const GameStateModuleIO::VehicleOutputInterface* lpVehicleOutput,
                const CgsModule::EventQueue<TakedownEvent, 8>* lpTakedownEventQueue,
                GameStateModuleIO::EGameModeType leGameModeType,
                bool lbIsAnythingPaused);
    // X360 0x82357A70 / 0x82357AF8. Reset both FSMs at round boundaries.
    void OnRoundStart();
    void OnRoundEnd(bool lbResetState);

    // ---- event sinks (driven by GameStateModule::ProcessGameEvents et al.) ----
    // X360 0x82363C90. A remote player's mugshot image arrived over the network.
    void ProcessImageReceivedEvent(const OnlineImageReceivedEvent* lpImageReceivedEvent);
    // X360 0x82383A40. A road rule was beaten -> consider capturing a road-rule mugshot.
    void ProcessBeatenRoadRuleEvent(GameStateModuleIO::OutputBuffer* lpOutput,
                                    ::EActiveRaceCarIndex lBeatenPlayerRaceCarIndex,
                                    CgsID lBeatenRoadID,
                                    s32 leScoreType);
    // X360 (DWARF BrnMugshotManager.cpp:954). An online event was won -> victory mugshot.
    void ProcessOnlineWin(GameStateModuleIO::OutputBuffer* lpOutput);
    // X360 0x82363DE0. The image layer aborted an in-flight capture.
    void ProcessAbortCaptureEvent(const OnlineImageCaptureAbortedEvent* lpAbortEvent);

private:
    // ---- internals ----
    void ResetState();
    // X360 0x82382E18. Core "begin a capture/show of mugshot type X" worker; arbitrates which of
    // the two FSMs the local player drives and pushes the prepare/abort/start GameActions.
    void StartMugshotCapture(GameStateModuleIO::OutputBuffer* lpOutput,
                             GameStateModuleIO::EImageType leMugshotTypeToCapture,
                             ::EActiveRaceCarIndex leMugshotShowRaceCarIndex,
                             ::EActiveRaceCarIndex leMugshotCaptureRaceCarIndex,
                             bool lbMugshotRequiresBroadcast);
    // X360 0x82363AF0. Refresh maCameraStatusData[] from the network player-status interface.
    void UpdateCameraStatusData(const GameStateModuleIO::PreWorldInputBuffer* lpInput);
    // X360 0x823830D0. Walk the frame's takedown events; start a payback mugshot for ones we star in.
    void ProcessTakedownEvents(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                               GameStateModuleIO::OutputBuffer* lpOutput,
                               const CgsModule::EventQueue<TakedownEvent, 8>* lpTakedownEventQueue,
                               GameStateModuleIO::EGameModeType leGameModeType);
    // X360 0x823832D0. Scan the dirty-trick queue for a successful payback to mugshot.
    void CheckForSuccessfulPayback(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                   GameStateModuleIO::OutputBuffer* lpOutput);
    // X360-inlined FSM transition helpers (DWARF BrnMugshotManager.h:213/218). The X360 build
    // inlines them as plain field stores; declared-only here (de-inline to bodies if needed).
    void ChangeState(EMugshotCaptureState leNewCaptureState);
    void ChangeState(EMugshotShowState leNewShowState);
    void UpdateFSMTimers(const GameStateModuleIO::TimerStatusInterface* lpTimerStatusInterface);
    // X360 0x823579D8. True iff the player at the given active-race-car slot owns a working camera.
    bool DoesPlayerHaveACamera(::EActiveRaceCarIndex lePlayerRaceCarIndex);

    // ---- capture-FSM step handlers ----
    void HandlePreparingForMugshotCapture();                                       // X360 0x82363C38
    void HandleCapturingMugshot(GameStateModuleIO::OutputBuffer* lpOutput);         // X360 0x82383438
    void HandleTakingMugshot(GameStateModuleIO::OutputBuffer* lpOutput,
                             const GameStateModuleIO::VehicleOutputInterface* lpVehicleOutput,
                             GameStateModuleIO::EGameModeType leGameModeType);       // X360-inlined (case 3 helper)
    // ---- show-FSM step handlers ----
    void HandlePreparingToCaptureTheirMugshot();                                   // X360-inlined
    void HandleCapturingTheirMugshot(GameStateModuleIO::OutputBuffer* lpOutput);   // X360 0x82383768
    void HandleShowingTheirMugshot(GameStateModuleIO::OutputBuffer* lpOutput);     // X360 0x82383870

private:
    // ---- data members at exact X360 byte offsets ----
    CameraStatusData              maCameraStatusData[8];          // +0x00 (8 * 8 == 0x40)
    CgsID                         mRoadRuleMugshotBeatenRoadID;   // +0x40 (u64)
    ::EActiveRaceCarIndex         meMugshotCaptureRaceCarIndex;   // +0x48
    ::EActiveRaceCarIndex         meMugshotShowRaceCarIndex;      // +0x4C
    f32                           mfMugshotCaptureTimer;          // +0x50
    f32                           mfMugshotShowTimer;             // +0x54
    EMugshotCaptureState          meMugshotCaptureState;          // +0x58
    EMugshotShowState             meMugshotShowState;             // +0x5C
    GameStateModuleIO::EImageType meCaptureMugshotType;           // +0x60
    GameStateModuleIO::EImageType meShowMugshotType;              // +0x64
    GameStateModule*              mpGameStateModule;              // +0x68
    bool                          mbIsAnythingPaused;             // +0x6C

    // Compile-time guards: the X360 Construct stores pin every offset.
    static void _AssertLayout()
    {
        static_assert(sizeof(CameraStatusData) == 0x08, "CameraStatusData must be 8 bytes");
        static_assert(offsetof(MugshotManager, maCameraStatusData)           == 0x00, "maCameraStatusData @ +0x00");
        static_assert(offsetof(MugshotManager, mRoadRuleMugshotBeatenRoadID) == 0x40, "mRoadRuleMugshotBeatenRoadID @ +0x40");
        static_assert(offsetof(MugshotManager, meMugshotCaptureRaceCarIndex) == 0x48, "meMugshotCaptureRaceCarIndex @ +0x48");
        static_assert(offsetof(MugshotManager, meMugshotShowRaceCarIndex)    == 0x4C, "meMugshotShowRaceCarIndex @ +0x4C");
        static_assert(offsetof(MugshotManager, mfMugshotCaptureTimer)        == 0x50, "mfMugshotCaptureTimer @ +0x50");
        static_assert(offsetof(MugshotManager, mfMugshotShowTimer)           == 0x54, "mfMugshotShowTimer @ +0x54");
        static_assert(offsetof(MugshotManager, meMugshotCaptureState)        == 0x58, "meMugshotCaptureState @ +0x58");
        static_assert(offsetof(MugshotManager, meMugshotShowState)           == 0x5C, "meMugshotShowState @ +0x5C");
        static_assert(offsetof(MugshotManager, meCaptureMugshotType)         == 0x60, "meCaptureMugshotType @ +0x60");
        static_assert(offsetof(MugshotManager, meShowMugshotType)            == 0x64, "meShowMugshotType @ +0x64");
        static_assert(offsetof(MugshotManager, mpGameStateModule)            == 0x68, "mpGameStateModule @ +0x68");
        // mbIsAnythingPaused sits one pointer-width past mpGameStateModule. On the 4-byte-pointer
        // X360 ABI that is +0x6C (the authoritative offset); the host gate is a 64-bit (8-byte
        // pointer) build, so guard the exact check on the X360 pointer width.
        static_assert(sizeof(void*) != 4
                      || offsetof(MugshotManager, mbIsAnythingPaused) == 0x6C, "mbIsAnythingPaused @ +0x6C (4B-ptr ABI)");
    }
};
} // namespace BrnGameState
