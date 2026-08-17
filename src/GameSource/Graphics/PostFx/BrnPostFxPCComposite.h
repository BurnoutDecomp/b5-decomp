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
namespace rw { struct IResourceAllocator; }
namespace renderengine { class Texture; }

// Run BrnPostFx::Construct @0x82409F80 over the file-scope singleton, once. On the console this is
// BrnRendererModule::Construct @0x8240A778's own call (`addi r3, r11, mPostFxVault@l` @0x8240B774 /
// `bl BrnPostFx__Construct` @0x8240B78C, r4 = this->mpGraphicsAllocator, guest +0x394C) -- the same
// translation-unit boundary that keeps Render behind
// this seam keeps Construct behind it too. It is DEFERRED on PC to the frame that builds the post-fx
// pool (BrnRendererModule's EnsurePostFxSceneTargets), for the reason every other pool object is: the
// device does not exist at Construct time on this build, and the module's mpGraphicsAllocator is null
// (BrnResource::Allocators::GetGlobalGraphicsAllocator is declaration-only), so the bring-up
// allocator every other console Construct already runs through (sWorldDispatchAllocator -- see
// mIm3dRendererSkyDome.Construct) is what is passed. Idempotent: a second call does nothing.
void PCBringUpConstructPostFx(rw::IResourceAllocator* lpAllocator);

// Run BrnPostFx::Render @0x8240A468 over the render-target pool: down-sample buffer in, back-buffer
// pool slot out. RETURNS FALSE, having drawn nothing, when the pool cannot supply every surface the
// console body dereferences without a test (down-sample, bloom, depth-of-field, back buffer), or
// when PCBringUpConstructPostFx has not run -- BrnPostFx::Render's own first act is `if
// (mnActiveLayerCount == 0) return`, which Construct is what sets, so an un-Constructed singleton
// would "succeed" having drawn nothing. THE CALLER MUST PRESENT THE FRAME SOME OTHER WAY ON FALSE: the
// world is already drawn off-screen by that point, so "did nothing" means a black frame with the GUI
// on top. It does NOT hand the swap chain back either way; the caller owns that, so that the
// rebind-on-every-path property is visible at the call site instead of buried in here.
//
// lpafTint2dColourXYZW / lbMotionBlurEnabled are the console's OWN two effects-frame arguments to
// BrnPostFx::Render (v1 and r7 at the X360 call site 0x8240DE30 / 0x8240DE18): the 2D tint vector
// BrnRendererModule::Render evaluates at 0x8240DC80-0x8240DCBC and the layer-0 internal frame's
// mMotionBlurData.mbIsActive byte it reads at 0x8240C2DC. They were hard-coded here (zero / false)
// while mEffectsArbitrator was never Constructed; the bloom wave Constructs it, so they are
// parameters now. A null lpafTint2dColourXYZW keeps the previous zero-vector behaviour.
// lpOverrideSourceTexture is the console's LAST argument to BrnPostFx::Render (`stw r30, var_CD4`
// @0x8240DE28, read by Render as arg_6C): the calibration ramp texture the renderer derives from the
// dispatch buffer's calibration handle at 0x8240DD50-0x8240DE08, or null. Non-null replaces the
// composite's source, forces every effect off and the white level to 1.0 (BrnPostFx.cpp steps 5/8/10).
// Would PCBringUpRenderPostFxComposite below actually reach BrnPostFx::Render this frame? True when
// the pool holds every surface the console body dereferences without a test AND
// PCBringUpConstructPostFx has run. A pure query -- no logging, no side effect.
//
// IT EXISTS FOR THE TINT BLEND'S Lock/Unlock PAIRING. BrnPostFx::BeginTintBlend locks the tint volume
// texture; only BrnPostFx::Render's step-1 drain unlocks it. The console runs both under one gate
// (mbRenderPostFX, tested at 0x8240C6A8 for the tint block and 0x8240DC6C for the composite) so a
// lock cannot outlive its frame -- on PC the composite carries this extra precondition, so
// BrnRendererModule::Render asks it BEFORE scheduling the blend. RETIRED WITH THE BRING-UP, together
// with the rest of this header.
bool PCBringUpPostFxCompositeWillRun(BrnRendererMemory& lrRendererMemory);

bool PCBringUpRenderPostFxComposite(BrnRendererMemory& lrRendererMemory,
                                    f32 lfBrightness,
                                    f32 lfContrast,
                                    f32 lfFrameWhiteLevel,
                                    f32 lfAspectCorrection,
                                    const f32* lpafTint2dColourXYZW,
                                    bool lbMotionBlurEnabled,
                                    renderengine::Texture* lpOverrideSourceTexture);

#endif // BRN_POST_FX_PC_COMPOSITE_H
