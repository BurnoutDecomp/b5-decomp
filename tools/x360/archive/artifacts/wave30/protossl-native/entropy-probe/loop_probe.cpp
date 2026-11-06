#include "types.hpp"
extern "C" __declspec(naked) void EntropyProbe()
{
    __asm {
        li r11, 0
    loop_start:
        addi r11, r11, 1
        cmpwi cr6, r11, 32
        blt cr6, loop_start
        blr
    }
}
