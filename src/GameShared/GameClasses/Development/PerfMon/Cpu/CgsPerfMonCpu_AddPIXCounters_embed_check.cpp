// Embed check for CgsDev::PerfMonCpu::AddPIXCounters @ 0x828172E8.
// Forces the home header to compile and references the bodied func so its
// signature stays wired to its home.
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"

namespace
{
    void ExerciseAddPIXCounters()
    {
        // Walks the (possibly empty) live registry and emits PIX counters.
        CgsDev::PerfMonCpu::AddPIXCounters();
    }
}

extern void CgsPerfMonCpu_AddPIXCounters_embed_check_anchor();
void CgsPerfMonCpu_AddPIXCounters_embed_check_anchor() { ExerciseAddPIXCounters(); }
