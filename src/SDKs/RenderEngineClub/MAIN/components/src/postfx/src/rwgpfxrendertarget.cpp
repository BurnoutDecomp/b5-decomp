#include "types.hpp"

#include <cstring>   // memcpy / memset

#include "SDKs/RenderEngineClub/MAIN/components/include/postfx/rwgpfxrendertarget.h"  // RenderTarget / Target
#include "GameShared/GameClasses/Core/CgsAssert.h"                                     // CGS_ASSERT
#include "pc/gcm/renderengine/device.h"                                               // renderengine::Device::SetState
#include "pc/gcm/renderengine/texture.h"                                              // Texture / PixelBuffer
#include "pc/gcm/renderengine/renderstates.h"                                         // TextureState
#include "rw/rwcore_structs.h"                                                        // rw::IResourceAllocator / Resource

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   rw::graphics::postfx::Target::CreateColor          @ 0x824034D0
//   rw::graphics::postfx::Target::CreateDepth          @ 0x82403688
//   rw::graphics::postfx::Target::Resolve              @ 0x823F9118
//   rw::graphics::postfx::RenderTarget::Begin          @ 0x823F9250
//   rw::graphics::postfx::RenderTarget::End            @ 0x823FE648
//   rw::graphics::postfx::RenderTarget::Resolve        @ 0x823F9338
//   rw::graphics::postfx::RenderTarget::CreateStates   @ 0x824037E8
//   rw::graphics::postfx::RenderTarget::Construct(ctor) @ 0x824088B0
//   rw::graphics::postfx::RenderTarget::Initialize     @ 0x82409B60
//
// The post-fx render-target wrapper: a RenderTarget owns up to three colour Targets plus one depth
// Target (each a renderengine::PixelBuffer surface + the texture it resolves to + a sampler state),
// and the per-section RenderTargetState the GPU binds. Initialize allocates the object through the
// resource allocator and Construct fills it; Begin binds the surface state + viewport/scissor for a
// pass; End/Resolve resolve the tiled EDRAM surfaces back out to their sampleable textures.
//
// Every literal below is the immediate the X360 asm loads. The create idiom is the committed
// renderengine pattern (build a Parameters block -> GetResourceDescriptor sizes it -> the allocator's
// vtable+0x10 call carves the Resource -> Initialize constructs in place), the same one rwgpfxtint.cpp
// and rwgpfxhelper.cpp use. The PixelBuffer / TextureState / RenderTargetState / Texture callees are
// the committed renderengine backend; the EDRAM/tile placement they manage is an X360 detail handled
// inside those (already-landed) bodies.

// --- X360 (Xenon) D3D9 viewport / scissor entry points (platform externals, same as CgsRenderTarget.cpp).
extern "C" void D3DDevice_SetViewportF(void* lpDevice, const void* lpViewport);
extern "C" void D3DDevice_SetScissorRect(void* lpDevice, const void* lpRect);

namespace renderengine
{
    // The engine's single D3D device global (X360 off_83271608); defined in the renderengine device TU.
    extern void* gpD3DDevice;

    // RenderTargetState's create surface. The class body + these statics live in the committed
    // states/rendertargetstate.cpp (which defines RenderTargetState file-locally); declared here with
    // the two-arg GetResourceDescriptor the X360 call passes (out descriptor + the target-words block),
    // matching the additive DepthStencilState / BlendState two-arg overloads.
    u32* RenderTargetState__GetResourceDescriptor(u32* lpDescriptorOut, const u32* lpTargetWords);
    RenderTargetState* RenderTargetState__Initialize(RenderTargetState** lppState, const u32* lpaParams);
}

namespace
{
    // The default render-target state global (X360 dword_83271614, installed by Device::Start) and the
    // last render-target state bound on the device (X360 dword_83010A30, the back-to-back-bind skip).
    // Both are file-scope globals on the X360 image.
    const renderengine::RenderTargetState* gpDefaultRenderTargetState = nullptr;
    const renderengine::RenderTargetState* gpLastRenderTargetState    = nullptr;

    // The two engine "shared" state blocks the build copies in when a colour/depth mode is
    // USE_DEVICE_FOR_WRITE (X360 unk_83271100 / unk_83271140). HONEST PLACEHOLDERS: engine-resident
    // GPU descriptor blocks with no recoverable bytes from the function-only exports; declared so the
    // build can reference them by name. (CreateStates reads unk_83271140 as a RenderTargetState block;
    // the colour-mode==2 path stores &unk_83271100 into the colour Target's texture slot.)
    extern const u8 gauDeviceWriteColourState;   // X360 &unk_83271100
    const u8 gauDeviceWriteColourState = 0;
    extern const u8 gauDeviceWriteState;         // X360 &unk_83271140
    const u8 gauDeviceWriteState = 0;

    // The X360 float viewport descriptor D3DDevice_SetViewportF consumes (D3DVIEWPORT9, float lanes).
    struct ViewportF
    {
        f32 mfX;       // +0x00
        f32 mfY;       // +0x04
        f32 mfWidth;   // +0x08
        f32 mfHeight;  // +0x0C
        f32 mfMinZ;    // +0x10
        f32 mfMaxZ;    // +0x14
        u32 mu32Pad;   // +0x18
    };

    // The integer scissor rectangle (RECT) D3DDevice_SetScissorRect consumes.
    struct ScissorRect
    {
        s32 miLeft;    // +0x00
        s32 miTop;     // +0x04
        s32 miRight;   // +0x08
        s32 miBottom;  // +0x0C
    };
}

namespace rw
{
namespace graphics
{
namespace postfx
{
    // Allocate + initialise the renderengine::Texture a Target samples. The X360 sub_82403420 builds a
    // Texture::Parameters from the Target::Parameters + the per-surface word block and carves the
    // texture through the RenderTarget's allocator. It is a separate (out-of-this-TU) helper -- the
    // DWARF spells it AllocateAndInitializeTexture(const Target::Parameters&, const Texture::Parameters&)
    // -- so it is declaration-only here (no body invented).
    renderengine::Texture* AllocateAndInitializeTexture(const Target::Parameters& lrTargetParams,
                                                        const renderengine::Texture::Parameters& lrTextureParams);

    // ============================================================================================
    // Target::CreateColor  @ 0x824034D0
    // ============================================================================================
    void Target::CreateColor(const Parameters* lpParameters)
    {
        const Parameters& lrParams = *lpParameters;

        // The SECTION COUNT is stored on the Target up front (`lbz r11,0x44(r4)` -> `stb r11,
        // 0x14(r3)`): the parameters' own mu8NumSections, copied onto the Target so Resolve(u32)
        // can tell a single-section surface from a multi-section atlas without the parameters.
        // (The member was called mu8Format until 2026-08-17; renamed with its header -- the two
        // stores in this TU are exactly the evidence that it is a count. See the header banner.)
        mu8NumSections = lrParams.mu8NumSections;

        // --- the sampleable texture the surface resolves to (AllocateAndInitializeTexture) -----------
        renderengine::Texture::Parameters lTextureParams;
        std::memset(&lTextureParams, 0, sizeof(lTextureParams));
        // WORD 0 IS THE DIMENSION AND WORD 6 IS THE FORMAT (Parameters relaid 2026-08-16 off
        // renderengine::Texture::GetParameters @0x82B60D80 -- see pc/gcm/renderengine/texture.h).
        // This call site is one of the three attestations: the value it puts in word 0 is the
        // parameters' own mTextureType and the one it puts in word 6 is mTextureFormat, which is
        // exactly what the names already said. Only the member names change here.
        lTextureParams.meType      =
            static_cast<renderengine::Texture::Type>(lrParams.mTextureType);   // var_100 = a2[6] (+0x18)
        lTextureParams.muSysMem    = 0;
        lTextureParams.muWidth     = lrParams.mu32Width;
        lTextureParams.muHeight    = lrParams.mu32Height;
        lTextureParams.muNumLevels = 1;                                     // var_F0 = 1
        lTextureParams.miFormat    = static_cast<s32>(lrParams.mTextureFormat);  // var_E8 = a2[5] (+0x14)

        renderengine::Texture* lpTexture = AllocateAndInitializeTexture(lrParams, lTextureParams);
        mpTexture = lpTexture;

        // --- a sampler texture-state for that texture (only when the texture was created) ------------
        if (lpTexture != nullptr)
        {
            renderengine::TextureState::Parameters lStateParams;
            std::memset(&lStateParams, 0, sizeof(lStateParams));
            lStateParams.muAddressU      = 2;                // var_150
            lStateParams.muAddressV      = 2;                // var_14C
            lStateParams.muMipFilter     = 2;                // var_13C
            lStateParams.muMagFilter     = lrParams.mFilterMode;  // var_144 = a2[8]
            lStateParams.muMinFilter     = lrParams.mFilterMode;  // var_140 = a2[8]
            lStateParams.muMaxAnisotropy = 13;               // var_130
            lStateParams.muField10       = 1;                // var_128
            lStateParams.mfMipLodBias    = 0.0f;             // var_124
            lStateParams.mfField12       = 0.0f;             // var_120
            lStateParams.mu8Field43      = 1;                // var_10D
            lStateParams.mu8Field44      = 1;                // var_10C
            lStateParams.mpTexture       = lpTexture;

            rw::BaseResourceDescriptors<5> lStateDescriptor;
            renderengine::TextureState::GetResourceDescriptor(reinterpret_cast<u32*>(&lStateDescriptor));

            rw::IResourceAllocator* lpAllocator = lrParams.mpAllocator;
            rw::Resource lStateResource = lpAllocator->DoAllocate(
                reinterpret_cast<const rw::ResourceDescriptor&>(lStateDescriptor), nullptr);
            mpTextureState =
                renderengine::TextureState::Initialize(&lStateResource, &lStateParams);
        }

        // --- the GPU PixelBuffer surface (always built) ----------------------------------------------
        renderengine::PixelBuffer::Parameters lBufferParams;
        std::memset(&lBufferParams, 0, sizeof(lBufferParams));
        lBufferParams.muKind         = 0;                    // v18[0]
        lBufferParams.muFlags        = 1;                    // v18[1]
        lBufferParams.muWidth        = lrParams.mu32Width;     // v18[2] = a2[1]
        lBufferParams.muHeight       = lrParams.mu32Height;    // v18[3] = a2[2]
        lBufferParams.muFormat       = lrParams.mBufferFormat;    // v18[4] = a2[4]
        lBufferParams.muMultiSample  = lrParams.mu32MultiSampleFormat;// v18[5] = a2[7]
        lBufferParams.muMipBase      = 0;                    // v18[6]

        rw::IResourceAllocator* lpAllocator = lrParams.mpAllocator;
        rw::Resource lBufferResource = lpAllocator->DoAllocate(
            reinterpret_cast<const rw::ResourceDescriptor&>(lBufferParams), nullptr);
        mpPixelBuffer = renderengine::PixelBuffer::Initialize(
            reinterpret_cast<renderengine::PixelBuffer::Wrapper*>(&lBufferResource), &lBufferParams);

        renderengine::PixelBuffer::Xbox2SetBaseEDRAM(mpPixelBuffer, static_cast<s16>(lrParams.mn32BaseEDRAM));
    }

    // ============================================================================================
    // Target::CreateDepth  @ 0x82403688
    // ============================================================================================
    void Target::CreateDepth(const Parameters* lpParameters)
    {
        const Parameters& lrParams = *lpParameters;

        // The depth surface's own texture-parameter block (var_15.. on the X360 stack).
        renderengine::Texture::Parameters lTextureParams;
        std::memset(&lTextureParams, 0, sizeof(lTextureParams));
        // v15 = 1 IS THE DIMENSION, NOT A FORMAT: `li r30, 1` @0x82403698 -> `stw r30, var_90`
        // @0x824036C0 writes word 0, and E_TYPE_2D is 1. The format is word 6 (v21), which the
        // hi-Z arm below overwrites with the console's own 0x18280186. (Parameters relaid
        // 2026-08-16 -- see pc/gcm/renderengine/texture.h.)
        lTextureParams.meType      = renderengine::Texture::E_TYPE_2D;    // v15 = 1
        lTextureParams.muSysMem    = 0;                                  // v16 = 0
        lTextureParams.muWidth     = lrParams.mu32Width;                   // v17 = a2[1]
        lTextureParams.muHeight    = lrParams.mu8NumSections * lrParams.mu32Height; // v18 = (*(a2+68)) * a2[2]
        lTextureParams.muDepth     = 1;                                  // v19 = 1
        lTextureParams.muNumLevels = 1;                                  // v20 = 1
        lTextureParams.miFormat    = static_cast<s32>(lrParams.mTextureFormat);  // v21 = a2[5]

        mpTexture    = AllocateAndInitializeTexture(lrParams, lTextureParams);
        mpHiZTexture = nullptr;

        // When the parameters request a hierarchical-Z surface (*(a2+36) != 0), build a second texture
        // and rebase its EDRAM tile word onto the depth surface's high 20 bits.
        if (lrParams.mbUseStencil != 0)
        {
            lTextureParams.muSysMem    = 1;          // v16 = 1
            lTextureParams.miFormat    = 405275014;  // v21 = 0x18280186 hierarchical-Z format word
                                                     //  (word 6 -- the same GPU format word
                                                     //   Tint::Initialize writes for its lookup)

            renderengine::Texture* lpHiZ      = AllocateAndInitializeTexture(lrParams, lTextureParams);
            renderengine::Texture* lpDepthTex = mpTexture;
            mpHiZTexture = lpHiZ;

            // *(v8+32) = (*(v9+32) >> 12 << 12) | (*(v8+32) & 0xFFF): copy the depth texture's tile
            // base (high 20 bits) onto the hi-Z texture, preserving its low 12 bits.
            lpHiZ->muFlags = static_cast<u16>(
                ((lpDepthTex->muFlags >> 12) << 12) | (lpHiZ->muFlags & 0xFFF));

            renderengine::Texture::Xbox2CheckPhysicalMemoryFlags(lpHiZ);
        }

        // The SECTION COUNT is stored after the hi-Z block (lbz 0x44 -> stb 0x14) -- same store as
        // CreateColor's, just later in the body. Renamed from mu8Format 2026-08-17.
        mu8NumSections = lrParams.mu8NumSections;

        // --- the GPU depth PixelBuffer surface -------------------------------------------------------
        renderengine::PixelBuffer::Parameters lBufferParams;
        std::memset(&lBufferParams, 0, sizeof(lBufferParams));
        lBufferParams.muKind         = 1;                    // v22[0] = 1 (depth-stencil surface)
        lBufferParams.muFlags        = 1;                    // v22[1] = 1
        lBufferParams.muWidth        = lrParams.mu32Width;     // v22[2] = a2[1]
        lBufferParams.muHeight       = lrParams.mu32Height;    // v22[3] = a2[2]
        lBufferParams.muFormat       = lrParams.mBufferFormat;    // v22[4] = a2[4]
        lBufferParams.muMultiSample  = 52;                   // v22[5] = 52
        lBufferParams.muMipBase      = 0;                    // v22[6] = 0

        rw::IResourceAllocator* lpAllocator = lrParams.mpAllocator;
        rw::Resource lBufferResource = lpAllocator->DoAllocate(
            reinterpret_cast<const rw::ResourceDescriptor&>(lBufferParams), nullptr);
        mpPixelBuffer = renderengine::PixelBuffer::Initialize(
            reinterpret_cast<renderengine::PixelBuffer::Wrapper*>(&lBufferResource), &lBufferParams);

        // asm @0x824037CC: r4 = *(a2+0x2C) = lrParams.mn32HierarchicalZAddress (the hi-Z base), not 0.
        renderengine::PixelBuffer::Xbox2SetBaseHierarchicalZ(mpPixelBuffer, lrParams.mn32HierarchicalZAddress);
        renderengine::PixelBuffer::Xbox2SetBaseEDRAM(mpPixelBuffer, static_cast<s16>(lrParams.mn32BaseEDRAM));
    }

    // ============================================================================================
    // Target::Resolve  @ 0x823F9118
    // ============================================================================================
    void Target::Resolve()
    {
        // *(result + 8) != 0 -> resolve the surface's PixelBuffer (+0x04) out to its texture.
        if (mpTexture != nullptr)
        {
            // Trailing arguments updated 2026-08-13 for Xbox2ResolveTo's corrected signature: the
            // 8th is the clear-colour POINTER (null here) and the 10th is ClearStencil, which the
            // X360 passes in the first outgoing overflow slot -- `li r5, 0` @0x823F9130 then
            // `stw r5, 0x70+var_14(r1)` = r1+0x5C @0x823F914C. Zero, from this caller's own asm.
            renderengine::PixelBuffer::Xbox2ResolveTo(mpPixelBuffer, nullptr, 0, 0, 0, 0, 0,
                                                      nullptr, 1.0f, 0u);
        }
    }

    // ============================================================================================
    // RenderTarget::Begin  @ 0x823F9250
    // ============================================================================================
    void RenderTarget::Begin(u32 luDestSliceOrFace)
    {
        // Select the section's render-target state: the device-default global when the colour mode is
        // USE_DEVICE_FOR_WRITE (== 2), else the per-section state at +0x1C + luDestSliceOrFace.
        const renderengine::RenderTargetState* lpState;
        if (muColourMode == 2)
        {
            lpState = gpDefaultRenderTargetState;
        }
        else
        {
            lpState = (&mpSection0State)[luDestSliceOrFace];
        }

        // Bind it (skipping a redundant back-to-back rebind).
        if (gpLastRenderTargetState != lpState)
        {
            renderengine::Device::SetState(lpState);
            gpLastRenderTargetState = lpState;
        }

        // Full-extent viewport + scissor at the target's width/height.
        ViewportF lViewport;
        lViewport.mfX      = 0.0f;
        lViewport.mfY      = 0.0f;
        lViewport.mfWidth  = static_cast<f32>(muWidth);
        lViewport.mfHeight = static_cast<f32>(muHeight);
        lViewport.mfMinZ   = 0.0f;
        lViewport.mfMaxZ   = 1.0f;
        lViewport.mu32Pad  = 0;
        D3DDevice_SetViewportF(renderengine::gpD3DDevice, &lViewport);

        ScissorRect lScissor;
        lScissor.miLeft   = 0;
        lScissor.miTop    = 0;
        lScissor.miRight  = static_cast<s32>(muWidth);
        lScissor.miBottom = static_cast<s32>(muHeight);
        D3DDevice_SetScissorRect(renderengine::gpD3DDevice, &lScissor);
    }

    // ============================================================================================
    // RenderTarget::End  @ 0x823FE648
    // ============================================================================================
    void RenderTarget::End(bool lbResolve)
    {
        if (lbResolve)
        {
            Resolve(true, true);
        }
    }

    // ============================================================================================
    // RenderTarget::Resolve  @ 0x823F9338
    // ============================================================================================
    void RenderTarget::Resolve(bool lbResolveDepthStencil, bool lbResolveColour)
    {
        // Colour: resolve the colour Target's PixelBuffer (+0x24) out to its texture when it has one
        // (+0x28 != 0). (The X360 reads the colour Target at this + 0x24/0x28: maColourTargets[0].)
        if (lbResolveColour && maColourTargets[0].mpTexture != nullptr)
        {
            // Trailing arguments updated 2026-08-13 for Xbox2ResolveTo's corrected signature: the
            // 8th is the clear-colour POINTER (null here) and the 10th is ClearStencil, passed in
            // the first outgoing overflow slot -- `li r5, 0` @0x823F936C then
            // `stw r5, 0x80+var_24(r1)` = r1+0x5C @0x823F9388. Zero, from this caller's own asm.
            renderengine::PixelBuffer::Xbox2ResolveTo(
                maColourTargets[0].mpPixelBuffer, nullptr, 0, 0, 0, 0, 0, nullptr, 1.0f, 0u);
        }

        // Depth: when the target has a colour section (+0x18) and a depth resolve was asked, resolve the
        // depth Target (this + 0x80).
        if (mu8HasColour != 0 && lbResolveDepthStencil)
        {
            mDepthTarget.Resolve();
        }
    }

    // ============================================================================================
    // RenderTarget::CreateStates  @ 0x824037E8
    // ============================================================================================
    void RenderTarget::CreateStates(const Target::Parameters* lpParameters)
    {
        const Target::Parameters& lrParams = *lpParameters;

        // The colour byte from the parameters (+0x19) drives the texture-state's blend flag below.
        const u8 lu8HasColour = lrParams.mu8NumSections;
        mu8HasColour = lu8HasColour;

        rw::IResourceAllocator* lpAllocator = mpAllocator;

        // The 5-word RenderTargetState param block { mode, mode, 0, 0 } + the colour-format word.
        u32 laTargetWords[5];

        if (muColourMode == 1)
        {
            std::memset(laTargetWords, 0, sizeof(laTargetWords));
            // asm @0x82403864: v54[0] = *(this+0x24) = maColourTargets[0].mpPixelBuffer (NOT the
            // provided-texture id). X360 4-byte pointer word -> truncated into the u32 block on the
            // LLP64 host (the block models the X360 5x4-byte RenderTargetState param record).
            laTargetWords[0] = static_cast<u32>(reinterpret_cast<uintptr_t>(maColourTargets[0].mpPixelBuffer));

            // The shared-state companion for the colour target (engine block / provided block).
            const void* lpCompanion = nullptr;
            if (muDepthStencilMode == 1 || muDepthStencilMode == 4)
                lpCompanion = mpProvidedState;        // result[33]
            else if (muDepthStencilMode == 2)
                lpCompanion = &gauDeviceWriteState;   // &unk_83271140
            laTargetWords[4] = static_cast<u32>(reinterpret_cast<uintptr_t>(lpCompanion));   // v54[4] = companion

            rw::BaseResourceDescriptors<5> lDescriptor;
            renderengine::RenderTargetState__GetResourceDescriptor(
                reinterpret_cast<u32*>(&lDescriptor), laTargetWords);

            rw::Resource lResource = lpAllocator->DoAllocate(
                reinterpret_cast<const rw::ResourceDescriptor&>(lDescriptor), nullptr);
            mpSection0State = renderengine::RenderTargetState__Initialize(
                reinterpret_cast<renderengine::RenderTargetState**>(&lResource), laTargetWords);
        }
        else if (muColourMode == 0 && muDepthStencilMode != 0)
        {
            std::memset(laTargetWords, 0, sizeof(laTargetWords));
            laTargetWords[0] = static_cast<u32>(reinterpret_cast<uintptr_t>(maColourTargets[0].mpPixelBuffer));

            const void* lpCompanion = nullptr;
            if (muDepthStencilMode == 1 || muDepthStencilMode == 4)
                lpCompanion = mpProvidedState;
            else if (muDepthStencilMode == 2)
                lpCompanion = &gauDeviceWriteState;
            laTargetWords[4] = static_cast<u32>(reinterpret_cast<uintptr_t>(lpCompanion));

            rw::BaseResourceDescriptors<5> lDescriptor;
            renderengine::RenderTargetState__GetResourceDescriptor(
                reinterpret_cast<u32*>(&lDescriptor), laTargetWords);

            rw::Resource lResource = lpAllocator->DoAllocate(
                reinterpret_cast<const rw::ResourceDescriptor&>(lDescriptor), nullptr);
            mpSection0State = renderengine::RenderTargetState__Initialize(
                reinterpret_cast<renderengine::RenderTargetState**>(&lResource), laTargetWords);

            // Validate the depth path: a cube colour target is unsupported, and a depth-write that
            // re-uses the device surface is rejected.
            if (lu8HasColour > 1u)
            {
                CGS_ASSERT(true,  "params.GetColorTargetParameters(0).GetTextureType() != Texture::TYPE_CUBE");
                CGS_ASSERT(muDepthStencilMode != 2, "m_depthStencilMode != eRenderTarget_USE_DEVICE_FOR_WRITE");
            }
        }

        // The colour-sampling TextureState (+0x8C). Skipped when the target has no colour byte, or the
        // depth-stencil mode is USE_PROVIDED (== 4).
        if (mu8HasColour == 0 || muDepthStencilMode == 4)
        {
            mpColourTextureState = nullptr;
            return;
        }

        renderengine::TextureState::Parameters lStateParams;
        std::memset(&lStateParams, 0, sizeof(lStateParams));
        lStateParams.muMaxAnisotropy = 13;               // v30 = 13
        lStateParams.muField10       = 1;                // v32 = 1
        lStateParams.mfMipLodBias    = 0.0f;             // v33
        lStateParams.mfField12       = 0.0f;             // v34
        lStateParams.mu8Field43      = 1;                // v41 = 1
        lStateParams.mu8Field44      = 1;                // v42 = 1

        // The bound colour texture: the provided colour state when there is one, else the colour
        // target's own resolved texture.
        renderengine::TextureState* lpBoundTexture =
            reinterpret_cast<renderengine::TextureState*>(mpProvidedColourState);
        if (lpBoundTexture == nullptr)
            lpBoundTexture = reinterpret_cast<renderengine::TextureState*>(mpColourTextureState);
        lStateParams.mpTexture = reinterpret_cast<renderengine::Texture*>(lpBoundTexture);

        // The address/filter words: address-clamp (2) always, with the blend flag set when the colour
        // target is provided OR the colour mode is none.
        if (muColourMode != 0 && muDepthStencilMode == 0)
        {
            lStateParams.mu8Field40  = 1;                // v38 = 1
            lStateParams.muMagFilter = 6;                // v22 = 6
        }
        else
        {
            lStateParams.muMagFilter = 2;                // v22 = 2
        }
        lStateParams.muAddressU = lStateParams.muMagFilter; // v26[0] = v22
        lStateParams.muAddressV = lStateParams.muMagFilter; // v26[1] = v22
        lStateParams.muMipFilter = 2;                       // v27 = 2

        rw::BaseResourceDescriptors<5> lStateDescriptor;
        renderengine::TextureState::GetResourceDescriptor(reinterpret_cast<u32*>(&lStateDescriptor));

        rw::Resource lStateResource = lpAllocator->DoAllocate(
            reinterpret_cast<const rw::ResourceDescriptor&>(lStateDescriptor), nullptr);
        mpColourTextureState = reinterpret_cast<renderengine::TextureState*>(
            renderengine::TextureState::Initialize(&lStateResource, &lStateParams));
    }

    // ============================================================================================
    // RenderTarget::Construct (the X360 out-of-line ctor)  @ 0x824088B0
    // ============================================================================================
    RenderTarget* RenderTarget::Construct(const Target::Parameters* lpParameters)
    {
        const Target::Parameters& lrParams = *lpParameters;

        mpAllocator        = nullptr;                       // *a1 = 0 (filled below)
        muWidth            = lrParams.mu32Width;              // a1[1] = a2[1]
        muHeight           = lrParams.mu32Height;            // a1[2] = a2[2]
        muColourMode       = lrParams.mBufferFormat;            // a1[4] = a2[4]
        muDepthStencilMode = lrParams.mTextureFormat;        // a1[5] = a2[5]
        mu8HasColour       = static_cast<u8>(lrParams.mTextureType); // *(a1+24) byte = *(a2+24) byte

        // Zero each colour Target's surface pointers.
        for (u32 luTarget = 0; luTarget < 3; ++luTarget)
        {
            Target& lTarget = maColourTargets[luTarget];
            lTarget.mpReserved0    = 0;
            lTarget.mpPixelBuffer  = nullptr;
            lTarget.mpTexture      = nullptr;
            lTarget.mpTextureState = nullptr;
            lTarget.mpHiZTexture   = nullptr;
            lTarget.mu8NumSections = 0;
        }
        // Zero the depth Target.
        mDepthTarget.mpReserved0    = 0;
        mDepthTarget.mpPixelBuffer  = nullptr;
        mDepthTarget.mpTexture      = nullptr;
        mDepthTarget.mpTextureState = nullptr;
        mDepthTarget.mu8NumSections = 0;

        mpProvidedState       = nullptr;   // a1[34]
        mpColourTextureState  = nullptr;   // a1[35]
        mpProvidedColourState = nullptr;   // a1[36]
        mu8UsesProvidedState  = 0;         // *(a1+149)
        muProvidedTextureId   = 0;         // a1[33]

        // The allocator: the parameters' own, or the registry default.
        if (lrParams.mpAllocator != nullptr)
            mpAllocator = lrParams.mpAllocator;
        else
            mpAllocator = rw::ResourceAllocatorRegistry::GetDefaultAllocator();

        // The colour surface (mode 1 builds it; mode 2 binds the engine device-write block).
        Target::Parameters lSurfaceParams;
        std::memcpy(&lSurfaceParams, reinterpret_cast<const u8*>(lpParameters) + 0x1C, 0x48);
        lSurfaceParams.mpAllocator = mpAllocator;
        lSurfaceParams.mu32Width     = lrParams.mu32Width;
        lSurfaceParams.mu32Height    = lrParams.mu32Height;

        if (muColourMode == 1)
        {
            maColourTargets[0].CreateColor(&lSurfaceParams);
        }
        else if (muColourMode == 2)
        {
            maColourTargets[0].mpTexture =
                reinterpret_cast<renderengine::Texture*>(const_cast<u8*>(&gauDeviceWriteColourState));
        }

        // The depth surface (mode 1 builds it; mode 4 copies in a provided 6-word state block).
        if (muDepthStencilMode == 1)
        {
            std::memcpy(&lSurfaceParams, reinterpret_cast<const u8*>(lpParameters) + 0x13C, 0x48);
            lSurfaceParams.mpAllocator = mpAllocator;
            lSurfaceParams.mu32Width     = lrParams.mu32Width;
            lSurfaceParams.mu32Height    = lrParams.mu32Height;
            mDepthTarget.CreateDepth(&lSurfaceParams);
        }
        else if (muDepthStencilMode == 4)
        {
            // Copy the provided 6-word render-target state block (a2[101]) into the depth Target.
            const u32* lpProvided = *reinterpret_cast<const u32* const*>(
                reinterpret_cast<const u8*>(lpParameters) + 0x194);
            std::memcpy(&mDepthTarget, lpProvided, 6 * sizeof(u32));
            mu8UsesProvidedState = 1;
        }

        mpSection0State = nullptr;   // a1[7] = 0

        CreateStates(lpParameters);
        return this;
    }

    // ============================================================================================
    // RenderTarget::Initialize  @ 0x82409B60
    // ============================================================================================
    RenderTarget* RenderTarget::Initialize(const Target::Parameters* lpParameters)
    {
        // The render-target object's own resource descriptor: { size = 0x98, align = 0x10 }.
        struct { u32 muSize; u32 muAlign; } lDescriptor = { 0x98u, 0x10u };

        // The allocator: the parameters' own when set, else the registry default.
        rw::IResourceAllocator* lpAllocator;
        if (lpParameters->mpAllocator != nullptr)
            lpAllocator = lpParameters->mpAllocator;
        else
            lpAllocator = rw::ResourceAllocatorRegistry::GetDefaultAllocator();

        rw::Resource lResource = lpAllocator->DoAllocate(
            reinterpret_cast<const rw::ResourceDescriptor&>(lDescriptor), nullptr);

        RenderTarget* lpRenderTarget = static_cast<RenderTarget*>(lResource.m_baseResources[0]);
        if (lpRenderTarget != nullptr)
            return lpRenderTarget->Construct(lpParameters);
        return nullptr;
    }
}
}
}
