#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                                     // Vector3 / Vector3Plus / Vector4
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm3d.h"        // CgsGraphics::ImRenderer<V>
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsIm3dSkyDomeVertex.h"  // Im3dSkyDomeVertex
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"     // renderengine::ProgramVariableHandle

namespace rw { class IResourceAllocator; }
namespace renderengine { class SamplerState; class Texture; }

namespace BrnGraphics
{
    const u32 KU_SKYDOME_STATE_HANDLE_COUNT = 16u;

    class Im3dSkyDome : public CgsGraphics::Im3dBase<Im3dSkyDomeVertex>
    {
    public:
        void Construct(rw::IResourceAllocator* lpAllocator);

        void SetConstants(Vector3Plus lViewPositionAndSkyScale,
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
                          Vector4     lCloudLayerOpacity);

        const void* GetVertexDescriptorData() const { return mpVertexDescriptor; }

        bool HasPrograms() const
        {
            return mapVertexProgramBuffer[0] != 0 && mapPixelProgramBuffer[0] != 0;
        }

    private:
        renderengine::ProgramVariableHandle maStateHandles[KU_SKYDOME_STATE_HANDLE_COUNT];

        const renderengine::SamplerState* mpCloudsSamplerState;
    };
}
