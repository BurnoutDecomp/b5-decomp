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
//   * the surfaces are D3D9 objects: an INTZ depth TEXTURE (see the format ladder
//     below) whose level-0 surface is bound as the depth-stencil, and -- because
//     D3D9 rejects SetRenderTarget(0, NULL) -- a throwaway colour render target
//     for the depth-only case,
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
#include "rw/rwcore_structs.h"                           // rw::IResourceAllocator / Resource / ResourceDescriptor
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // CgsDev::Log::WriteToLog ([shadow-rt] diagnostic)

#include <cstring>
#include <cstdio>

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

    // The last render-target state installed on the device (X360 dword_83010A30): the bind
    // skips a redundant Device::SetState when the same state is rebound back-to-back. This is
    // the console's own optimisation, kept.
    const renderengine::RenderTargetState* gpLastRenderTargetState = nullptr;

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
    // vendor FOURCC and bound both as the depth-stencil surface and as a sampler resource -- so
    // the console's 0x2D200196 is not translated, it is REPLACED by the D3D9 equivalent.
    //
    // The ladder, best first:
    //   INTZ  - depth as texture, works on every current NVIDIA/AMD/Intel D3D9 part; returns the
    //           raw depth in .r when sampled. This is what the shadow receiver wants.
    //   DF24  - the older ATI depth-texture format (24-bit).
    //   DF16  - the older ATI depth-texture format (16-bit); last resort that is still sampleable.
    // If none of the three can be created, the target falls back to a plain non-sampleable
    // D3DFMT_D24S8 depth-stencil surface: the pass still renders (and is still correct), but
    // GetDepthStencilTexture() returns null and the [shadow-rt] line below says so, so a machine
    // without depth-texture support reports the fact instead of silently drawing black.
    struct DepthSurfaceResult
    {
        IDirect3DTexture9* mpTexture;
        IDirect3DSurface9* mpSurface;
        u32                muFourCC;   // 0 => the non-sampleable D24S8 fallback
    };

    DepthSurfaceResult CreateDepthSurface(u32 luWidth, u32 luHeight)
    {
        DepthSurfaceResult lResult = { nullptr, nullptr, 0u };

        IDirect3DDevice9* const lpDevice = Dev();
        if (lpDevice == nullptr)
            return lResult;

        const u32 lauCandidates[3] =
        {
            static_cast<u32>(MAKEFOURCC('I', 'N', 'T', 'Z')),
            static_cast<u32>(MAKEFOURCC('D', 'F', '2', '4')),
            static_cast<u32>(MAKEFOURCC('D', 'F', '1', '6'))
        };

        for (u32 luCandidate = 0; luCandidate < 3u; ++luCandidate)
        {
            IDirect3DTexture9* lpTexture = nullptr;
            if (SUCCEEDED(lpDevice->CreateTexture(luWidth, luHeight, 1, D3DUSAGE_DEPTHSTENCIL,
                                                  static_cast<D3DFORMAT>(lauCandidates[luCandidate]),
                                                  D3DPOOL_DEFAULT, &lpTexture, nullptr))
                && lpTexture != nullptr)
            {
                IDirect3DSurface9* lpSurface = nullptr;
                if (SUCCEEDED(lpTexture->GetSurfaceLevel(0, &lpSurface)) && lpSurface != nullptr)
                {
                    lResult.mpTexture = lpTexture;
                    lResult.mpSurface = lpSurface;
                    lResult.muFourCC  = lauCandidates[luCandidate];
                    return lResult;
                }
                lpTexture->Release();
            }
        }

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
        for (u32 luLane = 0; luLane < 4u; ++luLane)
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
    renderengine::Texture* WrapTexture(rw::IResourceAllocator* lpAllocator,
                                       IDirect3DTexture9* lpD3DTexture,
                                       u32 luFormat, u32 luWidth, u32 luHeight)
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
        lpTexture->muDepth        = 1;
        lpTexture->muNumMipLevels = 1;
        return lpTexture;
    }

    // ---- the [shadow-rt] diagnostic ---------------------------------------------------------
    // VALUE-latched, not one-shot: a `static bool` fires on the loading screen (before any
    // target exists) and then never again, which is how this build lost the last two render
    // diagnostics. This re-reports whenever the reported tuple CHANGES, so the first line is
    // emitted the moment a real target appears and any later change (a lost device, a null
    // depth texture) shows up too.
    u32 guLastWidth      = 0xFFFFFFFFu;
    u32 guLastHeight     = 0xFFFFFFFFu;
    u32 guLastSections   = 0xFFFFFFFFu;
    u32 guLastDepthValid = 0xFFFFFFFFu;
    u32 guLastFourCC     = 0xFFFFFFFFu;

    void ReportShadowRenderTarget(u32 luWidth, u32 luHeight, u32 luSections,
                                  bool lbDepthTextureValid, u32 luFourCC)
    {
        const u32 luDepthValid = lbDepthTextureValid ? 1u : 0u;
        if (luWidth == guLastWidth && luHeight == guLastHeight && luSections == guLastSections
            && luDepthValid == guLastDepthValid && luFourCC == guLastFourCC)
        {
            return;
        }
        guLastWidth      = luWidth;
        guLastHeight     = luHeight;
        guLastSections   = luSections;
        guLastDepthValid = luDepthValid;
        guLastFourCC     = luFourCC;

        char lacFourCC[8];
        if (luFourCC != 0u)
        {
            lacFourCC[0] = static_cast<char>(luFourCC & 0xFFu);
            lacFourCC[1] = static_cast<char>((luFourCC >> 8) & 0xFFu);
            lacFourCC[2] = static_cast<char>((luFourCC >> 16) & 0xFFu);
            lacFourCC[3] = static_cast<char>((luFourCC >> 24) & 0xFFu);
            lacFourCC[4] = '\0';
        }
        else
        {
            std::snprintf(lacFourCC, sizeof(lacFourCC), "D24S8");
        }

        char lacMessage[192];
        std::snprintf(lacMessage, sizeof(lacMessage),
                      "[shadow-rt] target %ux%u sections=%u depthFormat=%s depthTexture=%s\n",
                      static_cast<unsigned>(luWidth), static_cast<unsigned>(luHeight),
                      static_cast<unsigned>(luSections), lacFourCC,
                      lbDepthTextureValid ? "OK" : "NULL");
        CgsDev::Log::WriteToLog(lacMessage);
    }

    // The FOURCC + section count the live target was created with, kept so
    // GetDepthStencilTexture can re-report the same tuple without re-deriving it (neither is a
    // console member of RenderTarget and neither is worth inventing one for).
    u32 guCreatedFourCC   = 0u;
    u32 guCreatedSections = 1u;
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

        // ORDER MATTERS on D3D9: the bound depth-stencil surface must be at least as large as the
        // render target, and that invariant is checked on SetRenderTarget. Going straight from the
        // (small) back buffer's depth surface to the 1280x1920 shadow colour target would violate
        // it mid-swap, so the depth surface is dropped first and re-bound after.
        lpDevice->SetDepthStencilSurface(nullptr);
        if (lpColour != nullptr)
            lpDevice->SetRenderTarget(0, lpColour);
        lpDevice->SetDepthStencilSurface(lpState->mpDepthSurface);
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
    // It stays NULL on this build: the console installs it from renderengine::Device::Start
    // (X360 dword_83271614), and this PC build's Device::Start has no surface-descriptor
    // object to install. A null default is exactly what the callers' `if (state == nullptr)`
    // guards already expect, and Device::SetState above ignores a null state, so the back
    // buffer simply stays bound -- no invented state object.
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

        IDirect3DSurface9* lpColourSurface = nullptr;
        IDirect3DSurface9* lpDepthSurface  = nullptr;
        u32                luDepthFourCC   = 0u;

        // --- the colour surface -------------------------------------------------------------
        // eRenderTarget_CREATE is the only mode that owns a surface here; USE_DEVICE_FOR_WRITE
        // means "the device's own back buffer", which on PC is already bound, and USE_PROVIDED /
        // NONE own nothing. The shadow map is NONE (every colour section is cleared by
        // CreateShadowmapBuffer), so this branch is not on the shadow path.
        if (lrParameters.mColourMode == static_cast<u32>(eRenderTarget_CREATE))
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
                                static_cast<u32>(D3DFMT_A8R8G8B8), luSurfaceWidth, luSurfaceHeight);
            }
        }

        // --- the depth / stencil surface ----------------------------------------------------
        if (lrParameters.mDepthStencilMode == static_cast<u32>(eRenderTarget_CREATE))
        {
            const DepthSurfaceResult lDepth = CreateDepthSurface(luSurfaceWidth, luSurfaceHeight);
            lpDepthSurface = lDepth.mpSurface;
            luDepthFourCC  = lDepth.muFourCC;

            // The sampleable wrapper is built only when the target asked to be sampled AND a
            // depth-texture format was actually available.
            if (lrParameters.mbUseDepthStencilAsTexture)
            {
                lpRenderTarget->mDepthTarget.mpTexture =
                    WrapTexture(lpAllocator, lDepth.mpTexture, lDepth.muFourCC,
                                luSurfaceWidth, luSurfaceHeight);
            }
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

        guCreatedFourCC   = luDepthFourCC;
        guCreatedSections = luNumSections;
        ReportShadowRenderTarget(luSurfaceWidth, luSurfaceHeight, luNumSections,
                                 lpRenderTarget->mDepthTarget.mpTexture != nullptr, luDepthFourCC);

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
    // =========================================================================
    void RenderTarget::Begin(u32 luDestSliceOrFace)
    {
        const renderengine::RenderTargetState* lpState;
        if (muColourMode == static_cast<u32>(eRenderTarget_USE_DEVICE_FOR_WRITE))
            lpState = gpDefaultRenderTargetState;
        else
            lpState = GetRenderTargetState(luDestSliceOrFace);

        if (gpLastRenderTargetState != lpState)
        {
            renderengine::Device::SetState(lpState);
            gpLastRenderTargetState = lpState;
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
        ReportShadowRenderTarget(muWidth, muHeight * guCreatedSections, guCreatedSections,
                                 mDepthTarget.mpTexture != nullptr, guCreatedFourCC);
        return mDepthTarget.mpTexture;
    }
}
}
}
