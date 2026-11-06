#include "types.hpp"
extern "C" u32 gEntropyProbe[4];
extern "C" u32 NetTick();
extern "C" void __savegprlr_28();
extern "C" void __restgprlr_28();
extern "C" __declspec(naked) u32 EntropyProbe()
{
    __asm { mflr r12 }
    __asm { bl __savegprlr_28 }
    __asm { stwu r1, -128(r1) }
    __asm { lis r31, gEntropyProbe@ha }
    __asm { addi r31, r31, gEntropyProbe@l }
    __asm { bl NetTick }
    __asm { addi r1, r1, 128 }
    __asm { b __restgprlr_28 }
}
