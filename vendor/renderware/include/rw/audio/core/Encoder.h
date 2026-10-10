#pragma once

// =====================================================================================
// rw::audio::core::Encoder -- the EARenderWare "rwaudio" stream-encoder base. The encode
// direction of the codec family: where a Decoder turns a codec stream into f32 frames, an
// Encoder takes interleaved f32 frames and emits a codec stream. Three concrete encoders
// derive from it (Pcm16BigEnc 'P6B0', XasEnc 'Xas0', Xas1Enc 'Xas1'); each is created
// through its static CreateInstance, which the encoder registration table names.
//
// Reconstructed from the console image; its PowerPC asm is authoritative for every offset,
// width and side-effect. No source, debug information or PDB entry covers these types, so
// member names are reconstructed from observed behaviour.
//
// V-TABLE (six slots, identical order in all four tables):
//   [0] Encode                 -- pure in the base (the base table holds _purecall)
//   [1] Flush                  -- pure in the base
//   [2] GetDataRateOverhead    -- shared base body
//   [3] VFunc3                 -- shared base body that returns 0; the linker folded it onto
//                                 an unrelated identical body, so no rwaudio name survives
//   [4] Release                -- shared base body
//   [5] the destructor         -- every concrete encoder's deleting destructor reinstalls
//                                 the base table and conditionally frees; ~Encoder and the
//                                 derived destructors have no member teardown
//
// LAYOUT (console byte offsets; the host widens mpSystem, so these describe the console image
// only and every access is by name):
//   +0x00  vptr
//   +0x04  mfDataRate         channels * sample rate * the codec's output bytes per sample
//   +0x08  mfField08          seeded 0.0 by every CreateInstance; no encoder body reads it
//   +0x0C  miField0C          seeded 4000 by every CreateInstance; no encoder body reads it
//   +0x10  mpSystem           the owning System (Release frees through its allocator)
//   +0x14  (8 bytes no encoder body touches)
//   +0x1C  mucChannelCount    interleaved channel count (Encode / Flush / the overhead)
//   console sizeof 0x20 (Pcm16BigEnc's whole allocation)
// Neither CreateInstance writes mpSystem or mucChannelCount: the registry that calls it
// fills them in.
// =====================================================================================

#include "types.hpp" // f32, s32, u8

namespace rw
{
namespace audio
{
namespace core
{

class System;

class Encoder
{
public:
    // vt[0] -- encode up to iNumSamples interleaved frames from pafInput into pOutput.
    // *piBytesWritten receives the bytes emitted; *piAuxOut (optional) is cleared by every
    // encoder. iArg5 is passed through the slot but no encoder reads it. Returns the
    // frames consumed.
    virtual s32 Encode(const f32 *pafInput, void *pOutput, s32 iNumSamples,
                       s32 *piBytesWritten, s32 iArg5, s32 *piAuxOut) = 0;

    // vt[1] -- emit whatever the encoder still buffers. *piBytesWritten receives the bytes
    // emitted, *piAuxOut (optional) is cleared; iArg3 is unread. Returns the frames flushed.
    virtual s32 Flush(void *pOutput, s32 *piBytesWritten, s32 iArg3, s32 *piAuxOut) = 0;

    // vt[2] -- the fixed per-stream overhead: 10240 bytes per channel.
    virtual s32 GetDataRateOverhead();

    // vt[3] -- returns 0 (see the v-table note above).
    virtual s32 VFunc3() { return 0; }

    // vt[4] -- hand this instance back to the owning System's allocator. No destructor
    // runs: the encoders own nothing else.
    virtual void Release();

    // vt[5]
    virtual ~Encoder() {}

    f32     mfDataRate;          // +0x04
    f32     mfField08;           // +0x08
    s32     miField0C;           // +0x0C
    System *mpSystem;            // +0x10
    u8      mGap14[0x1C - 0x14]; // +0x14
    u8      mucChannelCount;     // +0x1C
};

} // namespace core
} // namespace audio
} // namespace rw
