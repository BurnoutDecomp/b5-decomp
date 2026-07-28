#pragma once

// =====================================================================================
// Xbox 360 XMA hardware-decoder context HAL (XDK audio layer).
//
// The X360 decodes XMA (the console's native WMA-derived stream format) on a dedicated
// hardware unit driven through per-stream DMA "context" blocks. The XDK exposes the unit
// through the XMA* C API declared here; BURNOUT_X360_ARTIST.XEX imports these bodies from
// the XDK (they are not part of the game image), so on the PC target they are a platform
// layer: only the declarations are needed under the per-TU compile gate, and a PC
// implementation (e.g. a software XMA decoder behind the same API) supplies the bodies at
// link time. Signatures are grounded in the call sites of rw::audio::core::EaXmaDec
// (AllocateResources @0x82B8F580, Reset @0x82B8F8D0, Service @0x82B95EB8, DecodeEvent
// @0x82B96380, DeallocateResources @0x82B93BB0) and the IDA parameter annotations
// (pContext / ppContext / pInit / BlockIfBusy / NumBlocks / OffsetInBits / NumSubframes).
//
// Declared extern "C" in the established platform-API style (cf. XPhysicalAlloc in
// rw/audio/core/System.cpp, GetTickCount in SDKs/XCam/*.h).
// =====================================================================================

#include "types.hpp" // u8, u32, s32

// -------------------------------------------------------------------------------------
// XmaContext -- one hardware decoder context DMA block. Allocated/owned by the XMA layer
// (XMACreateContext hands one back); the game drives it through the API below and never
// lays one out itself. Only the two status bytes that EaXmaDec::DecodeEvent probes
// directly during its hardware-recovery check (@0x82B96714/@0x82B96720: `lbz +0x08` /
// `lbz +0x0C`, testing bit 0x80 of each) are named; the rest of the block is opaque
// hardware state, reached only through the API.
// -------------------------------------------------------------------------------------
struct XmaContext
{
    u8 mauReserved00[8]; // +0x00..+0x07  opaque hardware control words
    u8 mucStatus08;      // +0x08  bit 0x80 probed by the EaXmaDec recovery check
                         //        (input-buffer-0 pending/valid state)
    u8 mauReserved09[3]; // +0x09..+0x0B
    u8 mucStatus0C;      // +0x0C  bit 0x80 probed by the EaXmaDec recovery check
                         //        (input-buffer-1 pending/valid state)
                         // ...the hardware block continues; never instantiated by game code.
};

// -------------------------------------------------------------------------------------
// XmaContextInit -- the context (re)initialisation record passed to XMAInitializeContext.
// Layout grounded in EaXmaDec::Reset @0x82B8F8D0, which zeroes 0x38 bytes on the stack and
// writes exactly these fields (X360 offsets in the comments; the pointer fields widen on
// the x64 host, which is fine -- the record is an API parameter, not game data).
// -------------------------------------------------------------------------------------
struct XmaContextInit
{
    const void *mpInputBuffer0;   // X360 +0x00  first 2048-byte XMA packet buffer
    u32 muInputBuffer0Packets;    //      +0x04  (EaXmaDec passes 1)
    const void *mpInputBuffer1;   //      +0x08  second 2048-byte XMA packet buffer
    u32 muInputBuffer1Packets;    //      +0x0C  (EaXmaDec passes 1)
    u32 muUnknown10;              //      +0x10  (EaXmaDec passes 0x20)
    void *mpOutputBuffer;         //      +0x14  the 6144-byte decoded-PCM ring
    u32 muOutputBufferBlocks;     //      +0x18  (EaXmaDec passes 0x18 = 6144/256)
    void *mpOverlapBuffer;        //      +0x1C  512-byte decoder overlap/scratch area
    u32 muUnknown20;              //      +0x20  (EaXmaDec passes 4)
    u32 muChannelsMinusOne;       //      +0x24  0 = mono, 1 = stereo pair
    u32 muStreamMode;             //      +0x28  low 2 bits of the stream's leading word
    u32 mauReserved2C[3];         //      +0x2C..+0x37  left zeroed by EaXmaDec::Reset
};

extern "C"
{
// Context pool management.
s32 XMACreateContext(XmaContext **ppContext);
void XMAReleaseContext(XmaContext *pContext);
s32 XMAInitializeContext(XmaContext *pContext, const XmaContextInit *pInit);

// Start/stop. BlockIfBusy: EaXmaDec always passes 1 (wait for the unit to quiesce).
s32 XMAEnableContext(XmaContext *pContext);
s32 XMADisableContext(XmaContext *pContext, s32 iBlockIfBusy);

// Decoded-output ring state. Offsets are in the hardware's output units (the write offset
// is scaled by << 8 to bytes at the EaXmaDec call site; the read offset is set from the
// value DecodeEvent derives -- IDA names the parameter NumSubframes).
u32 XMAGetOutputBufferWriteOffset(XmaContext *pContext);
u32 XMAGetOutputBufferReadOffset(XmaContext *pContext);
s32 XMASetOutputBufferReadOffset(XmaContext *pContext, u32 uReadOffset);
s32 XMAIsOutputBufferValid(XmaContext *pContext);
s32 XMASetOutputBufferValid(XmaContext *pContext);

// Encoded-input double buffer (two 2048-byte packet buffers, fed alternately).
s32 XMASetInputBuffer0(XmaContext *pContext, const void *pBuffer, u32 uNumBlocks);
s32 XMASetInputBuffer1(XmaContext *pContext, const void *pBuffer, u32 uNumBlocks);
s32 XMAIsInputBuffer0Valid(XmaContext *pContext);
s32 XMAIsInputBuffer1Valid(XmaContext *pContext);
s32 XMASetInputBuffer0Valid(XmaContext *pContext);
s32 XMASetInputBuffer1Valid(XmaContext *pContext);
s32 XMASetInputBufferReadOffset(XmaContext *pContext, u32 uOffsetInBits);
}
