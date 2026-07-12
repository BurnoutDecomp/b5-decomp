#pragma once

// =====================================================================================
// rw::audio::core::Pan2D1 -- the "Pan2D1" spatial (2D) panner plug-in.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every member offset and every store. The DecFIGS DWARF for this type
// carries only the plug-in registration GUID (pan2d1.h:48, DecoderDesc::Guid =
// 1349399089); there is NO struct layout, NO Feb-2007 leak source, and NO ProStreet08
// rwaudio PDB entry for it, so every offset below is grounded directly in the disassembly
// of the bodied member functions:
//   GetPlugInDescRunTime          @0x82B98748 -> the registered "Pan2D1" descriptor
//   GetSize                       @0x82B982C8 -> 440 (0x1B8)
//   SpeakerConfig                 @0x82B98758 -> build the speaker-direction / inverse-pan matrix
//   EmitterConfig                 @0x82B98900 -> map emitter azimuth/spread to per-source pan targets
//   ComputePerimeterTerm          @0x82B98F78 -> per-source perimeter (edge-sector) gain accumulation
//   ComputeLevels                 @0x82B991D8 -> normalise per-source gain vectors
//   PanOutput                     @0x82B995C8 -> apply the pan matrix to the frame (hard set)
//   RampPanOutput                 @0x82B99340 -> apply with a de-clicked gain ramp
//   Process                       @0x82B997C8 -> per-frame entry (recompute-if-changed + ping-pong)
//   `scalar deleting destructor'  @0x82BA1B00 -> reinstall base vtable, conditional free
//
// (ComputeInteriorTerm @0x82B98xxx lives in a sibling TU; it is declared here and called by
//  ComputeLevels, but its body is reconstructed elsewhere.)
//
// Sibling to the committed rw::audio::core plug-in homes (Gain, Limiter1, the Iir2* filter
// shapes, ...). Like them the PlugIn base sub-object is EMBEDDED by value (composition, not
// inheritance) as a PlugInBaseView at +0x00 so the compiler inserts no second vptr and the
// data offsets are not shifted. The +0xNN annotations are the X360 (32-bit) offsets and are
// documentary only -- on the x64 PC target the PlugInBaseView pointer members widen, so only
// the member ORDER is load-bearing and every access below is by name / by array index.
// GetSize returns the X360 constant 440, not sizeof(Pan2D1).
//
// Lowercase rw::audio:: namespaces match the third-party middleware API (per
// CXX_NAMING_CONVENTIONS: lowercase namespaces are acceptable to match a third-party API).
// =====================================================================================

#include "types.hpp"                     // f32, f64, s32, u32
#include "rw/audio/core/Iir2Filters.h"   // PlugInBaseView, AudioChannelBuffer, AudioProcessContext

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// Pan2D1 -- layout grounded in the disassembly (X360 sizeof 0x1B8 == 440; GetSize is the
// hard-coded 440):
//   +0x00  mBase (PlugInBaseView)  -- installed v-table + graph wiring. +0x20/+0x21 hold the
//                                      input/output channel counts (mbFlag20 / mbChannelCount).
//   +0x28  mfAzimuthDeg            -- emitter graph attributes (8-byte-strided table):
//   +0x30  mfRadius                    azimuth (deg), radius, width, spread (deg),
//   +0x38  mfWidth                      focus, level, centre level.
//   +0x40  mfSpreadDeg
//   +0x48  mfFocus
//   +0x50  mfLevel
//   +0x58  mfCentreLevel
//   +0x60  mfCached[7]             -- packed snapshot of the 7 attributes above (change guard).
//   +0x7C  mfSpeakerAngle0         -- speaker half-angles used to build the pan matrix.
//   +0x80  mfSpeakerAngle1
//   +0x84  mfNormGain              -- per-source normalisation gain.
//   +0x88  mfSpeakerScale          -- 2*cos(mfSpeakerAngle0) (SpeakerConfig).
//   +0x8C  mfPanMatrix[16]         -- four inverse 2x2 speaker-pair matrices (SpeakerConfig).
//   +0xCC  miNumSources            -- active virtual-source count (EmitterConfig switch 1/2/4/5).
//   +0xD0  miNumSpeakers           -- active output-speaker count (2 == stereo fast path).
//   +0xD4  mfGains[25]             -- per-source gain vectors: 5 floats x up to 5 sources.
//   +0x138 mfCentreGain            -- centre/LFE gain (snapshot of mfCentreLevel).
//   +0x13C mSpeakerDir[5]          -- five unit speaker-direction pairs (SpeakerConfig).
//   +0x164 mPanTarget[5]           -- five {x, y, mag, angle} pan targets (EmitterConfig).
// The +0x24../+0x2C../.. gaps are alignment padding preserved so the named members land at
// the observed X360 offsets; only the ORDER is load-bearing (see header note).
// -------------------------------------------------------------------------------------
class Pan2D1
{
public:
    // Plug-in registration id from the DecFIGS DWARF (pan2d1.h:48).
    enum { KU_GUID = 1349399089u };

    // One output speaker's constant-power stereo gain pair. Two single-precision floats;
    // Set() writes cos(theta) to the first and sin(theta) to the second.
    // FLAG (rwaudio PDB reconcile): Pan2D1::Speaker [sizeof=8] confirmed; PDB names the pair
    // x/y (float +0x00, float +0x04); guessed mfGainA/mfGainB renamed to x/y.
    struct Speaker
    {
        f32 x; // +0x00 -- cos(theta)
        f32 y; // +0x04 -- sin(theta)

        // @0x82B986F0 -- set the pair from a pan angle `theta` (radians).
        void Set(f64 theta);
    };

    // One EmitterConfig pan target: a clamped position {x,y} in the speaker plane, its
    // squared magnitude (clamped to 1.0), and its atan2 angle. 16 bytes (stride matches the
    // asm's `16 * index + this` addressing).
    struct PanTarget
    {
        f32 x;     // +0x00
        f32 y;     // +0x04
        f32 mag;   // +0x08 -- squared magnitude, clamped to <= 1.0
        f32 angle; // +0x0C -- atan2(y, x)
    };

    // ---- entry points (asm addresses above) --------------------------------------------
    static char **GetPlugInDescRunTime();                       // @0x82B98748
    static int    GetSize();                                    // @0x82B982C8 -> 440
    static void  *ScalarDeletingDestructor(Pan2D1 *self,
                                           char flags);         // @0x82BA1B00

    int  SpeakerConfig();                                       // @0x82B98758
    int  EmitterConfig();                                       // @0x82B98900
    void ComputePerimeterTerm(int index);                      // @0x82B98F78
    int  ComputeLevels();                                       // @0x82B991D8
    int  PanOutput(AudioChannelBuffer *pDst,
                   AudioChannelBuffer *pSrc);                   // @0x82B995C8
    int  RampPanOutput(AudioChannelBuffer *pDst, AudioChannelBuffer *pSrc,
                       const f32 *pOldGains, f32 fOldCentreGain); // @0x82B99340
    int  Process(AudioProcessContext *ctx, bool bImmediate);   // @0x82B997C8

    // Declared-only: body lives in the sibling ComputeInteriorTerm TU. Called by
    // ComputeLevels (r3=this, r4=source index); its return is discarded.
    int  ComputeInteriorTerm(int index);

    // ---- layout (X360 sizeof 0x1B8 == 440; offsets documentary, order load-bearing) ----
    PlugInBaseView mBase;                     // +0x00 .. +0x23
    char  mPad24[0x28 - 0x24];                // +0x24
    f32   mfAzimuthDeg;                        // +0x28
    char  mPad2C[0x30 - 0x2C];                // +0x2C
    f32   mfRadius;                            // +0x30
    char  mPad34[0x38 - 0x34];                // +0x34
    f32   mfWidth;                             // +0x38
    char  mPad3C[0x40 - 0x3C];                // +0x3C
    f32   mfSpreadDeg;                         // +0x40
    char  mPad44[0x48 - 0x44];                // +0x44
    f32   mfFocus;                             // +0x48
    char  mPad4C[0x50 - 0x4C];                // +0x4C
    f32   mfLevel;                             // +0x50
    char  mPad54[0x58 - 0x54];                // +0x54
    f32   mfCentreLevel;                       // +0x58
    char  mPad5C[0x60 - 0x5C];                // +0x5C
    f32   mfCached[7];                         // +0x60 .. +0x7B -- attribute change-guard snapshot
    f32   mfSpeakerAngle0;                     // +0x7C
    f32   mfSpeakerAngle1;                     // +0x80
    f32   mfNormGain;                          // +0x84
    f32   mfSpeakerScale;                      // +0x88 -- 2*cos(mfSpeakerAngle0)
    f32   mfPanMatrix[16];                     // +0x8C .. +0xCB -- 4 inverse 2x2 speaker-pair matrices
    s32   miNumSources;                        // +0xCC
    s32   miNumSpeakers;                       // +0xD0
    f32   mfGains[25];                         // +0xD4 .. +0x137 -- 5 gains x up to 5 sources
    f32   mfCentreGain;                        // +0x138
    Speaker   mSpeakerDir[5];                  // +0x13C .. +0x163
    PanTarget mPanTarget[5];                   // +0x164 .. +0x1B3
    char  mPad1B4[0x1B8 - 0x1B4];             // +0x1B4 (X360 sizeof 0x1B8 == 440; GetSize hard-coded)
};

} // namespace core
} // namespace audio
} // namespace rw
