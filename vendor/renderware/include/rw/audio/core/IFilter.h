#pragma once

// =====================================================================================
// rw::audio::core::IFilter -- the base unit-filter interface of the rwaudio reverb/delay
// filter family. The concrete sections AllPassFilter / CombFilter (ReverbFilters.h) and
// DelayFilter (DelayFilter.h) are its derived unit filters; the ProStreet-08 X360 rwaudio
// PDB records the shared base layout as:
//     class rw::audio::core::IFilter [sizeof = 16] {
//       +0x00 mpApplyFunc   (function pointer)
//       +0x04 mpResetFunc   (function pointer)
//       +0x08 mReadLength   (int)
//       +0x0C mChannels     (int)
//     }
//
// rw::audio::core::DelayLine::ApplyFilter installs one of these via SetFilter and, per
// processing chunk, dispatches the apply function pointer at +0x00 (the X360 asm does
//   r3 = this->mpFilter;  ctr = *(r3 + 0);  bctrl
// i.e. call the first word as a function). Only that word is touched by DelayLine, and
// only ever through the mpFilter POINTER -- so modelling this base with x64 pointer widths
// (which shifts mReadLength/mChannels off their 4-byte X360 offsets) is safe here: the
// member ORDER is all that is load-bearing for the by-pointer apply dispatch.
//
// Lowercase rw::audio:: namespaces match the third-party middleware API (per
// CXX_NAMING_CONVENTIONS: lowercase namespaces are acceptable to match a third-party API).
// =====================================================================================

#include "types.hpp" // s32

namespace rw
{
namespace audio
{
namespace core
{

struct IFilter;

// The apply / reset processing function-pointer signatures an IFilter installs. The apply
// func processes `count` samples of channel `channel` through the per-tap context `ctx`
// (built by the caller, e.g. DelayLine::ApplyFilter); `src` is a caller-supplied
// pass-through argument. Grounded in the DelayLine::ApplyFilter dispatch site
// (@0x82B6E5D0: r4=count, r5=src, r6=channel, r7=&context) and the DelayFilter apply
// prototype (DelayFilter.h). The context is opaque at this interface (void*) so IFilter
// stays decoupled from any one filter's context shape.
typedef void   *(*IFilterApplyFunc)(IFilter *pSelf, s32 count, s32 src, s32 channel,
                                    void *pContext);
typedef IFilter *(*IFilterResetFunc)(IFilter *pSelf);

struct IFilter
{
    IFilterApplyFunc mpApplyFunc;  // +0x00 -- per-chunk processing function
    IFilterResetFunc mpResetFunc;  // +0x04 -- reset-state function
    s32              miReadLength; // +0x08 -- tap read length
    s32              miChannels;   // +0x0C -- channel count
};

} // namespace core
} // namespace audio
} // namespace rw
