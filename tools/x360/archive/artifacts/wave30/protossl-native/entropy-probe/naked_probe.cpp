#include "types.hpp"
extern "C" __declspec(naked) u32 EntropyProbe()
{
    __asm { li r3, 0 }
    __asm { blr }
}
