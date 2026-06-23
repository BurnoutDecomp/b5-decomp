#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsEventQueue.h" // CgsModule::EventQueue<T,N>

#include <cstddef>   // offsetof
#include <cstring>   // std::memcpy (models the X360 inlined field-by-field block copy)

// Pin the X360-proven PadOutputInformation field offsets (the controller bridges read these by
// name; the record must stay 932 bytes so OutputBuffer::maPadOutputInformation[7] does not move).
namespace CgsInput { namespace InputIO {
static_assert(sizeof(PadOutputInformation) == 932, "PadOutputInformation must be 0x3A4 bytes");
static_assert(offsetof(PadOutputInformation, mfStickLX)       == 0x00,  "stickLX @ +0x00");
static_assert(offsetof(PadOutputInformation, maActionInfo)    == 0x18,  "action table @ +0x18");
static_assert(offsetof(PadOutputInformation, muConnectionWord)== 0x398, "connection word @ +0x398");
static_assert(offsetof(PadOutputInformation, meControllerState)==0x39C, "controller state @ +0x39C");
static_assert(offsetof(PadOutputInformation, mbDisconnected)  == 0x3A0, "disconnected flag @ +0x3A0");
static_assert(sizeof(ActionInfo) == 8, "ActionInfo must be 8 bytes");

// Pin the sizes of the two record types the Set/Get accessors below copy.
static_assert(sizeof(CgsInput::Device::WheelFFSpring) == 8, "WheelFFSpring must be 8 bytes");
static_assert(sizeof(CgsSystem::TimerStatusInterface) == 48, "TimerStatusInterface must be 48 bytes");
} }

// =============================================================================
// CgsInput::InputIO IO-buffer accessors (the genuine "class layout" of this TU).
// Each lock-guarded accessor asserts the IOBuffer lock (read-lock bit 4 for const
// getters, write-lock bit 3 for non-const getters) then returns &member-at-X360-offset.
// Owner / full-name / return-type / const-ness for each derived strictly from the
// DecFIGS DWARF (CgsInputModuleIO.h) cross-checked against return-offset + lock-direction.
// =============================================================================

namespace CgsInput
{
namespace InputIO
{

// =====================  PostWorldInputBuffer  =====================

// X360 0x828E6A88 - read-lock accessor for the bind-request queue (this+4).
// Caller: CgsInput::InputModule::ProcessBindRequestQueue.
const PostWorldInputBuffer::BindRequestQueue* PostWorldInputBuffer::GetBindRequestQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mBindRequestQueue;
}

// X360 0x828E6BD8 - read-lock accessor for the pad-mapping queue (this+156).
// Caller: CgsInput::InputModule::ProcessMappingQueue. mPadMappingQueue is held as raw
// storage (PadMapping un-homed); the returned pointer reproduces the X360 `return a1 + 156`.
const PostWorldInputBuffer::PadMappingQueue* PostWorldInputBuffer::GetPadMappingQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return reinterpret_cast<const PadMappingQueue*>(&mPadMappingQueueStorage);
}

// X360 0x828E6C80 - read-lock accessor returning the published wheel FFB spring (this+632).
// Caller: CgsInput::InputModule::PostWorldUpdate (copies the two spring words into its module state).
const CgsInput::Device::WheelFFSpring* PostWorldInputBuffer::GetWheelFFSpring() const
{
    // Member scope grants access to pin the modelled PC offset (matches the X360 this+632 stride;
    // the gap in the header is sized off the PC member sizes -- see the header note on pointer width).
    static_assert(offsetof(PostWorldInputBuffer, mWheelFFSpring) == 632, "mWheelFFSpring kept at the X360 +632 stride");
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mWheelFFSpring;
}

// X360 0x823B0F80 - write-lock accessor that copies a WheelFFSpring into the buffer (this+632).
// Caller: BrnGame::BrnGameModule::DoUpdate_InputPostWorld (passes the vehicle output interface's
// WheelFFSpring sub-record). The X360 body copies the two source words verbatim into this+632/+636.
void PostWorldInputBuffer::SetWheelFFSpring(const CgsInput::Device::WheelFFSpring& lSpring)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mWheelFFSpring = lSpring;
}

// =====================  OutputBuffer  =====================

// X360 0x823B1038 (ledger "OutputBuf") - read-lock accessor for the bind-result queue (this+4).
const OutputBuffer::BindResultQueue* OutputBuffer::GetBindResultQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mBindResultQueue;
}

// X360 0x823B10E0 (ledger "OutputB") - read-lock accessor for the unbind-result queue (this+112).
const OutputBuffer::UnBindResultQueue* OutputBuffer::GetUnbindResultQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mUnBindResultQueue;
}

// X360 0x823B1188 (ledger "Outpu") - read-lock accessor for the pad-disconnected queue (this+220).
const OutputBuffer::PadDisconnectedQueue* OutputBuffer::GetPadDisconnectedQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mPadDisconnectedQueue;
}

// X360 0x828E6DD0 (ledger "OutputBuffer_") - write-lock accessor for the unbind-result queue (this+112).
// Caller: CgsInput::InputModule::PreWorldUpdate. (DWARF spells the name "Unbind".)
OutputBuffer::UnBindResultQueue* OutputBuffer::GetUnbindResultQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return &mUnBindResultQueue;
}

// X360 0x828E6E78 (ledger "OutputBuffe") - write-lock accessor for the pad-disconnected queue (this+220).
// Caller: CgsInput::InputPads::Update.
OutputBuffer::PadDisconnectedQueue* OutputBuffer::GetPadDisconnectedQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return &mPadDisconnectedQueue;
}

// X360 0x823B1230 - read-lock accessor returning the iPort-th pad output record (this+296+932*iPort).
// Callers: BrnGame::BrnGameModule::{GetPadInfoForPlayer0, BridgeControllerToDirector,
// DoUpdate_InputPreWorld, BridgeControllerToWorld, DoUpdate_Effects, BridgeControllerToGui}.
// The X360 asserts the read lock then the pad index against [0,4) - matching the three
// inline asserts at CgsInputModuleIO.h:1203-1205 (messages preserved verbatim, typos and all).
const PadOutputInformation* OutputBuffer::GetPadInfo(s32 iPort) const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    CGS_ASSERT(iPort < 4, "Invalid pad specified\n");
    CGS_ASSERT(iPort >= 0, "Port must me positive\n");
    return &maPadOutputInformation[iPort];
}

// =====================  PreWorldInputBuffer  =====================

// X360 0x828E6740 - read-lock accessor for the play-jolt-effect event queue (this+4).
// Caller chain: CgsInput::InputModule::ProcessRumbleRequests.
const PreWorldInputBuffer::PlayJoltEffectEventQueue* PreWorldInputBuffer::GetPlayJoltEffectEventQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mPlayJoltEffectEventQueue;
}

// X360 0x828E69E0 - read-lock accessor returning the published timer snapshot (this+868).
// Caller: CgsInput::InputModule::ProcessRumbleRequests. (DWARF abbreviates the name to
// "GetTimerStatusInt"; it returns the TimerStatusInterface pointer at this+868.)
const CgsSystem::TimerStatusInterface* PreWorldInputBuffer::GetTimerStatusInt() const
{
    // Member scope grants access to pin the modelled PC offset (matches the X360 this+868 stride).
    static_assert(offsetof(PreWorldInputBuffer, mTimerStatusInterface) == 868, "mTimerStatusInterface kept at the X360 +868 stride");
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mTimerStatusInterface;
}

// X360 0x82362418 - write-lock accessor copying a TimerStatusInterface into the buffer (this+868).
// Caller: BrnGameState::RumbleManager::BridgeRumbleToInput. The X360 body copies the source
// interface's two 24-byte TimerStatus blocks (game then sim) field-by-field into this+868; a
// block copy reproduces that store-for-store. (TimerStatusInterface user-declares its operator=
// as a separate, not-yet-recovered TU, so this copies the 48 trivially-copyable bytes directly --
// the same effect as the X360 inlined field stores, without depending on that other TU's body.)
void PreWorldInputBuffer::SetTimerStatusInterface(const CgsSystem::TimerStatusInterface& lTimerStatus)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    std::memcpy(&mTimerStatusInterface, &lTimerStatus, sizeof(CgsSystem::TimerStatusInterface));
}

}
}
