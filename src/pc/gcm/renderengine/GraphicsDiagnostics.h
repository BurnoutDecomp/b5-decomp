#pragma once
// FLAG PC-platform leaf: opt-in evidence from the actual geometry submission paths.
#include "types.hpp"
#include "GameShared/GameClasses/Graphics/CgsModel.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "pc/gcm/renderengine/reflections/ReflectionDistance.h"
#include "pc/gcm/renderengine/reflections/ReflectionLod.h"
#include <cstdio>
#include <cstdlib>

namespace renderengine
{
    enum GraphicsModelCategoryPC { E_GRAPHICS_WORLD, E_GRAPHICS_PROP, E_GRAPHICS_ENVMAP,
                                  E_GRAPHICS_TRAFFIC, E_GRAPHICS_RACECAR, E_GRAPHICS_PROP_ENVMAP,
                                  E_GRAPHICS_MODEL_CATEGORIES };
    struct GraphicsDiagnosticsPC
    {
        u32 mauLods[E_GRAPHICS_MODEL_CATEGORIES][5] = {};
        u32 mauDistinctModels[E_GRAPHICS_MODEL_CATEGORIES] = {};
        u32 muWorldBase = 0, muPropBase = 0, muEnvLod = 2;
        u32 muEnvConsidered = 0, muEnvMissing = 0, muEnvCulled = 0;
        u32 muEnvExtended = 0, muPropEnvExtended = 0;
        u32 muTrafficShadowRecords = 0;
    };
    inline bool GraphicsDiagnosticsEnabledPC()
    {
        static const bool sbEnabled = std::getenv("BRN_GRAPHICS_DIAG") != nullptr;
        return sbEnabled;
    }
    inline GraphicsDiagnosticsPC& GetGraphicsDiagnosticsPC()
    {
        static GraphicsDiagnosticsPC sDiagnostics;
        return sDiagnostics;
    }
    inline void BeginGraphicsDiagnosticsPC()
    {
        if (GraphicsDiagnosticsEnabledPC()) GetGraphicsDiagnosticsPC() = GraphicsDiagnosticsPC();
    }
    inline void RecordGraphicsModelPC(GraphicsModelCategoryPC leCategory,
                                      const CgsGraphics::Model* lpModel, u32 luLod)
    {
        if (!GraphicsDiagnosticsEnabledPC()) return;
        GraphicsDiagnosticsPC& lrDiag = GetGraphicsDiagnosticsPC();
        if (luLod < 5u) ++lrDiag.mauLods[leCategory][luLod];
        const CgsGraphics::Renderable* lpChosen = lpModel->GetRenderable(static_cast<CgsGraphics::Model::State>(luLod));
        for (u32 luState = 0; luState < lpModel->GetNumLods(); ++luState)
        {
            const auto leState = static_cast<CgsGraphics::Model::State>(luState);
            if (lpModel->DoesStateExist(leState) && lpModel->GetRenderable(leState) != lpChosen)
            {
                ++lrDiag.mauDistinctModels[leCategory];
                break;
            }
        }
    }
    inline void EndGraphicsDiagnosticsPC()
    {
        if (!GraphicsDiagnosticsEnabledPC()) return;
        static u32 suFrames = 0;
        if ((++suFrames % 120u) != 0u) return;
        const auto& lrDiag = GetGraphicsDiagnosticsPC();
        char lacMessage[768];
        std::snprintf(lacMessage, sizeof(lacMessage),
            "[graphics-effect] frame=%u worldBase=%u propBase=%u envLOD=%u"
            " world=%u/%u/%u/%u/%u prop=%u/%u/%u/%u/%u env=%u/%u/%u/%u/%u"
            " traffic=%u/%u/%u/%u/%u racecar=%u/%u/%u/%u/%u"
            " distinct=%u/%u/%u/%u/%u envCandidates=%u missing=%u culled=%u trafficShadowRecords=%u\n",
            suFrames, lrDiag.muWorldBase, lrDiag.muPropBase, lrDiag.muEnvLod,
            lrDiag.mauLods[0][0],lrDiag.mauLods[0][1],lrDiag.mauLods[0][2],lrDiag.mauLods[0][3],lrDiag.mauLods[0][4],
            lrDiag.mauLods[1][0],lrDiag.mauLods[1][1],lrDiag.mauLods[1][2],lrDiag.mauLods[1][3],lrDiag.mauLods[1][4],
            lrDiag.mauLods[2][0],lrDiag.mauLods[2][1],lrDiag.mauLods[2][2],lrDiag.mauLods[2][3],lrDiag.mauLods[2][4],
            lrDiag.mauLods[3][0],lrDiag.mauLods[3][1],lrDiag.mauLods[3][2],lrDiag.mauLods[3][3],lrDiag.mauLods[3][4],
            lrDiag.mauLods[4][0],lrDiag.mauLods[4][1],lrDiag.mauLods[4][2],lrDiag.mauLods[4][3],lrDiag.mauLods[4][4],
            lrDiag.mauDistinctModels[0],lrDiag.mauDistinctModels[1],lrDiag.mauDistinctModels[2],
            lrDiag.mauDistinctModels[3],lrDiag.mauDistinctModels[4],
            lrDiag.muEnvConsidered,lrDiag.muEnvMissing,lrDiag.muEnvCulled,lrDiag.muTrafficShadowRecords);
        CgsDev::Log::WriteToLog(lacMessage);
        std::snprintf(lacMessage, sizeof(lacMessage),
            "[graphics-reflection-distance] frame=%u distance=%.9g worldExtended=%u propExtended=%u\n",
            suFrames, EnvironmentMapDrawDistancePC(), lrDiag.muEnvExtended, lrDiag.muPropEnvExtended);
        CgsDev::Log::WriteToLog(lacMessage);
        std::snprintf(lacMessage, sizeof(lacMessage),
            "[graphics-prop-reflection] frame=%u lods=%u/%u/%u distinct=%u\n", suFrames,
            lrDiag.mauLods[E_GRAPHICS_PROP_ENVMAP][0], lrDiag.mauLods[E_GRAPHICS_PROP_ENVMAP][1],
            lrDiag.mauLods[E_GRAPHICS_PROP_ENVMAP][2], lrDiag.mauDistinctModels[E_GRAPHICS_PROP_ENVMAP]);
        CgsDev::Log::WriteToLog(lacMessage);
        const auto& lrWorldLod = WorldEnvironmentMapLodSettingsPC();
        const auto& lrPropLod = PropEnvironmentMapLodSettingsPC();
        std::snprintf(lacMessage, sizeof(lacMessage),
            "[graphics-reflection-lod] frame=%u worldMode=%d propMode=%d worldScale=%.9g propScale=%.9g"
            " world=%u/%u/%u prop=%u/%u/%u\n", suFrames,
            lrWorldLod.miMode, lrPropLod.miMode, lrWorldLod.mfDistanceScale, lrPropLod.mfDistanceScale,
            lrDiag.mauLods[E_GRAPHICS_ENVMAP][0], lrDiag.mauLods[E_GRAPHICS_ENVMAP][1], lrDiag.mauLods[E_GRAPHICS_ENVMAP][2],
            lrDiag.mauLods[E_GRAPHICS_PROP_ENVMAP][0], lrDiag.mauLods[E_GRAPHICS_PROP_ENVMAP][1], lrDiag.mauLods[E_GRAPHICS_PROP_ENVMAP][2]);
        CgsDev::Log::WriteToLog(lacMessage);
    }
}
