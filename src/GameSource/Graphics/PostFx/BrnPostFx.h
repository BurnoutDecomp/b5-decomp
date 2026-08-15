#ifndef BRN_POST_FX_H
#define BRN_POST_FX_H

#include "types.hpp"
#include "rw/rwcore_structs.h"                                                  // rw::IResourceAllocator
#include "rw/math/vpu/types.h"                                                  // rw::math::vpu::Vector4 / Vector2
#include "pc/gcm/renderengine/texture.h"                                        // renderengine::Texture
#include "SDKs/EATech/eajobs/job.h"                                             // EA::Jobs::Job
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxvignette.h"     // Vignette + Vignette::State
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxdof.h"          // DepthOfField + State
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxb4blur.h"       // B4Blur::State
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxtint.h"         // Tint
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxcolourcube.h"   // ColourCube
#include "SDKs/RenderEngineClub/MAIN/components/include/postfx/rwgpfxrendertarget.h" // RenderTarget
#include "GameSource/Graphics/PostFx/BrnPostFxShader.h"                         // BrnPostFxShader
#include "GameSource/Graphics/PostFx/BrnPostFxBloom.h"                          // BrnPostFxBloom
#include "GameSource/Graphics/PostFx/BrnPostFxBloomData.h"                      // BrnPostFxBloomData

// BrnPostFx -- the Burnout post-effects driver. It owns every render-engine post-fx effect
// (depth-of-field, vignette, a pair of colour-tint effects, the Burnout-4 blur, and bloom), the
// render targets they draw through, and the per-frame state blocks that feed each effect's shader
// constants. Render() schedules the bloom/blur/tint post passes over the render-engine textures and
// composites them, while BeginTintBlend()/EndTintBlend() drive the colour-cube tint blend as an
// EA::Jobs job.
//
// SHAPE authoritative from the DecFIGS DWARF (GameSource/Graphics/PostFx/BrnPostFx.h): member names,
// order and logical types. The X360 ARTIST asm (BrnPostFx ctor @0x82407FC8, Construct @0x82409F80)
// confirms the construction-time member values reconstructed below. As with the committed post-fx
// siblings, members are reached BY NAME (no raw-offset reads) and the layout is shape-faithful, not
// X360-byte-exact (the 4-byte-pointer guest object widens on the LLP64 host).

// `struct`, not `class`: BrnRendererMemory.h:47 defines it as a struct, and the mismatch is a C4099
// the moment both are visible in one TU -- which BrnPostFx.cpp now is, because PrepareDownSampleBuffers
// and Render reach the pool through its accessors.
struct BrnRendererMemory;

// Both of the types this header used to reserve as padding are now modelled: BrnPostFxBloomData in
// its own header (included above) and MotionBlurState in BrnPostFxShader.h, where the DWARF puts it
// (BrnPostFxShader.h:41). The opaque forward declaration that stood in for the first is gone -- it
// declared a `class` where the real type is a `struct`, which is a C4099 the moment both are visible.

class BrnPostFx
{
public:
    // The post-fx effect bits packed into m_enabledFx. The X360 ctor clears the word; the Render /
    // BeginTintBlend asm tests bit 0x20 (tint) and the down-sample path tests bits 0x01 (DoF) and
    // 0x02 (bloom). The remaining named lanes mirror the Set* mutators' bit positions.
    enum EnabledFx
    {
        E_FX_DEPTH_OF_FIELD = 0x01,
        E_FX_BLOOM          = 0x02,
        E_FX_VIGNETTE       = 0x10,
        E_FX_TINT           = 0x20,
        E_FX_B4BLUR         = 0x40,
    };

    BrnPostFx();

    static BrnPostFx& GetInstance();

    void SetFrameTarget(renderengine::RenderTargetState* lpFrameRenderTargetState);
    void Construct(rw::IResourceAllocator* lpAllocator);
    void Destruct();
    void Clear();

    void Render(BrnRendererMemory& lAllocatedMemory,
                rw::graphics::postfx::RenderTarget* lpSourceRenderTarget,
                rw::graphics::postfx::RenderTarget* lpDestinationRenderTarget,
                rw::math::vpu::Vector4 lTintColour,
                bool lbMotionBlurEnabled,
                f32 lfBrightness, f32 lfContrast, f32 lfWhiteLevel, f32 lfAspectCorrection,
                renderengine::Texture* lpOverrideSourceTexture);

    void BeginTintBlend();
    void EndTintBlend();

    void SetDepthOfField(const bool& lrbEnabled);
    void SetBloom(const bool& lrbEnabled);
    void SetVignette(const bool& lrbEnabled);
    void SetTint(const bool& lrbEnabled);
    void SetB4Blur(const bool& lrbEnabled);
    void SetTintBlendNumber(const int& lriNumber);
    void SetTintBlendFactor(int liIndex, f32 lfFactor);
    void SetColourCube(int liIndex, rw::graphics::postfx::ColourCube* lpColourCube);

    bool IsDepthOfField() const;
    bool IsBloom() const;
    bool IsVignette() const;
    int  IsTint() const;
    bool IsB4Blur() const;

    rw::graphics::postfx::Vignette*     GetVignette();
    BrnPostFxBloomData*                 GetBloom();
    rw::graphics::postfx::DepthOfField* GetDepthOfField();
    rw::graphics::postfx::Tint*         GetTint();
    rw::graphics::postfx::B4Blur*       GetB4Blur();

    rw::graphics::postfx::Vignette::State*     GetVignetteState();
    rw::graphics::postfx::DepthOfField::State* GetDofState();
    rw::graphics::postfx::B4Blur::State*       GetB4BlurState();
    MotionBlurState&                           GetMotionBlurState();   // BrnPostFx.h:200

private:
    void PrepareDownSampleBuffers(BrnRendererMemory& lAllocatedMemory,
                                  rw::graphics::postfx::RenderTarget* lpResultRt,
                                  f32 lfWhiteLevel);

    // --- construction-time constants (X360 ctor / Construct asm immediates) -----------------------
    static const u32 KU_POST_FX_MAX_LAYER_COUNT = 16;       // BrnPostFx.h:207
    static const u32 KU_POST_FX_BUFFER_SIZE     = 4194304;  // BrnPostFx.h:208
    static const u32 KU_POST_FX_BUFFER_ALIGNMENT = 128;     // BrnPostFx.h:209
    static const u32 KU_POST_FX_NUM_TINT_BUFFERS = 2;       // BrnPostFx.h:210

    BrnPostFxShader        mPostFxShader;        // BrnPostFx.h:204
    rw::IResourceAllocator* mpAllocator;         // BrnPostFx.h:205

    u32 mnActiveLayerBitMask;                    // BrnPostFx.h:212
    u32 mnActiveLayerCount;                      // BrnPostFx.h:213
    u32 m_enabledFx;                             // BrnPostFx.h:214

    rw::graphics::postfx::Vignette::State     m_vignetteState; // BrnPostFx.h:216
    rw::graphics::postfx::DepthOfField::State m_dofState;      // BrnPostFx.h:217
    rw::graphics::postfx::B4Blur::State       m_b4blurState;   // BrnPostFx.h:218

    // The motion-blur reprojection state (DWARF BrnPostFx.h:220; the type is declared at
    // BrnPostFxShader.h:41). BrnPostFx::Construct seeds it -- both matrices to the identity and
    // meQuality to E_QUALITY_EXPENSIVE -- and BrnPostFx::Render hands it to BrnPostFxShader::Render
    // by reference, which builds the camera-motion reprojection matrix out of the two WVPs and
    // asserts on meQuality (BrnPostFxShader.cpp:798). Guest size 0x84, not the 0x20 the old
    // placeholder reserved; reached by name, so the host size is the compiler's business.
    MotionBlurState mMotionBlurState;            // BrnPostFx.h:220

    rw::graphics::postfx::RenderTarget* m_deviceRt;       // BrnPostFx.h:223
    rw::graphics::postfx::RenderTarget* m_mainRt;         // BrnPostFx.h:224
    rw::graphics::postfx::RenderTarget* m_renderTarget;   // BrnPostFx.h:225
    rw::graphics::postfx::RenderTarget* m_frameRt;        // BrnPostFx.h:227
    rw::graphics::postfx::RenderTarget* m_bloomDsRt;      // BrnPostFx.h:230
    rw::graphics::postfx::RenderTarget* m_depthOfFieldRt; // BrnPostFx.h:231

    rw::graphics::postfx::DepthOfField* m_pfxDof;         // BrnPostFx.h:233
    rw::graphics::postfx::Vignette*     m_pfxVignette;    // BrnPostFx.h:234
    rw::graphics::postfx::Tint*         m_pfxTint[2];     // BrnPostFx.h:235
    rw::graphics::postfx::B4Blur*       m_pfxB4Blur;      // BrnPostFx.h:236

    rw::graphics::postfx::ColourCube*   m_colourCubes[5]; // BrnPostFx.h:238
    f32                                 m_tintFactors[5]; // BrnPostFx.h:239
    bool                                m_processTint;    // BrnPostFx.h:240
    u32                                 m_numCubesToBlend;   // BrnPostFx.h:241
    u32                                 m_currentTintBuffer; // BrnPostFx.h:242

    EA::Jobs::Job                       m_blendJob;       // BrnPostFx.h:244

    // The bloom CONSTANT block (DWARF BrnPostFx.h:249). Written every frame by
    // BrnRendererModule::Render, read by BrnPostFx::PrepareDownSampleBuffers (mfThreshold) and
    // handed to BrnPostFxShader::Render by reference (mColour + mfLuminance become BloomColour).
    // Three members, guest size 0x18 -- see BrnPostFxBloomData.h for the three-site attestation.
    BrnPostFxBloomData                  mBloomData;             // BrnPostFx.h:249
    BrnPostFxBloom                      mBloom;                 // BrnPostFx.h:250
};

// The post-fx singleton (X360 dword the GetInstance accessor returns; BrnPostFx.h:246).
extern BrnPostFx msPostFx;

#endif // BRN_POST_FX_H
