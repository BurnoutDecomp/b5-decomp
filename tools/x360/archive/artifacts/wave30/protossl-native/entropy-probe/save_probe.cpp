#include "types.hpp"
extern "C" __declspec(naked) u32 EntropyProbe()
{
    __asm { mflr r12 }
    __asm { stw r12, -8(r1) }
    __asm { blr }
}
