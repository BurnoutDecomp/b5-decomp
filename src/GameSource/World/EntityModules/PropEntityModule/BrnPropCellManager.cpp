// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/BrnPropCellManager.cpp
//
// BrnWorld::PropCellManager -- the prop streaming/cell manager embedded by value at
// offset 0 of BrnWorld::PropZoneManager.
//
// This TU (class:BrnWorld::PropCellManager) homes the physical-slot bookkeeping and
// lookup helpers of the cell manager: the free/free-slot + time-in-sim bit-array
// maintenance, the cell-loaded scan, and the part-instance resolver. The larger
// scene/sim traversals (AddCells/RemoveCells/Add*/Remove* and the per-frame
// ClearPropsNearPosition / RecordPropPositions sweeps) live in the sibling file TU
// (BrnPropCellManager.cpp file-keyed) and are not written here.
//
// Source-of-truth: the X360 ARTIST asm is authoritative (per-function addresses noted
// on each body). The inlined container bounds asserts (CgsBitArray.h:203 "invalid
// index", :241 "luIndex < NUMBITS") are emitted at these call sites because the shared
// CgsBitArray.h keeps its bodies assert-free (the assert-system dependency lives at the
// caller). They are reproduced verbatim; file/line are dropped per house convention.
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropCellManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnWorld
{
    // KU_MAX_LOADED_PROP_INSTANCES == 5400 (0x1518), the asm-pinned entity-index bound
    // fired by GetPart (matches BrnWorld::PropZoneManager::KU_MAX_LOADED_PROP_INSTANCES).
    static const u32 KU_MAX_LOADED_PROP_INSTANCES = 5400;

    // @ 0x822BBE08.
    // result = &mpaPropParts[ (u16)(lEntityId.GetPartIndex()
    //                                + mpaProps[lEntityId.GetEntityIndex()].mu16PartsIndex - 1) ]
    // The asm inlines the PropEntityID owner tripwire three times (once explicitly, once
    // inside the prop-slot fetch, once inside the part-index fetch); reproduced in order.
    PropPartEntityInstance* PropCellManager::GetPart(PropEntityID lEntityId)
    {
        CGS_ASSERT(lEntityId.GetOwner() == E_ENTITYTYPE_PROP,
                   "mEntityId.GetOwner() == E_ENTITYTYPE_PROP");                     // BrnPropEntityID.h:278

        const u32 luEntityIndex = lEntityId.GetEntityIndex();
        CGS_ASSERT(luEntityIndex < KU_MAX_LOADED_PROP_INSTANCES,
                   "lEntityId.GetEntityIndex() < BrnPhysics::Props::KU_MAX_LOADED_PROP_INSTANCES"); // :573

        CGS_ASSERT(lEntityId.GetOwner() == E_ENTITYTYPE_PROP,
                   "mEntityId.GetOwner() == E_ENTITYTYPE_PROP");                     // BrnPropEntityID.h:278

        const u16 lu16FirstPart = mpaProps[luEntityIndex].mu16PartsIndex;           // prop slot field @72

        CGS_ASSERT(lEntityId.GetOwner() == E_ENTITYTYPE_PROP,
                   "mEntityId.GetOwner() == E_ENTITYTYPE_PROP");                     // BrnPropEntityID.h:278

        const u16 lu16PartSlot = static_cast<u16>(lEntityId.GetPartIndex() + lu16FirstPart - 1);
        return &mpaPropParts[lu16PartSlot];
    }

    // @ 0x822A4130. Linear scan of the loaded-cell registry for luCellId.
    bool PropCellManager::IsCellLoaded(u32 luCellId) const
    {
        for (s32 liCell = 0; liCell < miNumLoadedCells; ++liCell)
        {
            if (maCells[liCell].muCellId == luCellId)
            {
                return true;
            }
        }
        return false;
    }

    // @ 0x822BBF10. Release a physical-prop slot: assert the slot is valid + currently set,
    // then clear its bit. The unsigned index compares reproduce the -1 sentinel failing the
    // < 15 bounds path (the CgsBitArray IsBitSet[] / UnSetBit bounds checks, inlined here).
    void PropCellManager::FreePhysicalPropSlot(s32 liPhysicsIndex)
    {
        CGS_ASSERT(liPhysicsIndex != -1, "liProp != -1");                           // BrnPropCellManager.h:611
        CGS_ASSERT(static_cast<u32>(liPhysicsIndex) < 15u, "invalid index : < 15"); // CgsBitArray.h:203
        CGS_ASSERT(mPhysicalProps.IsBitSet(static_cast<u32>(liPhysicsIndex)),
                   "mPhysicalProps.IsBitSet( liProp )");                            // BrnPropCellManager.h:612
        CGS_ASSERT(static_cast<u32>(liPhysicsIndex) < 15u, "luIndex < NUMBITS");    // CgsBitArray.h:241
        mPhysicalProps.UnSetBit(static_cast<u32>(liPhysicsIndex));
    }

    // @ 0x822BC0A0. Release a physical-part slot (30-bit array; asserts liPart != -1).
    void PropCellManager::FreePhysicalPartSlot(s32 liPhysicsIndex)
    {
        CGS_ASSERT(liPhysicsIndex != -1, "liPart!= -1");                            // BrnPropCellManager.h:624
        CGS_ASSERT(static_cast<u32>(liPhysicsIndex) < 30u, "invalid index : < 30"); // CgsBitArray.h:203
        CGS_ASSERT(mPhysicalParts.IsBitSet(static_cast<u32>(liPhysicsIndex)),
                   "mPhysicalParts.IsBitSet( liPart )");                            // BrnPropCellManager.h:625
        CGS_ASSERT(static_cast<u32>(liPhysicsIndex) < 30u, "luIndex < NUMBITS");    // CgsBitArray.h:241
        mPhysicalParts.UnSetBit(static_cast<u32>(liPhysicsIndex));
    }

    // @ 0x822BC230. Accumulate the frame time step into a physical prop's time-in-sim.
    // The asm asserts the < 15 bounds (inlined IsBitSet[]) then that the slot bit is set.
    void PropCellManager::IncrementPropsTimeInSim(u32 luPhysicsIndex, f32 lfTimeStep)
    {
        CGS_ASSERT(luPhysicsIndex < 15u, "invalid index : < 15");                   // CgsBitArray.h:203
        CGS_ASSERT(mPhysicalProps.IsBitSet(luPhysicsIndex),
                   "mPhysicalProps.IsBitSet( liPhysicalPropIndex )");               // BrnPropCellManager.h:639
        maPhysicalPropParams[luPhysicsIndex].mfTimeInSim += lfTimeStep;
    }

    // @ 0x822BC380. Accumulate the frame time step into a physical part's time-in-sim.
    void PropCellManager::IncrementPartsTimeInSim(u32 luPhysicsIndex, f32 lfTimeStep)
    {
        CGS_ASSERT(luPhysicsIndex < 30u, "invalid index : < 30");                   // CgsBitArray.h:203
        CGS_ASSERT(mPhysicalParts.IsBitSet(luPhysicsIndex),
                   "mPhysicalParts.IsBitSet( liPhysicalPartIndex )");               // BrnPropCellManager.h:652
        maPhysicalPartParams[luPhysicsIndex].mfTimeInSim += lfTimeStep;
    }
}
