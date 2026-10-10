#pragma once

#include "types.hpp"
#include <cmath>

namespace renderengine
{
    // FLAG PC-platform leaf: optional reflection-only mesh detail policy. Fixed
    // mode preserves the console's LOD2 default and its missing-state behavior.
    enum EnvironmentMapLodModePC
    {
        E_ENVIRONMENT_MAP_LOD_FIXED = 0,
        E_ENVIRONMENT_MAP_LOD_RELATIVE,
        E_ENVIRONMENT_MAP_LOD_CUSTOM
    };

    struct EnvironmentMapLodSettingsPC
    {
        s32 miMode = E_ENVIRONMENT_MAP_LOD_FIXED;
        f32 mfDistanceScale = 0.5f;
        f32 mafTransitionDistances[2] = {50.0f, 100.0f};

        bool IsDistanceBased() const
        {
            return miMode == E_ENVIRONMENT_MAP_LOD_RELATIVE || miMode == E_ENVIRONMENT_MAP_LOD_CUSTOM;
        }

        f32 GetTransitionDistance(u32 luLod, f32 lfNormalDistance) const
        {
            const f32 lfScale = std::isfinite(mfDistanceScale) && mfDistanceScale > 0.0f
                && mfDistanceScale <= 10.0f ? mfDistanceScale : 0.5f;
            if (miMode == E_ENVIRONMENT_MAP_LOD_CUSTOM)
            {
                const f32 lfCustomDistance = mafTransitionDistances[luLod];
                if (std::isfinite(lfCustomDistance) && lfCustomDistance > 0.0f && lfCustomDistance <= 10000.0f)
                    return lfCustomDistance;
            }
            return lfNormalDistance * lfScale;
        }
    };

    inline EnvironmentMapLodSettingsPC& WorldEnvironmentMapLodSettingsPC()
    {
        static EnvironmentMapLodSettingsPC sSettings;
        return sSettings;
    }

    inline EnvironmentMapLodSettingsPC& PropEnvironmentMapLodSettingsPC()
    {
        static EnvironmentMapLodSettingsPC sSettings;
        return sSettings;
    }

    // ModelType is the graphics model API; keeping this header independent of
    // the render backend also lets the production dispatch fixtures observe it.
    template<class ModelType>
    inline s32 SelectEnvironmentMapLodPC(const ModelType* lpModel, f32 lfDistanceSq,
        const EnvironmentMapLodSettingsPC& lrSettings, bool lbOverrideDistances,
        const s32* lpaOverrideDistances)
    {
        s32 liCoarsestAvailable = -1;
        f32 lfPreviousDistance = 0.0f;
        for (u32 luLod = 0; luLod < lpModel->GetNumLods() && luLod < 3u; ++luLod)
        {
            f32 lfDistance = 0.0f;
            if (luLod < 2u)
            {
                const f32 lfNormalDistance = lbOverrideDistances
                    ? static_cast<f32>(lpaOverrideDistances[luLod]) : lpModel->GetLodDistance(luLod);
                lfDistance = lrSettings.GetTransitionDistance(luLod, lfNormalDistance);
                // Independently editable thresholds must still form an ordered ladder.
                if (lfDistance < lfPreviousDistance) lfDistance = lfPreviousDistance;
                lfPreviousDistance = lfDistance;
            }
            if (!lpModel->DoesStateExist(static_cast<typename ModelType::State>(luLod))) continue;
            liCoarsestAvailable = static_cast<s32>(luLod);
            if (luLod == 2u || lfDistanceSq <= lfDistance * lfDistance) return liCoarsestAvailable;
        }
        // Sparse models keep their coarsest available mesh through the separate
        // draw cutoff, rather than disappearing when a transition state is absent.
        return liCoarsestAvailable;
    }
}
