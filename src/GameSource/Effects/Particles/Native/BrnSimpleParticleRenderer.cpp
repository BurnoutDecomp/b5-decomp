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
#include "GameSource/Effects/Particles/Native/BrnIm3dSmokeRenderer.h"
#include "GameSource/Effects/Particles/Native/BrnSimpleFxDiag.h"   // [diag] BRN_SIMPLEFX_DIAG
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"    // ImRendererBase::mgpActiveRenderer
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasicColouredTexturedVertex.h"  // the 24-byte stride
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"      // shadow::Device
#include "pc/gcm/renderengine/ShadowPassPCLeaf.h"                           // LionParticleSampler_ApplyState

#include <cstdio>    // [diag] snprintf
#include <cstdlib>   // malloc

// The device surface Dispatch binds through -- the same minimal extern surface SparkRenderer::Dispatch
// (BrnSparkRenderer_Render.cpp) and the Lion dispatch declare.
struct IDirect3DDevice9;
extern IDirect3DDevice9* gpD3DDevice;
extern "C"
{
    void D3DDevice_SetStreamSource(IDirect3DDevice9* lpDevice, u32 luStreamNumber,
                                   const void* lpStreamData, u32 luOffsetInBytes,
                                   u32 luStride, u32 luFlags);
    void D3DDevice_DrawVertices(IDirect3DDevice9* lpDevice, u32 luPrimitiveType,
                                u32 luStartVertex, u32 luVertexCount);
}

// The shared-library rasteriser and depth-stencil states Dispatch binds (ImmediateModePCLeaf.cpp).
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

        // The three templates Construct reads are the immediate-mode library's first three blend
        // states, built by ImRendererBase::ConstructOnceOnly @0x827F1C20 as
        //     dword_83010F20 = ConstructBlendState(6, 7, 0)   Standard     SRCALPHA / INVSRCALPHA, ADD
        //     dword_83010F24 = ConstructBlendState(6, 1, 0)   Additive     SRCALPHA / ONE,         ADD
        //     dword_83010F28 = ConstructBlendState(6, 7, 1)   Subtractive  SRCALPHA / INVSRCALPHA, op 1
        // (0x827F1C34..0x827F1C80). FLAG PC-platform: that library is not built on this backend
        // (ImRendererBase::ConstructOnceOnly is an empty PC leaf -- the same fact LionParticleRender's
        // CreateInternalMaterial works around for dword_83010F20), so a template word reads null and
        // the console's unconditional GetParameters would fault. Where the template is absent this
        // writes what GetParameters would have read back out of it: ConstructBlendState's own block
        // (CgsImRendererBlendState.cpp @0x827ED118 -- word 0 = src | op << 5 | dst << 8 | 0x07060000,
        // custom factors ON, everything else the defaults above). DELETE-WHEN a caller fills the
        // render-state library.
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
            static bool sbLogged = false;
            if (!sbLogged && SimpleFxDiagArmed())      // [diag] default OFF (BRN_SIMPLEFX_DIAG)
            {
                sbLogged = true;
                CgsDev::Log::WriteToLog("[simplefx] Dispatch SKIPPED: mpRenderer is null "
                                        "(BrnSimpleParticleRenderer::Construct did not run)\n");
            }
            return;
        }

        shadow::Device::ResetShadowing();
        ImDeviceSetRasterizerState(gpSkyDomeRasterizerState);     // dword_83010F3C (cull none)
        ImDeviceSetDepthStencilState(gpSkyDomeDepthStencilState); // dword_83010F4C (test on, write off)
        renderengine::LionParticleSampler_ApplyState(0);          // dword_83010F60 on unit 0

        // THE Z-FADE ARM IS NOT DRAWN ON THIS BUILD -- FLAG PC-platform, stated rather than faked.
        // With the flag set the console binds program 0 (the soft-particle pair), puts the depth
        // target's resolved texture state on sampler 1 (`lwz r10, 0x108(r28) ; lwz r4, 0x8C(r10)` into
        // ImRendererBase::SetState(TextureState*, 1) @0x8227D228), and calls
        // Im3dSmokeRenderer::SetConstants(GetHalfPixelOffset(target), near, far, KF_ZFADE_DISTANCE)
        // (0x8228CB64..0x8228CBAC). Three things that arm reads do not exist here: the target itself
        // (ParticleModule::RenderQuarterResParticles is reached from BrnRendererModule::Render WITHOUT
        // the `*(renderer + 0x25C)` the console passes in r5), the post-fx target's +0x8C texture
        // state, and rw::graphics::postfx::RenderTarget::GetHalfPixelOffset @0x823FE668 (inlined on
        // every other PC call site). Binding program 0 over an unbound depth sampler would draw
        // garbage, so the SansZFade pair draws either way and the request is reported once.
        // SetConstants is reconstructed (BrnIm3dSmokeRenderer.cpp) and waits for that plumbing. On
        // this build the flag is (muFlags & 0x40) || mbZFadeEnabled: only a REDUCED frame rate or the
        // debug toggle sets it. DELETE-WHEN the depth target reaches this call.
        (void)lpDepthTarget;
        (void)lfNearPlane;
        (void)lfFarPlane;
        (void)KF_ZFADE_DISTANCE;
        if (lbZFade)
        {
            static bool sbLogged = false;
            if (!sbLogged && SimpleFxDiagArmed())      // [diag] default OFF (BRN_SIMPLEFX_DIAG)
            {
                sbLogged = true;
                CgsDev::Log::WriteToLog("[simplefx] Dispatch: z-fade requested; this build has no depth "
                                        "target to sample -- drawing the SansZFade program\n");
            }
        }
        const bool lbUseZFade = false;
        mpRenderer->BeginRendering(BrnGraphics::Im3dSmokeRenderer::KI8_PROGRAM_SANS_ZFADE);

        // The stream. The console reads the stride out of the descriptor off_82FAB6A4's own element
        // table; that descriptor IS the 24-byte BasicColouredTexturedVertex stream the renderer
        // declares (and NativeParticleVertex::VertexIterator::Write fills), so the stride is taken
        // from the type -- the same substitution SparkRenderer::Dispatch documents as its FLAG (1).
        const u32 luVertexStride =
            static_cast<u32>(sizeof(CgsGraphics::BasicColouredTexturedVertex));   // 24
        D3DDevice_SetStreamSource(gpD3DDevice, 0, lpVertexBuffer, 0, 0, 1);
        D3DDevice_SetStreamSource(gpD3DDevice, 0, lpVertexBuffer, 0, luVertexStride, 1);

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

            // [DIAG] NOT IN THE X360 BINARY. The texture witness, once per type, BRN_SIMPLEFX_DIAG=1
            // only (default OFF): a null texture keeps every count perfect and draws nothing.
            // DELETE-WHEN-STABLE.
            {
                static u32 suLoggedTypes = 0;
                const u32 luBit = 1u << (lrBatch.meParticleType & 31u);
                if ((suLoggedTypes & luBit) == 0 && SimpleFxDiagArmed())
                {
                    suLoggedTypes |= luBit;
                    char lacMsg[192];
                    std::snprintf(lacMsg, sizeof(lacMsg),
                        "[simplefx] draw type %u: texture=%p blend=%u verts=%u start=%u zfade=%d\n",
                        lrBatch.meParticleType, static_cast<const void*>(lpTexture), lrBatch.meBlendMode,
                        lrBatch.muVertexCount, lrBatch.muStartVertex, lbUseZFade ? 1 : 0);
                    CgsDev::Log::WriteToLog(lacMsg);
                }
            }

            shadow::Device::FlushVertexProgramState();
            D3DDevice_DrawVertices(gpD3DDevice, KU_XENOS_QUADLIST,
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
