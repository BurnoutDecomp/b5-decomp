#include "rw/audio/core/PlugIn.h"
void ProfilerShutdownProbe(rw::audio::core::System *system) {
    rw::audio::core::System::GetProfiler(system);
    rw::audio::core::System::Release(system);
}

