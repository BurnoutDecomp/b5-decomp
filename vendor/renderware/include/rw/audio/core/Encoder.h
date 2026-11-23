#pragma once

// =====================================================================================
// rw::audio::core::Encoder -- the EARenderWare "rwaudio" stream-encoder base. The encode
// direction of the codec family: where a Decoder turns a codec stream into f32 frames, an
// Encoder takes interleaved f32 frames and emits a codec stream. Three concrete encoders
// derive from it (Pcm16BigEnc 'P6B0', XasEnc 'Xas0', Xas1Enc 'Xas1'); each is created
// through its static CreateInstance, which the encoder registration table names.
//
// The ARTIST vtables establish the slot order. The Xbox 360 RenderWare PDB supplies the
// member types and signatures.
//
// V-TABLE (six slots, identical order in all four tables):
//   [0] Encode                 -- pure in the base (the base table holds _purecall)
//   [1] Flush                  -- pure in the base
//   [2] GetDataRateOverhead    -- shared base body
//   [3] GetSeekMemoryRequired  -- shared zero-return body at 0x82B0F1B8
//   [4] Release                -- shared base body
//   [5] the destructor         -- every concrete encoder's deleting destructor reinstalls
//                                 the base table and conditionally frees; ~Encoder and the
//                                 derived destructors have no member teardown
//
// The native instance is 0x20 bytes. The registry supplies the owning System and channel
// count when it instantiates an encoder.
// =====================================================================================

#include "types.hpp" // f32, s32, u8

namespace rw
{
namespace audio
{
namespace core
{

class System;
struct EncoderDesc;

class Encoder
{
public:
    // vt[0] -- encode up to iNumSamples interleaved frames from pafInput into pOutput.
    // piBytesWritten and piAuxOut receive the encoded byte count and seek-data byte count.
    // Returns the frames consumed.
    virtual s32 Encode(f32 *pafInput, u8 *pOutput, s32 iNumSamples,
                       s32 *piBytesWritten, void *pSeekData, s32 *piAuxOut) = 0;

    // vt[1] -- emit buffered frames and return the number flushed.
    virtual s32 Flush(u8 *pOutput, s32 *piBytesWritten, void *pSeekData, s32 *piAuxOut) = 0;

    // vt[2] -- the fixed per-stream overhead: 10240 bytes per channel.
    virtual s32 GetDataRateOverhead();

    // vt[3] -- returns 0 (see the v-table note above).
    virtual s32 GetSeekMemoryRequired(s32 /*numFrames*/) { return 0; }

    // vt[4] -- hand this instance back to the owning System's allocator. No destructor
    // runs: the encoders own nothing else.
    virtual void Release();

    // vt[5]
    virtual ~Encoder() {}

    f32          mAverageDataRate;    // +0x04
    f32          mVbrQuality;        // +0x08
    s32          mCbrRate;           // +0x0C
    System      *mpSystem;           // +0x10
    EncoderDesc *mpEncoderDesc;      // +0x14
    s32          mSampleRate;        // +0x18
    u8           mNumChannels;       // +0x1C
    u8           mBitRateManagement; // +0x1D
};

} // namespace core
} // namespace audio
} // namespace rw
