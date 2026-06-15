#include "GameSource/Game/BrnGlobalCpuMonitors.h"

namespace BrnGame
{
    // Zero every perfmon handle. The real Construct registers each monitor with
    // CgsDev::PerfMonCpu::AddMonitor; until that registry is reconstructed the handles start
    // inert (0), which the no-op Start/StopMonitor accept.
    void BrnCpuMonitors::Construct()
    {
        s32* lpHandle = &miUT_TotalUpdate;
        for (u32 luIndex = 0; luIndex < sizeof(BrnCpuMonitors) / sizeof(s32); ++luIndex)
            lpHandle[luIndex] = 0;
    }
}
