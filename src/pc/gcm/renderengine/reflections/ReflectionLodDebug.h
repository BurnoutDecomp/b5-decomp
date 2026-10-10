#pragma once

#include "pc/gcm/renderengine/reflections/ReflectionLod.h"
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"

namespace renderengine
{
    // FLAG PC-platform leaf: register optional reflection settings with the real
    // engine debug menu/INI registry, without adding another configuration copy.
    inline void RegisterEnvironmentMapLodSettingsPC(CgsDev::DebugInterface& lrDebugInterface,
        EnvironmentMapLodSettingsPC& lrSettings, const char* lpcPath)
    {
        static const CgsDev::DebugUI::StringList KA_MODE_OPTIONS[] =
        {
            {E_ENVIRONMENT_MAP_LOD_FIXED, "Fixed"},
            {E_ENVIRONMENT_MAP_LOD_RELATIVE, "Relative"},
            {E_ENVIRONMENT_MAP_LOD_CUSTOM, "Custom"},
            {0, nullptr}
        };
        lrDebugInterface.RegisterVariable(&lrSettings.miMode, lpcPath, "LOD mode");
        lrDebugInterface.SetRange(&lrSettings.miMode, 0, 2);
        lrDebugInterface.SetOptions(&lrSettings.miMode, KA_MODE_OPTIONS);
        lrDebugInterface.RegisterVariable(&lrSettings.mfDistanceScale, lpcPath, "LOD distance scale");
        lrDebugInterface.SetRange(&lrSettings.mfDistanceScale, 0.001f, 10.0f);
        lrDebugInterface.SetStep(&lrSettings.mfDistanceScale, 0.05f);
        static const char* const KAPC_DISTANCE_NAMES[] = {"LOD0 distance", "LOD1 distance"};
        for (u32 luLod = 0; luLod < 2u; ++luLod)
        {
            lrDebugInterface.RegisterVariable(&lrSettings.mafTransitionDistances[luLod], lpcPath, KAPC_DISTANCE_NAMES[luLod]);
            lrDebugInterface.SetRange(&lrSettings.mafTransitionDistances[luLod], 0.01f, 10000.0f);
            lrDebugInterface.SetStep(&lrSettings.mafTransitionDistances[luLod], 1.0f);
        }
    }
}
