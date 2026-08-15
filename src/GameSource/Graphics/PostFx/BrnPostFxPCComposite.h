#ifndef BRN_POST_FX_PC_COMPOSITE_H
#define BRN_POST_FX_PC_COMPOSITE_H

#include "types.hpp"

// ==================================================================================================
// [FLAG PC bring-up] THE ONE DECLARATION BrnRendererModule.cpp NEEDS TO REACH THE POST-FX COMPOSITE.
//
// WHY THIS HEADER EXISTS, and it is not tidiness -- it is the only shape that compiles.
// BrnRendererModule.cpp cannot include GameSource/Graphics/PostFx/BrnPostFx.h. That header needs the
// REAL EA::Jobs::Job (BrnPostFx::m_blendJob is one by value, so it needs the complete type), while
// BrnRendererModule.h:21-31 still carries an off-path PLACEHOLDER `class EA::Jobs::Job` with an
// invented `Job(s32 = 0)` constructor. Bringing both into one translation unit is
//     error C2011: 'EA::Jobs::Job': class type redefinition   (job.h:41 vs BrnRendererModule.h:25)
// followed by ten cascade errors and C2079 on BrnPostFx::m_blendJob -- MEASURED, not predicted.
//
// AND THE PLACEHOLDER CANNOT SIMPLY BE RETIRED IN THIS WAVE. The real EA::Jobs::Job has exactly one
// constructor, `explicit Job(const char*)`, in the reconstructed header (job.h:68) AND in the DWARF
// (references/DecFIGS/dwarfdump/SDKs/EATech/include/job_manager/job.h:83 -- `void Job(const char*)`,
// no default constructor anywhere in the class). BrnRendererModule.h declares eleven Job members and
// three Job ARRAYS by value (BrnRendererModule.h:557-574), every one of them default-constructed, so
// swapping in the real type requires naming all of them in the module's constructor. That is the
// renderer module's own reconstruction work, and inventing a default constructor to dodge it would
// be a fabrication. So the two headers stay apart and this one-function seam is what crosses.
//
// The seam is deliberately as thin as a seam can be: one free function, one forward declaration, no
// post-fx type in the signature. RETIRED WITH THE BRING-UP -- when EA::Jobs::Job is real in
// BrnRendererModule.h, this file and its definition in BrnPostFx.cpp are deleted and
// BrnRendererModule.cpp includes BrnPostFx.h directly, as the console's single translation unit did.
// ==================================================================================================

// Pointer/reference-only use: the documented cascade-avoidance forward declaration in AGENTS.md.
// `struct`, matching BrnRendererMemory.h:47.
struct BrnRendererMemory;

// Run BrnPostFx::Render @0x8240A468 over the render-target pool: down-sample buffer in, back-buffer
// pool slot out. RETURNS FALSE, having drawn nothing, when the pool cannot supply both surfaces --
// which is this build's state, because BrnRendererMemory::PCBringUpCreatePostFxSceneTargets creates
// only DOWN_SAMPLE and ANTI_ALIAS. THE CALLER MUST PRESENT THE FRAME SOME OTHER WAY ON FALSE: the
// world is already drawn off-screen by that point, so "did nothing" means a black frame with the GUI
// on top. It does NOT hand the swap chain back either way; the caller owns that, so that the
// rebind-on-every-path property is visible at the call site instead of buried in here.
bool PCBringUpRenderPostFxComposite(BrnRendererMemory& lrRendererMemory,
                                    f32 lfBrightness,
                                    f32 lfContrast,
                                    f32 lfFrameWhiteLevel,
                                    f32 lfAspectCorrection);

#endif // BRN_POST_FX_PC_COMPOSITE_H
