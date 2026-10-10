#pragma once

#include "pc/gcm/renderengine/reflections/ReflectionLod.h"
#include <algorithm>
#include <cmath>

namespace CgsPC::Reflections
{
    // FLAG PC-platform leaf: opt-in scene categories for native cube captures.
    // Each category owns its policy; the recovered main-view settings are read,
    // never overwritten while a reflection face is being generated.
    enum DistanceMode { E_DISTANCE_RELATIVE, E_DISTANCE_FIXED };
    struct ObjectSettings
    {
        bool mbEnabled = false;
        s32 miDistanceMode = E_DISTANCE_FIXED;
        f32 mfDrawDistance = 100.0f;
        f32 mfDrawDistanceScale = 0.5f;
        s32 miFixedLod = 2;
        renderengine::EnvironmentMapLodSettingsPC mLod;

        f32 GetDrawDistance(f32 lfNormalDistance) const
        {
            const f32 lfNormal = std::isfinite(lfNormalDistance) && lfNormalDistance > 0.0f
                ? lfNormalDistance : 0.0f;
            const f32 lfScale = std::isfinite(mfDrawDistanceScale) && mfDrawDistanceScale > 0.0f
                ? (std::min)(mfDrawDistanceScale, 10.0f) : 0.5f;
            const f32 lfFixed = std::isfinite(mfDrawDistance) && mfDrawDistance > 0.0f
                ? mfDrawDistance : 0.0f;
            return (std::min)(miDistanceMode == E_DISTANCE_RELATIVE ? lfNormal * lfScale : lfFixed, 10000.0f);
        }

        bool IsVisible(f32 lfDistanceSquared, f32 lfNormalDistance) const
        {
            const f32 lfDistance = GetDrawDistance(lfNormalDistance);
            return mbEnabled && std::isfinite(lfDistanceSquared) && lfDistanceSquared >= 0.0f
                && lfDistance > 0.0f && lfDistanceSquared <= lfDistance * lfDistance;
        }

        template<class ModelType>
        s32 SelectLod(const ModelType* lpModel, f32 lfDistanceSquared, const f32* lpaNormalDistances = nullptr) const
        {
            if (mLod.IsDistanceBased())
            {
                if (lpaNormalDistances)
                {
                    s32 liAvailable = -1;
                    f32 lfPrevious = 0.0f;
                    for (u32 luLod = 0; luLod < lpModel->GetNumLods() && luLod < 3u; ++luLod)
                    {
                        const f32 lfDistance = luLod < 2u ? (std::max)(lfPrevious,
                            mLod.GetTransitionDistance(luLod, lpaNormalDistances[luLod])) : 0.0f;
                        lfPrevious = lfDistance;
                        if (!lpModel->DoesStateExist(static_cast<typename ModelType::State>(luLod))) continue;
                        liAvailable = static_cast<s32>(luLod);
                        if (luLod == 2u || lfDistanceSquared <= lfDistance * lfDistance) return liAvailable;
                    }
                    return liAvailable;
                }
                return renderengine::SelectEnvironmentMapLodPC(lpModel, lfDistanceSquared, mLod, false, nullptr);
            }
            // New categories may have a single authored mesh. Choose the nearest
            // available coarser state, then the coarsest finer state, excluding
            // vehicle box/proxy states above LOD2.
            const s32 liWanted = std::clamp(miFixedLod, 0, 2);
            s32 liFiner = -1;
            for (u32 luLod = 0; luLod < lpModel->GetNumLods() && luLod < 3u; ++luLod)
            {
                if (!lpModel->DoesStateExist(static_cast<typename ModelType::State>(luLod))) continue;
                if (static_cast<s32>(luLod) >= liWanted) return static_cast<s32>(luLod);
                liFiner = static_cast<s32>(luLod);
            }
            return liFiner;
        }
    };

    inline ObjectSettings& Backdrops()
    {
        static ObjectSettings sSettings = [] { ObjectSettings lSettings; lSettings.mfDrawDistance = 3000.0f; return lSettings; }();
        return sSettings;
    }
    inline ObjectSettings& Traffic() { static ObjectSettings sSettings; return sSettings; }
    inline ObjectSettings& Rivals() { static ObjectSettings sSettings; return sSettings; }
    inline ObjectSettings& Wheels()
    {
        static ObjectSettings sSettings = [] { ObjectSettings lSettings; lSettings.mbEnabled = true; lSettings.miFixedLod = 1; return lSettings; }();
        return sSettings;
    }
    inline ObjectSettings& Lights() { static ObjectSettings sSettings; return sSettings; }
    inline ObjectSettings& Particles() { static ObjectSettings sSettings; return sSettings; }
    inline ObjectSettings& Glass() { static ObjectSettings sSettings; return sSettings; }
    inline ObjectSettings& Decals() { static ObjectSettings sSettings; return sSettings; }
    inline ObjectSettings& PlayerWheels()
    {
        static ObjectSettings sSettings = [] { ObjectSettings lSettings; lSettings.miFixedLod = 1; return lSettings; }();
        return sSettings;
    }
    inline bool& IncludePlayerParticles() { static bool sbEnabled = true; return sbEnabled; }

    // Published by the normal vehicle LOD calculation before face generation.
    // Reflection detail follows its live quality/aggressive blend and zoom.
    inline f32 safVehicleLodDistances[5] = {10, 22, 35, 50, 70};
    inline void SetVehicleLodDistances(const f32* lpaDistances)
    { for (u32 lu = 0; lu < 5; ++lu) safVehicleLodDistances[lu] = lpaDistances[lu]; }
    inline f32 NormalVehicleDistance() { return safVehicleLodDistances[3]; }
    inline f32 sfTrafficNormalDistance = 250.0f;
    inline f32 sfParticleNormalDistance = 10000.0f;

    inline f32 CaptureDistance(f32 lfWorldDistance)
    {
        f32 lfDistance = lfWorldDistance;
        if (Backdrops().mbEnabled) lfDistance = (std::max)(lfDistance, Backdrops().GetDrawDistance(10000.0f));
        if (Traffic().mbEnabled) lfDistance = (std::max)(lfDistance, Traffic().GetDrawDistance(sfTrafficNormalDistance));
        if (Rivals().mbEnabled) lfDistance = (std::max)(lfDistance, Rivals().GetDrawDistance(NormalVehicleDistance()));
        if (PlayerWheels().mbEnabled) lfDistance = (std::max)(lfDistance, PlayerWheels().GetDrawDistance(NormalVehicleDistance()));
        if (Lights().mbEnabled) lfDistance = (std::max)(lfDistance, Lights().GetDrawDistance(250.0f));
        if (Particles().mbEnabled) lfDistance = (std::max)(lfDistance, Particles().GetDrawDistance(sfParticleNormalDistance));
        if (Decals().mbEnabled) lfDistance = (std::max)(lfDistance, Decals().GetDrawDistance(sfParticleNormalDistance));
        return lfDistance;
    }

    enum CaptureCategory { E_CAPTURE_NONE, E_CAPTURE_TRAFFIC, E_CAPTURE_RIVALS, E_CAPTURE_PLAYER_WHEELS };
    inline thread_local CaptureCategory seCaptureCategory = E_CAPTURE_NONE;
    inline thread_local f32 sfVehicleDrawDistance = 50.0f;
    inline thread_local s32 siVehicleFaceList = 5;
    class VehicleScope
    {
    public:
        explicit VehicleScope(CaptureCategory leCategory, s32 liFaceList = 5, f32 lfNormalDistance = NormalVehicleDistance())
            : mePrevious(seCaptureCategory), mfPreviousDistance(sfVehicleDrawDistance), miPreviousList(siVehicleFaceList)
        { seCaptureCategory = leCategory; sfVehicleDrawDistance = lfNormalDistance; siVehicleFaceList = liFaceList; }
        ~VehicleScope()
        { seCaptureCategory = mePrevious; sfVehicleDrawDistance = mfPreviousDistance; siVehicleFaceList = miPreviousList; }
        VehicleScope(const VehicleScope&) = delete;
        VehicleScope& operator=(const VehicleScope&) = delete;
    private:
        CaptureCategory mePrevious;
        f32 mfPreviousDistance;
        s32 miPreviousList;
    };
    inline bool IsVehicleCapture() { return seCaptureCategory != E_CAPTURE_NONE; }
    inline bool IsPlayerWheelCapture() { return seCaptureCategory == E_CAPTURE_PLAYER_WHEELS; }
    inline const ObjectSettings& WheelSettings() { return IsPlayerWheelCapture() ? PlayerWheels() : Wheels(); }
    inline const ObjectSettings& VehicleSettings() { return seCaptureCategory == E_CAPTURE_TRAFFIC ? Traffic() : Rivals(); }
    inline s32 GlassMeshList() { return 25 + siVehicleFaceList - 5; }
    template<class ModelType>
    inline s32 SelectVehicleLod(const ObjectSettings& lrSettings, const ModelType* lpModel, f32 lfDistanceSquared)
    { return lrSettings.SelectLod(lpModel, lfDistanceSquared, safVehicleLodDistances); }
}
