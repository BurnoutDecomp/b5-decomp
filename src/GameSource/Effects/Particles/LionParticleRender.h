#pragma once

// ============================================================================
// GameSource/Effects/Particles/LionParticleRender.h
//
// BrnParticle::LionParticleRender -- Burnout's concrete iParticleRender implementation
// over the Lion (eauk_lion) particle runtime. It owns the camera matrices, the
// texture-name map, and a BrnGraphics::LionBlendRenderer that does the actual immediate-
// mode geometry emission, and it maintains the process-wide texture / material / depth-
// stencil tables used while a particle group is rendered.
//
// SOURCE-OF-TRUTH:
//  * Declaration shape (member set, method set, base class) is from the DecFIGS DWARF
//    (LionParticleRender.h:81), gated on the X360 ARTIST ledger: only the methods the
//    X360 build attests for THIS class are declared (the PS3-only DWARF surface --
//    the cVector Render overload, Begin/End/SetCameraData/SetRenderer/SetAllocator/
//    SetTextureNameMap/IsVisible/GetPackedFrustumLrtb/AcquireTexture(2-arg) -- is left
//    out, except where a body for it appears in the X360 asm dossier for this TU).
//  * Member OFFSETS are taken from the X360 ARTIST asm for the bodies in this TU:
//      mpHeapMalloc      @+0x08  (Setup: Malloc(mpHeapMalloc,...))
//      mTextureNameMap   @+0x0C  (FindTexture: walks mTextureNameMap entries)
//      mCameraTransform  @+0x20  (GetCameraMatrix copies the 64-byte matrix)
//      mBackMat          @+0x60  / mViewMat @+0xA0  (RenderGroupBeginLite -> SetCameraData)
//      mViewProjection   @+0xE0  (BeginRendering / SetCameraData)
//      mPackedFrustumLrtb@+0x120
//      mpRenderer        @+0x160 / mpCurrentRenderer @+0x164 (ctor zeroes both)
//    The DWARF member ORDER puts mTexturePathCount before mpHeapMalloc; the X360 build
//    places mpHeapMalloc at +0x08 and the texture-path bookkeeping fields are not touched
//    by any body in this TU, so they are modelled as named members in the X360-attested
//    slots with explicit alignment padding. X360 pointers are 32-bit; on the host they
//    widen, so absolute offsets are NOT a host layout fact -- members are accessed BY NAME.
// ============================================================================

#include "types.hpp"
#include "rw/math/vpu/types.h"   // rw::math::vpu::Matrix44 / Matrix44Affine
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleRender/ParticleRender.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleMaterial.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"   // CgsResource::SafeResourceHandle
#include "SharedClasses/Graphics/TextureNameMapResourceType.h"          // BrnParticle::TextureNameMap

// cMatrix is the engine 4x4 matrix (typedef of rw::math::vpu::Matrix44) -- supplied by
// ParticleRender.h (included above). GetCameraMatrix returns it by value.

namespace CgsMemory { class HeapMalloc; }
namespace renderengine { class Texture; class TextureState; }

namespace BrnGraphics { class LionBlendRenderer; }

namespace BrnParticle
{
    class LionParticleRender : public iParticleRender
    {
    public:
        LionParticleRender();
        virtual ~LionParticleRender() {}

        // --- iParticleRender overrides (X360-attested bodies in this TU) ----------------
        virtual void Render(EffectsVertexBufferIterator& arIterator,
                            RenderedParticle* apParticle,
                            const cMatrix* apMatrix,
                            U32 auCount,
                            U32 auFlags,
                            const cParticleEmitter* apEmitter,
                            const cLionFog* apFog,
                            const cTime& arTime);

        virtual void TextureRegister(cParticleMaterial* apMaterial, char* apcTextureName);
        virtual void TextureUnRegister(cParticleMaterial* apMaterial);

        virtual void RenderGroupBeginLite(const cParticleMaterial& arMaterial);
        virtual void RenderGroupEndLite();

        virtual void BeginRendering(float32_t afNear, bool8_t abFogEnable,
                                    float32_t afFogNear, float32_t afFogFar,
                                    float32_t afD, float32_t afE, float32_t afF,
                                    renderengine::TextureState* apTextureState);
        virtual void EndRendering();

        virtual void RenderGroupBegin(const cParticleMaterial& arMaterial);
        virtual void RenderGroupEnd();

        virtual U32 GetVertexStride(const cParticleMaterial& arMaterial);

        // --- Non-virtual surface attested in the X360 asm dossier for this TU -----------
        cMatrix GetCameraMatrix();

        // Build the process-wide depth-stencil state table (called by ParticleModule::Prepare).
        void Setup();

        // Acquire / register the textures into the process-wide texture table. AcquireTexture
        // takes the texture-map hash plus the resolved texture handle (X360 0x82280B60).
        void AcquireTexture(U32 auTextureMapHandle,
                            CgsResource::SafeResourceHandle<renderengine::Texture> aTexture);

    private:
        // Find the acquired texture for a texture-map hash via the name map (X360 0x822895D8).
        renderengine::Texture* FindTexture(U32 auTextureMapHandle) const;

        // Bind the per-material render state (texture + depth-stencil + blend) on the
        // current renderer (X360 0x822896B8).
        void SetMaterial(const cParticleMaterial* apMaterial);

        // FNV-1a hash of a material's render-state bytes (X360 0x82279CC0).
        U32 HashMaterial(const cParticleMaterial* apMaterial);

        // Look the material hash up in the process-wide internal-material table; 0 if absent
        // (X360 0x82279D50).
        U32 GetMaterialHandle(U32 auMaterialHash);

        // Create a new internal material entry for a material (callee; own TU / trap stub).
        U32 CreateInternalMaterial(const cParticleMaterial* apMaterial);

        // ------------------------------------------------------------------------------
        // Layout (offsets are X360-attested where a body in this TU dereferences them;
        // unreferenced texture-path bookkeeping is named but its slot is inferred).
        // ------------------------------------------------------------------------------
        char**   mppTexturePaths;     // +0x04  (registered texture-path strings; unused here)
        CgsMemory::HeapMalloc* mpHeapMalloc;                                  // +0x08
        CgsResource::SafeResourceHandle<BrnParticle::TextureNameMap> mTextureNameMap; // +0x0C (8B)
        U32      mTexturePathCount;   // +0x14  (count of mppTexturePaths; unused here)
        u8       mPad18[0x20 - 0x18]; // align mCameraTransform to +0x20 (16-aligned matrix)

        cMatrix                          mCameraTransform;   // +0x20
        rw::math::vpu::Matrix44Affine    mBackMat;           // +0x60
        rw::math::vpu::Matrix44Affine    mViewMat;           // +0xA0
        rw::math::vpu::Matrix44          mViewProjection;    // +0xE0
        rw::math::vpu::Matrix44          mPackedFrustumLrtb; // +0x120

        BrnGraphics::LionBlendRenderer*  mpRenderer;         // +0x160
        BrnGraphics::LionBlendRenderer*  mpCurrentRenderer;  // +0x164
    };
}
