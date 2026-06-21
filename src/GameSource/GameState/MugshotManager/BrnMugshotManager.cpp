// ============================================================================
// b5-decomp/src/GameSource/GameState/MugshotManager/BrnMugshotManager.cpp
// ============================================================================
// Bodies for BrnGameState::MugshotManager (home: BrnMugshotManager.h). Reconstructed
// store-for-store from the X360 ARTIST.XEX (asm authoritative; DWARF for names/shapes).
//
// The manager runs two parallel FSMs that ask the image/camera layer (via GameActions
// pushed onto the OutputBuffer's GameActionQueue) to capture and show mugshots:
//   * CAPTURE machine (meMugshotCaptureState): the local player's own mugshot.
//   * SHOW machine    (meMugshotShowState):    a remote player's mugshot.
//
// GAME-ACTION PAYLOADS (pushed via CgsModule::VariableEventQueue<13312,16>::AddEvent):
//   * type 213 (0xD5), 32 bytes -- the "start mugshot" action (DWARF PaybackMugshotAction):
//        +0x00 CgsID mImageId; +0x08 show-car; +0x0C player-car; +0x10 state/index word;
//        +0x14 EImageType; +0x18 bool; +0x19 bool broadcast.
//   * type 214 (0xD6), 16 bytes -- the "abort mugshot capture" action (DWARF AbortMugshotCaptureAction):
//        +0x00 aborted-show-car; +0x04 player-car; +0x08 EImageType; +0x0C bool.
// PaybackMugshotAction / AbortMugshotCaptureAction have no committed home yet, so each is
// modelled here as the exact byte image the X360 builds on the stack and memcpy's into the
// queue. They are file-local payloads (the queue stores them by byte image); when those
// GameAction types land in BrnGameActions.h, swap these for the real structs (the layouts match).
//
// ASSERT-PARITY NOTE: the X360 bakes the verbatim source path "d:\\p4\\b5_main\\...
// BrnMugshotManager.cpp" and the exact line numbers into every FireAssert. The house CGS_ASSERT
// macro emits __FILE__/__LINE__ instead; this is the project-wide benign assert-machinery YELLOW.

#include "GameSource/GameState/MugshotManager/BrnMugshotManager.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // CgsModule::VariableEventQueue<13312,16>::AddEvent

namespace BrnGameState
{
namespace
{
    // --- KAI_MUGSHOT_PRIORITIES (DWARF BrnMugshotManager.h:163, `extern int32_t[]`) ---------
    // Indexed by GameStateModuleIO::EImageType (0..6, the default meCaptureMugshotType == COUNT(6)
    // is a valid index). StartMugshotCapture only ever uses these for the RELATIVE comparison
    // `priority[new] < priority[current]` to decide whether a newly-requested capture out-ranks the
    // one already pending: a LOWER value means HIGHER precedence. The slot for the cleared/default
    // type (COUNT) holds the lowest precedence so any real request displaces the idle default.
    // FLAG(values-unverified): the table's raw words are not separately exported from the binary;
    // the ORDERING below is reconstructed from the override behaviour (victory > road-rule >
    // payback > mugshot > freeburn, count last). Adjust the literal values if the data export
    // attests otherwise -- only their relative order is load-bearing.
    const s32 KAI_MUGSHOT_PRIORITIES[GameStateModuleIO::E_IMAGE_TYPE_COUNT + 1] =
    {
        5,  // E_IMAGE_TYPE_FREEBURN_MUGSHOT        (0)
        4,  // E_IMAGE_TYPE_MUGSHOT                 (1)
        3,  // E_IMAGE_TYPE_PAYBACK_MUGSHOT         (2)
        2,  // E_IMAGE_TYPE_ROAD_RULE_TIME_MUGSHOT  (3)
        2,  // E_IMAGE_TYPE_ROAD_RULE_CRASH_MUGSHOT (4)
        1,  // E_IMAGE_TYPE_VICTORY_MUGSHOT         (5)
        6,  // E_IMAGE_TYPE_COUNT                   (6) -- lowest precedence (cleared/default slot)
    };

    // --- FSM timing constants (DWARF BrnMugshotManager.cpp:44-50) -----------------------------
    // X360 float immediates (flt_8202AC1C etc.); the exact constants the FSM compares timers against.
    const f32 KF_WAIT_MUGSHOT_DURATION                = 0.2f;       // flt_8202AC1C
    const f32 KF_WAIT_PAYBACK_MUGSHOT_DURATION        = 3.0f;       // flt_8202AC20
    const f32 KF_CAPTURE_YOUR_MUGSHOT_DURATION        = 2.3f;       // flt_8202AC24
    const f32 KF_CAPTURE_YOUR_VICTORY_MUGSHOT_DURATION = 4.8000002f; // flt_8202AC28
    const f32 KF_SHOW_YOUR_MUGSHOT_DURATION           = 4.0f;       // flt_8202AEAC
    const f32 KF_SHOW_THEIR_MUGSHOT_DURATION          = 4.0f;       // (show machine timeout)
    const f32 KF_CAPTURE_THEIR_MUGSHOT_TIMEOUT_DURATION = 10.0f;    // flt_8202AC38

    // The two GameAction event types + their 32/16-byte payload sizes (X360 AddEvent immediates).
    const s32 KI_GAME_ACTION_START_MUGSHOT = 213;  // 0xD5
    const s32 KI_GAME_ACTION_ABORT_MUGSHOT = 214;  // 0xD6

    // The "begin capture/show a mugshot" GameAction payload (type 213, 32 bytes). Byte image matches
    // the stack record the X360 builds before AddEvent. The state/index word at +0x10 carries either
    // 0 (StartMugshotCapture queue), or the FSM-stage tag (1/3/5) the step handlers stamp.
    struct StartMugshotGameAction
    {
        CgsID                         mImageId;          // +0x00 (mRoadRuleMugshotBeatenRoadID)
        ::EActiveRaceCarIndex         meShowRaceCar;     // +0x08
        ::EActiveRaceCarIndex         mePlayerRaceCar;   // +0x0C
        s32                           miStageTag;        // +0x10
        GameStateModuleIO::EImageType meImageType;       // +0x14
        bool                          mbFlagA;           // +0x18
        bool                          mbBroadcast;       // +0x19
        u8                            maPad[0x20 - 0x1A]; // +0x1A..0x20
    };

    // The "abort an in-flight mugshot capture" GameAction payload (type 214, 16 bytes).
    struct AbortMugshotGameAction
    {
        ::EActiveRaceCarIndex         meAbortedShowRaceCar; // +0x00 (the meMugshotShowRaceCarIndex being abandoned)
        ::EActiveRaceCarIndex         mePlayerRaceCar;      // +0x04
        GameStateModuleIO::EImageType meImageType;          // +0x08
        bool                          mbZero;               // +0x0C
        u8                            maPad[0x10 - 0x0D];    // +0x0D..0x10
    };

    // The GameActionQueue concrete type (forward-declared in BrnGameStateModuleIO.h) is the variable
    // event queue the GameStateModuleIO TU returns; the X360 calls VariableEventQueue<13312,16>::AddEvent
    // straight on it. Bridge the opaque return to the queue type the X360 invokes the method on.
    inline CgsModule::VariableEventQueue<13312, 16>* AsVeq(GameStateModuleIO::GameActionQueue* lpQueue)
    {
        return reinterpret_cast<CgsModule::VariableEventQueue<13312, 16>*>(lpQueue);
    }
}

// ---------------------------------------------------------------------------
// CameraStatusData::Clear -- X360 (inlined). Reset to {INVALID, COUNT(4)}.
// ---------------------------------------------------------------------------
void MugshotManager::CameraStatusData::Clear()
{
    meActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;
    meCameraStatus       = BrnNetwork::E_CAMERA_STATUS_COUNT;
}

// ---------------------------------------------------------------------------
// Construct -- X360 0x82363A88. Seed every member to its cleared value.
// ---------------------------------------------------------------------------
void MugshotManager::Construct(GameStateModule* lpGameStateModule)
{
    mpGameStateModule           = lpGameStateModule;          // this+0x68
    meMugshotCaptureState       = E_MUGSHOT_CAPTURE_STATE_IDLE; // this+0x58 (0)
    meMugshotShowState          = E_MUGSHOT_SHOW_STATE_IDLE;    // this+0x5C (0)
    // this+0x40: CgsID seeded to 0x800000000 (HIDWORD == 8, LODWORD == 0).
    mRoadRuleMugshotBeatenRoadID = (static_cast<u64>(8) << 32);
    mfMugshotCaptureTimer       = -1.0f;                      // this+0x50
    mbIsAnythingPaused          = false;                      // this+0x6C
    mfMugshotShowTimer          = -1.0f;                      // this+0x54
    meMugshotCaptureRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_INVALID; // this+0x48
    meMugshotShowRaceCarIndex    = ::E_ACTIVE_RACE_CAR_INDEX_INVALID; // this+0x4C
    meCaptureMugshotType        = GameStateModuleIO::E_IMAGE_TYPE_COUNT; // this+0x60 (6)
    meShowMugshotType           = GameStateModuleIO::E_IMAGE_TYPE_COUNT; // this+0x64 (6)

    // this+0x00..0x40: maCameraStatusData[8] = {INVALID(-1), 4} each.
    for (s32 liIndex = 0; liIndex < 8; ++liIndex)
    {
        maCameraStatusData[liIndex].meActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;
        maCameraStatusData[liIndex].meCameraStatus       = BrnNetwork::E_CAMERA_STATUS_COUNT;
    }
}

// ---------------------------------------------------------------------------
// Prepare / Release / Destruct -- X360-inlined no-ops for this manager.
// ---------------------------------------------------------------------------
bool MugshotManager::Prepare()  { return true; }
bool MugshotManager::Release()  { return true; }
void MugshotManager::Destruct() { }

// ---------------------------------------------------------------------------
// ResetState -- X360 (BrnMugshotManager.cpp:138). Shared body used by OnRoundStart /
// OnRoundEnd to reset both FSMs and the camera-status cache (round-boundary HIDWORD == 6).
// ---------------------------------------------------------------------------
void MugshotManager::ResetState()
{
    mfMugshotCaptureTimer       = -1.0f;
    meMugshotCaptureState       = E_MUGSHOT_CAPTURE_STATE_IDLE;
    mfMugshotShowTimer          = -1.0f;
    meMugshotShowState          = E_MUGSHOT_SHOW_STATE_IDLE;
    meCaptureMugshotType        = GameStateModuleIO::E_IMAGE_TYPE_COUNT;
    meShowMugshotType           = GameStateModuleIO::E_IMAGE_TYPE_COUNT;
    meMugshotCaptureRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;
    meMugshotShowRaceCarIndex    = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;
    mRoadRuleMugshotBeatenRoadID = (static_cast<u64>(6) << 32); // 0x600000000
    mbIsAnythingPaused          = false;

    for (s32 liIndex = 0; liIndex < 8; ++liIndex)
    {
        maCameraStatusData[liIndex].meActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;
        maCameraStatusData[liIndex].meCameraStatus       = BrnNetwork::E_CAMERA_STATUS_COUNT;
    }
}

// ---------------------------------------------------------------------------
// OnRoundStart -- X360 0x82357A70. Unconditional full reset.
// ---------------------------------------------------------------------------
void MugshotManager::OnRoundStart()
{
    ResetState();
}

// ---------------------------------------------------------------------------
// OnRoundEnd -- X360 0x82357AF8. Reset only when asked.
// ---------------------------------------------------------------------------
void MugshotManager::OnRoundEnd(bool lbResetState)
{
    if (lbResetState)
    {
        ResetState();
    }
}

// ---------------------------------------------------------------------------
// DoesPlayerHaveACamera -- X360 0x823579D8. Find the cache slot for the given player
// and report whether they have a working camera (status != 0; status 4 == COUNT asserts).
// ---------------------------------------------------------------------------
bool MugshotManager::DoesPlayerHaveACamera(::EActiveRaceCarIndex lePlayerRaceCarIndex)
{
    s32 liPlayerIndex = 0;
    while (maCameraStatusData[liPlayerIndex].meActiveRaceCarIndex != lePlayerRaceCarIndex)
    {
        if (++liPlayerIndex >= 8)
            return false;
    }

    CGS_ASSERT(maCameraStatusData[liPlayerIndex].meCameraStatus != BrnNetwork::E_CAMERA_STATUS_COUNT,
               "maCameraStatusData[liPlayerIndex].meCameraStatus != BrnNetwork::E_CAMERA_STATUS_COUNT");

    if (maCameraStatusData[liPlayerIndex].meCameraStatus == BrnNetwork::E_CAMERA_STATUS_NONE)
        return false;
    return true;
}

// ---------------------------------------------------------------------------
// UpdateCameraStatusData -- X360 0x82363AF0. Copy each network player's active-race-car
// slot + camera status into maCameraStatusData[], padding the tail to {INVALID, COUNT}.
// ---------------------------------------------------------------------------
void MugshotManager::UpdateCameraStatusData(const GameStateModuleIO::PreWorldInputBuffer* lpInput)
{
    CGS_ASSERT(lpInput != nullptr, "lpInput");

    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusInterface* lpPlayerStatusInterface =
        lpInput->GetPlayerStatusInterface();
    CGS_ASSERT(lpPlayerStatusInterface != nullptr, "lpPlayerStatusInterface");

    s32 liIndex = 0;
    const s32 liNumPlayers = lpPlayerStatusInterface->GetNumPlayers();
    for (; liIndex < liNumPlayers; ++liIndex)
    {
        // The X360 reads the player record directly; the bounds asserts below mirror its
        // GetPlayerStatusData() asserts (file BrnNetworkModuleInGamePlayerStatusInterface.h).
        CGS_ASSERT(liIndex >= 0, "liIndex >= 0");
        CGS_ASSERT(liIndex < liNumPlayers, "liIndex < miNumPlayers");

        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerData =
            lpPlayerStatusInterface->GetPlayerStatusData(liIndex);
        maCameraStatusData[liIndex].meActiveRaceCarIndex =
            (::EActiveRaceCarIndex)lpPlayerData->meActiveRaceCarIndex;
        maCameraStatusData[liIndex].meCameraStatus       = lpPlayerData->meCameraStatus;
    }

    for (; liIndex < 8; ++liIndex)
    {
        maCameraStatusData[liIndex].meActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;
        maCameraStatusData[liIndex].meCameraStatus       = BrnNetwork::E_CAMERA_STATUS_COUNT;
    }
}

// ---------------------------------------------------------------------------
// StartMugshotCapture -- X360 0x82382E18. Arbitrate which FSM the local player drives for a
// newly-requested capture of mugshot type leMugshotTypeToCapture and queue the GameActions.
// ---------------------------------------------------------------------------
void MugshotManager::StartMugshotCapture(GameStateModuleIO::OutputBuffer* lpOutput,
                                         GameStateModuleIO::EImageType leMugshotTypeToCapture,
                                         ::EActiveRaceCarIndex leMugshotShowRaceCarIndex,
                                         ::EActiveRaceCarIndex leMugshotCaptureRaceCarIndex,
                                         bool lbMugshotRequiresBroadcast)
{
    CGS_ASSERT(mpGameStateModule != nullptr, "mpGameStateModule");
    CGS_ASSERT(mpGameStateModule->GetModeManager() != nullptr, "mpGameStateModule->GetModeManager()");

    if (mbIsAnythingPaused)
        return;

    // Suppress mugshot capture during post-event states (unless it is the victory mugshot).
    const bool lbInPostEvent = mpGameStateModule->GetModeManager()->IsInPostEvent();
    if (lbInPostEvent && leMugshotTypeToCapture != GameStateModuleIO::E_IMAGE_TYPE_VICTORY_MUGSHOT)
        return;

    // Resolve which active-race-car index plays the "show" target and which the "capture" target.
    ::EActiveRaceCarIndex leImageShowRaceCarIndex;
    ::EActiveRaceCarIndex leImageCaptureRaceCarIndex;
    if (lbMugshotRequiresBroadcast)
    {
        leImageShowRaceCarIndex    = leMugshotShowRaceCarIndex;
        leImageCaptureRaceCarIndex = leMugshotCaptureRaceCarIndex;
    }
    else
    {
        leImageShowRaceCarIndex    = leMugshotCaptureRaceCarIndex;
        leImageCaptureRaceCarIndex = leMugshotShowRaceCarIndex;
    }

    if (mpGameStateModule->GetPlayerActiveRaceCarIndex() == leMugshotCaptureRaceCarIndex)
    {
        // The local player is the SHOW target: drive the capture FSM if the new request out-ranks
        // any capture already pending.
        if (KAI_MUGSHOT_PRIORITIES[leMugshotTypeToCapture] < KAI_MUGSHOT_PRIORITIES[meCaptureMugshotType])
        {
            if (meMugshotCaptureState != E_MUGSHOT_CAPTURE_STATE_IDLE)
            {
                // Abort the capture already in flight before starting the new one.
                AbortMugshotGameAction lAbortMugshotAction;
                lAbortMugshotAction.meAbortedShowRaceCar = meMugshotShowRaceCarIndex;
                lAbortMugshotAction.mePlayerRaceCar      = mpGameStateModule->GetPlayerActiveRaceCarIndex();
                lAbortMugshotAction.meImageType          = meCaptureMugshotType;
                lAbortMugshotAction.mbZero               = false;

                CGS_ASSERT(lpOutput != nullptr, "lpOutput");
                CGS_ASSERT(lpOutput->GetGameActionQueue() != nullptr, "lpOutput->GetGameActionQueue()");
                AsVeq(lpOutput->GetGameActionQueue())->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lAbortMugshotAction),
                    KI_GAME_ACTION_ABORT_MUGSHOT, (s32)sizeof(AbortMugshotGameAction));
            }

            meMugshotShowRaceCarIndex = leImageCaptureRaceCarIndex;
            meCaptureMugshotType      = leMugshotTypeToCapture;
            meMugshotCaptureState     = E_MUGSHOT_CAPTURE_STATE_PREPARE_FOR_CAPTURE;
            mfMugshotCaptureTimer     = -1.0f;
        }
    }
    else if (mpGameStateModule->GetPlayerActiveRaceCarIndex() == leMugshotShowRaceCarIndex)
    {
        // The local player is the CAPTURE target: drive the show FSM (only when idle).
        if (meMugshotShowState == E_MUGSHOT_SHOW_STATE_IDLE
            && meMugshotCaptureState == E_MUGSHOT_CAPTURE_STATE_IDLE)
        {
            meMugshotCaptureRaceCarIndex = leImageShowRaceCarIndex;
            meShowMugshotType            = leMugshotTypeToCapture;
            meMugshotShowState           = E_MUGSHOT_SHOW_STATE_PREPARE_TO_CAPTURE_THEIR_MUGSHOT;
            mfMugshotShowTimer           = -1.0f;
        }
        else
        {
            return; // X360 LABEL_36: a capture/show already runs -- do not queue the broadcast.
        }
    }

    // Broadcast the "begin capture" GameAction.
    // X360 (0x82383044-0x82383058): the payload's +0x08 word receives r28 (the CAPTURE-resolved
    // index) and the +0x0C word receives r29 (the SHOW-resolved index) -- the opposite assignment
    // to the other builders' use of these slots. Write +0x08 = capture-resolved, +0x0C = show-resolved.
    StartMugshotGameAction lMugshotPrepareCaptureAction;
    lMugshotPrepareCaptureAction.mImageId        = mRoadRuleMugshotBeatenRoadID;
    lMugshotPrepareCaptureAction.meShowRaceCar   = leImageCaptureRaceCarIndex; // +0x08 (r28)
    lMugshotPrepareCaptureAction.mePlayerRaceCar = leImageShowRaceCarIndex;    // +0x0C (r29)
    lMugshotPrepareCaptureAction.miStageTag      = 0;
    lMugshotPrepareCaptureAction.meImageType     = leMugshotTypeToCapture;
    lMugshotPrepareCaptureAction.mbFlagA         = false;
    lMugshotPrepareCaptureAction.mbBroadcast     = lbMugshotRequiresBroadcast;

    CGS_ASSERT(lpOutput != nullptr, "lpOutput");
    CGS_ASSERT(lpOutput->GetGameActionQueue() != nullptr, "lpOutput->GetGameActionQueue()");
    AsVeq(lpOutput->GetGameActionQueue())->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lMugshotPrepareCaptureAction),
        KI_GAME_ACTION_START_MUGSHOT, (s32)sizeof(StartMugshotGameAction));
}

// ---------------------------------------------------------------------------
// ProcessTakedownEvents -- X360 0x823830D0. For each frame takedown the local player is
// the victim of (and has a camera for), start a payback mugshot capture.
// ---------------------------------------------------------------------------
void MugshotManager::ProcessTakedownEvents(const GameStateModuleIO::PreWorldInputBuffer* /*lpInput*/,
                                           GameStateModuleIO::OutputBuffer* lpOutput,
                                           const CgsModule::EventQueue<TakedownEvent, 8>* lpTakedownEventQueue,
                                           GameStateModuleIO::EGameModeType leGameModeType)
{
    const s32 liNumEvents = lpTakedownEventQueue->GetLength();
    for (s32 liEvent = 0; liEvent < liNumEvents; ++liEvent)
    {
        const TakedownEvent& lEvent = lpTakedownEventQueue->GetEvent(liEvent);
        const EActiveRaceCarIndex leTakedownAggressorRaceCarIndex = lEvent.meAggressorIndex;
        const EActiveRaceCarIndex leTakedownVictimRaceCarIndex    = lEvent.meVictimIndex;

        CGS_ASSERT(leTakedownAggressorRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID,
                   "leTakedownAggressorRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID");
        CGS_ASSERT(leTakedownAggressorRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0
                   && leTakedownAggressorRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leTakedownAggressorRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0 && leTakedownAggressorRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
        CGS_ASSERT(leTakedownVictimRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID,
                   "leTakedownVictimRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID");
        CGS_ASSERT(leTakedownVictimRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0
                   && leTakedownVictimRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leTakedownVictimRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0 && leTakedownVictimRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

        CGS_ASSERT(mpGameStateModule != nullptr, "mpGameStateModule");

        // Only react when the local player is the aggressor or the victim.
        if (mpGameStateModule->GetPlayerActiveRaceCarIndex() != (::EActiveRaceCarIndex)leTakedownAggressorRaceCarIndex
            && mpGameStateModule->GetPlayerActiveRaceCarIndex() != (::EActiveRaceCarIndex)leTakedownVictimRaceCarIndex)
        {
            continue;
        }
        if (!DoesPlayerHaveACamera((::EActiveRaceCarIndex)leTakedownVictimRaceCarIndex))
            continue;

        // Online (non-lobby) takedowns broadcast their mugshot; free-burn-lobby takedowns capture
        // locally without broadcast; everything else (offline online-lobby case) is skipped.
        if (mpGameStateModule->IsOnlineGameMode()
            && !GameStateModuleIO::IsOnlineFreeBurnLobby(leGameModeType))
        {
            StartMugshotCapture(lpOutput, meShowMugshotType,
                                (::EActiveRaceCarIndex)leTakedownAggressorRaceCarIndex,
                                (::EActiveRaceCarIndex)leTakedownVictimRaceCarIndex, true);
        }
        else if (GameStateModuleIO::IsOnlineFreeBurnLobby(leGameModeType))
        {
            StartMugshotCapture(lpOutput, meShowMugshotType,
                                (::EActiveRaceCarIndex)leTakedownAggressorRaceCarIndex,
                                (::EActiveRaceCarIndex)leTakedownVictimRaceCarIndex, false);
        }
    }
}

// ---------------------------------------------------------------------------
// CheckForSuccessfulPayback -- X360 0x823832D0. Scan the network dirty-trick queue for a
// completed payback (type 4) the local player pulled off, and start a payback mugshot.
//
// NOTE: the dirty-trick queue lives on the PreWorldInputBuffer's NetworkToGameStateInterface
// (this+0x2268 region in the X360). That interface is still a named-opaque placeholder
// (BrnGameStateModuleIO.h NetworkToGameStateInterface), so the iteration over its dirty-trick
// records cannot yet be spelled by named members. The control flow + the per-record action are
// reconstructed; FLAG: the queue walk is gated behind GetNetworkToGameStateInterface() and is a
// no-op until that interface's real layout (dirty-trick record array) is homed.
// ---------------------------------------------------------------------------
void MugshotManager::CheckForSuccessfulPayback(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                               GameStateModuleIO::OutputBuffer* /*lpOutput*/)
{
    CGS_ASSERT(lpInput != nullptr, "lpInput");
    CGS_ASSERT(lpInput->GetNetworkToGameStateInterface() != nullptr,
               "lpInput->GetNetworkToGameStateInterface()");
    // The X360 also asserts the dirty-trick queue is non-null; that sub-accessor is not yet on the
    // opaque NetworkToGameStateInterface, so the record walk + StartMugshotCapture(..., PAYBACK ...)
    // per matched dirty-trick (type 4) lands when that interface is homed (see FLAG above).
}

// ---------------------------------------------------------------------------
// HandlePreparingForMugshotCapture -- X360 0x82363C38 (capture FSM, state 1).
// Wait out the prepare delay (longer for the payback type), then advance to CAPTURE.
// ---------------------------------------------------------------------------
void MugshotManager::HandlePreparingForMugshotCapture()
{
    bool lbReadyToCapture;
    if (meCaptureMugshotType == GameStateModuleIO::E_IMAGE_TYPE_PAYBACK_MUGSHOT)
        lbReadyToCapture = (mfMugshotCaptureTimer >= KF_WAIT_PAYBACK_MUGSHOT_DURATION);
    else
        lbReadyToCapture = (mfMugshotCaptureTimer >= KF_WAIT_MUGSHOT_DURATION);

    if (lbReadyToCapture)
    {
        mfMugshotCaptureTimer = -1.0f;
        meMugshotCaptureState = E_MUGSHOT_CAPTURE_STATE_CAPTURE_YOUR_MUGSHOT;
    }
}

// ---------------------------------------------------------------------------
// HandleCapturingMugshot -- X360 0x82383438 (capture FSM, state 2).
// On the first frame (timer just reset to 0) queue the capture action; thereafter wait out the
// per-type capture duration then advance to TAKE.
// ---------------------------------------------------------------------------
void MugshotManager::HandleCapturingMugshot(GameStateModuleIO::OutputBuffer* lpOutput)
{
    if (mfMugshotCaptureTimer == 0.0f)
    {
        StartMugshotGameAction lAction;
        lAction.mImageId        = mRoadRuleMugshotBeatenRoadID;
        lAction.meShowRaceCar   = meMugshotShowRaceCarIndex;
        lAction.mePlayerRaceCar = mpGameStateModule->GetPlayerActiveRaceCarIndex();
        lAction.miStageTag      = 1;
        lAction.meImageType     = meCaptureMugshotType;
        lAction.mbFlagA         = false;
        lAction.mbBroadcast     = (meCaptureMugshotType == GameStateModuleIO::E_IMAGE_TYPE_VICTORY_MUGSHOT);

        CGS_ASSERT(lpOutput != nullptr, "lpOutput");
        CGS_ASSERT(lpOutput->GetGameActionQueue() != nullptr, "lpOutput->GetGameActionQueue()");
        AsVeq(lpOutput->GetGameActionQueue())->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lAction),
            KI_GAME_ACTION_START_MUGSHOT, (s32)sizeof(StartMugshotGameAction));
    }
    else
    {
        const f32 lfCaptureDuration =
            (meCaptureMugshotType == GameStateModuleIO::E_IMAGE_TYPE_VICTORY_MUGSHOT)
                ? KF_CAPTURE_YOUR_VICTORY_MUGSHOT_DURATION
                : KF_CAPTURE_YOUR_MUGSHOT_DURATION;
        if (mfMugshotCaptureTimer >= lfCaptureDuration)
        {
            mfMugshotCaptureTimer = -1.0f;
            meMugshotCaptureState = E_MUGSHOT_CAPTURE_STATE_TAKE_MUGSHOT;
        }
    }
}

// ---------------------------------------------------------------------------
// HandleTakingMugshot -- X360-inlined (capture FSM, state 3). No out-of-line body was emitted
// (the Update switch's case-3 handler collapses to nothing observable in the trace milestone).
// FLAG(still_unbodied): no X360 asm section for this handler in the dossier -- left as a no-op.
// ---------------------------------------------------------------------------
void MugshotManager::HandleTakingMugshot(GameStateModuleIO::OutputBuffer* /*lpOutput*/,
                                         const GameStateModuleIO::VehicleOutputInterface* /*lpVehicleOutput*/,
                                         GameStateModuleIO::EGameModeType /*leGameModeType*/)
{
}

// ---------------------------------------------------------------------------
// HandlePreparingToCaptureTheirMugshot -- X360-inlined (show FSM, state 1). The Update switch's
// case-1 handler is inlined directly (wait KF_WAIT_MUGSHOT_DURATION then advance to CAPTURE), so
// there is no separate body; the transition lives in Update(). Declared for completeness.
// ---------------------------------------------------------------------------
void MugshotManager::HandlePreparingToCaptureTheirMugshot()
{
}

// ---------------------------------------------------------------------------
// HandleCapturingTheirMugshot -- X360 0x82383768 (show FSM, state 2).
// Once the capture-their timeout elapses, queue the capture action and advance to SHOW (resetting
// the show timer + the beaten-road id to the round-reset value 0x600000000).
// ---------------------------------------------------------------------------
void MugshotManager::HandleCapturingTheirMugshot(GameStateModuleIO::OutputBuffer* lpOutput)
{
    if (mfMugshotShowTimer >= KF_CAPTURE_THEIR_MUGSHOT_TIMEOUT_DURATION)
    {
        StartMugshotGameAction lAction;
        lAction.mImageId        = mRoadRuleMugshotBeatenRoadID;
        lAction.meShowRaceCar   = meMugshotCaptureRaceCarIndex;
        lAction.mePlayerRaceCar = mpGameStateModule->GetPlayerActiveRaceCarIndex();
        lAction.miStageTag      = 5;
        lAction.meImageType     = meShowMugshotType;
        lAction.mbFlagA         = true;
        lAction.mbBroadcast     = (meShowMugshotType == GameStateModuleIO::E_IMAGE_TYPE_VICTORY_MUGSHOT);

        CGS_ASSERT(lpOutput != nullptr, "lpOutput");
        CGS_ASSERT(lpOutput->GetGameActionQueue() != nullptr, "lpOutput->GetGameActionQueue()");
        AsVeq(lpOutput->GetGameActionQueue())->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lAction),
            KI_GAME_ACTION_START_MUGSHOT, (s32)sizeof(StartMugshotGameAction));

        mfMugshotShowTimer           = -1.0f;
        mRoadRuleMugshotBeatenRoadID = (static_cast<u64>(6) << 32);
        meShowMugshotType            = GameStateModuleIO::E_IMAGE_TYPE_COUNT;
        meMugshotShowState           = E_MUGSHOT_SHOW_STATE_IDLE;
    }
}

// ---------------------------------------------------------------------------
// HandleShowingTheirMugshot -- X360 0x82383870 (show FSM, state 3).
// On the first frame (timer just reset to 0) queue the show action; once the show duration elapses
// queue the dismiss action and return to IDLE.
// ---------------------------------------------------------------------------
void MugshotManager::HandleShowingTheirMugshot(GameStateModuleIO::OutputBuffer* lpOutput)
{
    if (mfMugshotShowTimer == 0.0f)
    {
        StartMugshotGameAction lAction;
        lAction.mImageId        = mRoadRuleMugshotBeatenRoadID;
        lAction.meShowRaceCar   = meMugshotCaptureRaceCarIndex;
        lAction.mePlayerRaceCar = mpGameStateModule->GetPlayerActiveRaceCarIndex();
        lAction.miStageTag      = 3;
        lAction.meImageType     = meShowMugshotType;
        lAction.mbFlagA         = true;
        lAction.mbBroadcast     = (meShowMugshotType == GameStateModuleIO::E_IMAGE_TYPE_VICTORY_MUGSHOT);

        CGS_ASSERT(lpOutput != nullptr, "lpOutput");
        CGS_ASSERT(lpOutput->GetGameActionQueue() != nullptr, "lpOutput->GetGameActionQueue()");
        AsVeq(lpOutput->GetGameActionQueue())->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lAction),
            KI_GAME_ACTION_START_MUGSHOT, (s32)sizeof(StartMugshotGameAction));
    }
    else if (mfMugshotShowTimer >= KF_SHOW_YOUR_MUGSHOT_DURATION)
    {
        StartMugshotGameAction lAction;
        lAction.mImageId        = mRoadRuleMugshotBeatenRoadID;
        lAction.meShowRaceCar   = meMugshotCaptureRaceCarIndex;
        lAction.mePlayerRaceCar = mpGameStateModule->GetPlayerActiveRaceCarIndex();
        lAction.miStageTag      = 5;
        lAction.meImageType     = meShowMugshotType;
        lAction.mbFlagA         = true;
        lAction.mbBroadcast     = (meShowMugshotType == GameStateModuleIO::E_IMAGE_TYPE_VICTORY_MUGSHOT);

        CGS_ASSERT(lpOutput != nullptr, "lpOutput");
        CGS_ASSERT(lpOutput->GetGameActionQueue() != nullptr, "lpOutput->GetGameActionQueue()");
        AsVeq(lpOutput->GetGameActionQueue())->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lAction),
            KI_GAME_ACTION_START_MUGSHOT, (s32)sizeof(StartMugshotGameAction));

        mfMugshotShowTimer           = -1.0f;
        mRoadRuleMugshotBeatenRoadID = (static_cast<u64>(6) << 32);
        meShowMugshotType            = GameStateModuleIO::E_IMAGE_TYPE_COUNT;
        meMugshotShowState           = E_MUGSHOT_SHOW_STATE_IDLE;
    }
}

// ---------------------------------------------------------------------------
// Update -- X360 0x82391690 (BrnGameStateModule::PreWorldUpdate). Drive both FSMs.
// ---------------------------------------------------------------------------
void MugshotManager::Update(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                            GameStateModuleIO::OutputBuffer* lpOutput,
                            const GameStateModuleIO::VehicleOutputInterface* lpVehicleOutput,
                            const CgsModule::EventQueue<TakedownEvent, 8>* lpTakedownEventQueue,
                            GameStateModuleIO::EGameModeType leGameModeType,
                            bool lbIsAnythingPaused)
{
    mbIsAnythingPaused = lbIsAnythingPaused;

    UpdateCameraStatusData(lpInput);
    ProcessTakedownEvents(lpInput, lpOutput, lpTakedownEventQueue, leGameModeType);
    CheckForSuccessfulPayback(lpInput, lpOutput);

    // Advance both timers by the frame's delta-time (timer[1] * timer[2] in the X360 read: the
    // PreWorldInputBuffer timer-status payload words at +0x04/+0x08); -1.0f means "freshly armed",
    // which seeds the timer at 0.0f without advancing.
    const GameStateModuleIO::TimerStatusInterface* lpTimer = lpInput->GetTimerStatusInterface();
    const f32 lfDeltaTime = lpTimer->maEntries[0].mfValue04 * lpTimer->maEntries[0].mfValue08;

    if (mfMugshotShowTimer == -1.0f)
        mfMugshotShowTimer = 0.0f;
    else
        mfMugshotShowTimer = lfDeltaTime + mfMugshotShowTimer;

    if (mfMugshotCaptureTimer == -1.0f)
        mfMugshotCaptureTimer = 0.0f;
    else
        mfMugshotCaptureTimer = lfDeltaTime + mfMugshotCaptureTimer;

    // ---- capture FSM ----
    switch (meMugshotCaptureState)
    {
        case E_MUGSHOT_CAPTURE_STATE_IDLE:
            break;
        case E_MUGSHOT_CAPTURE_STATE_PREPARE_FOR_CAPTURE:
            HandlePreparingForMugshotCapture();
            break;
        case E_MUGSHOT_CAPTURE_STATE_CAPTURE_YOUR_MUGSHOT:
            HandleCapturingMugshot(lpOutput);
            break;
        case E_MUGSHOT_CAPTURE_STATE_TAKE_MUGSHOT:
            HandleTakingMugshot(lpOutput, lpVehicleOutput, leGameModeType);
            break;
        default:
            CGS_ASSERT(false, "Unknown mugshot capture state: ");
            break;
    }

    // ---- show FSM ----
    switch (meMugshotShowState)
    {
        case E_MUGSHOT_SHOW_STATE_IDLE:
            break;
        case E_MUGSHOT_SHOW_STATE_PREPARE_TO_CAPTURE_THEIR_MUGSHOT:
            if (mfMugshotShowTimer >= KF_WAIT_MUGSHOT_DURATION)
            {
                mfMugshotShowTimer = -1.0f;
                meMugshotShowState = E_MUGSHOT_SHOW_STATE_CAPTURE_THEIR_MUGSHOT;
            }
            break;
        case E_MUGSHOT_SHOW_STATE_CAPTURE_THEIR_MUGSHOT:
            HandleCapturingTheirMugshot(lpOutput);
            break;
        case E_MUGSHOT_SHOW_STATE_SHOW_THEIR_MUGSHOT:
            HandleShowingTheirMugshot(lpOutput);
            break;
        default:
            CGS_ASSERT(false, "Unknown mugshot victim state: ");
            break;
    }
}

// ---------------------------------------------------------------------------
// ProcessImageReceivedEvent -- X360 0x82363C90. A remote player's mugshot arrived: if we are not
// already showing one (and not in a blocked post-event-camera state), latch it and arm the show FSM.
// ---------------------------------------------------------------------------
void MugshotManager::ProcessImageReceivedEvent(const OnlineImageReceivedEvent* lpImageReceivedEvent)
{
    CGS_ASSERT(lpImageReceivedEvent != nullptr, "lpImageReceivedEvent");
    CGS_ASSERT(mpGameStateModule != nullptr, "mpGameStateModule");
    CGS_ASSERT(mpGameStateModule->GetModeManager() != nullptr, "mpGameStateModule->GetModeManager()");

    // X360 gate: proceed when the show FSM is idle, OR it is in the capture-their state AND the
    // current mode is NOT in a blocked post-event camera state.
    bool lbAcceptImage;
    if (meMugshotShowState == E_MUGSHOT_SHOW_STATE_IDLE)
    {
        lbAcceptImage = true;
    }
    else if (meMugshotShowState == E_MUGSHOT_SHOW_STATE_CAPTURE_THEIR_MUGSHOT)
    {
        lbAcceptImage = !mpGameStateModule->GetModeManager()->IsInPostEvent();
    }
    else
    {
        lbAcceptImage = false;
    }

    if (lbAcceptImage)
    {
        const GameStateModuleIO::EImageType leImageType = lpImageReceivedEvent->meImageType;
        if (leImageType <= GameStateModuleIO::E_IMAGE_TYPE_ROAD_RULE_CRASH_MUGSHOT) // <= 4
        {
            meShowMugshotType            = leImageType;
            meMugshotCaptureRaceCarIndex = (::EActiveRaceCarIndex)lpImageReceivedEvent->miCaptureCarWord;
            mfMugshotShowTimer           = -1.0f;
            meMugshotShowState           = E_MUGSHOT_SHOW_STATE_SHOW_THEIR_MUGSHOT;
            mRoadRuleMugshotBeatenRoadID = lpImageReceivedEvent->mImageId;
        }
    }
}

// ---------------------------------------------------------------------------
// ProcessBeatenRoadRuleEvent -- X360 0x82383A40. A road rule was beaten by lBeatenPlayerRaceCarIndex:
// if the local player has a camera, latch the beaten road id and start the road-rule mugshot (crash
// vs time variant chosen by leScoreType: non-zero score -> time mugshot (3), zero -> crash (4)).
// ---------------------------------------------------------------------------
void MugshotManager::ProcessBeatenRoadRuleEvent(GameStateModuleIO::OutputBuffer* lpOutput,
                                                ::EActiveRaceCarIndex lBeatenPlayerRaceCarIndex,
                                                CgsID lBeatenRoadID,
                                                s32 leScoreType)
{
    const CgsID lCachedRoadID = lBeatenRoadID;

    if (DoesPlayerHaveACamera(mpGameStateModule->GetPlayerActiveRaceCarIndex()))
    {
        mRoadRuleMugshotBeatenRoadID = lCachedRoadID;
        // leScoreType == 0 -> +3 (E_IMAGE_TYPE_ROAD_RULE_TIME_MUGSHOT);
        // leScoreType != 0 -> +4 (E_IMAGE_TYPE_ROAD_RULE_CRASH_MUGSHOT).
        const GameStateModuleIO::EImageType leImageType =
            (GameStateModuleIO::EImageType)((leScoreType != 0 ? 1 : 0) + 3);
        StartMugshotCapture(lpOutput, leImageType, lBeatenPlayerRaceCarIndex,
                            mpGameStateModule->GetPlayerActiveRaceCarIndex(), false);
    }
}

// ---------------------------------------------------------------------------
// ProcessOnlineWin -- X360 (BrnMugshotManager.cpp:954). The local player won an online event:
// start a victory mugshot of themselves (show == capture == the local player).
// ---------------------------------------------------------------------------
void MugshotManager::ProcessOnlineWin(GameStateModuleIO::OutputBuffer* lpOutput)
{
    StartMugshotCapture(lpOutput, GameStateModuleIO::E_IMAGE_TYPE_VICTORY_MUGSHOT,
                        mpGameStateModule->GetPlayerActiveRaceCarIndex(),
                        mpGameStateModule->GetPlayerActiveRaceCarIndex(), false);
}

// ---------------------------------------------------------------------------
// ProcessAbortCaptureEvent -- X360 0x82363DE0. The image layer aborted an in-flight capture:
// reset whichever FSM (capture if mbAbortCaptureMachine, else show) back to IDLE.
// ---------------------------------------------------------------------------
void MugshotManager::ProcessAbortCaptureEvent(const OnlineImageCaptureAbortedEvent* lpAbortEvent)
{
    CGS_ASSERT(lpAbortEvent != nullptr, "lpAbortEvent");

    if (lpAbortEvent->mbAbortCaptureMachine)
    {
        mfMugshotCaptureTimer        = -1.0f;
        meMugshotCaptureState        = E_MUGSHOT_CAPTURE_STATE_IDLE;
        mRoadRuleMugshotBeatenRoadID = (static_cast<u64>(6) << 32); // HIDWORD reset to COUNT(6)
        meCaptureMugshotType         = GameStateModuleIO::E_IMAGE_TYPE_COUNT;
    }
    else
    {
        mfMugshotShowTimer = -1.0f;
        meMugshotShowState = E_MUGSHOT_SHOW_STATE_IDLE;
        meShowMugshotType  = GameStateModuleIO::E_IMAGE_TYPE_COUNT;
    }
}

// ---------------------------------------------------------------------------
// ChangeState / UpdateFSMTimers -- X360-inlined helpers (declared in the home). The X360 inlines
// the state writes / timer advance at each call site (see Update / StartMugshotCapture), so no
// out-of-line bodies were emitted; de-inlined definitions provided for the named declarations.
// ---------------------------------------------------------------------------
void MugshotManager::ChangeState(EMugshotCaptureState leNewCaptureState)
{
    meMugshotCaptureState = leNewCaptureState;
}

void MugshotManager::ChangeState(EMugshotShowState leNewShowState)
{
    meMugshotShowState = leNewShowState;
}

void MugshotManager::UpdateFSMTimers(const GameStateModuleIO::TimerStatusInterface* lpTimerStatusInterface)
{
    const f32 lfDeltaTime =
        lpTimerStatusInterface->maEntries[0].mfValue04 * lpTimerStatusInterface->maEntries[0].mfValue08;
    if (mfMugshotShowTimer == -1.0f)
        mfMugshotShowTimer = 0.0f;
    else
        mfMugshotShowTimer += lfDeltaTime;
    if (mfMugshotCaptureTimer == -1.0f)
        mfMugshotCaptureTimer = 0.0f;
    else
        mfMugshotCaptureTimer += lfDeltaTime;
}

} // namespace BrnGameState
