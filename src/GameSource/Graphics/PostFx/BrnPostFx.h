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

class BrnRendererMemory;

// The DWARF names a MotionBlurState (BrnPostFx.h:220) and a BrnPostFxBloomData (BrnPostFx.h:249)
// member; neither type is modelled by any TU yet, so both are reserved as named padding below and
// the BrnPostFxBloomData accessor returns this opaque, forward-declared handle.
class BrnPostFxBloomData;

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

    void Render(const BrnRendererMemory& lAllocatedMemory,
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

private:
    void PrepareDownSampleBuffers(const BrnRendererMemory& lAllocatedMemory,
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

    // The DecFIGS DWARF carries a MotionBlurState member here; no TU models that type yet, so its
    // storage is reserved as named padding (the driver never reaches it by name in the reconstructed
    // methods). 0x20 bytes mirror the X360 gap between the B4Blur state and the render-target pointers.
    u8 mau8MotionBlurStatePad[0x20];             // BrnPostFx.h:220  (MotionBlurState mMotionBlurState)

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

    // BrnPostFxBloomData mBloomData (BrnPostFx.h:249) -- unmodelled DWARF type, reserved as named
    // padding. mBloom is then constructed at the X360 +604 (word) offset BrnPostFxBloom::Construct
    // writes to; the bloom-data block precedes it.
    u8                                  mau8BloomDataPad[0x30]; // BrnPostFx.h:249
    BrnPostFxBloom                      mBloom;                 // BrnPostFx.h:250
};

// The post-fx singleton (X360 dword the GetInstance accessor returns; BrnPostFx.h:246).
extern BrnPostFx msPostFx;

#endif // BRN_POST_FX_H
