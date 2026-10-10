#pragma once

// FLAG PC-platform leaf: native quality options without a debug-variable equivalent.
// Registered engine controls use the generic [Debug] section instead.
#include "types.hpp"
#include <Windows.h>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace renderengine
{
    struct GraphicsSettingsPC
    {
        s32 miAnisotropicFiltering = 1;
        s32 miShadowResolutionScale = 1;
        f32 mfShadowDistance = 120.0f;
        f32 mfShadowSlopeBias = 0.0f;
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
        const s32 liAnisotropy = ReadGraphicsIntPC(lpcPath, "AnisotropicFiltering", 1, 1, 16);
        lrSettings.miAnisotropicFiltering = (liAnisotropy & (liAnisotropy - 1)) == 0 ? liAnisotropy : 1;
        lrSettings.miShadowResolutionScale = ReadGraphicsIntPC(lpcPath, "ShadowResolutionScale", 1, 1, 2);
        lrSettings.mfShadowDistance = ReadGraphicsFloatPC(lpcPath, "ShadowDistance", 120.0f, 30.0f, 500.0f);
        lrSettings.mfShadowSlopeBias = ReadGraphicsFloatPC(lpcPath, "ShadowSlopeBias", 0.0f, 0.0f, 4.0f);
    }

    inline void SaveGraphicsSettingsPC(const char* lpcPath)
    {
        const GraphicsSettingsPC& lrSettings = GetGraphicsSettingsPC();
        char lacValue[128];
        std::snprintf(lacValue, sizeof(lacValue), "%d", lrSettings.miAnisotropicFiltering);
        WritePrivateProfileStringA("Graphics", "AnisotropicFiltering", lacValue, lpcPath);
        std::snprintf(lacValue, sizeof(lacValue), "%d", lrSettings.miShadowResolutionScale);
        WritePrivateProfileStringA("Graphics", "ShadowResolutionScale", lacValue, lpcPath);
        std::snprintf(lacValue, sizeof(lacValue), "%.9g", lrSettings.mfShadowDistance);
        WritePrivateProfileStringA("Graphics", "ShadowDistance", lacValue, lpcPath);
        std::snprintf(lacValue, sizeof(lacValue), "%.9g", lrSettings.mfShadowSlopeBias);
        WritePrivateProfileStringA("Graphics", "ShadowSlopeBias", lacValue, lpcPath);
    }
}
