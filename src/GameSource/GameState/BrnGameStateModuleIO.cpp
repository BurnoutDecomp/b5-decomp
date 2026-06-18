#include "GameSource/GameState/BrnGameStateModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CgsDev::Assert Begin/Fire/EndAssert

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
    return reinterpret_cast<const ControllerInput*>(&mControllerInputStorage);
}

// X360 0x8231CD80 - read-lock accessor for the buffered game-event queue (this+0x4C).
const GameEventQueue* PreWorldInputBuffer::GetGameEventQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return reinterpret_cast<const GameEventQueue*>(&mGameEventQueueStorage);
}

// X360 0x823B8C60 - write-lock (mutable) accessor for the game-event queue (this+0x4C).
GameEventQueue* PreWorldInputBuffer::GetGameEventQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return reinterpret_cast<GameEventQueue*>(&mGameEventQueueStorage);
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
    return reinterpret_cast<const NetworkPlayerResultsInterface*>(&mNetworkPlayerResultsInterfaceStorage);
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
    return reinterpret_cast<const GameEventQueue*>(&mGameEventQueueStorage);
}

// X360 0x823B91B0 - write-lock accessor for the game-event queue (this+0xA4B0).
GameEventQueue* PostWorldInputBuffer::GetGameEventQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return reinterpret_cast<GameEventQueue*>(&mGameEventQueueStorage);
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

// =====================  OutputBuffer (GameStateModuleIO TU)  =====================

// X360 0x8231D4B8 - write-lock accessor for the game-action queue (this+0x04).
GameActionQueue* OutputBuffer::GetGameActionQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return reinterpret_cast<GameActionQueue*>(&mGameActionQueueStorage);
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

// X360 0x823630F0 - write-lock accessor for the race-car race-distance interface (this+0x2A48C).
RaceCarRaceDistanceInterface* OutputBuffer::GetRaceCarRaceDistanceInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return reinterpret_cast<RaceCarRaceDistanceInterface*>(&mRaceCarRaceDistanceInterfaceStorage);
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
EActiveRaceCarIndex OutputBuffer::GetActivePaybackAggressor() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return meActivePaybackAggressor;
}

// X360 0x82362ED0 - write-lock setter for meActivePaybackAggressor (this+173184).
// (Verifier fix: was CGS_ASSERT_W, an undefined macro -> explicit Begin/Fire/End.)
void OutputBuffer::SetActivePaybackAggressor(EActiveRaceCarIndex leAggressor)
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
