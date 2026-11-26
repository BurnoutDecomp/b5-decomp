#include "GameSource/Effects/SharedIO/BrnEffectsModuleIO_OutputBuffer.h"
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"   // CGS_ASSERT
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\effects\\EffectsModuleIO.h"
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

// ---- Construct (DWARF :308, X360 0x822881B0) ---------------------------------------------
// Status = 1, the two request queues Construct + Clear (RequestInterface<N>::Construct does
// both), and the replay request interface's Construct (its eleven serialiser slots nulled).
void OutputBuffer::Construct()
{
    // ARTIST 0x822881C8..0x82288210: the vault (AttribSys<2048>) queue's Construct + Clear
    // first (DWARF lists AttribSysRequestInterface<2048>::Construct/Clear inlined here), then
    // the resource queue, then the replay request interface's inline Construct.
    CgsModule::IOBuffer::Construct();
    mVaultRequestInterface.mRequestQueue.Construct();
    mVaultRequestInterface.mRequestQueue.Clear();
    mResourceRequestInterface.Construct();
    mReplayRequestInterface.Construct();
}

// ============================== getters (read-lock, status bit 4) ==============================

// X360 0x823BABF0 (:291): read-lock handle to the resource-request interface.
const OutputBuffer::EffectsModuleResourceQueue* OutputBuffer::GetResourceRequestInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), CGS_ASSERT_STREAM_MESSAGE_AT("Not locked for reading\n", CGS_ARTIST_FILE, 291));
    return &mResourceRequestInterface;
}

// X360 0x823BAC98 (:296): read-lock handle to the vault-request interface.
const OutputBuffer::VaultRequestInterface* OutputBuffer::GetVaultRequestInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), CGS_ASSERT_STREAM_MESSAGE_AT("Not locked for reading\n", CGS_ARTIST_FILE, 295));
    return &mVaultRequestInterface;
}

// X360 0x823BAD40 (:298): read-lock handle to the replay-request interface.
const OutputBuffer::ReplayRequestInterface* OutputBuffer::GetReplayRequestInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), CGS_ASSERT_STREAM_MESSAGE_AT("Not locked for reading\n", CGS_ARTIST_FILE, 298));
    return &mReplayRequestInterface;
}

// ============================== write-lock getters (status bit 3) ==============================

// X360 0x8227E130 (:292).
OutputBuffer::EffectsModuleResourceQueue* OutputBuffer::GetResourceRequestInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), CGS_ASSERT_STREAM_MESSAGE_AT("Not locked for writing\n", CGS_ARTIST_FILE, 292));
    return &mResourceRequestInterface;
}

// X360 0x8227E1D8 (:295).
OutputBuffer::VaultRequestInterface* OutputBuffer::GetVaultRequestInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), CGS_ASSERT_STREAM_MESSAGE_AT("Not locked for writing\n", CGS_ARTIST_FILE, 296));
    return &mVaultRequestInterface;
}

// X360 0x8227E280 (:300).
OutputBuffer::ReplayRequestInterface* OutputBuffer::GetReplayRequestInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), CGS_ASSERT_STREAM_MESSAGE_AT("Not locked for writing\n", CGS_ARTIST_FILE, 300));
    return &mReplayRequestInterface;
}

// ============================== setter (write-lock) ==============================

// X360 0x8227E078 (:290): write-lock store of the resource-request interface -- the console
// memcpy's the whole 0x1010-byte RequestInterface<4096>; on the host the same object is
// copied whole (its queue buffer is inline, nothing to alias). EffectsModule::Prepare hands
// the particle module's PrepareOutputBuffer queue through here while the FX bundle loads.
void OutputBuffer::SetResourceRequestInterface(const EffectsModuleResourceQueue* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), CGS_ASSERT_STREAM_MESSAGE_AT("Not locked for writing\n", CGS_ARTIST_FILE, 290));
    mResourceRequestInterface = *lpInterface;
}

}   // namespace EffectsIO
}   // namespace BrnEffects

// DecFIGS EffectsModuleIO.cpp:153: the whole body is the base teardown. ARTIST ICF-folds it into
// BrnWorld::PropEntityIO::OutputBuffer_PreScene::Destruct @0x822DC3D0 (`b CgsModule::IOBuffer::Destruct`),
// the target DestroyIOBuffer<OutputBuffer> @0x823AE848 calls.
void BrnEffects::EffectsIO::OutputBuffer::Destruct()
{
    CgsModule::IOBuffer::Destruct();
}

#undef CGS_ARTIST_FILE
