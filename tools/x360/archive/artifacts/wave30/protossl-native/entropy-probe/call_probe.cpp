#include "types.hpp"
extern "C" u32 NetTick();
extern "C" __declspec(naked) u32 EntropyProbe()
{
    __asm { mflr r12 }
    __asm { bl NetTick }
    __asm { blr }
}
