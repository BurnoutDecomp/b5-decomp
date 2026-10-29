#include "SDKs/RenderEngineClub/MAIN/components/src/states/texturestate.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImNativePrograms.h"
#include "GameSource/Graphics/ImmediateMode/BrnIm3d.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"             // gpDebugPrint (the one-shot PC gate)
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"  // shadow::Device
#include "rw/rwcore_structs.h"                                         // rw::IResourceAllocator

#include <cmath>     // sqrtf
#include <cstring>   // std::memcpy / std::memset

namespace
{

    inline void Lanes(const void* lpVector, f32 lafOut[4])
    {
        std::memcpy(lafOut, lpVector, 16);
    }
}

namespace
{
    void PushShaderConstant(renderengine::ProgramVariableHandle* lpHandle, const void* lpValue16)
    {
        renderengine::FloatShaderStateIterator lIterator;
        renderengine::Device::BeginShaderStates(*lpHandle, lIterator);
        std::memcpy(lIterator.m_dest, lpValue16, 16);
    }

}

namespace BrnGraphics
{

void Im3dSkyDome::Construct(rw::IResourceAllocator* lpAllocator)
{
    const void* const lapVertexProgramBinary[1] = { CgsGraphics::gaIm3dSkyDomeVertexProgram };
    const u32 lauVertexProgramSize[1] = { sizeof(CgsGraphics::gaIm3dSkyDomeVertexProgram) };
    const void* const lapPixelProgramBinary[1] = { CgsGraphics::gaIm3dSkyDomePixelProgram };
    const u32 lauPixelProgramSize[1] = { sizeof(CgsGraphics::gaIm3dSkyDomePixelProgram) };

    CgsGraphics::ImRenderer<Im3dSkyDomeVertex>::Construct(
        lpAllocator,
        lapVertexProgramBinary, lauVertexProgramSize,
        lapPixelProgramBinary,  lauPixelProgramSize,
        1);

    renderengine::ProgramBufferData* const lpVertexProgram =
        reinterpret_cast<renderengine::ProgramBufferData*>(mapVertexProgramBuffer[0]);
    if (lpVertexProgram != 0)
    {
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpVertexProgram, reinterpret_cast<const u8*>("worldViewProj"),
            &maWorldViewProjStateHandle[0]);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpVertexProgram, reinterpret_cast<const u8*>("ViewPositionAndSkyScale"), &maStateHandles[0]);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpVertexProgram, reinterpret_cast<const u8*>("KeyLightDirAndXZLength"), &maStateHandles[1]);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpVertexProgram, reinterpret_cast<const u8*>("TopColourDrk"), &maStateHandles[2]);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpVertexProgram, reinterpret_cast<const u8*>("HorColourPow"), &maStateHandles[3]);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpVertexProgram, reinterpret_cast<const u8*>("SunColourPow"), &maStateHandles[4]);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpVertexProgram, reinterpret_cast<const u8*>("HorBleedSclPow"), &maStateHandles[5]);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpVertexProgram, reinterpret_cast<const u8*>("g_domeRanges"), &maStateHandles[6]);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpVertexProgram, reinterpret_cast<const u8*>("g_textureScaleAndOffsets"), &maStateHandles[7]);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpVertexProgram, reinterpret_cast<const u8*>("g_scatteringCoefficients"), &maStateHandles[8]);
    }
    else
    {
        CGS_ASSERT(false, "mapVertexProgramBuffer[ li8Program ] != NULL");
    }

    renderengine::ProgramBufferData* const lpPixelProgram =
        reinterpret_cast<renderengine::ProgramBufferData*>(mapPixelProgramBuffer[0]);
    if (lpPixelProgram != 0)
    {
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpPixelProgram, reinterpret_cast<const u8*>("g_densitySampler"), &maStateHandles[9]);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpPixelProgram, reinterpret_cast<const u8*>("g_lightSampler"), &maStateHandles[10]);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpPixelProgram, reinterpret_cast<const u8*>("g_darkColour"), &maStateHandles[11]);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpPixelProgram, reinterpret_cast<const u8*>("g_liteColour"), &maStateHandles[12]);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpPixelProgram, reinterpret_cast<const u8*>("g_layerCloudiness"), &maStateHandles[13]);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpPixelProgram, reinterpret_cast<const u8*>("g_layerInvFeather"), &maStateHandles[14]);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpPixelProgram, reinterpret_cast<const u8*>("g_layerAlphas"), &maStateHandles[15]);
    }
    else
    {
        CGS_ASSERT(false, "mapPixelProgramBuffer[ li8Program ] != NULL");
    }

    renderengine::TextureState::Parameters lSamplerParameters;
    std::memset(&lSamplerParameters, 0, sizeof(lSamplerParameters));
    lSamplerParameters.muAddressU      = 0u;
    lSamplerParameters.muAddressV      = 0u;
    lSamplerParameters.muAddressW      = 0u;
    lSamplerParameters.muMagFilter     = 1u;
    lSamplerParameters.muMinFilter     = 1u;
    lSamplerParameters.muMipFilter     = 1u;
    lSamplerParameters.muMagFilterZ        = 0u;
    lSamplerParameters.muMinFilterZ        = 0u;
    lSamplerParameters.muMinMipLevel = 13u;
    lSamplerParameters.muMaxMipLevel        = 0u;
    lSamplerParameters.muMaxAnisotropy       = 1u;
    lSamplerParameters.mfMipLodBias    = 0.0f;
    lSamplerParameters.mfAnisotropyBias       = 0.0f;
    lSamplerParameters.miHGradientExpBias       = 0u;
    lSamplerParameters.miVGradientExpBias       = 0u;
    lSamplerParameters.muTrilinearThreshold       = 0u;
    lSamplerParameters.mbWhiteBorder      = 0u;
    lSamplerParameters.mbSeparateZFilterEnable      = 0u;
    lSamplerParameters.mbWhiteBorderW      = 0u;
    lSamplerParameters.mbPointBorderEnable      = 1u;
    lSamplerParameters.mbUnidentified44      = 1u;

    rw::ResourceDescriptor lDescriptor;
    renderengine::SamplerState::GetResourceDescriptor(
        reinterpret_cast<renderengine::ResourceDescriptorEntry*>(&lDescriptor));
    rw::Resource lResource = lpAllocator->DoAllocate(lDescriptor, 0);
    mpCloudsSamplerState = reinterpret_cast<const renderengine::SamplerState*>(
        renderengine::SamplerState::Initialize(
            reinterpret_cast<renderengine::SamplerStateData**>(&lResource),
            reinterpret_cast<const renderengine::SamplerStateParameters*>(&lSamplerParameters)));
}

void Im3dSkyDome::SetConstants(Vector3Plus lViewPositionAndSkyScale,
                               Vector3     lKeyLightDirection,
                               Vector4     lSky_TopColourDrk,
                               Vector4     lSky_HorColourPow,
                               Vector4     lSky_SunColourPow,
                               Vector3     lSky_HorBleedSclPow,
                               const renderengine::Texture* lpLayer0Density,
                               const renderengine::Texture* lpLayer0Lighting,
                               Vector4     lFogScattering,
                               Vector4     lCloudDarkColour0,
                               Vector4     lCloudLiteColour0,
                               Vector4     lCloudDomeRanges,
                               Vector4     lCloudTextureScaleAndOffsets0,
                               Vector4     lCloudLayerDensity,
                               Vector4     lCloudLayerInvFeather,
                               Vector4     lCloudLayerOpacity)
{
    PushShaderConstant(&maStateHandles[0], &lViewPositionAndSkyScale);

    f32 lafKeyLight[4];
    Lanes(&lKeyLightDirection, lafKeyLight);
    f32 lafKeyLightDirAndXZLength[4];
    lafKeyLightDirAndXZLength[0] = -lafKeyLight[0];
    lafKeyLightDirAndXZLength[1] = -lafKeyLight[1];
    lafKeyLightDirAndXZLength[2] = -lafKeyLight[2];
    lafKeyLightDirAndXZLength[3] =
        sqrtf((lafKeyLight[0] * lafKeyLight[0]) + (lafKeyLight[2] * lafKeyLight[2]));
    PushShaderConstant(&maStateHandles[1], lafKeyLightDirAndXZLength);

    PushShaderConstant(&maStateHandles[2], &lSky_TopColourDrk);
    PushShaderConstant(&maStateHandles[3], &lSky_HorColourPow);
    PushShaderConstant(&maStateHandles[4], &lSky_SunColourPow);

    f32 lafHorBleedSclPow[4];
    Lanes(&lSky_HorBleedSclPow, lafHorBleedSclPow);
    lafHorBleedSclPow[3] = 0.0f;
    PushShaderConstant(&maStateHandles[5], lafHorBleedSclPow);

    PushShaderConstant(&maStateHandles[7], &lCloudTextureScaleAndOffsets0);
    PushShaderConstant(&maStateHandles[8], &lFogScattering);
    PushShaderConstant(&maStateHandles[6], &lCloudDomeRanges);

    {
        void* const lpSamplerState =
            const_cast<void*>(static_cast<const void*>(mpCloudsSamplerState));

        const u32 luDensitySampler = maStateHandles[9].mu8RegisterSet;
        shadow::Device::SetState(lpSamplerState, luDensitySampler);
        shadow::Device::SetResource(const_cast<void*>(static_cast<const void*>(lpLayer0Density)),
                                    luDensitySampler);

        const u32 luLightSampler = maStateHandles[10].mu8RegisterSet;
        shadow::Device::SetState(lpSamplerState, luLightSampler);
        shadow::Device::SetResource(const_cast<void*>(static_cast<const void*>(lpLayer0Lighting)),
                                    luLightSampler);
    }

    PushShaderConstant(&maStateHandles[11], &lCloudDarkColour0);
    PushShaderConstant(&maStateHandles[12], &lCloudLiteColour0);
    PushShaderConstant(&maStateHandles[13], &lCloudLayerDensity);
    PushShaderConstant(&maStateHandles[14], &lCloudLayerInvFeather);
    PushShaderConstant(&maStateHandles[15], &lCloudLayerOpacity);
}

}   // namespace BrnGraphics
