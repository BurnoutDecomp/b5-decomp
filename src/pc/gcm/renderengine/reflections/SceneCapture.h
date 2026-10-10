#pragma once

#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"

namespace BrnWorld { class WorldEntityModule; struct ShaderLodInfo; }
namespace CgsGraphics { class DispatchFrame; class Camera; }
namespace BrnTraffic { class TrafficEntityModule; namespace BrnTrafficIO { class InputBuffer_Dispatch; } }

namespace CgsPC::Reflections
{
    // FLAG PC-platform leaf: native scene additions, separate from recovered
    // console entry points. Access is limited to the resident backdrop stream.
    struct WorldCapture
    {
        static void SubmitBackdrops(BrnWorld::WorldEntityModule& lrWorld, CgsGraphics::DispatchFrame* lpFrame,
            const CgsGraphics::Camera& lrCamera, const BrnWorld::ShaderLodInfo& lrShaderLod, s32 liFaceList);
    };
    struct TrafficCapture
    {
        static void Submit(BrnTraffic::TrafficEntityModule& lrTraffic,
            const Array<CgsSceneManager::EntityId, 650u>& lrVisible,
            const BrnTraffic::BrnTrafficIO::InputBuffer_Dispatch* lpInput,
            const CgsGraphics::Camera& lrCamera, Vector4 lvFogScattering, Vector4 lvFogColour, s32 liFaceList);
    };
}
