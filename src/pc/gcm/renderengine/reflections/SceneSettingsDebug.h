#pragma once

#include "pc/gcm/renderengine/reflections/SceneSettings.h"
#include "pc/gcm/renderengine/reflections/ReflectionLodDebug.h"

namespace CgsPC::Reflections
{
    // FLAG PC-platform leaf: expose the capture policy through the engine's
    // existing debug registry, shared with the [Debug] INI and console.
    inline void RegisterObjectSettings(CgsDev::DebugInterface& lrDebug, ObjectSettings& lrSettings, const char* lpcPath, bool lbMesh)
    {
        static const CgsDev::DebugUI::StringList KA_DISTANCE_MODES[] = {
            {E_DISTANCE_RELATIVE, "Relative"}, {E_DISTANCE_FIXED, "Fixed"}, {0, nullptr}};
        lrDebug.RegisterVariable(&lrSettings.mbEnabled, lpcPath, "Enabled");
        lrDebug.RegisterVariable(&lrSettings.miDistanceMode, lpcPath, "Draw distance mode");
        lrDebug.SetRange(&lrSettings.miDistanceMode, 0, 1);
        lrDebug.SetOptions(&lrSettings.miDistanceMode, KA_DISTANCE_MODES);
        lrDebug.RegisterVariable(&lrSettings.mfDrawDistance, lpcPath, "Draw distance");
        lrDebug.SetRange(&lrSettings.mfDrawDistance, 0.0f, 10000.0f);
        lrDebug.SetStep(&lrSettings.mfDrawDistance, 5.0f);
        lrDebug.RegisterVariable(&lrSettings.mfDrawDistanceScale, lpcPath, "Draw distance scale");
        lrDebug.SetRange(&lrSettings.mfDrawDistanceScale, 0.001f, 10.0f);
        lrDebug.SetStep(&lrSettings.mfDrawDistanceScale, 0.05f);
        if (lbMesh)
        {
            lrDebug.RegisterVariable(&lrSettings.miFixedLod, lpcPath, "Fixed LOD");
            lrDebug.SetRange(&lrSettings.miFixedLod, 0, 2);
            renderengine::RegisterEnvironmentMapLodSettingsPC(lrDebug, lrSettings.mLod, lpcPath);
        }
    }

    inline void RegisterSceneSettings(CgsDev::DebugInterface& lrDebug)
    {
        RegisterObjectSettings(lrDebug, Backdrops(), "World/Reflections/Backdrops", true);
        RegisterObjectSettings(lrDebug, Traffic(), "World/Reflections/Traffic", true);
        RegisterObjectSettings(lrDebug, Rivals(), "World/Reflections/Rivals", true);
        RegisterObjectSettings(lrDebug, Wheels(), "World/Reflections/Wheels", true);
        RegisterObjectSettings(lrDebug, PlayerWheels(), "World/Reflections/Player wheels", true);
        RegisterObjectSettings(lrDebug, Glass(), "World/Reflections/Glass", true);
        RegisterObjectSettings(lrDebug, Lights(), "World/Reflections/Lights", false);
        RegisterObjectSettings(lrDebug, Particles(), "World/Reflections/Particles", false);
        lrDebug.RegisterVariable(&IncludePlayerParticles(), "World/Reflections/Particles", "Include player effects");
        RegisterObjectSettings(lrDebug, Decals(), "World/Reflections/Decals", false);
    }
}
