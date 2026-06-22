#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsEventQueue.h" // CgsModule::EventQueue<T,N>

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

}
}
