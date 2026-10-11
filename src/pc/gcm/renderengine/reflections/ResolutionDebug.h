#pragma once
#include "pc/gcm/renderengine/reflections/Resolution.h"
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"

namespace CgsPC::Reflections
{
    inline void StepResolution(void* lpValue, void*)
    {
        auto& liPixels = *static_cast<s32*>(lpValue);
        if (SanitizeResolution(liPixels) == static_cast<u32>(liPixels)) return;
        // The original integer editor adds/subtracts one, even with a string
        // list. Keep this option in pixels while arrows step through its sizes.
        for (s32 liSize = 128; liSize <= 2048; liSize *= 2)
        {
            if (liPixels == liSize + 1) { liPixels = liSize < 2048 ? liSize * 2 : 2048; return; }
            if (liPixels == liSize - 1) { liPixels = liSize > 128 ? liSize / 2 : 128; return; }
        }
        liPixels = static_cast<s32>(SanitizeResolution(liPixels));
    }

    // FLAG PC-platform leaf: use the existing shared menu/INI registry. Targets
    // have no native recreation lifecycle yet, so a change applies next launch.
    inline void RegisterResolution(CgsDev::DebugInterface& lrDebug)
    {
        static const CgsDev::DebugUI::StringList KA_RESOLUTIONS[] = {
            {128, "128 x 128 (original)"}, {256, "256 x 256"}, {512, "512 x 512"},
            {1024, "1024 x 1024"}, {2048, "2048 x 2048"}, {0, nullptr}};
        lrDebug.RegisterVariable(&RequestedResolution(), "World/Reflections", "Resolution (restart)");
        lrDebug.SetRange(&RequestedResolution(), 128, 2048);
        lrDebug.SetOptions(&RequestedResolution(), KA_RESOLUTIONS);
        lrDebug.SetChangeCallback(&RequestedResolution(), &StepResolution, nullptr);
    }
}
