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
// LAYOUT (console sizeof == 2432, asm-authoritative): PropZoneManager places maProps
// right after mCellManager (the X360 Construct sets mpaProps = this + 2432). The member
// offsets below are pinned by the X360 PropZoneManager::Construct stores (0x822F0568)
// plus the per-member reads in IsCellLoaded/AddCells/ActivateCell/DeactivateCell/
// RecordPropPositions:
//     maCells[150]       @ +0      (PropCellRecord, asm stride 0xC -> [0,1800))
//     miNumLoadedCells   @ +1800   (asm 0x708; AddCells/IsCellLoaded/RemoveCells/Update)
//     maActiveCells[8]   @ +1804   (asm 0x70C; ActivateCell appends, DeactivateCell
//                                   swap-erases, RecordPropPositions walks it)
//     miNumActiveCells   @ +1900   (asm 0x76C; ActivateCell asserts < 8)
//     mpaProps           @ +1904   (stw this+2432  -> &PropZoneManager::maProps[0])
//     mpaPropParts       @ +1908   (stw this+435968)
//     mPhysicalProps     @ +1912   (std 0, BitArray<15> -> 1 u64)
//     mPhysicalParts     @ +1920   (std 0, BitArray<30> -> 1 u64)
//     ... per-frame physical params arrays + target list ...
//     miSizeOfTargetList @ +2304   (stw 0; asm 0x900)
//     miNumPropsInSim    @ +2308   (stw 0; asm 0x904)
//     miNumPartsInSim    @ +2312   (stw 0; asm 0x908)
//     mu16NumberOfPropVolumesInScene   @ +2316 (sth 0; asm 0x90C)
//     mu16NumberOfPropEntitiesInScene  @ +2318 (sth 0; asm 0x90E)
//
// ⚠️ X360-vs-PC OFFSETS: everything up to (but excluding) mpaProps is pointer-free, so
// those console offsets ARE the host offsets and are pinned by _AssertLayout() below.
// From mpaProps on, the two member pointers widen 4->8 bytes on the x64 host, so the
// console byte offsets no longer hold and are recorded as provenance only. All bodies
// index BY NAME -- never by a transcribed console constant.
//
// Member names/types are the DecFIGS DWARF (BrnPropCellManager.h), X360-gated. The X360
// build additionally carries maActiveCells/miNumActiveCells, which the (older) PS3 DWARF
// does not list -- a merge-window delta; they are attested by the ARTIST asm above.
#include "types.hpp"
#include "BrnCommonTypes.h"          // Vector3, VecFloat, Matrix44Affine
#include "BrnPropEntityInstance.h"   // PropEntityInstance, PropPartEntityInstance (mpaProps/mpaPropParts)
#include "GameShared/GameClasses/Containers/CgsBitArray.h"  // CgsContainers::BitArray
#include "GameShared/GameClasses/Containers/CgsSet.h"       // CgsContainers::Set (RecentlyBrokenPropsArray)
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"    // PropEntityID, PropVolumeInstanceID
#include "SharedClasses/Physics/Props/BrnPhysicsPropZoneData.h" // PropCellId, PropCellData, PropZoneData
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModuleIO.h" // InSceneUpdateInterface

namespace BrnPhysics { namespace Props {
    class  PropPhysicsDataHeader;
    class  PropTypeData;
    struct PropInputInterface;
}}

namespace BrnReplays { class PropEntitySerialiser; }

namespace BrnWorld
{
    namespace PropEntityIO { class OutputBuffer_PreScene; }

    // ------------------------------------------------------------------------
    // BrnPropCellManager.h:45 (DWARF) -- one loaded cell's registry entry. 12 bytes
    // (asm stride 0xC in IsCellLoaded / AddCells / ActivateCell / DeactivateCell /
    // RemoveCells / RecordPropPositions). Pointer-free, so the console layout IS the
    // host layout (pinned by PropCellRecord_AssertLayout below).
    //
    // ⚠️ CELL-ID CONVENTION: the field at +0 is a BrnPhysics::Props::PropCellId, i.e.
    // TWO u16s { muX @+0, muZ @+2 } -- NOT a u32. The X360 compares it member-wise
    // (IsCellLoaded @0x822A4130 does `lhz` at +0 and +2 and compares each half
    // separately; Update/DeactivateCell do the same), so a u32 view would be
    // endianness-dependent (big-endian (X<<16)|Z vs little-endian (Z<<16)|X). Modelling
    // it as the real PropCellId struct and comparing with PropCellId::operator== is
    // exactly what the asm does and is endianness-independent.
    //
    // ⚠️ The field at +8 is the owning ZONE ID (DWARF `int16_t mi16ZoneId`), NOT padding:
    // AddCells @0x822A9F5C/0x822A9F88 does `lhz r9,0x18(lpZoneData)` (PropZoneData::
    // muZoneId) then `sth r9,8(newRecord)`, and RemoveCells @0x822FC828 reads it back
    // sign-extended to match the zone being unloaded.
    struct PropCellRecord
    {
    public:
        // BrnPropCellManager.h:52 -- populate a registry entry from a streamed
        // PropCellData. Inlined by the X360 into AddCells (0x822A9F34..0x822A9F90):
        // the stored [start,end) span is the cell's zone-local range biased by the
        // zone's base slot, i.e. start = liStartIndex + cellData.GetStartIndex() and
        // end = start + cellData.GetCount(). (`Constuct` is the original's spelling.)
        void Constuct(s16 li16ZoneId, s16 li16StartIndex,
                      const BrnPhysics::Props::PropCellData* lpCellData)
        {
            mu16StartIndex = static_cast<u16>(lpCellData->GetStartIndex() + li16StartIndex);
            mu16EndIndex   = static_cast<u16>(lpCellData->GetCount() + mu16StartIndex);
            mCellId        = lpCellData->GetId();
            mi16ZoneId     = li16ZoneId;
            mbActive       = false;
        }

        BrnPhysics::Props::PropCellId GetId() const { return mCellId; }        // :55
        s16  GetZoneId() const      { return mi16ZoneId; }                     // :58
        u16  GetStartIndex() const  { return mu16StartIndex; }                 // :61
        u16  GetEndIndex() const    { return mu16EndIndex; }                   // :64
        void SetActive(bool lbActive) { mbActive = lbActive; }                 // :68
        bool IsActive() const       { return mbActive; }                       // :71

    private:
        BrnPhysics::Props::PropCellId mCellId;        // +0  :74  { u16 muX, u16 muZ }
        u16 mu16StartIndex;                           // +4  :75  first prop slot in mpaProps
        u16 mu16EndIndex;                             // +6  :76  one-past-last prop slot
        s16 mi16ZoneId;                               // +8  :77  owning zone id
        bool mbActive;                                // +10 :78  cell currently activated
        // +11 trailing pad -> sizeof 12
    };

    // Pin the asm-attested PropCellRecord interior (stride 0xC, zone id at +8).
    inline void PropCellRecord_AssertLayout()
    {
        static_assert(sizeof(BrnPhysics::Props::PropCellId) == 4, "PropCellId is two u16");
        static_assert(sizeof(PropCellRecord) == 12,               "PropCellRecord stride 0xC");
    }

    struct PropCellManager
    {
    public:
        typedef BrnPhysics::Props::PropTypeData             PropTypeData;
        typedef BrnPhysics::Props::PropZoneData             PropZoneData;
        typedef BrnPhysics::Props::PropCellData             PropCellData;
        typedef BrnPhysics::Props::PropCellId               PropCellId;
        typedef BrnPhysics::Props::PropPhysicsDataHeader    PropPhysicsDataHeader;
        typedef BrnPhysics::Props::PropInputInterface       PropInputInterface;
        typedef CgsSceneManager::SceneManagerIO::InSceneUpdateInterface InSceneUpdateInterface;
        // BrnPropCellManager.h:39 (DWARF typedef; the DWARF spells the template
        // CgsContainers::Set, the committed container lives at global scope).
        typedef ::Set<PropEntityID, 32u>                    RecentlyBrokenPropsArray;

        // ---- asm-pinned capacities / tuning constants --------------------------------
        static const s32 KI_MAX_CELLS        = 150;   // AddCells assert `< 0x96`
        static const s32 KI_MAX_ACTIVE_CELLS = 8;     // ActivateCell assert `< 8`
        static const s32 KI_MAX_TARGET_CELLS = 4;     // maTargetList[4] (DWARF :324)
        static const u32 KU_MAX_PHYSICAL_PROPS = 15;  // BitArray<15>
        static const u32 KU_MAX_PHYSICAL_PARTS = 30;  // BitArray<30>
        // AddPropToScene / AddPropPartsToScene overflow guards (`cmplwi 0x1D4C`, `0xC80`).
        static const u32 KU_MAX_PROP_ENTITIES_IN_SCENE = 7500;  // 0x1D4C
        static const u32 KU_MAX_PROP_VOLUMES_IN_SCENE  = 3200;  // 0xC80
        // Scene entity-type flag word the prop AddEntity pushes (`li r5, 0x490`).
        static const u32 KU_PROP_SCENE_ENTITY_TYPE_FLAG = 0x490;
        // AddPropToScene transform sanity bound (`flt_8201D2BC` == 15000.0f).
        static const f32 KF_MAX_VALID_POSITION_ALONG_AXIS;

        // BrnPropCellManager.h:96 -- wire the cell manager to its owner's prop/part pools
        // and zero its per-frame bookkeeping (inlined into PropZoneManager::Construct).
        void Construct(PropEntityInstance* lpaProps, PropPartEntityInstance* lpaPropParts);

        void AddCells(const PropZoneData* lpZoneData, s32 liStartIndex);

        // X360 @0x822FC7A8. The shipped build carries the replay serialiser + the zone's
        // prop start index alongside the DWARF's 4 logical params, because DeactivateCell
        // needs them to compute each prop's index-within-zone.
        void RemoveCells(s16 li16ZoneId, const PropPhysicsDataHeader* lpTypes,
                         RecentlyBrokenPropsArray* lpRecentlyBroken,
                         PropEntityIO::OutputBuffer_PreScene* lpOutput,
                         BrnReplays::PropEntitySerialiser* lpSerialiser,
                         u16 lu16ZoneStartIndex);

        // X360 @0x822FC8F0 -- the per-frame cell sweep PropEntityModule::PreSceneUpdate
        // drives: rebuild the target list around the player, then activate/deactivate
        // every loaded cell to match it.
        //
        // ⭐ SIGNATURE GROWN 2026-08-18 (wave Q keystone). The committed form had SIX
        // parameters; the shipped X360 function takes SEVEN. The missing one is `r7`, and
        // the sole call site pins what it is:
        //   PreSceneUpdate @0x8230ADF0  `lbzx r7, r30, 0xD3334`  == PropEntityModule::mbInReplay
        //   PreSceneUpdate @0x8230ADF4  `bl BrnWorld__PropCellManager__Update`
        // with r8 = &mPropEntitySerialiser (var_2EC, the same local ClearPropsNearPosition
        // takes in its own r7) and r9 = &mZoneManager.mauStartIndexOfZone. Inserting the flag
        // at position 5 is what makes r8/r9 land on lpSerialiser/lpuZoneStartIndices instead of
        // being shifted one slot left -- i.e. the six-parameter form silently mis-bound the
        // last two arguments at every call site that ever gets written.
        //
        // ⚠️ MEASURED: the shipped BODY NEVER READS r7. Its prologue moves r4->r28, r5->r25,
        // r6->r27, r8->r26, r9->r24 and leaves r7 untouched (0x822FC8F0..0x822FC970), and no
        // later instruction reads r7 before the first call clobbers it. The parameter is
        // therefore dead in the shipped build; it is declared because the CALL SITE passes it
        // and the argument positions after it depend on it -- NOT because the body needs it.
        void Update(Vector3 lv3Position, const PropPhysicsDataHeader* lpTypes,
                    RecentlyBrokenPropsArray* lpRecentlyBroken,
                    PropEntityIO::OutputBuffer_PreScene* lpOutput,
                    bool lbInReplay,
                    BrnReplays::PropEntitySerialiser* lpSerialiser,
                    const u16* lpuZoneStartIndices);

        // X360 @0x822E1988 (ledger `reviewed`, but it had NO declaration and NO body anywhere
        // in the tree until wave Q -- see the spec's "ledger-reviewed != implemented" list).
        // The replay RECORD-side snapshot PropEntityModule::PostPhysicsUpdate drives once per
        // frame while the serialiser is recording: clear the frame's eight prop/part arrays,
        // then, for each of the miNumActiveCells entries of maActiveCells, append the cell id
        // to the frame's 4-slot cell array ("muLength < MaxLength", BrnReplayArray.h:98) and
        // walk that cell's [GetStartIndex(),GetEndIndex()) run of mpaProps recording every
        // prop that carries KU_ADDED_TO_SCENE_BIT.
        //
        // SIGNATURE from the 0x822E1988 prologue: r3 = this, r4 = the PropEntitySerialiser
        // (immediately fed to GetStaticLayout), r5 = the prop type table. The caller
        // (PostPhysicsUpdate @0x823032F8) passes r3 = module+0x280 == &mZoneManager == this
        // (PropCellManager is at offset 0 of PropZoneManager), r4 = module+0xD3180 ==
        // &mPropEntitySerialiser, r5 = mpPropPhysicsDataHeader.GetMemoryResource().
        void RecordPropPositions(BrnReplays::PropEntitySerialiser* lpSerialiser,
                                 const PropPhysicsDataHeader* lpTypes);

        // X360 @0x822C6068 -- the 2x2 block of cells around lv3Position.
        void GenerateTargetList(Vector3 lv3Position);

        // Scene / contact-gen / sim operations. The shipped X360 build passes the replay
        // serialiser, the prop's zone id and its index-within-zone alongside the DWARF's
        // 4 logical params (the PropZoneManager forwarders compute liPropIndexInZone =
        // volInstId.GetEntityIndex() - mauStartIndexOfZone[zoneId]); kept in the signature
        // so the forwarders are faithful named calls (no offset poking).
        void AddPropToScene(PropEntityInstance* lpProp, const PropTypeData* lpType,
                            PropVolumeInstanceID lVolumeInstanceID, InSceneUpdateInterface* lpScene,
                            BrnReplays::PropEntitySerialiser* lpSerialiser,
                            u16 lu16ZoneId, s32 liPropIndexInZone);
        void RemovePropFromScene(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                 PropVolumeInstanceID lVolumeInstanceID, InSceneUpdateInterface* lpScene,
                                 BrnReplays::PropEntitySerialiser* lpSerialiser,
                                 u16 lu16ZoneId, s32 liPropIndexInZone);
        void AddPropPartsToScene(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                 PropVolumeInstanceID lVolumeInstanceID, InSceneUpdateInterface* lpScene,
                                 BrnReplays::PropEntitySerialiser* lpSerialiser,
                                 u16 lu16ZoneId, s32 liPropIndexInZone);
        void RemovePropPartsFromScene(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                      PropVolumeInstanceID lVolumeInstanceID, InSceneUpdateInterface* lpScene,
                                      BrnReplays::PropEntitySerialiser* lpSerialiser,
                                      u16 lu16ZoneId, s32 liPropIndexInZone);

        void AddPropToContactGeneration(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                        PropVolumeInstanceID lVolumeInstanceID, InSceneUpdateInterface* lpScene);
        void AddPropPartsToContactGeneration(PropEntityInstance* lpProp, PropPartEntityInstance* lpFirstPart,
                                             const PropTypeData* lpType, PropVolumeInstanceID lVolumeInstanceID,
                                             InSceneUpdateInterface* lpScene);
        void RemovePropFromContactGeneration(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                             PropVolumeInstanceID lVolumeInstanceID, InSceneUpdateInterface* lpScene);
        void RemovePropPartsFromContactGeneration(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                                  PropVolumeInstanceID lVolumeInstanceID, InSceneUpdateInterface* lpScene);

        void RemovePropFromSim(PropEntityInstance* lpProp, const PropTypeData* lpType,
                               PropVolumeInstanceID lVolumeInstanceID, PropInputInterface* lpPropInput);
        void RemovePropPartsFromSimIfPhysical(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                              PropVolumeInstanceID lVolumeInstanceID,
                                              PropEntityIO::OutputBuffer_PreScene* lpOutput);

        // X360 @0x822DFCB8 / @0x822F1940 -- push a loaded cell's props into / out of
        // contact generation (and, on deactivate, out of the sim and the scene).
        void ActivateCell(PropCellRecord* lpCell, const PropPhysicsDataHeader* lpTypes,
                          InSceneUpdateInterface* lpScene);
        void DeactivateCell(PropCellRecord* lpCell, const PropPhysicsDataHeader* lpTypes,
                            RecentlyBrokenPropsArray* lpRecentlyBroken,
                            PropEntityIO::OutputBuffer_PreScene* lpOutput,
                            BrnReplays::PropEntitySerialiser* lpSerialiser,
                            s32 liZoneId, u16 lu16ZoneStartIndex);

        // X360 @0x822A9FA8 -- would this type's part volumes still fit the scene budget?
        bool CanAddPartVolumes(const PropTypeData* lpType);

        // X360 @0x822E11C0 / @0x822E0DB0 -- claim a physical prop/part slot (may evict
        // the longest-resident one).
        bool GetPhysicalPropSlot(PropEntityID lEntityId, s32& lriSlot,
                                 PropEntityID& lrEvictedId, bool& lrbEvicted);
        bool GetPhysicalPartSlot(PropEntityID lEntityId, s32& lriSlot,
                                 PropEntityID& lrEvictedId, bool& lrbEvicted);

        // Physical-slot bookkeeping called by UpdateInstance (per-part time-in-sim /
        // free-slot). The float is the elapsed time step.
        void FreePhysicalPropSlot(s32 liPhysicsIndex);
        void FreePhysicalPartSlot(s32 liPhysicsIndex);
        void IncrementPropsTimeInSim(u32 luPhysicsIndex, f32 lfTimeStep);
        void IncrementPartsTimeInSim(u32 luPhysicsIndex, f32 lfTimeStep);

        // @ 0x822BBE08 -- resolve a prop-part entity id to its part instance:
        // &mpaPropParts[ liEntityId.GetPartIndex() + mpaProps[liEntityId.GetEntityIndex()]
        // .mu16PartsIndex - 1 ]. Called by AddPropPartsToScene.
        PropPartEntityInstance* GetPart(PropEntityID lEntityId);

        // @ 0x822A4130 -- linear scan of maCells[0..miNumLoadedCells) for lCellId.
        bool IsCellLoaded(PropCellId lCellId) const;

        // @ 0x822E1600 -- per-frame sweep (PropEntityModule::PreSceneUpdate). For every
        // LOADED cell whose id is currently in maTargetList, tests each prop in the cell's
        // [start,end) slot range against a sphere centred on lv3Position with radius
        // (lvClearRadius + the prop type's bounding radius). A prop that overlaps, is not a
        // traffic light, and is not one of three special street-furniture graphics ids is
        // removed from the sim (sim-resident states), from contact generation (if the
        // ADDED_TO_CONTACT_GEN flag is set) and from the scene (if ADDED_TO_SCENE is set).
        // lvClearRadius is a broadcast VecFloat (all lanes equal). Parameter ORDER follows
        // the DWARF (BrnPropCellManager.h:296); the shipped X360 build appends the replay
        // serialiser + the per-zone start-index table the scene removal needs.
        void ClearPropsNearPosition(Vector3 lv3Position, VecFloat lvClearRadius,
                                    const PropPhysicsDataHeader* lpTypes,
                                    PropInputInterface* lpPropInput,
                                    InSceneUpdateInterface* lpScene,
                                    BrnReplays::PropEntitySerialiser* lpSerialiser,
                                    const u16* lpuZoneStartIndices);

        // BrnPropCellManager.h:302 -- per-physical-slot bookkeeping record.
        struct PhysicalParams
        {
            PropEntityID mEntityId;   // :304  (4B packed handle)
            f32          mfTimeInSim; // :305
        };

    public:
        // ---- DWARF-faithful layout (offsets asm-pinned by PropZoneManager::Construct) ----
        PropCellRecord              maCells[150];         // +0     [0,1800)
        s32                         miNumLoadedCells;     // +1800  (asm 0x708)
        // The X360-only activated-cell registry: ActivateCell copies a whole PropCellRecord
        // in at maActiveCells[miNumActiveCells++] (asm 0x70C base, 6x `sth` = a 12-byte
        // struct copy), DeactivateCell swap-erases it, RecordPropPositions walks it to
        // snapshot each active cell's prop range into the replay serialiser.
        PropCellRecord              maActiveCells[8];     // +1804  [1804,1900)
        s32                         miNumActiveCells;     // +1900  (asm 0x76C)
        PropEntityInstance*         mpaProps;             // +1904  (asm 0x770) -> PropZoneManager::maProps
        PropPartEntityInstance*     mpaPropParts;         // +1908  (asm 0x774) -> PropZoneManager::maParts
        CgsContainers::BitArray<15u> mPhysicalProps;      // +1912  (asm 0x778, 1 u64)
        CgsContainers::BitArray<30u> mPhysicalParts;      // +1920  (asm 0x780, 1 u64)
        PhysicalParams              maPhysicalPropParams[15]; // +1928  (15*8=120) -> 2048
        PhysicalParams              maPhysicalPartParams[30]; // +2048  (30*8=240) -> 2288
        PropCellId                  maTargetList[4];      // +2288  (asm 0x8F0, 4B each) -> 2304
        s32                         miSizeOfTargetList;   // +2304  (asm 0x900)
        s32                         miNumPropsInSim;      // +2308  (asm 0x904)
        s32                         miNumPartsInSim;      // +2312  (asm 0x908)
        u16                         mu16NumberOfPropVolumesInScene;   // +2316 (asm 0x90C)
        u16                         mu16NumberOfPropEntitiesInScene;  // +2318 (asm 0x90E)
        u8                          mu8TailPad[104];      // trailing align/reserve
        // NOTE on size: the CONSOLE sizeof is 2432 (4-byte pointers; PropZoneManager's
        // Construct sets mpaProps = this + 2432). On our x64 PC compile the two member
        // pointers (mpaProps/mpaPropParts) are 8 bytes each, so the PC sizeof is wider by
        // 8 bytes than the console value; we do NOT byte-match (semantic parity, not
        // byte-matching). The type stays a self-consistent sized block embedded by value.

        // Pin the pointer-free head of the record, whose console offsets ARE the host
        // offsets (everything before mpaProps). offsetof inside a never-called member
        // function compiles even for a private/complex aggregate.
        static void _AssertLayout();
    };
}
