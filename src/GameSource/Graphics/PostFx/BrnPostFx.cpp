#include "GameSource/Graphics/PostFx/BrnPostFx.h"
#include <cstddef>
#include <new>                                                                  // placement new (the carved effects)
#include "GameSource/Graphics/BrnRendererMemory.h"                              // the render-target pool
#include "SDKs/RenderEngineClub/MAIN/components/include/postfx/rwgpfxrendertarget.h"
#include "GameShared/GameClasses/Graphics/CgsRenderTarget.h"                     // CgsRenderTarget::GetRenderTarget
#include "GameShared/GameClasses/Graphics/CgsDepthStencilStateFactory.h"        // saDepthStencilStates[1] (Render)
#include "GameShared/GameClasses/Graphics/CgsRasterizerStateFactory.h"          // saRasterizerStates[2]   (Render)
#include "GameShared/GameClasses/Graphics/CgsBlendStateFactory.h"               // saBlendStates[1]        (Construct: B4Blur scatter)
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"           // shadow::Device::SetState
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxhelper.h"  // PfxHelper + gpPfxHelper
#include "GameShared/GameClasses/System/CgsHardwareInit.h"
#include "SDKs/EATech/eajobs/job_scheduler.h"
#include "GameShared/Jobs/TintBlend/TintBlend.h"                                // TintBlendEntry

BrnPostFx msPostFx;

// ARTIST 82407FC8.
BrnPostFx::BrnPostFx()
    : m_blendJob(0) // EA::Jobs::Job::Job(&m_blendJob, 0): the unnamed tint-blend job
{

    m_enabledFx = 0;

    m_vignetteState.m_gradientScalars.x = 0.0f;
    m_vignetteState.m_gradientScalars.y = 0.0f;
    m_vignetteState.m_gradientScalars.z = 0.0f;
    m_vignetteState.m_gradientScalars.w = 0.0f;
    m_vignetteState.m_innerColour.x = 1.0f;
    m_vignetteState.m_innerColour.y = 1.0f;
    m_vignetteState.m_innerColour.z = 1.0f;
    m_vignetteState.m_innerColour.w = 1.0f;
    m_vignetteState.m_outerColour.x = 1.0f;
    m_vignetteState.m_outerColour.y = 1.0f;
    m_vignetteState.m_outerColour.z = 1.0f;
    m_vignetteState.m_outerColour.w = 1.0f;
    m_vignetteState.m_centerScale.x = 0.5f;
    m_vignetteState.m_centerScale.y = 0.5f;
    m_vignetteState.m_centerScale.z = 1.0f;
    m_vignetteState.m_centerScale.w = 1.0f;

    m_dofState.m_focalPlanes[0] = 0.1f;
    m_dofState.m_focalPlanes[1] = 10.0f;
    m_dofState.m_focalPlanes[2] = 100.0f;
    m_dofState.m_focalPlanes[3] = 1000.0f;
    m_dofState.m_projNearPlane = 0.1f;
    m_dofState.m_projFarPlane = 1000.0f;
    m_dofState.m_dofAmount = 1.0f;
    m_dofState.m_blurRadius = 4.5f;

    m_deviceRt = 0;
    m_mainRt = 0;
    m_renderTarget = 0;
    m_frameRt = 0;
    m_bloomDsRt = 0;
    m_depthOfFieldRt = 0;

    m_pfxDof = 0;
    m_pfxVignette = 0;
    m_pfxTint[0] = 0;
    m_pfxTint[1] = 0;
    m_pfxB4Blur = 0;

    m_processTint = false;
    m_currentTintBuffer = 0;
}

namespace
{
    rw::Resource AllocatePostFxResource(rw::IResourceAllocator* lpAllocator,
                                        u32 lu32Size, u32 lu32Alignment)
    {
        rw::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = lu32Size;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = lu32Alignment;
        for (u32 luEntry = 1u; luEntry < rw::KU_RESOURCE_LANE_COUNT; ++luEntry)
        {
            lDescriptor.m_baseResourceDescriptors[luEntry].m_size      = 0u;
            lDescriptor.m_baseResourceDescriptors[luEntry].m_alignment = 1u;
        }
        return lpAllocator->DoAllocate(lDescriptor, 0);
    }

    const f32 KF_DOF_FOCAL_PLANE_0 = 0.1f;     // flt_82004014 (lfs @0x8240A04C, stfs @0x8240A054)
    const f32 KF_DOF_FOCAL_PLANE_1 = 10.0f;    // flt_82004A20 (lfs @0x8240A05C, stfs @0x8240A064)
    const f32 KF_DOF_FOCAL_PLANE_2 = 100.0f;   // flt_820049E0 (lfs @0x8240A068, stfs @0x8240A070)
    const f32 KF_DOF_FOCAL_PLANE_3 = 1000.0f;  // flt_82009E10 (lfs @0x8240A074, stfs @0x8240A078)
    const f32 KF_DOF_PROJ_NEAR     = 0.1f;     // flt_82004014 again (stfs @0x8240A058)
    const f32 KF_DOF_PROJ_FAR      = 1000.0f;  // flt_82009E10 again (stfs @0x8240A080)
    const f32 KF_DOF_AMOUNT        = 1.0f;     // flt_82001C98 (lfs @0x8240A084, stfs @0x8240A08C)
    const f32 KF_DOF_BLUR_RADIUS   = 4.5f;     // flt_820139F0 (lfs @0x8240A090, stfs @0x8240A094)

    const u32 KU_TINT_LOOKUP_SIZE = 32u;

    const s32 KI_TINT_BLEND_JOB_CODE_SIZE = 0;
}

// ARTIST 82409F80.
void BrnPostFx::Construct(rw::IResourceAllocator* lpAllocator)
{
    mnActiveLayerBitMask = 0u;
    mnActiveLayerCount   = 0u;
    m_enabledFx          = 0u;

    rw::graphics::postfx::PfxHelper::Parameters lHelperParameters;
    lHelperParameters.allocator = lpAllocator;
    {
        const rw::Resource lHelperResource = AllocatePostFxResource(
            lpAllocator,
            static_cast<u32>(sizeof(rw::graphics::postfx::PfxHelper)),   // X360: 0x50
            0x10u);
        if (lHelperResource.m_baseResources[0] != 0)
        {
            new (lHelperResource.m_baseResources[0])
                rw::graphics::postfx::PfxHelper(lHelperParameters);
        }
    }

    rw::graphics::postfx::DepthOfField::Parameters lDofParameters;
    lDofParameters.m_state.m_focalPlanes[0] = KF_DOF_FOCAL_PLANE_0;
    lDofParameters.m_state.m_focalPlanes[1] = KF_DOF_FOCAL_PLANE_1;
    lDofParameters.m_state.m_focalPlanes[2] = KF_DOF_FOCAL_PLANE_2;
    lDofParameters.m_state.m_focalPlanes[3] = KF_DOF_FOCAL_PLANE_3;
    lDofParameters.m_state.m_projNearPlane  = KF_DOF_PROJ_NEAR;
    lDofParameters.m_state.m_projFarPlane   = KF_DOF_PROJ_FAR;
    lDofParameters.m_state.m_dofAmount      = KF_DOF_AMOUNT;
    lDofParameters.m_state.m_blurRadius     = KF_DOF_BLUR_RADIUS;
    {
        const rw::Resource lDofResource = AllocatePostFxResource(
            lpAllocator,
            static_cast<u32>(sizeof(rw::graphics::postfx::DepthOfField)),   // X360: 0x30
            0x10u);
        m_pfxDof = (lDofResource.m_baseResources[0] != 0)
            ? new (lDofResource.m_baseResources[0])
                  rw::graphics::postfx::DepthOfField(lDofParameters)
            : 0;
    }

    rw::graphics::postfx::Vignette::Parameters lVignetteParameters;
    lVignetteParameters.m_allocator = lpAllocator;
    {
        const rw::Resource lVignetteResource = AllocatePostFxResource(
            lpAllocator,
            static_cast<u32>(sizeof(rw::graphics::postfx::Vignette)),   // X360: 0xD0
            0x10u);
        m_pfxVignette = (lVignetteResource.m_baseResources[0] != 0)
            ? new (lVignetteResource.m_baseResources[0])
                  rw::graphics::postfx::Vignette(lVignetteParameters)
            : 0;
    }

    rw::graphics::postfx::Tint::InitializePixelProgram(lpAllocator);

    rw::graphics::postfx::Tint::Parameters lTintParameters;
    lTintParameters.muSize      = KU_TINT_LOOKUP_SIZE;
    lTintParameters.mpAllocator = lpAllocator;
    for (u32 luTint = 0u; luTint < KU_POST_FX_NUM_TINT_BUFFERS; ++luTint)
    {
        const rw::Resource lTintResource = AllocatePostFxResource(
            lpAllocator,
            static_cast<u32>(sizeof(rw::graphics::postfx::Tint)),   // X360: 0x100
            0x80u);
        m_pfxTint[luTint] = rw::graphics::postfx::Tint::Initialize(lTintResource, lTintParameters);
    }

    m_blendJob.Clear();
    m_blendJob.GetEntryPoint().SetCode(static_cast<EA::Jobs::JobEnvironment>(0),
                                       reinterpret_cast<const void*>(&TintBlendEntry),
                                       KI_TINT_BLEND_JOB_CODE_SIZE);
    m_blendJob.GetEntryPoint().SetName("TintBlend");

    rw::graphics::postfx::B4Blur::Parameters lB4BlurParameters;
    lB4BlurParameters.m_allocator         = lpAllocator;
    lB4BlurParameters.m_scatterBlendState =
        CgsBlendStateFactory::GetState(E_FACTORY_BLEND_STATE_TRANSPARENT_MODULATE_NO_ALPHA_TEST_DEST_RGBA);
    lB4BlurParameters.m_state             = rw::graphics::postfx::B4Blur::State();
    {
        const rw::Resource lB4BlurResource = AllocatePostFxResource(
            lpAllocator,
            static_cast<u32>(sizeof(rw::graphics::postfx::B4Blur)),   // X360: 0x130
            0x10u);
        m_pfxB4Blur = (lB4BlurResource.m_baseResources[0] != 0)
            ? new (lB4BlurResource.m_baseResources[0])
                  rw::graphics::postfx::B4Blur(lB4BlurParameters)
            : 0;
    }

    mnActiveLayerCount = 1u;

    mMotionBlurState.Construct();

    mPostFxShader.Construct(lpAllocator);
    mpAllocator = lpAllocator;
    mBloom.Construct(lpAllocator);
}

// ARTIST 82408138.
void BrnPostFx::Destruct()
{
    for (u32 luTint = 0u; luTint < KU_POST_FX_NUM_TINT_BUFFERS; ++luTint)
    {
        if (m_pfxTint[luTint] != 0)
            m_pfxTint[luTint]->Release();
    }

    if (m_pfxDof != 0)
        m_pfxDof->Release();

    if (m_pfxVignette != 0)
        m_pfxVignette->Release();
    if (rw::graphics::postfx::gpPfxHelper != 0)
        rw::graphics::postfx::gpPfxHelper->Release();

    mPostFxShader.Destruct(mpAllocator);

    mBloom.Destruct();
}

// ARTIST 82408E88.
void BrnPostFx::PrepareDownSampleBuffers(BrnRendererMemory& lrAllocatedMemory,
                                         rw::graphics::postfx::RenderTarget* lpResultRt,
                                         f32 lfWhiteLevel)
{
    if ((m_enabledFx & E_FX_BLOOM) != 0u)
    {
        mBloom.Render(lrAllocatedMemory.GetBloomBuffer()->GetRenderTarget(),
                      lrAllocatedMemory.GetWorkBuffer()->GetRenderTarget(),
                      lpResultRt,
                      mBloomData.mfThreshold,
                      lfWhiteLevel);
    }

    if ((m_enabledFx & E_FX_DEPTH_OF_FIELD) != 0u)
    {
        m_pfxDof->DownSampleAndGaussianBlur(
            lrAllocatedMemory.GetDepthOfFieldBuffer()->GetRenderTarget(),
            lpResultRt,
            lrAllocatedMemory.GetWorkBuffer()->GetRenderTarget());
    }
}

namespace
{
    const f32 KF_POST_FX_HALF_TEXEL = 0.5f;

    const f32 KF_POST_FX_ONE = 1.0f;

    const f32 KF_POST_FX_ZERO = 0.0f;
}

// ARTIST 8240A468.
void BrnPostFx::Render(BrnRendererMemory& lrAllocatedMemory,
                       rw::graphics::postfx::RenderTarget* lpSourceRenderTarget,
                       rw::graphics::postfx::RenderTarget* lpDestinationRenderTarget,
                       rw::math::vpu::Vector4 lTintColour,
                       bool lbMotionBlurEnabled,
                       f32 lfBrightness, f32 lfContrast, f32 lfWhiteLevel, f32 lfAspectCorrection,
                       renderengine::Texture* lpOverrideSourceTexture)
{
    if (m_processTint)
    {
        m_processTint = false;
        m_blendJob.WaitOn();
        EndTintBlend();
    }

    if (mnActiveLayerCount == 0u)
    {
        return;
    }

    shadow::Device::SetState(
        CgsDepthStencilStateFactory::GetState(E_FACTORY_DEPTH_STENCIL_STATE_ZOFF_ZALL_ZWRITEOFF));
    shadow::Device::SetState(
        CgsRasterizerStateFactory::GetState(E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_NONE));

    PrepareDownSampleBuffers(lrAllocatedMemory, lpSourceRenderTarget, lfWhiteLevel);

    const bool lbOverrideSource = (lpOverrideSourceTexture != 0);
    renderengine::Texture* const lpSourceTexture =
        lbOverrideSource ? lpOverrideSourceTexture : lpSourceRenderTarget->GetTexture(0u);

    const f32 lfInverseWidth  = KF_POST_FX_ONE / static_cast<f32>(lpSourceRenderTarget->muWidth);
    const f32 lfInverseHeight = KF_POST_FX_ONE / static_cast<f32>(lpSourceRenderTarget->muHeight);

    rw::math::vpu::Vector2 lHalfTexelOffset;
    lHalfTexelOffset.x = KF_POST_FX_HALF_TEXEL * lfInverseWidth;
    lHalfTexelOffset.y = KF_POST_FX_HALF_TEXEL * lfInverseHeight;
    lHalfTexelOffset.z = lHalfTexelOffset.x;
    lHalfTexelOffset.w = lHalfTexelOffset.x;

    renderengine::Texture* const lpBloomTexture =
        lrAllocatedMemory.GetBloomBuffer()->GetRenderTarget()->GetTexture(0u);
    renderengine::Texture* const lpDepthOfFieldTexture =
        lrAllocatedMemory.GetDepthOfFieldBuffer()->GetRenderTarget()->GetTexture(0u);
    const renderengine::TextureState* const lpSourceTextureState =
        lrAllocatedMemory.GetDownSampleBuffer()->GetRenderTarget()->mDepthTarget.mpTextureState;
    const renderengine::TextureState* const lpTintTextureState =
        m_pfxTint[m_currentTintBuffer]->GetTextureState();

    const f32 lfCompositeWhiteLevel = lbOverrideSource ? KF_POST_FX_ONE : lfWhiteLevel;

    lpDestinationRenderTarget->Begin(0u);

    const bool lbEffectsAllowed = !lbOverrideSource;
    const bool lbDepthOfField = lbEffectsAllowed && ((m_enabledFx & E_FX_DEPTH_OF_FIELD) != 0u);
    const bool lbBloom        = lbEffectsAllowed && ((m_enabledFx & E_FX_BLOOM)          != 0u);
    const bool lbTint         = lbEffectsAllowed && ((m_enabledFx & E_FX_TINT)           != 0u);
    const bool lbVignette     = lbEffectsAllowed && ((m_enabledFx & E_FX_VIGNETTE)       != 0u);
    const bool lbMotionBlur   = lbEffectsAllowed && lbMotionBlurEnabled;

    mPostFxShader.Render(lfCompositeWhiteLevel,
                         mBloomData,
                         m_vignetteState,
                         m_dofState,
                         mMotionBlurState,
                         lTintColour,
                         lfBrightness,
                         lfContrast,
                         lbDepthOfField,
                         lbBloom,
                         lbTint,
                         lbVignette,
                         lbMotionBlur,
                         lpSourceTexture,
                         lpBloomTexture,
                         lpDepthOfFieldTexture,
                         lpTintTextureState,
                         lpSourceTextureState,
                         lHalfTexelOffset,
                         lfAspectCorrection,
                         lbOverrideSource);

    lpDestinationRenderTarget->Resolve(true, true);
}

// ARTIST 823F8380: fill the fixed-up source pointers, then submit the native job.
void BrnPostFx::BeginTintBlend()
{
    CGS_STATIC_ASSERT(offsetof(BrnPostFx, mnActiveLayerCount) == 0x380, "ARTIST postfx active count");
    CGS_STATIC_ASSERT(offsetof(BrnPostFx, m_pfxTint) == 0x5C0, "ARTIST postfx tints");
    CGS_STATIC_ASSERT(offsetof(BrnPostFx, m_colourCubes) == 0x5CC, "ARTIST postfx cubes");
    CGS_STATIC_ASSERT(offsetof(BrnPostFx, m_tintFactors) == 0x5E0, "ARTIST postfx factors");
    CGS_STATIC_ASSERT(offsetof(BrnPostFx, m_processTint) == 0x5F4, "ARTIST postfx pending byte");
    CGS_STATIC_ASSERT(offsetof(BrnPostFx, m_blendJob) == 0x600, "ARTIST postfx blend job");
    CGS_STATIC_ASSERT(offsetof(rw::graphics::postfx::RenderTarget, mDepthTarget) +
        offsetof(rw::graphics::postfx::Target, mpTextureState) == 0x8C, "ARTIST postfx depth state");
    if (mnActiveLayerCount == 0u)
    {
        return;
    }
    if ((m_enabledFx & static_cast<u32>(E_FX_TINT)) == 0u)
    {
        return;
    }

    m_currentTintBuffer = (m_currentTintBuffer - 1u) & 1u;

    rw::graphics::postfx::TintBlendParameters& lrBlendParameters =
        m_pfxTint[m_currentTintBuffer]->BeginBlendJob();

    lrBlendParameters.numSources = m_numCubesToBlend;
    for (u32 luSource = 0u; luSource < m_numCubesToBlend; ++luSource)
    {
        lrBlendParameters.src[luSource]    = m_colourCubes[luSource]->GetPixels();
        lrBlendParameters.factor[luSource] = m_tintFactors[luSource];
    }

    CGS_ASSERT(m_numCubesToBlend <= 5u, "m_numCubesToBlend <= 5");           // BrnPostFx.cpp:266

    lrBlendParameters.factor[5] = KF_POST_FX_ZERO;   // stfs flt_82001CC0, 0x40(r29)

    m_blendJob.SetData(&lrBlendParameters,
                       sizeof(rw::graphics::postfx::TintBlendParameters));   // X360: 128

    CgsSystem::JobManager()->AddJobs(&m_blendJob, 1);

    m_processTint = true;
}

void BrnPostFx::EndTintBlend()
{
    m_pfxTint[m_currentTintBuffer]->EndBlendJob();
}

void BrnPostFx::SetColourCube(int liIndex, rw::graphics::postfx::ColourCube* lpColourCube)
{
    if (lpColourCube != 0)
    {
        m_colourCubes[liIndex] = lpColourCube;
    }
}

void BrnPostFx::SetTintBlendFactor(int liIndex, f32 lfFactor)
{
    m_tintFactors[liIndex] = lfFactor;
}

void BrnPostFx::SetTintBlendNumber(const int& lriNumber)
{
    m_numCubesToBlend = static_cast<u32>(lriNumber);
}

void BrnPostFx::SetDepthOfField(const bool& lrbEnabled)
{
    if (lrbEnabled) { m_enabledFx |=  static_cast<u32>(E_FX_DEPTH_OF_FIELD); }
    else            { m_enabledFx &= ~static_cast<u32>(E_FX_DEPTH_OF_FIELD); }
}

void BrnPostFx::SetBloom(const bool& lrbEnabled)
{
    if (lrbEnabled) { m_enabledFx |=  static_cast<u32>(E_FX_BLOOM); }
    else            { m_enabledFx &= ~static_cast<u32>(E_FX_BLOOM); }
}

void BrnPostFx::SetVignette(const bool& lrbEnabled)
{
    if (lrbEnabled) { m_enabledFx |=  static_cast<u32>(E_FX_VIGNETTE); }
    else            { m_enabledFx &= ~static_cast<u32>(E_FX_VIGNETTE); }
}

void BrnPostFx::SetTint(const bool& lrbEnabled)
{
    if (lrbEnabled) { m_enabledFx |=  static_cast<u32>(E_FX_TINT); }
    else            { m_enabledFx &= ~static_cast<u32>(E_FX_TINT); }
}

void BrnPostFx::SetB4Blur(const bool& lrbEnabled)
{
    if (lrbEnabled) { m_enabledFx |=  static_cast<u32>(E_FX_B4BLUR); }
    else            { m_enabledFx &= ~static_cast<u32>(E_FX_B4BLUR); }
}

rw::graphics::postfx::Vignette*     BrnPostFx::GetVignette()     { return m_pfxVignette; }
BrnPostFxBloomData*                 BrnPostFx::GetBloom()        { return &mBloomData; }
rw::graphics::postfx::DepthOfField* BrnPostFx::GetDepthOfField() { return m_pfxDof; }
rw::graphics::postfx::B4Blur*       BrnPostFx::GetB4Blur()       { return m_pfxB4Blur; }

rw::graphics::postfx::Vignette::State*     BrnPostFx::GetVignetteState() { return &m_vignetteState; }
rw::graphics::postfx::DepthOfField::State* BrnPostFx::GetDofState()      { return &m_dofState; }
rw::graphics::postfx::B4Blur::State*       BrnPostFx::GetB4BlurState()   { return &m_b4blurState; }
MotionBlurState&                           BrnPostFx::GetMotionBlurState() { return mMotionBlurState; }
