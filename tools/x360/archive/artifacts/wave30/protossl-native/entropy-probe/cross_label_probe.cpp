#include "types.hpp"
extern "C" u32 NetTick();
extern "C" __declspec(naked) void EntropyProbe()
{
    __asm { li r11, 1 }
    __asm { cmpwi cr6, r11, 0 }
    __asm { bne cr6, skip_tick }
    NetTick();
skip_tick:
    __asm { blr }
}
