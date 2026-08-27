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

    // ============================================================================================
    // GetWheel (by owning entity + wheel ordinal) @0x825E8308 (198 insns) -- BODIED 2026-08-27
    // (detach-3 wave). IDA leaves it `sub_825E8308`; the identity is pinned by its one caller
    // (DeformableObject::DoDetachedWheelWorldContactGeneration passes the car's mGlobalEntityId and
    // a 0..3 wheel ordinal and uses the result as a PhysicalWheel*) and by the DWARF declaration
    // BrnDetachedWheelManager.h:112 this header already carried BODYLESS.
    //
    // A BitArray<20> walk over mUsedWheels (the manager+2960 field the sibling IsSlotUsed reads),
    // taking the first slot that matches on ALL THREE of, read off 0x825E83C8..0x825E8420:
    //   1. maWheelOwnerType[slot] == lEntityId.GetOwner()       (`lwzx` at this + 4*(slot+0x2D0),
    //      i.e. this+2880 stride 4, vs `srwi r10, r14, 24`)
    //   2. the wheel handle's entity INDEX == lEntityId.GetEntityIndex()
    //      (`ld 0x70(&maWheels[slot]) ; srdi 32 ; srwi 10 ; xor ; clrlwi 18` -- an xor/mask
    //       equality on the 14-bit field, which is a compare)
    //   3. the wheel handle's PART index == liWheelOrdinal     (`clrlwi r10, r10, 22` == & 0x3FF)
    // and returning nullptr when the walk runs out (`li r3, 0`).
    //
    // The "invalid index : N < 20" strings the function materialises belong to the inlined
    // GetNextNonZeroBit tripwire (CgsBitArray.h:203), which lives in the committed BitArray, so it
    // is not restated here.
    //
    // The handle is read through GetVolumeInstanceId() rather than the private mWheelBodyId: it
    // packs the same three fields into the same 64-bit word the console `ld`s, so the high dword is
    // bit-for-bit the entity word the asm shifts out.
    // ============================================================================================
    const PhysicalWheel* DetachedWheelManager::GetWheel(EntityId lEntityId, s32 liWheelOrdinal) const
    {
        const CgsSceneManager::EntityId lQueryId(lEntityId.muValue);

        for (s32 liSlot = mUsedWheels.GetFirstNonZeroBit();
             liSlot != -1;
             liSlot = mUsedWheels.GetNextNonZeroBit(liSlot))
        {
            if (static_cast<u32>(maWheelOwnerType[liSlot]) != static_cast<u32>(lQueryId.GetOwner()))
            {
                continue;
            }

            const CgsSceneManager::EntityId lWheelId(
                static_cast<u32>(maWheels[liSlot].GetVolumeInstanceId().muId >> 32));

            if (lWheelId.GetEntityIndex() != lQueryId.GetEntityIndex())
            {
                continue;
            }
            if (static_cast<s32>(lWheelId.GetPartIndex()) != liWheelOrdinal)
            {
                continue;
            }

            return &maWheels[liSlot];
        }

        return nullptr;
    }

}
}
