#include "types.hpp"
extern "C" u32 gEntropyProbe[4];
extern "C" __declspec(naked) void EntropyProbe()
{
    __asm { lis r31, gEntropyProbe }
    __asm { addi r31, r31, gEntropyProbe }
    __asm { blr }
}
