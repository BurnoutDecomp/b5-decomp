#include "types.hpp"

#include <cstdio>   // std::fputs (the one-shot BLOCKED report)

#include "GameShared/Jobs/TintBlend/TintBlend.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxtint.h"  // TintBlendParameters

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   TintBlendEntry                        @ 0x82AD2CE8
//   rw::graphics::postfx::TintBlend       @ 0x82AD4860
//
// The colour-cube tint blend: BrnPostFx::BeginTintBlend locks the current Tint's colour-lookup
// volume, fills a TintBlendParameters with the destination surface plus up to six source cubes and
// their weights, and schedules this job; BrnPostFx::Render drains it (Job::WaitOn) and unlocks the
// surface through Tint::EndBlendJob before anything samples the map.

namespace rw
{
namespace graphics
{
namespace postfx
{
    // ============================================================================================
    // TintBlend @0x82AD4860 -- eight instructions, and all eight are one indirect jump:
    //
    //     lwz   r10, 4(r3)                 ; numSources  (TintBlendParameters +0x04)
    //     lis   r11, dword_82F7238C@ha
    //     addi  r11, r11, dword_82F7238C@l
    //     slwi  r10, r10, 2                ; * sizeof(void*)
    //     lwzx  r11, r10, r11
    //     mtctr r11
    //     bctr                             ; TAIL call, r3 still the parameter block
    //
    // i.e. `gapTintBlendVariants[lrParameters.numSources](lrParameters)` -- a per-source-count
    // specialisation table, the usual shape for a blend kernel that unrolls its source loop.
    // (This is also the second, independent proof that TintBlendParameters +0x04 is `numSources`:
    // the word is used here as a JUMP-TABLE INDEX. The committed rwgpfxtint.h had no such member
    // and BeginBlendJob wrote the destination row pitch into that slot -- corrected in this wave.)
    //
    // [FLAG BLOCKED: the variant table `dword_82F7238C` and the kernels it points at.
    //  WHAT IS MISSING, exactly: (a) the seven pointer words at X360 0x82F7238C..0x82F723A7 -- .data,
    //  and the IDA export is function-only so they are not in the export set nor in
    //  scratch/postfx_wave1b_dossiers/DATA_DUMP.md; (b) the kernels themselves, which are unnamed
    //  `sub_82AD....` bodies -- a name search over progress/identity.json returns only
    //  `TintBlendEntry` and `rw::graphics::postfx::TintBlend` in the whole 0x82AD.... page, so no
    //  entry point in the ledger reaches them and their addresses have to come from the table.
    //  Recovering (a) makes (b) a normal reconstruction task.
    //
    //  UNTIL THEN THIS IS A NO-OP, AND THAT IS THE SAFE FAILURE: the destination surface is the
    //  Tint's LOCKED colour-lookup volume, so a fabricated blend would write wrong pixels into a
    //  live 32x32x32 lookup table that the composite then samples for every pixel of the frame --
    //  a plausible-looking, uniformly mis-graded image. Doing nothing leaves the cube at whatever
    //  Tint::Initialize left, and the composite's tint permutation is off on this build anyway
    //  (m_enabledFx is 0, so BrnPostFx::BeginTintBlend's `& 0x20` gate never fires and this job is
    //  never even scheduled).]
    // ============================================================================================
    void TintBlend(TintBlendParameters& lrParameters)
    {
        static bool sbReportedNoVariants = false;
        if (!sbReportedNoVariants)
        {
            sbReportedNoVariants = true;
            std::fputs("[TintBlend] the per-source-count blend variant table (X360 dword_82F7238C)"
                       " is not recovered: the colour-cube blend is a no-op."
                       " [FLAG BLOCKED: dword_82F7238C + the kernels it indexes]\n", stderr);
        }
        (void)lrParameters;
    }
}
}
}

// ================================================================================================
// TintBlendEntry @0x82AD2CE8
//
//     mr r3, r4
//     b  rw__graphics__postfx__TintBlend
//
// A two-instruction thunk: take the job's SECOND parameter word and tail-call the blend with it.
// Param 1 is where EA::Jobs::Job::SetData parks the data pointer (job.cpp:137), and
// BrnPostFx::BeginTintBlend @0x823F8464 calls SetData with the TintBlendParameters block
// Tint::BeginBlendJob just returned -- so the cast below is naming what the console already relies
// on, not reinterpreting an unknown word. The other three Params are untouched (r5/r6/r7 are never
// read), which is reproduced by leaving them unnamed.
// ================================================================================================
void TintBlendEntry(EA::Jobs::Param /*lParam0*/, EA::Jobs::Param lParam1,
                    EA::Jobs::Param /*lParam2*/, EA::Jobs::Param /*lParam3*/)
{
    rw::graphics::postfx::TintBlend(
        *static_cast<rw::graphics::postfx::TintBlendParameters*>(lParam1.mpValue));
}
