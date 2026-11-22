#pragma once

// =====================================================================================
// rw::audio::core::DelayFilter -- the feedback delay-line filter used by the
// rw::audio::core::Delay audio plug-in (Delay::Delay constructs it, Delay::Process
// re-tunes it via SetFeedback). It shares the IFilter base and the apply/reset
// interface with AllPassFilter and CombFilter.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm
// is authoritative for every member offset. No LegacySrc reference source and no DecFIGS DWARF
// exist for this type, so each offset below is grounded directly in the disassembly of
// the bodied members:
//   DelayFilter::DelayFilter          @0x82B67D10 -- store-for-store
//   DelayFilter::DelayFilterApplyFunc @0x82B67DD0
//   DelayFilter::DelayFilterResetFunc @0x82B67E88
//   DelayFilter::SetFeedback          @0x82B67EA0
//
// Lowercase rw::audio:: namespaces match the third-party middleware API (per
// CXX_NAMING_CONVENTIONS: lowercase namespaces are acceptable to match a third-party
// API). Member NAMES are reconstructed to match observed semantics.
//
// The Xbox 360 audio PDB declares IFilter as the base. ARTIST at 0x82B67D10
// confirms the base's four fields followed by two feedback values.
// =====================================================================================

#include "types.hpp" // f32, s32
#include "rw/audio/core/IFilter.h"

namespace rw
{
namespace audio
{
namespace core
{

class DelayFilter : public IFilter
{
public:
    static DelayFilter *DelayFilter_ctor(DelayFilter *self);                // @0x82B67D10
    static void DelayFilterApplyFunc(IFilter *filter, s32 count, FilterOutputMode mode,
                                     s32 channel, LocationParams *ctx);      // @0x82B67DD0
    static void DelayFilterResetFunc(IFilter *filter);                      // @0x82B67E88
    static DelayFilter *SetFeedback(DelayFilter *self, f32 feedback);       // @0x82B67EA0

    f32 mfFeedback;     // +0x10
    f32 mfPrevFeedback; // +0x14
};

// -------------------------------------------------------------------------------------
// ARTIST 82B67D40: r6 base, r7 primary, r8 secondary, r9 weight,
// r10 accumulation destination and stack argument feedforward destination.
void DelayFilterCrossFadeFunc(s32 count, f32 feedback, f32 prevFeedback,
                             f32 *base, f32 *primary, f32 *secondary,
                             f32 *weight, f32 *accum, f32 *feedforward);

} // namespace core
} // namespace audio
} // namespace rw
