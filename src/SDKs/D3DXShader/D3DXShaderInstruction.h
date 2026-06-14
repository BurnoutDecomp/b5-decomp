#pragma once

#include "types.hpp"

namespace D3DXShader
{
class Instruction
{
public:
    explicit Instruction(u32 luOpcode);

private:
    u8  mPad0[8];
    u32 muToken;
    u8  mPad1[32];
};
}
