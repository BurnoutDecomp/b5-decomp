#pragma once

// =====================================================================================
// rw::audio::core reverb building-block filters:
//   AllPassFilter -- Schroeder all-pass section (delay-line feedforward/feedback)
//   CombFilter    -- feedback comb section with a one-pole low-pass in the loop
//
// These are the unit filters a ReverbModel1 chains together (each is constructed and
// gain-configured from ReverbModel1::ReverbModel1 / ConfigureModel). EARenderWare
// "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm is
// authoritative for every member offset. No Feb-2007 leak source and no DecFIGS DWARF
// exist for these types, so each offset below is grounded directly in the disassembly
// of the bodied members (see ReverbFilters.cpp).
//
// Lowercase rw::audio:: namespaces match the third-party middleware API (per
// CXX_NAMING_CONVENTIONS: lowercase namespaces are acceptable to match a third-party
// API). Member NAMES are reconstructed to match observed semantics.
// =====================================================================================

#include "types.hpp" // f32, s32

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// The all-pass / comb processing kernel shared by AllPassFilter::AllPassFilterApplyFunc.
// It has its own X360 address (@0x82B64560) but no standalone TU; it is the recurrence
// body the apply-func tail-calls, so it is homed here alongside its only caller family.
//
// `base`/`work`/`feedforwardA`/`feedforwardB` are the sample streams the apply func
// threads (each a constant byte delta from `work`). `count` is the per-call sample
// count, `g1`/`g2` the all-pass gains, `lowPass` selects the second (comb-style, +=
// accumulating) variant. The +1e-18/-1e-18 round-trip is the engine's denormal-flush
// bias (flt_8214B108), exactly as in Iir2::Filter.
//
// The IDA prototype is (count, g1, g2, a4, a5, base, work, forwardA, forwardB, lowPass);
// a4/a5 are dead register leftovers the kernel never reads, so they are omitted here.
// -------------------------------------------------------------------------------------
void AllPassFilterFunc(s32 count, f32 g1, f32 g2, f32 *base, f32 *work,
                       f32 *feedforwardA, f32 *feedforwardB, s32 lowPass);

// -------------------------------------------------------------------------------------
// AllPassFilter -- one all-pass reverb section.
//
// FLAG (rwaudio PDB reconcile -- DIVERGE, layout kept): the ProStreet-08 X360 PDB names
// this type and reveals it derives from rw::audio::core::IFilter:
//     class rw::audio::core::AllPassFilter [sizeof = 24] : public IFilter {
//       base +0x00 [16] IFilter { mpApplyFunc(+0x00), mpResetFunc(+0x04),
//                                  mReadLength(+0x08 int), mChannels(+0x0C int) }
//       data +0x10 [4] float mG
//       data +0x14 [4] float mScaleFactor }
// sizeof MATCHES (0x18), and the two trailing floats align 1:1 (mfGain1==mG @+0x10,
// mfGain2==mScaleFactor @+0x14). The +0x00..+0x0F region DIVERGES structurally: the PDB
// has an IFilter base (the first two slots are FUNCTION POINTERS, not the ints this
// ARTIST reconstruction guessed), and IFilter's members belong to a separate (not-yet-
// modeled) base type. Per reconcile rules we keep the ARTIST flat-int layout untouched
// (modeling the IFilter base as real x64 pointers would also break the 4-byte X360
// widths this struct depends on) and do NOT rename, to leave the bodied .cpp intact.
// PDB float names recorded above for when an IFilter base is introduced.
//
// Layout grounded in AllPassFilter::AllPassFilter @0x82B6C3D8 (and SetGains @0x82B64688,
// AllPassFilterApplyFunc @0x82B64640):
//   +0x00  miField00  (int,  init 0)
//   +0x04  miField04  (int,  init 0)
//   +0x08  miMode     (int,  init 1; the loop's tap/length descriptor)
//   +0x0C  miField0C  (int,  init 1)
//   +0x10  mfGain1    (f32,  init 0.0; SetGains arg1 -> the feedback/feedforward gain)
//   +0x14  mfGain2    (f32,  init 0.0; SetGains arg2 -> the mix gain)
// (sizeof == 0x18; the apply func reads three buffer/stride words from a separate
//  per-tap context struct, not from this object.)
// -------------------------------------------------------------------------------------
class AllPassFilter
{
public:
    // Per-tap context the apply func is handed (the `a5` struct): the delay-line base,
    // its two work strides and the active flag the apply func tests at +0x08.
    struct Context
    {
        f32 *mpBase;        // +0x00 -- delay-line base (kernel arg `base`)
        f32 *mpFeedback;    // +0x04 -- feedback buffer  (kernel arg `feedbackBuf`)
        s32  miInactive;    // +0x08 -- when nonzero the tap is silenced (memset path)
        s32  miField0C;     // +0x0C
        f32 *mpAccum;       // +0x10 -- accumulate buffer (kernel arg `accumBuf`)
        f32 *mpFeedforward; // +0x14 -- feedforward / output buffer (also the memset dst)
    };

    static AllPassFilter *AllPassFilter_ctor(AllPassFilter *self); // @0x82B6C3D8
    static void *AllPassFilterApplyFunc(AllPassFilter *self, s32 count, s32 src,
                                        f32 *extra, Context *ctx); // @0x82B64640
    static AllPassFilter *SetGains(AllPassFilter *self, f32 g1, f32 g2); // @0x82B64688

    s32 miField00; // +0x00
    s32 miField04; // +0x04
    s32 miMode;    // +0x08
    s32 miField0C; // +0x0C
    f32 mfGain1;   // +0x10
    f32 mfGain2;   // +0x14
};

// -------------------------------------------------------------------------------------
// CombFilter -- one feedback comb reverb section (a comb whose loop contains a damping
// gain pair).
//
// FLAG (rwaudio PDB reconcile -- DIVERGE, layout kept): the ProStreet-08 X360 PDB names
// this type and reveals it derives from rw::audio::core::IFilter:
//     class rw::audio::core::CombFilter [sizeof = 36] : public IFilter {
//       base +0x00 [16] IFilter { mpApplyFunc(+0x00), mpResetFunc(+0x04),
//                                  mReadLength(+0x08 int), mChannels(+0x0C int) }
//       data +0x10 [4] float ma_1
//       data +0x14 [4] float ma_M
//       data +0x18 [4] float mb_MPlus1
//       data +0x1c [4] float mScaleFactor
//       data +0x20 [4] float mPrev }
// sizeof MATCHES (0x24), and the five trailing floats align 1:1 by offset
// (mfGain1==ma_1 @+0x10, mfGain2==ma_M @+0x14, mfGain3==mb_MPlus1 @+0x18,
// mfGain4==mScaleFactor @+0x1C, mfState==mPrev @+0x20). The +0x00..+0x0F region
// DIVERGES structurally (PDB IFilter base with two FUNCTION POINTERS vs the four guessed
// ints here; IFilter's members belong to a separate base type). Per reconcile rules we
// keep the ARTIST flat-int layout untouched (an x64 IFilter base would break the X360
// 4-byte pointer widths) and do NOT rename, to leave the bodied .cpp intact. PDB float
// names recorded above for when an IFilter base is introduced.
//
// Layout grounded in CombFilter::CombFilter @0x82B6C6B0, SetGains
// @0x82B64D98, CombFilterResetFunc @0x82B64D88:
//   +0x00  miField00  (int,  init 0)
//   +0x04  miField04  (int,  init 0)
//   +0x08  miMode     (int,  init 2; tap/length descriptor)
//   +0x0C  miField0C  (int,  init 1)
//   +0x10  mfGain1    (f32,  init 0.0; SetGains arg1)
//   +0x14  mfGain2    (f32,  init 0.0; SetGains arg2)
//   +0x18  mfGain3    (f32,  init 0.0; SetGains arg3 -- damping coefficient)
//   +0x1C  mfGain4    (f32,  init 0.0; SetGains arg4 -- damping coefficient)
//   +0x20  mfState    (f32,  init 0.0; the loop's one-pole damping state, cleared by
//                            CombFilterResetFunc)
// (sizeof == 0x24.)
// -------------------------------------------------------------------------------------
class CombFilter
{
public:
    static CombFilter *CombFilter_ctor(CombFilter *self);          // @0x82B6C6B0
    static CombFilter *CombFilterResetFunc(CombFilter *self);      // @0x82B64D88
    static CombFilter *SetGains(CombFilter *self, f32 g1, f32 g2,
                                f32 g3, f32 g4);                   // @0x82B64D98

    s32 miField00; // +0x00
    s32 miField04; // +0x04
    s32 miMode;    // +0x08
    s32 miField0C; // +0x0C
    f32 mfGain1;   // +0x10
    f32 mfGain2;   // +0x14
    f32 mfGain3;   // +0x18
    f32 mfGain4;   // +0x1C
    f32 mfState;   // +0x20
};

} // namespace core
} // namespace audio
} // namespace rw
