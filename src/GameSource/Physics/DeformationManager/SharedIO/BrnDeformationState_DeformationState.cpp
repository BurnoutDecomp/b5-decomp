#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationState.h"
#include <cstddef>   // offsetof

// X360-attested shape (DeformationState::GetCarStateF @ 0x822CC340): the per-car record array
// base is this+0 with a 1712-byte stride, the car-id table is at this+47936, and the live-slot
// BitArray is at this+48048. (The private-member offset pins are checked in _AssertLayout below,
// which has member access; the public element stride is asserted here.)
static_assert(sizeof(BrnPhysics::Deformation::CarStateRecord) == 0x6B0, "CarStateRecord stride 1712");

// BrnPhysics::Deformation::DeformationState::GetCarStateF @ 0x822CC340
// Reconstructed from BURNOUT_X360_ARTIST.XEX (asm authoritative). The deformation manager keeps
// a fixed pool of up to KU_MAX_DEFORMATION_MODELS(28) per-car deformation records; a parallel
// car-id table records which car owns each slot, and a BitArray marks the live slots. This is the
// "find the live record owning this car" lookup:
//   - GetFirstNonZeroBit() finds the first live slot (the X360 inlines the BitArray's
//     field*64 + (63 - clz64(field & -field)) lowest-set-bit idiom; the committed
//     BitArray<28>::GetFirstNonZeroBit reproduces it value-identically, returning -1 when none
//     or when the computed bit is out of range -- matching the asm's `>= 28 || < 0` -> return 0).
//   - for each live slot it compares maCarIds[slot] (a 32-bit word at this + 47936 + 4*slot)
//     against luCarId; on a match it returns &maCarStates[slot] (this + 1712*slot).
//   - GetNextNonZeroBit(slot) advances to the next live slot (the asm's within-field bit walk
//     + field-crossing), returning -1 when the pool is exhausted -> the function returns nullptr.
//   - each visited slot index is asserted < KU_MAX_DEFORMATION_MODELS (the X360 baked
//     "liCarLoop < (int32_t)KU_MAX_DEFORMATION_MODELS", BrnDeformationState.h:109) -- a
//     non-gating tripwire that never trips for a valid bit index.
// The X360 return is a pointer (ABI-returned record address); modelled as the CarStateRecord* the
// member-by-name access yields, nullptr for the not-found case (the asm's `li r3,0`).

namespace BrnPhysics
{
namespace Deformation
{
    void DeformationState::_AssertLayout()
    {
        static_assert(offsetof(DeformationState, maCarStates) == 0, "maCarStates @0");
        static_assert(offsetof(DeformationState, maCarIds) == 47936, "maCarIds @47936");
        static_assert(offsetof(DeformationState, mxLiveSlots) == 48048, "mxLiveSlots @48048");
    }

    CarStateRecord* DeformationState::GetCarStateF(u32 luCarId)
    {
        s32 liSlot = mxLiveSlots.GetFirstNonZeroBit();
        while (liSlot >= 0)
        {
            CGS_ASSERT(liSlot < static_cast<s32>(KU_MAX_DEFORMATION_MODELS),
                       "liCarLoop < (int32_t)KU_MAX_DEFORMATION_MODELS");
            if (maCarIds[liSlot] == luCarId)
            {
                return &maCarStates[liSlot];
            }
            liSlot = mxLiveSlots.GetNextNonZeroBit(liSlot);
        }
        return nullptr;
    }
}
}
