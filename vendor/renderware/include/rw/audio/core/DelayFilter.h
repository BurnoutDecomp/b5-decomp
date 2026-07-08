#pragma once

// =====================================================================================
// rw::audio::core::DelayFilter -- the feedback delay-line filter used by the
// rw::audio::core::Delay audio plug-in (Delay::Delay constructs it, Delay::Process
// re-tunes it via SetFeedback). It is a unit filter of the same family as the reverb
// AllPassFilter / CombFilter sections (see ReverbFilters.h): a flat record of four
// leading ints (the IFilter book-keeping slots) followed by two floats, driven by a
// C-style apply/reset function pair the owning Delay installs.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm
// is authoritative for every member offset. No Feb-2007 leak source and no DecFIGS DWARF
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
// FLAG (rwaudio family reconcile): the sibling reverb sections in this family are named
// in the ProStreet-08 X360 PDB as deriving from rw::audio::core::IFilter, whose first two
// slots are the mpApplyFunc / mpResetFunc function pointers and whose next two ints are
// mReadLength / mChannels. DelayFilter shares that exact +0x00..+0x0F shape (four ints,
// miMode==mReadLength). As with AllPassFilter / CombFilter (see ReverbFilters.h) we keep
// the flat-int ARTIST layout rather than modelling an x64 IFilter base -- an x64 base of
// two real pointers would break the 4-byte X360 widths these offsets depend on.
// =====================================================================================

#include "types.hpp" // f32, s32

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// The per-call context the apply func is handed (the `a5` struct), byte-identical to the
// reverb filters' AllPassFilter::Context: the delay-line base, the "work" feedback buffer
// the kernel iterates, the active-crossfade flag tested at +0x08, and the accumulate /
// feedforward output streams. Each of mpBase / mpAccum / mpFeedforward is addressed as a
// constant byte delta from mpFeedback in the asm; element form is equivalent.
// -------------------------------------------------------------------------------------
struct DelayFilterContext
{
    f32 *mpBase;        // +0x00 -- delay-line base (kernel `base`)
    f32 *mpFeedback;    // +0x04 -- the "work" buffer the apply loop iterates
    s32  miInactive;    // +0x08 -- when nonzero the filter takes the crossfade path
    s32  miField0C;     // +0x0C -- forwarded to the crossfade helper
    f32 *mpAccum;       // +0x10 -- accumulate buffer (the delayed-mix destination)
    f32 *mpFeedforward; // +0x14 -- feedforward / delay-write output buffer
};

// -------------------------------------------------------------------------------------
// DelayFilter -- layout grounded in DelayFilter::DelayFilter @0x82B67D10:
//   +0x00  miField00       (int,  init 0)
//   +0x04  miField04       (int,  init 0)
//   +0x08  miMode          (int,  init 1; the IFilter read-length / tap descriptor)
//   +0x0C  miField0C       (int,  init 0)
//   +0x10  mfFeedback      (f32,  init 0.0; the live feedback gain, ramp target)
//   +0x14  mfPrevFeedback  (f32,  init 0.0; the feedback the previous frame used --
//                                 SetFeedback latches the old value here so the apply
//                                 func can cross-fade from it when the gain changes)
// (sizeof == 0x18.)
// -------------------------------------------------------------------------------------
class DelayFilter
{
public:
    typedef DelayFilterContext Context;

    static DelayFilter *DelayFilter_ctor(DelayFilter *self);                // @0x82B67D10
    static void        *DelayFilterApplyFunc(DelayFilter *self, s32 count,
                                             s32 src, f32 *extra,
                                             Context *ctx);                  // @0x82B67DD0
    static DelayFilter *DelayFilterResetFunc(DelayFilter *self);            // @0x82B67E88
    static DelayFilter *SetFeedback(DelayFilter *self, f32 feedback);       // @0x82B67EA0

    s32 miField00;      // +0x00
    s32 miField04;      // +0x04
    s32 miMode;         // +0x08
    s32 miField0C;      // +0x0C
    f32 mfFeedback;     // +0x10
    f32 mfPrevFeedback; // +0x14
};

// -------------------------------------------------------------------------------------
// The crossfade recurrence the apply func tail-calls when ctx->miInactive is set (a
// feedback change is in flight). It has its own X360 address and is a separate TU; only
// its declaration is needed here so the two share ONE signature. The apply func unpacks
// the context into the argument registers exactly as below (r3=count, f1=feedback,
// f2=prevFeedback, r6=base, r7=feedbackBuf, r8=crossFadeFlag [the reused miInactive],
// r9=field0C, r10=accum, stack=feedforward).
// -------------------------------------------------------------------------------------
void *DelayFilterCrossFadeFunc(s32 count, f32 feedback, f32 prevFeedback,
                               f32 *base, f32 *feedbackBuf, s32 crossFadeFlag,
                               s32 field0C, f32 *accum, f32 *feedforward);

} // namespace core
} // namespace audio
} // namespace rw
