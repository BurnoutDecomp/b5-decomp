// ============================================================================
// GameSource/Unity/../Effects/Particles/LionParticleRender.cpp
//
// BrnParticle::LionParticleRender -- Burnout's concrete iParticleRender over the Lion
// (eauk_lion) particle runtime. Reconstructed from the X360 ARTIST pseudocode + asm
// (behaviour authority) and the DecFIGS DWARF / burnout.wiki (declaration / layout shape).
//
// The process-wide tables (acquired textures, internal-material -> blend-state map, the
// depth-stencil state set, the cached bound texture) are file-scope statics in the
// original TU; they are modelled here as an anonymous-namespace state block. The X360
// build keeps them as raw .data globals (qword_82FAC3A0 / qword_82FAAD80 / dword_82FAB580
// etc.); the named members below preserve the same semantics.
// ============================================================================

#include "GameSource/Effects/Particles/LionParticleRender.h"

#include "GameSource/Effects/Particles/Native/BrnLionBlendRenderer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // the [texreg] witness
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleDescriptor.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleEmitter.h"
#include "pc/gcm/renderengine/renderstates.h"   // renderengine::DepthStencilState / TextureState / MaterialState
#include "pc/gcm/renderengine/texture.h"          // renderengine::Texture
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"  // shadow::Device::SetState -- the rasteriser third BeginRendering binds

#include <cstdio>   // snprintf (the [texreg] witness)

// D3DDevice_SetTexture -- the platform (Xbox 360 D3D) texture-bind entry point. X360 call:
// D3DDevice_SetTexture(off_83271608, 0, texture, 0x80000000) -- (device, sampler, texture,
// flags); the 0x80000000 flag requests the "set immediately / no GPU stall" path. Declared
// here as a free function so the texture-bind side effect is preserved (PC backend body / stub).
extern "C" void D3DDevice_SetTexture(void* apDevice, unsigned int auSampler,
                                     void* apTexture, unsigned int auFlags);

// The global D3D device the particle renderer binds textures on (off_83271608). Modelled as a
// file-scope pointer the platform layer fills; on PC it resolves to the active device.
namespace { void* spD3DDevice = 0; }

// -------------------------------------------------------------------------------------------
// dword_83010F20 -- the process-wide DEFAULT blend-state template. Its console .data home is
// THIS translation unit (the LionParticleRender TU), which is why BrnLionBlendRenderer.cpp
// externs it rather than owning it; BrnGraphics::LionBlendRenderer::EndRendering @0x8227E610
// binds it at every batch end (`bl ImRendererBase::SetState` @0x82276D08 with r4 == this word).
//
// ⛔ IT READS NULL ON THIS BUILD, AND THAT IS A MEASURED FACT ABOUT THE STATE LIBRARY, NOT A
// PLACEHOLDER FOR THIS PATH. The entry belongs to the console's render-state table, and nothing
// on this build populates that table: shadowingdevice.cpp:614-628 records the same thing for
// its sibling (CgsBlendStateFactory::Construct has a body but no caller), which is exactly why
// shadow::Device::Xbox2SetStateLowLevelShadowed carries a null guard the console does not have.
// A null bind there invalidates the shadow cache and binds nothing -- the console's own
// semantics for an unset state -- so this is inert rather than wrong.
//
// ⛔⛔ DO NOT POINT IT AT CgsGui::gpGuiBlendStateStandard, which names the SAME console word
// (dword_83010F20) for the 2D GUI path. That one is an opaque four-word SENTINEL over static
// storage whose own banner says "nothing may dereference them" -- the GUI dispatch only compares
// its identity. The blend applier here DOES dereference (eighteen words), so adopting it would
// read past a 16-byte array and bind garbage.
// DELETE-WHEN a caller fills the render-state table.
// -------------------------------------------------------------------------------------------
renderengine::BlendMaterialState* dword_83010F20 = 0;

namespace BrnParticle
{
namespace
{
    // ------------------------------------------------------------------------
    // Process-wide tables shared by every LionParticleRender instance (the X360
    // build keeps these as .data globals; reproduced here as file-scope state).
    // ------------------------------------------------------------------------

    // The maximum number of textures the particle system can have registered at once
    // (AcquireTexture asserts on overflow). X360: hard-coded 0x100 compare.
    const u32 KU_MAX_PARTICLE_TEXTURES = 256;

    // The stack matrix array the cVector Render overload (sub_82289158) expands into: 2048
    // bytes at sp+0x60 of a 0x880 frame == 32 cMatrix, the same run cParticleRender::
    // EmitterRender streams with.
    const u32 KU_VECTOR_DRAW_RUN = 32;

    // Acquired-texture table: indexed by AcquireTexture's running counter and read back
    // by FindTexture once a texture-map hash resolves to an array index (qword_82FAC3A0).
    CgsResource::SafeResourceHandle<renderengine::Texture> saTextures[KU_MAX_PARTICLE_TEXTURES];
    s32 siTextureCount = 0;                          // dword_82FAB6A0

    // Internal-material -> blend-state map (qword_82FAAD80). One entry per distinct
    // material render-state combination; the DWARF names this BlendStateMapEntry.
    struct BlendStateMapEntry
    {
        u32 muHash;                                  // FNV-1a of the material render bytes
        BrnGraphics::BlendState* mpBlendState;
    };
    BlendStateMapEntry saBlendStates[256];           // qword_82FAAD80
    s32 siMaterialCount = 0;                          // dword_82FAB69C

    // Depth-stencil state set built by Setup(): 4 z-test/z-write combinations
    // (dword_82FAB580[4]) plus the always-on default (dword_82FAB590).
    renderengine::DepthStencilState* saDepthStencilStates[4] = { 0, 0, 0, 0 };
    renderengine::DepthStencilState* spDefaultDepthStencil = 0;   // dword_82FAB590

    // Cached bound texture so SetMaterial only re-binds when the texture changes
    // (dword_830109E8 / dword_83010968).
    renderengine::Texture* spBoundTexture = 0;
    s32 siBoundTextureDirty = 0;

    // Default render states BeginRendering binds after starting the immediate-mode batch
    // (dword_83010F48 / dword_83010F3C): a default depth-stencil state and a default blend
    // state the renderer resets to at the start of each rendering pass. They are configured by
    // the renderer-construction path (out of this TU's scope); modelled as file-scope pointers.
    renderengine::DepthStencilState* spBeginDepthStencilState = 0;   // dword_83010F48
    // ⭐ CORRECTED 2026-09-05: dword_83010F3C IS A RASTERISER STATE, NOT A BLEND STATE, and the
    // asm has always said so -- LionParticleRender::BeginRendering @0x82289568 word 18 calls
    // sub_82276E48, whose lock byte is mbRasteriserStateLocked and whose cache slot is
    // dword_83010A2C (the rasteriser third), NOT the blend wrapper @0x82276D08. This global was
    // typed BlendState* and that mis-typing is what made the SetState pair look like a
    // contradiction for two waves: they read a call LionParticleRender makes for ITSELF as if it
    // came through LionBlendRenderer::SetState. It does not. See the header banner in
    // BrnLionBlendRenderer.h.
    const renderengine::RasterizerState* spBeginRasterizerState = 0;   // dword_83010F3C
}

// ----------------------------------------------------------------------------
// LionParticleRender::LionParticleRender (X360 0x82280948)
// The ctor only clears the two renderer pointers; the heavyweight state (matrices,
// the name map, the heap) is configured later by SetCameraData / SetAllocator / Setup.
// ----------------------------------------------------------------------------
LionParticleRender::LionParticleRender()
{
    mpRenderer        = 0;
    mpCurrentRenderer = 0;
}

// ----------------------------------------------------------------------------
// LionParticleRender::HashMaterial (X360 0x82279CC0)
// FNV-1a over the four render-state bytes mBlendMode / mAlphaTestMode / mAlphaTestValue /
// mZTestMode (material bytes +0x3A..+0x3D). This is the key that identifies an internal
// material (a unique depth/blend render-state combination).
// ----------------------------------------------------------------------------
U32 LionParticleRender::HashMaterial(const cParticleMaterial* apMaterial)
{
    CGS_ASSERT(apMaterial != 0, "apMaterial != NULL");

    const u32 KU_FNV_OFFSET_BASIS = 0x811C9DC5u;
    const u32 KU_FNV_PRIME        = 0x01000193u;

    u32 luHash = KU_FNV_OFFSET_BASIS;
    luHash = (luHash ^ apMaterial->mBlendMode)      * KU_FNV_PRIME;
    luHash = (luHash ^ apMaterial->mAlphaTestMode)  * KU_FNV_PRIME;
    luHash = (luHash ^ apMaterial->mAlphaTestValue) * KU_FNV_PRIME;
    luHash = (luHash ^ apMaterial->mZTestMode)      * KU_FNV_PRIME;
    return luHash;
}

// ----------------------------------------------------------------------------
// LionParticleRender::GetMaterialHandle (X360 0x82279D50)
// Look a material hash up in the internal-material table. Returns the hash itself as the
// "handle" when an entry with that hash already exists, otherwise 0 (caller then creates
// a fresh internal material).
// ----------------------------------------------------------------------------
U32 LionParticleRender::GetMaterialHandle(U32 auMaterialHash)
{
    CGS_ASSERT(auMaterialHash != 0, "luMaterialHash != 0");

    for (s32 liIndex = 0; liIndex < siMaterialCount; ++liIndex)
    {
        if (saBlendStates[liIndex].muHash == auMaterialHash)
            return auMaterialHash;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// LionParticleRender::GetCameraMatrix (X360 0x82279C98)
// Return the stored camera transform by value (the asm copies the 64-byte matrix).
// ----------------------------------------------------------------------------
cMatrix LionParticleRender::GetCameraMatrix()
{
    return mCameraTransform;
}

// ----------------------------------------------------------------------------
// LionParticleRender::GetVertexStride (X360 0x82279DF8)
// The Lion blend renderer always emits a 36-byte vertex. Two debug asserts guard that the
// material is a Lion-shader material and that a renderer is bound.
// ----------------------------------------------------------------------------
U32 LionParticleRender::GetVertexStride(const cParticleMaterial& arMaterial)
{
    CGS_ASSERT(arMaterial.GetShaderType() == cParticleMaterial::eSHADER_LION,
               "lMaterial.GetShaderType() == cParticleMaterial::eSHADER_LION");
    CGS_ASSERT(mpRenderer != 0, "NULL != lpShader");
    return 36;
}

// ----------------------------------------------------------------------------
// LionParticleRender::RenderGroupEnd (X360 0x82279C80)
// Clear the current renderer (only when one is set).
// ----------------------------------------------------------------------------
void LionParticleRender::RenderGroupEnd()
{
    if (mpCurrentRenderer != 0)
        mpCurrentRenderer = 0;
}

// ----------------------------------------------------------------------------
// LionParticleRender::RenderGroupEndLite (X360 0x82279C70)
// Unconditionally clear the current renderer.
// ----------------------------------------------------------------------------
void LionParticleRender::RenderGroupEndLite()
{
    mpCurrentRenderer = 0;
}

// ----------------------------------------------------------------------------
// LionParticleRender::RenderGroupBegin (X360 0x82294198)
// Select the bound renderer as the current renderer and load the per-material render state.
// ----------------------------------------------------------------------------
void LionParticleRender::RenderGroupBegin(const cParticleMaterial& arMaterial)
{
    CGS_ASSERT(arMaterial.GetShaderType() == cParticleMaterial::eSHADER_LION,
               "lMaterial.GetShaderType() == cParticleMaterial::eSHADER_LION");

    mpCurrentRenderer = mpRenderer;
    CGS_ASSERT(mpCurrentRenderer != 0, "A valid renderer is expected here.");

    SetMaterial(&arMaterial);
}

// ----------------------------------------------------------------------------
// LionParticleRender::RenderGroupBeginLite (X360 0x822894C8)
// The "lite" path: select the renderer and push the camera matrices, but does not bind a
// material (used by the emitter-render fast path).
// ----------------------------------------------------------------------------
void LionParticleRender::RenderGroupBeginLite(const cParticleMaterial& arMaterial)
{
    CGS_ASSERT(arMaterial.GetShaderType() == cParticleMaterial::eSHADER_LION,
               "lMaterial.GetShaderType() == cParticleMaterial::eSHADER_LION");

    mpCurrentRenderer = mpRenderer;
    CGS_ASSERT(mpCurrentRenderer != 0, "A valid renderer is expected here.");

    mpCurrentRenderer->SetCameraData(mBackMat, mViewMat, mViewProjection);
}

// ----------------------------------------------------------------------------
// LionParticleRender::FindTexture (X360 0x822895D8)
// Resolve a texture-map hash to the acquired texture: walk the texture-name map for the
// matching entry index, then return that slot from the acquired-texture table.
// ----------------------------------------------------------------------------
renderengine::Texture* LionParticleRender::FindTexture(U32 auTextureMapHandle) const
{
    CGS_ASSERT(auTextureMapHandle != 0, "aTextureMapHandle != NULL");

    renderengine::Texture* lpTexture = 0;

    const BrnParticle::TextureNameMap::Entry* lpEntries = mTextureNameMap->GetEntries();
    const u32 luEntryCount = mTextureNameMap->GetEntryCount();

    for (u32 luIndex = 0; luIndex < luEntryCount; ++luIndex)
    {
        if (lpEntries[luIndex].muHashedLionTextureName == auTextureMapHandle)
        {
            lpTexture = saTextures[luIndex];
            break;
        }
    }

    CGS_ASSERT(lpTexture != 0, "lpTexture != NULL");
    return lpTexture;
}

// ----------------------------------------------------------------------------
// LionParticleRender::SetMaterial (X360 0x822896B8)
// Bind the texture, depth-stencil and blend render-states for a material on the current
// renderer. The texture is only re-bound on the D3D device when it changes.
// ----------------------------------------------------------------------------
void LionParticleRender::SetMaterial(const cParticleMaterial* apMaterial)
{
    if (apMaterial == 0)
    {
        CGS_ASSERT(apMaterial, "apMaterial");
        return;
    }

    CGS_ASSERT(mpCurrentRenderer != 0, "mpCurrentRenderer != NULL");
    BrnGraphics::LionBlendRenderer* lpRenderer = mpCurrentRenderer;

    // Resolve + bind the material's texture (only touch the device when it changes).
    renderengine::Texture* lpTexture = FindTexture(apMaterial->mTextureHandle);
    if (spBoundTexture != lpTexture)
    {
        D3DDevice_SetTexture(spD3DDevice, 0, lpTexture, 0x80000000u);
        spBoundTexture = lpTexture;
        siBoundTextureDirty = 0;
    }

    // Pick the depth-stencil state from the material's z-test / z-write flag bits (inverted:
    // a set flag disables that test, selecting the matching pre-built state).
    const u32 luFlags = apMaterial->mFlags;
    const u32 luDepthIndex = ((~luFlags >> 5) & 1) + ((~luFlags >> 3) & 2);
    lpRenderer->SetState(saDepthStencilStates[luDepthIndex]);

    // Find the internal-material blend state for this material handle and bind it.
    s32 liIndex = 0;
    if (siMaterialCount > 0)
    {
        while (liIndex < siMaterialCount)
        {
            if (saBlendStates[liIndex].muHash == apMaterial->mMaterialHandle)
                break;
            ++liIndex;
        }
    }
    if (liIndex != siMaterialCount)
        lpRenderer->SetState(saBlendStates[liIndex].mpBlendState);
}

// ----------------------------------------------------------------------------
// LionParticleRender::TextureRegister (X360 0x82289398)
// Register a material's texture (+ optional normal map) with the renderer: ensure the
// material has an internal-material handle, then store the resolved texture / normal-map
// name hashes back into the material.
// ----------------------------------------------------------------------------
void LionParticleRender::TextureRegister(cParticleMaterial* apMaterial, char* apcTextureName)
{
    CGS_ASSERT(apMaterial, "apMaterial");
    CGS_ASSERT(apMaterial->GetShaderType() == cParticleMaterial::eSHADER_LION,
               "apMaterial->GetShaderType() == cParticleMaterial::eSHADER_LION");
    CGS_ASSERT(apMaterial->mUCoordOption != cParticleMaterial::eUVOPTION_FLIPPED,
               "apMaterial->mUCoordOption != cParticleMaterial::eUVOPTION_FLIPPED");
    CGS_ASSERT(apMaterial->mVCoordOption != cParticleMaterial::eUVOPTION_FLIPPED,
               "apMaterial->mVCoordOption != cParticleMaterial::eUVOPTION_FLIPPED");

    const u32 luMaterialHash = HashMaterial(apMaterial);
    u32 luMaterialHandle = GetMaterialHandle(luMaterialHash);
    if (luMaterialHandle == 0)
        luMaterialHandle = CreateInternalMaterial(apMaterial);
    apMaterial->mMaterialHandle = luMaterialHandle;

    // Entry::HashString ignores `this` (the X360 body takes only the string in r3); the
    // committed design declares it as a non-static Entry member, so it is invoked through an
    // instance (matching the sibling BrnSimpleParticleArray usage).
    BrnParticle::TextureNameMap::Entry lHasher;
    const u32 luTextureHash = lHasher.HashString(apcTextureName);
    apMaterial->SetTextureMapHandle(luTextureHash);

    char* lpcNormalMapName = apMaterial->mpNormalMapName.Get();
    if (lpcNormalMapName != 0)
    {
        const u32 luNormalHash = lHasher.HashString(lpcNormalMapName);
        apMaterial->SetNormalMapHandle(luNormalHash);
    }

    // ---- [texreg] witness. NOT console behaviour: ours, bounded, log-only. -------------------
    // ⭐ WHY A COUNT AND NOT A ONE-SHOT. This function is the ONLY place a particle material ever
    // gets a texture-map handle, and it is reached only while gpLionParticleRender is non-null --
    // which, before cLionFX::Init landed, it never was. A one-shot line would prove the path is
    // live and say nothing about COVERAGE, and "at least one material registered" is exactly the
    // kind of claim this project keeps having to retract. So: the first registration, then every
    // 32nd, then a final line -- each carrying the material's texture NAME and the handle the
    // hash produced, because a wrong record layout would give a garbage name or a zero handle
    // here rather than nothing at all. DELETE-WHEN-STABLE.
    {
        static u32 suRegistered = 0;
        ++suRegistered;
        if (suRegistered == 1 || (suRegistered % 32) == 0)
        {
            char lacMsg[256];
            std::snprintf(lacMsg, sizeof(lacMsg),
                          "[texreg] #%u material=%p texture=\"%s\" texHandle=%08X matHandle=%08X\n",
                          suRegistered, static_cast<const void*>(apMaterial),
                          apcTextureName ? apcTextureName : "<null>",
                          luTextureHash, apMaterial->mMaterialHandle);
            CgsDev::Log::WriteToLog(lacMsg);
        }
    }
}

// ----------------------------------------------------------------------------
// LionParticleRender::AcquireTexture (X360 0x82280B60)
// Append a resolved texture handle to the process-wide acquired-texture table.
// ----------------------------------------------------------------------------
void LionParticleRender::AcquireTexture(U32 /*auTextureMapHandle*/,
                                        CgsResource::SafeResourceHandle<renderengine::Texture> aTexture)
{
    CGS_ASSERT(siTextureCount < static_cast<s32>(KU_MAX_PARTICLE_TEXTURES), "Too many textures");
    saTextures[siTextureCount] = aTexture;
    ++siTextureCount;
}

// ----------------------------------------------------------------------------
// LionParticleRender::Render (X360 0x82289050)
// Dispatch one particle group to the current renderer, selecting the draw shape from the
// emitter's descriptor render mode (0 sprites, 1 quads, 3/4 tilts; 2/other = nothing).
// Does nothing when the group is empty (auCount == 0).
// ----------------------------------------------------------------------------
void LionParticleRender::Render(EffectsVertexBufferIterator& arIterator,
                                RenderedParticle* apParticle,
                                const cMatrix* apMatrix,
                                U32 auCount,
                                U32 /*auFlags*/,
                                const cParticleEmitter* apEmitter,
                                const cLionFog* /*apFog*/,
                                const cTime& arTime)
{
    CGS_ASSERT(mpCurrentRenderer != 0, "No renderer set up. RenderGroupBegin not called");

    if (auCount == 0)
        return;

    BrnGraphics::LionBlendRenderer* lpRenderer = mpCurrentRenderer;
    const cParticleDescriptor* lpDescriptor = apEmitter->GetDescriptor();

    switch (lpDescriptor->GetRenderMode())
    {
        case 0:  // sprites
            lpRenderer->RenderSprites(arIterator, apParticle, apMatrix, auCount, apEmitter, arTime);
            break;
        case 1:  // quads
            lpRenderer->RenderQuads(arIterator, apParticle, apMatrix, auCount, apEmitter, arTime);
            break;
        case 3:  // tilts
        case 4:
            lpRenderer->RenderTilts(arIterator, apParticle, apMatrix, auCount, apEmitter, arTime);
            break;
        default: // case 2 + everything else: no geometry for this shape
            break;
    }
}

// ----------------------------------------------------------------------------
// LionParticleRender::BeginRendering (X360 0x82289568)
// Start the bound renderer's immediate-mode batch with the stored view-projection matrix and
// the fog / alpha-test parameters, then reset the renderer to the default depth-stencil and
// blend states for the pass. The X360 build emits the begin call against mpRenderer's Im3dBlend
// sub-object and then two SetState calls against its immediate-mode renderer.
// ----------------------------------------------------------------------------
void LionParticleRender::BeginRendering(float32_t afNear, bool8_t abFogEnable,
                                        float32_t afFogNear, float32_t afFogFar,
                                        float32_t afD, float32_t afE, float32_t afF,
                                        renderengine::TextureState* apTextureState)
{
    mpRenderer->BeginRendering(mViewProjection, afNear, abFogEnable, afFogNear, afFogFar,
                               afD, afE, afF, apTextureState);
    mpRenderer->SetState(spBeginDepthStencilState);  // sub_82276DA8(mpRenderer+4, dword_83010F48)

    // sub_82276E48(mpRenderer+4, dword_83010F3C) -- the RASTERISER third, bound by this class
    // directly on the renderer's ImRendererBase rather than through LionBlendRenderer (which the
    // DWARF gives only the DepthStencilState and BlendState overloads). shadow::Device::SetState
    // is what that wrapper's body is, minus the mgpActiveRenderer assert.
    shadow::Device::SetState(spBeginRasterizerState);
}

// ----------------------------------------------------------------------------
// LionParticleRender::EndRendering (X360 0x82280958)
// End the bound renderer's immediate-mode batch and restore the always-on default depth-stencil
// state for the next pass. The function IS X360-attested -- it is called by cParticleRender::
// Dispatch (xref @0x82911E98). The asm:
//   r10 = mpRenderer       (lwz r10,0x160(r31))
//   sub_82276DA8(mpRenderer+4, dword_82FAB590)   ; depth-stencil setter <- spDefaultDepthStencil
//   LionBlendRenderer::EndRendering(mpRenderer)   (lwz r3,0x160(r31); bl ...)
// dword_82FAB590 is spDefaultDepthStencil -- the always-on default depth-stencil state built by
// Setup() and restored at batch end (NOT spBeginDepthStencilState / dword_83010F48, which is the
// per-pass begin state bound in BeginRendering). sub_82276DA8 is the depth-stencil setter, the
// same routine BeginRendering routes through; it is modelled as LionBlendRenderer::SetState
// (which is sub_82276DA8(mpRenderer+4, state)).
// ----------------------------------------------------------------------------
void LionParticleRender::EndRendering()
{
    mpRenderer->SetState(spDefaultDepthStencil);  // sub_82276DA8(mpRenderer+4, dword_82FAB590)
    mpRenderer->EndRendering();
}

// ----------------------------------------------------------------------------
// LionParticleRender::TextureUnRegister (declared by the interface; the X360 body for this
// class is a no-op stub -- the acquired-texture table is torn down wholesale elsewhere).
// ----------------------------------------------------------------------------
void LionParticleRender::TextureUnRegister(cParticleMaterial* /*apMaterial*/)
{
}

// ----------------------------------------------------------------------------
// LionParticleRender::Setup (X360 0x822809A0)
// Build the process-wide depth-stencil state set: four z-test/z-write combinations plus the
// always-on default. Each state is allocated from the configured heap and initialised from a
// positionally-filled DepthStencilState::Parameters block.
// ----------------------------------------------------------------------------
void LionParticleRender::Setup()
{
    using renderengine::DepthStencilState;

    // The four combinations: outer index = depth-test enable, inner index = depth-write enable.
    s32 liStateIndex = 0;
    for (u32 luTestPass = 0; luTestPass < 2; ++luTestPass)
    {
        for (u32 luWritePass = 0; luWritePass < 2; ++luWritePass)
        {
            DepthStencilState::Parameters lParams;
            lParams.muFunction        = 3;   // leading state word (asm li r24,3)
            lParams.maState1[0]       = 0;
            lParams.maState1[1]       = 0;
            lParams.maState1[2]       = 0;
            lParams.muState4          = DepthStencilState::E_FUNCTION_ALWAYS;  // 7
            lParams.maState5[0]       = 0;
            lParams.maState5[1]       = 0;
            lParams.maState5[2]       = 0;
            lParams.muState8          = DepthStencilState::E_FUNCTION_ALWAYS;  // 7
            lParams.muState9          = 0;
            lParams.muState10         = 0;
            lParams.muStencilReadMask = 0xFFFFFFFFu;
            lParams.muStencilWriteMask= 0xFFFFFFFFu;
            lParams.muState13         = 0;
            lParams.muState14         = 0xFFFFFFFFu;
            lParams.muState15         = 0xFFFFFFFFu;
            lParams.muState16         = 0;
            lParams.mbDepthTestEnable = static_cast<u8>(luTestPass == 0);   // (v2 == 0)
            lParams.mbDepthWriteEnable= static_cast<u8>(luWritePass == 0);  // (i == 0)
            lParams.mu8Flag2          = 0;
            lParams.mu8Flag3          = 0;
            lParams.mu8Flag4          = 0;
            lParams.mu8Flag5          = 0;

            renderengine::ResourceDescriptor5 lDescriptor;
            DepthStencilState::GetResourceDescriptor(&lDescriptor, &lParams);

            void* lpMemory = mpHeapMalloc->Malloc(lDescriptor.maEntries[0].muSize,
                                                  lDescriptor.maEntries[0].muAlignment);
            DepthStencilState* lpState = reinterpret_cast<DepthStencilState*>(lpMemory);
            saDepthStencilStates[liStateIndex] = DepthStencilState::Initialize(&lpState, &lParams);
            ++liStateIndex;
        }
    }

    // The always-on default state: depth test + write both enabled.
    DepthStencilState::Parameters lDefault;
    lDefault.muFunction        = 3;
    lDefault.maState1[0]       = 0;
    lDefault.maState1[1]       = 0;
    lDefault.maState1[2]       = 0;
    lDefault.muState4          = DepthStencilState::E_FUNCTION_ALWAYS;  // 7
    lDefault.maState5[0]       = 0;
    lDefault.maState5[1]       = 0;
    lDefault.maState5[2]       = 0;
    lDefault.muState8          = DepthStencilState::E_FUNCTION_ALWAYS;  // 7
    lDefault.muState9          = 0;
    lDefault.muState10         = 0;
    lDefault.muStencilReadMask = 0xFFFFFFFFu;
    lDefault.muStencilWriteMask= 0xFFFFFFFFu;
    lDefault.muState13         = 0;
    lDefault.muState14         = 0xFFFFFFFFu;
    lDefault.muState15         = 0xFFFFFFFFu;
    lDefault.muState16         = 0;
    lDefault.mbDepthTestEnable = 1;
    lDefault.mbDepthWriteEnable= 1;
    lDefault.mu8Flag2          = 0;
    lDefault.mu8Flag3          = 0;
    lDefault.mu8Flag4          = 0;
    lDefault.mu8Flag5          = 0;

    renderengine::ResourceDescriptor5 lDefaultDescriptor;
    DepthStencilState::GetResourceDescriptor(&lDefaultDescriptor, &lDefault);

    void* lpDefaultMemory = mpHeapMalloc->Malloc(lDefaultDescriptor.maEntries[0].muSize,
                                                 lDefaultDescriptor.maEntries[0].muAlignment);
    DepthStencilState* lpDefaultState = reinterpret_cast<DepthStencilState*>(lpDefaultMemory);
    spDefaultDepthStencil = DepthStencilState::Initialize(&lpDefaultState, &lDefault);
}


// =================================================================================================
// Landed 2026-09-05 by the boost-exhaust wave: the frame's camera publish and the cVector draw
// adapter. Both sit between ParticleModule::BuildLionVertexBuffers and cParticleRender.
// =================================================================================================

// ------------------------------------------------------------------------------------------------
// LionParticleRender::SetCameraData  @ 0x82281068  (76 instructions)
//
// Publish the frame's camera to the Lion renderer. Its ONLY caller is
// ParticleModule::BuildLionVertexBuffers @0x8228AC20 (`bl` @0x8228B1B8 with r3 == module + 21104
// == &mLionRenderer), once per frame, immediately before cLionFX::Render.
//
// It is the FIVE-argument sibling of BrnGraphics::LionBlendRenderer::SetCameraData @0x822824F8 --
// same two halves, plus a fourth matrix:
//
//   1. mCameraTransform (this+0x20) is built SCALAR, three floats per row, with the fourth lane
//      FORCED: 0.0 on the three basis rows (flt_82001CC0, read out of the image as 00000000) and
//      1.0 on the translation row (flt_82001C98 == 3F800000). Twelve `lfs` from arBackMat and
//      sixteen `stfs` into +0x20..+0x5C. ⚠ The w lanes are NOT copied from arBackMat -- the
//      console overwrites them, which is the whole reason this scalar path exists beside the
//      vector copies below. cParticleRender::Render then reads its za row as the camera DIRECTION
//      and its wa row as the camera POSITION.
//   2. mBackMat / mViewMat / mViewProjection / mPackedFrustumLrtb are copied WHOLE -- four
//      lvx128/stvx128 pairs each into +0x60 / +0xA0 / +0xE0 / +0x120, w lanes included, verbatim.
//
// ⭐ THE FOURTH MATRIX IS THE ONE THAT MATTERS FOR CULLING and it is not a camera matrix at all:
// BuildLionVertexBuffers transposes CgsGraphics::Camera::GetFrustum's planes 2..5 (left / right /
// top / bottom) into SoA rows -- (Lx,Rx,Tx,Bx), (Ly,Ry,Ty,By), (Lz,Rz,Tz,Bz), (Ld,Rd,Td,Bd) --
// with six vperm and three vsldoi, and hands the result here. cParticleRender::Render tests all
// four planes in one pass against it. Copying it as if it were a transform would still compile
// and would cull the whole world.
// ------------------------------------------------------------------------------------------------
void LionParticleRender::SetCameraData(const rw::math::vpu::Matrix44Affine& arBackMat,
                                       const rw::math::vpu::Matrix44Affine& arViewMat,
                                       const rw::math::vpu::Matrix44& arViewProjection,
                                       const rw::math::vpu::Matrix44& arPackedFrustumLrtb)
{
    // --- half 1: the scalar convert into the Lion cMatrix (asm words 3-40) --------------------
    mCameraTransform.xa.x = arBackMat.xAxis.x;
    mCameraTransform.xa.y = arBackMat.xAxis.y;
    mCameraTransform.xa.z = arBackMat.xAxis.z;
    mCameraTransform.xa.w = 0.0f;                 // flt_82001CC0

    mCameraTransform.ya.x = arBackMat.yAxis.x;
    mCameraTransform.ya.y = arBackMat.yAxis.y;
    mCameraTransform.ya.z = arBackMat.yAxis.z;
    mCameraTransform.ya.w = 0.0f;

    mCameraTransform.za.x = arBackMat.zAxis.x;
    mCameraTransform.za.y = arBackMat.zAxis.y;
    mCameraTransform.za.z = arBackMat.zAxis.z;
    mCameraTransform.za.w = 0.0f;

    mCameraTransform.wa.x = arBackMat.wAxis.x;
    mCameraTransform.wa.y = arBackMat.wAxis.y;
    mCameraTransform.wa.z = arBackMat.wAxis.z;
    mCameraTransform.wa.w = 1.0f;                 // flt_82001C98

    // --- half 2: the four verbatim 64-byte copies (asm words 41-76) ---------------------------
    mBackMat           = arBackMat;
    mViewMat           = arViewMat;
    mViewProjection    = arViewProjection;
    mPackedFrustumLrtb = arPackedFrustumLrtb;
}

// ------------------------------------------------------------------------------------------------
// LionParticleRender::Render(..., const cVector*, ...)  @ sub_82289158  (143 instructions)
//   DWARF ParticleRender.h:8 / :170 declares it on iParticleRender AND on LionParticleRender; the
//   idb leaves it unnamed, which is why two waves recorded it as "the VECTOR-bucket draw".
//
// ⭐ IT IS AN ADAPTER, NOT A SECOND DRAW PATH. It expands the run's per-particle cVector POSITIONS
// into a stack array of full cMatrix -- identity rotation, the vector as the translation row --
// and tail-calls the cMatrix overload (`bl BrnParticle__LionParticleRender__Render` @0x8228938C,
// with the ninth argument re-pushed from its own frame at 0x82289380). So a vector bucket draws
// through exactly the same RenderSprites / RenderQuads / RenderTilts as a matrix bucket; the only
// difference is that its particles carry no orientation.
//
// Every lane is written explicitly, in the console's own order (the loop is unrolled x4 with a
// remainder loop; re-rolled here per the project's de-optimisation rule):
//     {  1, 0, 0, 0 }
//     {  0, 1, 0, 0 }
//     {  0, 0, 1, 0 }
//     { v.x, v.y, v.z, 1 }
// The two constants are flt_82001C98 (1.0) and flt_82001CC0 (0.0), read out of the image.
//
// ⚠ THE ARRAY IS 32 MATRICES ON THE STACK (2048 bytes at sp+0x60, frame 0x880) -- the same
// KU_SIMULATION_RUN cParticleRender::EmitterRender streams with, which is what bounds auCount.
// ------------------------------------------------------------------------------------------------
void LionParticleRender::Render(EffectsVertexBufferIterator& arIterator,
                                RenderedParticle* apParticle,
                                const cVector* apVectors,
                                U32 auCount,
                                U32 auFirstIndex,
                                const cParticleEmitter* apEmitter,
                                const cLionFog* apFog,
                                const cTime& arTime)
{
    cMatrix laMatrices[KU_VECTOR_DRAW_RUN];

    // [lionvec] ONE-SHOT bring-up witness. NOT console behaviour. The per-particle VECTOR this
    // adapter reads out of the bucket's side array, beside the particle's own local position --
    // the second half of the spawn/draw split. DELETE-WHEN-STABLE.
    {
        static bool sbVecOnce = false;
        if (!sbVecOnce && auCount != 0)
        {
            sbVecOnce = true;
            char lacMsg[224];
            std::snprintf(lacMsg, sizeof(lacMsg),
                "[lionvec] count=%u v0=(%.2f,%.2f,%.2f,%.2f) partPos=(%.2f,%.2f,%.2f)\n",
                (unsigned)auCount, apVectors[0].x, apVectors[0].y, apVectors[0].z, apVectors[0].w,
                apParticle[0].mPos.x, apParticle[0].mPos.y, apParticle[0].mPos.z);
            CgsDev::Log::WriteToLog(lacMsg);
        }
    }

    for (U32 luIndex = 0; luIndex < auCount; ++luIndex)
    {
        cMatrix& lrMatrix = laMatrices[luIndex];
        lrMatrix.xa.x = 1.0f; lrMatrix.xa.y = 0.0f; lrMatrix.xa.z = 0.0f; lrMatrix.xa.w = 0.0f;
        lrMatrix.ya.x = 0.0f; lrMatrix.ya.y = 1.0f; lrMatrix.ya.z = 0.0f; lrMatrix.ya.w = 0.0f;
        lrMatrix.za.x = 0.0f; lrMatrix.za.y = 0.0f; lrMatrix.za.z = 1.0f; lrMatrix.za.w = 0.0f;
        lrMatrix.wa.x = apVectors[luIndex].x;
        lrMatrix.wa.y = apVectors[luIndex].y;
        lrMatrix.wa.z = apVectors[luIndex].z;
        lrMatrix.wa.w = 1.0f;
    }

    Render(arIterator, apParticle, laMatrices, auCount, auFirstIndex, apEmitter, apFog, arTime);
}

}  // namespace BrnParticle
