#pragma once

// =====================================================================================
// rw::audio::core::LowPassButterworth -- the "LowPassButterworth" low-pass filter plug-in.
//
// The exact mirror of the committed HighPassButterworth: same layout, same embedded
// cascaded-Butterworth kernel, same recompute guard, same decay rebase. Reconstructed from
// BURNOUT_X360_ARTIST.XEX; decode report
// progress/scratch_dossiers/gainfader_lowpassbutterworth_decode_codex.md, whose divergence
// audit confirms the two are instruction-for-instruction mirrors apart from the points
// listed below.
//   GetPlugInDescRunTime         @0x82B97BF0 -> the registered record (off_82F8D24C, 'LPB0')
//   GetSize                      @0x82B9E4B0 -> Butterworth::GetSize(channels) + header
//   CreateInstance               @0x82BA2FA0
//   Process                      @0x82B97C00
//   `vector deleting destructor' @0x82BA1908; vt[0]/vt[1]/vt[2] are ICF-shared no-ops
//     (its descriptor declares ZERO events, so even vt[1] is the inherited no-op)
//
// THE FOUR DIVERGENCES FROM HighPassButterworth (everything else matches):
//   * the initial live cutoff is 96000.0f, not 0.0f -- a low-pass opens wide at the top of
//     the band, where a high-pass opens wide at the bottom;
//   * the in-band test is `cutoff <= highGuard` rather than `cutoff >= lowGuard`, so the
//     BYPASS side is above the high guard instead of below the low guard;
//   * the clamp pushes values below the LOW guard up to it (the high-pass clamps values
//     above the HIGH guard down);
//   * the coefficient designer's type selector is 0 (low-pass) rather than 1 (high-pass).
// In BOTH, an unordered (NaN) cutoff takes the bypass arm and DOES clear the filter state.
//
// There is no vendor header for this type (the Feb-2007 rwaudiocore plugins/ directory ships
// lowpassfir64.h, a different filter). Names below therefore follow the committed
// HighPassButterworth sibling, which the decode audit cross-checked field for field.
//
// The PlugIn base sub-object is embedded (composition, not inheritance) as a PlugInBaseView
// -- the sibling idiom -- and its pointer members widen on x64, so the +0xNN annotations are
// the X360 offsets and are documentary only. The embedded Butterworth is reached through the
// stored relative offset so it stays self-consistent at host widths.
// =====================================================================================

#include "types.hpp" // f32, s32, u8, u16
#include "rw/audio/core/Iir2Filters.h" // PlugInBaseView / AudioProcessContext
#include "rw/audio/core/Butterworth.h" // Butterworth (embedded by value at the tail)
#include "rw/audio/core/Voice.h"       // VoiceStageConfig (GetSize's config argument)

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// LowPassButterworth -- X360 header span 0x60 == 96, then the embedded Butterworth and its
// trailing work buffers. Layout grounded in CreateInstance @0x82BA2FA0 / Process @0x82B97C00:
//   +0x28  mfCutoffFreq      -- ATTRIBUTE_SETFREQUENCY (init 96000.0)
//   +0x30  mfFilterOrder     -- ATTRIBUTE_SETORDER (init 4.0)
//   +0x38  mfParam3          -- the third shaping attribute (init 1.0)
//   +0x40  mfLastCutoffFreq  -- recompute guard (init 15000.0)
//   +0x48  mfLastFilterOrder -- recompute guard
//   +0x50  mfLastParam3      -- recompute guard; Process READS it but never re-latches it
//                               (faithful omission, same as the high-pass sibling)
//   +0x58  muButterworthOffset
//   +0x60  mButterworth
// -------------------------------------------------------------------------------------
class LowPassButterworth
{
public:
    static char **GetPlugInDescRunTime();                                  // @0x82B97BF0
    static int    GetSize(const VoiceStageConfig *config);                 // @0x82B9E4B0
    static int    CreateInstance(LowPassButterworth *self);                // @0x82BA2FA0
    static int    Process(LowPassButterworth *self, AudioProcessContext *ctx,
                          bool discontinuity);                             // @0x82B97C00
    static void  *VectorDeletingDestructor(LowPassButterworth *self,
                                           char flags);                    // @0x82BA1908

    PlugInBaseView mBase;                     // +0x00 .. +0x23
    char        mPad24[0x28 - 0x24];          // +0x24 .. +0x27
    f32         mfCutoffFreq;                  // +0x28
    char        mPad2C[0x30 - 0x2C];          // +0x2C .. +0x2F
    f32         mfFilterOrder;                 // +0x30
    char        mPad34[0x38 - 0x34];          // +0x34 .. +0x37
    f32         mfParam3;                      // +0x38
    char        mPad3C[0x40 - 0x3C];          // +0x3C .. +0x3F
    f32         mfLastCutoffFreq;              // +0x40
    char        mPad44[0x48 - 0x44];          // +0x44 .. +0x47
    f32         mfLastFilterOrder;             // +0x48
    char        mPad4C[0x50 - 0x4C];          // +0x4C .. +0x4F
    f32         mfLastParam3;                  // +0x50
    char        mPad54[0x58 - 0x54];          // +0x54 .. +0x57
    u16         muButterworthOffset;           // +0x58
    char        mPad5A[0x60 - 0x5A];          // +0x5A .. +0x5F
    Butterworth mButterworth;                  // +0x60 (LAST member; trailing work buffers)
};

} // namespace core
} // namespace audio
} // namespace rw
