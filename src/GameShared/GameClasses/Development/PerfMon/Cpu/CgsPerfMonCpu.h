#pragma once

#include "types.hpp"

// CgsDev::PerfMonCpu - the CPU performance-monitor registry. Monitors are referenced by an
// int32 handle (returned by AddMonitor); Start/StopMonitor bracket a timed region and
// SetNumIterationsTaken records how many sub-iterations a frame ran. Reconstructed from the
// DecFIGS DWARF (Development/PerfMon/Cpu/CgsPerfMonCpu.h). Only the entry points the
// per-frame update spine uses are declared here; the registry bodies are their own TU.
namespace CgsDev
{
    namespace PerfMonCpu
    {
        void SetNumIterationsTaken(s32 liNumIterations);
        void StartMonitor(s32 liMonitorHandle);
        void StopMonitor(s32 liMonitorHandle);
    }
}
