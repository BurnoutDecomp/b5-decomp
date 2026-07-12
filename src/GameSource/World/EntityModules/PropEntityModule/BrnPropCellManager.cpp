// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/BrnPropCellManager.cpp
//
// BrnWorld::PropCellManager -- the prop streaming/cell manager embedded by value at
// offset 0 of BrnWorld::PropZoneManager.
//
// This TU (class:BrnWorld::PropCellManager) homes the physical-slot bookkeeping and
// lookup helpers of the cell manager: the free/free-slot + time-in-sim bit-array
// maintenance, the cell-loaded scan, the part-instance resolver, and the per-frame
// ClearPropsNearPosition sweep. The remaining scene/sim traversals
// (AddCells/RemoveCells/Add*/Remove*) and the RecordPropPositions replay-snapshot sweep
// are not written here: RecordPropPositions stays deferred until the PropSerialiserFrame
// interior (its per-cell/per-prop/per-part replay arrays) is reconstructed -- it cannot be
// homed against the current opaque frame model without raw-offset stores.
//
// Source-of-truth: the X360 ARTIST asm is authoritative (per-function addresses noted
// on each body). The inlined container bounds asserts (CgsBitArray.h:203 "invalid
// index", :241 "luIndex < NUMBITS") are emitted at these call sites because the shared
// CgsBitArray.h keeps its bodies assert-free (the assert-system dependency lives at the
// caller). They are reproduced verbatim; file/line are dropped per house convention.
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropCellManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"  // PropPhysicsDataHeader::GetType, PropTypeData accessors
#include "rw/math/vpu/vector3_operation.h"                          // operator-, MagnitudeSquared (sphere test)

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

    // @ 0x822E1600. Per-frame sweep called by PropEntityModule::PreSceneUpdate. Walks the
    // loaded-cell registry; for each cell whose id is currently in maTargetList and that is
    // loaded, tests every prop in the cell's [start,end) slot range against a sphere
    // centred on lv3Position with radius (lvClearRadius + the type's bounding radius). A prop
    // that overlaps, is not a traffic light, and is not one of three special street-furniture
    // graphics ids is removed from the sim (sim-resident states {STATIC,LEANING,PHYSICAL,
    // SMASHED}), from contact generation (ADDED_TO_CONTACT_GEN flag), and from the scene
    // (ADDED_TO_SCENE flag). The two bounds asserts on the type id are inlined by the asm
    // (reproduced by GetType); the state assert follows. lvClearRadius is a broadcast VecFloat.
    void PropCellManager::ClearPropsNearPosition(const PropPhysicsDataHeader* lpTypes,
                                                 Vector3 lv3Position, VecFloat lvClearRadius,
                                                 PropInputInterface* lpPropInput,
                                                 InSceneUpdateInterface* lpScene,
                                                 u16 lu16ZoneId,
                                                 const u16* lpuZoneStartIndices)
    {
        // Street-furniture prop graphics ids this sweep never clears (asm literals
        // 0x7A4D6 / 0x7A4C2 / 0x7B8C2).
        static const u32 KU_UNCLEARABLE_GRAPHICS_ID_0 = 500950u; // 0x7A4D6
        static const u32 KU_UNCLEARABLE_GRAPHICS_ID_1 = 500930u; // 0x7A4C2
        static const u32 KU_UNCLEARABLE_GRAPHICS_ID_2 = 506050u; // 0x7B8C2

        for (s32 liCell = 0; liCell < miNumLoadedCells; ++liCell)
        {
            const PropCellRecord& lrCell = maCells[liCell];

            // Is this cell's id currently in the target list?
            bool lbInTargetList = false;
            for (s32 liTarget = 0; liTarget < miSizeOfTargetList; ++liTarget)
            {
                if (maTargetList[liTarget] == lrCell.muCellId)
                {
                    lbInTargetList = true;
                    break;
                }
            }

            if (!lrCell.mu8Loaded || !lbInTargetList)
            {
                continue;
            }

            for (s32 liPropSlot = lrCell.mu16StartIndex; liPropSlot < lrCell.mu16EndIndex; ++liPropSlot)
            {
                PropEntityInstance& lrProp = mpaProps[liPropSlot];

                const u32 luTypeId = lrProp.muTypeId;
                const PropTypeData* lpType = lpTypes->GetType(luTypeId); // inlines the two type-id asserts

                CGS_ASSERT(lrProp.mu8State < E_STATE_COUNT, "mu8State < E_STATE_COUNT"); // BrnPropEntityInstance.h:653

                // Only non-smashed props (state < E_SMASHED) are candidates.
                if (lrProp.mu8State >= E_SMASHED)
                {
                    continue;
                }

                // Build the prop's volume-instance id: owner byte == E_ENTITYTYPE_PROP in the
                // top byte of the embedded entity word, then splice in this prop's slot index.
                PropVolumeInstanceID lVolumeInstanceID;
                lVolumeInstanceID.mVolumeInstanceId.muId = static_cast<u64>(E_ENTITYTYPE_PROP) << 56;
                lVolumeInstanceID.SetEntityIndex(static_cast<u16>(liPropSlot));

                // Sphere overlap: (lvClearRadius + type bounding radius)^2 > |pos - propPos|^2.
                const f32 lfCombinedRadius = lvClearRadius.x + lpType->GetBoundingRadius();
                const Vector3 lv3Delta = lv3Position - lrProp.mWorldTransform.Pos();
                if (lfCombinedRadius * lfCombinedRadius <= MagnitudeSquared(lv3Delta))
                {
                    continue;
                }

                if (lpType->IsTrafficLight())
                {
                    continue;
                }

                const u32 luGraphicsId = lpType->GetGraphicsId();
                if (luGraphicsId == KU_UNCLEARABLE_GRAPHICS_ID_0 ||
                    luGraphicsId == KU_UNCLEARABLE_GRAPHICS_ID_1 ||
                    luGraphicsId == KU_UNCLEARABLE_GRAPHICS_ID_2)
                {
                    continue;
                }

                // Sim removal only for the sim-resident states (asm set {1,2,4,6}).
                const u8 lu8State = lrProp.mu8State;
                if (lu8State == E_STATIC || lu8State == E_LEANING ||
                    lu8State == E_PHYSICAL || lu8State == E_SMASHED)
                {
                    RemovePropFromSim(&lrProp, lpType, lVolumeInstanceID, lpPropInput);
                }
                if ((lrProp.mu8Flags & KU_ADDED_TO_CONTACT_GEN_BIT) != 0)
                {
                    RemovePropFromContactGeneration(&lrProp, lpType, lVolumeInstanceID, lpScene);
                }
                if ((lrProp.mu8Flags & KU_ADDED_TO_SCENE_BIT) != 0)
                {
                    // index-within-zone = global prop slot - this prop's zone start index.
                    const s32 liPropIndexInZone =
                        liPropSlot - static_cast<s32>(lpuZoneStartIndices[lrProp.mu16ZoneIndex]);
                    RemovePropFromScene(&lrProp, lpType, lVolumeInstanceID, lpScene,
                                        lu16ZoneId, liPropIndexInZone);
                }
            }
        }
    }
}
