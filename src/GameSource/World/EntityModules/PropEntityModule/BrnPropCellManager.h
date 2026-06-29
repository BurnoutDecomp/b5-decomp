#pragma once

// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/BrnPropCellManager.h
//
// BrnWorld::PropCellManager -- the prop streaming/cell manager embedded BY VALUE at
// offset 0 of BrnWorld::PropZoneManager. PropZoneManager forwards almost every
// scene/sim/contact-generation operation to it (AddCells/RemoveCells/Add/Remove
// PropFromScene/Sim/ContactGeneration, etc.), so PropZoneManager needs this type's
// COMPLETE layout (for the by-value embed) and the called-method DECLARATIONS.
//
// LAYOUT (sizeof == 2432, asm-authoritative): PropZoneManager places maProps right
// after mCellManager (the X360 Construct sets mpaProps = this + 2432), so
// sizeof(PropCellManager) == 2432. The member offsets below are pinned by the X360
// PropZoneManager::Construct stores (0x822F0568):
//     miNumLoadedCells   @ +1900   (stw 0)
//     mpaProps           @ +1904   (stw this+2432  -> &PropZoneManager::maProps[0])
//     mpaPropParts       @ +1908   (stw this+435968)
//     mPhysicalProps     @ +1912   (std 0, BitArray<15> -> 1 u64)
//     mPhysicalParts     @ +1920   (std 0, BitArray<30> -> 1 u64)
//     ... per-frame physical params arrays + target list ...
//     miSizeOfTargetList @ +2304   (stw 0)
//     miNumPropsInSim    @ +2308   (stw 0)
//     miNumPartsInSim    @ +2312   (stw 0)
//     mu16NumberOfPropVolumesInScene   @ +2316 (sth 0)
//     mu16NumberOfPropEntitiesInScene  @ +2318 (sth 0)
// The leading maCells[150] PropCellRecord span (DWARF BrnPropCellManager.h:309) fills
// [0, 1900); its per-record sub-layout is owned by the PropCellManager TU and is NOT
// re-derived here -- it is modelled as a sized opaque span so the embed size is exact
// without forking PropCellRecord's interior (recovered by its own TU).
//
// Member names/types are the DecFIGS DWARF (BrnPropCellManager.h), X360-gated.
#include "types.hpp"
#include "BrnPropEntityInstance.h"   // PropEntityInstance, PropPartEntityInstance (mpaProps/mpaPropParts)
#include "GameShared/GameClasses/Containers/CgsBitArray.h"  // CgsContainers::BitArray
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"    // PropEntityID, PropVolumeInstanceID
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModuleIO.h" // InSceneUpdateInterface

namespace BrnPhysics { namespace Props {
    class  PropPhysicsDataHeader;
    class  PropTypeData;
    struct PropZoneData;
    class  PropInputInterface;
}}

namespace BrnWorld
{
    namespace PropEntityIO { class OutputBuffer_PreScene; }

    struct PropCellManager
    {
    public:
        // ---- methods PropZoneManager calls (X360-attested; bodies = own TU) ----
        // Each takes the prop/part instance pointer, its PropTypeData, a packed
        // PropVolumeInstanceID, and the relevant scene/prop/contact interface.
        typedef BrnPhysics::Props::PropTypeData             PropTypeData;
        typedef BrnPhysics::Props::PropZoneData             PropZoneData;
        typedef BrnPhysics::Props::PropPhysicsDataHeader    PropPhysicsDataHeader;
        typedef BrnPhysics::Props::PropInputInterface       PropInputInterface;
        typedef CgsSceneManager::SceneManagerIO::InSceneUpdateInterface InSceneUpdateInterface;

        // BrnPropCellManager.h:96 -- wire the cell manager to its owner's prop/part pools
        // and zero its per-frame bookkeeping (inlined into PropZoneManager::Construct).
        void Construct(PropEntityInstance* lpaProps, PropPartEntityInstance* lpaPropParts);

        void AddCells(const PropZoneData* lpZoneData, s32 liStartIndex);
        void RemoveCells(s16 li16ZoneId, s32 liStartIndex, s32 liNumInstances,
                         const PropPhysicsDataHeader* lpTypes, void* lpRecentlyBroken,
                         PropEntityIO::OutputBuffer_PreScene* lpOutput);

        // Scene / contact-gen / sim operations. The shipped X360 build passes the prop's
        // zone id and its index-within-zone alongside the DWARF's 4 logical params (the
        // PropZoneManager forwarders compute liPropIndexInZone = volInstId.GetEntityIndex()
        // - mauStartIndexOfZone[zoneId]); kept in the signature so the forwarders are
        // faithful named calls (no offset poking).
        void AddPropToScene(PropEntityInstance* lpProp, const PropTypeData* lpType,
                            PropVolumeInstanceID lVolumeInstanceID, InSceneUpdateInterface* lpScene,
                            u16 lu16ZoneId, s32 liPropIndexInZone);
        void RemovePropFromScene(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                 PropVolumeInstanceID lVolumeInstanceID, InSceneUpdateInterface* lpScene,
                                 u16 lu16ZoneId, s32 liPropIndexInZone);
        void AddPropPartsToScene(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                 PropVolumeInstanceID lVolumeInstanceID, InSceneUpdateInterface* lpScene,
                                 u16 lu16ZoneId, s32 liPropIndexInZone);
        void RemovePropPartsFromScene(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                      PropVolumeInstanceID lVolumeInstanceID, InSceneUpdateInterface* lpScene,
                                      u16 lu16ZoneId, s32 liPropIndexInZone);

        void RemovePropFromContactGeneration(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                             PropVolumeInstanceID lVolumeInstanceID, InSceneUpdateInterface* lpScene);
        void RemovePropPartsFromContactGeneration(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                                  PropVolumeInstanceID lVolumeInstanceID, InSceneUpdateInterface* lpScene);

        void RemovePropFromSim(PropEntityInstance* lpProp, const PropTypeData* lpType,
                               PropVolumeInstanceID lVolumeInstanceID, PropInputInterface* lpPropInput);
        void RemovePropPartsFromSimIfPhysical(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                              PropVolumeInstanceID lVolumeInstanceID,
                                              PropEntityIO::OutputBuffer_PreScene* lpOutput);

        // Physical-slot bookkeeping called by UpdateInstance (per-part time-in-sim /
        // free-slot). The float is the elapsed time step.
        void FreePhysicalPropSlot(s32 liPhysicsIndex);
        void FreePhysicalPartSlot(s32 liPhysicsIndex);
        void IncrementPropsTimeInSim(u32 luPhysicsIndex, f32 lfTimeStep);
        void IncrementPartsTimeInSim(u32 luPhysicsIndex, f32 lfTimeStep);

        // BrnPropCellManager.h:302 -- per-physical-slot bookkeeping record.
        struct PhysicalParams
        {
            PropEntityID mEntityId;   // :304  (4B packed handle)
            f32          mfTimeInSim; // :305
        };

    public:
        // ---- DWARF-faithful layout (offsets asm-pinned by PropZoneManager::Construct) ----
        // maCells[150] PropCellRecord fills [0,1900); interior owned by PropCellManager TU.
        u8                          maCells[1900];        // +0     PropCellRecord[150] (opaque span)
        s32                         miNumLoadedCells;     // +1900
        PropEntityInstance*         mpaProps;             // +1904  -> PropZoneManager::maProps
        PropPartEntityInstance*     mpaPropParts;         // +1908  -> PropZoneManager::maParts
        CgsContainers::BitArray<15u> mPhysicalProps;      // +1912  (1 u64)
        CgsContainers::BitArray<30u> mPhysicalParts;      // +1920  (1 u64)
        PhysicalParams              maPhysicalPropParams[15]; // +1928  (15*8=120) -> 2048
        PhysicalParams              maPhysicalPartParams[30]; // +2048  (30*8=240) -> 2288
        u32                         maTargetList[4];      // +2288  PropCellId[4] (4B each) -> 2304
        s32                         miSizeOfTargetList;   // +2304
        s32                         miNumPropsInSim;      // +2308
        s32                         miNumPartsInSim;      // +2312
        u16                         mu16NumberOfPropVolumesInScene;   // +2316
        u16                         mu16NumberOfPropEntitiesInScene;  // +2318
        u8                          mu8TailPad[104];      // trailing align/reserve
        // NOTE on size: the CONSOLE sizeof is 2432 (4-byte pointers; PropZoneManager's
        // Construct sets mpaProps = this + 2432). On our x64 PC compile the two member
        // pointers (mpaProps/mpaPropParts) are 8 bytes each, so the PC sizeof is wider by
        // 8 bytes than the console value; we do NOT byte-match (semantic parity, not
        // byte-matching). The type stays a self-consistent sized block embedded by value.
    };
}
