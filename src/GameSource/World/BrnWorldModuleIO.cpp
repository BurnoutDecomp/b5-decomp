// ============================================================================
// b5-decomp/src/GameSource/World/BrnWorldModuleIO.cpp
//
// Out-of-line bodies for the 32 class:BrnWorldIO::UpdateInputBuffer accessors the X360
// build emitted out-of-line. Each body tests the buffer status flag (write-lock
// status>>3&1 / read-lock status>>4&1) then acts on the named member:
//   - const getters assert read-lock  ("Not locked for reading")  and return &member;
//   - mutable getters / Append / Set mutators assert write-lock ("Not locked for writing")
//     and return &member or perform the store-for-store mutation.
// The X360-baked d:\p4 file/line is intentionally not reproduced -- CGS_ASSERT stamps
// __FILE__/__LINE__. Offsets are layout-derived (member-by-name), not hardcoded.
#include "GameSource/World/BrnWorldModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstring>   // memcpy

namespace BrnWorldIO
{

// Pin the X360-proven leading-array byte offsets (store-for-store load-bearing). Non-inline +
// uncalled, but a non-template member function body IS compiled by MSVC, so these fire.
// X360 SetRaceCarColourIndex stores u16 at this+2+2*idx, the valid flag at this+34+idx, etc.
void UpdateInputBuffer::_AssertLayout()
{
    static_assert(offsetof(UpdateInputBuffer, mau16RaceCarColourIndex)         ==  2, "colour@2");
    static_assert(offsetof(UpdateInputBuffer, mau16RaceCarPaintFinishIndex)    == 18, "paint@18");
    static_assert(offsetof(UpdateInputBuffer, mabRaceCarColourIndexValid)      == 34, "colourValid@34");
    static_assert(offsetof(UpdateInputBuffer, mabRaceCarPaintFinishIndexValid) == 42, "paintValid@42");
    static_assert(offsetof(UpdateInputBuffer, mabLostContactThisFrame)         == 50, "lostContact@50");
    static_assert(offsetof(UpdateInputBuffer, mabRegainedContactThisFrame)     == 58, "regained@58");
    static_assert(offsetof(UpdateInputBuffer, mabCarSelectStatus)              == 66, "carSelect@66");
    static_assert(offsetof(UpdateInputBuffer, mabCarSelectStatusValid)         == 74, "carSelectValid@74");
}

// ---- lifecycle -----------------------------------------------------------------

// X360 0x827C9E90 tail seed: the eight-word block at the online-scoring interface + 32 is
// written to -1. The interface interior is not named yet, so the seed is applied through the
// payload by offset [FLAG: serialized-opaque payload, interior owned by its own TU].
void OnlineScoringInterface::Construct()
{
    std::memset(maData, 0, sizeof(maData));
    s32* lpiSlots = reinterpret_cast<s32*>(maData + 32);
    for (s32 liSlot = 0; liSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liSlot)
        lpiSlots[liSlot] = -1;
}

// @ 0x827C9E90 -- construct the world's per-frame Update INPUT buffer. The X360 body is the
// IOBuffer base status (`*this = 1` == eStatusConstructed) followed by the member-wise
// Construct/Clear chain over the ~25 embedded interfaces and event queues, then the trailing
// per-active-race-car clear loop over the eight leading arrays. Without it every embedded
// VariableEventQueue stays un-Constructed and the first consumer (WorldModule::
// HandleGameActions -> VariableEventQueue<13312,16>::GetFirstEvent) fires "Not Constructed".
//
// PARTIAL SLICE (FLAG) -- same shape as the sibling UpdateOutputBuffer::Construct: the members
// whose committed types expose Construct/Clear run the REAL call; the payload slices whose
// canonical interiors are not committed yet (VehicleInputInterface, TrafficNetworkInputInterface
// and the scoring/debug-controller/timer PODs) run their local zero-fill stand-in, and the
// leading zero-fill below covers every byte the X360 seeds to 0 inside them [marked deviation].
// The zero-fill starts at the first member (offset 2, pinned by _AssertLayout) so the IOBuffer
// base status/lock byte is never touched.
void UpdateInputBuffer::Construct()
{
    std::memset(&mau16RaceCarColourIndex, 0,
                sizeof(UpdateInputBuffer) - offsetof(UpdateInputBuffer, mau16RaceCarColourIndex));

    CgsModule::IOBuffer::Construct();                       // X360 *this = 1

    // -- the member chain, in the X360 call order (console offsets cited) --
    mVehicleInputInterface.Construct();                     // +96      VehicleInputInterface
    mVehicleDriverInputInterface.Construct();               // +142272  VehicleDriverInputInterface
    miPlayerRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID; // +147568  = -1
    mGameActionQueue.Construct();                           // +147572  VEQ<13312,16>
    mTimerStatusInterface.Clear();                          // +160900  TimerStatusInterface::Clear
    mTakedownEventQueue.Construct();                        // +160952  TakedownEvent<8>
    // +161288 VEQ<131072,16>::Construct and +292376 InRemoveTriggerEvent<256>::Construct are
    // the committed aggregate's own two embedded queues (see the typedef note in the header).
    mTriggerManagementInputInterface.GetAddTriggerEventQueue().Construct();
    mTriggerManagementInputInterface.GetRemoveTriggerEventQueue().Construct();
    mTriggerQueryInputInterface.Construct();                // +293412  VEQ<4096,16>
    // +297524: the X360 stores the literal 3 == the "no dirty trick" EPaybackType sentinel
    // (the same value BrnGameState::PaybackManager::KE_NO_DIRTY_TRICK carries).
    meActivePaybackType      = static_cast<BrnNetwork::EPaybackType>(3);
    meActivePaybackAggressor = E_ACTIVE_RACE_CAR_INDEX_INVALID;  // +297528 = -1
    mInWorldEventQueue.Construct();                         // +297532  VEQ<4096,16>
    mTrafficNetworkInterface.Construct();                   // +301644  ActivateHull<8> + mbDiverged
    mCrashNetworkInterface.Construct();                     // +301760  CrashIO::NetworkInputInterface
    mPlayerVehicleControls.Clear();                         // +317264  13 f32 + 7 bytes = 0
    mDebugController.Clear();                               // +317324  DebugController::Clear
    mRaceCarRaceDistanceInterface.Clear();                  // +317496  RaceCarRaceDistanceInterface::Clear
    mScoringInterface.Clear();                              // +317536  memset(.., 0, 2736)
    mbControllerActive = false;                             // +320272
    // +320273/+320274 == the two RequestInterface flags (covered by the member's own ctor and
    // the zero-fill; named here to keep the X360 store order visible).
    mWorldEntityRequestInterface.mbInvalidateCollisionWorld = false;
    mWorldEntityRequestInterface.mbValidateCollisionWorld   = false;
    // +320276 the replay status: flag word, the six reel head bytes, the two current-reel
    // indices (-1) and the trailing debug alpha.
    mReplayStatusInterface.mxStatusFlags = 0;
    for (s32 liReel = 0; liReel < 6; ++liReel)
        mReplayStatusInterface.maReels[liReel].macName[0] = '\0';
    mReplayStatusInterface.miCurrentRecordReel   = -1;      // +321824
    mReplayStatusInterface.miCurrentPlaybackReel = -1;      // +321828
    mReplayStatusInterface.mfDebugHudAlpha       = 0.0f;    // +321832
    mAudioCarDataLoadedQueue.Construct();                   // +321840  AudioCarDataLoadedEvent<16>
    mRaceRouteRequestQueue.Construct();                     // +322240  RaceRouteRequest<1>
    mOnlineScoringInterface.Construct();                    // +322384  (+32 block seeded to -1)

    // -- the trailing per-active-race-car clear loop (X360 0x827CA094..0x827CA0E4). The
    //    increment runs through the range-guarded EActiveRaceCarIndex operator++ (its
    //    "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT" assert is the one baked at
    //    BurnoutConstants.h:39 in the X360 body).
    for (EActiveRaceCarIndex leIndex = E_ACTIVE_RACE_CAR_INDEX_0;
         leIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT;
         leIndex++)
    {
        mau16RaceCarColourIndex[leIndex]         = 0;
        mau16RaceCarPaintFinishIndex[leIndex]    = 0;
        mabRaceCarColourIndexValid[leIndex]      = false;
        mabRaceCarPaintFinishIndexValid[leIndex] = false;
        mabLostContactThisFrame[leIndex]         = false;
        mabRegainedContactThisFrame[leIndex]     = false;
        mabCarSelectStatus[leIndex]              = false;
        mabCarSelectStatusValid[leIndex]         = false;
    }
}

// ---- per-active-race-car colour / paint / contact / select (store-only) -----
// These X360 bodies bounds-assert the index then store into the leading arrays;
// they take no lock (the producing module fills them while write-locked, but the
// individual stores carry no per-call lock test in the binary).

// X360 0x823A8340 (:203) -- store colour index + mark valid.
void UpdateInputBuffer::SetRaceCarColourIndex(EActiveRaceCarIndex leActiveRaceCarIndex, u16 lu16ColourIndex)
{
    CGS_ASSERT(leActiveRaceCarIndex >= 0,                            "leActiveRaceCarIndex >= 0");
    CGS_ASSERT(leActiveRaceCarIndex <  E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    mau16RaceCarColourIndex[leActiveRaceCarIndex]    = lu16ColourIndex;
    mabRaceCarColourIndexValid[leActiveRaceCarIndex] = true;
}

// X360 0x823A83C8 (:216) -- store paint-finish index + mark valid.
void UpdateInputBuffer::SetRaceCarPaintFinishIndex(EActiveRaceCarIndex leActiveRaceCarIndex, u16 lu16PaintIndex)
{
    CGS_ASSERT(leActiveRaceCarIndex >= 0,                            "leActiveRaceCarIndex >= 0");
    CGS_ASSERT(leActiveRaceCarIndex <  E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    mau16RaceCarPaintFinishIndex[leActiveRaceCarIndex]    = lu16PaintIndex;
    mabRaceCarPaintFinishIndexValid[leActiveRaceCarIndex] = true;
}

// X360 0x823A8450 (:228) -- flag lost-contact for this active race car.
void UpdateInputBuffer::SetLostContact(EActiveRaceCarIndex leActiveRaceCarIndex)
{
    CGS_ASSERT((leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT) && (leActiveRaceCarIndex >= 0),
               "( leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT ) && ( leActiveRaceCarIndex >= 0 )");
    mabLostContactThisFrame[leActiveRaceCarIndex] = true;
}

// X360 0x823A84C0 (:236) -- flag regained-contact for this active race car.
void UpdateInputBuffer::SetRegainedContact(EActiveRaceCarIndex leActiveRaceCarIndex)
{
    CGS_ASSERT((leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT) && (leActiveRaceCarIndex >= 0),
               "( leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT ) && ( leActiveRaceCarIndex >= 0 )");
    mabRegainedContactThisFrame[leActiveRaceCarIndex] = true;
}

// X360 0x823A8530 (:245) -- store car-select status + mark valid. Unlike its SetLostContact/
// SetRegainedContact siblings (one combined assert), the X360 body fires two SEPARATE asserts
// here (bge then blt, each its own BeginAssert/FireAssert/EndAssert) -- modelled as two
// CGS_ASSERTs to match.
void UpdateInputBuffer::SetCarSelectStatus(EActiveRaceCarIndex leActiveRaceCarIndex, bool lbStatus)
{
    CGS_ASSERT(leActiveRaceCarIndex >= 0,                            "leActiveRaceCarIndex >= 0");
    CGS_ASSERT(leActiveRaceCarIndex <  E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    mabCarSelectStatus[leActiveRaceCarIndex]      = lbStatus;
    mabCarSelectStatusValid[leActiveRaceCarIndex] = true;
}

// ---- vehicle input interfaces ----------------------------------------------

// (:259 R) const vehicle-input accessor (inlined on X360; provided for completeness).
const VehicleInputInterface* UpdateInputBuffer::GetVehicleInputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mVehicleInputInterface;
}

// X360 0x823C8AD0 (:260 W) -- merge the source vehicle-input interface into ours (+96).
void UpdateInputBuffer::AppendVehicleInputInterface(const VehicleInputInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mVehicleInputInterface.Append(lpInterface);
}

// X360 0x827A3660 (:262 R, GetVehicl) -- const vehicle-driver-input accessor (+142272).
const VehicleDriverInputInterface* UpdateInputBuffer::GetVehicleDriverInputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mVehicleDriverInputInterface;
}

// X360 0x823DB6C0 (:263 W) -- merge the source vehicle-driver-input interface (+142272).
// The X360 forwards `this` (the interface's leading mDriverUpdateQueue member) straight into
// BrnPhysics::Vehicle::VehicleDriverInputInterface::Append, a thin queue-merge; modelled via the
// committed type's own GetUpdateDriverQueue()->Append(...) (VariableEventQueue<5040,16>::Append).
void UpdateInputBuffer::AppendVehicleDriverInputInterface(const VehicleDriverInputInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mVehicleDriverInputInterface.GetUpdateDriverQueue()->Append(*lpInterface->GetUpdateDriverQueue());
}

// ---- game-action queue ------------------------------------------------------

// X360 0x823B4738 (:266 W, GetG) -- mutable game-action queue accessor (+147572).
GameActionQueue* UpdateInputBuffer::GetGameActionQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mGameActionQueue;
}

// X360 0x823C8B80 (:267 W) -- merge a source game-action queue into ours (+147572).
// The X360 dispatches VariableEventQueue<13312,16>::Append<13312,16>(this->mGameActionQueue, src)
// directly (a single bulk memcpy of the source's packed bytes + one overflow check), with no
// null/self guard. Call the committed bulk Append by name instead of re-walking events one by one.
void UpdateInputBuffer::AppendGameActionQueue(const GameActionQueue* lpQueue)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mGameActionQueue.Append(*lpQueue);
}

// ---- timer status -----------------------------------------------------------

// X360 0x823B47E0 (:270 W) -- copy the source timer-status block into +160900..+160948.
void UpdateInputBuffer::SetTimerStatusInterface(const TimerStatusInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mTimerStatusInterface = *lpInterface;   // store-for-store (12-word block)
}

// ---- takedown event queue ---------------------------------------------------

// X360 0x823C8C38 (:273 W) -- merge a source takedown-event queue into ours (+160952).
// The X360 dispatches BrnGameState::TakedownEvent_::Append, the committed
// CgsModule::EventQueue<TakedownEvent,8> (== BaseEventQueue<TakedownEvent>) instantiation's own
// Append merge -- called by name.
void UpdateInputBuffer::AppendTakedownEventQueue(const TakedownEventQueue* lpQueue)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mTakedownEventQueue.Append(*lpQueue);
}

// ---- trigger query interface ------------------------------------------------

// X360 0x823C8CF0 (:279 W) -- merge a source trigger-query queue into ours (+293412).
// The X360 dispatches VariableEventQueue<4096,16>::Append<4096,16>(this->mTriggerQueryInputInterface,
// src) directly (a single bulk memcpy + one overflow check), with no null/self guard. Call the
// committed bulk Append by name instead of re-walking events one by one.
void UpdateInputBuffer::AppendTriggerQueryInputInterface(const TriggerQueryInputInterface* lpQueue)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mTriggerQueryInputInterface.Append(*lpQueue);
}

// ---- traffic / crash network interfaces -------------------------------------

// X360 0x827A3AF8 (:284 R, Get) -- const traffic-network accessor (+301644).
const TrafficNetworkInputInterface* UpdateInputBuffer::GetTrafficNetworkInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mTrafficNetworkInterface;
}

// X360 0x823C8DA8 (:285 W) -- update the traffic-network interface (member @+301644).
// FAITHFULNESS NOTE: the X360 does NOT blanket-copy the interface; it performs a SELECTIVE
// 3-field update within the member -- zero the queue's miLength at member+8 (BaseEventQueue::
// Clear()), BaseEventQueue<ActivateHullEvent>::Append the source's activate-hull queue into the
// member's queue, and copy the mbDiverged byte at member+0x6C (this+301752). The canonical home
// (BrnTraffic::BrnTrafficIO::TrafficNetworkInputInterface, BrnTrafficNetworkInterfaces.h) IS
// committed and byte-confirms this 3-op shape, but it only exposes a const GetActivateHullQueue()
// + SetDiverged/HasDiverged -- no public mutator lets an outside caller Clear()+Append() its
// private queue. That mutator must be grown onto BrnTrafficNetworkInterfaces.h/.cpp (a different
// TU) before the 3 ops can be re-modelled by name here; until then this stays a local placeholder
// abstracted into Set(). The member touched + offsets are correct.
void UpdateInputBuffer::SetTrafficNetworkInterface(const TrafficNetworkInputInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mTrafficNetworkInterface.Set(lpInterface);
}

// X360 0x827A3BA0 (:287 R, GetCrashNetworkIn) -- const crash-network accessor (+301760).
const CrashNetworkInputInterface* UpdateInputBuffer::GetCrashNetworkInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mCrashNetworkInterface;
}

// X360 0x823C8E70 (:288 W) -- set the crash-network interface (+301760).
// The X360 forwards to BrnWorld::CrashIO::NetworkInputInterface::operator= (bitset copy + a
// clear+merge per race-car crashing-traffic queue), the committed type's own operator=.
void UpdateInputBuffer::SetCrashNetworkInterface(const CrashNetworkInputInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mCrashNetworkInterface = *lpInterface;
}

// ---- debug controller -------------------------------------------------------

// X360 0x827BBE48 (:290 R, GetDebugContro) -- const debug-controller accessor (+317324).
const DebugController* UpdateInputBuffer::GetDebugController() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mDebugController;
}

// X360 0x823B49B0 (:291 W) -- mutable debug-controller accessor (+317324).
DebugController* UpdateInputBuffer::GetDebugController()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mDebugController;
}

// ---- race-car race-distance interface ---------------------------------------

// X360 0x823B4A58 (:295 W) -- copy the source race-distance block into the member.
void UpdateInputBuffer::SetRaceCarRaceDistanceInterface(const RaceCarRaceDistanceInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mRaceCarRaceDistanceInterface = *lpInterface;   // store-for-store (10-word block)
}

// ---- scoring interfaces -----------------------------------------------------

// X360 0x827A3CF0 (:297 R, Ge) -- const scoring accessor (+317536).
const ScoringInterface* UpdateInputBuffer::GetScoringInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mScoringInterface;
}

// X360 0x823B4B28 (:298 W) -- copy the source scoring block (2736 bytes) into +317536.
void UpdateInputBuffer::SetScoringInterface(const ScoringInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    std::memcpy(&mScoringInterface, lpInterface, sizeof(ScoringInterface));
}

// X360 0x823B4BE0 (:301 W) -- copy the source online-scoring block (164 bytes) into +322384.
void UpdateInputBuffer::SetOnlineScoringInterface(const OnlineScoringInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    std::memcpy(&mOnlineScoringInterface, lpInterface, sizeof(OnlineScoringInterface));
}

// ---- controller-active flag -------------------------------------------------

// X360 0x827A3E40 (:303 R) -- const controller-active read (+320272).
bool UpdateInputBuffer::GetControllerActive() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mbControllerActive;
}

// X360 0x823B4C98 (:304 W) -- set controller-active (+320272).
void UpdateInputBuffer::SetControllerActive(bool lbActive)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mbControllerActive = lbActive;
}

// ---- world-entity request interface -----------------------------------------

// X360 0x823B4D48 (:307 W, GetWorldEntityRequestI) -- mutable request accessor (+320273).
WorldEntityRequestInterface* UpdateInputBuffer::GetWorldEntityRequestInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mWorldEntityRequestInterface;
}

// X360 0x827A3EF0 (:308 R, GetWorldEntityRe) -- const request accessor (+320273).
const WorldEntityRequestInterface* UpdateInputBuffer::GetWorldEntityRequestInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mWorldEntityRequestInterface;
}

// ---- replay status interface ------------------------------------------------

// X360 0x827A3F98 (:310 R, GetReplayStatusInter) -- const replay-status accessor (+320276).
const ReplayStatusInterface* UpdateInputBuffer::GetReplayStatusInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mReplayStatusInterface;
}

// X360 0x823B4DF0 (:311 W) -- set the replay-status interface (+320276).
// The X360 forwards to BrnReplays::ReplayIO::StatusInterface::operator= (member-wise copy: status
// flags, all 6 reels, the two current-reel indices, and the trailing debug alpha), the committed
// type's own operator=.
void UpdateInputBuffer::SetReplayStatusInterface(const ReplayStatusInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mReplayStatusInterface = *lpInterface;
}

// ---- player vehicle controls ------------------------------------------------

// X360 0x823B48F8 (:282 W) -- copy the source player-vehicle-controls (60 bytes) into +317264.
void UpdateInputBuffer::SetPlayerVehicleControls(const PlayerVehicleControls* lpControls)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    std::memcpy(&mPlayerVehicleControls, lpControls, sizeof(PlayerVehicleControls));
}

// ---- active payback ---------------------------------------------------------

// X360 0x823B4F50 (:317 W) -- set the active payback type.
void UpdateInputBuffer::SetActivePaybackType(BrnNetwork::EPaybackType lePaybackType)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    meActivePaybackType = lePaybackType;
}

// X360 0x823B5000 (:319 W) -- set the active payback aggressor.
void UpdateInputBuffer::SetActivePaybackAggressor(EActiveRaceCarIndex leAggressor)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    meActivePaybackAggressor = leAggressor;
}

// ---- ADDITIVE GROW (wave33: WorldBridgeInput* consumer-side const getters) ----

// X360 0x827A3708 (:265 R, IDA "UpdateInputBuffer") -- const game-action queue accessor (+147572).
const GameActionQueue* UpdateInputBuffer::GetGameActionQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mGameActionQueue;
}

// X360 0x827A3858 (:272 R, IDA "UpdateInputBuffer_") -- const takedown-event queue accessor (+160952).
const TakedownEventQueue* UpdateInputBuffer::GetTakedownEventQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mTakedownEventQueue;
}

// X360 0x827A3900 (:276 R, IDA "UpdateInputB") -- const trigger-management-input accessor (+161288).
const TriggerManagementInputInterface* UpdateInputBuffer::GetTriggerManagementInputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mTriggerManagementInputInterface;
}

// X360 0x827A3C48 (:294 R, IDA "UpdateInputBuff") -- const race-car race-distance accessor (+317496).
const RaceCarRaceDistanceInterface* UpdateInputBuffer::GetRaceCarRaceDistanceInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mRaceCarRaceDistanceInterface;
}

// X360 0x827A3510 (:255 R, IDA "Upda") -- const AI race-route-request-queue accessor (+322240).
// Returns &mRaceRouteRequestQueue (DWARF member :353 == BrnAI::RouteMapModuleIO::RaceRouteRequestQueue
// == CgsModule::EventQueue<RaceRouteRequest,1>). Consumed by WorldModule::BridgeInputToAIModule.
// (Host LLP64 offset differs from X360 +322240; parity is by named member.)
const UpdateInputBuffer::RaceRouteRequestQueue* UpdateInputBuffer::GetRaceRouteRequestQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mRaceRouteRequestQueue;
}

}   // namespace BrnWorldIO
