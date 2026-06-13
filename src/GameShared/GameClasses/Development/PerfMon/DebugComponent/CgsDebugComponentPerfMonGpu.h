#pragma once

#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsDev::DebugComponentPerfMonGpu::GetName @ 0x828177F0  -> "GPU Monitors"
//   CgsDev::DebugComponentPerfMonGpu::GetPath @ 0x82817800  -> "Core/Debug/Performance"
//
// Debug-UI identity hooks for the GPU performance monitor component. Header-keyed
// TU: the recovered members are defined inline on a minimal standalone class.

namespace CgsDev
{
    class DebugComponentPerfMonGpu
    {
    public:
        const char* GetName() const { return "GPU Monitors"; }
        const char* GetPath() const { return "Core/Debug/Performance"; }
    };
}
