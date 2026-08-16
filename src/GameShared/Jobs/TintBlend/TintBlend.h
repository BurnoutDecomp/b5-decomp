#ifndef GAMESHARED_JOBS_TINTBLEND_H
#define GAMESHARED_JOBS_TINTBLEND_H

#include "types.hpp"
#include "SDKs/EATech/eajobs/job_types.h"   // EA::Jobs::Param

// The colour-cube tint blend job.
//
// HOME. The DecFIGS DWARF puts this job in GameShared/Jobs/TintBlend/ (TintBlend.h:56-57 declare
// `_binary_TintBlend_elf_start` / `_size`, and GameSource/Unity/BrnJobsUnity.cpp:88 embeds the
// 19,264-byte ELF): on the PS3 the blend is an SPU job linked in as a binary blob. The X360 build
// has no SPUs, so the same job is a pair of ordinary PPU functions -- `TintBlendEntry` @0x82AD2CE8
// (the EA::Jobs entry point) and `rw::graphics::postfx::TintBlend` @0x82AD4860 (the dispatcher) --
// which the ledger leaves without a primary_file. They live here, in the DWARF's own directory for
// this job, rather than being smuggled into BrnPostFx.cpp (which merely ARMS the job:
// BrnPostFx::Construct @0x8240A2D4 hands `&TintBlendEntry` to EntryPoint::SetCode).
//
// ENTRY-POINT SHAPE. `EA::Jobs::EntryPoint::SetCode` takes a code pointer that the scheduler calls
// with the job's four Param words, so the signature is the four-Param local-job entry
// (job_types.h:64-66). The X360 body is two instructions -- `mr r3, r4` / `b TintBlend` -- i.e. it
// forwards PARAM 1, which is exactly where `EA::Jobs::Job::SetData` parks the data pointer
// (job.cpp:137, `mParams[1].mpValue = lpvData`), and BrnPostFx::BeginTintBlend @0x823F8464 calls
// SetData with the Tint's TintBlendParameters block.

namespace rw
{
namespace graphics
{
namespace postfx
{
    struct TintBlendParameters;

    // X360 0x82AD4860 -- run the blend described by the parameter block.
    void TintBlend(TintBlendParameters& lrParameters);

    // The per-source-count blend variants the dispatch table dword_82F7238C indexes with
    // TintBlendParameters::numSources. Declared here (rather than left file-local) because they
    // are named X360 functions in their own right, and because the table that holds their
    // addresses is the ONLY thing that reaches them -- a reader who greps for a call site of
    // Blend4Cubes and finds none must be able to see why.
    //   [1] 0x82AD4078 SetColour     [2] 0x82AD4170 Blend2Cubes   [3] 0x82AD4280 Blend3Cubes
    //   [4] 0x82AD43D0 Blend4Cubes   [5] 0x82AD4528 Blend5Cubes   [6] 0x82AD46B0 Blend6Cubes
    void SetColour  (TintBlendParameters& lrParameters);
    void Blend2Cubes(TintBlendParameters& lrParameters);
    void Blend3Cubes(TintBlendParameters& lrParameters);
    void Blend4Cubes(TintBlendParameters& lrParameters);
    void Blend5Cubes(TintBlendParameters& lrParameters);
    void Blend6Cubes(TintBlendParameters& lrParameters);
}
}
}

// X360 0x82AD2CE8 -- the EA::Jobs entry point BrnPostFx::Construct arms the tint-blend job with.
// Global scope and `extern "C++"` linkage exactly as the console has it (the symbol is the plain
// C++-mangled `TintBlendEntry`; BrnPostFx.cpp takes its address).
void TintBlendEntry(EA::Jobs::Param lParam0, EA::Jobs::Param lParam1,
                    EA::Jobs::Param lParam2, EA::Jobs::Param lParam3);

#endif // GAMESHARED_JOBS_TINTBLEND_H
