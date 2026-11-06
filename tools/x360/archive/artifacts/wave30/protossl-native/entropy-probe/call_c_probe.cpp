#include "types.hpp"
extern "C" u32 NetTick();
extern "C" __declspec(naked) u32 EntropyProbe()
{
    __asm { stwu r1, -128(r1) }
    NetTick();
    __asm { addi r1, r1, 128 }
    __asm { blr }
}
