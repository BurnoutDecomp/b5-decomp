#pragma once

#include "pc/input/AdaptiveTriggers.h"
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cstdio>

// FLAG PC-platform leaf: strict parser for config.ini [AdaptiveTriggers].
namespace CgsInput::AdaptiveTriggers
{
    inline bool Equal(const char* lpA, const char* lpB)
    {
        while (*lpA && *lpB)
            if (std::tolower(static_cast<unsigned char>(*lpA++)) !=
                std::tolower(static_cast<unsigned char>(*lpB++))) return false;
        return *lpA == *lpB;
    }

    inline unsigned ParseSettings(const char* lpSection, Settings& lrSettings)
    {
        lrSettings = {};
        unsigned luInvalid = 0;
        for (const char* lpEntry = lpSection; lpEntry && *lpEntry; lpEntry += std::strlen(lpEntry) + 1)
        {
            const char* lpEquals = std::strchr(lpEntry, '=');
            if (!lpEquals || lpEquals - lpEntry >= 64) { ++luInvalid; continue; }
            char lacKey[64]{};
            std::memcpy(lacKey, lpEntry, lpEquals - lpEntry);
            char* lpEndKey = lacKey + std::strlen(lacKey);
            while (lpEndKey > lacKey && std::isspace(static_cast<unsigned char>(lpEndKey[-1]))) *--lpEndKey = 0;
            char* lpKey = lacKey;
            while (std::isspace(static_cast<unsigned char>(*lpKey))) ++lpKey;
            const char* lpValue = lpEquals + 1;
            while (std::isspace(static_cast<unsigned char>(*lpValue))) ++lpValue;
            char lacValue[128]{};
            if (std::strlen(lpValue) >= sizeof(lacValue)) { ++luInvalid; continue; }
            std::strcpy(lacValue, lpValue);
            char* lpEndValue = lacValue + std::strlen(lacValue);
            while (lpEndValue > lacValue && std::isspace(static_cast<unsigned char>(lpEndValue[-1]))) *--lpEndValue = 0;
            if (Equal(lpKey, "Preset"))
            {
                if (Equal(lacValue, "Off")) lrSettings.mePreset = Preset::Off;
                else if (Equal(lacValue, "Light")) lrSettings.mePreset = Preset::Light;
                else if (Equal(lacValue, "Standard")) lrSettings.mePreset = Preset::Standard;
                else if (Equal(lacValue, "Immersive")) lrSettings.mePreset = Preset::Immersive;
                else ++luInvalid;
                continue;
            }
            errno = 0;
            char* lpTail = nullptr;
            const long liValue = std::strtol(lacValue, &lpTail, 10);
            if (!*lacValue || *lpTail || errno == ERANGE) { ++luInvalid; continue; }
            if (Equal(lpKey, "Resistance") || Equal(lpKey, "Texture") ||
                Equal(lpKey, "LeftStrength") || Equal(lpKey, "RightStrength"))
            {
                const float lfValue = static_cast<float>(std::clamp(liValue, 0l, 100l)) / 100.0f;
                if (Equal(lpKey, "Resistance")) lrSettings.mfResistance = lfValue;
                else if (Equal(lpKey, "Texture")) lrSettings.mfTexture = lfValue;
                else if (Equal(lpKey, "LeftStrength")) lrSettings.mfLeftStrength = lfValue;
                else lrSettings.mfRightStrength = lfValue;
                if (liValue < 0 || liValue > 100) ++luInvalid;
            }
            else if (Equal(lpKey, "Enabled") || Equal(lpKey, "ArcadeCues") || Equal(lpKey, "Diagnostics"))
            {
                if (liValue != 0 && liValue != 1) { ++luInvalid; continue; }
                if (Equal(lpKey, "Enabled")) lrSettings.mbEnabled = liValue != 0;
                else if (Equal(lpKey, "ArcadeCues")) lrSettings.mbArcadeCues = liValue != 0;
                else lrSettings.mbDiagnostics = liValue != 0;
            }
            else
            {
                bool lbFound = false;
                for (unsigned i = 1; i < static_cast<unsigned>(Cue::Count) && !lbFound; ++i)
                {
                    auto& lrTuning = lrSettings.maCues[i];
                    const char* lpCue = GetCueKey(static_cast<Cue>(i));
                    for (const char* lpField : {"Strength", "Frequency", "Duration", "StartPosition"})
                    {
                        char lacName[64]{};
                        std::snprintf(lacName, sizeof(lacName), "%s%s", lpCue, lpField);
                        if (!Equal(lpKey, lacName)) continue;
                        lbFound = true;
                        const long liMin = Equal(lpField, "StartPosition") ? -1 : Equal(lpField, "Frequency") ? 25 : 0;
                        const long liMax = Equal(lpField, "StartPosition") ? 9 : 200;
                        const long liClamped = std::clamp(liValue, liMin, liMax);
                        if (liValue != liClamped) ++luInvalid;
                        if (Equal(lpField, "StartPosition")) lrTuning.miStartPosition = static_cast<int>(liClamped);
                        else if (Equal(lpField, "Strength")) lrTuning.mfStrength = liClamped / 100.0f;
                        else if (Equal(lpField, "Frequency")) lrTuning.mfFrequency = liClamped / 100.0f;
                        else lrTuning.mfDuration = liClamped / 100.0f;
                        break;
                    }
                }
                if (!lbFound) ++luInvalid;
            }
        }
        return luInvalid;
    }
}
