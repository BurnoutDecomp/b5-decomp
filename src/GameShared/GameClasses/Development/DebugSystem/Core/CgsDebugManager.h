#pragma once

#include "types.hpp"

// CgsDev::DebugManager - owns the in-game debug systems (perfmon overlays, debug menus,
// console, on-screen variables). Update() ticks them with the frame delta time. Only the
// per-frame entry point the game module's update spine calls is declared here; the full
// manager (construct parameters, pools, menu/console subsystems) is its own TU. Recovered
// from the DecFIGS DWARF (Development/DebugSystem/Core/CgsDebugManager.h).
namespace CgsDev
{
    class DebugManager
    {
    public:
        DebugManager();
        ~DebugManager();

        void Update(f32 lfDeltaTime);
    };
}
