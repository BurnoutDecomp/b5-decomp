#pragma once

// ===========================================================================
// XAUDIO::CFrameBuffer -- an XAudio interleaved-PCM frame/ring buffer in
// BURNOUT_X360_ARTIST.XEX. This header is the canonical OWNING home for:
//
//     XAUDIO::CFrameBuffer::CFrameBuffer                  @ 0x82964E38
//     XAUDIO::CFrameBuffer::GetProcessingData             @ 0x82964DD8
//     XAUDIO::CFrameBuffer::Prepare                       @ 0x82964FA0
//     XAUDIO::CFrameBuffer::Release                       @ 0x82964F48
//     XAUDIO::CFrameBuffer::`scalar deleting destructor'  @ 0x829651B8
//
// There is no reference source and no DWARF for this TU, so the SHAPE below is
// reconstructed purely from the X360 asm of these five methods. `XAUDIO` is an
// external middleware boundary, so its API identifiers are preserved verbatim
// per the naming convention; reconstructed members use the project Hungarian.
//
// LAYOUT (X360 32-bit byte offsets, grounded in the asm load/store
// displacements; the host build's pointer width shifts the later offsets --
// semantic parity, the exact byte offsets are an X360 compiler artifact):
//   +0x00  mpVTable            -- vtable pointer. The ctor seats CFrameBuffer's
//                                 own vtable (off_821091AC); the scalar deleting
//                                 destructor re-seats the shared base image
//                                 (off_82108FF4). Release dispatches slot [3].
//   +0x04  miRefCount          -- 32-bit reference count (ctor sets 1; Release
//                                 decrements, destructs at 0). Same intrusive
//                                 ref-count idiom as XAUDIO::CObjectRefCount.
//   +0x08  muUseExternalBuffer -- descriptor mode byte (lbz 0(desc) -> stb 8);
//                                 non-zero => use a caller-supplied buffer.
//   +0x0C  mFormat             -- source PCM format block copied from the
//                                 descriptor (channel count @+0x0D, source
//                                 sample rate @+0x10).
//   +0x14  muProcessingStatus  -- status byte emitted as the first word of the
//   +0x15  muPreparedChannels  -- channel count last Prepare()d (read back to
//                                 gauge how much of the buffer is live).
//   +0x16  muReserved16        -- reserved half; +0x14..+0x17 are emitted as one
//                                 word by GetProcessingData (the compiler folds
//                                 the three adjacent fields into a single move).
//   +0x18  muSampleRate        -- the buffer's mix/output sample rate (48000).
//   +0x1C  mpBuffer            -- the (128-byte-aligned) sample buffer: either
//                                 the descriptor's external buffer, or one
//                                 allocated through the supplied allocator.
//   +0x20  muField20           -- a descriptor word (desc +0x10) stashed
//                                 verbatim; not otherwise used by this TU.
// ===========================================================================

#include "types.hpp"

namespace XAUDIO
{

// The PCM format block consumed by CFrameBuffer (8 bytes). Reconstructed from
// the ctor/Prepare asm, which reads a channel-count byte (+0x01) and a sample-
// rate word (+0x04) out of it and copies the leading word verbatim. Modelled
// with named byte fields so the ctor's word copy is a plain struct assignment.
struct CFrameBufferFormat
{
    u8  muByte0;      // +0x00 -- leading byte of the format word (role unknown)
    u8  muChannels;   // +0x01 -- interleaved channel count
    u16 muHalf2;      // +0x02 -- trailing half of the format word (role unknown)
    u32 muSampleRate; // +0x04 -- source sample rate
};

// The initialisation descriptor passed to the CFrameBuffer constructor. Field
// offsets are grounded in the ctor asm (desc +0x00 mode byte, desc +0x04 format
// block, desc +0x0C external buffer, desc +0x10 stashed word).
struct CFrameBufferDesc
{
    u8                 muUseExternalBuffer; // +0x00 -- non-zero => mpExternalBuffer supplies the buffer
    u8                 mPad01[3];           // +0x01
    CFrameBufferFormat mFormat;             // +0x04
    void*              mpExternalBuffer;    // +0x0C -- used only when muUseExternalBuffer != 0
    u32                muField10;           // +0x10 -- stashed into CFrameBuffer::muField20
};

// The out-parameter GetProcessingData fills. The three fields mirror the three
// words the asm copies out of the object (+0x14 status/channels, +0x18 sample
// rate, +0x1C buffer pointer).
struct CFrameBufferProcessingData
{
    u8    muStatus;       // +0x00 -- from CFrameBuffer::muProcessingStatus
    u8    muChannels;     // +0x01 -- from CFrameBuffer::muPreparedChannels
    u16   muReserved;     // +0x02 -- from CFrameBuffer::muReserved16
    u32   muSampleRate;   // +0x04 -- from CFrameBuffer::muSampleRate
    void* mpBuffer;       // +0x08 -- from CFrameBuffer::mpBuffer
};

// FLAGGED: the allocator object the constructor allocates the sample buffer
// through. It is reached only through vtable slot [5] (offset +0x14), an
// Allocate(size)->ptr call; its concrete type is not homed here. This captures
// exactly that one virtual call so the dispatch shape is faithful.
class IFrameBufferAllocator
{
public:
    struct VTable
    {
        void* mpSlot00; // [0]
        void* mpSlot04; // [1]
        void* mpSlot08; // [2]
        void* mpSlot0C; // [3]
        void* mpSlot10; // [4]
        // [5] @+0x14 -- Allocate(this, byteSize) -> raw pointer.
        void* (*mpAllocate)(IFrameBufferAllocator* apSelf, u32 uSize);
    };

    const VTable* mpVTable; // +0x00
};

class CFrameBuffer
{
public:
    // The CFrameBuffer vtable. Only slot [3] (the destructor Release dispatches
    // through at refcount 0) is attested by this TU; the rest are opaque here.
    struct VTable
    {
        void* mpSlot00; // [0] @+0x00
        void* mpSlot04; // [1] @+0x04
        void* mpSlot08; // [2] @+0x08
        // [3] @+0x0C -- destructor slot Release dispatches through at refcount 0.
        void  (*mpDestruct)(CFrameBuffer* apSelf);
    };

    // @ 0x82964E38 -- seat the vtable, set refcount 1, copy the descriptor's
    // mode/format/stashed word, fix the mix rate at 48000, then bind the sample
    // buffer: an external buffer when the mode byte is set, otherwise one sized
    // from the format and allocated (128-byte-aligned) through `apAllocator`.
    CFrameBuffer(const CFrameBufferDesc* apDesc, IFrameBufferAllocator* apAllocator);

    // @ 0x82964DD8 -- copy the processing state (status/channels word, sample
    // rate, buffer pointer) into `*apOut`. Returns 0 (S_OK).
    s32 GetProcessingData(CFrameBufferProcessingData* apOut);

    // @ 0x82964FA0 -- validate `apFormat` (defaulting to the buffer's own format
    // when null) against the buffer capacity, optionally zero the affected
    // sample range, and record the prepared channel count. Returns 0 on success,
    // 0x8007000E when the request exceeds capacity, or 0x8000FFFF when it would
    // shrink an already-prepared buffer without the reset flag.
    s32 Prepare(const CFrameBufferFormat* apFormat, u8 auFlags);

    // @ 0x82964F48 -- decrement the reference count; at zero dispatch the
    // destructor through vtable slot [3] and return 0, else return the surviving
    // count. (Same body as XAUDIO::CObjectRefCount::Release.)
    s32 Release();

    // @ 0x829651B8 -- re-seat the shared base vtable image (off_82108FF4) into
    // *this and return this. (No free; the asm neither takes a delete flag nor
    // routes to XMemFree.)
    void* ScalarDeletingDestructor();

    const VTable*      mpVTable;            // +0x00
    s32                miRefCount;          // +0x04
    u8                 muUseExternalBuffer; // +0x08
    u8                 mPad09[3];           // +0x09
    CFrameBufferFormat mFormat;             // +0x0C
    u8                 muProcessingStatus;  // +0x14
    u8                 muPreparedChannels;  // +0x15
    u16                muReserved16;        // +0x16
    u32                muSampleRate;        // +0x18
    void*              mpBuffer;            // +0x1C
    u32                muField20;           // +0x20
};

} // namespace XAUDIO
