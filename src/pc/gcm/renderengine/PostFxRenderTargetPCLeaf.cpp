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
#include "pc/gcm/renderengine/renderstates.h"            // renderengine::TextureState (the colour sampler states)
#include "pc/gcm/renderengine/ShadowPassPCLeaf.h"        // renderengine::ShadowDepthFormat* (homed below)
#include "rw/rwcore_structs.h"                           // rw::IResourceAllocator / Resource / ResourceDescriptor
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // CgsDev::Log::WriteToLog ([shadow-rt] diagnostic)

#include <cstring>
#include <cstdio>
#include <cstddef>

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
    struct DepthSurfaceResult
    {
        IDirect3DTexture9* mpTexture;
        IDirect3DSurface9* mpSurface;
        u32                muFormat;           // 0 => the non-sampleable D24S8 fallback
        bool               mbHardwareCompare;  // true => the fetch compares (Xenos semantic)
    };

    DepthSurfaceResult CreateDepthSurface(u32 luWidth, u32 luHeight)
    {
        DepthSurfaceResult lResult = { nullptr, nullptr, 0u, false };

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

        struct Candidate { u32 muFormat; bool mbHardwareCompare; };
        const Candidate laCandidates[5] =
        {
            { static_cast<u32>(D3DFMT_D24X8),                 true  },
            { static_cast<u32>(D3DFMT_D16),                   true  },
            { static_cast<u32>(MAKEFOURCC('I', 'N', 'T', 'Z')), false },
            { static_cast<u32>(MAKEFOURCC('D', 'F', '2', '4')), false },
            { static_cast<u32>(MAKEFOURCC('D', 'F', '1', '6')), false }
        };

        for (u32 luCandidate = 0; luCandidate < 5u; ++luCandidate)
        {
            const D3DFORMAT leFormat = static_cast<D3DFORMAT>(laCandidates[luCandidate].muFormat);

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
                    lResult.muFormat          = laCandidates[luCandidate].muFormat;
                    lResult.mbHardwareCompare = laCandidates[luCandidate].mbHardwareCompare;
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
    u32 guLastFormat     = 0xFFFFFFFFu;

    // Spell a depth format for the log: a FOURCC prints as its four characters, a real
    // D3DFORMAT enumerant as its name.
    void FormatName(u32 luFormat, char* lpcOut, size_t luSize)
    {
        switch (luFormat)
        {
        case 0u:                             std::snprintf(lpcOut, luSize, "D24S8-surface"); return;
        case static_cast<u32>(D3DFMT_D24X8): std::snprintf(lpcOut, luSize, "D24X8");         return;
        case static_cast<u32>(D3DFMT_D16):   std::snprintf(lpcOut, luSize, "D16");           return;
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

    void ReportShadowRenderTarget(u32 luWidth, u32 luHeight, u32 luSections,
                                  bool lbDepthTextureValid, u32 luFormat, bool lbHardwareCompare)
    {
        const u32 luDepthValid = lbDepthTextureValid ? 1u : 0u;
        if (luWidth == guLastWidth && luHeight == guLastHeight && luSections == guLastSections
            && luDepthValid == guLastDepthValid && luFormat == guLastFormat)
        {
            return;
        }
        guLastWidth      = luWidth;
        guLastHeight     = luHeight;
        guLastSections   = luSections;
        guLastDepthValid = luDepthValid;
        guLastFormat     = luFormat;

        char lacFormat[16];
        FormatName(luFormat, lacFormat, sizeof(lacFormat));

        char lacMessage[224];
        std::snprintf(lacMessage, sizeof(lacMessage),
                      "[shadow-rt] target %ux%u sections=%u depthFormat=%s depthTexture=%s"
                      " compare=%s\n",
                      static_cast<unsigned>(luWidth), static_cast<unsigned>(luHeight),
                      static_cast<unsigned>(luSections), lacFormat,
                      lbDepthTextureValid ? "OK" : "NULL",
                      lbHardwareCompare ? "HW" : "RAW");
        CgsDev::Log::WriteToLog(lacMessage);
    }

    // The sibling reporter for every NON-depth-sampled target -- the post-fx spine's scene colour
    // buffer and the rest of the pool. Kept separate from ReportShadowRenderTarget because that one
    // owns the shadow tuple and its once-only de-duplication state; a colour target flowing through
    // it would both clobber that state and print a shadow line about something that is not a
    // shadow. Unconditional (there are a handful of these per run, created once each).
    void ReportRenderTarget(u32 luWidth, u32 luHeight, u32 luSections,
                            u32 luColourMode, u32 luDepthStencilMode, bool lbColourTextureValid)
    {
        char lacMessage[224];
        std::snprintf(lacMessage, sizeof(lacMessage),
                      "[postfx-rt] target %ux%u sections=%u colourMode=%u depthMode=%u"
                      " colourTexture=%s\n",
                      static_cast<unsigned>(luWidth), static_cast<unsigned>(luHeight),
                      static_cast<unsigned>(luSections), static_cast<unsigned>(luColourMode),
                      static_cast<unsigned>(luDepthStencilMode),
                      lbColourTextureValid ? "OK" : "NULL");
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
    // silently dropped from LINEAR (the hardware PCF the depth-compare formats give us) to POINT,
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

        // ORDER MATTERS on D3D9: the bound depth-stencil surface must be at least as large as the
        // render target, and that invariant is checked on SetRenderTarget. Going straight from the
        // (small) back buffer's depth surface to the 1280x1920 shadow colour target would violate
        // it mid-swap, so the depth surface is dropped first and re-bound after.
        lpDevice->SetDepthStencilSurface(nullptr);
        if (lpColour != nullptr)
            lpDevice->SetRenderTarget(0, lpColour);
        lpDevice->SetDepthStencilSurface(lpState->mpDepthSurface);
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

            // --- the colour target's SAMPLER TEXTURE-STATES (gate-flip wave, 2026-08-15) ---------
            // The console builds TWO TextureStates over a colour target's resolved texture, and
            // this leaf built neither, so both pointers were NULL on PC -- and the post-fx composite
            // dereferences one of them: BrnPostFx::Render @0x8240A468 reads the DOWN-SAMPLE
            // buffer's mpColourTextureState (+0x8C) and BrnPostFxShader::Render binds it whole
            // (shadow::Device::SetState(const TextureState*, u32) @0x8227D158, which reads
            // ->mpRaster with no test -- the console never has a null there). The bloom passes read
            // the per-Target one (`lwz r3, 0x2C(source)` = maColourTargets[0].mpTextureState).
            //
            //  * per-Target (Target::CreateColor @0x82403C88, rwgpfxrendertarget.cpp:133-155):
            //    address U/V = 2 (clamp), mip = 2, mag = min = the target's filter mode
            //    (Target::Parameters::mFilterMode, +0x20 -- CgsRenderTarget::SetColourFilterMode's
            //    word: 0 point / 1 linear), aniso 13, field10 = 1, the two trailing flag bytes 1,
            //    texture = the target's own.
            //  * the RenderTarget-level one (+0x8C, RenderTarget::Initialize, rwgpfxrendertarget.cpp
            //    :418-456): the same words except address = mag = 6 when (colour mode != NONE &&
            //    depth mode == NONE) else 2, mip 2, texture = the provided colour state's, else the
            //    colour target's own resolved texture (the console reads its own +0x8C at that
            //    point, i.e. it binds the resolved colour of THIS target).
            // On this backend the sampler words are stored, not applied (SetSamplerStateLowLevel
            // is a stub), so what matters is the raster: both states carry the wrapped colour
            // texture. Same TextureState::Initialize the console uses (texturestate.cpp PC leaf).
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

                renderengine::TextureState::Parameters lColourStateParams;
                std::memset(&lColourStateParams, 0, sizeof(lColourStateParams));
                const bool lbBlendFlag =
                    (lrParameters.mColourMode != static_cast<u32>(eRenderTarget_NONE)) &&
                    (lrParameters.mDepthStencilMode == static_cast<u32>(eRenderTarget_NONE));
                lColourStateParams.mu8Field40      = lbBlendFlag ? 1u : 0u;
                lColourStateParams.muMagFilter     = lbBlendFlag ? 6u : 2u;
                lColourStateParams.muAddressU      = lColourStateParams.muMagFilter;
                lColourStateParams.muAddressV      = lColourStateParams.muMagFilter;
                lColourStateParams.muMipFilter     = 2u;
                lColourStateParams.muMaxAnisotropy = 13u;
                lColourStateParams.muField10       = 1u;
                lColourStateParams.mu8Field43      = 1u;
                lColourStateParams.mu8Field44      = 1u;
                lColourStateParams.mpTexture       = lpRenderTarget->maColourTargets[0].mpTexture;
                lpRenderTarget->mpColourTextureState =
                    renderengine::TextureState::Initialize(nullptr, &lColourStateParams);
            }
        }

        // --- the depth / stencil surface ----------------------------------------------------
        if (lrParameters.mDepthStencilMode == static_cast<u32>(eRenderTarget_CREATE))
        {
            const DepthSurfaceResult lDepth = CreateDepthSurface(luSurfaceWidth, luSurfaceHeight);
            lpDepthSurface   = lDepth.mpSurface;
            luDepthFormat    = lDepth.muFormat;
            lbDepthHwCompare = lDepth.mbHardwareCompare;

            // The sampleable wrapper is built only when the target asked to be sampled AND a
            // depth-texture format was actually available.
            if (lrParameters.mbUseDepthStencilAsTexture)
            {
                lpRenderTarget->mDepthTarget.mpTexture =
                    WrapTexture(lpAllocator, lDepth.mpTexture, lDepth.muFormat,
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
        // carry a colour surface.
        const bool lbIsShadowMapTarget =
            (lpRenderTarget->mDepthTarget.mpTexture != nullptr)
            && (lrParameters.mColourMode == static_cast<u32>(eRenderTarget_NONE));
        if (lbIsShadowMapTarget)
        {
            guCreatedFormat          = luDepthFormat;
            gbCreatedHardwareCompare = lbDepthHwCompare;
            guCreatedSections        = luNumSections;
            ReportShadowRenderTarget(luSurfaceWidth, luSurfaceHeight, luNumSections,
                                     true, luDepthFormat, gbCreatedHardwareCompare);
        }
        else
        {
            ReportRenderTarget(luSurfaceWidth, luSurfaceHeight, luNumSections,
                               lrParameters.mColourMode, lrParameters.mDepthStencilMode,
                               lpRenderTarget->maColourTargets[0].mpTexture != nullptr);
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
    // =========================================================================
    void RenderTarget::Begin(u32 luDestSliceOrFace)
    {
        const renderengine::RenderTargetState* lpState;
        if (muColourMode == static_cast<u32>(eRenderTarget_USE_DEVICE_FOR_WRITE))
            lpState = gpDefaultRenderTargetState;
        else
            lpState = GetRenderTargetState(luDestSliceOrFace);

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
                                 mDepthTarget.mpTexture != nullptr, guCreatedFormat,
                                 gbCreatedHardwareCompare);
        return mDepthTarget.mpTexture;
    }
}
}
}
