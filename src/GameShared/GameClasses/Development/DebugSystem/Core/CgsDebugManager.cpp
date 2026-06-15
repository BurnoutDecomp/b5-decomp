#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"

// CgsDev::DebugManager - the in-game debug systems. The full manager (perfmon overlays,
// debug menus, console, on-screen variables) is its own TU; for the boot/loading path the
// per-frame tick is a no-op so the update spine links and runs.
namespace CgsDev
{
    DebugManager::DebugManager() {}
    DebugManager::~DebugManager() {}

    void DebugManager::Update(f32) {}
}
