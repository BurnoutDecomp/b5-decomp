// Embed check for the rw::audio::core plug-in family homes -- forces the headers to
// be parsed/instantiated standalone (no .cpp dependency) and exercises the templated
// System::New2 helper so its instantiation compiles.
#include "rw/audio/core/PlugIn.h"
#include "rw/audio/core/Iir2.h"

namespace
{
void EmbedCheck()
{
    using namespace rw::audio::core;
    sizeof(PlugIn);
    sizeof(PlugInDescRunTime);
    sizeof(PlugInRegistry);
    sizeof(PlugInSetAttributeCommand);
    sizeof(System);
    sizeof(Iir2State);
    sizeof(Iir2Coeffs);

    PlugInRegistry *reg = 0;
    System::New2<PlugInRegistry>(0, &reg, 0, 24, 16, 0);
    (void)reg;
}
}
