// =====================================================================================
// rw::audio::core::EaXmaDec bodies (partial).
//
// EARenderWare "rwaudio" XMA (Xbox 360 hardware-format) stream decoder plug-in.
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative for every
// offset, width and side-effect. No Feb-2007 source and no DecFIGS DWARF exist for this TU.
//
// This file homes ONLY the functions that can be grounded without the (un-recovered) XMA
// hardware-context layout, the shared XMA-context free-list pool, and the codec's appended
// per-instance decode state:
//   GetSize          @0x82B93C78
//   FeedEvent        @0x82B96140
//   LeftShiftInBits  @0x82B8FB20
//   ParseToNextFrame @0x82B8FB88
//   XMAmemcpy        @0x82B8FA18
// The decode-driving members (AllocateResources, CreateInstanceEvent, DecodeEvent, Service,
// Reset, StartPair, ReleaseEvent, EnterRecoveryMode, GetDecoderDesc) reach the XMA hardware
// intrinsics (XMACreateContext / XMAInitializeContext / XMA*OutputBuffer* / ...), the shared
// context free-list pool, and the un-recovered instance layout by byte offset; they are left
// for a future slice.
//
// The `_savegprlr_* / _restgprlr_*` calls in the pseudocode are the compiler's register
// save/restore prologue/epilogue helpers -- not source-level calls -- so they are dropped.
// =====================================================================================

#include "rw/audio/core/EaXmaDec.h"

#include <cstring> // memcpy

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// GetSize @0x82B93C78
// Report the codec's per-instance allocation footprint. The XMA decoder needs one 28-byte
// (0x1C) context-pair record per stereo pair -- ceil(channels/2) of them -- on top of an
// 88-byte (0x58) fixed header. The required alignment (16) is written through the out-param.
// The descriptor framework passes the channel count in the first register.
// -------------------------------------------------------------------------------------
u32 EaXmaDec::GetSize(u32 uNumChannels, u32 *puAlignment)
{
    *puAlignment = 16;
    return 28 * ((uNumChannels + 1) >> 1) + 88;
}

// -------------------------------------------------------------------------------------
// FeedEvent @0x82B96140
// "Feed more input" callback: a straight tail-call onto the internal Service pump (the asm
// is a bare `b Service`, so `this` passes straight through).
// -------------------------------------------------------------------------------------
s32 EaXmaDec::FeedEvent()
{
    return Service();
}

// -------------------------------------------------------------------------------------
// LeftShiftInBits @0x82B8FB20
// Extract `iNumBits` big-endian bits beginning at bit position `iBitPos` from the packed
// byte stream `pStream`, and shift them into the low end of the 32-bit accumulator, after
// shifting the accumulator's prior contents up by `iNumBits`. `pStream` is external packed
// XMA data, so it is walked as a raw byte buffer. No-op when iNumBits <= 0.
//
// A 24-bit big-endian window (three bytes starting at iBitPos/8) is assembled into the top
// of a 32-bit word, shifted left by the intra-byte bit offset (iBitPos % 8), then shifted
// down by (32 - iNumBits) so the requested field lands in the low bits.
// -------------------------------------------------------------------------------------
u32 *EaXmaDec::LeftShiftInBits(u32 *puAccum, const u8 *pStream, s32 iBitPos, s32 iNumBits)
{
    if (iNumBits > 0)
    {
        const u8 *pByte = pStream + iBitPos / 8;
        u32 uWindow = (static_cast<u32>(pByte[0]) << 24) |
                      (static_cast<u32>(pByte[1]) << 16) |
                      (static_cast<u32>(pByte[2]) << 8);
        u32 uBits = (uWindow << (iBitPos % 8)) >> (32 - iNumBits);
        *puAccum = uBits | (*puAccum << iNumBits);
    }
    return puAccum;
}

// -------------------------------------------------------------------------------------
// ParseToNextFrame @0x82B8FB88
// Advance a (byte-cursor, bit-offset) pair across one XMA frame. The frame-length field is
// decoded out of the bitstream with two LeftShiftInBits reads (split around the packet's
// 15-bit header budget when the offset is near the packet tail, `iBitPos > 0x3FD1`), then
// added to the running bit offset. When the offset crosses the 0x3FE0-bit packet boundary,
// the byte cursor wraps onto the next 2048-byte (0x800) XMA packet and the offset rolls back
// by one packet's worth of bits (0x3FE0).
//
// The X360 code leaves the LeftShiftInBits accumulator pointer in r3 on return, but every
// caller ignores it, so the source shape is a void frame-walk.
// -------------------------------------------------------------------------------------
void EaXmaDec::ParseToNextFrame(const u8 **ppCursor, s32 *piBitOffset)
{
    s32 iBitOffset = *piBitOffset;
    s32 iTailBits = 0;
    if (iBitOffset > 0x3FD1)
        iTailBits = iBitOffset - 0x3FD1;

    const u8 *pStream = *ppCursor;
    u32 uFrameLen = 0;
    LeftShiftInBits(&uFrameLen, pStream, iBitOffset + 32, 15 - iTailBits);
    LeftShiftInBits(&uFrameLen, pStream, iBitOffset + (15 - iTailBits) + 64, iTailBits);

    s32 iNext = iBitOffset + static_cast<s32>(uFrameLen);
    if (static_cast<u32>(iNext) >= 0x3FE0u)
    {
        *ppCursor = pStream + 2048;
        iNext = *piBitOffset + static_cast<s32>(uFrameLen) - 0x3FE0;
    }
    *piBitOffset = iNext;
}

// -------------------------------------------------------------------------------------
// XMAmemcpy @0x82B8FA18
// Copy `iSize` bytes from `pSrc` to `pDst` (topping up the XMA hardware input buffers). The
// X360 body is a hand-unrolled VMX unaligned block copy (lvlx/lvrx -> stvx128 over 128-byte
// blocks) with a scalar memcpy tail; semantically a plain byte copy. A member, but the asm
// overwrites r3 (this) with pDst before use, so the instance is unreferenced. Returns pDst.
// -------------------------------------------------------------------------------------
void *EaXmaDec::XMAmemcpy(void *pDst, const void *pSrc, s32 iSize)
{
    return std::memcpy(pDst, pSrc, static_cast<usize>(iSize));
}

} // namespace core
} // namespace audio
} // namespace rw
