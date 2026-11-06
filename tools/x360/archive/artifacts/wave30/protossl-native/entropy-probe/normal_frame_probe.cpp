#include "types.hpp"
#include "cryptarc4.h"
extern "C" u32 NetTick();
static u32 seed[4] = {0,0,0,0};
extern "C" __declspec(naked) __declspec(noinline) u32 SampleStack(u32)
{
    __asm {
        mr r10, r3
        li r11, 0
        stw r11, 80(r1)
    sample_loop:
        slwi r9, r11, 2
        addi r8, r1, 80
        addi r11, r11, 1
        cmpwi cr6, r11, 32
        lwzx r9, r9, r8
        stw r11, 80(r1)
        add r10, r9, r10
        blt cr6, sample_loop
        mr r3, r10
        blr
    }
}
extern "C" void EntropyProbe(u8* buffer, s32 length, CryptArc4T* arc4)
{
    if (!seed[0]) seed[0] = NetTick();
    u32 tick = NetTick();
    ++seed[2];
    seed[1] += tick;
    seed[3] = SampleStack(seed[3]);
    if (buffer)
    {
        CryptArc4Init(arc4, reinterpret_cast<const u8*>(seed), 16, 3);
        CryptArc4Apply(arc4, buffer, length);
    }
}
