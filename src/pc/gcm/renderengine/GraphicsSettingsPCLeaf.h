#pragma once

// FLAG PC-platform leaf: INI access to ARTIST tweakables and native quality options.
// Load once before module Construct; consumers seed the existing engine state.
#include "types.hpp"
#include <Windows.h>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace renderengine
{
    enum VehicleLodPresetPC
    {
        E_VEHICLE_LOD_DEFAULT,
        E_VEHICLE_LOD_POTATO,
        E_VEHICLE_LOD_LOW,
        E_VEHICLE_LOD_MEDIUM,
        E_VEHICLE_LOD_HIGH,
        E_VEHICLE_LOD_ULTRA,
        E_VEHICLE_LOD_CUSTOM,
        E_NUM_VEHICLE_LOD_PRESETS
    };

    inline const char* VehicleLodPresetNamePC(VehicleLodPresetPC lePreset)
    {
        static const char* const kapcNames[E_NUM_VEHICLE_LOD_PRESETS] =
            { "Default", "Potato", "Low", "Medium", "High", "Ultra", "Custom" };
        return kapcNames[lePreset];
    }

    struct GraphicsSettingsPC
    {
        f32 mfBloomLuminanceScale = 1.0f;
        s32 miEnvironmentMapLod = 2;
        bool mbTrafficShadows = false;
        s32 miAnisotropicFiltering = 1;
        s32 miShadowResolutionScale = 1;
        f32 mfShadowDistance = 120.0f;
        f32 mfShadowSlopeBias = 0.0f;
        s32 miWorldLodOverrideDistance = 0;
        s32 miPropLodOverrideDistance = 0;
        VehicleLodPresetPC meVehicleLodPreset = E_VEHICLE_LOD_DEFAULT;
        f32 mafVehicleLodDistances[5] = { 10.0f, 22.0f, 35.0f, 50.0f, 70.0f };

        void SetVehicleLodPreset(VehicleLodPresetPC lePreset)
        {
            // Preset distances decoded from reference float32 values, not multipliers.
            // High's LOD3/4 are 150/210; Low's LOD2 is 17.
            static const f32 kaafDistances[E_VEHICLE_LOD_CUSTOM][5] =
            {
                { 10.0f, 22.0f,  35.0f,  50.0f,  70.0f },
                {  1.0f,  2.0f,   4.0f,   6.0f,  10.0f },
                {  5.0f, 11.0f,  17.0f,  25.0f,  35.0f },
                { 20.0f, 44.0f,  70.0f, 100.0f, 140.0f },
                { 30.0f, 66.0f, 105.0f, 150.0f, 210.0f },
                { 50.0f,110.0f, 175.0f, 250.0f, 350.0f }
            };
            meVehicleLodPreset = lePreset;
            if (lePreset == E_VEHICLE_LOD_CUSTOM) return;
            for (u32 luLod = 0; luLod < 5u; ++luLod)
                mafVehicleLodDistances[luLod] = kaafDistances[lePreset][luLod];
        }

        void ApplyLodOverride(s32 liBaseDistance, bool& lrbOverride, s32 (&lraiDistances)[3]) const
        {
            if (liBaseDistance == 0) return; // Keep the original asset-defined switch distances.
            lrbOverride = true;
            for (s32 liLod = 0; liLod < 3; ++liLod)
                lraiDistances[liLod] = liBaseDistance * (liLod + 1);
        }
    };

    inline GraphicsSettingsPC& GetGraphicsSettingsPC()
    {
        static GraphicsSettingsPC sSettings;
        return sSettings;
    }

    inline bool GraphicsNumberHasEndedPC(const char* lpcEnd)
    {
        while (std::isspace(static_cast<unsigned char>(*lpcEnd))) ++lpcEnd;
        return *lpcEnd == '\0';
    }

    inline f32 ReadGraphicsFloatPC(const char* lpcPath, const char* lpcKey,
                                  f32 lfDefault, f32 lfMin, f32 lfMax)
    {
        char lacValue[128];
        GetPrivateProfileStringA("Graphics", lpcKey, "", lacValue, sizeof(lacValue), lpcPath);
        char* lpcEnd = nullptr;
        errno = 0;
        const f32 lfValue = std::strtof(lacValue, &lpcEnd);
        return lpcEnd != lacValue && GraphicsNumberHasEndedPC(lpcEnd) && errno != ERANGE &&
               std::isfinite(lfValue) && lfValue >= lfMin && lfValue <= lfMax ? lfValue : lfDefault;
    }

    inline s32 ReadGraphicsIntPC(const char* lpcPath, const char* lpcKey,
                                s32 liDefault, s32 liMin, s32 liMax)
    {
        char lacValue[128];
        GetPrivateProfileStringA("Graphics", lpcKey, "", lacValue, sizeof(lacValue), lpcPath);
        char* lpcEnd = nullptr;
        errno = 0;
        const long liValue = std::strtol(lacValue, &lpcEnd, 10);
        return lpcEnd != lacValue && GraphicsNumberHasEndedPC(lpcEnd) && errno != ERANGE &&
               liValue >= liMin && liValue <= liMax ? static_cast<s32>(liValue) : liDefault;
    }

    inline void LoadGraphicsSettingsPC(const char* lpcPath)
    {
        GraphicsSettingsPC& lrSettings = GetGraphicsSettingsPC();
        lrSettings = GraphicsSettingsPC();
        lrSettings.mfBloomLuminanceScale = ReadGraphicsFloatPC(lpcPath, "BloomLuminanceScale", 1.0f, 0.0f, 10.0f);
        lrSettings.miEnvironmentMapLod = ReadGraphicsIntPC(lpcPath, "EnvironmentMapLOD", 2, 0, 2);
        lrSettings.mbTrafficShadows = ReadGraphicsIntPC(lpcPath, "TrafficShadows", 0, 0, 1) != 0;
        const s32 liAnisotropy = ReadGraphicsIntPC(lpcPath, "AnisotropicFiltering", 1, 1, 16);
        lrSettings.miAnisotropicFiltering = (liAnisotropy & (liAnisotropy - 1)) == 0 ? liAnisotropy : 1;
        lrSettings.miShadowResolutionScale = ReadGraphicsIntPC(lpcPath, "ShadowResolutionScale", 1, 1, 2);
        lrSettings.mfShadowDistance = ReadGraphicsFloatPC(lpcPath, "ShadowDistance", 120.0f, 30.0f, 500.0f);
        lrSettings.mfShadowSlopeBias = ReadGraphicsFloatPC(lpcPath, "ShadowSlopeBias", 0.0f, 0.0f, 4.0f);
        lrSettings.miWorldLodOverrideDistance = ReadGraphicsIntPC(lpcPath, "WorldLODOverrideDistance", 0, 0, 10000);
        lrSettings.miPropLodOverrideDistance = ReadGraphicsIntPC(lpcPath, "PropLODOverrideDistance", 0, 0, 10000);

        char lacPreset[128];
        GetPrivateProfileStringA("Graphics", "VehicleLODPreset", "Default", lacPreset, sizeof(lacPreset), lpcPath);
        for (s32 liPreset = 0; liPreset < E_NUM_VEHICLE_LOD_PRESETS; ++liPreset)
        {
            const VehicleLodPresetPC lePreset = static_cast<VehicleLodPresetPC>(liPreset);
            if (lstrcmpiA(lacPreset, VehicleLodPresetNamePC(lePreset)) == 0)
            {
                lrSettings.SetVehicleLodPreset(lePreset);
                break;
            }
        }
        if (lrSettings.meVehicleLodPreset == E_VEHICLE_LOD_CUSTOM)
        {
            for (u32 luLod = 0; luLod < 5u; ++luLod)
            {
                char lacKey[32];
                std::snprintf(lacKey, sizeof(lacKey), "VehicleLOD%uDistance", luLod);
                lrSettings.mafVehicleLodDistances[luLod] = ReadGraphicsFloatPC(
                    lpcPath, lacKey, lrSettings.mafVehicleLodDistances[luLod], 0.0f, 10000.0f);
            }
            for (u32 luLod = 1; luLod < 5u; ++luLod)
            {
                if (lrSettings.mafVehicleLodDistances[luLod] < lrSettings.mafVehicleLodDistances[luLod - 1u])
                {
                    lrSettings.SetVehicleLodPreset(E_VEHICLE_LOD_DEFAULT);
                    break;
                }
            }
        }
    }

    inline void SaveGraphicsSettingsPC(const char* lpcPath)
    {
        const GraphicsSettingsPC& lrSettings = GetGraphicsSettingsPC();
        char lacValue[128];
        std::snprintf(lacValue, sizeof(lacValue), "%.9g", lrSettings.mfBloomLuminanceScale);
        WritePrivateProfileStringA("Graphics", "BloomLuminanceScale", lacValue, lpcPath);
        std::snprintf(lacValue, sizeof(lacValue), "%d", lrSettings.miEnvironmentMapLod);
        WritePrivateProfileStringA("Graphics", "EnvironmentMapLOD", lacValue, lpcPath);
        WritePrivateProfileStringA("Graphics", "TrafficShadows", lrSettings.mbTrafficShadows ? "1" : "0", lpcPath);
        std::snprintf(lacValue, sizeof(lacValue), "%d", lrSettings.miAnisotropicFiltering);
        WritePrivateProfileStringA("Graphics", "AnisotropicFiltering", lacValue, lpcPath);
        std::snprintf(lacValue, sizeof(lacValue), "%d", lrSettings.miShadowResolutionScale);
        WritePrivateProfileStringA("Graphics", "ShadowResolutionScale", lacValue, lpcPath);
        std::snprintf(lacValue, sizeof(lacValue), "%.9g", lrSettings.mfShadowDistance);
        WritePrivateProfileStringA("Graphics", "ShadowDistance", lacValue, lpcPath);
        std::snprintf(lacValue, sizeof(lacValue), "%.9g", lrSettings.mfShadowSlopeBias);
        WritePrivateProfileStringA("Graphics", "ShadowSlopeBias", lacValue, lpcPath);
        std::snprintf(lacValue, sizeof(lacValue), "%d", lrSettings.miWorldLodOverrideDistance);
        WritePrivateProfileStringA("Graphics", "WorldLODOverrideDistance", lacValue, lpcPath);
        std::snprintf(lacValue, sizeof(lacValue), "%d", lrSettings.miPropLodOverrideDistance);
        WritePrivateProfileStringA("Graphics", "PropLODOverrideDistance", lacValue, lpcPath);
        WritePrivateProfileStringA("Graphics", "VehicleLODPreset", VehicleLodPresetNamePC(lrSettings.meVehicleLodPreset), lpcPath);
        // Inactive custom keys are left intact so changing presets does not erase them.
        if (lrSettings.meVehicleLodPreset == E_VEHICLE_LOD_CUSTOM)
        {
            for (u32 luLod = 0; luLod < 5u; ++luLod)
            {
                char lacKey[32];
                std::snprintf(lacKey, sizeof(lacKey), "VehicleLOD%uDistance", luLod);
                std::snprintf(lacValue, sizeof(lacValue), "%.9g", lrSettings.mafVehicleLodDistances[luLod]);
                WritePrivateProfileStringA("Graphics", lacKey, lacValue, lpcPath);
            }
        }
    }
}
