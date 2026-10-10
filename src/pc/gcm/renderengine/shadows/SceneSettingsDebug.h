#pragma once
#include "pc/gcm/renderengine/shadows/SceneSettings.h"
#include "pc/gcm/renderengine/reflections/SceneSettingsDebug.h"
namespace CgsPC::Shadows
{
    inline void RegisterSettings(CgsDev::DebugInterface& lrDebug)
    {
        Reflections::RegisterObjectSettings(lrDebug, SmallObjects(), "World/ShadowMap/Small objects", true);
        lrDebug.RegisterVariable(&SmallObjectRadius(), "World/ShadowMap/Small objects", "Maximum radius");
        lrDebug.SetRange(&SmallObjectRadius(), 0.01f, 20.0f);
        lrDebug.SetStep(&SmallObjectRadius(), 0.1f);
        Reflections::RegisterObjectSettings(lrDebug, Debris(), "World/ShadowMap/Solid debris", false);
    }
}
