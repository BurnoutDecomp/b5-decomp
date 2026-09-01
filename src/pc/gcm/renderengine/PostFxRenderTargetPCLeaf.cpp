// =============================================================================
// PostFxRenderTargetPCLeaf.cpp  (pc/gcm/renderengine)
//
// [PC platform leaf] The Direct3D 9 realisation of the post-fx RENDER-TARGET
// surface -- the layer underneath CgsRenderTarget that the shadow-map pass needs
// and that this build has never had.
//
// WHY A LEAF AND NOT A DECOMPILE
// ------------------------------
// The X360 render-target layer is EDRAM-based end to end. Its Target::CreateColor
// / CreateDepth build a tiled EDRAM PixelBuffer through XGSetSurfaceHeader, place
// it with Xbox2SetBaseEDRAM / Xbox2SetBaseHierarchicalZ, and RESOLVE it out to a
// linear texture with Xbox2ResolveTo. None of that has a PC Direct3D 9
// counterpart: there is no EDRAM, no tile allocator, no hierarchical-Z region and
// no resolve step. That console path IS reconstructed -- faithfully -- in
// SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxrendertarget.cpp,
// which is deliberately NOT in the link (it would drag the whole Xbox2* /
// PixelBuffer / TextureState EDRAM surface with it). This file is its PC sibling,
// in the established style of SkyDomeProgramsPC.cpp / ImmediateModePCLeaf.cpp /
// XenonD3D9Shims.cpp: the MINIMUM of the same declared surface, over D3D9, so the
// finished shadow pass has a real depth-sampleable target to render into.
//
// WHAT IS CONSOLE-FAITHFUL HERE (kept, because it is cheap and recoverable)
//   * the Begin/End bracket semantics                (Begin binds section n's
//     surface state + the full-extent viewport/scissor; End resolves),
//   * the per-section render-target-state array and the section-0 fallback to
//     rw::graphics::postfx::gpDefaultRenderTargetState,
//   * the "skip a redundant back-to-back Device::SetState" shadow,
//   * the depth TEXTURE height formula out of Target::CreateDepth @0x82403688:
//         textureHeight = numSections * height
//     (three 640-row cascade bands in one 1280x1920 texture, which is exactly the
//     1x3 vertical-strip atlas the recovered ShadowMap_* constants encode),
//   * the allocator plumbing (objects are carved from the rw resource allocator
//     the parameters carry, or the registry default, as RenderTarget::Initialize
//     @0x82409B60 does).
//
// WHAT IS A PC BRING-UP CHOICE (flagged, NOT dressed up as console behaviour)
//   * the surfaces are D3D9 objects: a depth TEXTURE whose level-0 surface is bound
//     as the depth-stencil -- picked from ONE OF TWO FORMAT LADDERS, because the
//     shadow map wants a hardware COMPARE fetch and every post-fx target wants the
//     raw depth VALUE (see the two-ladder banner on CreateDepthSurface below) --
//     and, because D3D9 rejects SetRenderTarget(0, NULL), a throwaway colour render
//     target for the depth-only case,
//   * Resolve() is a NO-OP, and legitimately so: on PC the depth texture IS the
//     surface that was rendered into, so there is nothing to copy out. This is
//     stated rather than faked; no EDRAM resolve is simulated,
//   * no EDRAM base / tile index / hierarchical-Z / compression-base field is
//     honoured (they describe hardware that does not exist here),
//   * the per-section states all reference the SAME D3D9 surfaces (see the
//     SECTIONS note on Initialize).
// =============================================================================

// INCLUDE ORDER IS LOAD-BEARING: <d3d9.h> must come FIRST, before any project header.
// rw/core/debug/DebugCriticalSection.h (pulled in transitively by rw/rwcore_structs.h, which the
// postfx header needs for rw::IResourceAllocator) defines NOGDI / NOUSER / WIN32_LEAN_AND_MEAN
// before its own <windows.h>, so a later <d3d9.h> finds RGNDATA / LPMSG missing and fails to
// parse IDirect3DDevice9. Including the D3D headers first makes the Windows headers complete
// before anything can trim them; the trimming macros are then no-ops (windows.h is already in).
#include <Windows.h>
#include <d3d9.h>

#include "types.hpp"

#include "SDKs/RenderEngineClub/MAIN/components/include/postfx/rwgpfxrendertarget.h"
#include "pc/gcm/renderengine/device.h"                  // renderengine::gDevice / gD3D9 / Device::SetState
#include "pc/gcm/renderengine/texture.h"                 // renderengine::Texture (mpD3DTexture)
#include "pc/gcm/renderengine/renderstates.h"            // renderengine::TextureState (the colour sampler states)
#include "pc/gcm/renderengine/ShadowPassPCLeaf.h"        // renderengine::ShadowDepthFormat* (homed below)
#include "rw/rwcore_structs.h"                           // rw::IResourceAllocator / Resource / ResourceDescriptor
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"  // shadow::Device::SetResource (the sampler-shadow-aware unbind)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // CgsDev::Log::WriteToLog ([shadow-rt] diagnostic)

#include <cstring>
#include <cstdio>
#include <cstddef>
#include <cstdlib>   // std::getenv (the BRN_ENVMAP_DEBUG face-colour knob, OFF by default)

// =============================================================================
// renderengine::RenderTargetState -- the bound-surface object.
//
// On the X360 this is the GPU D3DSURFACES descriptor Device::SetState installs
// (built by RenderTarget::CreateStates @0x824037E8 out of a five-word parameter
// block). It has no header anywhere in the tree -- every consumer holds it by
// pointer and only Device::SetState ever looks inside -- so the PC realisation
// defines it HERE, at its single point of use, and nothing else sees the layout.
//
// FLAG PC-platform leaf: the members are the D3D9 objects a colour/depth bind
// needs, not the console's packed GPU register words.
// =============================================================================
namespace renderengine
{
    class RenderTargetState
    {
    public:
        IDirect3DSurface9* mpColourSurface;   // null => depth-only (the shadow map)
        IDirect3DSurface9* mpDepthSurface;    // null => no depth-stencil bound
        u32                muWidth;
        u32                muHeight;
    };
}

namespace
{
    // The engine device, refreshed on every entry (same idiom as XenonD3D9Shims.cpp's Dev()).
    inline IDirect3DDevice9* Dev()
    {
        return renderengine::gDevice;
    }

    // ---- the throwaway colour render target -------------------------------------------------
    // FLAG PC bring-up: Direct3D 9 has no "no colour attachment" mode -- SetRenderTarget(0, NULL)
    // is an invalid call -- so a depth-only pass still needs a colour surface bound. The console
    // does not (the Xenos writes depth into EDRAM with every colour target switched off, which is
    // exactly what CreateShadowmapBuffer describes). One surface is cached and re-created only if
    // a larger extent is asked for; its contents are never read.
    IDirect3DSurface9* gpNullColourSurface = nullptr;
    u32                guNullColourWidth   = 0;
    u32                guNullColourHeight  = 0;

    // The vendor "NULL" render-target format (FOURCC 'NULL'): a zero-bandwidth colour target that
    // both major PC vendors expose for exactly this depth-only case. When it is unavailable the
    // fallback is a real 16-bit surface (1280x1920 => ~4.7 MB, written and never read).
    IDirect3DSurface9* AcquireNullColourSurface(u32 luWidth, u32 luHeight)
    {
        IDirect3DDevice9* const lpDevice = Dev();
        if (lpDevice == nullptr)
            return nullptr;

        if (gpNullColourSurface != nullptr && guNullColourWidth >= luWidth && guNullColourHeight >= luHeight)
            return gpNullColourSurface;

        if (gpNullColourSurface != nullptr)
        {
            gpNullColourSurface->Release();
            gpNullColourSurface = nullptr;
        }

        const D3DFORMAT laeCandidates[3] =
        {
            static_cast<D3DFORMAT>(MAKEFOURCC('N', 'U', 'L', 'L')),
            D3DFMT_R5G6B5,
            D3DFMT_A8R8G8B8
        };

        for (u32 luCandidate = 0; luCandidate < 3u; ++luCandidate)
        {
            IDirect3DSurface9* lpSurface = nullptr;
            if (SUCCEEDED(lpDevice->CreateRenderTarget(luWidth, luHeight, laeCandidates[luCandidate],
                                                       D3DMULTISAMPLE_NONE, 0, FALSE,
                                                       &lpSurface, nullptr))
                && lpSurface != nullptr)
            {
                gpNullColourSurface = lpSurface;
                guNullColourWidth   = luWidth;
                guNullColourHeight  = luHeight;
                return gpNullColourSurface;
            }
        }
        return nullptr;
    }

    // ---- the depth-sampleable texture -------------------------------------------------------
    // FLAG PC bring-up: the X360 samples a depth surface after Xbox2ResolveTo has copied it out
    // of EDRAM into a linear texture of the packed format 0x2D200196. On PC the standard (and
    // only) way to sample depth under Direct3D 9 is a depth-stencil TEXTURE, created with a
    // vendor format and bound both as the depth-stencil surface and as a sampler resource -- so
    // the console's 0x2D200196 is not translated, it is REPLACED by the D3D9 equivalent.
    //
    // âš  CORRECTED 2026-08-12 (the shadow-fetch wave). The ladder used to be
    // INTZ -> DF24 -> DF16 -> D24S8, chosen for "is it sampleable at all". That is the WRONG
    // axis: the receiver does not want a depth VALUE, it wants a depth COMPARISON. Every one of
    // the 92 s15 pixel shaders in build/game/SHADERS.BNDL reads
    //     texldp rN, rN, s15 ; ... mul r0.w, r0.w, rN.x
    // i.e. it takes .x straight as a 0..1 lit factor, with rN.z/rN.w as the reference -- the
    // Xenos hardware depth-compare fetch. None of the 92 does a manual compare (verified by
    // disassembling all 110 shipped pixel shaders with fxc -dumpbin). INTZ/DF24/DF16 return the
    // RAW stored depth and compare nothing, so on those formats `lit *= rawDepth` is not a
    // shadow test at all -- on a cleared (1.0) map it is identically "fully lit".
    //
    // The ladder is therefore ordered by SEMANTICS first, availability second:
    //   D24X8 - a real depth-stencil format created as a TEXTURE. Where the driver allows it
    //           (NVIDIA and Intel do; it is the classic "hardware shadow map"), the fetch
    //           COMPARES ref against the stored depth and, with a LINEAR filter, 2x2-PCFs the
    //           result -- exactly the Xenos semantic the shaders were written against.
    //   D16   - the same thing at 16 bits.
    //   INTZ / DF24 / DF16 - readable depth, NO comparison. Kept as a last resort so an AMD part
    //           still gets a bound, populated sampler rather than an unbound one, but the fetch
    //           semantics DIVERGE from the shaders and ShadowDepthFormatIsHardwareCompare()
    //           reports false so the [shadow-rt] line says `compare=RAW`. Fixing that case is a
    //           SHADER-PIPELINE job (the technique needs a manual-compare variant); it cannot be
    //           done from the engine without rewriting shipped shader semantics.
    // If nothing above can be created, the target falls back to a plain non-sampleable
    // D3DFMT_D24S8 depth-stencil surface: the pass still renders (and is still correct), but
    // GetDepthStencilTexture() returns null and the [shadow-rt] line below says so.
    //
    // Each candidate is gated on CheckDeviceFormat rather than a blind CreateTexture: a driver
    // that merely tolerates a depth-stencil texture is not the same as one that advertises it,
    // and the old ladder's first blind CreateTexture(INTZ) succeeding is precisely how the
    // semantically wrong format won.
    //
    // =====================================================================================
    // CORRECTED AGAIN 2026-08-15 (the post-fx step-5 wave): THERE ARE **TWO** LADDERS,
    // because there are two mutually exclusive sample semantics and ONE ladder cannot serve
    // both. The paragraph above is right about the SHADOW MAP and wrong about everything else.
    //
    //   COMPARE (the shadow map, unchanged above): the receiver wants a 0/1 (PCF-filtered)
    //     COMPARISON of a reference against the stored depth -- `texldp rN, rN, s15` then
    //     `mul r0.w, r0.w, rN.x`. D24X8 / D16 created as a texture give exactly that.
    //
    //   RAW VALUE (every post-fx target that asks to be sampled): the receiver wants the depth
    //     VALUE itself, to reconstruct a clip position or a focus distance. Its consumers are
    //     named, and they read it with a PLAIN `texld` against SamplerDepth (unit 4, decoded in
    //     BrnPostFxShader.cpp:174-195):
    //       * BrnPostFxShader's DoF permutations (slots 2/3/6/7/10/11) -- DofParamsA/B vs depth,
    //       * its motion-blur permutations (slots 4..11) -- depth + BlurMatrixX/Y/W rebuild the
    //         previous frame's clip position and the tap direction,
    //       * rw::graphics::postfx::DepthOfField (rwgpfxdof.cpp) and PfxHelper's down-sample.
    //     On D24X8 those fetches return a COMPARISON against whatever happens to be in the
    //     coordinate's z/w -- 0.0 or 1.0, never a depth -- so the effect is silently, uniformly
    //     wrong rather than absent. INTZ / DF24 / DF16 are the formats that return the stored
    //     value, i.e. exactly the three the compare ladder DEMOTED.
    //
    // The discriminator is the one the memory of this campaign already attests and which
    // Initialize's shadow latch below already uses: the SHADOW MAP is the only pool target
    // with colour mode NONE that still resolves its depth to a texture (CreateShadowmapBuffer
    // clears every colour section); the down-sample buffer, the particle buffer and the env-map
    // buffer all carry colour. So: colour mode NONE -> COMPARE ladder, anything else -> RAW.
    //
    // Targets that create depth but never sample it (the anti-alias buffer, the back buffer)
    // keep the COMPARE ladder untouched -- their format is unobservable either way, and
    // changing it would be an unforced change to a working path.
    //
    // D3DCLEAR_STENCIL stays consistent automatically: XenonD3D9Shims.cpp's
    // TilingDepthFormatHasStencil answers from the format of the surface CURRENTLY BOUND
    // (INTZ and RAWZ alias D24S8 and DO carry stencil; DF24 / DF16 do not), and every clear in
    // this build goes through TilingClearBoundSurfaces, which asks it. Nothing here has to
    // know which ladder produced the surface.
    // =====================================================================================
    struct DepthSurfaceResult
    {
        IDirect3DTexture9* mpTexture;
        IDirect3DSurface9* mpSurface;
        u32                muFormat;           // 0 => the non-sampleable D24S8 fallback
        bool               mbHardwareCompare;  // true => the fetch compares (Xenos shadow semantic)
        bool               mbRawDepthValue;    // true => the fetch returns the stored depth VALUE
    };

    struct DepthFormatCandidate
    {
        u32  muFormat;
        bool mbHardwareCompare;
        bool mbRawDepthValue;
    };

    // The COMPARE ladder (the shadow map's; unchanged, order and members both).
    const DepthFormatCandidate KA_DEPTH_LADDER_COMPARE[5] =
    {
        { static_cast<u32>(D3DFMT_D24X8),                  true,  false },
        { static_cast<u32>(D3DFMT_D16),                    true,  false },
        { static_cast<u32>(MAKEFOURCC('I', 'N', 'T', 'Z')), false, true  },
        { static_cast<u32>(MAKEFOURCC('D', 'F', '2', '4')), false, true  },
        { static_cast<u32>(MAKEFOURCC('D', 'F', '1', '6')), false, true  }
    };

    // The RAW-VALUE ladder (every sampled post-fx target's). D24X8 / D16 are deliberately NOT
    // on it: a compare fetch is not a depth read, and falling back to one would produce the
    // silent-wrong picture this split exists to prevent. If none of the three can be created
    // the target takes the same non-sampleable D24S8 fallback the compare ladder takes, and
    // reports depthTexture=NONE -- an honestly empty slot rather than a wrong one.
    const DepthFormatCandidate KA_DEPTH_LADDER_RAW[3] =
    {
        { static_cast<u32>(MAKEFOURCC('I', 'N', 'T', 'Z')), false, true },
        { static_cast<u32>(MAKEFOURCC('D', 'F', '2', '4')), false, true },
        { static_cast<u32>(MAKEFOURCC('D', 'F', '1', '6')), false, true }
    };

    // =====================================================================================
    // THE MULTISAMPLE (MSAA) SURFACE PATH  [rung 8, 2026-08-16]
    //
    // WHAT THE CONSOLE DOES. The anti-alias scene target is created at MULTISAMPLE FORMAT 1:
    // BrnRendererMemory::CreateAntiAliasBuffer @0x823F6B40 selects one of two predicated-tiling
    // records and takes the format straight out of it (BrnAntiAliasTiling.h --
    // KMSAA_TILING_PLAN.mn32MultiSampleFormat == 1, KNO_MSAA_TILING_PLAN's == 0), and
    // CgsRenderTarget::Construct forwards that one word into EVERY Target::Parameters block --
    // the colour sections at CgsRenderTarget.cpp:174 and the depth block at :220, both from the
    // single CgsRenderTarget::mn32MultiSampleFormat, so colour and depth cannot disagree.
    // Until this wave the PC leaf ignored the word entirely and hard-wired D3DMULTISAMPLE_NONE.
    //
    // THE FORMAT -> SAMPLE-COUNT MAP IS RECOVERED, NOT ASSUMED. CreateAntiAliasBuffer's EDRAM
    // arithmetic is the attestation: `format >= 1` doubles the surface's tile HEIGHT and
    // `format == 2` doubles its WIDTH as well (cmpwi 1 / blt @0x823F6C20, cmpwi 2 / bne
    // @0x823F6C2C; reproduced at BrnRendererMemory.cpp:667-676). One doubling of the footprint
    // is 2 samples, two doublings is 4. So 0 -> 1 sample (none), 1 -> 2, 2 -> 4. No other value
    // appears in the image (the pool writes 0 everywhere except the anti-alias buffer's plan and
    // the env-map buffer's KI_ENV_MAP_MULTISAMPLE_FORMAT == 2).
    //
    // WHAT D3D9 NEEDS THAT THE CONSOLE DID NOT.
    //   * A MULTISAMPLED COLOUR SECTION IS A SURFACE, NOT A TEXTURE. IDirect3DDevice9::
    //     CreateTexture takes no multisample type at all; a multisampled render target must come
    //     from CreateRenderTarget and can never be bound to a sampler. That costs this target
    //     nothing, because its pixels reach the rest of the frame through the RESOLVE, whose
    //     destination is the DOWN-SAMPLE buffer's texture -- BrnRendererModule::ResolveMSAA hands
    //     `GetDownSampleBuffer()->GetTexture(0)` to D3DDevice_Resolve (BrnRendererModule.cpp:2238
    //     tiled / :2278 untiled). So a multisampled target keeps NO colour texture, GetTexture(0)
    //     returns null for it, the colour TextureState is not built (its gate already tests the
    //     texture), and the [postfx-rt] line spells `colourTexture=MSAA` rather than `NULL` so the
    //     log does not read as a creation failure.
    //     NOTHING READS THE ANTI-ALIAS TARGET'S TEXTURE -- verified, not assumed:
    //       $ grep -rn "GetAntiAliasBuffer" b5-decomp/src --include=*.cpp --include=*.h
    //       GameSource/Graphics/BrnRendererMemory.cpp:543:    CgsRenderTarget* const lpAntiAliasBuffer  = GetAntiAliasBuffer();
    //       GameSource/Graphics/BrnRendererMemory.h:105:    CgsRenderTarget* GetAntiAliasBuffer()  { return mapRenderTarget[E_RENDER_TARGET_ANTI_ALIAS]; }
    //       GameSource/Graphics/BrnRendererModule.cpp:670:        gpAntiAliasTarget = lrRendererMemory.GetAntiAliasBuffer();
    //       GameSource/Graphics/BrnRendererModule.cpp:2065:        ShadowedSetRenderTargetState(mAllocatedRenderTargets.GetAntiAliasBuffer());
    //       GameSource/Graphics/BrnRendererModule.cpp:2102:        ShadowedSetRenderTargetState(mAllocatedRenderTargets.GetAntiAliasBuffer());
    //     (plus three comment lines). The two live uses are surface BINDS; :543 is the pool's own
    //     one-shot creation clear, which goes through the section state, not a texture; :670/:685
    //     is a readiness test on GetRenderTarget() != null.
    //   * THE DEPTH-STENCIL MUST MATCH THE COLOUR'S SAMPLE COUNT **AND QUALITY** or
    //     SetDepthStencilSurface fails -- and a multisampled depth cannot be a texture either. So
    //     it is a plain CreateDepthStencilSurface at the same type/quality. The anti-alias target
    //     does not ask for a sampleable depth anyway (CreateAntiAliasBuffer:
    //     `SetUseDepthStencilAsTexture(false)`); the target that does is the DOWN-SAMPLE buffer,
    //     which the console leaves single-sampled and whose rung-6 INTZ ladder is untouched here.
    //     The bridge between the two is the RESZ depth resolve in XenonD3D9Shims.cpp.
    //   * EVERY COUNT IS CheckDeviceMultiSampleType-GATED, for the colour format AND for a depth
    //     format, and a count the device refuses steps DOWN one at a time to 1, reporting
    //     `msaa=<requested> unsupported -> <used>`. QUALITY LEVEL 0: the D3D9 contract is
    //     `Quality < pQualityLevels`, 0 is the level every part that supports a type exposes, and
    //     reaching for a vendor quality (CSAA / EQAA) would be a PC choice with no console
    //     counterpart.
    //
    // THE PC KNOB, AND WHY IT IS NOT A NEW GLOBAL. `renderengine::gAntiAliasing` ALREADY EXISTS
    // and is ALREADY the PC display setting for exactly this -- declared device.h:16, defined
    // device.cpp:25, seeded 0 by Device::Initialize, read from config.ini by
    // GameSource/Main/BrnMain.cpp:123 (`GetPrivateProfileIntA("Settings", "AntiAliasing", ...)`,
    // clamped 0..16, TUB's own clamp) and written back by SaveConfig at :145. Minting a second
    // `gMultisampleOverride` beside it would be the split-brain this project has paid for before
    // (AGENTS.md "STATE OWNERSHIP"), so THE KNOB IS THAT WORD:
    //     0        -> use the console's own multisample format (the default, and the shipped
    //                 behaviour: the anti-alias target's plan says format 1 == 2 samples)
    //     1        -> force MSAA off
    //     2 / 4 / 8-> force that sample count
    // It NEVER turns MSAA on for a target the console leaves single-sampled: an override applied
    // to a console format of 0 is IGNORED, because that would be a silent change to a working
    // path rather than a knob.
    // =====================================================================================
    const u32 KU_MULTISAMPLE_MAX_SAMPLES = 8u;

    // Xenos multisample format -> sample count. See the banner for the attestation.
    u32 MultisampleSampleCountForConsoleFormat(u32 luConsoleFormat)
    {
        switch (luConsoleFormat)
        {
        case 0u: return 1u;
        case 1u: return 2u;
        case 2u: return 4u;
        default: break;
        }
        // There is no fourth value in the image, and CgsRenderTarget::mn32MultiSampleFormat is NOT
        // seeded by its constructor (CgsRenderTarget.cpp:123 writes only mu8NumSections; every one
        // of the eleven pool creators writes the format itself). So an unexpected value means an
        // uninitialised target reached here, not a format this leaf should honour -- refuse it, and
        // say so once rather than silently multisampling a target nobody asked to multisample.
        static bool sbReportedBadFormat = false;
        if (!sbReportedBadFormat)
        {
            sbReportedBadFormat = true;
            char lacMessage[192];
            std::snprintf(lacMessage, sizeof(lacMessage),
                          "[postfx-rt] multisample format %u is not one of the console's 0/1/2 --"
                          " the target is treated as single-sampled\n",
                          static_cast<unsigned>(luConsoleFormat));
            CgsDev::Log::WriteToLog(lacMessage);
        }
        return 1u;
    }

    // THE PC KNOB (see the banner). Returns 0 for "use the console's format".
    u32 MultisampleOverrideSampleCount()
    {
        const s32 lnAntiAliasing = renderengine::gAntiAliasing;
        if (lnAntiAliasing <= 0)
            return 0u;
        u32 luSamples = static_cast<u32>(lnAntiAliasing);
        if (luSamples > KU_MULTISAMPLE_MAX_SAMPLES)
            luSamples = KU_MULTISAMPLE_MAX_SAMPLES;
        return luSamples;
    }

    struct MultisampleChoice
    {
        D3DMULTISAMPLE_TYPE meType;
        DWORD               muQuality;
        u32                 muSamples;        // 1 == D3DMULTISAMPLE_NONE
        D3DFORMAT           meDepthFormat;    // the DS format that supports muSamples
    };

    // The MSAA depth-stencil ladder. D24S8 FIRST, because that is what the rest of this build
    // already assumes it is looking at: the device's own auto depth-stencil is D24S8
    // (device.cpp:97), the RESZ resolve's destination is an INTZ texture and INTZ *is* a D24S8
    // layout, and XenonD3D9Shims.cpp's TilingDepthFormatHasStencil only lets the frame's clear ask
    // for D3DCLEAR_STENCIL on a format that carries stencil (a stencil clear against a
    // stencil-less surface makes D3D9's Clear fail ENTIRELY, taking the depth clear with it).
    // D24X8 / D16 are honest fallbacks for a part that multisamples neither.
    const D3DFORMAT KAE_MSAA_DEPTH_FORMATS[3] = { D3DFMT_D24S8, D3DFMT_D24X8, D3DFMT_D16 };

    // Pick the highest supported sample count <= luRequestedSamples that BOTH the colour format
    // and one of the depth formats accept. Steps down by one so a request the device refuses lands
    // on the next lower count rather than on nothing.
    MultisampleChoice ChooseMultisampleType(u32 luRequestedSamples, D3DFORMAT leColourFormat)
    {
        MultisampleChoice lChoice = { D3DMULTISAMPLE_NONE, 0u, 1u, D3DFMT_D24S8 };

        IDirect3DDevice9* const lpDevice = Dev();
        if (lpDevice == nullptr || luRequestedSamples <= 1u)
            return lChoice;

        D3DDEVICE_CREATION_PARAMETERS lCreation;
        std::memset(&lCreation, 0, sizeof(lCreation));
        lCreation.AdapterOrdinal = D3DADAPTER_DEFAULT;
        lCreation.DeviceType     = D3DDEVTYPE_HAL;
        lpDevice->GetCreationParameters(&lCreation);

        // WINDOWED IS READ, NOT ASSUMED: CheckDeviceMultiSampleType answers differently for the two
        // and this build forces windowed today (device.cpp:52 `gFullscreen = false`) without being
        // required to. The swap chain knows what the device was actually created with.
        BOOL lbWindowed = TRUE;
        IDirect3DSwapChain9* lpSwapChain = nullptr;
        if (SUCCEEDED(lpDevice->GetSwapChain(0u, &lpSwapChain)) && lpSwapChain != nullptr)
        {
            D3DPRESENT_PARAMETERS lPresent;
            std::memset(&lPresent, 0, sizeof(lPresent));
            if (SUCCEEDED(lpSwapChain->GetPresentParameters(&lPresent)))
                lbWindowed = lPresent.Windowed;
            lpSwapChain->Release();                 // GetSwapChain AddRefs
        }

        IDirect3D9* lpD3D = nullptr;
        if (FAILED(lpDevice->GetDirect3D(&lpD3D)) || lpD3D == nullptr)
            return lChoice;

        for (u32 luSamples = luRequestedSamples; luSamples > 1u; --luSamples)
        {
            const D3DMULTISAMPLE_TYPE leType = static_cast<D3DMULTISAMPLE_TYPE>(luSamples);

            DWORD luColourQuality = 0u;
            if (FAILED(lpD3D->CheckDeviceMultiSampleType(lCreation.AdapterOrdinal,
                                                         lCreation.DeviceType, leColourFormat,
                                                         lbWindowed, leType, &luColourQuality)))
            {
                continue;
            }

            for (u32 luDepth = 0; luDepth < 3u; ++luDepth)
            {
                DWORD luDepthQuality = 0u;
                if (SUCCEEDED(lpD3D->CheckDeviceMultiSampleType(lCreation.AdapterOrdinal,
                                                                lCreation.DeviceType,
                                                                KAE_MSAA_DEPTH_FORMATS[luDepth],
                                                                lbWindowed, leType,
                                                                &luDepthQuality)))
                {
                    lChoice.meType        = leType;
                    lChoice.muQuality     = 0u;     // Quality < pQualityLevels; see the banner
                    lChoice.muSamples     = luSamples;
                    lChoice.meDepthFormat = KAE_MSAA_DEPTH_FORMATS[luDepth];
                    lpD3D->Release();
                    return lChoice;
                }
            }
        }

        lpD3D->Release();
        return lChoice;
    }

    // The MULTISAMPLED colour render target. A SURFACE, and deliberately without a texture: the
    // resolve writes the DOWN-SAMPLE buffer's texture, not this target's (see the banner).
    // Lockable FALSE -- a lockable multisampled render target is not creatable, and nothing reads
    // this surface from the CPU (the ImVerts read-back diag in XenonD3D9Shims.cpp now reports
    // `rt=MSAA(no readback)` instead of pretending it can).
    IDirect3DSurface9* CreateMultisampledColourSurface(u32 luWidth, u32 luHeight,
                                                       D3DFORMAT leFormat,
                                                       const MultisampleChoice& lrChoice)
    {
        IDirect3DDevice9* const lpDevice = Dev();
        if (lpDevice == nullptr)
            return nullptr;

        IDirect3DSurface9* lpSurface = nullptr;
        if (FAILED(lpDevice->CreateRenderTarget(luWidth, luHeight, leFormat, lrChoice.meType,
                                                lrChoice.muQuality, FALSE, &lpSurface, nullptr)))
        {
            return nullptr;
        }
        return lpSurface;
    }

    // The MULTISAMPLED depth-stencil surface. Sample type AND quality must equal the colour
    // target's or the bind fails. Discard FALSE, and that is load-bearing: the RESZ depth resolve
    // (XenonD3D9Shims.cpp) reads this surface at the END of the frame, and a discardable
    // depth-stencil's contents are undefined the moment it stops being the bound one.
    IDirect3DSurface9* CreateMultisampledDepthSurface(u32 luWidth, u32 luHeight,
                                                      const MultisampleChoice& lrChoice)
    {
        IDirect3DDevice9* const lpDevice = Dev();
        if (lpDevice == nullptr)
            return nullptr;

        IDirect3DSurface9* lpSurface = nullptr;
        if (FAILED(lpDevice->CreateDepthStencilSurface(luWidth, luHeight, lrChoice.meDepthFormat,
                                                       lrChoice.meType, lrChoice.muQuality,
                                                       FALSE, &lpSurface, nullptr)))
        {
            return nullptr;
        }
        return lpSurface;
    }

    DepthSurfaceResult CreateDepthSurface(u32 luWidth, u32 luHeight, bool lbWantRawDepthValue)
    {
        // SINGLE-SAMPLED ONLY (rung 8). A multisampled target takes CreateMultisampledDepthSurface
        // above instead: every format on both ladders below is a TEXTURE format, and D3D9 has no
        // multisampled texture of any kind -- CreateTexture does not even take a multisample type.
        DepthSurfaceResult lResult = { nullptr, nullptr, 0u, false, false };

        IDirect3DDevice9* const lpDevice = Dev();
        if (lpDevice == nullptr)
            return lResult;

        // The adapter/device identity the format query needs. D3DADAPTER_DEFAULT +
        // D3DDEVTYPE_HAL is what the device was created with (see device.cpp); the creation
        // parameters are read back rather than assumed.
        D3DDEVICE_CREATION_PARAMETERS lCreation;
        std::memset(&lCreation, 0, sizeof(lCreation));
        lCreation.AdapterOrdinal = D3DADAPTER_DEFAULT;
        lCreation.DeviceType     = D3DDEVTYPE_HAL;
        lpDevice->GetCreationParameters(&lCreation);

        IDirect3D9* lpD3D = nullptr;
        D3DFORMAT   leAdapterFormat = D3DFMT_X8R8G8B8;
        if (SUCCEEDED(lpDevice->GetDirect3D(&lpD3D)) && lpD3D != nullptr)
        {
            D3DDISPLAYMODE lMode;
            if (SUCCEEDED(lpD3D->GetAdapterDisplayMode(lCreation.AdapterOrdinal, &lMode)))
                leAdapterFormat = lMode.Format;
        }

        const DepthFormatCandidate* const lpCandidates =
            lbWantRawDepthValue ? KA_DEPTH_LADDER_RAW : KA_DEPTH_LADDER_COMPARE;
        const u32 luCandidateCount = lbWantRawDepthValue ? 3u : 5u;

        for (u32 luCandidate = 0; luCandidate < luCandidateCount; ++luCandidate)
        {
            const D3DFORMAT leFormat = static_cast<D3DFORMAT>(lpCandidates[luCandidate].muFormat);

            if (lpD3D != nullptr
                && FAILED(lpD3D->CheckDeviceFormat(lCreation.AdapterOrdinal, lCreation.DeviceType,
                                                   leAdapterFormat, D3DUSAGE_DEPTHSTENCIL,
                                                   D3DRTYPE_TEXTURE, leFormat)))
            {
                continue;
            }

            IDirect3DTexture9* lpTexture = nullptr;
            if (SUCCEEDED(lpDevice->CreateTexture(luWidth, luHeight, 1, D3DUSAGE_DEPTHSTENCIL,
                                                  leFormat, D3DPOOL_DEFAULT, &lpTexture, nullptr))
                && lpTexture != nullptr)
            {
                IDirect3DSurface9* lpSurface = nullptr;
                if (SUCCEEDED(lpTexture->GetSurfaceLevel(0, &lpSurface)) && lpSurface != nullptr)
                {
                    lResult.mpTexture         = lpTexture;
                    lResult.mpSurface         = lpSurface;
                    lResult.muFormat          = lpCandidates[luCandidate].muFormat;
                    lResult.mbHardwareCompare = lpCandidates[luCandidate].mbHardwareCompare;
                    lResult.mbRawDepthValue   = lpCandidates[luCandidate].mbRawDepthValue;
                    if (lpD3D != nullptr)
                        lpD3D->Release();
                    return lResult;
                }
                lpTexture->Release();
            }
        }

        if (lpD3D != nullptr)
            lpD3D->Release();

        // Non-sampleable fallback: the pass renders, the receiver gets nothing.
        IDirect3DSurface9* lpPlainDepth = nullptr;
        if (SUCCEEDED(lpDevice->CreateDepthStencilSurface(luWidth, luHeight, D3DFMT_D24S8,
                                                          D3DMULTISAMPLE_NONE, 0, FALSE,
                                                          &lpPlainDepth, nullptr)))
        {
            lResult.mpSurface = lpPlainDepth;
        }
        return lResult;
    }

    // ---- resource carving -------------------------------------------------------------------
    // The console carves every render-target-side object out of the rw resource allocator the
    // parameters carry (RenderTarget::Initialize @0x82409B60 builds a { size, align } descriptor
    // and calls DoAllocate). Kept, so these objects live in the renderer's own arena rather than
    // forking a private heap.
    void* CarveZeroed(rw::IResourceAllocator* lpAllocator, u32 luSize, u32 luAlignment)
    {
        if (lpAllocator == nullptr)
            lpAllocator = rw::ResourceAllocatorRegistry::GetDefaultAllocator();
        if (lpAllocator == nullptr)
            return nullptr;

        rw::ResourceDescriptor lDescriptor;
        for (u32 luLane = 0; luLane < rw::KU_RESOURCE_LANE_COUNT; ++luLane)
        {
            lDescriptor.m_baseResourceDescriptors[luLane].m_size      = (luLane == 0) ? luSize : 0u;
            lDescriptor.m_baseResourceDescriptors[luLane].m_alignment = (luLane == 0) ? luAlignment : 1u;
        }

        rw::Resource lResource = lpAllocator->DoAllocate(lDescriptor, nullptr);
        void* lpBlock = lResource.m_baseResources[0];
        if (lpBlock != nullptr)
            std::memset(lpBlock, 0, luSize);
        return lpBlock;
    }

    // Wrap a created D3D texture in a renderengine::Texture so a sampler bind can consume it.
    // shadow::Device::SetResource -> D3DDevice_SetTexture reads mpD3DTexture straight off this
    // object (XenonD3D9Shims.cpp:1640), which is why the shadow map has to arrive as one.
    //
    // TWO TRAILING PARAMETERS ADDED (reflections step 1), both defaulted so every existing caller
    // is unchanged: the wrapper's DIMENSION and its depth. They matter because the env-map cube is
    // the first render target whose texture is not 2D, and the renderer's readiness test asks the
    // wrapper what it is -- BrnRendererModule::EnsureEnvMapTarget refuses the pass unless
    // renderengine::Texture::GetType(GetTexture(0)) == E_TYPE_CUBE. GetType answers from the raster
    // header's +0x28 dimension byte (texture.cpp TextureDimension), so the wrapper must carry it;
    // it also takes IDirect3DBaseTexture9* now, because an IDirect3DCubeTexture9 is not an
    // IDirect3DTexture9 (both derive from the base, which is what mpD3DTexture already is).
    renderengine::Texture* WrapTexture(rw::IResourceAllocator* lpAllocator,
                                       IDirect3DBaseTexture9* lpD3DTexture,
                                       u32 luFormat, u32 luWidth, u32 luHeight,
                                       renderengine::Texture::Type meDimension
                                           = renderengine::Texture::E_TYPE_2D,
                                       u32 luDepth = 1u)
    {
        if (lpD3DTexture == nullptr)
            return nullptr;

        renderengine::Texture* const lpTexture = static_cast<renderengine::Texture*>(
            CarveZeroed(lpAllocator, static_cast<u32>(sizeof(renderengine::Texture)), 16u));
        if (lpTexture == nullptr)
            return nullptr;

        lpTexture->mpD3DTexture   = lpD3DTexture;
        lpTexture->miFormat       = static_cast<s32>(luFormat);
        lpTexture->muWidth        = static_cast<u16>(luWidth);
        lpTexture->muHeight       = static_cast<u16>(luHeight);
        lpTexture->muDepth        = static_cast<u8>(luDepth);
        lpTexture->muNumMipLevels = 1;
        // Same encoding rule as Texture::Create: 0 spells E_TYPE_2D, and only a dimension that was
        // really built is recorded.
        lpTexture->mu8Dimension =
            (meDimension == renderengine::Texture::E_TYPE_CUBE
             || meDimension == renderengine::Texture::E_TYPE_VOLUME)
                ? static_cast<u8>(meDimension) : 0u;
        return lpTexture;
    }

    // =========================================================================================
    // THE CUBE RENDER-TARGET RECORD  [reflections step 1 -- seam S1]
    //
    // WHY A SIDE RECORD RATHER THAN A MEMBER. rw::graphics::postfx::Target::Resolve(u32) is a
    // method on the Target, and on PC it needs two objects the console's Target does not have a
    // slot for: the IDirect3DCubeTexture9 that is the resolve DESTINATION, and the face-independent
    // SCRATCH render-target surface that is the resolve SOURCE. The console's own "the surface that
    // was rendered into" slot is mpPixelBuffer, a renderengine::PixelBuffer::SurfaceHeader* -- an
    // EDRAM GPU descriptor. Storing an IDirect3DSurface9* through it would be a type pun on a
    // member whose real type is declared in the shared header, i.e. exactly the split-brain this
    // project keeps paying for. So the association is kept beside the object, in the same idiom
    // this file already uses twice (gaShadowRtLatch, gaDepthTargetRecords), and NOTHING in the
    // render path reads it except Resolve(face).
    //
    // FOUR SLOTS: the pool has exactly one cube target (E_RENDER_TARGET_ENV_MAP), created once.
    //
    // LIFETIME -- NOTHING RELEASES THESE, AND THERE IS NOWHERE TO PUT A RELEASE (cubeleaf run-2
    // F9, re-checked 2026-08-17). The rows are never cleared and neither mpCube nor mpScratch is
    // ever Released, exactly like gaDepthTargetRecords and gaShadowRtLatch beside them. That is
    // not an oversight in this record: THIS BUILD HAS NO RENDER-TARGET TEARDOWN PATH AT ALL.
    //   * rw::graphics::postfx::RenderTarget / Target declare no Release/Destroy in the tree --
    //     the DWARF's `Target::Release()` (rwgpfxrendertarget.h:379) was never reconstructed and
    //     `git grep -nE "postfx::(RenderTarget|Target)::(Release|Destroy)" -- src` is empty.
    //   * the owner's destructor is EMPTY ON THE CONSOLE: CgsRenderTarget::Destruct @0x8284CB38 is
    //     a lone `blr` (ICF-folded), reconstructed as an empty body at CgsRenderTarget.cpp:247.
    //     The PS3 build has a real teardown there; the X360 target does not.
    //   * there is no device-lost / Reset path either: the only `OnLostDevice` in the whole tree
    //     is the word inside Initialize's own comment, and nothing calls IDirect3DDevice9::Reset.
    // So a release added here would have no caller, and inventing one would be inventing a
    // lifetime the target build does not have.
    // DELETE-WHEN a device-lost/Reset path lands: at that point gaCubeTargetRecords needs the same
    // OnLostDevice/OnResetDevice treatment as every other D3DPOOL_DEFAULT object this leaf makes
    // (the cube texture, the multisampled scratch, gpNullColourSurface, the depth surfaces), and
    // the rows must be cleared so a RenderTarget re-created at a different address cannot match a
    // stale mpCube/mpScratch. RegisterCubeTarget already handles re-creation at the SAME address
    // (it matches on mpTarget before it takes a free slot).
    // =========================================================================================
    struct CubeTargetRecord
    {
        const void*            mpTarget;     // &RenderTarget::maColourTargets[0]
        IDirect3DCubeTexture9* mpCube;       // the resolve destination (RENDERTARGET/DEFAULT)
        IDirect3DSurface9*     mpScratch;    // the face-independent surface section 0 binds
        u32                    muEdge;
    };
    CubeTargetRecord gaCubeTargetRecords[4] = {};

    void RegisterCubeTarget(const void* lpTarget, IDirect3DCubeTexture9* lpCube,
                            IDirect3DSurface9* lpScratch, u32 luEdge)
    {
        for (u32 lu = 0; lu < 4u; ++lu)
        {
            if (gaCubeTargetRecords[lu].mpTarget == lpTarget
                || gaCubeTargetRecords[lu].mpTarget == nullptr)
            {
                gaCubeTargetRecords[lu].mpTarget  = lpTarget;
                gaCubeTargetRecords[lu].mpCube    = lpCube;
                gaCubeTargetRecords[lu].mpScratch = lpScratch;
                gaCubeTargetRecords[lu].muEdge    = luEdge;
                return;
            }
        }
    }

    const CubeTargetRecord* FindCubeTarget(const void* lpTarget)
    {
        for (u32 lu = 0; lu < 4u; ++lu)
        {
            if (gaCubeTargetRecords[lu].mpTarget == lpTarget
                && gaCubeTargetRecords[lu].mpCube != nullptr)
                return &gaCubeTargetRecords[lu];
        }
        return nullptr;
    }

    // =========================================================================================
    // BRN_ENVMAP_DEBUG -- the FACE-ORIENTATION probe. OFF by default, environment variable only,
    // DELETE-WHEN the face orientation has been read off a boot screenshot and written down.
    //
    // WHAT IS AND IS NOT SETTLED (seam S4). The face ORDER is settled: the console's face table
    // (BrnEnvironmentMap.cpp:16-34, KAV_ENV_MAP_LOOK_DIRECTIONS = +X,-X,+Y,-Y,+Z,-Z with ups
    // +Y,+Y,-Z,+Z,+Y,+Y) is index-for-index D3D9's D3DCUBEMAP_FACE_POSITIVE_X..NEGATIVE_Z
    // (Windows Kit 10.0.26100.0 shared/d3d9types.h:1680-1690), so face n needs no remap and this
    // Resolve does none. What is NOT settled from the tree alone is HANDEDNESS: the producer builds
    // the face camera with Camera::SetPerspectiveProjectionMatrixRightHanded and negated near/far
    // (envproducer's finding F4), while D3D9's cube lookup is the left-handed convention -- which
    // shows up as a MIRRORED face, not a wrong one.
    //
    // A SYNTHETIC FILL CANNOT SHOW A MIRROR EITHER (verify F2), so the probe is ADDITIVE: the real
    // resolved face is kept and only its TOP-LEFT QUADRANT (the texel-(0,0) corner) is painted the
    // face's identity colour. On a screenshot of a car the reflected world itself shows a mirror or
    // a hemisphere swap against the actual scene, and the coloured corner says WHICH face the paint
    // sampled and where that face's origin sits.
    // =========================================================================================
    bool EnvMapDebugFaceColours()
    {
        static int siState = -1;
        if (siState < 0)
        {
            const char* const lpcEnv = std::getenv("BRN_ENVMAP_DEBUG");
            siState = (lpcEnv != nullptr && lpcEnv[0] != '\0' && lpcEnv[0] != '0') ? 1 : 0;
        }
        return siState != 0;
    }

    // 0:+X red, 1:-X cyan, 2:+Y green, 3:-Y magenta, 4:+Z blue, 5:-Z yellow -- the SAME legend
    // BrnRendererModule.cpp's EnvMapDebugFaceColours prints at start-up (verify F1, cubeleaf run 2:
    // the two tables disagreed; complementary hues survive the paint's fresnel attenuation where a
    // 255-vs-128 channel pair does not).
    const D3DCOLOR KA_ENVMAP_DEBUG_FACE_COLOURS[6] =
    {
        D3DCOLOR_ARGB(255, 255,   0,   0), D3DCOLOR_ARGB(255,   0, 255, 255),
        D3DCOLOR_ARGB(255,   0, 255,   0), D3DCOLOR_ARGB(255, 255,   0, 255),
        D3DCOLOR_ARGB(255,   0,   0, 255), D3DCOLOR_ARGB(255, 255, 255,   0)
    };
    const char* const KAPC_ENVMAP_DEBUG_FACE_NAMES[6] =
    { "+X red", "-X cyan", "+Y green", "-Y magenta", "+Z blue", "-Z yellow" };

    // ---- the [shadow-rt] diagnostic ---------------------------------------------------------
    // VALUE-latched, not one-shot: a `static bool` fires on the loading screen (before any
    // target exists) and then never again, which is how this build lost the last two render
    // diagnostics. This re-reports whenever the reported tuple CHANGES, so the first line is
    // emitted the moment a real target appears and any later change (a lost device, a null
    // depth texture) shows up too.
    // Latched PER TARGET (2026-08-15): GetDepthStencilTexture runs once a frame for EVERY
    // depth-sampled target -- the shadow map AND the post-fx down-sample buffer -- and a single
    // global tuple flipped between the two every frame, re-printing 3-4 lines a frame (14,485
    // lines in one drive; each a fputs+fflush). One slot per target object; the first few
    // targets created are the only ones that ever reach the s15 bind.
    struct ShadowRtLatch
    {
        const void* mpTarget;
        u32 muWidth, muHeight, muSections, muDepthValid, muFormat;
    };
    ShadowRtLatch gaShadowRtLatch[8] = {};

    // =========================================================================================
    // THE PER-TARGET DEPTH CREATION RECORD (added with the two-ladder split, 2026-08-15).
    //
    // WHY IT HAD TO EXIST. GetDepthStencilTexture re-reports its target's tuple every frame,
    // and it used to read guCreatedFormat / gbCreatedHardwareCompare -- which are the SHADOW
    // MAP's globals. With one ladder that lie was invisible (every target got D24X8, so the
    // shadow's tuple happened to describe the down-sample buffer too). With two ladders it
    // would print `depthFormat=D24X8 compare=HW` on a target that is actually INTZ/RAW -- a
    // log that says the opposite of the truth, on the exact axis this wave exists to fix.
    //
    // It is a DIAGNOSTIC record keyed by target pointer, not new engine state: nothing in the
    // render path reads it, and the shadow-sampler seam (guCreatedFormat below) is untouched.
    // Eight slots for the same reason the latch has eight -- the pool creates a handful of
    // depth targets once each and never again.
    // =========================================================================================
    struct DepthTargetRecord
    {
        const void* mpTarget;
        u32         muWidth, muHeight, muSections, muFormat;
        bool        mbHardwareCompare;
        bool        mbRawDepthValue;
        bool        mbShadowMap;
    };
    DepthTargetRecord gaDepthTargetRecords[8] = {};

    void RegisterDepthTarget(const void* lpTarget, u32 luWidth, u32 luHeight, u32 luSections,
                             u32 luFormat, bool lbHardwareCompare, bool lbRawDepthValue,
                             bool lbShadowMap)
    {
        for (u32 lu = 0; lu < 8u; ++lu)
        {
            if (gaDepthTargetRecords[lu].mpTarget == lpTarget
                || gaDepthTargetRecords[lu].mpTarget == nullptr)
            {
                gaDepthTargetRecords[lu].mpTarget          = lpTarget;
                gaDepthTargetRecords[lu].muWidth           = luWidth;
                gaDepthTargetRecords[lu].muHeight          = luHeight;
                gaDepthTargetRecords[lu].muSections        = luSections;
                gaDepthTargetRecords[lu].muFormat          = luFormat;
                gaDepthTargetRecords[lu].mbHardwareCompare = lbHardwareCompare;
                gaDepthTargetRecords[lu].mbRawDepthValue   = lbRawDepthValue;
                gaDepthTargetRecords[lu].mbShadowMap       = lbShadowMap;
                return;
            }
        }
    }

    const DepthTargetRecord* FindDepthTarget(const void* lpTarget)
    {
        for (u32 lu = 0; lu < 8u; ++lu)
        {
            if (gaDepthTargetRecords[lu].mpTarget == lpTarget)
                return &gaDepthTargetRecords[lu];
        }
        return nullptr;
    }

    // Spell a depth format for the log: a FOURCC prints as its four characters, a real
    // D3DFORMAT enumerant as its name.
    void FormatName(u32 luFormat, char* lpcOut, size_t luSize)
    {
        switch (luFormat)
        {
        case 0u:                             std::snprintf(lpcOut, luSize, "D24S8-surface"); return;
        case static_cast<u32>(D3DFMT_D24X8): std::snprintf(lpcOut, luSize, "D24X8");         return;
        case static_cast<u32>(D3DFMT_D16):   std::snprintf(lpcOut, luSize, "D16");           return;
        // ADDED (reflections step 1): the MSAA arm has been storing lMultisample.meDepthFormat
        // here since rung 8, and D3DFMT_D24S8 (75) is not a FOURCC -- so it fell through to the
        // four-character spelling below and printed control bytes. Same for the cube target's
        // plain depth-stencil surface, which is created from the same ladder.
        case static_cast<u32>(D3DFMT_D24S8): std::snprintf(lpcOut, luSize, "D24S8");         return;
        default: break;
        }
        // Anything else on the ladder is a FOURCC (INTZ / DF24 / DF16).
        const char lacChars[5] =
        {
            static_cast<char>(luFormat & 0xFFu),
            static_cast<char>((luFormat >> 8) & 0xFFu),
            static_cast<char>((luFormat >> 16) & 0xFFu),
            static_cast<char>((luFormat >> 24) & 0xFFu),
            '\0'
        };
        std::snprintf(lpcOut, luSize, "%s", lacChars);
    }

    // The DEPTH-target reporter. It used to print only `[shadow-rt]` because the shadow map was
    // the only depth-sampled target that mattered; it now takes the LINE PREFIX so the post-fx
    // depth targets report under `[postfx-rt]` (their colour line already does) with their OWN
    // format and their OWN semantic. The shadow map's prefix is passed as the same
    // "[shadow-rt] target" it always printed, so that line is byte-identical to today's except
    // for the failure spelling below. Everything else -- the per-target latch, the
    // value-latched re-report -- is unchanged.
    //   compare=HW    the fetch COMPARES (D24X8 / D16): the shadow-map semantic.
    //   compare=RAW   the fetch returns the stored depth VALUE (INTZ / DF24 / DF16).
    //   compare=NONE  no sampleable depth texture at all (the plain D24S8 fallback).
    void ReportDepthRenderTarget(const char* lpcTag, const void* lpTarget,
                                 u32 luWidth, u32 luHeight, u32 luSections,
                                 bool lbDepthTextureValid, u32 luFormat, bool lbHardwareCompare,
                                 bool lbRawDepthValue)
    {
        const u32 luDepthValid = lbDepthTextureValid ? 1u : 0u;
        ShadowRtLatch* lpSlot = nullptr;
        for (u32 lu = 0; lu < 8u; ++lu)
        {
            if (gaShadowRtLatch[lu].mpTarget == lpTarget || gaShadowRtLatch[lu].mpTarget == nullptr)
            {
                lpSlot = &gaShadowRtLatch[lu];
                break;
            }
        }
        if (lpSlot == nullptr)
            return;                       // more than 8 depth-sampled targets: stop reporting
        if (lpSlot->mpTarget == lpTarget
            && luWidth == lpSlot->muWidth && luHeight == lpSlot->muHeight
            && luSections == lpSlot->muSections && luDepthValid == lpSlot->muDepthValid
            && luFormat == lpSlot->muFormat)
        {
            return;
        }
        lpSlot->mpTarget     = lpTarget;
        lpSlot->muWidth      = luWidth;
        lpSlot->muHeight     = luHeight;
        lpSlot->muSections   = luSections;
        lpSlot->muDepthValid = luDepthValid;
        lpSlot->muFormat     = luFormat;

        char lacFormat[16];
        FormatName(luFormat, lacFormat, sizeof(lacFormat));

        const char* lpcCompare = "NONE";
        if (lbHardwareCompare)
            lpcCompare = "HW";
        else if (lbRawDepthValue)
            lpcCompare = "RAW";

        char lacMessage[224];
        std::snprintf(lacMessage, sizeof(lacMessage),
                      "%s %ux%u sections=%u depthFormat=%s depthTexture=%s"
                      " compare=%s\n",
                      lpcTag,
                      static_cast<unsigned>(luWidth), static_cast<unsigned>(luHeight),
                      static_cast<unsigned>(luSections), lacFormat,
                      lbDepthTextureValid ? "OK" : "NONE",
                      lpcCompare);
        CgsDev::Log::WriteToLog(lacMessage);
    }

    // The sibling reporter for every NON-depth-sampled target -- the post-fx spine's scene colour
    // buffer and the rest of the pool. Kept separate from ReportShadowRenderTarget because that one
    // owns the shadow tuple and its once-only de-duplication state; a colour target flowing through
    // it would both clobber that state and print a shadow line about something that is not a
    // shadow. Unconditional (there are a handful of these per run, created once each).
    // `msaa=` ADDED (rung 8): the sample count the target was actually created at, so the boot log
    // proves per target which one multisamples and which do not. It is printed for EVERY target
    // (msaa=1 == D3DMULTISAMPLE_NONE) because "the line is absent" and "the line says 1" are the
    // same picture and only one of them is checkable.
    // `colourTexture=MSAA` is NOT a failure spelling: a multisampled colour section legitimately
    // owns no texture on D3D9 (see the MSAA banner above); `NULL` still means a CreateTexture that
    // was asked for and failed, which is the condition this field was added to catch.
    void ReportRenderTarget(u32 luWidth, u32 luHeight, u32 luSections,
                            u32 luColourMode, u32 luDepthStencilMode, bool lbColourTextureValid,
                            u32 luSamples)
    {
        char lacMessage[224];
        std::snprintf(lacMessage, sizeof(lacMessage),
                      "[postfx-rt] target %ux%u sections=%u colourMode=%u depthMode=%u"
                      " colourTexture=%s msaa=%u\n",
                      static_cast<unsigned>(luWidth), static_cast<unsigned>(luHeight),
                      static_cast<unsigned>(luSections), static_cast<unsigned>(luColourMode),
                      static_cast<unsigned>(luDepthStencilMode),
                      // ⚠ ORDER CORRECTED (reflections step 1): a VALID texture is reported as OK
                      // even on a multisampled target. `MSAA` means "multisampled and therefore
                      // legitimately textureless" (the anti-alias scene target), and the CUBE
                      // target below is multisampled AND owns a texture -- reporting that as
                      // `MSAA` would hide the one fact rule 9 wants this line to carry.
                      lbColourTextureValid ? "OK" : ((luSamples > 1u) ? "MSAA" : "NULL"),
                      static_cast<unsigned>(luSamples));
        CgsDev::Log::WriteToLog(lacMessage);
    }

    // The CUBE target's own creation line (reflections step 1). Separate from ReportRenderTarget
    // because the facts that make a cube target non-degenerate are different ones: the D3D resource
    // TYPE actually created, the face count, the scratch surface that section 0 binds, and the
    // depth format -- printed once, at creation, by the side that made the objects. Rule 9 of this
    // wave: "the frame looks identical" is not a gate for an off-screen target, so print the extent
    // and the object type and refuse a degenerate one.
    void ReportCubeRenderTarget(u32 luEdge, u32 luSamples, const void* lpScratch,
                                u32 luDepthFormat, IDirect3DCubeTexture9* lpCube)
    {
        char lacDepth[16];
        FormatName(luDepthFormat, lacDepth, sizeof(lacDepth));

        // The D3D resource type, read back from the object rather than assumed (D3DRTYPE_CUBETEXTURE
        // == 4). A creation that silently produced anything else is exactly what this line exists
        // to expose.
        const unsigned luD3DType =
            (lpCube != nullptr) ? static_cast<unsigned>(lpCube->GetType()) : 0u;

        char lacMessage[224];
        std::snprintf(lacMessage, sizeof(lacMessage),
                      "[envmap-rt] cube %ux%u faces=6 msaa=%u scratch=%p depth=%s"
                      " d3dType=%u (4==D3DRTYPE_CUBETEXTURE)\n",
                      static_cast<unsigned>(luEdge), static_cast<unsigned>(luEdge),
                      static_cast<unsigned>(luSamples), lpScratch, lacDepth, luD3DType);
        CgsDev::Log::WriteToLog(lacMessage);
    }

    // The format + section count THE SHADOW-MAP TARGET was created with, kept so
    // GetDepthStencilTexture can re-report the same tuple without re-deriving it (neither is a
    // console member of RenderTarget and neither is worth inventing one for).
    //
    // âš ï¸ THESE DESCRIBE THE SHADOW TARGET SPECIFICALLY, NOT "the last target Initialize saw".
    // Initialize used to latch them unconditionally, which was correct only while exactly ONE
    // render target existed in the whole build. The post-fx spine (2026-08-13) creates a SECOND
    // one -- the off-screen scene colour buffer, which has no depth texture at all -- and an
    // unconditional latch would have set guCreatedFormat = 0 / gbCreatedHardwareCompare = false
    // the moment it was built. renderengine::ShadowSampler_ApplyState (XenonD3D9Shims.cpp:2909)
    // reads exactly that flag to choose sampler 15's filter, so shadow sampling would have
    // fallen from LINEAR (the hardware PCF the depth-compare formats give us) to POINT,
    // and guCreatedSections would have fallen 3 -> 1 and mis-reported the cascade atlas.
    // Nothing would have crashed, nothing would have printed a warning, and the shadows would
    // just have got worse -- the same shape of single-consumer-global bug as the shadow campaign's
    // own defects. The latch below is therefore gated on the target being the one that asked to be
    // SAMPLED AS DEPTH, which is the shadow map and only the shadow map.
    u32  guCreatedFormat          = 0u;
    bool gbCreatedHardwareCompare = false;
    u32  guCreatedSections        = 1u;
}

// The two accessors ShadowPassPCLeaf.h declares (see the SAMPLE SEMANTICS SEAM banner there):
// which depth format the shadow target actually got, and whether that format's fetch COMPARES.
// They read the same two file statics Initialize wrote, so no state is duplicated.
namespace renderengine
{
    u32  ShadowDepthFormat()                  { return guCreatedFormat; }
    bool ShadowDepthFormatIsHardwareCompare() { return gbCreatedHardwareCompare; }
}

// =============================================================================
// renderengine::Device::SetState(const RenderTargetState*)
//
// Declared at pc/gcm/renderengine/device.h:40 and defined NOWHERE in the tree until now (the
// only Device::SetState that existed was shadow::Device::SetState(void*, u32), a different
// class on a different device -- which is why the audit kept reporting it unresolved).
//
// Console shape: install the surface descriptor on the GPU. PC: bind the state's colour and
// depth-stencil surfaces on the D3D9 device.
// =============================================================================
namespace renderengine
{
    // X360 dword_83010A30 -- THE single "last state installed" shadow (see the banner in
    // ShadowPassPCLeaf.h for why it lives here and why it MUST be shared).
    const RenderTargetState* gpLastRenderTargetState = nullptr;

    void Device::SetState(const RenderTargetState* lpState)
    {
        IDirect3DDevice9* const lpDevice = Dev();
        if (lpDevice == nullptr || lpState == nullptr)
            return;

        // FLAG PC bring-up: a depth-only state still needs SOMETHING on colour slot 0 -- D3D9
        // rejects a null render target there. See AcquireNullColourSurface.
        IDirect3DSurface9* lpColour = lpState->mpColourSurface;
        if (lpColour == nullptr)
            lpColour = AcquireNullColourSurface(lpState->muWidth, lpState->muHeight);

        // ⚠ A DEPTH TEXTURE CANNOT BE A SAMPLER SOURCE AND THE DEPTH-STENCIL TARGET AT THE SAME
        // TIME (added with the raw-depth ladder, 2026-08-15). D3D9 -- unlike D3D10+ -- does NOT
        // silently unbind the sampler for you: the write path keeps working while the SAMPLE
        // becomes undefined, and on some drivers the depth WRITE is what degrades. The composite
        // now leaves the down-sample buffer's INTZ depth texture bound at unit 4 (SamplerDepth)
        // from one frame to the next, so the very next Begin() on that target would re-bind the
        // same texture's surface as the depth-stencil underneath it.
        //
        // The unbind goes through shadow::Device::SetResource(nullptr, unit) rather than a raw
        // SetTexture so the shadow CACHE is invalidated too. A raw unbind would leave
        // mapTextureState[unit] still holding the composite's TextureState, and the next
        // SetState(state, unit) would take its HIT path and skip the rebind -- the shader would
        // then sample an unbound unit and nothing would report it. (This is the same class of
        // bug as the gpLastRenderTargetState split-brain in ShadowPassPCLeaf.h's banner.)
        //
        // Costs nothing when no raw-depth texture is bound: the mask is zero and the loop does
        // not run. Only units that received a raw-depth texture are ever in it.
        if (lpState->mpDepthSurface != nullptr)
        {
            u32 luRawDepthUnits = renderengine::PostFxDepthSampler_BoundUnitMask();
            for (u32 luUnit = 0; luRawDepthUnits != 0u && luUnit < 16u; ++luUnit)
            {
                if ((luRawDepthUnits & (1u << luUnit)) != 0u)
                {
                    luRawDepthUnits &= ~(1u << luUnit);
                    shadow::Device::SetResource(nullptr, luUnit);
                }
            }
        }

        // ORDER MATTERS on D3D9: the bound depth-stencil surface must be at least as large as the
        // render target, and that invariant is checked on SetRenderTarget. Going straight from the
        // (small) back buffer's depth surface to the 1280x1920 shadow colour target would violate
        // it mid-swap, so the depth surface is dropped first and re-bound after.
        lpDevice->SetDepthStencilSurface(nullptr);
        if (lpColour != nullptr)
            lpDevice->SetRenderTarget(0, lpColour);
        lpDevice->SetDepthStencilSurface(lpState->mpDepthSurface);

        // The bound colour target's sample count is an input of the alpha-to-coverage
        // derivation (XenonD3D9Shims.cpp AlphaCoverage_Reconcile); this is the one writer of
        // that input, so it reconciles -- LAST, after both surfaces are in place.
        PCAlphaCoverage_Reconcile();
    }

    // =========================================================================
    // [PC-platform leaf] PCInstallDefaultRenderTargetState -- see ShadowPassPCLeaf.h.
    //
    // Captured ONCE, immediately after CreateDevice, while the swap chain's back buffer and
    // the auto depth-stencil are exactly what the device has bound: GetRenderTarget(0) /
    // GetDepthStencilSurface here ARE those two surfaces (nothing has rebound yet). Both
    // AddRef'd for the device's lifetime -- the console's default state lives as long as the
    // device does, and this build has no Reset. Extent = the swap chain's, which is what
    // Device::SetState's AcquireNullColourSurface / D3D9's depth>=target rule need.
    // =========================================================================
    void PCInstallDefaultRenderTargetState(u32 luWidth, u32 luHeight)
    {
        static RenderTargetState sDefaultState = { nullptr, nullptr, 0u, 0u };

        IDirect3DDevice9* const lpDevice = Dev();
        if (lpDevice == nullptr)
            return;
        if (rw::graphics::postfx::gpDefaultRenderTargetState != nullptr)
            return;                                        // installed once per device

        IDirect3DSurface9* lpColour = nullptr;
        IDirect3DSurface9* lpDepth  = nullptr;
        if (FAILED(lpDevice->GetRenderTarget(0, &lpColour)))       // AddRefs; kept for the device's life
            lpColour = nullptr;
        if (FAILED(lpDevice->GetDepthStencilSurface(&lpDepth)))     // AddRefs; kept for the device's life
            lpDepth = nullptr;

        if (lpColour == nullptr)
        {
            CgsDev::Log::WriteToLog("[postfx-rt] default render-target state NOT installed: no back buffer bound\n");
            if (lpDepth != nullptr) lpDepth->Release();
            return;
        }

        sDefaultState.mpColourSurface = lpColour;
        sDefaultState.mpDepthSurface  = lpDepth;
        sDefaultState.muWidth         = luWidth;
        sDefaultState.muHeight        = luHeight;
        rw::graphics::postfx::gpDefaultRenderTargetState = &sDefaultState;

        char lacMsg[160];
        std::snprintf(lacMsg, sizeof(lacMsg),
                      "[postfx-rt] default render-target state installed: %ux%u colour=%p depth=%p\n",
                      static_cast<unsigned>(luWidth), static_cast<unsigned>(luHeight),
                      static_cast<void*>(lpColour), static_cast<void*>(lpDepth));
        CgsDev::Log::WriteToLog(lacMsg);
    }
}

namespace rw
{
namespace graphics
{
namespace postfx
{
    // =========================================================================
    // gpDefaultRenderTargetState -- THE canonical definition.
    //
    // It was declared `renderengine::RenderTargetState*` in rwgpfxrendertarget.h:260 and then
    // defined, twice, as a file-local `const renderengine::RenderTargetState*` (in
    // CgsRenderTarget.cpp and in the parked rwgpfxrendertarget.cpp) -- a type mismatch against
    // the declaration AND a duplicate. Both file-local copies were dead: nothing could ever
    // assign to them, so every "fall back to the default state" path fell back to null.
    // This is the single non-const definition the header declares.
    //
    // Null until renderengine::Device::Start installs it -- the console's own placement (X360
    // dword_83271614) -- through PCInstallDefaultRenderTargetState below: the swap chain's back
    // buffer + the auto depth-stencil, i.e. the device's own surfaces. Before that (and on a
    // build without a device) the callers' `if (state == nullptr)` guards and Device::SetState's
    // null-ignore keep whatever is bound, as they always did.
    // =========================================================================
    renderengine::RenderTargetState* gpDefaultRenderTargetState = nullptr;

    // =========================================================================
    // RenderTarget::Parameters::Parameters()  (X360 rw__graphics__postfx__RenderTarget__Parameters__Parameters,
    // called out of line from CgsRenderTarget::Construct @0x827ECCCC)
    //
    // The console body is not exported, but its CONTRACT is pinned by its only caller:
    // CgsRenderTarget::Construct writes every field it cares about immediately afterwards, and
    // the fields it never writes (pitch, use-stencil, hi-Z address, compression base, shared
    // buffer address, the provided-target pointers) must arrive as "nothing requested". That is
    // an all-zero block -- both modes eRenderTarget_NONE, every pointer null, every count 0.
    // Zeroing is therefore recovered from the caller, not guessed.
    // =========================================================================
    RenderTarget::Parameters::Parameters()
    {
        std::memset(this, 0, sizeof(*this));
    }

    // =========================================================================
    // RenderTarget::Initialize(const Parameters&)   [PC realisation of X360 0x82409B60]
    //
    // Console: carve the object from the allocator, then Construct() builds the EDRAM colour /
    // depth surfaces and CreateStates() builds the per-section GPU surface descriptors.
    // PC: carve the object the same way, then create the D3D9 surfaces and fill the section
    // states with them.
    //
    // SECTIONS. The console's depth SURFACE is one section tall (1280x640 of EDRAM) and its
    // depth TEXTURE is numSections tall (1280x1920) -- Target::CreateDepth @0x82403688 computes
    // `textureHeight = numSections * height` -- because each cascade is rendered into the same
    // EDRAM surface and RESOLVED into its own band of the texture. PC has no resolve: the
    // texture IS the surface. So the surface created here is the full numSections*height extent
    // and all section states reference it; the per-cascade band is selected by the VIEWPORT.
    // That is why BrnRendererMemory::PCBringUpCreateShadowMapBufferOnly describes the shadow
    // target as ONE section at the combined 1280x1920 -- it makes
    // ShadowMapRenderManager::BeginRenderShadowMap take its own single-section branch, which is
    // console code that already packs the faces into vertical thirds, and lands cascade n on
    // rows [n*640, (n+1)*640) -- exactly the 1x3 vertical-strip atlas the recovered
    // ShadowMap_WorldToLight / ShadowMap_Constants matrices encode.
    // =========================================================================
    RenderTarget* RenderTarget::Initialize(const Parameters& lrParameters)
    {
        rw::IResourceAllocator* lpAllocator = lrParameters.mpAllocator;
        if (lpAllocator == nullptr)
            lpAllocator = rw::ResourceAllocatorRegistry::GetDefaultAllocator();

        RenderTarget* const lpRenderTarget = static_cast<RenderTarget*>(
            CarveZeroed(lpAllocator, static_cast<u32>(sizeof(RenderTarget)), 16u));
        if (lpRenderTarget == nullptr)
            return nullptr;

        lpRenderTarget->mpAllocator        = lpAllocator;
        lpRenderTarget->muWidth            = lrParameters.mu32Width;
        lpRenderTarget->muHeight           = lrParameters.mu32Height;
        lpRenderTarget->muColourMode       = lrParameters.mColourMode;
        lpRenderTarget->muDepthStencilMode = lrParameters.mDepthStencilMode;
        lpRenderTarget->mu8HasColour       = static_cast<u8>(
            (lrParameters.mColourMode != static_cast<u32>(eRenderTarget_NONE)) ? 1u : 0u);

        const u32 luNumSections = (lrParameters.mu8NumSections != 0u) ? lrParameters.mu8NumSections : 1u;

        // The full surface extent (see the SECTIONS note above).
        const u32 luSurfaceWidth  = lrParameters.mu32Width;
        const u32 luSurfaceHeight = lrParameters.mu32Height * luNumSections;

        IDirect3DSurface9* lpColourSurface  = nullptr;
        IDirect3DSurface9* lpDepthSurface   = nullptr;
        u32                luDepthFormat    = 0u;
        bool               lbDepthHwCompare = false;
        bool               lbDepthRawValue  = false;

        // --- WHICH DEPTH SEMANTIC DOES THIS TARGET NEED? (see the two-ladder banner above) ----
        // Decided from the PARAMETERS, before anything is created, because it selects the ladder.
        // The discriminator is the attested one: the shadow map is the only pool target that asks
        // for a sampleable depth while having NO COLOUR AT ALL. Everything else that asks to be
        // sampled (down-sample, particle, env-map) is a post-fx target reading depth VALUES.
        const bool lbWantsSampleableDepth =
            (lrParameters.mDepthStencilMode == static_cast<u32>(eRenderTarget_CREATE))
            && lrParameters.mbUseDepthStencilAsTexture;
        const bool lbIsShadowMapTarget =
            lbWantsSampleableDepth
            && (lrParameters.mColourMode == static_cast<u32>(eRenderTarget_NONE));
        const bool lbWantRawDepthValue = lbWantsSampleableDepth && !lbIsShadowMapTarget;

        // --- IS THIS A CUBE TARGET? (reflections step 1 -- seam S1) ---------------------------
        // The discriminator is the one the console itself writes and nothing else in the pool sets:
        // colour section 0's TEXTURE TYPE. BrnRendererMemory::CreateEnvmapBuffer @0x823F6C88 does
        // `SetTextureType(0, renderengine::Texture::E_TYPE_CUBE)` (X360 `li r7, 3` -> the colour
        // record's mu32TextureType @0x823F6D40) and CgsRenderTarget::Construct forwards it verbatim
        // into the Target::Parameters block (CgsRenderTarget.cpp:173
        // `lrColour.SetTextureType(lrSurface.mu32TextureType)`). Every other pool target leaves the
        // constructor's E_TYPE_2D (CgsRenderTarget.cpp:110 `mu32TextureType = 1;`), so this selects
        // the env map and only the env map -- it is not a size or a format heuristic.
        const bool lbIsCubeTarget =
            (lrParameters.mColourMode == static_cast<u32>(eRenderTarget_CREATE))
            && (lrParameters.maColourTargetParams[0].mTextureType
                == static_cast<u32>(renderengine::Texture::E_TYPE_CUBE));

        // A cube face is square. REFUSE rather than build a degenerate target that errors nowhere
        // (this wave's rule 9); the refusal leaves GetTexture(0) null, which is what
        // BrnRendererModule::EnsureEnvMapTarget tests before it turns the pass on.
        const bool lbCubeExtentUsable =
            lbIsCubeTarget && (lrParameters.mu32Width != 0u)
            && (lrParameters.mu32Width == lrParameters.mu32Height)
            && (luNumSections == 1u);
        if (lbIsCubeTarget && !lbCubeExtentUsable)
        {
            char lacCubeRefused[192];
            std::snprintf(lacCubeRefused, sizeof(lacCubeRefused),
                          "[envmap-rt] REFUSED: a CUBE target must be square with one section,"
                          " asked for %ux%u sections=%u\n",
                          static_cast<unsigned>(lrParameters.mu32Width),
                          static_cast<unsigned>(lrParameters.mu32Height),
                          static_cast<unsigned>(luNumSections));
            CgsDev::Log::WriteToLog(lacCubeRefused);
        }
        IDirect3DCubeTexture9* lpCubeTexture = nullptr;

        // --- DOES THIS TARGET MULTISAMPLE? (rung 8 -- see the MSAA banner above) --------------
        // The word CgsRenderTarget::Construct forwarded into the Target::Parameters blocks. Colour
        // and depth carry the SAME value (both are written from the one
        // CgsRenderTarget::mn32MultiSampleFormat at CgsRenderTarget.cpp:174 / :220), so it is read
        // from the colour section when the target has one and from the depth block otherwise --
        // which keeps a colour-less target (the shadow map, format 0) answering from its own
        // record rather than from an unwritten colour slot.
        const u32 luConsoleMultisampleFormat =
            (lrParameters.mColourMode != static_cast<u32>(eRenderTarget_NONE))
            ? lrParameters.maColourTargetParams[0].mu32MultiSampleFormat
            : lrParameters.mDepthStencilParams.mu32MultiSampleFormat;

        const u32 luConsoleSamples =
            MultisampleSampleCountForConsoleFormat(luConsoleMultisampleFormat);

        // The knob only RE-SCALES a target the console already multisamples; it never turns MSAA
        // on for one the console leaves single-sampled (see the banner).
        u32 luRequestedSamples = luConsoleSamples;
        if (luConsoleSamples > 1u)
        {
            const u32 luOverrideSamples = MultisampleOverrideSampleCount();
            if (luOverrideSamples != 0u)
                luRequestedSamples = luOverrideSamples;
        }

        // A8R8G8B8 is the colour format this leaf creates below; the gate has to ask about the
        // format that will actually be created, not a nominal one.
        const MultisampleChoice lMultisample =
            ChooseMultisampleType(luRequestedSamples, D3DFMT_A8R8G8B8);

        if (luRequestedSamples > 1u && lMultisample.muSamples != luRequestedSamples)
        {
            char lacMsaaMessage[192];
            std::snprintf(lacMsaaMessage, sizeof(lacMsaaMessage),
                          "[postfx-rt] target %ux%u msaa=%u unsupported -> %u\n",
                          static_cast<unsigned>(luSurfaceWidth),
                          static_cast<unsigned>(luSurfaceHeight),
                          static_cast<unsigned>(luRequestedSamples),
                          static_cast<unsigned>(lMultisample.muSamples));
            CgsDev::Log::WriteToLog(lacMsaaMessage);
        }

        // --- the colour surface -------------------------------------------------------------
        // eRenderTarget_CREATE is the only mode that owns a surface here; USE_DEVICE_FOR_WRITE
        // means "the device's own back buffer", which on PC is already bound, and USE_PROVIDED /
        // NONE own nothing. The shadow map is NONE (every colour section is cleared by
        // CreateShadowmapBuffer), so this branch is not on the shadow path.
        if (lrParameters.mColourMode == static_cast<u32>(eRenderTarget_CREATE))
        {
            if (lbCubeExtentUsable)
            {
                // ============================================================================
                // THE CUBE COLOUR TARGET -- BOTH objects, and that is the point (seam S1).
                //
                // ⚠ THE RUNG-8 MSAA ARM BELOW IS **NOT** THE PRECEDENT TO COPY HERE, even though
                // this target is the only multisampled one in the pool besides the anti-alias
                // buffer. That arm deliberately leaves maColourTargets[0].mpTexture NULL, because
                // the anti-alias target's pixels reach the frame through a resolve into ANOTHER
                // target's texture. The env map's whole purpose is to BE sampled -- 22 shipped
                // vehicle pixel shaders declare ReflectionTextureSampler as a samplerCUBE at s13 --
                // so a null texture here means sampler 13 binds nothing and every car reflects
                // flat black, with no error and no log line anywhere.
                //
                // So the cube target owns TWO D3D objects:
                //   (1) an IDirect3DCubeTexture9 (RENDERTARGET, DEFAULT pool, single-sampled --
                //       D3D9 has no multisampled texture of any kind). This is mpTexture, the
                //       resolve DESTINATION, and what GetTexture(0) hands the sampler-13 bind.
                //   (2) a face-independent SCRATCH colour render-target surface at the target's
                //       own multisample type. This is what section 0's RenderTargetState binds,
                //       and it is why CgsRenderTarget::SetRenderTargetStateInvertDepth binding
                //       "section 0 regardless of the face argument" (CgsRenderTarget.cpp:336) is
                //       faithful rather than a shortcut: on Xenos every face renders into the SAME
                //       EDRAM tile and the face only matters at RESOLVE time. Same here.
                // Target::Resolve(luFace) then StretchRects (2) into face luFace of (1) -- which on
                // D3D9 IS the MSAA resolve, the same call rung 8 uses for the scene target.
                //
                // The sample count comes from the SHARED ladder (ChooseMultisampleType /
                // MultisampleSampleCountForConsoleFormat / MultisampleOverrideSampleCount), not a
                // second one: CreateEnvmapBuffer asks for console multisample format 2, which that
                // ladder already maps to 4 samples, and gAntiAliasing's 0/1/2/4/8 semantics apply
                // to this target exactly as they do to the scene target.
                //
                // FLAG PC bring-up (device loss): both objects live in D3DPOOL_DEFAULT and would
                // be lost by an IDirect3DDevice9::Reset. This build never Resets (it recreates the
                // device or exits -- same standing note as the post-fx pool targets), so no
                // OnLostDevice/OnResetDevice hook is written here rather than a dead one being
                // faked. DELETE-WHEN a real Reset path lands.
                // ============================================================================
                const u32 luEdge = lrParameters.mu32Width;

                IDirect3DDevice9* const lpDevice = Dev();
                if (lpDevice != nullptr
                    && SUCCEEDED(lpDevice->CreateCubeTexture(luEdge, 1u, D3DUSAGE_RENDERTARGET,
                                                             D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                                                             &lpCubeTexture, nullptr))
                    && lpCubeTexture != nullptr)
                {
                    lpRenderTarget->maColourTargets[0].mpTexture =
                        WrapTexture(lpAllocator, lpCubeTexture,
                                    static_cast<u32>(D3DFMT_A8R8G8B8), luEdge, luEdge,
                                    renderengine::Texture::E_TYPE_CUBE, 6u);
                }
                else
                {
                    CgsDev::Log::WriteToLog(
                        "[envmap-rt] CreateCubeTexture(RENDERTARGET/DEFAULT A8R8G8B8) FAILED --"
                        " sampler 13 will stay unbound\n");
                }

                // The scratch surface. CreateMultisampledColourSurface is a plain CreateRenderTarget
                // and takes D3DMULTISAMPLE_NONE happily, so the single-sampled case (gAntiAliasing
                // == 1, or a device that refuses 4x) needs no second call site.
                lpColourSurface = CreateMultisampledColourSurface(luEdge, luEdge,
                                                                  D3DFMT_A8R8G8B8, lMultisample);
                if (lpColourSurface == nullptr)
                {
                    // Verify F3 (cubeleaf run 2): a cube texture WITHOUT its scratch would pass
                    // the renderer's usability gate (E_TYPE_CUBE wrapper, non-null object), bind
                    // at s13, and then Resolve(face) would early-out silently every frame while
                    // the pass rendered into the null-colour stand-in -- an unwritten DEFAULT-pool
                    // cube in every car's paint, no log. Refuse the whole target instead.
                    CgsDev::Log::WriteToLog(
                        "[envmap-rt] the cube target's SCRATCH colour surface could not be created"
                        " -- REFUSING the cube target (sampler 13 stays unbound)\n");
                    if (lpCubeTexture != nullptr)
                    {
                        lpCubeTexture->Release();
                        lpCubeTexture = nullptr;
                    }
                    lpRenderTarget->maColourTargets[0].mpTexture = nullptr;
                }

                if (lpCubeTexture != nullptr && lpColourSurface != nullptr)
                {
                    RegisterCubeTarget(&lpRenderTarget->maColourTargets[0], lpCubeTexture,
                                       lpColourSurface, luEdge);
                }
            }
            else if (lMultisample.muSamples > 1u)
            {
                // MULTISAMPLED: a render-target SURFACE and NO texture -- D3D9 has no multisampled
                // texture, and this target does not need one (the resolve's destination is another
                // target's texture). maColourTargets[0].mpTexture therefore stays null, GetTexture(0)
                // returns null for this target, and the colour TextureState block below is skipped
                // by its own existing `mpTexture != nullptr` gate. See the MSAA banner for the grep
                // that shows nothing reads this target's texture.
                lpColourSurface = CreateMultisampledColourSurface(luSurfaceWidth, luSurfaceHeight,
                                                                  D3DFMT_A8R8G8B8, lMultisample);
            }
            else
            {
                IDirect3DDevice9* const lpDevice = Dev();
                IDirect3DTexture9*      lpColourTexture = nullptr;
                if (lpDevice != nullptr
                    && SUCCEEDED(lpDevice->CreateTexture(luSurfaceWidth, luSurfaceHeight, 1,
                                                         D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                                         D3DPOOL_DEFAULT, &lpColourTexture, nullptr))
                    && lpColourTexture != nullptr)
                {
                    lpColourTexture->GetSurfaceLevel(0, &lpColourSurface);
                    lpRenderTarget->maColourTargets[0].mpTexture =
                        WrapTexture(lpAllocator, lpColourTexture,
                                    static_cast<u32>(D3DFMT_A8R8G8B8),
                                    luSurfaceWidth, luSurfaceHeight);
                }
            }

            // --- the colour target's SAMPLER TEXTURE-STATE (gate-flip wave, 2026-08-15) ---------
            // Target::CreateColor @0x82403C88 (rwgpfxrendertarget.cpp:133-155) builds ONE
            // TextureState over the colour target's resolved texture and stores it on the Target
            // itself (+0x0C of the record, i.e. +0x2C on the RenderTarget for colour target 0 --
            // `lwz r3, 0x2C(source)` is what the bloom passes read):
            //    address U/V = 2 (clamp), mip = 2, mag = min = the target's filter mode
            //    (Target::Parameters::mFilterMode, +0x20 -- CgsRenderTarget::SetColourFilterMode's
            //    word: 0 point / 1 linear), aniso 13, field10 = 1, the two trailing flag bytes 1,
            //    texture = the target's own.
            // On this backend the sampler words are stored, not applied (SetSamplerStateLowLevel
            // is a stub), so what matters is the raster: the state carries the wrapped colour
            // texture. Same TextureState::Initialize the console uses (texturestate.cpp PC leaf).
            //
            // ⚠ THE SECOND STATE THIS BLOCK USED TO BUILD -- a COLOUR state written into
            // mpColourTextureState (+0x8C) -- IS GONE, and its removal is a correction, not a
            // trim. +0x8C is NOT a colour state on the console: see the RenderTarget::CreateStates
            // banner in the depth section below. It is `mDepthTarget.mpTextureState`, the DEPTH
            // texture state, and it is now built there from the depth texture.
            if (lpRenderTarget->maColourTargets[0].mpTexture != nullptr)
            {
                const u32 luFilterMode = lrParameters.maColourTargetParams[0].mFilterMode;   // +0x20

                renderengine::TextureState::Parameters lTargetStateParams;
                std::memset(&lTargetStateParams, 0, sizeof(lTargetStateParams));
                lTargetStateParams.muAddressU      = 2u;
                lTargetStateParams.muAddressV      = 2u;
                lTargetStateParams.muMipFilter     = 2u;
                lTargetStateParams.muMagFilter     = luFilterMode;
                lTargetStateParams.muMinFilter     = luFilterMode;
                lTargetStateParams.muMaxAnisotropy = 13u;
                lTargetStateParams.muField10       = 1u;
                lTargetStateParams.mu8Field43      = 1u;
                lTargetStateParams.mu8Field44      = 1u;
                lTargetStateParams.mpTexture       = lpRenderTarget->maColourTargets[0].mpTexture;
                lpRenderTarget->maColourTargets[0].mpTextureState =
                    renderengine::TextureState::Initialize(nullptr, &lTargetStateParams);
            }
        }

        // --- the depth / stencil surface ----------------------------------------------------
        if (lrParameters.mDepthStencilMode == static_cast<u32>(eRenderTarget_CREATE))
        {
            if (lbCubeExtentUsable)
            {
                // ============================================================================
                // THE CUBE TARGET'S DEPTH: A PLAIN DEPTH-STENCIL SURFACE, DELIBERATELY NOT A
                // SAMPLEABLE (INTZ) ONE -- and that is a measured decision, not a default.
                //
                // CreateEnvmapBuffer calls SetUseDepthStencilAsTexture(true)
                // (BrnRendererMemory.cpp:864) and the target HAS colour, so the classifier above
                // computes lbWantRawDepthValue = true and, left alone, the RAW ladder would hand
                // this target an INTZ depth texture. NOTHING WOULD READ IT. That exact shape --
                // an INTZ nobody samples -- cost 21 fps of driver stall in post-fx step 11.
                //
                // PROVEN, not assumed, both halves:
                //  (a) NO s13 TECHNIQUE SAMPLES A DEPTH TEXTURE OF THIS TARGET. Every one of the
                //      22 shipped pixel shaders whose CTAB declares ReflectionTextureSampler as a
                //      samplerCUBE at s13 was dumped whole
                //      (scratch/reflections_step1/cubeleaf/work/ctab_s13_sampler_dump.py over
                //      build/game/SHADERS.BNDL): their sampler sets are Diffuse / Normal /
                //      Emissive / AoMap / Crumple / Scratch / BlurDiffuse / BlurNormal /
                //      GlassFracture at s0..s5, s14, the cube at s13, and shadowMapSamplerHighDetail
                //      at s15 -- which is the SHADOW MAP's depth, a different target entirely. No
                //      env-map depth sampler exists in any of the 22.
                //  (b) NOTHING IN THE TREE READS ANY TARGET'S DEPTH TEXTURE BY NAME:
                //        $ grep -rn "GetDepthStencilTexture\|GetDepthTextureState" b5-decomp/src --include=*.cpp
                //        GameShared/GameClasses/Graphics/CgsRenderTarget.cpp:285:    return mpRenderTarget->GetDepthStencilTexture();
                //        pc/gcm/renderengine/PostFxRenderTargetPCLeaf.cpp:1542 (the definition)
                //      -- i.e. one forwarder with no callers of its own, and the definition. The
                //      env-map accessor envface added is GetEnvMapBuffer(); nothing SAMPLES the
                //      depth (envface's EnsureEnvMapTarget reads GetDepthStencilTexture() only to
                //      PRINT the pointer on its one-shot log line).
                //
                // So: a plain CreateDepthStencilSurface at the SCRATCH's exact sample type and
                // quality (D3D9 rejects the bind otherwise), no depth texture, and the depth line
                // below says depthTexture=NONE honestly rather than pointing at an unread INTZ.
                // DELETE-WHEN a technique that samples the env map's depth is found.
                // ============================================================================
                lpDepthSurface = CreateMultisampledDepthSurface(lrParameters.mu32Width,
                                                                lrParameters.mu32Width,
                                                                lMultisample);
                luDepthFormat  = static_cast<u32>(lMultisample.meDepthFormat);
                if (lpDepthSurface == nullptr)
                {
                    CgsDev::Log::WriteToLog(
                        "[envmap-rt] the cube target's depth-stencil surface could not be created"
                        " at the scratch's sample count\n");
                }
            }
            else if (lMultisample.muSamples > 1u)
            {
                // MULTISAMPLED depth: a plain depth-stencil SURFACE at the colour target's exact
                // sample type and quality (D3D9 rejects the bind otherwise). No depth TEXTURE --
                // D3D9 has none at a multisample type -- and this target does not ask for one:
                // CreateAntiAliasBuffer calls SetUseDepthStencilAsTexture(false), because on the
                // console the scene depth is read back through the DOWN-SAMPLE buffer, which is
                // exactly what the RESZ depth resolve in XenonD3D9Shims.cpp now writes.
                lpDepthSurface = CreateMultisampledDepthSurface(luSurfaceWidth, luSurfaceHeight,
                                                                lMultisample);
                luDepthFormat  = static_cast<u32>(lMultisample.meDepthFormat);

                if (lrParameters.mbUseDepthStencilAsTexture)
                {
                    // No target in the pool does this today (the anti-alias buffer is the only
                    // multisampled one and it does not ask). If one ever does, its depth texture is
                    // an honestly EMPTY slot rather than a wrong one, and it says so.
                    static bool sbReportedMsaaDepthTexture = false;
                    if (!sbReportedMsaaDepthTexture)
                    {
                        sbReportedMsaaDepthTexture = true;
                        CgsDev::Log::WriteToLog(
                            "[postfx-rt] a MULTISAMPLED target asked for a sampleable depth --"
                            " D3D9 has no multisampled depth texture, so the slot is left empty;"
                            " read the resolve destination's depth texture instead\n");
                    }
                }
            }
            else
            {
                const DepthSurfaceResult lDepth =
                    CreateDepthSurface(luSurfaceWidth, luSurfaceHeight, lbWantRawDepthValue);
                lpDepthSurface   = lDepth.mpSurface;
                luDepthFormat    = lDepth.muFormat;
                lbDepthHwCompare = lDepth.mbHardwareCompare;
                lbDepthRawValue  = lDepth.mbRawDepthValue;

                // The sampleable wrapper is built only when the target asked to be sampled AND a
                // depth-texture format was actually available.
                if (lrParameters.mbUseDepthStencilAsTexture)
                {
                    lpRenderTarget->mDepthTarget.mpTexture =
                        WrapTexture(lpAllocator, lDepth.mpTexture, lDepth.muFormat,
                                    luSurfaceWidth, luSurfaceHeight);
                }
            }
        }

        // =====================================================================================
        // THE DEPTH TEXTURE-STATE  [RenderTarget::CreateStates @0x824037E8, its final block]
        //
        // WHAT THE CONSOLE ACTUALLY DOES, read off the asm at 0x82403A18-0x82403B44:
        //     lbz  r11, 0x18(r27)      ; params.mbUseDepthStencilAsTexture   (r27 == the params)
        //     cmplwi cr6, r11, 0 ; beq -> stw r31(=0), 0x8C(r29)             ; not sampled: null
        //     lwz  r9,  0x14(r29)      ; this->muDepthStencilMode            (r29 == this)
        //     cmpwi cr6, r9, 4 ; beq  -> stw r31(=0), 0x8C(r29)              ; USE_PROVIDED: null
        //     ...build a TextureState::Parameters block...
        //     lwz  r11, 0x90(r29)      ; mDepthTarget.mpHiZTexture
        //     bne -> (skip) ; lwz r11, 0x88(r29)  ; ...else mDepthTarget.mpTexture
        //     stw  r11, var_138        ; params.mpTexture = that
        //     bl   TextureState::Initialize ; stw r3, 0x8C(r29)
        //
        // ⚠ SO +0x8C IS THE **DEPTH** TEXTURE STATE, NOT A COLOUR ONE. This tree names that word
        // `mpColourTextureState` (rwgpfxrendertarget.h:252) and the parked console TU
        // rwgpfxrendertarget.cpp:410-456 reads it as a colour state; both are wrong, and the
        // mistake is a pure artefact of the X360's 4-byte-pointer layout, where the whole
        // "provided state / provided texture id / colour texture state / provided colour state"
        // quartet at +0x84/+0x88/+0x8C/+0x90 OVERLAPS `mDepthTarget` (+0x80, one 0x18-byte Target
        // record: reserved +0x00, pixel buffer +0x04, texture +0x08, TEXTURE STATE +0x0C, hi-Z
        // +0x10, format byte +0x14). PROVEN, not asserted: RenderTarget::Construct @0x824088B0
        // does `addi r28, r31, 0x80` and hands r28 to Target::CreateDepth, so +0x88 and +0x90 are
        // that record's mpTexture and mpHiZTexture -- the two textures the block above binds --
        // and +0x8C is its mpTextureState, the slot CreateDepth itself leaves null.
        //
        // AND THE CONSUMER AGREES. BrnPostFx::Render @0x8240A5E4 does `lwz r25, 0x8C(r7)` on the
        // DOWN-SAMPLE buffer's RenderTarget and passes r25 as BrnPostFxShader::Render's last
        // TextureState argument, which BrnPostFxShader.cpp:1458 binds at sampler unit 4 --
        // SamplerDepth. A colour state there would make every DoF / motion-blur permutation read
        // the SCENE COLOUR as if it were depth: a silently, plausibly wrong picture, which is
        // exactly the class of defect this wave exists to remove before those permutations light.
        //
        // The parameter words are the asm's, mapped through renderstates.h's
        // TextureState::Parameters (base = var_180 in that frame):
        //   +0x00/+0x04 addressU/V = 6 when (colourMode == NONE && depthMode != NONE) else 2
        //               [`lwz r11,0x10(r29); bne ->else` then `cmpwi r9,0; beq ->else`, i.e. the
        //                DEPTH-ONLY target takes 6 -- note this is the OPPOSITE of the condition
        //                rwgpfxrendertarget.cpp:437 currently spells],
        //   +0x0C/+0x10 mag/min filter = 0 (POINT) -- STORED, not inherited from a zero-fill:
        //               `stw r31, var_174` @0x82403AD8 and `stw r31, var_170` @0x82403AD4 with
        //               r31 == 0 (`li r31, 0` @0x824037F8). Every other word of the block is
        //               written explicitly too, at 0x82403A38-0x82403A80,
        //   +0x14 mip = 2, +0x20 aniso = 13, +0x28 field10 = 1, +0x2C/+0x30 lod bias = 0.0f,
        //   +0x40 field40 = 1 only on the depth-only branch, +0x43/+0x44 = 1,
        //   +0x48 texture = hi-Z if present else the depth texture.
        // POINT is not a PC accommodation here -- it is what the console's own block sets, and it
        // is also the only correct filter for a raw-depth fetch (averaging depth VALUES is
        // meaningless). The PC sampler that actually enforces it is
        // renderengine::PostFxDepthSampler_ApplyState, applied at bind time in
        // XenonD3D9Shims.cpp's D3DDevice_SetTexture (SetSamplerStateLowLevel is a no-op on this
        // backend, so a TextureState's sampler words are stored and never reach D3D).
        //
        // There is no hi-Z texture on PC (no hierarchical-Z region exists), so the `hi-Z else
        // depth` choice always resolves to the depth texture -- stated rather than dropped.
        // The state is built even when no depth TEXTURE could be created (the D24S8 fallback):
        // the console's gate does not test the texture either, and a NULL state pointer here
        // would crash shadow::Device::SetState(const TextureState*, u32), which dereferences
        // ->mpRaster with no test (shadowingdevice.cpp:432). A state with a null raster binds
        // nothing, which is the honest empty slot.
        // =====================================================================================
        if (lrParameters.mbUseDepthStencilAsTexture
            && lrParameters.mDepthStencilMode != static_cast<u32>(eRenderTarget_USE_PROVIDED))
        {
            const u32 luDepthAddressMode =
                ((lrParameters.mColourMode == static_cast<u32>(eRenderTarget_NONE))
                 && (lrParameters.mDepthStencilMode != static_cast<u32>(eRenderTarget_NONE)))
                ? 6u : 2u;

            renderengine::TextureState::Parameters lDepthStateParams;
            std::memset(&lDepthStateParams, 0, sizeof(lDepthStateParams));
            lDepthStateParams.muAddressU      = luDepthAddressMode;
            lDepthStateParams.muAddressV      = luDepthAddressMode;
            lDepthStateParams.muMagFilter     = 0u;      // POINT -- see the banner
            lDepthStateParams.muMinFilter     = 0u;
            lDepthStateParams.muMipFilter     = 2u;
            lDepthStateParams.muMaxAnisotropy = 13u;
            lDepthStateParams.muField10       = 1u;
            lDepthStateParams.mu8Field40      = (luDepthAddressMode == 6u) ? 1u : 0u;
            lDepthStateParams.mu8Field43      = 1u;
            lDepthStateParams.mu8Field44      = 1u;
            lDepthStateParams.mpTexture       = (lpRenderTarget->mDepthTarget.mpHiZTexture != nullptr)
                                              ? lpRenderTarget->mDepthTarget.mpHiZTexture
                                              : lpRenderTarget->mDepthTarget.mpTexture;

            renderengine::TextureState* const lpDepthTextureState =
                renderengine::TextureState::Initialize(nullptr, &lDepthStateParams);

            // ONE storage word, the console's own: the depth Target record's mpTextureState.
            // mpColourTextureState is the SAME X360 word (+0x8C) under this tree's mis-derived
            // name, and BrnPostFx.cpp:813 still reads it by that name, so both spellings are
            // pointed at the one object rather than one of them being left stale -- a
            // split-brain here is what "the composite samples colour as depth" looks like.
            // FOLLOW-UP (cross-group request in REPORT.md): rename the member and move that
            // caller onto GetDepthTextureState(), then this second assignment goes away.
            lpRenderTarget->mDepthTarget.mpTextureState = lpDepthTextureState;
            lpRenderTarget->mpColourTextureState        = lpDepthTextureState;
        }

        // --- the per-section states ---------------------------------------------------------
        // One state object per section. They all carry the same surfaces (PC has one surface for
        // the whole atlas); the console's five slots are kept so CreateBackBuffer's section-4
        // sharing still has somewhere to write.
        for (u32 luSection = 0; luSection < luNumSections && luSection < KU_NUM_SECTION_STATES; ++luSection)
        {
            renderengine::RenderTargetState* const lpState =
                static_cast<renderengine::RenderTargetState*>(
                    CarveZeroed(lpAllocator,
                                static_cast<u32>(sizeof(renderengine::RenderTargetState)), 16u));
            if (lpState == nullptr)
                break;

            lpState->mpColourSurface = lpColourSurface;
            lpState->mpDepthSurface  = lpDepthSurface;
            lpState->muWidth         = luSurfaceWidth;
            lpState->muHeight        = luSurfaceHeight;

            lpRenderTarget->mapSectionState[luSection] = lpState;
        }
        lpRenderTarget->mpSection0State = lpRenderTarget->mapSectionState[0];

        // Latch the shadow-sampler tuple ONLY for the shadow map (see the banner on guCreatedFormat).
        //
        // âš ï¸ "HAS A DEPTH TEXTURE" IS NOT A TIGHT ENOUGH TEST, measured -- an earlier version of this
        // gate used exactly that and the first boot of the post-fx pool wave printed the DOWN-SAMPLE
        // buffer on the [shadow-rt] line: it is depth-CREATE + depth-as-texture too (post-fx samples its
        // depth for depth of field), so it re-latched the tuple with its own 1280x720 values. That was
        // harmless only by luck -- both targets happen to get D24X8 with hardware compare -- and it is
        // the very clobber this gate exists to stop. The particle buffer is a third such target.
        //
        // The DISCRIMINATOR is that the shadow map is the only target in the pool with NO COLOUR AT ALL
        // (CreateShadowmapBuffer clears every colour section, so Construct derives colour mode NONE)
        // while still resolving its depth to a sampleable texture. Down-sample, particle and env map all
        // carry a colour surface. That same test is what chose the ladder above (lbIsShadowMapTarget),
        // computed there from the parameters; the depth TEXTURE having been created is checked here.
        const bool lbShadowDepthTextureCreated =
            lbIsShadowMapTarget && (lpRenderTarget->mDepthTarget.mpTexture != nullptr);
        if (lbShadowDepthTextureCreated)
        {
            guCreatedFormat          = luDepthFormat;
            gbCreatedHardwareCompare = lbDepthHwCompare;
            guCreatedSections        = luNumSections;
        }

        // Every target that asked for a sampleable depth gets a record and a depth line -- the
        // shadow map under its own tag, everything else under [postfx-rt]. Without this the
        // post-fx targets' depth was reported with the SHADOW's format (see the record banner).
        if (lbWantsSampleableDepth)
        {
            RegisterDepthTarget(lpRenderTarget, luSurfaceWidth, luSurfaceHeight, luNumSections,
                                luDepthFormat, lbDepthHwCompare, lbDepthRawValue,
                                lbIsShadowMapTarget);
            ReportDepthRenderTarget(lbIsShadowMapTarget ? "[shadow-rt] target" : "[postfx-rt] depth target",
                                    lpRenderTarget, luSurfaceWidth, luSurfaceHeight, luNumSections,
                                    lpRenderTarget->mDepthTarget.mpTexture != nullptr,
                                    luDepthFormat, lbDepthHwCompare, lbDepthRawValue);
        }

        if (!lbIsShadowMapTarget)
        {
            ReportRenderTarget(luSurfaceWidth, luSurfaceHeight, luNumSections,
                               lrParameters.mColourMode, lrParameters.mDepthStencilMode,
                               lpRenderTarget->maColourTargets[0].mpTexture != nullptr,
                               lMultisample.muSamples);
        }

        // The cube target's own line, from the side that made the objects (see ReportCubeRenderTarget).
        if (lbIsCubeTarget)
        {
            ReportCubeRenderTarget(lrParameters.mu32Width, lMultisample.muSamples,
                                   lpColourSurface, luDepthFormat, lpCubeTexture);
        }

        return lpRenderTarget;
    }

    // =========================================================================
    // The per-section state accessors (X360 reads them as (&mpSection0State)[n]; see the
    // mapSectionState note in the header for why the PC build cannot use that pun).
    // =========================================================================
    renderengine::RenderTargetState* RenderTarget::GetRenderTargetState(u32 luSection)
    {
        if (luSection >= KU_NUM_SECTION_STATES)
            return nullptr;
        return mapSectionState[luSection];
    }

    renderengine::RenderTargetState* RenderTarget::GetSectionRenderTargetState(u32 luSection)
    {
        if (luSection >= KU_NUM_SECTION_STATES)
            return nullptr;
        return mapSectionState[luSection];
    }

    void RenderTarget::SetSectionRenderTargetState(u32 luSection, renderengine::RenderTargetState* lpState)
    {
        if (luSection >= KU_NUM_SECTION_STATES)
            return;
        mapSectionState[luSection] = lpState;
        if (luSection == 0u)
            mpSection0State = lpState;
    }

    // =========================================================================
    // Begin / End -- the console's bracket, over D3D9 surfaces.
    // Begin @0x823F9250: pick section luDestSliceOrFace's state (the device-default global when
    // the colour mode is USE_DEVICE_FOR_WRITE), bind it if it is not already bound, then set the
    // full-extent viewport + scissor. Kept store-for-store in shape.
    //
    // WHAT THE CONSOLE DOES WITH THE INDEX, read off the asm rather than the pseudocode
    // (reflections step 2, answering cubeleaf run-2 F5 / step-1 BRIEF B.3):
    //     0x823F9260  lwz  r11, 0x10(r31)      ; muColourMode
    //     0x823F9264  cmpwi cr6, r11, 2        ; == USE_DEVICE_FOR_WRITE ?
    //     0x823F926C  lis/lwz r30, dword_83271614   ; yes -> the engine-default state
    //     0x823F9278  addi r11, r4, 7          ; no  -> INDEX BY THE ARGUMENT:
    //     0x823F927C  slwi r11, r11, 2         ;        this + 0x1C + 4*luDestSliceOrFace
    //     0x823F9280  lwzx r30, r11, r31       ;        == (&mpSection0State)[face]
    // So the console DOES index the per-section state array by the face/slice, and it applies
    // NO clamp and NO null test before handing the result to Device::SetState.
    //
    // THE ONE PC DEVIATION, and it is a guard, not a behaviour change. A CUBE render target has
    // ONE section (CreateEnvmapBuffer leaves the constructor's count of 1), so Initialize fills
    // mapSectionState[0] and leaves [1..4] null. Reaching Begin(face > 0) on it would call
    // Device::SetState(nullptr) -- a safe no-op on this backend (see Device::SetState below) --
    // and then store that nullptr into renderengine::gpLastRenderTargetState, which is the SHARED
    // X360 shadow dword_83010A30, not a private copy. The device would keep whatever was really
    // bound while the shadow said "nothing", and the next Begin with the true state would compare
    // equal to nothing and re-bind correctly only by luck. That is the split-brain shape
    // ShadowPassPCLeaf.h's banner exists to warn about, so clamp to section 0 instead.
    //
    // IT IS UNREACHABLE TODAY, which is why a guard is the right size of answer. The env-map pass
    // never calls Begin on the cube target: BrnRendererModule::BeginRenderEnvironmentMapFace goes
    // through CgsRenderTarget::SetRenderTargetStateInvertDepth, whose X360 body @0x827E7668 binds
    // section 0 regardless of the face it is handed (CgsRenderTarget.cpp:336-344) -- faithful,
    // because on Xenos every face renders into the SAME EDRAM tile and the face only matters at
    // resolve time. Every Begin call site in the tree passes 0 (grep: git grep -n "Begin(" -- src
    // --include=*.cpp, all eight postfx RenderTarget::Begin sites pass 0/0u; recorded in
    // scratch/reflections_step2/verify_polish_leaf/work/).
    // =========================================================================
    void RenderTarget::Begin(u32 luDestSliceOrFace)
    {
        const renderengine::RenderTargetState* lpState;
        if (muColourMode == static_cast<u32>(eRenderTarget_USE_DEVICE_FOR_WRITE))
        {
            lpState = gpDefaultRenderTargetState;
        }
        else
        {
            lpState = GetRenderTargetState(luDestSliceOrFace);
            // [PC deviation -- see the banner] a one-section target has no state for face > 0; take
            // section 0's rather than publishing a null into the shared shadow below. Inside the
            // else arm ONLY (verify F4, step 2): the device-write arm keeps whatever
            // gpDefaultRenderTargetState is, null included, exactly as the console reads it.
            if (lpState == nullptr)
            {
                // Says so out loud the first time it fires (verify F5): the banner claims this is
                // unreachable today, and a guard justified by "it never fires" must be able to
                // contradict that claim.
                static bool sbLoggedFaceClamp = false;
                if (!sbLoggedFaceClamp)
                {
                    sbLoggedFaceClamp = true;
                    CgsDev::Log::WriteToLog("[postfx-rt] Begin(face>0) on a one-section target --"
                                            " clamped to section 0 (see the banner)\n");
                }
                lpState = GetRenderTargetState(0);
            }
        }

        // The SHARED shadow (X360 dword_83010A30), not a private copy -- see ShadowPassPCLeaf.h.
        if (renderengine::gpLastRenderTargetState != lpState)
        {
            renderengine::Device::SetState(lpState);
            renderengine::gpLastRenderTargetState = lpState;
        }

        IDirect3DDevice9* const lpDevice = Dev();
        if (lpDevice == nullptr)
            return;

        D3DVIEWPORT9 lViewport;
        lViewport.X      = 0;
        lViewport.Y      = 0;
        lViewport.Width  = muWidth;
        lViewport.Height = muHeight;
        lViewport.MinZ   = 0.0f;
        lViewport.MaxZ   = 1.0f;
        lpDevice->SetViewport(&lViewport);

        RECT lScissor;
        lScissor.left   = 0;
        lScissor.top    = 0;
        lScissor.right  = static_cast<LONG>(muWidth);
        lScissor.bottom = static_cast<LONG>(muHeight);
        lpDevice->SetScissorRect(&lScissor);
    }

    // End @0x823FE648 -- resolve on request. See RenderTarget::Resolve.
    void RenderTarget::End(bool lbResolve)
    {
        if (lbResolve)
        {
            Resolve(true, true);
        }
    }

    // =========================================================================
    // Resolve  -- A DOCUMENTED PC NO-OP, not a stub.
    //
    // The console's resolve exists because the surface that was rendered into lives in tiled
    // EDRAM and is NOT addressable by a texture fetch: Xbox2ResolveTo copies (and de-tiles) it
    // into the linear texture the sampler reads. On PC the depth TEXTURE created above is
    // itself the bound depth-stencil surface, so the pixels the pass wrote are already the
    // pixels the sampler will read. There is nothing to copy, and simulating a copy (a
    // StretchRect onto itself, say) would be strictly wrong -- D3D9 cannot even StretchRect a
    // depth surface.
    //
    // This is the only honest translation of Xbox2ResolveTo on this backend. It is called every
    // frame by ShadowMapRenderManager::EndRenderShadowMap.
    // =========================================================================
    void RenderTarget::Resolve(bool /*lbResolveDepthStencil*/, bool /*lbResolveColour*/)
    {
    }

    void Target::Resolve()
    {
    }

    // =========================================================================================
    // Target::Resolve(u32 luFace)  [X360 0x823F9170 -- seam S1, the ONE thing envface's env-map
    // pass calls into this leaf: `GetEnvMapBuffer()->GetRenderTarget()->maColourTargets[0]
    // .Resolve(luFace)`]
    //
    // THE CONSOLE BODY, from the ASM (the decode in rwgpfxrendertarget.h has the full listing;
    // the Hex-Rays rendering prints two of the ten arguments and led an earlier read to conclude
    // the face was only used in the atlas arm). It has two arms, chosen on the SECTION COUNT byte
    // at Target+0x14, and the face index is live in BOTH:
    //   <= 1 section (THE ENV MAP -- CreateEnvmapBuffer never calls SetNumSections, so it keeps
    //        the CgsRenderTarget constructor's 1): Xbox2ResolveTo(mpPixelBuffer, mpTexture,
    //        Flags=0, DestX=0, DestY=0, DestLevel=0, DestSlice=**FACE**, clearColour=null,
    //        clearZ=1.0, clearStencil=0) -- "resolve the tile into FACE luFace, at the origin".
    //   >  1 section (the SHADOW cascade atlas, reached from EndRenderShadowMap @0x823FD708 with
    //        the target's mDepthTarget and the cascade index): D3DDevice_Resolve with
    //        DestSliceOrFace = 0 and destPoint.y = height * index -- a vertical strip.
    //
    // THE PC TRANSLATION IS THE FIRST ARM AND ONLY THE FIRST ARM, and the discriminator here is
    // not the section byte but whether this Target is a REGISTERED CUBE (which is the same
    // question on this backend: the cube target is the only one whose Initialize built a scratch
    // surface + a cube destination). For everything else this is the same documented NO-OP that
    // Resolve() is, and for the same reason: the shadow pass renders straight into its INTZ
    // cascade atlas, so the pixels the pass wrote already ARE the pixels the sampler reads, and
    // D3D9 cannot StretchRect a depth surface anyway. Reading the "format" byte to decide would
    // ALSO have been wrong-looking-right -- see the note on that member in rwgpfxrendertarget.h.
    //
    // StretchRect from a multisampled render-target surface to a non-multisampled texture surface
    // IS the D3D9 MSAA resolve (the rung-8 scene-target precedent), so one call covers both the
    // 4-sample and the single-sample case. D3DTEXF_NONE because source and destination are the
    // same size -- a filter on a 1:1 blit is at best ignored and at worst refused.
    // =========================================================================================
    void Target::Resolve(u32 luFace)
    {
        const CubeTargetRecord* const lpRecord = FindCubeTarget(this);
        if (lpRecord == nullptr)
            return;                       // not a cube target: the documented no-op (see above)

        if (luFace >= 6u)
        {
            static bool sbReportedBadFace = false;
            if (!sbReportedBadFace)
            {
                sbReportedBadFace = true;
                char lacFace[128];
                std::snprintf(lacFace, sizeof(lacFace),
                              "[envmap-rt] Resolve(face=%u) is out of range -- ignored\n",
                              static_cast<unsigned>(luFace));
                CgsDev::Log::WriteToLog(lacFace);
            }
            return;
        }

        IDirect3DDevice9* const lpDevice = Dev();
        if (lpDevice == nullptr)
            return;

        IDirect3DSurface9* lpFaceSurface = nullptr;
        if (FAILED(lpRecord->mpCube->GetCubeMapSurface(static_cast<D3DCUBEMAP_FACES>(luFace), 0u,
                                                       &lpFaceSurface))
            || lpFaceSurface == nullptr)
        {
            return;
        }

        // The resolve proper: the face-independent scratch (the EDRAM tile's stand-in) into
        // face luFace of the cube -- the D3D9 spelling of Xbox2ResolveTo(..., destSlice = face).
        HRESULT lhr = lpDevice->StretchRect(lpRecord->mpScratch, nullptr, lpFaceSurface, nullptr,
                                            D3DTEXF_NONE);
        if (EnvMapDebugFaceColours() && SUCCEEDED(lhr))
        {
            // THE ORIENTATION PROBE, ADDITIVE (verify F2, cubeleaf run 2: a synthetic whole-face
            // fill is identical whatever the producer rendered, so it could show neither the
            // right-handed-projection MIRROR nor a -look hemisphere SWAP). The REAL resolved face
            // is kept and only its TOP-LEFT QUADRANT (texel (0,0) corner) is painted the face's
            // identity colour: one screenshot of a car then shows the reflected world (mirror /
            // swap visible against the actual scene) AND a corner naming the face + its origin.
            RECT lQuadrant;
            lQuadrant.left   = 0;
            lQuadrant.top    = 0;
            lQuadrant.right  = static_cast<LONG>(lpRecord->muEdge / 2u);
            lQuadrant.bottom = static_cast<LONG>(lpRecord->muEdge / 2u);
            if (lQuadrant.right > 0 && lQuadrant.bottom > 0)
                lpDevice->ColorFill(lpFaceSurface, &lQuadrant, KA_ENVMAP_DEBUG_FACE_COLOURS[luFace]);

            static bool sabReportedDebugFace[6] = {};
            if (!sabReportedDebugFace[luFace])
            {
                sabReportedDebugFace[luFace] = true;
                char lacDebug[176];
                std::snprintf(lacDebug, sizeof(lacDebug),
                              "[envmap-rt] BRN_ENVMAP_DEBUG: face %u resolved, top-left quadrant"
                              " painted %s hr=0x%08X\n",
                              static_cast<unsigned>(luFace),
                              KAPC_ENVMAP_DEBUG_FACE_NAMES[luFace],
                              static_cast<unsigned>(lhr));
                CgsDev::Log::WriteToLog(lacDebug);
            }
        }

        if (FAILED(lhr))
        {
            static bool sbReportedResolveFailure = false;
            if (!sbReportedResolveFailure)
            {
                sbReportedResolveFailure = true;
                char lacFail[176];
                std::snprintf(lacFail, sizeof(lacFail),
                              "[envmap-rt] face resolve FAILED hr=0x%08X (face=%u scratch=%p"
                              " edge=%u) -- the cube keeps its previous contents\n",
                              static_cast<unsigned>(lhr), static_cast<unsigned>(luFace),
                              static_cast<void*>(lpRecord->mpScratch),
                              static_cast<unsigned>(lpRecord->muEdge));
                CgsDev::Log::WriteToLog(lacFail);
            }
        }

        lpFaceSurface->Release();          // GetCubeMapSurface AddRefs
    }

    // =========================================================================
    // The resolved textures.
    // =========================================================================
    renderengine::Texture* RenderTarget::GetTexture(u32 luIndex)
    {
        if (luIndex >= 3u)
            return nullptr;
        return maColourTargets[luIndex].mpTexture;
    }

    renderengine::Texture* RenderTarget::GetDepthStencilTexture()
    {
        // Re-report on the way out: this runs once a frame from the s15 bind, so the
        // value-latched line re-fires if the depth texture is ever lost.
        //
        // FROM THIS TARGET'S OWN RECORD, not from the shadow map's globals. It used to pass
        // guCreatedFormat / gbCreatedHardwareCompare / guCreatedSections whoever `this` was --
        // invisible while one ladder gave every target D24X8, a straight falsehood now that the
        // post-fx targets are INTZ/DF24/DF16.
        const DepthTargetRecord* const lpRecord = FindDepthTarget(this);
        if (lpRecord != nullptr)
        {
            ReportDepthRenderTarget(lpRecord->mbShadowMap ? "[shadow-rt] target"
                                                          : "[postfx-rt] depth target",
                                    this, lpRecord->muWidth, lpRecord->muHeight,
                                    lpRecord->muSections, mDepthTarget.mpTexture != nullptr,
                                    lpRecord->muFormat, lpRecord->mbHardwareCompare,
                                    lpRecord->mbRawDepthValue);
        }
        return mDepthTarget.mpTexture;
    }

    // =========================================================================
    // RenderTarget::GetDepthTextureState  -- THE SEAM the post-fx composite and rwgpfxdof
    // bind SamplerDepth through.
    //
    // The console has no such accessor: BrnPostFx::Render reads the word directly
    // (`lwz r25, 0x8C(r7)` @0x8240A5E4). It is added here because on the x64 host that word is
    // TWO members (see the CreateStates banner in Initialize) and a caller that picks the wrong
    // one binds scene colour to SamplerDepth. Callers -- BrnPostFxShader's DoF / motion-blur
    // permutations via BrnPostFx.cpp, and rw::graphics::postfx::DepthOfField -- should reach the
    // depth sampler state by NAME through here.
    //
    // Null for every target that did not ask for a sampleable depth, exactly as the console's
    // +0x8C is (CreateStates stores 0 there on both of its early-out branches).
    // =========================================================================
    renderengine::TextureState* RenderTarget::GetDepthTextureState()
    {
        return mDepthTarget.mpTextureState;
    }
}
}
}
