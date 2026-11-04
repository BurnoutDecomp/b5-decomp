#include "GameSource/Graphics/BrnCoronaManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Graphics/CgsBlendStateFactory.h"
#include "GameShared/GameClasses/Graphics/CgsDepthStencilStateFactory.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/texturestate.h"
#include "SDKs/RenderEngineClub/MAIN/components/include/coronas/rwgcoronarenderer.h"
#include <vectorintrinsics.h>
#include <cstring>

// ARTIST82FAFC10 is zero in the image; CRT82C4EDD8 constructs48 native vectors.
Vector2 BrnCoronaManager::s_atlasUVs[12][4] =
{
    { Vector2(0.0f, 0.0f), Vector2(0.33333334f, 0.0f), Vector2(0.33333334f, 0.33333334f), Vector2(0.0f, 0.33333334f) },
    { Vector2(0.33333334f, 0.0f), Vector2(0.66666669f, 0.0f), Vector2(0.66666669f, 0.33333334f), Vector2(0.33333334f, 0.33333334f) },
    { Vector2(0.66666669f, 0.0f), Vector2(1.0f, 0.0f), Vector2(1.0f, 0.33333334f), Vector2(0.66666669f, 0.33333334f) },
    { Vector2(0.0f, 0.33333334f), Vector2(0.33333334f, 0.33333334f), Vector2(0.33333334f, 0.66666669f), Vector2(0.0f, 0.66666669f) },
    { Vector2(0.33333334f, 0.33333334f), Vector2(0.66666669f, 0.33333334f), Vector2(0.66666669f, 0.66666669f), Vector2(0.33333334f, 0.66666669f) },
    { Vector2(0.66666669f, 0.33333334f), Vector2(1.0f, 0.33333334f), Vector2(1.0f, 0.66666669f), Vector2(0.66666669f, 0.66666669f) },
    { Vector2(0.0f, 0.66666669f), Vector2(0.33333334f, 0.66666669f), Vector2(0.33333334f, 1.0f), Vector2(0.0f, 1.0f) },
    { Vector2(0.33333334f, 0.66666669f), Vector2(0.66666669f, 0.66666669f), Vector2(0.66666669f, 1.0f), Vector2(0.33333334f, 1.0f) },
    { Vector2(0.66666669f, 0.66666669f), Vector2(0.83333331f, 0.66666669f), Vector2(0.83333331f, 0.83333331f), Vector2(0.66666669f, 0.83333331f) },
    { Vector2(0.83333331f, 0.66666669f), Vector2(1.0f, 0.66666669f), Vector2(1.0f, 0.83333331f), Vector2(0.83333331f, 0.83333331f) },
    { Vector2(0.66666669f, 0.83333331f), Vector2(0.83333331f, 0.83333331f), Vector2(0.83333331f, 1.0f), Vector2(0.66666669f, 1.0f) },
    { Vector2(0.83333331f, 0.83333331f), Vector2(1.0f, 0.83333331f), Vector2(1.0f, 1.0f), Vector2(0.83333331f, 1.0f) }
};

namespace
{
const char gszSourceFile[] = "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Graphics/BrnCoronaManager.cpp";
f32 kfCoronaFadeDistance = 48400.0f; // ARTIST82F242CC, squared distance.
const f32 kfNoFadeDistanceSqrd = 62500.0f;
rw::RGBA gPropCoronaColour(255, 255, 255, 255); // ARTIST82F242D0.
// ARTIST82F242D4/E8/FC; DWARF BrnCoronaManager.cpp:60/64/68.
const BrnEffects::Curves::SmoothStep kVehicleCoronaScale = BrnEffects::Curves::SmoothStep(rw::math::fpu::Vector3Template<f32>(400.0f, 1400.0f, 22500.0f), rw::math::fpu::Vector2Template<f32>(0.75f, 2.25f));
const BrnEffects::Curves::SmoothStep kTrafficLightScale = BrnEffects::Curves::SmoothStep(rw::math::fpu::Vector3Template<f32>(400.0f, 10000.0f, 22500.0f), rw::math::fpu::Vector2Template<f32>(0.75f, 1.0f));
const BrnEffects::Curves::SmoothStep kPropCoronaScale = BrnEffects::Curves::SmoothStep(rw::math::fpu::Vector3Template<f32>(400.0f, 1400.0f, 14500.0f), rw::math::fpu::Vector2Template<f32>(0.75f, 2.25f));
}

// ARTIST82F24310 and CRT82C4F418:25 records, literal or genuine named curves.
BrnCoronaTypeParams BrnCoronaTypeParams::mParams[eCoronaTypeCount] =
{
    { Vector2(0.699999988f, 0.699999988f), 2, 0.5f,
      BrnEffects::Curves::SmoothStep(rw::math::fpu::Vector3Template<f32>(400.0f, 4800.0f, 22500.0f), rw::math::fpu::Vector2Template<f32>(0.349999994f, 1.39999998f)) },
    { Vector2(0.400000006f, 0.400000006f), 1, 0.5f,
      BrnEffects::Curves::SmoothStep(rw::math::fpu::Vector3Template<f32>(400.0f, 3600.0f, 22500.0f), rw::math::fpu::Vector2Template<f32>(0.600000024f, 3.0f)) },
    { Vector2(0.349999994f, 0.349999994f), 7, 0.5f,
      BrnEffects::Curves::SmoothStep(rw::math::fpu::Vector3Template<f32>(400.0f, 1600.0f, 22500.0f), rw::math::fpu::Vector2Template<f32>(0.699999988f, 3.0f)) },
    { Vector2(0.25f, 0.25f), 10, 0.5f,
      BrnEffects::Curves::SmoothStep(rw::math::fpu::Vector3Template<f32>(400.0f, 1600.0f, 22500.0f), rw::math::fpu::Vector2Template<f32>(0.75f, 2.5f)) },
    { Vector2(0.324999988f, 0.324999988f), 0, 0.5f,
      kVehicleCoronaScale },
    { Vector2(0.899999976f, 1.29999995f), 5, 0.349999994f,
      kTrafficLightScale },
    { Vector2(0.899999976f, 0.899999976f), 4, 0.349999994f,
      kTrafficLightScale },
    { Vector2(0.899999976f, 0.899999976f), 3, 0.349999994f,
      kTrafficLightScale },
    { Vector2(0.656000018f, 0.656000018f), 0, 0.5f,
      BrnEffects::Curves::SmoothStep(rw::math::fpu::Vector3Template<f32>(400.0f, 6400.0f, 22500.0f), rw::math::fpu::Vector2Template<f32>(0.300000012f, 2.25f)) },
    { Vector2(0.550000012f, 0.550000012f), 1, 0.5f,
      BrnEffects::Curves::SmoothStep(rw::math::fpu::Vector3Template<f32>(400.0f, 1600.0f, 12000.0f), rw::math::fpu::Vector2Template<f32>(0.699999988f, 3.0f)) },
    { Vector2(0.349999994f, 0.349999994f), 7, 0.5f,
      kVehicleCoronaScale },
    { Vector2(0.349999994f, 0.349999994f), 10, 0.5f,
      kVehicleCoronaScale },
    { Vector2(0.125f, 0.125f), 7, 0.5f,
      kVehicleCoronaScale },
    { Vector2(0.5f, 0.5f), 3, 0.5f,
      BrnEffects::Curves::SmoothStep(rw::math::fpu::Vector3Template<f32>(400.0f, 3600.0f, 22500.0f), rw::math::fpu::Vector2Template<f32>(0.400000006f, 2.25f)) },
    { Vector2(0.5f, 0.5f), 0, 0.5f,
      BrnEffects::Curves::SmoothStep(rw::math::fpu::Vector3Template<f32>(400.0f, 3600.0f, 22500.0f), rw::math::fpu::Vector2Template<f32>(0.400000006f, 2.25f)) },
    { Vector2(0.324999988f, 0.324999988f), 3, 0.5f,
      BrnEffects::Curves::SmoothStep(rw::math::fpu::Vector3Template<f32>(400.0f, 3600.0f, 22500.0f), rw::math::fpu::Vector2Template<f32>(0.449999988f, 2.25f)) },
    { Vector2(0.524999976f, 0.524999976f), 0, 0.5f,
      BrnEffects::Curves::SmoothStep(rw::math::fpu::Vector3Template<f32>(400.0f, 1400.0f, 22500.0f), rw::math::fpu::Vector2Template<f32>(0.300000012f, 2.25f)) },
    { Vector2(0.300000012f, 0.300000012f), 1, 0.5f,
      kVehicleCoronaScale },
    { Vector2(0.300000012f, 0.300000012f), 7, 0.5f,
      kVehicleCoronaScale },
    { Vector2(0.150000006f, 0.150000006f), 10, 0.5f,
      kVehicleCoronaScale },
    { Vector2(0.125f, 0.125f), 2, 0.5f,
      kVehicleCoronaScale },
    { Vector2(0.5f, 0.5f), 3, 0.5f,
      BrnEffects::Curves::SmoothStep(rw::math::fpu::Vector3Template<f32>(400.0f, 3600.0f, 22500.0f), rw::math::fpu::Vector2Template<f32>(0.400000006f, 2.25f)) },
    { Vector2(0.5f, 0.5f), 0, 0.5f,
      BrnEffects::Curves::SmoothStep(rw::math::fpu::Vector3Template<f32>(400.0f, 3600.0f, 22500.0f), rw::math::fpu::Vector2Template<f32>(0.400000006f, 2.25f)) },
    { Vector2(0.324999988f, 0.324999988f), 0, 0.5f,
      BrnEffects::Curves::SmoothStep(rw::math::fpu::Vector3Template<f32>(400.0f, 1400.0f, 22500.0f), rw::math::fpu::Vector2Template<f32>(0.324999988f, 2.25f)) },
    { Vector2(0.174999997f, 0.474999994f), 11, 0.5f,
      BrnEffects::Curves::SmoothStep(rw::math::fpu::Vector3Template<f32>(400.0f, 5700.0f, 22500.0f), rw::math::fpu::Vector2Template<f32>(0.224999994f, 2.0f)) }
};

// ARTIST823FD270: this r3, position/direction v1/v2, scale/opacity f1/f2, params r6.
void BrnCoronaManager::BrnSubmissionInterface::AddCorona(Vector3 lvPosition, Vector3 lvDirection,
    f32 lfScale, f32 lfOpacity, const BrnCoronaTypeParams& lrType)
{
    // The lazy colour initialization precedes the facing test.
    static rw::RGBA lColour(255, 255, 255, 255);
    const __vector4 lvDelta = __vsubfp(lvPosition.mV, mCameraPosition.mV);
    unsigned int luFacing;
    __vcmpgtfpR(__vspltisw(0), __vmsum3fp(lvDelta, lvDirection.mV), &luFacing);
    if (!(luFacing & 0x80u))
        return;

    const f32 lfDistSqr = __vmsum3fp(lvDelta, lvDelta).x;
    const f32 lfSizeScale = lrType.mScaleCurve.Evaluate(lfDistSqr) * lfScale;
    f32 lfAlphaScale = 1.0f;
    if (lfDistSqr > kfCoronaFadeDistance)
    {
        const f32 lfFade = 1.0f - (lfDistSqr - kfCoronaFadeDistance) /
            (kfNoFadeDistanceSqrd - kfCoronaFadeDistance);
        lfAlphaScale = static_cast<f32>(__fsel(-lfFade, 0.0, lfFade));
    }
    if (mBufferIterator.GetNumCoronasWritten() >= static_cast<u32>(KI_MAX_CORONAS))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "mBufferIterator.GetNumCoronasWritten() < static_cast<uint32_t>( KI_MAX_CORONAS )",
            gszSourceFile, 593);
        CgsDev::Assert::EndAssert();
    }
    union { double mFloat; s64 mInteger; } lAlpha;
    lAlpha.mFloat = __fctidz((lfAlphaScale * lfOpacity) * 255.0f);
    lColour.m_rgba = (lColour.m_rgba & 0x00FFFFFFu) |
        (static_cast<u32>(static_cast<u8>(lAlpha.mInteger)) << 24);
    Vector2 lvSize;
    lvSize.mV = __vmulfp(lrType.mvSize.mV, __vspltw(__lvlx(&lfSizeScale, 0), 0));
    mBufferIterator.Write(lvPosition, lvDirection, lvSize, lrType.mfBiasDistance,
        lColour, lrType.miTextureID);
}

// ARTIST823FD428: preserve vector/FPR inputs; replace only r6 with table[type].
void BrnCoronaManager::BrnSubmissionInterface::AddCorona(Vector3 lvPosition, Vector3 lvDirection,
    f32 lfScale, f32 lfOpacity, const BrnCoronaType& leType)
{
    AddCorona(lvPosition, lvDirection, lfScale, lfOpacity,
        BrnCoronaTypeParams::GetCoronaTypeParams(leType));
}

// ARTIST823FD138: size is a packed FPU pair, not a third vector-register argument.
void BrnCoronaManager::BrnSubmissionInterface::AddPropCorona(Vector3 lvPosition, Vector3 lvDirection,
    const rw::math::fpu::Vector2Template<f32>& lrSize, f32 lfAlpha, s32 liTextureID)
{
    if (mBufferIterator.GetNumCoronasWritten() >= static_cast<u32>(KI_MAX_CORONAS))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "mBufferIterator.GetNumCoronasWritten() < static_cast<uint32_t>( KI_MAX_CORONAS )",
            gszSourceFile, 533);
        CgsDev::Assert::EndAssert();
    }
    const rw::math::fpu::Vector2Template<f32> lScale(lrSize.X(), lrSize.Y());
    const BrnEffects::Curves::SmoothStep lScaleCurve(kPropCoronaScale.mCurveParams, lScale);
    const __vector4 lvDelta = __vsubfp(lvPosition.mV, mCameraPosition.mV);
    const f32 lfSize = lScaleCurve.Evaluate(__vmsum3fp(lvDelta, lvDelta).x);
    union { double mFloat; s64 mInteger; } lAlpha;
    lAlpha.mFloat = __fctidz(lfAlpha * 255.0f);
    gPropCoronaColour.m_rgba = (gPropCoronaColour.m_rgba & 0x00FFFFFFu) |
        (static_cast<u32>(static_cast<u8>(lAlpha.mInteger)) << 24);
    Vector2 lvBroadcastSize;
    lvBroadcastSize.mV = __vspltw(__lvlx(&lfSize, 0), 0);
    mBufferIterator.Write(lvPosition, lvDirection, lvBroadcastSize, 0.5f,
        gPropCoronaColour, liTextureID);
}

// Inlined by ARTIST Renderer Update82405EC4..82405FA0; DecFIGS h:113.
void BrnCoronaManager::BrnSubmissionInterface::SetCameraInfo(Matrix44 lViewProj,
    Vector3 lCameraPosition, Vector4 lViewXyScale)
{
    mViewProj = lViewProj;
    mCameraPosition = lCameraPosition;
    mViewXyScale = lViewXyScale;
}

// ARTIST823FCD90. No host readiness or allocation fallthrough exists here.
void BrnCoronaManager::Construct(rw::IResourceAllocator& lrAllocator)
{
    mbActive = true;
    renderengine::CoronaRenderer::Initialize(lrAllocator);
    renderengine::CoronaRenderer::SetBlendState(CgsBlendStateFactory::GetState(2));
    renderengine::CoronaRenderer::SetDepthStencilStates(CgsDepthStencilStateFactory::GetState(2),
        CgsDepthStencilStateFactory::GetState(1));
    renderengine::CoronaRenderer::SetTextureAtlas(0, 1, &s_atlasUVs[0][0]);
    renderengine::CoronaBuffer::Parameters lParams;
    lParams.SetNumCoronas(512);
    const rw::ResourceDescriptor lDescriptor = renderengine::CoronaBuffer::GetResourceDescriptor(lParams);
    m_coronaBuffer0Resource = lrAllocator.DoAllocate(lDescriptor, 0);
    m_coronaBuffer1Resource = lrAllocator.DoAllocate(lDescriptor, 0);
    m_coronaBuffer0 = renderengine::CoronaBuffer::Initialize(m_coronaBuffer0Resource, lParams);
    m_coronaBuffer1 = renderengine::CoronaBuffer::Initialize(m_coronaBuffer1Resource, lParams);

    const renderengine::Corona lDefault = { Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f),
        Vector2(1.0f, 1.0f), 1.0f, renderengine::RGBA8(255, 255, 255, 255), 0 };
    renderengine::CoronaBuffer::Iterator lIterator0, lIterator1;
    m_coronaBuffer0->Lock(lIterator0);
    m_coronaBuffer1->Lock(lIterator1);
    for (u32 luIndex = 0; luIndex < 512; ++luIndex)
    {
        std::memcpy(&*lIterator0, &lDefault, sizeof(lDefault));
        std::memcpy(&*lIterator1, &lDefault, sizeof(lDefault));
        ++lIterator0;
        ++lIterator1;
    }
    mu8SubmissionSwapIndex = 0;
    m_textureStateAtlas = 0;
    mSubmissionInterface[0].mpBuffer = m_coronaBuffer0;
    mSubmissionInterface[1].mpBuffer = m_coronaBuffer1;
}

// ARTIST 0x823F7720
bool BrnCoronaManager::Release(rw::IResourceAllocator& lrAllocator)
{
    if (m_textureStateAtlas != 0)
        lrAllocator.DoFree(m_textureStateAtlasResource);
    m_coronaBuffer1->Release();
    m_coronaBuffer0->Release();
    lrAllocator.DoFree(m_coronaBuffer1Resource);
    lrAllocator.DoFree(m_coronaBuffer0Resource);
    renderengine::CoronaRenderer::Release(lrAllocator);
    return true;
}

// Inlined by ARTIST Renderer StartOfFrame/SwapBuffers/Update.
void BrnCoronaManager::Clear()
{
    BrnSubmissionInterface& lrInterface = mSubmissionInterface[mu8SubmissionSwapIndex];
    lrInterface.mpBuffer->Lock(lrInterface.mBufferIterator);
}
void BrnCoronaManager::Swap()
{
    mu8SubmissionSwapIndex = static_cast<u8>(mu8SubmissionSwapIndex ^ 1u);
}
BrnCoronaManager::BrnSubmissionInterface* BrnCoronaManager::GetSubmissionInterface()
{
    return &mSubmissionInterface[mu8SubmissionSwapIndex];
}

// ARTIST824075D8. Begin is unconditional after the active/atlas gates.
void BrnCoronaManager::Render(f32 lfWhiteLevel)
{
    if (!mbActive || !m_textureStateAtlas)
        return;
    BrnSubmissionInterface& lrInterface = mSubmissionInterface[mu8SubmissionSwapIndex ^ 1u];
    renderengine::CoronaRenderer::RenderParameters lParams;
    lParams.mvCameraPositionPlusWhiteLevel.mV = __vrlimi(lrInterface.mCameraPosition.mV,
        __vspltw(__lvlx(&lfWhiteLevel, 0), 0), 1, 0);
    lParams.mViewProjectionMatrix = lrInterface.mViewProj;
    lParams.mvViewXyScale = lrInterface.mViewXyScale;
    renderengine::CoronaRenderer::Begin(lParams);
    const u32 luNumCoronas = lrInterface.mBufferIterator.GetNumCoronasWritten();
    if (luNumCoronas)
    {
        renderengine::CoronaRenderer::BatchParameters lBatch;
        lBatch.muNumCoronas = luNumCoronas;
        lBatch.muFlags = renderengine::CoronaRenderer::BatchParameters::FLAG_OCCLUSION_ZTEST;
        renderengine::CoronaRenderer::Dispatch(lBatch, lrInterface.mpBuffer);
    }
    renderengine::CoronaRenderer::End();
}

// ARTIST823FD000. The existing real TextureState provider owns parameter defaults.
void BrnCoronaManager::SetTextureAtlas(const rw::IResourceAllocator& lrAllocator,
    renderengine::Texture* lpTextureAtlas)
{
    rw::IResourceAllocator& lAllocator = const_cast<rw::IResourceAllocator&>(lrAllocator);
    if (m_textureStateAtlas)
        lAllocator.DoFree(m_textureStateAtlasResource);
    renderengine::TextureState::Parameters lParams;
    lParams.muAddressU = lParams.muAddressV = lParams.muAddressW = 2;
    lParams.muMagFilter = lParams.muMinFilter = 1;
    lParams.muMipFilter = 0;
    lParams.mpTexture = lpTextureAtlas;
    const rw::ResourceDescriptor lDescriptor = renderengine::TextureState::GetResourceDescriptor(lParams);
    m_textureStateAtlasResource = lAllocator.DoAllocate(lDescriptor, 0);
    m_textureStateAtlas = renderengine::TextureState::Initialize(m_textureStateAtlasResource, lParams);
    renderengine::CoronaRenderer::SetTextureAtlas(m_textureStateAtlas, 4, &s_atlasUVs[0][0]);
}
