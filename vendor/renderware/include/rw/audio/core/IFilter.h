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

struct IFilter
{
    // The audio PDB's inline constructor clears these four slots. ARTIST
    // folds that initialization into the concrete section constructors.
    IFilter()
        : mpApplyFunc(0), mpResetFunc(0), miReadLength(0), miChannels(0)
    {
    }

    enum FilterOutputMode
    {
        OVERWRITE = 0,
        ADD = 1
    };

    // The audio PDB declares this record. ARTIST passes its address in r7
    // at the DelayLine dispatch at 0x82B6E5D0.
    struct LocationParams
    {
        f32 *pInput;
        f32 *pDelayFull;
        f32 *pDelayCross;
        f32 *pCrossFadeCoeff;
        f32 *pDelayZero;
        f32 *pOutput;
    };

    void (*mpApplyFunc)(IFilter *filter, s32 count, FilterOutputMode mode,
                        s32 channel, LocationParams *locations);
    void (*mpResetFunc)(IFilter *filter);
    s32 miReadLength; // +0x08 on X360
    s32 miChannels;   // +0x0C on X360
};

} // namespace core
} // namespace audio
} // namespace rw
