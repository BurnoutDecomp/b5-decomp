#pragma once

// =====================================================================================
// rw::audio::core -- the 2nd-order (biquad) IIR filter-shape FAMILY.
//
//   LowPassIir2   HighPassIir2   LowShelfIir2   HighShelfIir2   BandPassIir2
//   PeakingIir2
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm
// is authoritative for every member offset and every coefficient computation. There is
// NO matching Feb-2007 leak source and NO DecFIGS DWARF for these types, so every
// offset and constant below is grounded directly in the disassembly of the bodied
// member functions (see the .cpp siblings of this header).
//
// All six shapes share ONE biquad kernel (rw::audio::core::Iir2, see Iir2.h): each
// holds a 5-coefficient block { a1, a2, b0, b1, b2 } and an array of per-channel
// Iir2State history slots, and each runs the shared Iir2::Filter difference equation
// from its Process() entry. The shapes differ only in (a) which design parameters they
// carry (cutoff, gain, Q), (b) the byte offsets of the coefficient block / state array,
// and (c) the CalculateFilterCoefficients() that maps the parameters to the 5
// coefficients. The coefficient maths is reproduced store-for-store from the asm in the
// per-shape .cpp files; the layouts here are byte-exact so every named member lands at
// the observed offset.
//
// Each shape is a PlugIn (audio processing-graph node, see PlugIn.h). The committed
// PlugIn home declares the family virtuals AND an explicit vtable word at +0x00, so it
// cannot be used as a C++ polymorphic base here without the compiler inserting a second
// hidden vptr and shifting every data field by a word. To keep the data offsets exact
// the shapes embed the PlugIn base fields directly (composition, not inheritance) as a
// PlugInBaseView at +0x00..+0x21; +0x00 is the same vtable word the game installs, the
// live "cutoff" graph attribute lives at mfAttrib1 (+0x18), and the active channel
// count (the per-channel state-loop bound) lives at mbChannelCount (+0x21). Six
// Iir2State slots are reserved per shape (Initialize<T> clears slots 0..5).
// =====================================================================================

#include "types.hpp" // f32, s32, u8, u16
#include "rw/audio/core/Iir2.h"

namespace rw
{
namespace audio
{
namespace core
{

// Six per-channel history slots are reserved by the Initialize<T> helper (it clears
// slots 0..5 at the shape's state offset).
enum { KI_IIR2_MAX_CHANNELS = 6 };

// -------------------------------------------------------------------------------------
// The PlugIn base sub-object, embedded byte-exactly at +0x00 of every filter shape.
// (Mirrors rw::audio::core::PlugIn's data layout; see PlugIn.h. Embedded rather than
// inherited so the shapes' data offsets are not shifted by a compiler-inserted vptr,
// since PlugIn is polymorphic with an explicit +0x00 vtable word.)
// -------------------------------------------------------------------------------------
// rwaudio PDB authoritative names (IDA Files/ProStreet08Milestone.pdb,
// rw::audio::core::PlugIn) for each base slot -- this filter-local view keeps the
// filter-semantic field names (the .cpp bodies use them; the generic base slots are
// repurposed by the filter, e.g. +0x18 holds the cutoff), with the canonical mapping:
//   mpSystem(+0x04)=mpSystemUseGetSystemAccessor  mpInput(+0x08)=mpVoice
//   mpAttributes(+0x0C)=mpAttribute  mpFactory(+0x10)=mpPlugInDescRunTime
//   mfAttrib0(+0x14)=mLatencyInSamples  mfAttrib1(+0x18)=mDecaySamples
//   mState(+0x1C)=mCpuTicks  mbFlag20(+0x20)=mInputChannels
//   mbChannelCount(+0x21)=mOutputChannels  (+0x22/+0x23 = PDB tail padding)
// See PlugIn.h for the canonical reconciled type.
struct PlugInBaseView
{
    void *mpVTable;     // +0x00 -- the installed v-table pointer
    void *mpSystem;     // +0x04
    void *mpInput;      // +0x08
    void *mpAttributes; // +0x0C
    void *mpFactory;    // +0x10
    f32   mfAttrib0;    // +0x14
    f32   mfAttrib1;    // +0x18 -- the live "cutoff" graph attribute
    s32   mState;       // +0x1C
    u8    mbFlag20;     // +0x20
    u8    mbChannelCount;// +0x21 -- active channel count (per-channel state-loop bound)
    u8    mbPad22;      // +0x22
    u8    mbPad23;      // +0x23
};

// -------------------------------------------------------------------------------------
// One source/destination channel buffer descriptor inside the Process context. The
// Process loops read stride (halfword at +0x0E, asm: lhz r,0xE(buf)) and base (+0x04)
// per channel:
//   sample_ptr = base + 4 * stride * channelIndex   (asm: 4 * *(buf+14) * idx + *(buf+4))
// (`*(buf+4)` is a f32* base, so the f32-typed step is base + stride*channelIndex.)
// Only the two fields the filters touch are named; the rest is opaque header so the
// observed offsets hold.
// -------------------------------------------------------------------------------------
struct AudioChannelBuffer
{
    char  mHeader00[0x04];        // +0x00 -- opaque descriptor header
    f32  *mpSamples;              // +0x04 -- base sample pointer
    char  mHeader08[0x0E - 0x08]; // +0x08 .. +0x0D -- opaque
    u16   muStride;               // +0x0E -- per-channel sample stride (halfword)
};

// The audio output format record (reached at ctx dword 49158 == byte +0x30018);
// the filters read its sample rate at +0x0C.
struct AudioFormat
{
    char mHeader00[0x0C]; // +0x00 -- opaque
    f32  mfSampleRate;    // +0x0C -- output sample rate (Hz)
};

// -------------------------------------------------------------------------------------
// The audio Process context passed to every PlugIn::Process (IDA's `a2`). The filters
// touch three deep fields (dword indices 49155 / 49156 / 49158, i.e. byte 0x3000C /
// 0x30010 / 0x30018):
//   +0x3000C  mpSrcBuffer   (the "0" channel-buffer pointer slot; swapped with dst)
//   +0x30010  mpDstBuffer   (the "1" channel-buffer pointer slot)
//   +0x30018  mpFormat      (-> +0x0C = sample rate)
//   +0x30020  mNumSamples   (the frame's active sample count; 0 == a silent frame. Read by
//                             Rechannel::Process @0x82B9A728 as both a has-work guard and
//                             the sample count it forwards to ReChannelGainWrite.)
//   +0x30028  mfResampleGain (a downstream gain the Resample stage scales by its clamped
//                             ratio -- see Resample::PreProcess @0x82B9AC10)
//   +0x3002C  mbChannelCount (the channel count of the currently-active src buffer;
//                             Rechannel::Process latches it and rewrites it to its own
//                             output channel count after remapping/swapping the buffers.)
// The 0x30000 base + opaque body are preserved as a byte span so the named fields land
// exactly. Process swaps the src/dst slots after filtering (ping-pong).
// -------------------------------------------------------------------------------------
struct AudioProcessContext
{
    char                mBody[0x3000C];                 // +0x00 .. +0x3000B -- opaque graph buffers
    AudioChannelBuffer *mpSrcBuffer;                    // +0x3000C
    AudioChannelBuffer *mpDstBuffer;                    // +0x30010
    char                mPad30014[0x30018 - 0x30014];   // +0x30014
    AudioFormat        *mpFormat;                       // +0x30018
    char                mPad3001C[0x30020 - 0x3001C];   // +0x3001C .. +0x3001F
    u32                 mNumSamples;                    // +0x30020 -- active frame sample count
    char                mPad30024[0x30028 - 0x30024];   // +0x30024 .. +0x30027
    f32                 mfResampleGain;                 // +0x30028 -- Resample-scaled gain
    u8                  mbChannelCount;                 // +0x3002C -- active src-buffer channel count
};

// -------------------------------------------------------------------------------------
// Low-pass 2nd-order IIR. Layout grounded in CreateInstance @0x82BA3130, Process
// @0x82B9E5C8, CalculateFilterCoefficients @0x82B97DC0.
//   +0x28  mfCutoffFreq   (Hz)                         state[] +0x30..+0x8F
//   +0x90  mCoeffs (5 f32)                             +0xA4  mfLastCutoffOmega
// -------------------------------------------------------------------------------------
class LowPassIir2
{
public:
    static char  **GetPlugInDescRunTime();                  // @0x82B97DB0
    static LowPassIir2 *CreateInstance(LowPassIir2 *self);  // @0x82BA3130
    void CalculateFilterCoefficients(f64 omega);            // @0x82B97DC0
    int  Process(AudioProcessContext *ctx);                 // @0x82B9E5C8

    PlugInBaseView mBase;                     // +0x00 .. +0x23
    char       mPad24[0x28 - 0x24];           // +0x24 .. +0x27
    f32        mfCutoffFreq;                  // +0x28
    char       mPad2C[0x30 - 0x2C];           // +0x2C .. +0x2F
    Iir2State  mState[KI_IIR2_MAX_CHANNELS];  // +0x30 .. +0x8F
    Iir2Coeffs mCoeffs;                       // +0x90 .. +0xA3
    f32        mfLastCutoffOmega;             // +0xA4
};

// -------------------------------------------------------------------------------------
// High-pass 2nd-order IIR. Same layout as LowPass; CalculateFilterCoefficients
// @0x82B978C0, Process @0x82B9E0A0. (No CreateInstance of its own in the export set.)
// -------------------------------------------------------------------------------------
class HighPassIir2
{
public:
    static char **GetPlugInDescRunTime();                   // @0x82B978B0
    void CalculateFilterCoefficients(f64 omega);            // @0x82B978C0
    int  Process(AudioProcessContext *ctx);                 // @0x82B9E0A0

    PlugInBaseView mBase;                     // +0x00 .. +0x23
    char       mPad24[0x28 - 0x24];           // +0x24 .. +0x27
    f32        mfCutoffFreq;                  // +0x28
    char       mPad2C[0x30 - 0x2C];           // +0x2C .. +0x2F
    Iir2State  mState[KI_IIR2_MAX_CHANNELS];  // +0x30 .. +0x8F
    Iir2Coeffs mCoeffs;                       // +0x90 .. +0xA3
    f32        mfLastCutoffOmega;             // +0xA4
};

// -------------------------------------------------------------------------------------
// Band-pass 2nd-order IIR. Layout grounded in CreateInstance @0x82BA2398, Process
// @0x82B9D778, CalculateFilterCoefficients @0x82B96A50.
//   +0x28 mfFreqLo  +0x30 mfFreqHi          state[] +0x38..+0x97
//   +0x98 mCoeffs (5 f32)   +0xAC mfLastOmegaLo   +0xB0 mfLastOmegaHi
// -------------------------------------------------------------------------------------
class BandPassIir2
{
public:
    static char **GetPlugInDescRunTime();                       // @0x82B96A40
    static BandPassIir2 *CreateInstance(BandPassIir2 *self);    // @0x82BA2398
    void CalculateFilterCoefficients(f64 omegaLo, f64 omegaHi); // @0x82B96A50
    int  Process(AudioProcessContext *ctx);                     // @0x82B9D778

    PlugInBaseView mBase;                     // +0x00 .. +0x23
    char       mPad24[0x28 - 0x24];           // +0x24 .. +0x27
    f32        mfFreqLo;                       // +0x28
    char       mPad2C[0x30 - 0x2C];           // +0x2C .. +0x2F
    f32        mfFreqHi;                       // +0x30
    char       mPad34[0x38 - 0x34];           // +0x34 .. +0x37
    Iir2State  mState[KI_IIR2_MAX_CHANNELS];  // +0x38 .. +0x97
    Iir2Coeffs mCoeffs;                       // +0x98 .. +0xAB
    f32        mfLastOmegaLo;                  // +0xAC
    f32        mfLastOmegaHi;                  // +0xB0
};

// -------------------------------------------------------------------------------------
// Low-shelf 2nd-order IIR. Layout grounded in CreateInstance @0x82BA3198, Process
// @0x82B9E720, CalculateFilterCoefficients @0x82B97E80.
//   +0x28 mfCutoffFreq  +0x30 mfGain        state[] +0x38..+0x97
//   +0x98 mbActive(int)  +0x9C mCoeffs (5 f32)  +0xB0 mfLastOmega  +0xB4 mfLastGain
// -------------------------------------------------------------------------------------
class LowShelfIir2
{
public:
    static char **GetPlugInDescRunTime();                            // @0x82B97E70
    static LowShelfIir2 *CreateInstance(LowShelfIir2 *self);         // @0x82BA3198
    void CalculateFilterCoefficients(f64 omega, f64 gain);           // @0x82B97E80
    int  Process(AudioProcessContext *ctx);                          // @0x82B9E720

    PlugInBaseView mBase;                     // +0x00 .. +0x23
    char       mPad24[0x28 - 0x24];           // +0x24 .. +0x27
    f32        mfCutoffFreq;                   // +0x28
    char       mPad2C[0x30 - 0x2C];           // +0x2C .. +0x2F
    f32        mfGain;                         // +0x30
    char       mPad34[0x38 - 0x34];           // +0x34 .. +0x37
    Iir2State  mState[KI_IIR2_MAX_CHANNELS];  // +0x38 .. +0x97
    s32        mbActive;                       // +0x98
    Iir2Coeffs mCoeffs;                       // +0x9C .. +0xAF
    f32        mfLastOmega;                    // +0xB0
    f32        mfLastGain;                     // +0xB4
};

// -------------------------------------------------------------------------------------
// High-shelf 2nd-order IIR. Same layout as LowShelf; CreateInstance @0x82BA2EA8,
// Process @0x82B9E1F8, CalculateFilterCoefficients @0x82B97988.
// -------------------------------------------------------------------------------------
class HighShelfIir2
{
public:
    static char **GetPlugInDescRunTime();                            // @0x82B97978
    static HighShelfIir2 *CreateInstance(HighShelfIir2 *self);       // @0x82BA2EA8
    void CalculateFilterCoefficients(f64 omega, f64 gain);           // @0x82B97988
    int  Process(AudioProcessContext *ctx);                          // @0x82B9E1F8

    PlugInBaseView mBase;                     // +0x00 .. +0x23
    char       mPad24[0x28 - 0x24];           // +0x24 .. +0x27
    f32        mfCutoffFreq;                   // +0x28
    char       mPad2C[0x30 - 0x2C];           // +0x2C .. +0x2F
    f32        mfGain;                         // +0x30
    char       mPad34[0x38 - 0x34];           // +0x34 .. +0x37
    Iir2State  mState[KI_IIR2_MAX_CHANNELS];  // +0x38 .. +0x97
    s32        mbActive;                       // +0x98
    Iir2Coeffs mCoeffs;                       // +0x9C .. +0xAF
    f32        mfLastOmega;                    // +0xB0
    f32        mfLastGain;                     // +0xB4
};

// -------------------------------------------------------------------------------------
// Peaking (parametric) 2nd-order IIR. Layout grounded in CreateInstance @0x82BA3708,
// Process @0x82B9F168, CalculateFilterCoefficients @0x82B9A470, GetSize @0x82B982D8.
//   +0x28 mfCutoffFreq  +0x30 mfGain  +0x38 mfQ        state[] +0x40..+0x9F
//   +0xA0 mbActive(int)  +0xA4 mCoeffs (5 f32)
//   +0xB8 mfLastOmega  +0xBC mfLastGain  +0xC0 mfLastQ        sizeof = 200 (0xC8)
// -------------------------------------------------------------------------------------
class PeakingIir2
{
public:
    static char **GetPlugInDescRunTime();                             // @0x82B9A460
    static PeakingIir2 *CreateInstance(PeakingIir2 *self);            // @0x82BA3708
    static int  GetSize();                                            // @0x82B982D8 -> 200
    void CalculateFilterCoefficients(f64 omega, f64 gain, f64 q);     // @0x82B9A470
    int  Process(AudioProcessContext *ctx);                           // @0x82B9F168

    PlugInBaseView mBase;                     // +0x00 .. +0x23
    char       mPad24[0x28 - 0x24];           // +0x24 .. +0x27
    f32        mfCutoffFreq;                   // +0x28
    char       mPad2C[0x30 - 0x2C];           // +0x2C .. +0x2F
    f32        mfGain;                         // +0x30
    char       mPad34[0x38 - 0x34];           // +0x34 .. +0x37
    f32        mfQ;                            // +0x38
    char       mPad3C[0x40 - 0x3C];           // +0x3C .. +0x3F
    Iir2State  mState[KI_IIR2_MAX_CHANNELS];  // +0x40 .. +0x9F
    s32        mbActive;                       // +0xA0
    Iir2Coeffs mCoeffs;                       // +0xA4 .. +0xB7
    f32        mfLastOmega;                    // +0xB8
    f32        mfLastGain;                     // +0xBC
    f32        mfLastQ;                        // +0xC0
    char       mPadC4[0xC8 - 0xC4];           // +0xC4 .. +0xC7  (sizeof 0xC8 = 200)
};

} // namespace core
} // namespace audio
} // namespace rw
