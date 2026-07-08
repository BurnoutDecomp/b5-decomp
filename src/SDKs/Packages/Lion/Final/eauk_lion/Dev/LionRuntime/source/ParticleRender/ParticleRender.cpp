// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/source/ParticleRender/ParticleRender.cpp
//
// cParticleRender -- the Lion (eauk_lion) particle-render pipeline driver. Reconstructed
// from the X360 ARTIST pseudocode + assembly (behaviour authority) and the DecFIGS DWARF
// (declaration shape).
//
// THIS PASS reconstructs cParticleRender::Dispatch (0x82911E98). The other three ledger
// functions in this TU are left declared-only, for the reasons recorded below so the next
// pass can pick them up:
//
//   * cParticleRender::Render          (0x829147F8) -- BLOCKED. The per-emitter frustum cull
//     is VMX128: it loads two un-recovered rodata constant tables (unk_8327F110, a vperm
//     permute table; unk_83123740, a masking constant) whose bytes are not in the dossier,
//     then runs vperm/vcmpgtfp/vcmpequw/vcmpeqfp over the packed frustum planes. The cull
//     cannot be faithfully reconstructed without those table bytes.
//   * cParticleRender::EmitterCubeRender (0x82913C80) -- BLOCKED. The per-particle box clip
//     is VMX128 (lvx128/stvx128/vsubfp/vmsum3fp128/fsel/fnmsubs over the bucket vectors) and
//     multiplies by an un-recovered rodata float constant (flt_82F357F4); the snap-to-plane
//     math cannot be reproduced without that constant's value.
//   * cParticleRender::EmitterRender   (0x82913928) -- BLOCKED on a committed-header type
//     conflict. Its body walks cParticleBucket (mpMatrices/mpVectors/mpEmitterNext), which is
//     only reachable by #including ParticleBucket.h; that header defines HONEST-PLACEHOLDER
//     `struct cMatrix`/`cVector`/`cTime`, which ODR-collide with this header's real
//     `typedef rw::math::vpu::Matrix44 cMatrix`. Unifying the bucket's placeholder math types
//     with the renderer's real cMatrix is a cross-header refactor of committed code
//     (ParticleBucket.h/.cpp) outside this TU's scope; reconstructing EmitterRender here would
//     break the compile gate. Deferred until the bucket's math-type home is unified.
//
// Dispatch's device path: the X360 build inlines the shadow-device sampler-state bind (the
// dword_830109A8 compare + sub_827E8950 + store block) that shadow::Device::SetState owns;
// it is de-inlined back to that call here (semantic parity, one owning body -- AGENTS.md).
// ============================================================================

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleRender/ParticleRender.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleMaterial.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"   // shadow::Device
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT

// --- Platform (Xbox 360 D3D) device + fast-path draw thunks ------------------------------------
// The engine's single D3D device global (X360 off_83271608 == renderengine::gpD3DDevice, defined
// in the renderengine device TU) and the two Xenon D3DDevice_* thunks Dispatch binds through. No
// project TU homes the thunks; declared here as the minimal extern surface, matching the XDK d3d9
// fast-set API and the shadow-device precedent (shadowingdevice.cpp).
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

// The Lion particle path's sampler-state object (X360 dword_83010F60): Dispatch binds it on
// sampler 0 through the shadow device before rendering. Set up by the particle module init; no
// project TU homes it yet, so it is referenced by name as an external.
extern void* gpLionParticleSamplerState;

// The Xenon particle draw primitive type (X360 li r4, 0xD passed to D3DDevice_DrawVertices).
static const u32 KU_PARTICLE_PRIMITIVE_TYPE = 13;

// ----------------------------------------------------------------------------
// cParticleRender::Dispatch @ 0x82911E98
//
// Replay the frame's accumulated batch list to the device. Called by cLionFX::Dispatch.
// ----------------------------------------------------------------------------
void cParticleRender::Dispatch(renderengine::VertexBuffer* apVertexBuffer,
                               const LionBatchArray& arBatchArray,
                               float32_t afWhiteLevel,
                               bool8_t abEnableZFade,
                               float32_t afNearPlane,
                               float32_t afFarPlane,
                               float32_t afDepthFadeDistance,
                               float32_t afDepthSamplerOffsetU,
                               float32_t afDepthSamplerOffsetV,
                               renderengine::TextureState* apDepthTextureState)
{
    // Nothing to replay if the batch list is empty. GetLength() fires the "Array used before
    // Construct/Clear was called" assert on the -1 sentinel, matching the X360 length read.
    if (arBatchArray.GetLength() == 0)
    {
        return;
    }

    // Reset the shadow device and bind the empty stream / particle sampler state for the pass.
    shadow::Device::ResetShadowing();
    D3DDevice_SetStreamSource(gpD3DDevice, 0, apVertexBuffer, 0, 0, 1);
    shadow::Device::SetState(gpLionParticleSamplerState, 0);

    mpRenderer->BeginRendering(afWhiteLevel, abEnableZFade, afNearPlane, afFarPlane,
                               afDepthFadeDistance, afDepthSamplerOffsetU, afDepthSamplerOffsetV,
                               apDepthTextureState);

    // Walk every batch, opening a render group each time the material changes and issuing one
    // DrawVertices per batch. The length is re-read each iteration (its assert re-fires) to match
    // the X360 loop.
    const cParticleMaterial* lpCurrentMaterial = nullptr;
    for (u32 luIndex = 0; luIndex < arBatchArray.GetLength(); ++luIndex)
    {
        const LionBatch& lrBatch = arBatchArray.GetItem(luIndex);
        CGS_ASSERT(lrBatch.GetVertexCount() > 0, "lBatch.GetVertexCount() > 0");

        if (lrBatch.GetMaterial() != lpCurrentMaterial)
        {
            if (lpCurrentMaterial != nullptr)
            {
                mpRenderer->RenderGroupEnd();
            }
            lpCurrentMaterial = lrBatch.GetMaterial();
            mpRenderer->RenderGroupBegin(*lpCurrentMaterial);
        }

        const u32 luVertexStride = mpRenderer->GetVertexStride(*lpCurrentMaterial);
        D3DDevice_SetStreamSource(gpD3DDevice, 0, apVertexBuffer, 0, luVertexStride, 1);

        const u32 luStartVertex = lrBatch.GetStartVertex();
        const u32 luVertexCount = lrBatch.GetVertexCount();
        shadow::Device::FlushVertexProgramState();
        D3DDevice_DrawVertices(gpD3DDevice, KU_PARTICLE_PRIMITIVE_TYPE, luStartVertex, luVertexCount);
    }

    mpRenderer->RenderGroupEnd();
    mpRenderer->EndRendering();
}
