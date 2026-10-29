#include "SDKs/RenderEngineClub/MAIN/components/src/vertexbuffer.h"
#include <xtl.h>
#include "SDKs/RenderEngineClub/MAIN/components/src/texture.h"
// ============================================================================
// GameSource/Effects/Particles/Native/BrnSimpleParticleRenderer.cpp
//
// BrnParticle::Native::BrnSimpleParticleRenderer::Construct  @0x82284518
// BrnParticle::Native::BrnSimpleParticleRenderer::Dispatch   @0x8228CA18
//
// ⭐ 2026-09-24 (FX-CRASHVFX): REWRITTEN to the DWARF shape (see the header). The previous body of
// Construct had the right three-template skeleton but two defects that the asm settles:
//   * its first slot was an `int` stored from the THIRD argument and returned a pointer through
//     `reinterpret_cast<int>` -- r5 is the Im3dSmokeRenderer* (DWARF :418 mpRenderer) and the
//     function returns nothing;
//   * it set the byte at params +0x30 (mbHasCustomBlendFactors) where the console stores to +0x36
//     (`stb r29, var_14A`, var_14A == var_180 + 0x36 == mbState16 -- AlphaTestEnable), and it
//     sized ONE ResourceDescriptorEntry where GetResourceDescriptor writes five.
// ============================================================================

#include "GameSource/Effects/Particles/Native/BrnSimpleParticleRenderer.h"
#include "GameSource/Effects/Particles/Native/BrnSimpleParticleArray.h"   // GetTexture, the diag counters
#include "GameSource/Effects/Particles/Native/BrnNativeParticleVertex.h"  // canonical packed GPU stride
#include "GameSource/Effects/Particles/Native/BrnIm3dSmokeRenderer.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"    // ImRendererBase::mgpActiveRenderer
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasicColouredTexturedVertex.h"  // the 24-byte stride
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"      // shadow::Device
#include <cstdio>    // [diag] snprintf
#include <cstdlib>   // malloc

// The device surface Dispatch binds through -- the same minimal extern surface SparkRenderer::Dispatch
// (BrnSparkRenderer_Render.cpp) and the Lion dispatch declare.
struct IDirect3DDevice9;
extern IDirect3DDevice9* gpD3DDevice;


// The shared-library rasteriser and depth-stencil states Dispatch binds (ImmediateMode.cpp).
extern void* gpSkyDomeRasterizerState;      // X360 dword_83010F3C  mpRasterizerState_CullNone
extern void* gpSkyDomeDepthStencilState;    // X360 dword_83010F4C  mpDepthStencilState_ZBufferOnWriteOff
void ImDeviceSetRasterizerState(void* lpState);
void ImDeviceSetDepthStencilState(void* lpState);

// X360 dword_83010F20 -- the immediate-mode library's STANDARD blend template (LionParticleRender.cpp
// owns the PC word; it reads null on this build, see Construct).
extern renderengine::BlendMaterialState* dword_83010F20;

namespace BrnParticle
{
namespace Native
{
    // [DIAG] NOT IN THE X360 BINARY -- see BrnSimpleParticleArray.h. DELETE-WHEN-STABLE.
    u32 gauSimpleParticleDrawnBatches  = 0;
    u32 gauSimpleParticleDrawnVertices = 0;

    namespace
    {
        // The Xenos primitive Dispatch passes to D3DDevice_DrawVertices (`li r4, 0xD` at 0x8228CD34):
        // QUADLIST -- four vertices per particle, which is what Render writes. The PC shim expands it
        // (XenonD3D9Shims.cpp WorldDraw_NonIndexedUP, the Lion pass's same primitive).
        const u32 KU_XENOS_QUADLIST = 13u;

        // Dispatch's soft-particle fade distance (flt_82004740 = 0.3, loaded into f3 at 0x8228CBA8).
        const f32 KF_ZFADE_DISTANCE = 0.30000001192092896f;

        // The BlendStateParameters block the console default-constructs on the stack before each of
        // its three GetParameters reads (0x82284524..0x822845A8, repeated twice): four 0x07060706
        // factor words, AlphaFunc 7 (ALWAYS), four colour-write masks of 15, AlphaToMaskOffsets 0x87,
        // AlphaRef 0, BlendFactor -1, every bool clear.
        void DefaultConstructBlendParameters(renderengine::BlendStateParameters& lrParameters)
        {
            const u32 KU_TEMPLATE_BLEND_WORD = 0x07060706u;   // `lis r11, 0x706 ; ori r22, r11, 0x706`
            lrParameters.maBlendFactor[0] = KU_TEMPLATE_BLEND_WORD;
            lrParameters.maBlendFactor[1] = KU_TEMPLATE_BLEND_WORD;
            lrParameters.maBlendFactor[2] = KU_TEMPLATE_BLEND_WORD;
            lrParameters.maBlendFactor[3] = KU_TEMPLATE_BLEND_WORD;
            lrParameters.muState15 = 7u;             // r28 -- AlphaFunc ALWAYS
            lrParameters.muState4  = 15u;            // r30 -- ColorWriteEnable x4
            lrParameters.muState5  = 15u;
            lrParameters.muState6  = 15u;
            lrParameters.muState7  = 15u;
            lrParameters.muState8  = 0x87u;          // r23 -- AlphaToMaskOffsets
            lrParameters.muState17 = 0u;             // r31 -- AlphaRef
            lrParameters.muState9  = 0xFFFFFFFFu;    // r27 -- BlendFactor
            lrParameters.mbHasCustomBlendFactors = 0u;
            lrParameters.mbState10 = 0u;
            lrParameters.mbState11 = 0u;
            lrParameters.mbState12 = 0u;
            lrParameters.mbState13 = 0u;
            lrParameters.mbState14 = 0u;
            lrParameters.mbState16 = 0u;
        }

        void ReadTemplateParameters(const renderengine::BlendMaterialState* lpTemplate,
                                    u32 luSourceFactor, u32 luDestinationFactor, u32 luBlendOp,
                                    renderengine::BlendStateParameters& lrParameters)
        {
            if (lpTemplate != 0)
            {
                renderengine::BlendState::GetParameters(lpTemplate, &lrParameters);
                return;
            }
            lrParameters.maBlendFactor[0] =
                (luSourceFactor & 0x1Fu)
                | (32u * (((luDestinationFactor * 8u) & 0x7F8u) | (luBlendOp & 0xFFFFF807u))) & 0xFFE0u
                | 0x07060000u;
            lrParameters.mbHasCustomBlendFactors = 1u;
        }

        // The per-template edit (0x822845B0..0x822845DC, and twice more): the ALPHA half of word 0
        // becomes source ONE (1), op ADD (0), destination INVSRCALPHA (7) -- `stb r28(=7)` into the
        // word's top byte, `rlwinm 0,11,7` + `insrwi r29(=1),5,11` into bits 11..15 -- and the alpha
        // test turns on at GREATER (4) than 1. The COLOUR half is the template's.
        renderengine::BlendMaterialState* BuildBlendState(renderengine::BlendStateParameters& lrParameters)
        {
            u32& lruWord = lrParameters.maBlendFactor[0];
            lruWord = (lruWord & 0x00FFFFFFu) | (7u << 24);              // stb 7 -> byte 0 (big-endian)
            lrParameters.mbState16 = 1u;                                 // AlphaTestEnable (params +0x36)
            lruWord = (lruWord & 0xFF1FFFFFu);                           // rlwinm r11, r11, 0, 11, 7
            lrParameters.muState15 = 4u;                                 // AlphaFunc GREATER
            lrParameters.muState17 = 1u;                                 // AlphaRef 1
            lruWord = (lruWord & ~0x001F0000u) | (1u << 16);             // insrwi r11, r29(=1), 5, 11

            renderengine::ResourceDescriptorEntry laDescriptor[5];
            renderengine::BlendState::GetResourceDescriptor(laDescriptor, &lrParameters);

            renderengine::BlendMaterialState* lapHandles[5] = { 0, 0, 0, 0, 0 };
            lapHandles[0] = static_cast<renderengine::BlendMaterialState*>(std::malloc(laDescriptor[0].muSize));
            return static_cast<renderengine::BlendMaterialState*>(
                renderengine::BlendState::Initialize(lapHandles, &lrParameters));
        }
    }

    // =============================================================================================
    // Construct @0x82284518. The heap argument is unread (r4 is never touched), exactly as in the
    // sibling SparkRenderer / TrailRenderer Constructs; the three states come from the CRT malloc.
    // =============================================================================================
    void BrnSimpleParticleRenderer::Construct(CgsMemory::HeapMalloc* /*lpHeapMalloc*/,
                                              BrnGraphics::Im3dSmokeRenderer* lpRenderer)
    {
        mpRenderer = lpRenderer;   // `stw r5, 0(r26)`

        renderengine::BlendStateParameters lParameters;

        DefaultConstructBlendParameters(lParameters);
        ReadTemplateParameters(dword_83010F20, 6u, 7u, 0u, lParameters);   // Standard
        mpStandardBlend = BuildBlendState(lParameters);                     // `stw r3, 4(r26)`

        DefaultConstructBlendParameters(lParameters);
        ReadTemplateParameters(0, 6u, 1u, 0u, lParameters);                // dword_83010F24, Additive
        mpAdditiveBlend = BuildBlendState(lParameters);                     // `stw r3, 8(r26)`

        DefaultConstructBlendParameters(lParameters);
        ReadTemplateParameters(0, 6u, 7u, 1u, lParameters);                // dword_83010F28, Subtractive
        mpSubtractiveBlend = BuildBlendState(lParameters);                  // `stw r3, 0xC(r26)`
    }

    // =============================================================================================
    // Dispatch @0x8228CA18. Instruction by instruction:
    //   0x8228CA4C  first == last -> return (nothing else runs)
    //   0x8228CA54  the blend table on the stack, indexed by meBlendMode:
    //                 [0] Normal = +0x04 mpStandardBlend   [1] Subtractive = +0x0C mpSubtractiveBlend
    //                 [2] Additive = +0x08 mpAdditiveBlend
    //   0x8228CA78..0x8228CAE8  asserts "luFirstBatch < luLastBatch" (:1336), the Array
    //               constructed-assert, "luLastBatch <= lBatchArray.GetLength()" (:1337)
    //   0x8228CAEC  shadow::Device::ResetShadowing()
    //   0x8228CAF8  rasteriser dword_83010F3C (cull none), depth-stencil dword_83010F4C (test on,
    //               write off) -- the spark pass's pair
    //   0x8228CB08  the library sampler dword_83010F60 on unit 0
    //   0x8228CB48  the z-fade byte: clear -> BeginRendering(1) (SansZFade); set -> BeginRendering(0),
    //               the depth target's texture state on sampler 1, its half-pixel offset, and
    //               SetConstants(offset, near, far, 0.3)
    //   0x8228CBB0  the stream: SetStreamSource(0, vb, 0, 0) then again with the descriptor stride
    //   per batch:  assert GetVertexCount() > 0 (:1389), assert blend < 3 (:1390), bind the blend
    //               state through the shadow (skipped when the blend lock byte is set), bind the
    //               type's texture on sampler 0 through the shadow, FlushVertexProgramState,
    //               DrawVertices(QUADLIST, start, count)
    //   0x8228CD48  the EndRendering fold: assert mgpActiveRenderer == renderer, clear it
    // =============================================================================================
    void BrnSimpleParticleRenderer::Dispatch(renderengine::VertexBuffer* lpVertexBuffer,
                                             const SimpleParticleBatchArray& lrBatches,
                                             u32 luFirstBatch,
                                             u32 luLastBatch,
                                             CgsRenderTarget* lpDepthTarget,
                                             f32 lfNearPlane,
                                             f32 lfFarPlane,
                                             bool lbZFade)
    {
        if (luFirstBatch == luLastBatch)
            return;

        renderengine::BlendMaterialState* const lapBlendStates[eParticleBlendMax] =
        {
            mpStandardBlend,      // eParticleBlendNormal
            mpSubtractiveBlend,   // eParticleBlendSubtractive
            mpAdditiveBlend,      // eParticleBlendAdditive
        };

        CGS_ASSERT(luFirstBatch < luLastBatch, "luFirstBatch < luLastBatch");
        CGS_ASSERT(lrBatches.GetCount() != -1, "Array used before Construct/Clear was called");
        CGS_ASSERT(luLastBatch <= static_cast<u32>(lrBatches.GetCount()),
                   "luLastBatch <= lBatchArray.GetLength()");

        // ⛔ The renderer is the one thing Dispatch cannot do without: its BeginRendering binds the
        // program pair and the vertex declaration. A null one is a crash, not a missing effect.
        if (mpRenderer == 0)
        {
            return;
        }

        shadow::Device::ResetShadowing();
        ImDeviceSetRasterizerState(gpSkyDomeRasterizerState);     // dword_83010F3C (cull none)
        ImDeviceSetDepthStencilState(gpSkyDomeDepthStencilState); // dword_83010F4C (test on, write off)
          // dword_83010F60 on unit 0

        (void)lfNearPlane;
        (void)lfFarPlane;
        (void)KF_ZFADE_DISTANCE;

        const bool lbUseZFade = false;
        mpRenderer->BeginRendering(BrnGraphics::Im3dSmokeRenderer::KI8_PROGRAM_SANS_ZFADE);

        // The stream. The console reads the stride out of the descriptor off_82FAB6A4's own element
        // table; that descriptor IS the 24-byte BasicColouredTexturedVertex stream the renderer
        // declares (and NativeParticleVertex::VertexIterator::Write fills), so use its packed
        // GPU stride. The separate BasicColouredTexturedVertex CPU record is32 bytes.
        const u32 luVertexStride = NativeParticleVertex::GetStride();
        D3DDevice_SetStreamSource(gpD3DDevice, 0, static_cast<D3DVertexBuffer*>(lpVertexBuffer), 0, 0, 1);
        D3DDevice_SetStreamSource(gpD3DDevice, 0, static_cast<D3DVertexBuffer*>(lpVertexBuffer), 0, luVertexStride, 1);

        for (u32 luBatch = luFirstBatch; luBatch < luLastBatch; ++luBatch)
        {
            const SimpleParticleBatch& lrBatch = lrBatches[luBatch];
            CGS_ASSERT(lrBatch.GetVertexCount() > 0, "lBatch.GetVertexCount() > 0");
            CGS_ASSERT(lrBatch.meBlendMode < static_cast<u32>(eParticleBlendMax),
                       "lBatch.meBlendMode < AttribSys::Enums::ParticleBlend::eParticleBlendMax");

            shadow::Device::SetState(lapBlendStates[lrBatch.meBlendMode]);

            renderengine::Texture* const lpTexture =
                BrnSimpleParticleArray::GetTexture(static_cast<ENativeParticleType>(lrBatch.meParticleType));
            shadow::Device::SetResource(static_cast<void*>(lpTexture), 0);



            shadow::Device::FlushVertexProgramState();
            D3DDevice_DrawVertices(gpD3DDevice, static_cast<D3DPRIMITIVETYPE>(KU_XENOS_QUADLIST),
                                   lrBatch.GetStartVertex(), lrBatch.GetVertexCount());

            gauSimpleParticleDrawnBatches  += 1u;                        // [diag]
            gauSimpleParticleDrawnVertices += lrBatch.GetVertexCount();  // [diag]
        }

        // The EndRendering fold at 0x8228CD48.
        CgsGraphics::ImRendererBase* const lpBase =
            static_cast<CgsGraphics::ImRendererBase*>(mpRenderer);
        CGS_ASSERT(CgsGraphics::ImRendererBase::mgpActiveRenderer == lpBase, "mgpActiveRenderer == this");
        CgsGraphics::ImRendererBase::mgpActiveRenderer = 0;
    }
}
}
