#include "GameSource/GameState/BrnGameStateModuleIO.h"

#include <cstring>                                   // std::memset (OutputBuffer::Construct's console zero-fills)
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CgsDev::Assert Begin/Fire/EndAssert
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h" // RequestInterface<3072> (the +0x3414 member's real type)
#include "GameSource/GameState/BrnGameStateSharedIO.h"            // RaceCarRaceDistanceInterface / ScoringOutputInterface / OnlineScoringOutputInterface (the tail members' real types)
#include "GameSource/World/EntityModules/TriggerEntityModule/SharedIO/BrnTriggerEntityModuleInputInterface.h" // TriggerManagementInputInterface (Construct's two embedded queues)

// =============================================================================
// BrnGameState::GameStateModuleIO buffer accessors.
//
// Two TUs land here: the GameStateModuleIO TU (15 lock-guarded buffer accessors across
// PreWorldInputBuffer/PostWorldInputBuffer/OutputBuffer) and the OutputBuffer TU (18 OutputBuffer
// accessors). Every body asserts the buffer's lock state (read-lock bit 4 for const getters,
// write-lock bit 3 for non-const getters/setters) then returns/reads the member at its exact X360
// offset. The X360-baked file/line strings are emitted via the explicit Begin/Fire/End sequence
// (NOT CGS_ASSERT, which would stamp __FILE__/__LINE__).
// =============================================================================

namespace BrnGameState
{
namespace GameStateModuleIO
{

// =====================  PreWorldInputBuffer  =====================

// X360 0x823632F8 - read-lock accessor for the controller input (this+0x34).
const ControllerInput* PreWorldInputBuffer::GetControllerInput() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mControllerInput;
}

// X360 0x8231CE28 - read-lock accessor for the timer-status payload (this+0x04).
const TimerStatusInterface* PreWorldInputBuffer::GetTimerStatusInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mTimerStatusInterface;
}

// X360 0x823B8D08 - write-lock setter copying both 0x18-byte timer-status entries (this+0x04).
void PreWorldInputBuffer::SetTimerStatusInterface(const TimerStatusInterface* lpSource)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    for (s32 liEntry = 0; liEntry < 2; ++liEntry)
    {
        TimerStatusInterface::Entry&       lDest = mTimerStatusInterface.maEntries[liEntry];
        const TimerStatusInterface::Entry& lSrc  = lpSource->maEntries[liEntry];
        lDest.miWord00  = lSrc.miWord00;
        lDest.mfValue04 = lSrc.mfValue04;
        lDest.mfValue08 = lSrc.mfValue08;
        lDest.mbFlag0C  = lSrc.mbFlag0C;
        lDest.miWord10  = lSrc.miWord10;
        lDest.mfValue14 = lSrc.mfValue14;
    }
}

// X360 0x823BA240 - write-lock setter deriving the controller button-state block (this+0x34) from
// the per-player pad action-info source. Each bool is the corresponding action-record status bit
// (bit1 == pressed this frame, bit0 == held). Two asserts: the write lock and a non-null source.
void PreWorldInputBuffer::SetButtonPressed(const ControllerActionSource* lpActionInfo)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    CGS_ASSERT(lpActionInfo != nullptr, "No action info supplied\n");

    const u32 KU_PRESSED_BIT = 2;
    const u32 KU_HELD_BIT    = 1;

    mControllerInput.mbDrivingActive       = (lpActionInfo->mStatus18C & KU_PRESSED_BIT) != 0;
    mControllerInput.mbReverseHeld         = (lpActionInfo->mStatus16C & KU_PRESSED_BIT) != 0;
    mControllerInput.mbHandbrakeHeld       = (lpActionInfo->mStatus174 & KU_PRESSED_BIT) != 0;
    mControllerInput.mbBoostHeld           = (lpActionInfo->mStatus1AC & KU_PRESSED_BIT) != 0;
    mControllerInput.mbAction14C           = (lpActionInfo->mStatus14C & KU_PRESSED_BIT) != 0;
    mControllerInput.mbAction154           = (lpActionInfo->mStatus154 & KU_PRESSED_BIT) != 0;
    mControllerInput.mbAction15C           = (lpActionInfo->mStatus15C & KU_PRESSED_BIT) != 0;
    mControllerInput.mbAction164           = (lpActionInfo->mStatus164 & KU_PRESSED_BIT) != 0;
    mControllerInput.mbAction1B4Pressed    = (lpActionInfo->mStatus1B4 & KU_PRESSED_BIT) != 0;
    mControllerInput.mbAction1B4Held       = (lpActionInfo->mStatus1B4 & KU_HELD_BIT) != 0;
    mControllerInput.mbAction1BCPressed    = (lpActionInfo->mStatus1BC & KU_PRESSED_BIT) != 0;
    mControllerInput.mbAction1BCHeld       = (lpActionInfo->mStatus1BC & KU_HELD_BIT) != 0;
    mControllerInput.mbAction05CPressed    = (lpActionInfo->mStatus05C & KU_PRESSED_BIT) != 0;
    mControllerInput.mbAction1BCHeldDup    = (lpActionInfo->mStatus1BC & KU_HELD_BIT) != 0;
    mControllerInput.mbAction1B4And1BCHeld =
        ((lpActionInfo->mStatus1B4 & KU_HELD_BIT) != 0) && ((lpActionInfo->mStatus1BC & KU_HELD_BIT) != 0);
    mControllerInput.mbAction024Pressed    = (lpActionInfo->mStatus024 & KU_PRESSED_BIT) != 0;
    mControllerInput.mbAction1D4Pressed    = (lpActionInfo->mStatus1D4 & KU_PRESSED_BIT) != 0;
    mControllerInput.mbAction004Held       = (lpActionInfo->mStatus004 & KU_HELD_BIT) != 0;
    mControllerInput.mbAction1E4Pressed    = (lpActionInfo->mStatus1E4 & KU_PRESSED_BIT) != 0;
    mControllerInput.mbAction13CPressed    = (lpActionInfo->mStatus13C & KU_PRESSED_BIT) != 0;
    mControllerInput.mbBothSticksDeflected =
        (lpActionInfo->mfStickX > 0.25f) && (lpActionInfo->mfStickY > 0.25f);
}

// X360 0x823C9550 - write-lock setter copying an InGamePlayerStatusInterface into this+0x2CC8.
// Forwards to the committed BrnNetwork InGamePlayerStatusInterface::operator= (X360 0x8236B020).
void PreWorldInputBuffer::SetPlayerStatusInterface(
        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusInterface* lpSource)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mPlayerStatusInterface = *lpSource;
}

// X360 0x823C53D0 - write-lock setter copying a PlayerResultsInterface into this+0x36B8.
// Forwards to the committed BrnNetwork PlayerResultsInterface::operator= (X360 0x823B8F68).
void PreWorldInputBuffer::SetNetworkPlayerResultsInterface(const NetworkPlayerResultsInterface* lpSource)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mNetworkPlayerResultsInterface = *lpSource;
}

// X360 0x8231CD80 - read-lock accessor for the buffered game-event queue (this+0x4C).
const GameEventQueue* PreWorldInputBuffer::GetGameEventQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    // [gateui] `&member`, not a reinterpret_cast over a u8 seat -- the seat is the real type now.
    return &mGameEventQueue;
}

// X360 0x823B8C60 - write-lock (mutable) accessor for the game-event queue (this+0x4C).
GameEventQueue* PreWorldInputBuffer::GetGameEventQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return &mGameEventQueue;
}

// X360 0x823B8E18 - write-lock accessor for the takedown-event input queue (this+0x660).
TakedownEventInputQueueType* PreWorldInputBuffer::GetTakedownEventInputQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return reinterpret_cast<TakedownEventInputQueueType*>(&mTakedownEventInputQueueStorage);
}

// X360 0x8231D020 - read-lock accessor for the network player-results interface (this+0x36B8).
const NetworkPlayerResultsInterface* PreWorldInputBuffer::GetNetworkPlayerResultsInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mNetworkPlayerResultsInterface;
}

// X360 0x8231CED0 - read-lock accessor for the network-to-game-state input interface (this+0x7B0).
const NetworkToGameStateInterface* PreWorldInputBuffer::GetNetworkToGameStateInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mNetworkToGameStateInterface;
}

// =====================  PostWorldInputBuffer  =====================

// X360 0x8231D218 - read-lock accessor for the vehicle output interface (this+0x220).
const VehicleOutputInterface* PostWorldInputBuffer::GetVehicleOutputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return reinterpret_cast<const VehicleOutputInterface*>(&mVehicleOutputInterfaceStorage);
}

// X360 0x823B9300 - write-lock accessor for the vehicle output interface (this+0x220).
VehicleOutputInterface* PostWorldInputBuffer::GetVehicleOutputInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return reinterpret_cast<VehicleOutputInterface*>(&mVehicleOutputInterfaceStorage);
}

// X360 0x8231D0C8 - read-lock accessor for the game-event queue (this+0xA4B0).
const GameEventQueue* PostWorldInputBuffer::GetGameEventQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    // [gateui] `&member`, not a reinterpret_cast over a u8 seat -- the seat is the real type now.
    // This is the queue owner `bridge`'s BridgeWorldToGameState @0x823E5368 Appends the world's
    // per-frame game events into, and the one GameStateModule::PostWorldUpdate @0x8238F358 drains
    // into the module's carry queue.
    return &mGameEventQueue;
}

// X360 0x823B91B0 - write-lock accessor for the game-event queue (this+0xA4B0).
GameEventQueue* PostWorldInputBuffer::GetGameEventQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return &mGameEventQueue;
}

// X360 0x8231D410 - read-lock accessor for the AI-car output interface (this+0xAAC0).
const AICarOutputInterface* PostWorldInputBuffer::GetAICarOutputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mAICarOutputInterface;
}

// X360 0x823B9648 - write-lock (mutable) accessor for the AI-car output interface (this+0xAAC0).
// Non-const twin of GetAICarOutputInterface() const.
AICarOutputInterface* PostWorldInputBuffer::GetAICarOutputInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return &mAICarOutputInterface;
}

// X360 0x823C9600 - write-lock-guarded forwarder onto the traffic-type response queue (this+0xBFA8).
// Asserts the write lock, then merges lSource into the member queue via
// CgsModule::BaseEventQueue<TrafficTypeResponse>::Append.
bool PostWorldInputBuffer::AppendTrafficTypeResponseQueue(
        const CgsModule::BaseEventQueue<BrnTraffic::BrnTrafficIO::TrafficTypeResponse>& lSource)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return mTrafficTypeResponseQueue.Append(lSource);
}

// X360 0x8231D170 - read-lock accessor for the race-car crash-event queue (this+0x10).
const RaceCarCrashEventQueue* PostWorldInputBuffer::GetRaceCarCrashEventQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mRaceCarCrashEventQueue;
}

// X360 0x82362A30 - read-lock accessor for the trigger-entity-module output interface (this+0x6E34).
const TriggerEntityModuleOutputInterface* PostWorldInputBuffer::GetTriggerEntityOutputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mTriggerEntityOutputInterface;
}

// X360 0x823B9450 - write-lock (mutable) twin accessor for the trigger-entity-module output
// interface (this+0x6E34).
TriggerEntityModuleOutputInterface* PostWorldInputBuffer::GetTriggerEntityOutputInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return &mTriggerEntityOutputInterface;
}

// =====================  OutputBuffer (GameStateModuleIO TU)  =====================

// ----------------------------------------------------------------------------
// X360 0x82382940 - OutputBuffer::Construct.
//
// The console body is fully recovered; its construct list (verbatim, by byte offset) is:
//     *this            = 1                                       // IOBuffer status: constructed
//     VariableEventQueue<13312,16>::Construct(this + 4)          // mGameActionQueue    @ +0x0004
//     VariableEventQueue<3072,16>::Construct (this + 13332)      // ResourceRequestIface@ +0x3414
//     VariableEventQueue<3072,16>::Clear     (this + 13332)
//     this+16420 = 0; this+16424 = 1.0f                          // TimerRequestInterface
//     this+16428 = 0; this+16432 = 1.0f                          // FrameRateTypeRequestInterface
//     this+16436 = 0; this+16440 = 0
//     TakedownEvent<..,8>::Construct         (this + 16448)      // TakedownEventOutputQ@ +0x4040
//     DirtyTrickEvent<..,28>::Construct      (this + 16784)
//     GameStateToNetworkInterface::Clear     (this + 16784)
//     BaseInputEvent<..,8>::Construct        (this + 17324)      // input BIND request queue
//     BaseInputEvent<..,8>::Construct        (this + 17400)      // input UNBIND request queue
//     this+17332 = 0; this+17408 = 0
//     GameStateToGuiInterface::Construct     (this + 17488)      // @ +0x4450
//     VariableEventQueue<18432,16>::Construct(this + 18496)      // GuiEventQueue       @ +0x4840
//     VariableEventQueue<131072,16>::Construct(this + 36944)     // TriggerMgmtIface    @ +0x9050
//     InRemoveTriggerEvent<..,256>::Construct(this + 168032)
//     VariableEventQueue<4096,16>::Construct (this + 169068)
//     this+173180 = 3 (EPaybackType); this+173184 = -1 (aggressor)
//     Time::SetFloatVal(this + 173188, 0.0f)
//     8 x s32 zeroed from this+173196; this+173228 = 0.0f; this+173232 = 0
//     memset(this + 173240, 0, 2736)                             // ScoringOutputInterface
//     this+175860 = -1; 8 x s32 = -1 from this+176008
//     this+176336 = 0; this+176344/348/352/356 = -1
//     this+184768 = 0; this+192484 = 0; this+192488/489/490 = 0
//
// ⚠️ FLAG (PC, partial): this model still carries several of those members as OPAQUE u8 storage
// (see the private section below), so only the members that are real typed objects here can be
// constructed. Grow this body as each opaque span is typed; do NOT pretend the buffer is fully
// constructed -- the explicit "STILL NOT MADE" list at the end of the body is the checklist.
// ----------------------------------------------------------------------------
void OutputBuffer::Construct()
{
    // The re-homed +0x3414 member must occupy EXACTLY the console's span, or every later
    // offset in this buffer moves. RequestInterface<3072> is pointer-free, so its host size
    // is the console's.
    static_assert(sizeof(BrnResource::GameDataIO::RequestInterface<3072>)
                      == (0x4024 - 0x3414),
                  "ResourceRequestInterface must be exactly 3088 bytes (RequestInterface<3072>)");

    CgsModule::IOBuffer::Construct();

    // Constructed WITHOUT the write lock on purpose: the console does the same (Construct runs
    // before anybody can lock the buffer), and the queue's own Construct is what makes
    // AddEvent/Append legal at all.
    reinterpret_cast<GameActionQueue*>(&mGameActionQueueStorage)->Construct();

    // ⭐ 2026-08-01: the console's own construct list (transcribed above) runs
    //     VariableEventQueue<3072,16>::Construct(this + 13332)
    //     VariableEventQueue<3072,16>::Clear    (this + 13332)
    // on the resource-request interface. It was missing here, so EVERY producer that reaches
    // GetResourceRequestInterface() -- TriggerQueryManager::Prepare, GameStateModule::Prepare's
    // list stages, StreetManager::LoadAIData, StuntManager::LoadDistrictMap -- would have been
    // AddEvent-ing onto an unconstructed queue. Now real.
    {
        BrnResource::GameDataIO::RequestInterface<3072>* lpRequests =
            reinterpret_cast<BrnResource::GameDataIO::RequestInterface<3072>*>(
                &mResourceRequestInterfaceStorage);
        lpRequests->mRequestQueue.Construct();
        lpRequests->mRequestQueue.Clear();
    }

    // ⭐⭐ 2026-08-01 (BridgeGameStateToWorld wave): the console's construct list for the members
    // that bridge READS. Until now every one of these was inside an opaque blob, so none of them
    // could be built -- and BridgeGameStateToWorld hands five of them straight to the world's
    // Append*/Set* legs. Reading an unconstructed VariableEventQueue fires its own
    // "Not Constructed" assert (CgsVariableEventQueue.h) and hands the destination a garbage
    // length; that is exactly the hazard the trigger wave found on the resource-request queue,
    // one layer along. These are the console's OWN calls, transcribed above and now made:
    //     VariableEventQueue<131072,16>::Construct(this + 36944)   // mgmt add queue
    //     InRemoveTriggerEvent<..,256>::Construct (this + 168032)  // mgmt remove queue
    //     VariableEventQueue<4096,16>::Construct  (this + 169068)  // trigger-query interface
    mTriggerManagementInputInterface.GetAddTriggerEventQueue().Construct();
    mTriggerManagementInputInterface.GetRemoveTriggerEventQueue().Construct();
    mTriggerQueryInputInterface.Construct();

    //     this+173180 = 3 (EPaybackType); this+173184 = -1 (aggressor)
    //     Time::SetFloatVal(this + 173188, 0.0f)
    // NOTE the payback seeds are NOT zero: the console's "no payback" idle value is 3, and the
    // aggressor is the invalid active-race-car index. BridgeGameStateToWorld copies both into the
    // world input buffer every frame, so a zero-initialised buffer would publish payback type 0
    // (a REAL payback kind) with aggressor car 0 from the first frame the bridge runs.
    meActivePaybackType      = static_cast<BrnNetwork::EPaybackType>(3);
    meActivePaybackAggressor = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;   // the GLOBAL enum -- see the accessor's ⚠️
    mGameModeElapsedTime.miSeconds  = 0;
    mGameModeElapsedTime.mfFraction = 0.0f;

    //     8 x s32 zeroed from this+173196; this+173228 = 0.0f; this+173232 = 0
    // == RaceCarRaceDistanceInterface::Clear (X360 0x82357470) on the +173196 member.
    reinterpret_cast<RaceCarRaceDistanceInterface*>(&mRaceCarRaceDistanceInterfaceStorage)->Clear();

    //     memset(this + 173240, 0, 2736)   // the scoring snapshot, console width
    std::memset(&mScoringOutputInterfaceStorage, 0, sizeof(mScoringOutputInterfaceStorage));

    //     8 x s32 = -1 from this+176008
    // 176008 - 175976 == 0x20 == OnlineScoringOutputInterface::maOnlineAwards, so the console's
    // "-1 block" is that array seeded to the invalid award id. Zero the interface first (the
    // console's own zero-fill covers it) then stamp the named member.
    std::memset(&mOnlineScoringOutputInterfaceStorage, 0, sizeof(mOnlineScoringOutputInterfaceStorage));
    {
        OnlineScoringOutputInterface* lpOnline =
            reinterpret_cast<OnlineScoringOutputInterface*>(&mOnlineScoringOutputInterfaceStorage);
        for (s32 liCar = 0; liCar < 8; ++liCar)
            lpOnline->maOnlineAwards[liCar] = static_cast<EOnlineAwardID>(-1);
    }

    //     this+192488/489/490 = 0
    mbSetUpAllEventStartsInterfaceIsValid  = false;
    mbSpecificGameModeEventInterfaceIsValid = false;
    mbControllerActive                      = false;

    // ⚠️ STILL NOT MADE, and named so nobody has to re-derive them:
    //   * TakedownEvent<..,8>::Construct(this + 16448) -- the takedown-event OUTPUT queue, which
    //     BridgeGameStateToWorld also forwards. Its member is still opaque storage
    //     (TakedownEventOutputQueueType is a forward-declared incomplete class here), so it
    //     cannot be constructed from this TU. It is SAFE to forward unconstructed only because
    //     BaseEventQueue<T>::Append early-outs on a zero-length source and the buffer's storage
    //     starts zeroed -- that is a property of the container, not a design, so construct it the
    //     moment that member is typed.
    //   * DirtyTrickEvent<..,28>::Construct + GameStateToNetworkInterface::Clear (this + 16784),
    //     the two input bind/unbind request queues (this + 17324 / 17400),
    //     GameStateToGuiInterface::Construct (this + 17488),
    //     VariableEventQueue<18432,16>::Construct (this + 18496) -- all still opaque.
    //   * the console's `this+175860 = -1` seed inside the scoring snapshot (== scoring + 2620).
    //     DELIBERATELY NOT REPRODUCED: the x64 ScoringOutputInterface layout is 2672 bytes against
    //     the console's 2736, so console byte 2620 does not name a member here. Poking it would be
    //     an offset hack over a type whose members ARE known -- when the field is identified from
    //     the console layout map, write it BY NAME.
}

// X360 0x8231D4B8 - write-lock accessor for the game-action queue (this+0x04).
GameActionQueue* OutputBuffer::GetGameActionQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return reinterpret_cast<GameActionQueue*>(&mGameActionQueueStorage);
}

// X360 0x823B96F0 (exported unnamed as sub_823B96F0) - read-lock accessor for the game-action
// queue (this+0x04); assert __FILE__/__LINE__ = BrnGameStateModuleIO.h:265.
const GameActionQueue* OutputBuffer::GetGameActionQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return reinterpret_cast<const GameActionQueue*>(&mGameActionQueueStorage);
}

// X360 0x823B9840 (exported unnamed as sub_823B9840) - read-lock accessor for the takedown-event
// output queue (this+0x4040); assert __FILE__/__LINE__ = BrnGameStateModuleIO.h:271.
const TakedownEventOutputQueueType* OutputBuffer::GetTakedownEventOutputQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return reinterpret_cast<const TakedownEventOutputQueueType*>(&mTakedownEventOutputQueueStorage);
}

// X360 0x823B9798 - read-lock accessor for the resource-request interface (this+0x3414).
const ResourceRequestInterface* OutputBuffer::GetResourceRequestInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return reinterpret_cast<const ResourceRequestInterface*>(&mResourceRequestInterfaceStorage);
}

// X360 0x82362B80 - write-lock accessor for the takedown-event output queue (this+0x4040).
TakedownEventOutputQueueType* OutputBuffer::GetTakedownEventOutputQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return reinterpret_cast<TakedownEventOutputQueueType*>(&mTakedownEventOutputQueueStorage);
}

// X360 0x8231D8A8 - write-lock accessor for the game-state-to-GUI interface (this+0x4450).
GameStateToGuiInterface* OutputBuffer::GetGameStateToGuiInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return reinterpret_cast<GameStateToGuiInterface*>(&mGameStateToGuiInterfaceStorage);
}

// X360 0x823B9D80 - read-lock (const) twin accessor for the game-state-to-GUI interface (this+0x4450,
// == 17488). class:BrnGameState catch-all TU; BridgeGameStateToGui reads it under a read lock.
const GameStateToGuiInterface* OutputBuffer::GetGameStateToGuiInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return reinterpret_cast<const GameStateToGuiInterface*>(&mGameStateToGuiInterfaceStorage);
}

// X360 0x823630F0 - write-lock accessor for the race-car race-distance interface (this+0x2A48C).
RaceCarRaceDistanceInterface* OutputBuffer::GetRaceCarRaceDistanceInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return reinterpret_cast<RaceCarRaceDistanceInterface*>(&mRaceCarRaceDistanceInterfaceStorage);
}

// X360 0x82362D78 - write-lock accessor for the game-state-to-controller output interface (this+0x43AC).
GameStateToControllerInterface* OutputBuffer::GetGameStateToControllerInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return &mGameStateToControllerInterface;
}

// X360 0x823B9AE0 - read-lock accessor for the trigger-management input interface (this+0x9050).
const TriggerManagementInputInterface* OutputBuffer::GetTriggerManagementInputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mTriggerManagementInputInterface;
}

// X360 0x8231D758 - write-lock (mutable) twin accessor for the trigger-management input interface
// (this+0x9050).
TriggerManagementInputInterface* OutputBuffer::GetTriggerManagementInputInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return &mTriggerManagementInputInterface;
}

// ---- BridgeGameStateToWorld (X360 0x823E1890) read side -------------------------------------
// The four sources that bridge reads off this buffer, in the order it reads them.

// X360 0x823B9B88 (exported unnamed as sub_823B9B88) - read-lock accessor for the trigger-query
// input interface (this+169068); assert __FILE__/__LINE__ = BrnGameStateModuleIO.h:279.
// RECOVERED IDENTITY, same route as GetGameActionQueue() const: the exports carry it unnamed, and
// the member it returns is pinned by the return offset (169068 == 36944 + 132124, i.e. exactly
// past the trigger-management interface) plus the console's own Construct call
// `VariableEventQueue<4096,16>::Construct(this + 169068)`.
const TriggerQueryInputInterface* OutputBuffer::GetTriggerQueryInputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mTriggerQueryInputInterface;
}

// X360 0x823BA038 (exported unnamed as sub_823BA038) - read-lock accessor for the race-car
// race-distance interface (this+173196); const twin of the 0x823630F0 write-side accessor.
const RaceCarRaceDistanceInterface* OutputBuffer::GetRaceCarRaceDistanceInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return reinterpret_cast<const RaceCarRaceDistanceInterface*>(&mRaceCarRaceDistanceInterfaceStorage);
}

// INLINED on the X360 -- BridgeGameStateToWorld computes `outputBuffer + 173240` itself
// (0x823E1938 `addis r4,r30,3` / 0x823E1940 `addi r4,r4,-0x5B48`) and hands the address straight
// to UpdateInputBuffer::SetScoringInterface, so there is no callable symbol and no lock assert of
// its own (the bridge holds the buffer's read lock across the whole call).
//
// ⚠️ THE STORAGE IS DELIBERATELY THE CONSOLE'S WIDTH, NOT sizeof(ScoringOutputInterface).
// MEASURED on this x64 build: sizeof(ScoringOutputInterface) == 2672, against the console's
// 2736-byte span (173240..175976, the width OutputBuffer::Construct memsets and the width
// UpdateInputBuffer::SetScoringInterface memcpy's). Handing the world a pointer to a 2672-byte
// object it will read 2736 bytes out of is a 64-byte over-read; keeping the console's span as
// opaque storage and viewing it through the committed type makes the read in-bounds by
// construction. Do NOT "tidy" this into a typed member until the world-side ScoringInterface
// slice is sized off the same type.
const ScoringOutputInterface* OutputBuffer::GetScoringOutputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    static_assert(sizeof(ScoringOutputInterface) <= (175976 - 173240),
                  "ScoringOutputInterface must fit the console's +173240 span (2736 bytes)");
    return reinterpret_cast<const ScoringOutputInterface*>(&mScoringOutputInterfaceStorage);
}

// INLINED on the X360 -- `outputBuffer + 175976` (0x823E1948 / 0x823E1950). Here the committed
// type and the console span agree exactly (164 == 164), so there is no slack.
const OnlineScoringOutputInterface* OutputBuffer::GetOnlineScoringOutputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    static_assert(sizeof(OnlineScoringOutputInterface) <= (176140 - 175976),
                  "OnlineScoringOutputInterface must fit the console's +175976 span (164 bytes)");
    return reinterpret_cast<const OnlineScoringOutputInterface*>(&mOnlineScoringOutputInterfaceStorage);
}

// INLINED on the X360 -- BridgeGameStateToSound @0x823CDE50 computes `outputBuffer + 176344`
// itself and hands the address straight to RootInputBuffer::SetGameModeInterface (the same
// idiom as the two scoring accessors above; the bridge holds the read lock). 16-byte record
// (the root setter's 4-word copy). Phase C3b.
const GameModeOutputInterface* OutputBuffer::GetGameModeOutputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return reinterpret_cast<const GameModeOutputInterface*>(&mGameModeOutputInterfaceStorage);
}

// =====================  OutputBuffer (OutputBuffer TU)  =====================

// X360 0x8231D560 - write-lock accessor for the resource-request interface (this+0x3414).
ResourceRequestInterface* OutputBuffer::GetResourceRequestInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return reinterpret_cast<ResourceRequestInterface*>(&mResourceRequestInterfaceStorage);
}

// X360 0x8231D608 - write-lock accessor for the timer-request interface (this+16420).
OutputBufferTimerRequestInterface* OutputBuffer::GetTimerRequestInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return reinterpret_cast<OutputBufferTimerRequestInterface*>(&mTimerRequestInterfaceStorage);
}

// X360 0x823B98E8 - read-lock accessor for the timer-request interface (this+16420).
const OutputBufferTimerRequestInterface* OutputBuffer::GetTimerRequestInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return reinterpret_cast<const OutputBufferTimerRequestInterface*>(&mTimerRequestInterfaceStorage);
}

// X360 0x8231D6B0 - write-lock accessor for the frame-rate-type request interface (this+16436).
OutputBufferFrameRateTypeReqInterface* OutputBuffer::GetFrameRateTypeRequestInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return reinterpret_cast<OutputBufferFrameRateTypeReqInterface*>(&mFrameRateTypeRequestInterfaceStorage);
}

// X360 0x823B9990 - read-lock accessor for the frame-rate-type request interface (this+16436).
const OutputBufferFrameRateTypeReqInterface* OutputBuffer::GetFrameRateTypeRequestInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return reinterpret_cast<const OutputBufferFrameRateTypeReqInterface*>(&mFrameRateTypeRequestInterfaceStorage);
}

// X360 0x82362C28 - write-lock accessor for the GUI event queue (this+18496).
OutputBufferGuiEventQueue* OutputBuffer::GetGuiEventQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return reinterpret_cast<OutputBufferGuiEventQueue*>(&mGuiEventQueueStorage);
}

// X360 0x823B9A38 - read-lock accessor for the GUI event queue (this+18496).
const OutputBufferGuiEventQueue* OutputBuffer::GetGuiEventQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return reinterpret_cast<const OutputBufferGuiEventQueue*>(&mGuiEventQueueStorage);
}

// X360 0x823B9E28 - read-lock getter for meActivePaybackType (this+173180).
BrnNetwork::EPaybackType OutputBuffer::GetActivePaybackType() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return meActivePaybackType;
}

// X360 0x82362E20 - write-lock setter for meActivePaybackType (this+173180).
void OutputBuffer::SetActivePaybackType(BrnNetwork::EPaybackType lePaybackType)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    meActivePaybackType = lePaybackType;
}

// X360 0x823B9ED8 - read-lock getter for meActivePaybackAggressor (this+173184).
::EActiveRaceCarIndex OutputBuffer::GetActivePaybackAggressor() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return meActivePaybackAggressor;
}

// X360 0x82362ED0 - write-lock setter for meActivePaybackAggressor (this+173184).
// (Verifier fix: was CGS_ASSERT_W, an undefined macro -> explicit Begin/Fire/End.)
void OutputBuffer::SetActivePaybackAggressor(::EActiveRaceCarIndex leAggressor)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    meActivePaybackAggressor = leAggressor;
}

// X360 0x82362F80 - write-lock setter for mGameModeElapsedTime (this+173188, two-word copy).
// (Verifier fix: was CGS_ASSERT_W -> explicit Begin/Fire/End.)
void OutputBuffer::SetGameModeElapsedTime(const OutputBufferTime* lpTime)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mGameModeElapsedTime.miSeconds  = lpTime->miSeconds;
    mGameModeElapsedTime.mfFraction = lpTime->mfFraction;
}

// X360 0x823B9F88 - read-lock getter for mbControllerActive (this+192490).
bool OutputBuffer::GetControllerActive() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return mbControllerActive;
}

// X360 0x82363040 - write-lock setter for mbControllerActive (this+192490).
// (Verifier fix: was CGS_ASSERT_W -> explicit Begin/Fire/End.)
void OutputBuffer::SetControllerActive(bool lbActive)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mbControllerActive = lbActive;
}

// X360 0x823BA0E0 - read-lock getter for mbSetUpAllEventStartsInterfaceIsValid (this+192488).
bool OutputBuffer::GetSetUpAllEventStartsInterfaceIsValid() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return mbSetUpAllEventStartsInterfaceIsValid;
}

// X360 0x82363198 - write-lock setter for mbSetUpAllEventStartsInterfaceIsValid (this+192488).
// (Verifier fix: was CGS_ASSERT_W -> explicit Begin/Fire/End.)
void OutputBuffer::SetSetUpAllEventStartsInterfaceIsValid(bool lbValid)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mbSetUpAllEventStartsInterfaceIsValid = lbValid;
}

// X360 0x823BA190 - read-lock getter for mbSpecificGameModeEventInterfaceIsValid (this+192489).
bool OutputBuffer::GetSpecificGameModeEventInterfaceIsValid() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return mbSpecificGameModeEventInterfaceIsValid;
}

// X360 0x82363248 - write-lock setter for mbSpecificGameModeEventInterfaceIsValid (this+192489).
// (Verifier fix: was CGS_ASSERT_W -> explicit Begin/Fire/End.)
void OutputBuffer::SetSpecificGameModeEventInterfaceIsValid(bool lbValid)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mbSpecificGameModeEventInterfaceIsValid = lbValid;
}

} // namespace GameStateModuleIO
} // namespace BrnGameState
