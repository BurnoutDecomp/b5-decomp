#pragma once

// =====================================================================================
// rw::audio::core::Pan2D -- the surround "Pan2D" spatial panner plug-in.
//
// A PlugIn processing-graph node (sibling of Gain / Send / Rechannel / Resample and the
// Iir2* filter shapes) that pans a mono input around a 2..6-speaker output ring from an
// azimuth angle plus a "radius" (point-source-vs-omnidirectional) blend, de-clicking level
// changes with a 64-sample gain ramp via rw::audio::core::CopyWithGainRamp. Distinct from
// the simpler Pan2D1 stereo panner in this same directory.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm is
// authoritative for every member offset. There is NO Feb-2007 leak source, NO DecFIGS DWARF,
// and NO ProStreet08 rwaudio PDB entry for this TU, so each offset below is grounded directly
// in the disassembly of the bodied member functions:
//   GetSize                 @0x82B982C0 -> 240 (0xF0)
//   GetPlugInDescRunTime    @0x82B984E8 -> the registered "Pan2D" descriptor
//   ComputeTransform        @0x82B984F8
//   SpeakerConfig           @0x82B985D0
//   ComputeZeroRadiusLevels @0x82B99930
//   ComputeLevels           @0x82B999B0
//   RampPanOutput           @0x82B99D18
//   Process                 @0x82B99ED8
//   CreateInstance          @0x82BA34A0
//   `vector deleting destructor' @0x82BA1AB8
//
// Lowercase rw::audio:: namespaces match the third-party middleware API. The PlugIn base
// sub-object is embedded (composition, not inheritance) as a PlugInBaseView so the data
// offsets are not shifted by a compiler-inserted vptr -- the same idiom Gain / the Iir2*
// filter shapes / Resample use (see Iir2Filters.h). +0x00 is the installed v-table word,
// mBase.mpAttributes (+0x0C) is pointed at the first input attribute (mfAzimuth @+0x28) so
// the base SetAttribute writes land there, and the active output channel count (the panner's
// per-channel bound) lives in mBase.mbChannelCount (+0x21).
// =====================================================================================

#include "types.hpp" // f32, f64, u8, u16, s32
#include "rw/audio/core/Iir2Filters.h" // PlugInBaseView / AudioProcessContext / AudioChannelBuffer

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// Pan2D -- byte-exact layout (sizeof 0xF0 == 240, GetSize @0x82B982C0).
//
// Input attributes (+0x28..+0x4C) are an 8-byte-stride attribute table (the base
// mpAttributes points at &mfAzimuth); the cached "last applied" snapshot (+0x50..+0x60)
// is a tightly packed f32[5] compared each Process to decide the fast (levels unchanged)
// vs. slow (recompute + ramp) path. The speaker geometry (+0x64..+0x78), the four 2x2
// per-sector rotation matrices (+0x7C..+0xBB, written by ComputeTransform), the
// omnidirectional "zero radius" per-channel levels (+0xBC.., written by
// ComputeZeroRadiusLevels) and the final per-output-channel levels (+0xD4.., written by
// ComputeLevels) complete the record.
// -------------------------------------------------------------------------------------
class Pan2D
{
public:
    static int    GetSize();                                                 // @0x82B982C0 -> 240
    static char **GetPlugInDescRunTime();                                    // @0x82B984E8
    static int    CreateInstance(Pan2D *self, const f32 *pConfig);           // @0x82BA34A0
    static int    SpeakerConfig(Pan2D *self);                                // @0x82B985D0
    static void   ComputeTransform(Pan2D *self, int sector,
                                   f64 angleA, f64 angleB);                  // @0x82B984F8
    static Pan2D *ComputeZeroRadiusLevels(Pan2D *self);                      // @0x82B99930
    static void   ComputeLevels(Pan2D *self);                               // @0x82B999B0
    static int    RampPanOutput(Pan2D *self, AudioChannelBuffer *dstBuffer,
                                AudioChannelBuffer *srcBuffer,
                                const f32 *pOldLevel);                       // @0x82B99D18
    static int    Process(Pan2D *self, AudioProcessContext *ctx,
                          char recompute);                                   // @0x82B99ED8
    static void  *VectorDeletingDestructor(Pan2D *self, char flags);         // @0x82BA1AB8

    PlugInBaseView mBase;                     // +0x00 .. +0x23 (mbChannelCount @+0x21)
    char       mPad24[0x28 - 0x24];           // +0x24 .. +0x27
    // ---- input attributes (8-byte stride attribute table; base = &mfAzimuth) ----
    f32        mfAzimuth;                      // +0x28 -- pan azimuth (degrees)
    char       mPad2C[0x30 - 0x2C];           // +0x2C
    f32        mfRadius;                       // +0x30 -- point-source vs omni blend (0..1)
    char       mPad34[0x38 - 0x34];           // +0x34
    f32        mfSpread;                       // +0x38 -- centre/divergence spread (6-ch fold)
    char       mPad3C[0x40 - 0x3C];           // +0x3C
    f32        mfGain;                         // +0x40 -- master output level
    char       mPad44[0x48 - 0x44];           // +0x44
    f32        mfLfeSend;                      // +0x48 -- direct LFE/6th-channel level
    char       mPad4C[0x50 - 0x4C];           // +0x4C
    // ---- cached last-applied input snapshot (packed f32[5]) ----
    f32        mfLastAzimuthRad;              // +0x50 -- last azimuth (radians)
    f32        mfLastRadius;                  // +0x54
    f32        mfLastSpread;                  // +0x58
    f32        mfLastGain;                    // +0x5C
    f32        mfLastLfeSend;                 // +0x60
    // ---- speaker geometry ----
    f32        mfConfigAzimuth;              // +0x64 -- configured front half-angle (degrees)
    f32        mfConfigSpread;               // +0x68 -- configured rear half-angle (degrees)
    f32        mfSpeakerAngle;               // +0x6C -- front half-angle (radians)
    f32        mfSpeakerAngle2;              // +0x70 -- rear half-angle (radians)
    f32        mfFrontWidth;                 // +0x74 -- 2*cos(mfSpeakerAngle)
    f32        mfRearWidth;                  // +0x78 -- 2*cos(PI - mfSpeakerAngle2)
    // ---- four 2x2 per-sector rotation matrices (ComputeTransform) ----
    f32        mfSectorMatrix[4][4];         // +0x7C .. +0xBB
    // ---- omnidirectional per-channel levels (ComputeZeroRadiusLevels; 5 spatial) ----
    f32        mfZeroRadiusLevel[5];         // +0xBC .. +0xCF
    char       mPadD0[0xD4 - 0xD0];          // +0xD0 (unused slot)
    // ---- final per-output-channel levels (ComputeLevels; [0..4] spatial, [5] LFE) ----
    f32        mfLevel[6];                    // +0xD4 .. +0xEB
    u8         mbHasConfig;                   // +0xEC -- an explicit speaker config was supplied
    char       mPadED[0xF0 - 0xED];           // +0xED (X360 sizeof 0xF0 == 240; GetSize is hard-coded)
};

// NB: the +0xNN annotations above are the X360 (32-bit-pointer) offsets from the ARTIST asm
// and are documentary only -- on the x64 PC compile the PlugInBaseView pointer members widen,
// so only member ORDER is load-bearing and every access is by name (the same idiom Gain.h /
// Iir2Filters.h use). GetSize returns the X360 constant 240, not sizeof(Pan2D).

} // namespace core
} // namespace audio
} // namespace rw
