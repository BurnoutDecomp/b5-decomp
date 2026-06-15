#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"

// CgsDev::PerfMonCpu - the per-frame CPU perfmon registry. The full registry (AddMonitor /
// timing / overlay) is its own TU; the per-frame bracketing entry points are no-ops here so
// the update/render spine links and runs (monitor handles are inert until the registry is
// reconstructed).
namespace CgsDev
{
    namespace PerfMonCpu
    {
        void SetNumIterationsTaken(s32) {}
        void StartMonitor(s32) {}
        void StopMonitor(s32) {}
    }
}
