// ============================================================================
// GameSource/Physics/DeformationManager/DeformationPhysics/BrnDetachedWheelManager_Accessors.cpp
//
// BrnPhysics::Deformation::DetachedWheelManager -- the two pool accessors the contact-spy
// bridge slice links against (FixupWheelVehicleContact @0x825A0D98):
//     GetWheel   @ 0x825A0B10
//     IsSlotUsed @ 0x825A0A10
// MOVED VERBATIM 2026-08-06 (bridge de-facade wave) out of the still-unmounted
// BrnDetachedWheelManager.cpp (whose RemoveWheel/UpdateTriangleCache tail has its own open
// closure). Fold back into the home TU when it mounts.
// ============================================================================

#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDetachedWheelManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnPhysics
{
namespace Deformation
{
    // ============================================================================================
    // GetWheel (raw pool slot) @ 0x825A0B10  (dossier "Get")
    //
    //   if ( !IsSlotUsed(luSlot) ) assert "IsSlotUsed(luSlot)"   (non-gating)
    //   return &maWheels[luSlot]                                 (asm `144*luSlot + this`)
    //
    // The frozen header types the return as const PhysicalWheel* -- the asm's `144*a2 + a1` is the
    // address of the slot's PhysicalWheel record (144 == the console stride; here &maWheels[slot]).
    // ============================================================================================
    const PhysicalWheel* DetachedWheelManager::GetWheel(u16 lu16Slot)
    {
        CGS_ASSERT(IsSlotUsed(lu16Slot), "IsSlotUsed(luSlot)");
        return &maWheels[lu16Slot];
    }

    // ============================================================================================
    // IsSlotUsed @ 0x825A0A10
    //
    //   if ( luSlot >= 20 ) assert "invalid index : 20 < 20"-shaped CgsBitArray.h:203 tripwire
    //                       (non-gating; the StrStream "invalid index : N < 20" diagnostic)
    //   return ((1 << (luSlot & 0x3F)) & mUsedWheels.field[luSlot>>6]) != 0   == mUsedWheels.IsBitSet
    //
    // The Hex-Rays tail `return ((*&a1 << SBYTE3(v14)) & v14) != 0` is the inlined IsBitSet bit-test
    // on the 64-bit field at +2960 (`*&v4[2*(luSlot>>6) + 740]`). Expressed through the canonical
    // BitArray::IsBitSet, which produces the identical bit.
    // ============================================================================================
    bool DetachedWheelManager::IsSlotUsed(u16 lu16Slot)
    {
        CGS_ASSERT(lu16Slot < KI_MAX_DETACHED_WHEELS, "invalid index : ");
        return mUsedWheels.IsBitSet(static_cast<u32>(lu16Slot));
    }

}
}
