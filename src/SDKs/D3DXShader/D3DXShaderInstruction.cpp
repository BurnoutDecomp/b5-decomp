#include "D3DXShaderInstruction.h"

#include <cstring>

namespace D3DXShader
{
Instruction::Instruction(u32 luOpcode)
{
    std::memset(this, 0, sizeof(*this));
    muToken = (muToken & 0xffffc07f) | ((luOpcode << 7) & 0x3f80) | 0x400000;
}
}
