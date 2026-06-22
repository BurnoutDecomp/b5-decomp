#include "GameShared/GameClasses/Graphics/CgsModel.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsModel.cpp - the three CgsGraphics::Model accessors bodied store-for-store from
// the X360 asm (GetRenderable @0x822A0AC8, DoesStateExist @0x822A0B60,
// GetLodDistance @0x822A0C00). The DWARF source paths report these at
// GameShared/GameClasses/Graphics/CgsModel.h:309/310, :344 and :367 (the asserts
// are inline accessors in the header on console); they are gathered into this TU.
//
// All member reads are BY NAME; offsets quoted in CgsModel.h match the asm
// (lbz 0x10/0x12, lwz 0/4/8 of `this`).

namespace CgsGraphics
{
    // GetRenderable @ 0x822A0AC8
    //   lbz   r11, 0x12(this)            ; mu8NumStates
    //   cmpw  leState, r11 ; assert leState < mu8NumStates
    //   lwz   r11, 4(this) ; lbzx r11, r11, leState  -> mpu8StateRenderableIndices[leState]
    //   assert !(index == 255 || index >= mu8NumRenderables)  -> "State does not exist"
    //   lwz   r11, 4(this) ; lwz r10, 0(this)
    //   lbzx  r11, r11, leState ; rotlwi r11, r11, 2 ; lwzx r3, r11, r10
    //     -> mppRenderables[index]  (rotlwi by 2 == *4 byte stride of a 32-bit pointer table)
    const Renderable* Model::GetRenderable(State leState) const
    {
        CGS_ASSERT(leState < mu8NumStates, "leState < mu8NumStates");

        const u32 luIndex = mpu8StateRenderableIndices[leState];
        CGS_ASSERT(!(luIndex == 255u || luIndex >= mu8NumRenderables), "State does not exist");

        return mppRenderables[mpu8StateRenderableIndices[leState]];
    }

    // DoesStateExist @ 0x822A0B60
    //   cmpwi leState, 0x20 ; assert leState < E_STATE_COUNT
    //   lbz   r10, 0x12(this)                 ; mu8NumStates
    //   r11 = mu8NumStates - 1 ; if (leState < mu8NumStates - 1) r11 = leState
    //     -> clamped index = min(leState, mu8NumStates - 1)
    //   lwz   r9, 4(this) ; lbzx r11, r9, r11 ; r11 -= 255 ; cntlzw ; extract bit ->
    //     bit set iff index == 255 ; xori 1 -> (index != 255)
    //   result = (leState < mu8NumStates) & (clampedIndex != 255)
    bool Model::DoesStateExist(State leState) const
    {
        CGS_ASSERT(leState < E_STATE_COUNT, "leState < E_STATE_COUNT");

        u32 luClampedIndex = mu8NumStates - 1u;
        if (static_cast<u32>(leState) < mu8NumStates - 1u)
        {
            luClampedIndex = static_cast<u32>(leState);
        }

        const bool lbInRange = static_cast<u32>(leState) < mu8NumStates;
        const bool lbUsed = mpu8StateRenderableIndices[luClampedIndex] != 255u;
        return lbInRange && lbUsed;
    }

    // GetLodDistance @ 0x822A0C00
    //   lbz   r11, 0x12(this) ; cmplw luLodIndex, r11 ; assert luLodIndex < mu8NumStates
    //   lwz   r11, 8(this) ; slwi r10, luLodIndex, 2 ; lfsx f1, r10, r11
    //     -> mpfLodDistances[luLodIndex]  (single-precision load; returned in f1)
    f32 Model::GetLodDistance(u32 luLodIndex) const
    {
        CGS_ASSERT(luLodIndex < mu8NumStates, "Invalid LOD index");

        return mpfLodDistances[luLodIndex];
    }
}
