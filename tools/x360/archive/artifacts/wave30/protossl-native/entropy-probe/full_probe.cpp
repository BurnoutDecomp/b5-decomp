#include "types.hpp"
#include "cryptarc4.h"
extern "C" u32 NetTick();
struct RandomContext { u32 seed[4]; u32 (*tick)(); void (*init)(CryptArc4T*,const u8*,s32,s32); void (*apply)(CryptArc4T*,u8*,s32); };
static RandomContext gRandom = {{0,0,0,0}, &NetTick, &CryptArc4Init, &CryptArc4Apply};
extern "C" __declspec(noinline) RandomContext* ProtoSSLRandomContext() { return &gRandom; }
extern "C" __declspec(naked) void ProtoSSLGenerateRandom(u8*,s32,CryptArc4T*)
{
    __asm {
        mflr r12
        std r27, -56(r1)
        std r28, -40(r1)
        std r29, -32(r1)
        std r30, -24(r1)
        std r31, -16(r1)
        stw r12, -8(r1)
        stwu r1, -128(r1)
        mr r29, r3
        mr r28, r4
        mr r30, r5
    }
    ProtoSSLRandomContext();
    __asm {
        mr r27, r3
        mr r31, r3
        lwz r11, 0(r31)
        cmplwi cr6, r11, 0
        bne cr6, seeded
        lwz r12, 16(r27)
        mtctr r12
        bctrl
        stw r3, 0(r31)
    seeded:
        lwz r12, 16(r27)
        mtctr r12
        bctrl
        lwz r11, 8(r31)
        addi r11, r11, 1
        stw r11, 8(r31)
        lwz r11, 4(r31)
        add r11, r3, r11
        stw r11, 4(r31)
        li r11, 0
        lwz r10, 12(r31)
        stw r11, 80(r1)
    loop_seed:
        slwi r9, r11, 2
        addi r8, r1, 80
        addi r11, r11, 1
        cmpwi cr6, r11, 32
        lwzx r9, r9, r8
        stw r11, 80(r1)
        add r10, r9, r10
        blt cr6, loop_seed
        stw r10, 12(r31)
        cmplwi cr6, r29, 0
        beq cr6, done
        li r6, 3
        li r5, 16
        mr r4, r31
        mr r3, r30
        lwz r12, 20(r27)
        mtctr r12
        bctrl
        mr r5, r28
        mr r4, r29
        mr r3, r30
        lwz r12, 24(r27)
        mtctr r12
        bctrl
    done:
        addi r1, r1, 128
        lwz r12, -8(r1)
        mtlr r12
        ld r27, -56(r1)
        ld r28, -40(r1)
        ld r29, -32(r1)
        ld r30, -24(r1)
        ld r31, -16(r1)
        blr
    }
}
