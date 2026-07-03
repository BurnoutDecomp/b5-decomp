#include "GameSource/Effects/SharedIO/BrnEffectsModuleIO_OutputBuffer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof
#include <cstring>   // memcpy

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

// ============================== getters (read-lock, status bit 4) ==============================

// X360 0x823BABF0 (:291): read-lock handle to the resource-request interface
// (mResourceRequestInterface @ this+0x4).
const OutputBuffer::EffectsModuleResourceQueue* OutputBuffer::GetResourceRequestInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mResourceRequestInterface;
}

// X360 0x823BAC98 (:296): read-lock handle to the vault-request interface
// (mVaultRequestInterface @ this+0x1014).
const OutputBuffer::VaultRequestInterface* OutputBuffer::GetVaultRequestInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mVaultRequestInterface;
}

// X360 0x823BAD40 (:298): read-lock handle to the replay-request interface
// (mReplayRequestInterface @ this+0x1824).
const OutputBuffer::ReplayRequestInterface* OutputBuffer::GetReplayRequestInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mReplayRequestInterface;
}

// ============================== getters (write-lock, status bit 3) =============================
// The non-const overloads faithfully test the WRITE bit as the asm names it (extrwi bit28);
// reproduced verbatim, NOT "fixed" to the read bit.

// X360 0x8227E130 (:292): write-lock handle to the resource-request interface (this+0x4).
OutputBuffer::EffectsModuleResourceQueue* OutputBuffer::GetResourceRequestInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return &mResourceRequestInterface;
}

// X360 0x8227E1D8 (:295): write-lock handle to the vault-request interface (this+0x1014).
OutputBuffer::VaultRequestInterface* OutputBuffer::GetVaultRequestInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return &mVaultRequestInterface;
}

// X360 0x8227E280 (:300): write-lock handle to the replay-request interface (this+0x1824).
OutputBuffer::ReplayRequestInterface* OutputBuffer::GetReplayRequestInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return &mReplayRequestInterface;
}

// ============================== setters (write-lock, status bit 3) =============================

// X360 0x8227E078 (:290): write-lock store of the resource-request interface. memcpy 0x1010
// (4112) bytes from *lpInterface into mResourceRequestInterface @ this+0x4 -- a whole
// EffectsModuleResourceQueue copy. (X360 tail leaves memcpy's return in r3; DWARF :290
// declares the method void, so the result is discarded.)
void OutputBuffer::SetResourceRequestInterface(const EffectsModuleResourceQueue* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    memcpy(&mResourceRequestInterface, lpInterface, sizeof(mResourceRequestInterface));
}

// ============================== layout pins ==============================
// The X360-attested member offsets are the load-bearing facts of this reconstruction.
void OutputBuffer::_AssertLayout()
{
    static_assert(offsetof(OutputBuffer, mResourceRequestInterface) == 0x4,    "OutputBuffer::mResourceRequestInterface @ +0x4");
    static_assert(offsetof(OutputBuffer, mVaultRequestInterface)    == 0x1014, "OutputBuffer::mVaultRequestInterface @ +0x1014");
    static_assert(offsetof(OutputBuffer, mReplayRequestInterface)   == 0x1824, "OutputBuffer::mReplayRequestInterface @ +0x1824");
}

}   // namespace EffectsIO
}   // namespace BrnEffects
