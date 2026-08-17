#include "GameSource/Graphics/PostFx/BrnPostFx.h"

#include <new>                                                                  // placement new (the carved effects)
#include <cstdio>                                                               // std::snprintf (the seam's sampled diag)
#include "GameSource/Graphics/BrnRendererMemory.h"                              // the render-target pool
#include "GameSource/Graphics/PostFx/BrnPostFxPCComposite.h"                    // the PC composite seam
#include "GameShared/GameClasses/Graphics/CgsRenderTarget.h"                     // CgsRenderTarget::GetRenderTarget
#include "GameShared/GameClasses/Graphics/CgsDepthStencilStateFactory.h"        // saDepthStencilStates[1] (Render)
#include "GameShared/GameClasses/Graphics/CgsRasterizerStateFactory.h"          // saRasterizerStates[2]   (Render)
#include "GameShared/GameClasses/Graphics/CgsBlendStateFactory.h"               // saBlendStates[1]        (Construct: B4Blur scatter)
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"           // shadow::Device::SetState
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                      // the seam's one-shot lines
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxhelper.h"  // PfxHelper + gpPfxHelper
#include "GameShared/Jobs/TintBlend/TintBlend.h"                                // TintBlendEntry

// BrnPostFx -- the Burnout post-effects driver.
//
// SOURCE-OF-TRUTH: the X360 ARTIST asm (BrnPostFx ctor @0x82407FC8) is the spine for the
// construction-time member values reconstructed here; the DecFIGS DWARF supplies the member shape.
//
// FOUR of the six previously declaration-only methods are now bodied -- Construct @0x82409F80,
// Destruct @0x82408138, PrepareDownSampleBuffers @0x82408E88 and Render @0x8240A468. Each carries
// its own banner mapping it to the X360 instructions it came from.
//
// BeginTintBlend / EndTintBlend ARE NOW BODIED (post-fx step 10). They are the colour-cube blend
// job: BeginTintBlend @0x823F8380 flips the double-buffered tint map, locks it through
// Tint::BeginBlendJob, fills the TintBlendParameters source list from m_colourCubes/m_tintFactors
// and dispatches the blend; EndTintBlend (DWARF BrnPostFx.cpp:288, inlined by the X360 into
// Render @0x8240A4DC) unlocks it. So are the three tint mutators SetColourCube /
// SetTintBlendNumber / SetTintBlendFactor, recovered from their inlined call sites.
//
// THE ORIGINAL "not on the build list" BANNER IS RETIRED: this TU IS on
// tools/build/build_game_exe.bat (line 502) as of the rung-5 composite wave --
//   $ grep -n "PostFx.BrnPostFx.cpp" tools/build/build_game_exe.bat
//   502:  echo "%SRC%\GameSource\Graphics\PostFx\BrnPostFx.cpp"
// -- and its unresolved set is link-closed. The historical set is kept below as the record of what
// had to land first. The measured set -- BrnPostFxShader::{Construct,Destruct,Render},
// PfxHelper::{PfxHelper,Release}, DepthOfField::{DepthOfField,Release,
// DownSampleAndGaussianBlur}, Vignette::{Vignette,Release}, Vignette::Parameters::Parameters,
// Tint::{Initialize,InitializePixelProgram,Release,EndBlendJob}, B4Blur::{B4Blur,State::State},
// B4Blur::Parameters::Parameters, BrnPostFxBloom::{Construct,Render,Destruct} and TintBlendEntry --
// is written out in scratch/postfx_step2_out/driver/REPORT.md with the owning TU for each.
//
// THREE ENTRIES OF THAT SET ARE CLOSED as of the states/motion-blur step: MotionBlurState::Construct
// is now bodied in BrnPostFxShader.cpp (its DWARF home, and that TU IS on the build list), and the
// two cached-state externs gpPostFxDepthStencilState / gpPostFxRasterizerState are DELETED rather
// than defined -- they never existed on the console; see the banner below.

BrnPostFx msPostFx;

BrnPostFx::BrnPostFx()
    : m_blendJob(nullptr) // EA::Jobs::Job::Job(&m_blendJob, 0): the unnamed tint-blend job
{
    // mPostFxShader, m_b4blurState and m_blendJob are constructed by their own constructors:
    //   BrnPostFxShader::BrnPostFxShader(this)           -- the embedded shader sub-object
    //   rw::graphics::postfx::B4Blur::State::State(...)  -- the embedded blur state block
    //   EA::Jobs::Job::Job(&m_blendJob, 0)               -- the tint-blend job, unnamed (null name)
    // matching the X360 ctor's three sub-object construction calls.

    m_enabledFx = 0;

    // The vignette state block (the v11..v14 stores into the vignette gap). RENAMED 2026-08-15 to the
    // DecFIGS member set, which the step-3 wave pinned against the X360 asm: the four vectors are
    // m_gradientScalars {0,0,0,0} / m_innerColour {1,1,1,1} / m_outerColour {1,1,1,1} /
    // m_centerScale {0.5,0.5,1,1}, and there is NO trailing sharpness float -- Vignette::State is
    // exactly 0x40 bytes, which is what Render's own `addi r6, r31, 0x390` / `addi r7, r31, 0x3D0`
    // pair measures. These are the same sixteen values as before, under the names that say what they
    // are: a centre of (0.5, 0.5) at unit scale, fed to a shader constant the composite's own interned
    // name calls VignetteCentreXyScaleXy. Vignette::Parameters::Parameters @0x823F5420 writes the
    // identical four vectors, which is the independent corroboration.
    m_vignetteState.m_gradientScalars.x = 0.0f;
    m_vignetteState.m_gradientScalars.y = 0.0f;
    m_vignetteState.m_gradientScalars.z = 0.0f;
    m_vignetteState.m_gradientScalars.w = 0.0f;
    m_vignetteState.m_innerColour.x = 1.0f;
    m_vignetteState.m_innerColour.y = 1.0f;
    m_vignetteState.m_innerColour.z = 1.0f;
    m_vignetteState.m_innerColour.w = 1.0f;
    m_vignetteState.m_outerColour.x = 1.0f;
    m_vignetteState.m_outerColour.y = 1.0f;
    m_vignetteState.m_outerColour.z = 1.0f;
    m_vignetteState.m_outerColour.w = 1.0f;
    m_vignetteState.m_centerScale.x = 0.5f;
    m_vignetteState.m_centerScale.y = 0.5f;
    m_vignetteState.m_centerScale.z = 1.0f;
    m_vignetteState.m_centerScale.w = 1.0f;

    // The depth-of-field state seeds (float3D0..float3EC immediates): the four focal planes, the
    // projection near/far planes, the dof amount and the blur radius.
    m_dofState.m_focalPlanes[0] = 0.1f;
    m_dofState.m_focalPlanes[1] = 10.0f;
    m_dofState.m_focalPlanes[2] = 100.0f;
    m_dofState.m_focalPlanes[3] = 1000.0f;
    m_dofState.m_projNearPlane = 0.1f;
    m_dofState.m_projFarPlane = 1000.0f;
    m_dofState.m_dofAmount = 1.0f;
    m_dofState.m_blurRadius = 4.5f;

    // The render-target / effect pointers and the tint-blend counters the ctor zeroes
    // (dword5A0/5A8/5B8/5BC/5C8, lpTint, dword5C4, byte5F4 and the tint-buffer index).
    m_deviceRt = nullptr;
    m_mainRt = nullptr;
    m_renderTarget = nullptr;
    m_frameRt = nullptr;
    m_bloomDsRt = nullptr;
    m_depthOfFieldRt = nullptr;

    m_pfxDof = nullptr;
    m_pfxVignette = nullptr;
    m_pfxTint[0] = nullptr;
    m_pfxTint[1] = nullptr;
    m_pfxB4Blur = nullptr;

    m_processTint = false;
    m_currentTintBuffer = 0;
}

// ================================================================================================
// THE TWO CACHED RENDER STATES BrnPostFx::Render PUSHES -- CLOSED. This block used to declare two
// externs, `gpPostFxDepthStencilState` and `gpPostFxRasterizerState`, that NOTHING DEFINED. They
// are gone, and nothing replaces them, because THERE IS NO SUCH GLOBAL ON THE CONSOLE: the two
// words the asm loads are two slots of the two state-factory tables, which this tree already owns.
//
//   X360 dword_83010910 == CgsDepthStencilStateFactory::saDepthStencilStates[1] -- the block base
//   the asm addresses is dword_8301090C == slot 0, and Construct @0x827EBBA0 writes the +4 slot at
//   0x827EBBEC / 0x827EBD4C. Slot 1 is E_FACTORY_DEPTH_STENCIL_STATE_ZOFF_ZALL_ZWRITEOFF: depth
//   test off, compare ALWAYS, no depth write -- exactly what a full-screen composite wants.
//
//   X360 dword_83010A40 == CgsRasterizerStateFactory::saRasterizerStates[2] ==
//   E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_NONE -- scissor on, no culling, which is what a
//   full-screen quad wants. Construct @0x827EBF30 stores it at 0x827EC14C (and clears it at
//   0x827EBFB0); a `stw` scan of all 30,095 function exports finds those two and no others.
//
// WHAT CHANGED SO THIS COULD BE WRITTEN (it was PLUMBING, never RE): both tables are now the
// DWARF's PRIVATE STATIC members with a bodied `GetState`, and CgsRasterizerStateFactory -- which
// had no header at all, only a TU-local, non-polymorphic, wrong-signature declaration inside its
// own .cpp -- now has one at the DWARF shape. Minting a host global for state the tree already owns
// would have been a split-brain; naming the slot is the fix. See CgsDepthStencilStateFactory.h and
// CgsRasterizerStateFactory.h (both included above) for the full evidence, including why GetState
// is spelled `static` and why that is an inference rather than an attestation.
//

// The EA::Jobs entry point BrnPostFx::Construct arms the tint-blend job with (X360 TintBlendEntry
// @0x82AD2CE8, referenced at 0x8240A2C8/0x8240A2D4). Its home is the DWARF's
// GameShared/Jobs/TintBlend/, where it is now defined (TintBlend.cpp); the declaration is included
// from that TU's header instead of being re-spelled here.

// ================================================================================================
// BrnPostFx::Construct @ 0x82409F80   (goes into b5-decomp/src/GameSource/Graphics/PostFx/BrnPostFx.cpp)
//
// Ground truth: the X360 ARTIST ASSEMBLY at 0x82409F80-0x8240A464 (dossier H1). Hex-Rays is used
// only to read the float immediates back; every store below is matched to an instruction.
//
// WHAT THE FUNCTION IS. Construct is the post-fx driver's one-time build: it carves each render-
// engine post-fx effect (the shared PfxHelper, DepthOfField, Vignette, two Tints, B4Blur) out of the
// supplied rw resource allocator, seeds each effect's tuning state, arms the tint-blend job, seeds
// the motion-blur state to identity, then constructs the shader array and the bloom sub-object.
//
// THE ALLOCATION IDIOM, which repeats six times. The X360 writes a 5-entry rw::BaseResourceDescriptors
// on the stack -- entry 0 = {size, alignment}, entries 1..4 = the identity {0, 1} -- and calls the
// allocator's vtable slot +0x10, which rwcore.pdb names DoAllocate(const ResourceDescriptor&,
// const char*). The name argument is 0 at every one of the six call sites (`li r6, 0`).
//
// ⚠⚠ THE FIVE BYTE SIZES ARE **HOST** sizeofs, NOT THE X360 IMMEDIATES, AND THAT IS A CORRECTNESS
// REQUIREMENT, NOT A STYLE CHOICE. Each carve is immediately placement-new'd with a HOST C++ object,
// so the block must be at least `sizeof` that host type. The guest is 32-bit big-endian and the host
// is LLP64: every pointer in these effect classes widens 4 -> 8, so EVERY X360 record size is TOO
// SMALL for its host twin. Writing the console immediates into the size argument means the
// constructor writes past the end of its own allocation, into whatever the bump allocator handed out
// next -- silent, finite heap corruption that presents as a plausible frame with something else
// broken somewhere else. AGENTS.md states the rule ("model the PC layout with x64 widths (widen
// pointers; same rule as every recon)") and this tree already applies it at runtime-carve sites:
// CgsDispatcher.cpp:131/181, CgsVideoDataResource.cpp:50, CgsRwTextureStateResourceType.cpp:34
// (`static_cast<u32>(sizeof(renderengine::TextureState)); // X360: 0x24`), which is the exact idiom
// copied below.
//
// The X360 immediates are therefore DOCUMENTATION, and each carve site names its own:
//     PfxHelper    X360 {0x50,  0x10}   (asm 0x82409F8C `li r11, 0x50`  / `li r24, 0x10`)
//     DepthOfField X360 {0x30,  0x10}   (asm 0x8240A0B0 `li r23, 0x30`)
//     Vignette     X360 {0xD0,  0x10}   (asm 0x8240A154 `li r11, 0xD0`)
//     Tint         X360 {0x100, 0x80}   (asm 0x8240A210 `li r11, 0x100` / 0x8240A238 `li r11, 0x80`)
//     B4Blur       X360 {0x130, 0x10}   (asm 0x8240A328 `li r11, 0x130`)
// THE ALIGNMENTS ARE NOT WIDENED and are passed verbatim: 0x10 is an alignment request, not a
// record size, and Tint's 0x80 is the console's own vector/constant-buffer alignment. Neither
// depends on pointer width.
//
// ⚠ THE HOST DESCRIPTOR HAS FOUR ENTRIES, NOT FIVE. rw::ResourceDescriptor is
// BaseResourceDescriptors<4> on the PC (rwcore.pdb, x64); the X360 SERIALISED form is <5>. Per
// AGENTS.md that is a documented cross-build delta, so the console's fifth identity pair is dropped
// rather than replicated -- entry 0 carries the request and the rest stay identity, exactly as
// CgsDepthStencilStateFactory::CreateDepthStencilState already does in this tree. (Same widening
// rule, applied to the entry COUNT instead of the byte size.)
// ================================================================================================

namespace
{
    // The console's inline descriptor build, un-inlined once instead of written out six times
    // (AGENTS.md: inlining reversal). Entry 0 is the request; the remaining entries are the
    // rw::BaseResourceDescriptor identity, {size 0, alignment 1}.
    //
    // ⚠ lu32Size IS A HOST sizeof AT EVERY CALLER. The block is placement-new'd immediately, so a
    // guest record size here is heap corruption on construction -- see the banner above.
    rw::Resource AllocatePostFxResource(rw::IResourceAllocator* lpAllocator,
                                        u32 lu32Size, u32 lu32Alignment)
    {
        rw::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = lu32Size;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = lu32Alignment;
        for (u32 luEntry = 1u; luEntry < 4u; ++luEntry)
        {
            lDescriptor.m_baseResourceDescriptors[luEntry].m_size      = 0u;
            lDescriptor.m_baseResourceDescriptors[luEntry].m_alignment = 1u;
        }
        return lpAllocator->DoAllocate(lDescriptor, nullptr);
    }

    // --- Construct's recovered immediates ---------------------------------------------------------
    // Each one is the .rdata symbol the matching `lfs` addresses, named for the store it feeds.
    // (The already-committed BrnPostFx constructor writes the same eight values into m_dofState from
    // its own copies of the same symbols; they are repeated because Construct seeds the DepthOfField
    // PARAMETERS block, which is a different object.)
    const f32 KF_DOF_FOCAL_PLANE_0 = 0.1f;     // flt_82004014 (lfs @0x8240A04C, stfs @0x8240A054)
    const f32 KF_DOF_FOCAL_PLANE_1 = 10.0f;    // flt_82004A20 (lfs @0x8240A05C, stfs @0x8240A064)
    const f32 KF_DOF_FOCAL_PLANE_2 = 100.0f;   // flt_820049E0 (lfs @0x8240A068, stfs @0x8240A070)
    const f32 KF_DOF_FOCAL_PLANE_3 = 1000.0f;  // flt_82009E10 (lfs @0x8240A074, stfs @0x8240A078)
    const f32 KF_DOF_PROJ_NEAR     = 0.1f;     // flt_82004014 again (stfs @0x8240A058)
    const f32 KF_DOF_PROJ_FAR      = 1000.0f;  // flt_82009E10 again (stfs @0x8240A080)
    const f32 KF_DOF_AMOUNT        = 1.0f;     // flt_82001C98 (lfs @0x8240A084, stfs @0x8240A08C)
    const f32 KF_DOF_BLUR_RADIUS   = 4.5f;     // flt_820139F0 (lfs @0x8240A090, stfs @0x8240A094)

    // The colour-lookup (tint map) cube edge: `li r25, 0x20` @0x8240A218, stored as the leading word
    // of the Tint::Parameters block (`stw r25, 0x300+var_278(r1)` @0x8240A23C).
    const u32 KU_TINT_LOOKUP_SIZE = 32u;

    // The tint-blend job's SetCode arguments: environment `li r4, 0` @0x8240A2D8 and size
    // `li r6, 0` @0x8240A2D0.
    const s32 KI_TINT_BLEND_JOB_CODE_SIZE = 0;
}

void BrnPostFx::Construct(rw::IResourceAllocator* lpAllocator)
{
    // asm 0x82409FBC-0x82409FD0: the three counters are cleared before anything is built. The
    // second of them is the LATCH BrnPostFx::Render gates the whole composite on; it is set to 1
    // at 0x8240A3DC -- BEFORE the motion-blur identity seed (through 0x8240A448) and before the two
    // sub-object Constructs (0x8240A44C, 0x8240A45C), not as the function's last act. The body
    // below reproduces that order exactly; see the note at the store.
    mnActiveLayerBitMask = 0u;
    mnActiveLayerCount   = 0u;
    m_enabledFx          = 0u;

    // ---- the shared post-fx helper (asm 0x82409FF4-0x8240A038) ---------------------------------
    // PfxHelper is a SINGLETON: its constructor publishes itself into the file-scope pointer the
    // whole post-fx layer reads (X360 off_82FAEE80), which is how Destruct and every effect reach it
    // afterwards. Construct keeps no pointer to it. The console builds the one-member parameter block
    // on the stack (`stw r28, 0x300+var_278(r1)` @0x82409FC4) and passes its ADDRESS
    // (`addi r4, r1, 0x300+var_278` @0x8240A034) -- which is PfxHelper::Parameters, the shape the
    // DWARF gives the constructor (rwgpfxhelper.h:110). It was previously spelled as a one-element
    // `rw::IResourceAllocator*` array; same address, wrong type.
    rw::graphics::postfx::PfxHelper::Parameters lHelperParameters;
    lHelperParameters.allocator = lpAllocator;
    {
        // HOST sizeof, not the console's 0x50: PfxHelper is placement-new'd into this block.
        const rw::Resource lHelperResource = AllocatePostFxResource(
            lpAllocator,
            static_cast<u32>(sizeof(rw::graphics::postfx::PfxHelper)),   // X360: 0x50
            0x10u);
        if (lHelperResource.m_baseResources[0] != nullptr)
        {
            new (lHelperResource.m_baseResources[0])
                rw::graphics::postfx::PfxHelper(lHelperParameters);
        }
    }

    // ---- depth of field (asm 0x8240A03C-0x8240A14C) --------------------------------------------
    // DepthOfField::Parameters' only member is a State, so the console builds the eight floats on
    // the stack and hands their address straight to the constructor.
    rw::graphics::postfx::DepthOfField::Parameters lDofParameters;
    lDofParameters.m_state.m_focalPlanes[0] = KF_DOF_FOCAL_PLANE_0;
    lDofParameters.m_state.m_focalPlanes[1] = KF_DOF_FOCAL_PLANE_1;
    lDofParameters.m_state.m_focalPlanes[2] = KF_DOF_FOCAL_PLANE_2;
    lDofParameters.m_state.m_focalPlanes[3] = KF_DOF_FOCAL_PLANE_3;
    lDofParameters.m_state.m_projNearPlane  = KF_DOF_PROJ_NEAR;
    lDofParameters.m_state.m_projFarPlane   = KF_DOF_PROJ_FAR;
    lDofParameters.m_state.m_dofAmount      = KF_DOF_AMOUNT;
    lDofParameters.m_state.m_blurRadius     = KF_DOF_BLUR_RADIUS;
    {
        // HOST sizeof, not the console's 0x30: DepthOfField is placement-new'd into this block.
        const rw::Resource lDofResource = AllocatePostFxResource(
            lpAllocator,
            static_cast<u32>(sizeof(rw::graphics::postfx::DepthOfField)),   // X360: 0x30
            0x10u);
        m_pfxDof = (lDofResource.m_baseResources[0] != nullptr)
            ? new (lDofResource.m_baseResources[0])
                  rw::graphics::postfx::DepthOfField(lDofParameters)
            : nullptr;
    }

    // ---- the vignette (asm 0x8240A150-0x8240A1F0) -----------------------------------------------
    // Vignette::Parameters::Parameters() seeds the block; only the leading allocator slot is then
    // overwritten (`stw r28, 0x300+var_100(r1)` @0x8240A160 -- var_100 IS the block's first word,
    // and DWARF rwgpfxvignette.h:89 names that member m_allocator).
    //
    // CORRECTED 2026-08-15: the constructor takes `const Parameters&` (DWARF rwgpfxvignette.h:139),
    // and the block is passed whole. The previous spelling handed it `&lVignetteParameters.m_allocator`
    // against a `rw::IResourceAllocator**` declaration, and the constructor then recovered the initial
    // State as `*(const State*)(lppParameters + 1)` -- which on x64 is base+8 while Parameters::m_state
    // sits at base+16 (Vector4 is alignas(16)). That read eight bytes of padding plus three quarters of
    // the real state into the vignette's shader constants. The console's own `addi r4, r22, 0x10`
    // @0x824041A0 is the +0x10 this now respects.
    rw::graphics::postfx::Vignette::Parameters lVignetteParameters;
    lVignetteParameters.m_allocator = lpAllocator;
    {
        // HOST sizeof, not the console's 0xD0: Vignette is placement-new'd into this block.
        const rw::Resource lVignetteResource = AllocatePostFxResource(
            lpAllocator,
            static_cast<u32>(sizeof(rw::graphics::postfx::Vignette)),   // X360: 0xD0
            0x10u);
        m_pfxVignette = (lVignetteResource.m_baseResources[0] != nullptr)
            ? new (lVignetteResource.m_baseResources[0])
                  rw::graphics::postfx::Vignette(lVignetteParameters)
            : nullptr;
    }

    // ---- the shared tint pixel program + the two tint buffers (asm 0x8240A1F4-0x8240A2BC) -------
    //
    // ⚠ ONE CONSOLE STORE IS DELIBERATELY NOT REPRODUCED, and it is recorded rather than faked:
    //     `dword_82FAEE88 = *(off_82FAEE80 + 19);`   (asm 0x8240A1F4-0x8240A208)
    // caches the PfxHelper's member at +0x4C -- the shared opaque blend state PfxHelper::CreateStates
    // @0x82402D88 stores there -- into a SECOND file-scope global. THAT GLOBAL IS WRITE-ONLY IN THE
    // SHIPPED BINARY: scanning the assembly of all 30,095 function exports for 82FAEE88 returns
    // exactly two instructions, the `lis` and the `stw` on these two lines, and no load anywhere. So
    // there is no consumer to be faithful to; minting a host global for it would be an invention with
    // no reader. Recorded in REPORT.md rather than silently dropped.
    rw::graphics::postfx::Tint::InitializePixelProgram(lpAllocator);

    rw::graphics::postfx::Tint::Parameters lTintParameters;
    lTintParameters.muSize      = KU_TINT_LOOKUP_SIZE;
    lTintParameters.mpAllocator = lpAllocator;
    for (u32 luTint = 0u; luTint < KU_POST_FX_NUM_TINT_BUFFERS; ++luTint)
    {
        // Tint::Initialize takes the carved resource by reference and constructs into its slot 0,
        // so the null test the other four effects carry is (faithfully) absent here.
        // HOST sizeof, not the console's 0x100: Tint::Initialize constructs into slot 0 of this
        // block. The 0x80 is an ALIGNMENT and is passed verbatim -- it does not widen.
        const rw::Resource lTintResource = AllocatePostFxResource(
            lpAllocator,
            static_cast<u32>(sizeof(rw::graphics::postfx::Tint)),   // X360: 0x100
            0x80u);
        m_pfxTint[luTint] = rw::graphics::postfx::Tint::Initialize(lTintResource, lTintParameters);
    }

    // ---- the tint-blend job (asm 0x8240A2C0-0x8240A2F0) -----------------------------------------
    // The console calls EA::Jobs::EntryPoint::SetCode / SetName on `job + 4`, i.e. on the job's
    // embedded EntryPoint (job.h:142, mEntryPoint at +0x04), not through Job's own forwarders.
    m_blendJob.Clear();
    m_blendJob.GetEntryPoint().SetCode(static_cast<EA::Jobs::JobEnvironment>(0),
                                       reinterpret_cast<const void*>(&TintBlendEntry),
                                       KI_TINT_BLEND_JOB_CODE_SIZE);
    m_blendJob.GetEntryPoint().SetName("TintBlend");

    // ---- the B4 blur (asm 0x8240A2F4-0x8240A3CC) ------------------------------------------------
    // The console default-constructs a B4Blur::State on the stack, copy-assigns it into the
    // parameter block's state slot (the twelve-doubleword `ld`/`std` loop @0x8240A314), then fills
    // the block's two leading words: the allocator, and `dword_83010F74` (asm 0x8240A368) --
    // which DWARF rwgpfxb4blur.h:129 names Parameters::m_scatterBlendState, a
    // renderengine::BlendState*.
    //
    // m_scatterBlendState: dword_83010F70 is the base of CgsBlendStateFactory::saBlendStates,
    // mapped slot by slot in CgsBlendStateFactory.h:150-159, so dword_83010F74 (`lwz` @0x8240A368)
    // is saBlendStates[1] == E_FACTORY_BLEND_STATE_TRANSPARENT_MODULATE_NO_ALPHA_TEST_DEST_RGBA,
    // filled by CgsBlendStateFactory::Construct @0x827EB2D8 -- which the PC bring-up now runs (the
    // factory is a real member of BrnRendererModule since the gate-flip wave). Nameable through the
    // factory's static GetState (2026-08-15 verify pass: it had been left INDETERMINATE -- B4Blur::
    // Parameters::Parameters() writes nothing, faithfully -- and copied into +0x108 as garbage).
    rw::graphics::postfx::B4Blur::Parameters lB4BlurParameters;
    lB4BlurParameters.m_allocator         = lpAllocator;
    lB4BlurParameters.m_scatterBlendState =
        CgsBlendStateFactory::GetState(E_FACTORY_BLEND_STATE_TRANSPARENT_MODULATE_NO_ALPHA_TEST_DEST_RGBA);
    lB4BlurParameters.m_state             = rw::graphics::postfx::B4Blur::State();
    {
        // HOST sizeof, not the console's 0x130: B4Blur is placement-new'd into this block.
        // ⚠ THE "EMPTY CLASS" NOTE THAT USED TO STAND HERE IS RESOLVED. rwgpfxb4blur.h now models
        // all 29 members (DWARF :254-308, every offset confirmed against the constructor's store map
        // and summing to the console's own 0x130), so this carve is a real object again -- which is
        // exactly the self-correction the host-sizeof idiom exists for.
        const rw::Resource lB4BlurResource = AllocatePostFxResource(
            lpAllocator,
            static_cast<u32>(sizeof(rw::graphics::postfx::B4Blur)),   // X360: 0x130
            0x10u);
        m_pfxB4Blur = (lB4BlurResource.m_baseResources[0] != nullptr)
            ? new (lB4BlurResource.m_baseResources[0])
                  rw::graphics::postfx::B4Blur(lB4BlurParameters)
            : nullptr;
    }

    // asm 0x8240A3DC `stw r30, 0x380(r29)` with r30 == 1. THE COMPOSITE'S MASTER LATCH.
    // ⚠ CORRECTION TO THE STEP BRIEF: BrnPostFx::Render does NOT gate on the enabled-fx word. Its
    // gate is `lwz r11, 0x380(r31); cmplwi; beq` @0x8240A4F8-0x8240A500, i.e. THIS member. The
    // enabled-fx word is +0x384 and is only ever read for the individual effect bits. So the
    // composite runs with every effect bit clear -- which is exactly the permutation-0
    // pass-through the spine wants.
    mnActiveLayerCount = 1u;

    // ---- the motion-blur state (asm 0x8240A3D0-0x8240A448) --------------------------------------
    // Eight `lvx128`/`stvx128` pairs writing the four identity basis vectors
    // (w::math::vpu::detail::gIVector and unk_82181510/20/30) into this+0x450 and this+0x490, then
    // `stw r30, 0x80(r11)` == meQuality = 1 (MotionBlurState::E_QUALITY_EXPENSIVE). That is
    // MotionBlurState::Construct (DWARF BrnPostFxShader.h:53) INLINED; AGENTS.md requires inlining
    // reversal, so the call is restored rather than the sixteen stores replicated.
    mMotionBlurState.Construct();

    // ---- the shader array and the bloom sub-object (asm 0x8240A44C-0x8240A45C) -------------------
    // `mr r3, r29` -- the shader's `this` is the BrnPostFx object itself, because mPostFxShader is
    // its first member at offset 0. Reached by name here; no offset survives.
    mPostFxShader.Construct(lpAllocator);
    mpAllocator = lpAllocator;
    mBloom.Construct(lpAllocator);
}

// ================================================================================================
// BrnPostFx::Destruct @ 0x82408138   (goes into b5-decomp/src/GameSource/Graphics/PostFx/BrnPostFx.cpp)
//
// Ground truth: the X360 ARTIST ASSEMBLY at 0x82408138-0x82408268 (dossier H1).
//
// The mirror of Construct. THE WHOLE BODY IS FOUR INLINED TEARDOWNS plus two out-of-line calls; the
// compiler flattened them and AGENTS.md requires the inlining to be reversed, so each is written as
// the call the original source made. The evidence for each boundary is given at the call.
// ================================================================================================

void BrnPostFx::Destruct()
{
    // ---- the two tint buffers (asm 0x8240814C-0x82408198) ---------------------------------------
    // The flattened trio per tint is
    //     renderengine::Texture::Release(m_textureTintMap);            (`lwz r3, 0xC0(r31)`)
    //     m_allocator->DoFree(m_textureTintMapResource);               (`lwz r3, 0xF0(r31)`, `addi r4, r31, 0xC8`)
    //     m_allocator->DoFree(m_textureStateTintMapResource);          (`addi r4, r31, 0xDC`)
    // -- i.e. exactly the three resources rw::graphics::postfx::Tint owns, freed through the Tint's
    // OWN allocator. That is Tint::Release() (DWARF rwgpfxtint.h:77) inlined; the three offsets
    // 0xC0 / 0xC8 / 0xDC are the committed rwgpfxtint.h members m_textureTintMap /
    // m_textureTintMapResource / m_textureStateTintMapResource, and 0xF0 is m_allocator.
    // Restoring the call also keeps this TU off three private members it has no business reaching.
    // [FLAG PC bring-up] The five sub-object tests below are NOT the console's -- its carves cannot
    // fail, so 0x8240814C-0x82408204 dereferences every one straight through -- but Construct above
    // already tests each carve and leaves the pointer null on a pool shortfall (host sizeof carves
    // from a bring-up allocator), and a null here would turn a missing effect into a teardown crash.
    // Mirrors Construct's own conditionals; the console order and calls are unchanged. (Verify pass
    // 2026-08-15.)
    for (u32 luTint = 0u; luTint < KU_POST_FX_NUM_TINT_BUFFERS; ++luTint)
    {
        if (m_pfxTint[luTint] != nullptr)
            m_pfxTint[luTint]->Release();
    }

    // ---- the depth-of-field pixel program (asm 0x8240819C-0x824081F4) ---------------------------
    // Same shape: release the DoF object's first member (m_depthOfFieldProgram), free the one-slot
    // rw::Resource it was carved into -- through the POST-FX HELPER SINGLETON's allocator, not the
    // DoF's (`lwz r29, off_82FAEE80@l(r27)` then `lwz r3, 0(r29)`) -- and null the member. That is
    // rw::graphics::postfx::DepthOfField::Release() inlined (the DWARF lists Release among the
    // methods that live in the DoF TU; rwgpfxdof.h in this tree records the same).
    if (m_pfxDof != nullptr)
        m_pfxDof->Release();

    // ---- the vignette and the shared helper (asm 0x824081F8-0x82408204) --------------------------
    // Both already out-of-line on the console.
    if (m_pfxVignette != nullptr)
        m_pfxVignette->Release();
    if (rw::graphics::postfx::gpPfxHelper != nullptr)
        rw::graphics::postfx::gpPfxHelper->Release();

    // ---- the shader array (asm 0x82408208-0x82408210) --------------------------------------------
    // `mr r3, r28` / `lwz r4, 0x378(r28)` -- the shader's `this` is the BrnPostFx object (first
    // member at offset 0) and the argument is mpAllocator. Both reached by name.
    mPostFxShader.Destruct(mpAllocator);

    // ---- the bloom sub-object (asm 0x82408214-0x82408260) ----------------------------------------
    // The FOURTH inlined teardown, and the one whose boundary the binary spells out: the assert it
    // fires cites
    //     "d:\p4\b5_main\burnout\main\code\gamesource\unity\../Graphics/PostFx/BrnPostFxBloom.cpp", 265
    // so these three operations were written in BrnPostFxBloom.cpp, not here. They are
    //     CGS_ASSERT(mpAllocator != NULL, "mpAllocator");
    //     renderengine::VertexDescriptor::Release(mpBloomVertexDescriptor);   (`lwz r3, 0x18(r31)`)
    //     mpAllocator->DoFree(mBloomVertexDescriptorResource);                (`addi r4, r31, 4`)
    // and 0x00 / 0x04 / 0x18 off this+0x970 are exactly BrnPostFxBloom's mpAllocator /
    // mBloomVertexDescriptorResource / mpBloomVertexDescriptor (BrnPostFxBloom.h:29-31). Restored as
    // the call, with the body landed in BrnPostFxBloom.cpp beside its sibling Construct.
    mBloom.Destruct();
}

// ================================================================================================
// BrnPostFx::PrepareDownSampleBuffers @ 0x82408E88
//   (goes into b5-decomp/src/GameSource/Graphics/PostFx/BrnPostFx.cpp)
//
// Ground truth: the X360 ARTIST ASSEMBLY at 0x82408E88-0x82408F04 (dossier H1). Thirty
// instructions; every one is accounted for below.
//
// SIGNATURE. DWARF BrnPostFx.h:259 gives PrepareDownSampleBuffers(BrnRendererMemory&,
// RenderTarget*, float32_t) and the asm agrees: r3 this, r4 the memory pool, r5 the result target,
// f1 the scalar. Hex-Rays models the duplication below as a phantom fourth parameter `double a4`
// plus a local `v5`; there is no fourth parameter.
//
// ⚠ WHAT `fmr f2, f1` @0x82408E98 IS, AND WHAT IT IS NOT. It runs ONCE AT ENTRY, before either
// branch, and it exists for the BLOOM call alone: BrnPostFxBloom::Render takes two floats
// (threshold, white level), and the single incoming white level has to be parked in f2 before
// `lfs f1, 0x960(r31)` @0x82408EC4 overwrites f1 with mBloomData.mfThreshold. It is NOT evidence
// that the DoF callee takes floats. The DoF branch (0x82408EE4-0x82408EFC) sets r3/r4/r5/r6 and
// touches no FPR at all; f1-f13 are volatile on this ABI, this function's prologue is
// `__savegprlr_29` with no FPR save area, and BrnPostFxBloom::Render has already clobbered f1/f2
// whenever the bloom branch ran -- so no float could be live there even in principle.
// DownSampleAndGaussianBlur therefore takes exactly the DWARF's THREE render targets. (An earlier
// draft of this wave declared five parameters on that misread; it is corrected here and in edit 05.)
//
// WHICH SCALAR IT IS. BrnPostFx::Render passes `fmr f1, f31` @0x8240A528, and f31 is Render's THIRD
// float argument -- lfWhiteLevel (see the Render body's argument map). The committed header already
// names the parameter lfWhiteLevel; the asm confirms it.
//
// WHICH POOL SLOTS. The three `lwz` displacements off r30 index BrnRendererMemory::mapRenderTarget,
// whose committed enum (BrnRendererMemory.h:54-65) reads them off directly:
//     0x18 -> [6] E_RENDER_TARGET_BLOOM        0x1C -> [7] E_RENDER_TARGET_DEPTH_OF_FIELD
//     0x20 -> [8] E_RENDER_TARGET_WORK
// and the +0x108 that follows each is CgsRenderTarget::GetRenderTarget(), the postfx RenderTarget
// the whole file already reaches by that name.
//
// ⚠ NEITHER BRANCH IS NULL-GUARDED, AND THAT IS FAITHFUL -- the asm has no test. Since the rung-3/4
// bring-up (2026-08-15) PCBringUpCreatePostFxSceneTargets fills all three of those pool slots
// (BrnRendererMemory.cpp: CreateBloomBuffer / CreateDepthOfFieldBuffer / CreateBackBuffer), so the
// dereferences are live-safe; the branches are still not TAKEN on this build because m_enabledFx is
// 0 and nothing calls Set{Bloom,DepthOfField} -- the gate that keeps them quiet is the caller's, not
// a guard smuggled into a console body. See REPORT.md.
// ================================================================================================

void BrnPostFx::PrepareDownSampleBuffers(BrnRendererMemory& lrAllocatedMemory,
                                         rw::graphics::postfx::RenderTarget* lpResultRt,
                                         f32 lfWhiteLevel)
{
    // asm 0x82408EA4-0x82408EB0: `lwz r11, 0x384(r31)` / `rlwinm r11, r11, 0,30,30` -- mask bit 30
    // in PPC MSB numbering == value bit 1 == 0x02 == E_FX_BLOOM.
    if ((m_enabledFx & E_FX_BLOOM) != 0u)
    {
        // r4 = BLOOM slot's RenderTarget, r5 = WORK slot's, r6 = lpResultRt,
        // f1 = mBloomData.mfThreshold (`lfs f1, 0x960(r31)`), f2 = lfWhiteLevel.
        // The committed BrnPostFxBloom::Render parameter names spell that
        // (bloom, intermediate, source, threshold, whiteLevel).
        mBloom.Render(lrAllocatedMemory.GetBloomBuffer()->GetRenderTarget(),
                      lrAllocatedMemory.GetWorkBuffer()->GetRenderTarget(),
                      lpResultRt,
                      mBloomData.mfThreshold,
                      lfWhiteLevel);
    }

    // asm 0x82408ED4-0x82408EE0: `clrlwi r11, r11, 31` == value bit 0 == 0x01 == E_FX_DEPTH_OF_FIELD.
    // The word is RE-LOADED from this+0x384 rather than reused, because BrnPostFxBloom::Render may
    // have changed it; reproduced by reading the member again rather than caching it.
    if ((m_enabledFx & E_FX_DEPTH_OF_FIELD) != 0u)
    {
        // r3 = m_pfxDof (`lwz r3, 0x5B8(r31)`), r4 = DEPTH_OF_FIELD slot's RenderTarget (dest),
        // r5 = lpResultRt (source), r6 = WORK slot's RenderTarget (work buffer). NO float arguments
        // -- see the banner. The DWARF names the three parameters destRenderTarget /
        // sourceRenderTarget / workBufferTarget, in that order.
        m_pfxDof->DownSampleAndGaussianBlur(
            lrAllocatedMemory.GetDepthOfFieldBuffer()->GetRenderTarget(),
            lpResultRt,
            lrAllocatedMemory.GetWorkBuffer()->GetRenderTarget());
    }
}

// ================================================================================================
// BrnPostFx::Render @ 0x8240A468   (goes into b5-decomp/src/GameSource/Graphics/PostFx/BrnPostFx.cpp)
//
// Ground truth: the X360 ARTIST ASSEMBLY at 0x8240A468-0x8240A774 (dossier H1), cross-read against
// the CALL SITE in BrnRendererModule::Render @0x8240BFA8 (asm 0x8240DE0C-0x8240DE3C) and against
// the CALLEE BrnPostFxShader::Render @0x82408F08 (dossier H3).
//
// ------------------------------------------------------------------------------------------------
// 1. THE SIGNATURE, AND THE 57-ARGUMENT MYTH
// ------------------------------------------------------------------------------------------------
// Hex-Rays prints 35 parameters for this function and 59 for its callee. Both are wrong, and the
// step brief's "BrnPostFx::Render takes 57 arguments" is the same artefact. THE REAL LISTS ARE
// SHORT, and they are pinned three ways that agree exactly:
//
//   (a) DWARF BrnPostFx.h:117 --
//         Render(BrnRendererMemory&, RenderTarget*, RenderTarget*, Vector4, bool8_t,
//                float32_t, float32_t, float32_t, float32_t, renderengine::Texture*)
//   (b) the CALLER writes exactly r3..r7, f1..f4, v1 and ONE stack word:
//         r3 = r20                     the BrnPostFx object
//         r4 = r31 + 0x238             BrnRendererModule::mAllocatedRenderTargets
//         r5 = *(r27 + 0x108)          mapRenderTarget[4] DOWN_SAMPLE  -> GetRenderTarget()
//         r6 = *(r29 + 0x108)          mapRenderTarget[5] BACK_BUFFER  -> GetRenderTarget()
//         r7 = var_CD0 (byte)          the motion-blur-active flag
//         v1 = var_CA0                 the 2D tint colour Vector4
//         f1 = brightness*0.01f - 0.5f  f2 = contrast*0.01f + 0.5f   (flt_82002138 / flt_82001DA0)
//         f3 = f26 = the frame white level      f4 = *(r31 + 0xC408) = the aspect correction
//         [r1 + 0x6C] = r30            the calibration Texture* (the override source)
//   (c) THE STACK OFFSET CLOSES THE PROOF. The caller stores the last argument at its r1+0x6C and
//       this function reads `lwz r11, 0x1B0+arg_6C(r1)` == its own r1 + 0x21C == caller r1 + 0x6C
//       after `stwu r1, -0x1B0`. Same word. Nothing else is passed.
//
// ⚠ WHY THE PPC FLOAT RULE MATTERS HERE, concretely. The parameter save area is DOUBLEWORD-slotted
// on this ABI and EVERY float argument still burns its slot. That is why r4, r9, r10 and r11 are
// used as pure SCRATCH in the flag chain at 0x8240A650-0x8240A6E8 and then spilled to the stack: r4
// is the skipped slot of the callee's FIRST parameter (a float), r9/r10 the skipped slots of two
// more. Reading `mr r4, r28` as "argument 2" is the trap; r4 is dead by the time the call happens.
//
// ------------------------------------------------------------------------------------------------
// 2. THE CALLEE'S REAL SIGNATURE (21 parameters), decoded from the caller's stores
// ------------------------------------------------------------------------------------------------
// DWARF BrnPostFxShader.h:113 gives BrnPostFxShader::Render 21 parameters. Every stack argument this
// function writes lands on the doubleword slot that parameter occupies, so the map is exact:
//
//   slot   offset off caller r1   written by                    parameter
//   ----   -------------------    -------------------------     ---------------------------------
//    0     0x08 (r3)              `mr r3, r31`                   this  (== the BrnPostFx object)
//    1     0x10 (r4, SKIPPED)     f1 = f31                       float32_t  white level
//    2     0x18 (r5)              `addi r5, r31, 0x950`          const BrnPostFxBloomData&
//    3     0x20 (r6)              `addi r6, r31, 0x390`          const Vignette::State&
//    4     0x28 (r7)              `addi r7, r31, 0x3D0`          const DepthOfField::State&
//    5     0x30 (r8)              `addi r8, r31, 0x450`          const MotionBlurState&
//    -     0x40..0x4F (v1)        `vmr128 v1, v126`              Vector4  tint colour
//    -     0x50 (r9, SKIPPED)     f2 = f30                       float32_t  brightness
//    -     0x58 (r10, SKIPPED)    f3 = f29                       float32_t  contrast
//          0x60  (byte at 0x67)   `stb r28`                      bool8_t  depth of field
//          0x68  (byte at 0x6F)   `stb r11`                      bool8_t  bloom
//          0x70  (byte at 0x77)   `stb r10`                      bool8_t  tint
//          0x78  (byte at 0x7F)   `stb r9`                       bool8_t  vignette
//          0x80  (byte at 0x87)   `stb r4`                       bool8_t  motion blur
//          0x88  (word at 0x8C)   `stw r23`                      Texture*  source colour
//          0x90  (word at 0x94)   `stw r27`                      Texture*  bloom
//          0x98  (word at 0x9C)   `stw r26`                      Texture*  depth of field
//          0xA0  (word at 0xA4)   `stw r24`                      const TextureState*  tint map
//          0xA8  (word at 0xAC)   `stw r25`                      const TextureState*  source
//    -     0xB0..0xBF (v2)        `vmr128 v2, v127`              Vector2  the half-texel offset
//    -     0xC0 (SKIPPED)         f4 = f28                       float32_t  aspect correction
//          0xC8  (byte at 0xCF)   `stb r29`                      bool8_t  source is an override
//
// TWO INDEPENDENT CHECKS THAT THE FIVE FLAGS ARE IN THAT ORDER, not merely in that count:
//   * BrnPostFxShader::Render reads `lbz r11, 0x450+arg_6F` (== caller 0x6F, the BLOOM slot) to gate
//     the block that computes `BloomData.mfLuminance * (1/whiteLevel)` and scales BloomData.mColour
//     by it -- that is the BloomColour constant, so slot 0x68 is bloom.
//   * It reads `lbz r11, 0x450+arg_7F` (== caller 0x7F, the VIGNETTE slot) to gate the block that
//     raises a value to the 16th power and scales by 500 -- which is verbatim the vignette
//     sharpness gradient rwgpfxvignette.h:50-51 documents (mul = s*500+1, add = -s*500).
//
// AND THAT SETTLES THE PERMUTATION AXES the conductor's ANALYSIS.md left open. The index arithmetic
// there is `index = 4*blurTerm + 2*flagA + flagB` with flagA = a33, flagB = a37, blurTerm from a41;
// under the map above a33 is the DEPTH-OF-FIELD flag, a37 is the TINT flag and a41 is MOTION BLUR:
//         index = 4 * (motionBlur ? meQuality + 1 : 0) + 2 * depthOfField + 1 * tint
// which is exactly consistent with ANALYSIS's reading of the interned constant names -- Sampler3dTint
// present in the six ODD indices (the colour-cube tint), DofParamsA/B in the alternating pairs.
// PERMUTATION 0 == depth of field OFF, tint OFF, motion blur OFF, which is what this build produces.
//
// ------------------------------------------------------------------------------------------------
// 3. WHAT THE BODY DOES
// ------------------------------------------------------------------------------------------------
// A thin driver: drain the tint-blend job, gate, push two cached render states, prepare the
// down-sample chain, build the half-texel vector, open the destination target, run the composite,
// resolve. It computes NO shader constants -- BrnPostFxShader::Render does all of that.
// ================================================================================================

namespace
{
    // The half-texel numerator: flt_82001DA0 == 0.5f, `lfs f0, flt_82001DA0@l(r10)` @0x8240A58C.
    // Same symbol, same value as BrnRendererModule.cpp's KF_HALF_PIXEL and as
    // rw::graphics::postfx::RenderTarget::GetHalfPixelOffset @0x823FE668, which builds the identical
    // (0.5/width, 0.5/height, 0, 0) vector from the identical two members.
    const f32 KF_POST_FX_HALF_TEXEL = 0.5f;

    // The reciprocal's numerator: flt_82001C98 == 1.0f, `lfs f0, flt_82001C98@l(r10)` @0x8240A604.
    // The SAME register is reused at 0x8240A640 as the white level forced when an override source
    // texture is supplied -- one constant, two uses, which is why it is named once.
    const f32 KF_POST_FX_ONE = 1.0f;

    // The zero the half-texel vector's z/w lanes are seeded with: flt_82001CC0 == 0.0f,
    // `lfs f0, flt_82001CC0@l(r10)` @0x8240A56C, stored to var_C8 and var_C4 @0x8240A578/0x8240A580.
    // A DUMPED zero read off a cited displacement, not a placeholder.
    const f32 KF_POST_FX_ZERO = 0.0f;
}

// THE TWO CACHED RENDER STATES THIS FUNCTION PUSHES -- CLOSED, and the two undefined
// `gpPostFx*State` externs that used to be declared here are DELETED. Both words are slots of the
// two state-factory tables, which this tree owns; there is no such global on the console. The full
// identification is in the banner over BrnPostFx::Construct above, and the evidence for the tables
// themselves is in CgsDepthStencilStateFactory.h / CgsRasterizerStateFactory.h.
//
//   dword_83010910 (asm 0x8240A504-0x8240A510, handed to shadow::Device::SetState(DepthStencilState*)
//   @0x82276AD0) == CgsDepthStencilStateFactory::saDepthStencilStates[1] ==
//   E_FACTORY_DEPTH_STENCIL_STATE_ZOFF_ZALL_ZWRITEOFF.
//
//   dword_83010A40 (asm 0x8240A514-0x8240A520, handed to shadow::Device::SetState(RasterizerState*)
//   @0x82276B38) == CgsRasterizerStateFactory::saRasterizerStates[2] ==
//   E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_NONE.
//
// ⚠ WHAT THIS DOES *NOT* FIX, stated so it is not mistaken for fixed: NOTHING ON THIS BUILD CALLS
// EITHER FACTORY'S Construct (BrnRendererModule::Construct does not reach mDepthStencilStateFactory
// / mRasterizerStateFactory, and neither factory TU is on tools/build/build_game_exe.bat), so both
// tables read NULL at runtime today. Both SetState overloads are the shadow cache's
// compare-then-apply, which stores and skips on a null exactly as it would on any other pointer --
// i.e. the console's own "state unchanged" path, not a crash. Retiring this is the same one-line
// change shadowingdevice.cpp:546-554 already names for saBlendStates: have
// BrnRendererModule::Construct call the three factories' Construct.

void BrnPostFx::Render(BrnRendererMemory& lrAllocatedMemory,
                       rw::graphics::postfx::RenderTarget* lpSourceRenderTarget,
                       rw::graphics::postfx::RenderTarget* lpDestinationRenderTarget,
                       rw::math::vpu::Vector4 lTintColour,
                       bool lbMotionBlurEnabled,
                       f32 lfBrightness, f32 lfContrast, f32 lfWhiteLevel, f32 lfAspectCorrection,
                       renderengine::Texture* lpOverrideSourceTexture)
{
    // ---- 1. drain the colour-cube blend job (asm 0x8240A4B4-0x8240A4F4) --------------------------
    // m_processTint (X360 +0x5F4) is set by BeginTintBlend when it schedules the blend. If it is
    // still set, the blend is in flight over a LOCKED tint-map surface: clear the flag, block until
    // the job finishes, and unlock the surface before anything samples it.
    //
    // The unlock is `renderengine::Texture::Unlock(tint->m_textureTintMap, &tint->m_textureLock)`
    // inlined (`lwz r3, 0xC0(r11)` / `addi r4, r11, 0xA4` -- the committed rwgpfxtint.h members) and
    // is restored as the call the original source made: Tint::EndBlendJob (DWARF rwgpfxtint.h:98),
    // the exact counterpart of the BeginBlendJob this tree already declares.
    //
    // The tint index is `m_currentTintBuffer` (X360 +0x5FC), reached through the same
    // `m_pfxTint[m_currentTintBuffer]` expression the composite's texture gather uses below --
    // on the console both are the `(index + 0x170) * 4` word-index pun off `this`, which does not
    // survive to the host and is not carried here.
    // INLINING REVERSAL (post-fx step 10): the flattened unlock IS BrnPostFx::EndTintBlend, which
    // the DecFIGS DWARF places at BrnPostFx.cpp:288 with exactly one statement in it
    // (references/DecFIGS/dwarfdump/GameSource/Graphics/PostFx/BrnPostFx.cpp:17,
    //  `rw::graphics::postfx::Tint::EndBlendJob(...)`). AGENTS.md requires the call to be restored,
    // so the two members it reaches are back behind their own method.
    if (m_processTint)
    {
        m_processTint = false;
        m_blendJob.WaitOn();
        EndTintBlend();
    }

    // ---- 2. the master gate (asm 0x8240A4F8-0x8240A500) ------------------------------------------
    // ⚠ THE GATE IS mnActiveLayerCount (X360 +0x380), NOT the enabled-fx word (+0x384). BrnPostFx::
    // Construct sets it to 1 as its last act before building the shader array; the ctor leaves it 0.
    // So this is "is the post-fx driver built", and the composite runs with every effect bit clear.
    // (The step brief says "gate on the enabled-fx word"; the asm says otherwise and the asm wins.)
    if (mnActiveLayerCount == 0u)
    {
        return;
    }

    // ---- 3. the two cached render states (asm 0x8240A504-0x8240A520) ------------------------------
    // Depth test off / no depth write, and the post-fx rasteriser state. Both go through the shadow
    // cache's compare-and-skip setters, so a repeat push costs nothing. The two slots are named
    // rather than fetched from an invented global -- see the banner above for why. Both factories
    // (and the blend one) are Constructed once in BrnRendererModule::Render's deferred PC bring-up,
    // right after EnsurePostFxSceneTargets; the boot log line "[postfx-composite] the three
    // render-state factories are Constructed" is the proof, and the composite diag's cull=1/scissor=1
    // z=0/zw=0 shows the two slots applied.
    shadow::Device::SetState(
        CgsDepthStencilStateFactory::GetState(E_FACTORY_DEPTH_STENCIL_STATE_ZOFF_ZALL_ZWRITEOFF));
    shadow::Device::SetState(
        CgsRasterizerStateFactory::GetState(E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_NONE));

    // ---- 4. the down-sample chain (asm 0x8240A524-0x8240A534) -------------------------------------
    // ⚠ THE RESULT TARGET IS THE *SOURCE*, not the destination: `mr r5, r29` where r29 is this
    // function's third argument. Bloom and depth of field both read the scene and write the pool's
    // bloom / DoF / work slots; the composite then samples those. And the white level handed down is
    // the ARGUMENT, before the override-source override applied in step 6 -- the order of the two is
    // load-bearing and is preserved (PrepareDownSampleBuffers is called at 0x8240A534, `fmr f31, f0`
    // is at 0x8240A640).
    PrepareDownSampleBuffers(lrAllocatedMemory, lpSourceRenderTarget, lfWhiteLevel);

    // ---- 5. the source texture and the override flag (asm 0x8240A538-0x8240A54C) ------------------
    // `lwz r11, arg_6C` / `cmplwi` / `bne` / `lwz r23, 0x28(r29)`: use the caller's override texture
    // when it supplied one, else the source render target's colour texture (+0x28 == the postfx
    // RenderTarget's maColourTargets[0].mpTexture, i.e. GetTexture(0)).
    const bool lbOverrideSource = (lpOverrideSourceTexture != nullptr);
    renderengine::Texture* const lpSourceTexture =
        lbOverrideSource ? lpOverrideSourceTexture : lpSourceRenderTarget->GetTexture(0u);

    // ---- 6. the HALF-TEXEL vector (asm 0x8240A54C-0x8240A638) -------------------------------------
    // The console's own screen-space sampling correction, computed here rather than fetched from
    // RenderTarget::GetHalfPixelOffset @0x823FE668 because the compiler inlined that function -- the
    // instruction sequence is identical (`lwz` width/height off +4/+8, `std`/`lfd`/`fcfid`/`frsp` to
    // float, `fdivs` against 1.0f, `vmulfp128` by a splat of 0.5f).
    //
    // ⚠ GETTING THIS WRONG DOES NOT LOOK LIKE A BUG. Without it every bilinear fetch straddles two
    // texels and the whole frame is uniformly, slightly soft -- which reads as "the target format is
    // lossy". BrnRendererModule.cpp's bring-up blit carries the same warning and the same magnitude.
    const f32 lfInverseWidth  = KF_POST_FX_ONE / static_cast<f32>(lpSourceRenderTarget->muWidth);
    const f32 lfInverseHeight = KF_POST_FX_ONE / static_cast<f32>(lpSourceRenderTarget->muHeight);

    rw::math::vpu::Vector2 lHalfTexelOffset;
    lHalfTexelOffset.x = KF_POST_FX_HALF_TEXEL * lfInverseWidth;
    lHalfTexelOffset.y = KF_POST_FX_HALF_TEXEL * lfInverseHeight;
    lHalfTexelOffset.z = KF_POST_FX_ZERO;
    lHalfTexelOffset.w = KF_POST_FX_ZERO;
    // THE FINAL LANE PERMUTATION IS RECOVERED, AND IT IS A NO-OP FOR EVERY LANE ANYONE READS.
    // The asm's last step is `vperm128 v127, v0, v0, v7` @0x8240A638 with the permute control
    // loaded from unk_82CDA350 (@0x8240A5A4/B0). Those sixteen bytes ARE dumped --
    // scratch/postfx_wave1b_dossiers/DATA_DUMP.md:1698 gives
    //     {0x00010203, 0x14151617, 0x00010203, 0x00010203}
    // and the same control word is already spelled out in three committed sources
    // (CgsAxisAlignedBox4.h:68, CgsAABBoxBuilder.cpp:106/114/153, BrnBehaviourRoadRunner.cpp:995).
    // BOTH vperm sources are the SAME register (v0, v0), so byte index 0x00-0x03 selects source
    // lane 0 and 0x14-0x17 selects source lane 1: the result is (du, dv, du, du).
    //   * Lanes 0 and 1 are UNCHANGED -- and lanes 0 and 1 are ALL the callee consumes
    //     (BrnPostFxShader::Render does `vspltw v12, v13, 0` / `vspltw v12, v13, 1` in its vertex
    //     emission and adds them to the quad's u and v respectively).
    //   * Lane 2 diverges harmlessly: the console leaves `du` there, this reconstruction leaves 0.
    //     Nothing reads it. Lane 3 likewise.
    // So the (0.5/width, 0.5/height, 0, 0) built above is the correct vector and the image is NOT
    // softly blurred. This is a RECOVERED fact, not an inference from what the callee happens to
    // read -- an earlier draft of this wave listed 0x82CDA350 as unrecovered data NEEDING A DUMP; it
    // was already in the dossier and that blocker was imaginary.

    // ---- 7. the composite's texture / state gather (asm 0x8240A560-0x8240A5E8) --------------------
    // All five reads are unconditional on the console. The two pool slots are the same
    // mapRenderTarget indices PrepareDownSampleBuffers writes, and +0x8C on a RenderTarget is
    // mpColourTextureState (rwgpfxrendertarget.h).
    //
    // ⚠ UNGUARDED, FAITHFULLY -- and see REPORT.md: on this PC build the BLOOM / DEPTH_OF_FIELD pool
    // slots are empty and m_pfxTint[] is null until Construct runs, so these five reads are exactly
    // why the composite cannot be switched on yet. The guard belongs in the caller's gate.
    renderengine::Texture* const lpBloomTexture =
        lrAllocatedMemory.GetBloomBuffer()->GetRenderTarget()->GetTexture(0u);
    renderengine::Texture* const lpDepthOfFieldTexture =
        lrAllocatedMemory.GetDepthOfFieldBuffer()->GetRenderTarget()->GetTexture(0u);
    const renderengine::TextureState* const lpSourceTextureState =
        lrAllocatedMemory.GetDownSampleBuffer()->GetRenderTarget()->mpColourTextureState;
    const renderengine::TextureState* const lpTintTextureState =
        m_pfxTint[m_currentTintBuffer]->GetTextureState();

    // ---- 8. the override white level (asm 0x8240A63C-0x8240A640) ----------------------------------
    // `beq` past `fmr f31, f0` where f0 still holds flt_82001C98 == 1.0f: when the caller supplied
    // its own source texture (the calibration/photo path), the composite runs at a white level of
    // exactly 1.0 rather than the frame's. AFTER PrepareDownSampleBuffers, never before.
    const f32 lfCompositeWhiteLevel = lbOverrideSource ? KF_POST_FX_ONE : lfWhiteLevel;

    // ---- 9. open the destination (asm 0x8240A644-0x8240A64C) --------------------------------------
    // `li r4, 0` -- slice/face 0, the 2D path.
    lpDestinationRenderTarget->Begin(0u);

    // ---- 10. the five effect flags (asm 0x8240A650-0x8240A6E8) ------------------------------------
    // EVERY ONE is ANDed with "no override source was supplied": the five `cmplwi cr6, r8, 0; beq`
    // pairs all test r30 == (lpOverrideSourceTexture == 0) first. A caller-supplied source image
    // therefore bypasses every effect and gets permutation 0 with a pass-through white level.
    // The bit masks are the `rlwinm` MSB-numbered fields, converted here to the committed EnabledFx
    // names: bit30 -> 0x02 bloom, bit27 -> 0x10 vignette, bit26 -> 0x20 tint, bit31 -> 0x01 DoF.
    const bool lbEffectsAllowed = !lbOverrideSource;
    const bool lbDepthOfField = lbEffectsAllowed && ((m_enabledFx & E_FX_DEPTH_OF_FIELD) != 0u);
    const bool lbBloom        = lbEffectsAllowed && ((m_enabledFx & E_FX_BLOOM)          != 0u);
    const bool lbTint         = lbEffectsAllowed && ((m_enabledFx & E_FX_TINT)           != 0u);
    const bool lbVignette     = lbEffectsAllowed && ((m_enabledFx & E_FX_VIGNETTE)       != 0u);
    const bool lbMotionBlur   = lbEffectsAllowed && lbMotionBlurEnabled;

    // ---- 11. THE COMPOSITE (asm 0x8240A6EC-0x8240A744) --------------------------------------------
    // The one draw. Argument order and types are the 21-parameter map in the banner above.
    // `mr r3, r31` -- the shader's `this` is the BrnPostFx object, mPostFxShader being its first
    // member; reached by name here.
    //
    // THE VISIBLE PAYOFF LIVES IN TWO OF THESE ARGUMENTS: lfBrightness and lfContrast become the
    // composite's `GlobalParams` shader constant, so the options-menu brightness slider takes effect
    // the moment this call is live. BrnRendererModule::Render already reads both off the dispatch
    // buffer (GetBrightness @0x8240DCC8 / GetContrast @0x8240DCFC) and pre-scales them --
    // brightness*0.01f - 0.5f and contrast*0.01f + 0.5f -- so nothing further is done to them here.
    // BOTH constants are RECOVERED, not assumed: flt_82001DA0 == 0.5f is dumped
    // (DATA_DUMP.md:1552), and flt_82002138 == 0.01f is read off
    // BrnDirector::Camera::Utils::Looker::Parameters::Construct @0x821F8D80, where
    // `lfs f11, flt_82002138@l(r10)` @0x821F8D8C feeds `stfs f11, 0x28(r3)` @0x821F8D94 and
    // Hex-Rays prints that store as `*(result + 40) = 0.0099999998;`.
    mPostFxShader.Render(lfCompositeWhiteLevel,
                         mBloomData,
                         m_vignetteState,
                         m_dofState,
                         mMotionBlurState,
                         lTintColour,
                         lfBrightness,
                         lfContrast,
                         lbDepthOfField,
                         lbBloom,
                         lbTint,
                         lbVignette,
                         lbMotionBlur,
                         lpSourceTexture,
                         lpBloomTexture,
                         lpDepthOfFieldTexture,
                         lpTintTextureState,
                         lpSourceTextureState,
                         lHalfTexelOffset,
                         lfAspectCorrection,
                         lbOverrideSource);

    // ---- 12. close the destination (asm 0x8240A748-0x8240A754) ------------------------------------
    // `li r5, 1` / `li r4, 1` -- resolve BOTH the depth/stencil and the colour surface.
    lpDestinationRenderTarget->Resolve(true, true);
}

// ================================================================================================
// [FLAG PC bring-up] PCBringUpRenderPostFxComposite -- the seam BrnRendererModule::Render reaches
// the composite through.  (goes into b5-decomp/src/GameSource/Graphics/PostFx/BrnPostFx.cpp)
//
// This is NOT a decompiled function. The console had no such helper: BrnRendererModule::Render calls
// BrnPostFx::Render directly, in one translation unit. It exists for one measured reason, spelled out
// in full in BrnPostFxPCComposite.h: BrnRendererModule.h still defines a PLACEHOLDER EA::Jobs::Job,
// so BrnRendererModule.cpp cannot include BrnPostFx.h without a C2011 redefinition against the real
// job.h that BrnPostFx.h needs for m_blendJob. Deleted the day that placeholder is retired.
//
// WHAT IT ADDS OVER THE CONSOLE, and nothing else: a NULL TEST ON THE POOL. BrnPostFx::Render
// dereferences the bloom, depth-of-field and back-buffer pool slots and m_pfxTint[] unconditionally,
// faithfully -- the X360 asm at 0x8240A560-0x8240A5E8 has no test. On PC three of those slots are
// empty, so the guard has to live somewhere; AGENTS.md says it does NOT go in the console body, so it
// goes HERE, in the PC gate, and the console body keeps its unguarded reads.
//
// IT DOES NOT HAND THE SWAP CHAIN BACK. That is the caller's, deliberately: the property "the back
// buffer is rebound on every path out of Render" is only checkable if both arms of the decision are
// visible at the call site. See the call site's banner in BrnRendererModule.cpp.
// ================================================================================================

namespace
{
    // Set by PCBringUpConstructPostFx. The seam's own record, not the object's: BrnPostFx keeps
    // mnActiveLayerCount private and the guard belongs to the PC gate, not to the console class.
    bool sbPostFxConstructed = false;
}

// [FLAG PC bring-up] The deferred BrnPostFx::Construct -- see BrnPostFxPCComposite.h. The console
// runs it from BrnRendererModule::Construct (`bl BrnPostFx__Construct` @0x8240B78C, r3 = mPostFxVault,
// r4 = this->mpGraphicsAllocator); on PC the module's Construct has no device and a null graphics
// allocator, so it runs from the frame that builds the post-fx pool, on the bring-up allocator.
void PCBringUpConstructPostFx(rw::IResourceAllocator* lpAllocator)
{
    if (sbPostFxConstructed || lpAllocator == 0)
    {
        return;
    }
    msPostFx.Construct(lpAllocator);
    sbPostFxConstructed = true;
    CgsDev::Log::WriteToLog("[postfx-composite] BrnPostFx::Construct ran (deferred PC bring-up)\n");
}

// [FLAG PC bring-up] WILL THE COMPOSITE ACTUALLY RUN THIS FRAME? -- the precondition, factored out
// of PCBringUpRenderPostFxComposite below so that a SECOND caller can ask it without duplicating it.
//
// That second caller is BrnRendererModule::Render's colour-cube tint block. BrnPostFx::BeginTintBlend
// LOCKS the tint volume texture and the only thing that unlocks it is BrnPostFx::Render's step-1
// drain -- which lives on the far side of this precondition. On the console both sites sit under one
// gate (the mbRenderPostFX byte, tested at 0x8240C6A8 and again at 0x8240DC6C), so a lock cannot
// outlive its frame; on PC the composite has this pool/Construct test as well, and a tint blend
// scheduled on a frame the composite skips would Lock and never Unlock. One function, one answer.
//
// It is a pure query: no logging, no side effect, cheap enough to call twice a frame (five pointer
// loads and a bool).
bool PCBringUpPostFxCompositeWillRun(BrnRendererMemory& lrRendererMemory)
{
    CgsRenderTarget* const lpSceneTarget = lrRendererMemory.GetDownSampleBuffer();
    CgsRenderTarget* const lpBackBuffer  = lrRendererMemory.GetBackBuffer();
    CgsRenderTarget* const lpBloomBuffer = lrRendererMemory.GetBloomBuffer();
    CgsRenderTarget* const lpDofBuffer   = lrRendererMemory.GetDepthOfFieldBuffer();
    CgsRenderTarget* const lpWorkBuffer  = lrRendererMemory.GetWorkBuffer();

    return lpSceneTarget != 0 && lpSceneTarget->GetRenderTarget() != 0 &&
           lpBackBuffer  != 0 && lpBackBuffer->GetRenderTarget()  != 0 &&
           lpBloomBuffer != 0 && lpBloomBuffer->GetRenderTarget() != 0 &&
           lpDofBuffer   != 0 && lpDofBuffer->GetRenderTarget()   != 0 &&
           lpWorkBuffer  != 0 && lpWorkBuffer->GetRenderTarget()  != 0 &&
           sbPostFxConstructed;
}

bool PCBringUpRenderPostFxComposite(BrnRendererMemory& lrRendererMemory,
                                    f32 lfBrightness,
                                    f32 lfContrast,
                                    f32 lfFrameWhiteLevel,
                                    f32 lfAspectCorrection,
                                    const f32* lpafTint2dColourXYZW,
                                    bool lbMotionBlurEnabled,
                                    renderengine::Texture* lpOverrideSourceTexture)
{
    // The console's own source and destination: BrnRendererModule::Render @0x8240BFA8 loads
    // `lwz r27, 0x248(r31)` and `lwz r29, 0x24C(r31)` off mAllocatedRenderTargets at this+0x238,
    // i.e. mapRenderTarget[4] E_RENDER_TARGET_DOWN_SAMPLE and mapRenderTarget[5]
    // E_RENDER_TARGET_BACK_BUFFER, and passes each slot's +0x108 (GetRenderTarget()).
    CgsRenderTarget* const lpSceneTarget = lrRendererMemory.GetDownSampleBuffer();
    CgsRenderTarget* const lpBackBuffer  = lrRendererMemory.GetBackBuffer();

    // ...and the bloom / depth-of-field / work slots the console body samples without a test (asm
    // 0x8240A560-0x8240A5E8: GetBloomBuffer()->GetRenderTarget()->GetTexture(0) /
    // GetDepthOfFieldBuffer()->..., plus the WORK buffer PrepareDownSampleBuffers hands the bloom /
    // DoF passes the moment E_FX_BLOOM or E_FX_DEPTH_OF_FIELD is set). Every slot BrnPostFx::Render
    // dereferences is tested in PCBringUpPostFxCompositeWillRun above -- once, in one place, so the
    // console body keeps its unguarded reads and the renderer's tint block can ask the same
    // question before it schedules a blend only this function's drain can unlock.
    if (!PCBringUpPostFxCompositeWillRun(lrRendererMemory))
    {
        // Nothing drawn, nothing bound, nothing resolved. The caller presents the frame the old way.
        // The !sbPostFxConstructed arm matters as much as the pool: BrnPostFx::Render's first act is
        // `if (mnActiveLayerCount == 0) return` and only Construct sets it, so an un-Constructed
        // singleton would return here having drawn NOTHING while this function reported success --
        // and the caller would present a black world with the GUI on top.
        static bool sbReported = false;
        if (!sbReported)
        {
            sbReported = true;
            CgsDev::Log::WriteToLog(sbPostFxConstructed
                ? "[postfx-composite] composite SKIPPED: the pool lacks a target it needs "
                  "(down-sample / bloom / depth-of-field / back buffer) -- presenting via the blit\n"
                : "[postfx-composite] composite SKIPPED: BrnPostFx::Construct has not run "
                  "-- presenting via the blit\n");
        }
        return false;
    }

    // [DIAG, sampled] what the composite is fed: the 1st / 100th / 1000th call's brightness,
    // contrast, white level and aspect correction. The neutral picture needs 0 / 1 / 1 / 1.
    {
        static u32 suCalls = 0u;
        ++suCalls;
        if (suCalls == 1u || suCalls == 100u || suCalls == 1000u)
        {
            char lacMsg[200];
            std::snprintf(lacMsg, sizeof(lacMsg),
                          "[postfx-composite] call %u: brightness=%g contrast=%g white=%g aspect=%g\n",
                          static_cast<unsigned>(suCalls), (double)lfBrightness, (double)lfContrast,
                          (double)lfFrameWhiteLevel, (double)lfAspectCorrection);
            CgsDev::Log::WriteToLog(lacMsg);
        }
    }

    // The 2D tint, now EVALUATED rather than assumed. BrnRendererModule::Render zeroes the vector
    // (`vspltisw v0, 0` / `stvx128 v0, r0, r11` @0x8240DC80-0x8240DC8C) and overwrites it only when
    // effects are allowed AND the layer-0 internal frame's mbUseTint2d is set, in which case
    // EffectsArbitrator::EvalTint2d fills it; that is what arrives here as lpafTint2dColourXYZW. On
    // this build the shipped tint2d asset ("374388", POSTFXVAULT.BIN) is (0,0,0,0), so the value is
    // still the pass-through neutral -- but it is now the data's zero, not a hard-coded one.
    rw::math::vpu::Vector4 lTintColour;
    if (lpafTint2dColourXYZW != 0)
    {
        lTintColour.x = lpafTint2dColourXYZW[0];
        lTintColour.y = lpafTint2dColourXYZW[1];
        lTintColour.z = lpafTint2dColourXYZW[2];
        lTintColour.w = lpafTint2dColourXYZW[3];
    }
    else
    {
        lTintColour.SetZero();
    }

    // The motion-blur flag is now READ off the layer-0 effects frame, as the console reads it in r7
    // (`lbz r7, 0xD40+var_CD0(r1)` @0x8240DE18 -- the byte Render sampled from mMotionBlurData
    // .mbIsActive at 0x8240C2DC). It is still false on this build, because nothing posts a
    // motion-blur event and MotionBlurData::Construct @0x821F84E8 seeds mbIsActive = false -- which
    // is what keeps the permutation index at 0 (index = 4*(mb ? q+1 : 0) + 2*dof + tint), the only
    // slot with a PC program pair.
    // Through the file-scope singleton, not BrnPostFx::GetInstance(): the console does the same --
    // BrnRendererModule::Render loads the global (`_R20 = mPostFxVault;`, pseudocode line 500) and
    // passes it as r3 at 0x8240DE0C. GetInstance() has no body in this tree, so calling it would
    // also have added an unresolved external for nothing.
    msPostFx.Render(lrRendererMemory,
                    lpSceneTarget->GetRenderTarget(),
                    lpBackBuffer->GetRenderTarget(),
                    lTintColour,
                    lbMotionBlurEnabled,
                    lfBrightness,
                    lfContrast,
                    lfFrameWhiteLevel,
                    lfAspectCorrection,
                    lpOverrideSourceTexture);   // the calibration ramp, or null (BrnRendererModule
                                                // derives it at the console's 0x8240DD50-0x8240DE08)
    return true;
}

// ================================================================================================
// BrnPostFx::BeginTintBlend @ 0x823F8380   (46 lines; the whole body is accounted for below)
//
// Ground truth: the X360 ARTIST ASSEMBLY at 0x823F8380-0x823F8488, cross-read against its ONE
// caller, BrnRendererModule::Render @0x8240BFA8 (asm 0x8240C6C0-0x8240C794).
//
// WHAT IT DOES. Flip the double-buffered colour-lookup volume, lock the new one through
// Tint::BeginBlendJob, publish this frame's colour cubes + weights into the returned
// TintBlendParameters, and dispatch the blend that fills it. BrnPostFx::Render drains it.
//
// THE GATE IS TWO TESTS, IN THIS ORDER (asm 0x823F8390-0x823F83A8):
//     lwz r11, 0x380(r31) / beq        -> mnActiveLayerCount, the same "is the driver built" latch
//                                         BrnPostFx::Render gates on
//     lwz r11, 0x384(r31) / rlwinm 0,26,26 / beq
//                                      -> m_enabledFx & 0x20 == E_FX_TINT
//
// THE BUFFER FLIP (asm 0x823F83AC-0x823F83C4):
//     lwz r11, 0x5FC / addi r11,r11,-1 / clrlwi r11,r11,31 / stw r11, 0x5FC
// i.e. `m_currentTintBuffer = (m_currentTintBuffer - 1) & 1` -- a toggle written as a decrement,
// because the field is unsigned and only bit 0 survives. The `addi r10, r11, 0x170 / slwi 2 / lwzx`
// that follows is the console's word-index pun for `m_pfxTint[m_currentTintBuffer]` (0x170*4 ==
// 0x5C0 == the array base); that arithmetic does not survive to the host and is not carried here.
//
// THE SOURCE LIST (asm 0x823F83D8-0x823F8418). `stw r11, 4(r29)` publishes m_numCubesToBlend into
// TintBlendParameters::numSources -- the word rw::graphics::postfx::TintBlend @0x82AD4860 uses as
// its variant jump-table index. The loop then walks two parallel arrays with ONE cursor
// (`addi r11, r31, 0x5E0` stepping 4, reading `-0x14(r11)` for the cube and `0(r11)` for the
// weight): m_colourCubes[i] at this+0x5CC and m_tintFactors[i] at this+0x5E0.
//
// ⚠ `lwz r8, 4(r8)` IS THE CUBE'S DATA POINTER, NOT THE CUBE. The console stores
// `colourCube->m_pixels` -- the +0x04 word its resource fix-up relocated -- as src[i], never the
// ColourCube itself. rwgpfxcolourcube.h::GetPixels() is that word's PC spelling (a 4-byte guest slot
// cannot hold a host pointer; the address it names, resource base + 16, is recomputed). The DWARF
// names both the member and the accessor -- see that header's banner.
//
// THE TWO STORES AFTER THE LOOP, in the console's order:
//   * the assert (asm 0x823F8420-0x823F8444) -- `cmplwi r11, 5 / ble`, i.e. it fires when
//     m_numCubesToBlend EXCEEDS 5, at BrnPostFx.cpp:266;
//   * `stfs f0, 0x40(r29)` with f0 = flt_82001CC0 == 0.0f (asm 0x823F8448-0x823F8460) --
//     TintBlendParameters::factor[5], the ONE weight slot the loop can never reach given that
//     assert. Reproduced because it is a real store into a block the job reads.
//
// [FLAG PC bring-up] THE DISPATCH, AND ONLY THE DISPATCH. The console ends with
//     EA::Jobs::Job::SetData(&m_blendJob, params, 128)
//     EA::Jobs::JobScheduler::AddJobs(&unk_830EA650, &m_blendJob, 1)
//     m_processTint = true
// `unk_830EA650` is the process-wide EA::Jobs::JobScheduler singleton, and IT DOES NOT EXIST ON
// THIS BUILD -- it is `gJobManager`, defined only in CgsHardwareInitPS3.cpp, and the PC variant has
// it commented out:
//   $ grep -n "JobManager" b5-decomp/src/GameShared/GameClasses/System/PC/CgsHardwareInitPC.cpp
//   40://JobScheduler CgsSystem::HardwareInit::mJobManager; // TODO: Implement HardwareInit
//   42:char CgsSystem::HardwareInit::macJobManagerBuffer[400 * 1024]; // 400 KB buffer for job manager
//   44://CgsMemory::HeapMallocCoreAllocator CgsSystem::HardwareInit::mJobManagerAllocator; // TODO ...
//   $ grep -n "CgsHardwareInitPS3" tools/build/build_game_exe.bat
//   (no output -- the PS3/X360 variant that defines gJobManager is not on the build list)
// So AddJobs -- and only AddJobs -- is replaced by running the job's OWN entry point inline, which
// is the precedent this tree already set for the same missing singleton in
// CgsCollisionGenerator_LineStream.cpp:175-180 and CgsLooseOctree.cpp:997. SetData still runs (it
// is the job's own state), m_processTint is still set, and Render's `m_blendJob.WaitOn()` is still
// correct: EA::Jobs::Job::WaitOn @0x82BCB238 opens with the console's own liveness guard on
// mJobInstanceHandle.mSchedulerBackend, so a job that was never submitted returns immediately --
// no spin, no hang (job.cpp:233-241). The one consequence, stated plainly: the blend costs its
// ~131 K texels on the dispatch thread here instead of on a worker, before Render's drain.
// DELETE WHEN a real job scheduler is brought up on PC.
// ================================================================================================

void BrnPostFx::BeginTintBlend()
{
    if (mnActiveLayerCount == 0u)
    {
        return;
    }
    if ((m_enabledFx & static_cast<u32>(E_FX_TINT)) == 0u)
    {
        return;
    }

    m_currentTintBuffer = (m_currentTintBuffer - 1u) & 1u;

    rw::graphics::postfx::TintBlendParameters& lrBlendParameters =
        m_pfxTint[m_currentTintBuffer]->BeginBlendJob();

    lrBlendParameters.numSources = m_numCubesToBlend;
    for (u32 luSource = 0u; luSource < m_numCubesToBlend; ++luSource)
    {
        // [FLAG PC bring-up] THE NULL TEST IS NOT THE CONSOLE'S, and it is one comparison.
        // The console cannot see a null here: BrnEffects::EffectsModule::PrepareResources
        // @0x8229DAAC seeds m_numCubesToBlend = 5 AND all five m_colourCubes with the default
        // cube before anything can set the tint bit, so `lwz r8, 4(r8)` is always on a real
        // object. On PC that producer is the tintdata half of this wave and may not be live yet,
        // and GetPixels() on a null would yield the address 16 -- a garbage pointer that is NOT
        // null, so the blend kernels' own null-source guard could not catch it. Carrying the null
        // through into src[] is what lets that guard see it.
        const rw::graphics::postfx::ColourCube* const lpColourCube = m_colourCubes[luSource];
        lrBlendParameters.src[luSource]    = (lpColourCube != 0) ? lpColourCube->GetPixels() : 0;
        lrBlendParameters.factor[luSource] = m_tintFactors[luSource];
    }

    CGS_ASSERT(m_numCubesToBlend <= 5u, "m_numCubesToBlend <= 5");           // BrnPostFx.cpp:266

    lrBlendParameters.factor[5] = KF_POST_FX_ZERO;   // stfs flt_82001CC0, 0x40(r29)

    // HOST sizeof, not the console's 0x80: every pointer in the block widens 4 -> 8 on this
    // target, so the guest record size would under-describe it (AGENTS.md GUEST vs HOST).
    m_blendJob.SetData(&lrBlendParameters,
                       sizeof(rw::graphics::postfx::TintBlendParameters));   // X360: 128

    {
        // The scheduler's own call shape: the entry point receives four Params and reads only
        // Param 1, which is where Job::SetData parks the data pointer (job.cpp:137). Running
        // TintBlendEntry rather than TintBlend keeps the console's entry path real.
        EA::Jobs::Param laJobParams[4];
        laJobParams[1] = EA::Jobs::Param(static_cast<void*>(&lrBlendParameters));
        TintBlendEntry(laJobParams[0], laJobParams[1], laJobParams[2], laJobParams[3]);
    }

    m_processTint = true;
}

// ================================================================================================
// BrnPostFx::EndTintBlend -- DWARF BrnPostFx.cpp:288 (declaration BrnPostFx.h:123).
//
// INLINED by the X360 compiler into BrnPostFx::Render at 0x8240A4DC-0x8240A4F4, where the flattened
// form is `lwz r11, 0x5FC(r31)` (the tint index) / `lwz r3, 0xC0(r11)` / `addi r4, r11, 0xA4` /
// `bl renderengine__Texture__Unlock` -- i.e. Tint::EndBlendJob on the CURRENT tint buffer, and
// nothing else. The DWARF body agrees exactly: one statement, `Tint::EndBlendJob(...)`.
//
// NOTE WHAT IT DOES *NOT* DO, reproduced by omission: it does not clear m_processTint (Render's own
// drain does that, before the WaitOn) and it does not clear the Tint's m_blendLock.
// ================================================================================================

void BrnPostFx::EndTintBlend()
{
    m_pfxTint[m_currentTintBuffer]->EndBlendJob();
}

// ==================================================================================================
// The THREE TINT MUTATORS (DWARF BrnPostFx.h:148/153 + the SetColourCube declaration at :84).
//
// None has a standalone X360 symbol -- like the five m_enabledFx mutators below, the console
// compiler inlined each into its caller. Both callers are known and both are read here:
//
//   BrnRendererModule::Render @0x8240BFA8, asm 0x8240C718-0x8240C790 -- the per-frame apply block,
//   five SetColourCube/SetTintBlendFactor pairs straight out of EffectsArbitrator::EvalTint's two
//   output arrays. This is where the ONE non-obvious fact lives:
//
//     0x8240C718  lwz   r11, var_CA0(r1)          ; the cube EvalTint wrote
//     0x8240C720  cmplwi cr6, r11, 0
//     0x8240C724  beq   cr6, loc_8240C72C          ; <-- SKIP the store when it is null
//     0x8240C728  stw   r11, dword_82FAF6BC(r20)   ; m_colourCubes[0]
//     0x8240C72C  ...
//     0x8240C730  stfs  f0, flt_82FAF6D0(r20)      ; m_tintFactors[0], NO test
//
//   ⚠ SetColourCube KEEPS THE PREVIOUS CUBE WHEN HANDED A NULL. All five cube stores are guarded
//   and none of the five weight stores is. Writing it as a plain assignment would clear a live LUT
//   to null on the first frame an effects layer went quiet -- and the blend would then either skip
//   (this build) or read address 0 (the console). The guard is the console's, not a PC accommodation.
//
//   BrnEffects::EffectsModule::PrepareResources @0x8229D8A8, asm 0x8229DA8C-0x8229DAD0 -- the
//   one-time seed: `li r9, 5` / `stw r9, dword_82FAF6E8` is SetTintBlendNumber(5), followed by five
//   SetTintBlendFactor(i, flt_8200DD40) and five SetColourCube(i, <the default cube>). That is the
//   only writer of m_numCubesToBlend in the whole image:
//     $ grep -l "82FAF6E8" .ida-exports/BURNOUT_X360_ARTIST.XEX/*.json
//     .ida-exports/BURNOUT_X360_ARTIST.XEX/0x8229D8A8.json
//   (recorded in this wave's report as a CROSS-GROUP item: with that seed absent, numSources stays
//   0 and TintBlend's variant table has a null in slot 0.)
//
// The `const int&` parameter spelling is the DWARF's, not a choice.
// ==================================================================================================
void BrnPostFx::SetColourCube(int liIndex, rw::graphics::postfx::ColourCube* lpColourCube)
{
    // THE ONE NULL GUARD, and this is where the console spells it: BrnRendererModule::Render
    // @0x8240BFA8 inlines this setter five times and every one of the five carries its own
    // `cmplwi cr6, r11, 0` / `beq` over the `stw` into m_colourCubes (0x8240C720/C738/C750/C768/
    // C780), while none of the five weight `stfs` has any test. The caller side therefore does NOT
    // repeat it (BrnRendererModulePostFx.cpp's apply loop) -- one rule, one place.
    if (lpColourCube != 0)
    {
        m_colourCubes[liIndex] = lpColourCube;
    }
}

void BrnPostFx::SetTintBlendFactor(int liIndex, f32 lfFactor)
{
    m_tintFactors[liIndex] = lfFactor;
}

void BrnPostFx::SetTintBlendNumber(const int& lriNumber)
{
    m_numCubesToBlend = static_cast<u32>(lriNumber);
}

// ==================================================================================================
// The m_enabledFx MUTATORS and the effect/state ACCESSORS (DWARF BrnPostFx.h:73-101).
//
// NONE has a standalone X360 symbol: the console compiler inlined every one of them into their single
// caller, BrnRendererModule::Render @0x8240BFA8's effects apply block, so each body below is
// recovered from THAT caller's assembly and AGENTS.md's inlining-reversal rule is what puts them back
// as calls. The five mutators are all the same compare-then-or/andc shape on m_enabledFx (this+0x384):
//   SetBloom         `ori r9, r11, 2`     / `rlwinm r9, r11, 0,31,29`  @0x8240D760 / 0x8240D768
//   SetVignette      `ori r11, r9, 0x10`  / `rlwinm r11, r9, 0,28,26`  @0x8240D80C / 0x8240D818
//   SetDepthOfField  `ori r11, r11, 1`    / `clrrwi r11, r11, 1`       @0x8240D934 / 0x8240D93C
//   SetB4Blur        `ori r11, r11, 0x40` / `rlwinm r11, r11, 0,26,24` @0x8240DA00 / 0x8240DA08
//   SetTint          bit 0x20 (Hex-Rays `v78 = dword_82FAF474 | 0x20` / `& 0xFFFFFFDF`, lines 512-514)
// and each accessor is the address the same asm forms: 0x950 mBloomData, 0x390 m_vignetteState,
// 0x3D0 m_dofState, 0x3F0 m_b4blurState, 0x450 mMotionBlurState, 0x5B8 m_pfxDof, 0x5BC m_pfxVignette,
// 0x5C8 m_pfxB4Blur (and 0x5C0/0x5C4 m_pfxTint[2], untouched by Render's apply block).
//
// The `const bool&` / `const int&` parameter spellings are the DWARF's, not a choice.
// ==================================================================================================
void BrnPostFx::SetDepthOfField(const bool& lrbEnabled)
{
    if (lrbEnabled) { m_enabledFx |=  static_cast<u32>(E_FX_DEPTH_OF_FIELD); }
    else            { m_enabledFx &= ~static_cast<u32>(E_FX_DEPTH_OF_FIELD); }
}

void BrnPostFx::SetBloom(const bool& lrbEnabled)
{
    if (lrbEnabled) { m_enabledFx |=  static_cast<u32>(E_FX_BLOOM); }
    else            { m_enabledFx &= ~static_cast<u32>(E_FX_BLOOM); }
}

void BrnPostFx::SetVignette(const bool& lrbEnabled)
{
    if (lrbEnabled) { m_enabledFx |=  static_cast<u32>(E_FX_VIGNETTE); }
    else            { m_enabledFx &= ~static_cast<u32>(E_FX_VIGNETTE); }
}

void BrnPostFx::SetTint(const bool& lrbEnabled)
{
    if (lrbEnabled) { m_enabledFx |=  static_cast<u32>(E_FX_TINT); }
    else            { m_enabledFx &= ~static_cast<u32>(E_FX_TINT); }
}

void BrnPostFx::SetB4Blur(const bool& lrbEnabled)
{
    if (lrbEnabled) { m_enabledFx |=  static_cast<u32>(E_FX_B4BLUR); }
    else            { m_enabledFx &= ~static_cast<u32>(E_FX_B4BLUR); }
}

rw::graphics::postfx::Vignette*     BrnPostFx::GetVignette()     { return m_pfxVignette; }
BrnPostFxBloomData*                 BrnPostFx::GetBloom()        { return &mBloomData; }
rw::graphics::postfx::DepthOfField* BrnPostFx::GetDepthOfField() { return m_pfxDof; }
rw::graphics::postfx::B4Blur*       BrnPostFx::GetB4Blur()       { return m_pfxB4Blur; }

rw::graphics::postfx::Vignette::State*     BrnPostFx::GetVignetteState() { return &m_vignetteState; }
rw::graphics::postfx::DepthOfField::State* BrnPostFx::GetDofState()      { return &m_dofState; }
rw::graphics::postfx::B4Blur::State*       BrnPostFx::GetB4BlurState()   { return &m_b4blurState; }
MotionBlurState&                           BrnPostFx::GetMotionBlurState() { return mMotionBlurState; }
