#include "GameSource/Effects/SharedIO/BrnEffectsModuleIO_OutputBuffer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include <cstring>                                    // std::memset (the replay slots)

// BrnEffects::EffectsIO::OutputBuffer member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. The per-frame buffer the effects module publishes to request
// resources / vault attribs / replay data. Every accessor shares the recurring
// CgsModule::IOBuffer lock-guard prologue: it reads the 1-byte status flag and tests a
// single lock bit -- getters test bit 4 (eStatusLockedForRead, "Not locked for reading\n"),
// setters test bit 3 (eStatusLockedForWrite, "Not locked for writing\n") -- then touches one
// named member. (These Effects lock strings carry the trailing \n per the X360 rodata.)

namespace BrnEffects
{
namespace EffectsIO
{

// ---- Construct (DWARF :308) -----------------------------------------------------------
// The CreateIOBuffer<EffectsIO::OutputBuffer> instantiation is an X360 export hole, so the
// body is the type's own, in the shape every sibling output buffer takes (sound
// RootOutputBuffer, world UpdateOutputBuffer): status = 1, the two request queues
// Construct + Clear (RequestInterface<N>::Construct does both), and the replay request
// interface's eleven serialiser slots zeroed (the sound module's memset of the same
// interface -- there is no ReplayIO::RequestInterface::Construct).
void OutputBuffer::Construct()
{
    CgsModule::IOBuffer::Construct();
    mResourceRequestInterface.Construct();
    mVaultRequestInterface.mRequestQueue.Construct();
    mVaultRequestInterface.mRequestQueue.Clear();
    std::memset(&mReplayRequestInterface, 0, sizeof(mReplayRequestInterface));
}

// ============================== getters (read-lock, status bit 4) ==============================

// X360 0x823BABF0 (:291): read-lock handle to the resource-request interface.
const OutputBuffer::EffectsModuleResourceQueue* OutputBuffer::GetResourceRequestInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mResourceRequestInterface;
}

// X360 0x823BAC98 (:296): read-lock handle to the vault-request interface.
const OutputBuffer::VaultRequestInterface* OutputBuffer::GetVaultRequestInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mVaultRequestInterface;
}

// X360 0x823BAD40 (:298): read-lock handle to the replay-request interface.
const OutputBuffer::ReplayRequestInterface* OutputBuffer::GetReplayRequestInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mReplayRequestInterface;
}

// ============================== write-lock getters (status bit 3) ==============================

// X360 0x8227E130 (:292).
OutputBuffer::EffectsModuleResourceQueue* OutputBuffer::GetResourceRequestInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return &mResourceRequestInterface;
}

// X360 0x8227E1D8 (:295).
OutputBuffer::VaultRequestInterface* OutputBuffer::GetVaultRequestInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return &mVaultRequestInterface;
}

// X360 0x8227E280 (:300).
OutputBuffer::ReplayRequestInterface* OutputBuffer::GetReplayRequestInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return &mReplayRequestInterface;
}

// ============================== setter (write-lock) ==============================

// X360 0x8227E078 (:290): write-lock store of the resource-request interface -- the console
// memcpy's the whole 0x1010-byte RequestInterface<4096>; on the host the same object is
// copied whole (its queue buffer is inline, nothing to alias). EffectsModule::Prepare hands
// the particle module's PrepareOutputBuffer queue through here while the FX bundle loads.
void OutputBuffer::SetResourceRequestInterface(const EffectsModuleResourceQueue* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mResourceRequestInterface = *lpInterface;
}

}   // namespace EffectsIO
}   // namespace BrnEffects
