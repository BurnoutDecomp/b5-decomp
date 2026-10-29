#pragma once

#include "types.hpp"

namespace renderengine
{
    struct ProgramVariableHandle
    {
        u8 mu8RegisterSet;
        u8 mu8RegisterIndex;
        u8 mu8ShaderType;
        u8 mu8RegisterCount;
    };
}
