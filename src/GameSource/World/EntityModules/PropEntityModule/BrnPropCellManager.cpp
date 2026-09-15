// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/BrnPropCellManager.cpp
//
// BrnWorld::PropCellManager -- the prop streaming/cell manager embedded by value at
// offset 0 of BrnWorld::PropZoneManager. This TU (class:BrnWorld::PropCellManager) homes
// the cell registry (Construct/AddCells/RemoveCells/Activate/Deactivate/Update/
// GenerateTargetList), the scene add/remove path (AddPropToScene / RemovePropFromScene /
// RemovePropPartsFromScene), the sim-removal path, the physical-slot bookkeeping and the
// per-frame ClearPropsNearPosition sweep.
//
// STILL DEFERRED (declared in the header, bodied by a later pass -- see the per-entry
// reason; nothing here is stubbed or invented):
//   * AddPropPartsToScene @0x822E0758 -- needs the per-part LOCAL OFFSET vector the X360
//     reads at PropPartVolumeGroup +0x00 (`lvx128 v0,r0,groupBase`, then
//     M*offset + M.Pos()). BrnPhysicsPropTypeData.h models that span as the opaque
//     `maReserved0[0x20]` with no accessor, and that header is owned by another slice, so
//     the offset cannot be reached by name yet.
//   * ⭐ NO LONGER DEFERRED (wave Q4, 2026-08-18): AddPropToContactGeneration @0x822DF6C8 /
//     AddPropPartsToContactGeneration @0x822DF9D8 / RemovePropFromContactGeneration
//     @0x822C6318 / RemovePropPartsFromContactGeneration @0x822C6430 are BODIED, in the
//     sibling partfile BrnPropCellManager_wQ4.cpp (a partfile only because this TU was
//     concurrently owned when they landed). Both prerequisites this entry used to name are
//     gone: PropVolumeInstanceID::SetVolumeNumber/GetVolumeNumber now exist
//     (BrnPropEntityID.h, DWARF :181/:199, forwarding to CgsSceneManager::VolumeInstanceId::
//     SetVolumeIndex/GetVolumeIndex), and InSceneUpdateInterface carries the DWARF's own
//     64-bit AddVolumeInstance/AddForCollision alongside the already-present 64-bit
//     RemoveForCollision/RemoveVolumeInstance. See that file's banner for the asm spine.
//   * GetPhysicalPropSlot @0x822E11C0 / GetPhysicalPartSlot @0x822E0DB0 -- the eviction
//     search over maPhysicalPropParams/maPhysicalPartParams; not needed by any path in
//     this pass and left to its own reconstruction.
//   * RecordPropPositions @0x822E1988 -- stays deferred until the PropSerialiserFrame
//     interior (its per-cell/per-prop/per-part replay arrays) is reconstructed.
//
// Source-of-truth: the X360 ARTIST asm is authoritative (per-function addresses noted on
// each body). The inlined container bounds asserts (CgsBitArray.h:203 "invalid index",
// :241 "luIndex < NUMBITS") are emitted at these call sites because the shared
// CgsBitArray.h keeps its bodies assert-free (the assert-system dependency lives at the
// caller). They are reproduced verbatim; file/line are dropped per house convention.
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropCellManager.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h" // OutputBuffer_PreScene
#include "GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.h"          // PropInputInterface
#include "GameSource/Replays/Serialisers/BrnReplayPropEntitySerialiser.h"           // PropEntitySerialiser
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // gpDebugPrint / gxMessageFilterFlags
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"  // PropPhysicsDataHeader::GetType, PropTypeData accessors
#include "rw/math/vpu/vector3_operation.h"                          // operator-, MagnitudeSquared (sphere test)
#include <math.h>                                                   // floorf (vrfim -- round toward -inf)

// includes folded in from the BrnPropCellManager_w*.cpp partfiles (2026-09-15)
#include "rw/physics/rigidbody.h"                                   // rw::physics::FROZEN_BODY

namespace BrnWorld
{
    using BrnPhysics::Props::KI_WORLD_X_HALF_EXTENT;
    using BrnPhysics::Props::KI_WORLD_X_EXTENSION;
    using BrnPhysics::Props::KI_WORLD_Z_HALF_EXTENT;
    using BrnPhysics::Props::KI_CELL_SIZE;

    // KU_MAX_LOADED_PROP_INSTANCES == 5400 (0x1518), the asm-pinned entity-index bound
    // fired by GetPart (matches BrnWorld::PropZoneManager::KU_MAX_LOADED_PROP_INSTANCES).
    static const u32 KU_MAX_LOADED_PROP_INSTANCES = 5400;

    // AddPropToScene's transform sanity bound (asm `flt_8201D2BC` == 15000.0f).
    const f32 PropCellManager::KF_MAX_VALID_POSITION_ALONG_AXIS = 15000.0f;

    namespace
    {
        // OUTLINED from AddPropToScene @0x822E0128, which inlines it twelve `vcmpeqfp.`
        // lanes deep (three lanes for each of the four transform columns). The function's
        // NAME is baked into the assert literal it guards ("IsValid(lTransform)"), so this
        // is the compiler-folded rw math helper restored to a call, not a new predicate:
        // `vcmpeqfp v,v,v` is the standard self-compare NaN test, and the four per-column
        // results are ANDed together.
        bool IsValid(const Matrix44Affine& lrTransform)
        {
            const Vector3* lpaColumns[4] =
            {
                &lrTransform.Right(), &lrTransform.Up(), &lrTransform.At(), &lrTransform.Pos()
            };
            for (s32 liColumn = 0; liColumn < 4; ++liColumn)
            {
                const Vector3& lrColumn = *lpaColumns[liColumn];
                if (!(lrColumn.x == lrColumn.x) ||
                    !(lrColumn.y == lrColumn.y) ||
                    !(lrColumn.z == lrColumn.z))
                {
                    return false;
                }
            }
            return true;
        }

        // The write-locked scene interface the prop output buffer carries. The buffer models
        // it as an opaque sized span (BrnPropEntityModuleIO.h owns that decision), so the
        // handle is re-typed here exactly the way PropZoneManager re-types the prop-input
        // interface it gets from the same buffer.
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface*
        GetSceneInterface(PropEntityIO::OutputBuffer_PreScene* lpOutput)
        {
            return reinterpret_cast<CgsSceneManager::SceneManagerIO::InSceneUpdateInterface*>(
                lpOutput->GetSceneInputInterface());
        }

        BrnPhysics::Props::PropInputInterface*
        GetPropInputInterface(PropEntityIO::OutputBuffer_PreScene* lpOutput)
        {
            return reinterpret_cast<BrnPhysics::Props::PropInputInterface*>(
                lpOutput->GetPropInputInterface());
        }

        // The scene manager keys its entity queues on CgsSceneManager::EntityId, while
        // BrnWorld::PropEntityID's `operator EntityId()` (X360 @0x822B78E8 -- a real
        // out-of-line call in every one of these bodies, and the home of the owner-byte
        // tripwire) yields the project's plain 32-bit EntityId word. Going through the
        // operator keeps that tripwire in the path; the word is then re-typed for the
        // scene call, which is exactly what the single `lwz r4,0(result)` in the asm does.
        CgsSceneManager::EntityId ToSceneEntityId(PropEntityID lPropEntityID)
        {
            return CgsSceneManager::EntityId(static_cast<EntityId>(lPropEntityID).muValue);
        }
    }

    // Pin the pointer-free head of the record (console offsets == host offsets there).
    void PropCellManager::_AssertLayout()
    {
        static_assert(offsetof(PropCellManager, maCells)          == 0,    "maCells @ +0");
        static_assert(offsetof(PropCellManager, miNumLoadedCells) == 1800, "miNumLoadedCells @ +1800 (asm 0x708)");
        static_assert(offsetof(PropCellManager, maActiveCells)    == 1804, "maActiveCells @ +1804 (asm 0x70C)");
        static_assert(offsetof(PropCellManager, miNumActiveCells) == 1900, "miNumActiveCells @ +1900 (asm 0x76C)");
        static_assert(offsetof(PropCellManager, mpaProps)         == 1904, "mpaProps @ +1904 (asm 0x770)");

        // ⭐ ADDED 2026-08-18 (wave Q round 2), from a round-1 verifier NIT on
        // PropEntityModule_wQ_07.cpp. The physical-slot ceiling is spelled THREE ways in this
        // subsystem -- this class's own KU_MAX_PHYSICAL_PROPS/KU_MAX_PHYSICAL_PARTS, the bare
        // literals that size the members below, and BrnPhysics::Props::KU_MAX_PHYSICAL_PROPS /
        // KU_MAX_PHYSICAL_PROP_PARTS (which the broker loops and both BitArray qualifications
        // in that partfile use). All three agree at 15/30 today, so nothing is wrong now --
        // but BrnPropInputInterface.h:48-51 explicitly marks its pair as temporary squatters
        // scheduled to MOVE to BrnPropConstants.h, by someone with no reason to open this
        // file. A pool resized through only one spelling is a silent out-of-bounds walk over
        // maPhysicalPartParams. These four asserts make the three spellings unable to drift.
        static_assert(KU_MAX_PHYSICAL_PROPS == BrnPhysics::Props::KU_MAX_PHYSICAL_PROPS,
                      "PropCellManager::KU_MAX_PHYSICAL_PROPS == BrnPhysics::Props::KU_MAX_PHYSICAL_PROPS");
        static_assert(KU_MAX_PHYSICAL_PARTS == BrnPhysics::Props::KU_MAX_PHYSICAL_PROP_PARTS,
                      "PropCellManager::KU_MAX_PHYSICAL_PARTS == BrnPhysics::Props::KU_MAX_PHYSICAL_PROP_PARTS");
        static_assert(sizeof(((PropCellManager*)0)->maPhysicalPropParams) / sizeof(PhysicalParams)
                          == KU_MAX_PHYSICAL_PROPS,
                      "maPhysicalPropParams extent == KU_MAX_PHYSICAL_PROPS");
        static_assert(sizeof(((PropCellManager*)0)->maPhysicalPartParams) / sizeof(PhysicalParams)
                          == KU_MAX_PHYSICAL_PARTS,
                      "maPhysicalPartParams extent == KU_MAX_PHYSICAL_PARTS");
    }

    // ========================================================================
    // PropCellManager::Construct -- INLINED by the X360 into
    // PropZoneManager::Construct @0x822F0568 (the stores at this-relative offsets below
    // 2432; everything at or beyond +2432 belongs to PropZoneManager's own arrays).
    // The recovered store sequence, in asm order, is:
    //     sth 0,   0x90C  -> mu16NumberOfPropVolumesInScene
    //     stw r8,  0x770  -> mpaProps      (= &PropZoneManager::maProps[0])
    //     stw r7,  0x774  -> mpaPropParts  (= &PropZoneManager::maParts[0])
    //     stw 0,   0x900/0x904/0x908 -> miSizeOfTargetList / miNumPropsInSim / miNumPartsInSim
    //     sth 0,   0x90E  -> mu16NumberOfPropEntitiesInScene
    //     stw 0,   0x76C  -> miNumActiveCells
    //     std 0,   0x778 / 0x780 (TWICE each) -> mPhysicalProps / mPhysicalParts
    // The doubled bit-array clears are the inlined BitArray Construct() followed by its
    // Clear(); reproduced rather than folded so the store count matches the binary.
    //
    // ⚠️ FINDING (reported, NOT "fixed"): the shipped X360 Construct does **not** write
    // miNumLoadedCells (+0x708). Every other counter in the record is explicitly zeroed,
    // so this is a genuine property of the binary, not a missed store -- the loaded-cell
    // count is left at whatever the containing PropZoneManager storage was allocated with
    // (zeroed module memory in practice). Adding a store the binary does not have would be
    // fabrication, so it is deliberately absent here; if a live run ever trips AddCells'
    // "miNumLoadedCells < KU_MAX_PROP_CELLS" tripwire on a cold boot, the fix belongs in
    // whatever zeroes the PropZoneManager block, not here.
    void PropCellManager::Construct(PropEntityInstance* lpaProps, PropPartEntityInstance* lpaPropParts)
    {
        mu16NumberOfPropVolumesInScene = 0;

        mpaProps     = lpaProps;
        mpaPropParts = lpaPropParts;

        miSizeOfTargetList = 0;
        miNumPropsInSim    = 0;
        miNumPartsInSim    = 0;

        mu16NumberOfPropEntitiesInScene = 0;
        miNumActiveCells                = 0;

        mPhysicalProps.UnSetAll();
        mPhysicalParts.UnSetAll();
        mPhysicalProps.UnSetAll();
        mPhysicalParts.UnSetAll();
    }

    // ========================================================================
    // @ 0x822A9E90 (70 insns). Register every cell of a freshly loaded zone. The zone's
    // cell ranges are zone-local, so each registry entry is biased by liStartIndex (the
    // zone's base slot in the shared prop pool). PropCellRecord::Constuct is inlined by
    // the X360; restored to a call here (the DWARF names it, typo and all).
    void PropCellManager::AddCells(const PropZoneData* lpZoneData, s32 liStartIndex)
    {
        const s32 liNumCells = lpZoneData->GetNumCells();

        for (s32 liCell = 0; liCell < liNumCells; ++liCell)
        {
            const PropCellData* lpCellData = lpZoneData->GetCellData(liCell);

            CGS_ASSERT(!IsCellLoaded(lpCellData->GetId()),
                       "!IsCellLoaded( lpCellData->GetId() )");
            CGS_ASSERT(miNumLoadedCells < KI_MAX_CELLS,
                       "miNumLoadedCells < BrnPhysics::Props::KU_MAX_PROP_CELLS");

            // The zone id written into the record comes from the ZONE, not the cell
            // (asm `lhz r9,0x18(lpZoneData)` -> `sth r9,8(newRecord)`).
            maCells[miNumLoadedCells].Constuct(static_cast<s16>(lpZoneData->GetZoneId()),
                                               static_cast<s16>(liStartIndex),
                                               lpCellData);
            ++miNumLoadedCells;
        }
    }

    // ========================================================================
    // @ 0x822FC7A8 (82 insns). Drop every registry entry belonging to li16ZoneId,
    // deactivating it first if it is live. Removal is a swap-with-last, so the loop index
    // steps BACK one slot after an erase to re-test the entry moved into the hole.
    void PropCellManager::RemoveCells(s16 li16ZoneId, const PropPhysicsDataHeader* lpTypes,
                                      RecentlyBrokenPropsArray* lpRecentlyBroken,
                                      PropEntityIO::OutputBuffer_PreScene* lpOutput,
                                      BrnReplays::PropEntitySerialiser* lpSerialiser,
                                      u16 lu16ZoneStartIndex)
    {
        for (s32 liCellIndex = 0; liCellIndex < miNumLoadedCells; ++liCellIndex)
        {
            CGS_ASSERT(liCellIndex >= 0 && liCellIndex < miNumLoadedCells,
                       "liCellIndex >= 0 && liCellIndex < miNumLoadedCells");

            PropCellRecord& lrCell = maCells[liCellIndex];
            if (lrCell.GetZoneId() != li16ZoneId)
            {
                continue;
            }

            if (lrCell.IsActive())
            {
                DeactivateCell(&lrCell, lpTypes, lpRecentlyBroken, lpOutput, lpSerialiser,
                               li16ZoneId, lu16ZoneStartIndex);
            }

            --miNumLoadedCells;
            maCells[liCellIndex] = maCells[miNumLoadedCells];
            CGS_ASSERT(miNumLoadedCells >= 0, "miNumLoadedCells >= 0");

            // Re-test the entry that was swapped into this hole.
            --liCellIndex;
        }
    }

    // ========================================================================
    // @ 0x822C6068 (171 insns). The target list is the 2x2 block of cells around
    // lv3Position: the cell it falls in, plus the neighbour in whichever direction the
    // position sits past the cell centre on each axis. The world-grid asserts are emitted
    // inline in this body (baked BrnPropCellManager.cpp lines 214-217), not in
    // PropZoneData::GetCellId. The cell sizes are function-local `static const f32`s (the
    // asm carries their lazy-init guard word); the division/floor pair is the `fdivs` +
    // `vrfim` (round toward -inf) sequence.
    void PropCellManager::GenerateTargetList(Vector3 lv3Position)
    {
        static const f32 lfCellSizeX = static_cast<f32>(KI_CELL_SIZE);
        static const f32 lfCellSizeZ = static_cast<f32>(KI_CELL_SIZE);

        const f32 lfX = lv3Position.x;
        const f32 lfZ = lv3Position.z;

        CGS_ASSERT(lfX > -static_cast<f32>(KI_WORLD_X_HALF_EXTENT),
                   "lfX > -KI_WORLD_X_HALF_EXTENT");
        CGS_ASSERT(lfX < static_cast<f32>(KI_WORLD_X_HALF_EXTENT + KI_WORLD_X_EXTENSION),
                   "lfX < KI_WORLD_X_HALF_EXTENT+KI_WORLD_X_EXTENSION");
        CGS_ASSERT(lfZ > -static_cast<f32>(KI_WORLD_Z_HALF_EXTENT),
                   "lfZ > -KI_WORLD_Z_HALF_EXTENT");
        CGS_ASSERT(lfZ < static_cast<f32>(KI_WORLD_Z_HALF_EXTENT),
                   "lfZ < KI_WORLD_Z_HALF_EXTENT");

        const f32 lfCellRatioX = (lfX + static_cast<f32>(KI_WORLD_X_HALF_EXTENT)) / lfCellSizeX;
        const f32 lfCellRatioZ = (lfZ + static_cast<f32>(KI_WORLD_Z_HALF_EXTENT)) / lfCellSizeZ;

        const u16 lu16CellX = static_cast<u16>(static_cast<s64>(floorf(lfCellRatioX)));
        const u16 lu16CellZ = static_cast<u16>(static_cast<s64>(floorf(lfCellRatioZ)));

        // Which side of the cell centre the position sits on picks the neighbour.
        const s32 liStepX = ((lfCellRatioX - static_cast<f32>(lu16CellX)) > 0.5f) ? 1 : -1;
        const s32 liStepZ = ((lfCellRatioZ - static_cast<f32>(lu16CellZ)) > 0.5f) ? 1 : -1;

        const u16 lu16NeighbourX = static_cast<u16>(lu16CellX + liStepX);
        const u16 lu16NeighbourZ = static_cast<u16>(lu16CellZ + liStepZ);

        miSizeOfTargetList = 0;
        maTargetList[miSizeOfTargetList++].Construct(lu16CellX,      lu16CellZ);
        maTargetList[miSizeOfTargetList++].Construct(lu16NeighbourX, lu16CellZ);
        maTargetList[miSizeOfTargetList++].Construct(lu16NeighbourX, lu16NeighbourZ);
        maTargetList[miSizeOfTargetList++].Construct(lu16CellX,      lu16NeighbourZ);
    }

    // ========================================================================
    // @ 0x822FC8F0 (185 insns). The per-frame cell sweep PropEntityModule::PreSceneUpdate
    // drives: audit the in-sim counters against the physical bit arrays, rebuild the
    // target list around the player, then bring every loaded cell's activation state in
    // line with it.
    //
    // ⭐ 2026-08-18 (wave Q keystone): `lbInReplay` added at position 5 to match the shipped
    // seven-parameter form (see the declaration's note). It is DELIBERATELY UNUSED here --
    // the X360 body never reads r7 -- so it is left unnamed in the definition rather than
    // given a name the body would not touch.
    void PropCellManager::Update(Vector3 lv3Position, const PropPhysicsDataHeader* lpTypes,
                                 RecentlyBrokenPropsArray* lpRecentlyBroken,
                                 PropEntityIO::OutputBuffer_PreScene* lpOutput,
                                 bool /*lbInReplay -- r7, never read by the shipped body*/,
                                 BrnReplays::PropEntitySerialiser* lpSerialiser,
                                 const u16* lpuZoneStartIndices)
    {
        CGS_ASSERT(miNumPropsInSim == static_cast<s32>(mPhysicalProps.CountSetBits()),
                   "miNumPropsInSim == static_cast< int32_t >( mPhysicalProps.CountSetBits() )");
        CGS_ASSERT(miNumPartsInSim == static_cast<s32>(mPhysicalParts.CountSetBits()),
                   "miNumPartsInSim == static_cast< int32_t >( mPhysicalParts.CountSetBits() )");

        GenerateTargetList(lv3Position);

        for (s32 liCell = 0; liCell < miNumLoadedCells; ++liCell)
        {
            PropCellRecord& lrCell = maCells[liCell];

            bool lbInTargetList = false;
            for (s32 liTarget = 0; liTarget < miSizeOfTargetList; ++liTarget)
            {
                if (maTargetList[liTarget] == lrCell.GetId())
                {
                    lbInTargetList = true;
                    break;
                }
            }

            if (!lbInTargetList && lrCell.IsActive())
            {
                const s32 liZoneId = lrCell.GetZoneId();
                DeactivateCell(&lrCell, lpTypes, lpRecentlyBroken, lpOutput, lpSerialiser,
                               liZoneId, lpuZoneStartIndices[liZoneId]);
            }
            if (lbInTargetList && !lrCell.IsActive())
            {
                ActivateCell(&lrCell, lpTypes, GetSceneInterface(lpOutput));
            }
        }
    }

    // ========================================================================
    // @ 0x822DFCB8 (163 insns). Push a loaded cell's props into contact generation and
    // record the cell in the active list.
    //
    // ⚠️ The `SetActive(true)` store sits INSIDE the per-prop loop in the shipped binary
    // (`stb r20,0xA(lpCell)` at 0x822DFF24, on the loop's join path), so an EMPTY cell
    // -- start == end -- is registered in maActiveCells but never marked active.
    // Reproduced as-shipped rather than hoisted, because hoisting would change behaviour
    // for empty cells.
    void PropCellManager::ActivateCell(PropCellRecord* lpCell, const PropPhysicsDataHeader* lpTypes,
                                       InSceneUpdateInterface* lpScene)
    {
        // Owner byte (E_ENTITYTYPE_PROP) seeded into the top byte of the embedded entity
        // word; the slot index is spliced in per prop below (asm `li 3; sldi 56; std`).
        PropVolumeInstanceID lVolumeInstanceID;
        lVolumeInstanceID.mVolumeInstanceId.muId = static_cast<u64>(E_ENTITYTYPE_PROP) << 56;

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "Activating cell: " << lpCell->GetId() << "\n";
        }

        CGS_ASSERT(!lpCell->IsActive(), "!lpCell->IsActive()");
        CGS_ASSERT(miNumActiveCells < KI_MAX_ACTIVE_CELLS,
                   "miNumActiveCells < BrnPhysics::Props::KU_MAX_ACTIVE_CELLS");

        maActiveCells[miNumActiveCells] = *lpCell;
        ++miNumActiveCells;

        for (s32 liProp = lpCell->GetStartIndex(); liProp < lpCell->GetEndIndex(); ++liProp)
        {
            PropEntityInstance* lpProp = &mpaProps[liProp];
            const PropTypeData* lpType = lpTypes->GetType(lpProp->muTypeId); // inlines the two type-id asserts

            lVolumeInstanceID.SetEntityIndex(static_cast<u16>(liProp));

            if ((lpProp->mu8Flags & KU_ADDED_TO_SCENE_BIT) != 0)
            {
                CGS_ASSERT(lpProp->mu8State < E_STATE_COUNT, "mu8State < E_STATE_COUNT");

                if (lpProp->mu8State < E_SMASHED)
                {
                    AddPropToContactGeneration(lpProp, lpType, lVolumeInstanceID, lpScene);
                }
                else
                {
                    AddPropPartsToContactGeneration(lpProp, &mpaPropParts[lpProp->mu16PartsIndex],
                                                    lpType, lVolumeInstanceID, lpScene);
                }
            }

            lpCell->SetActive(true);
        }
    }

    // ========================================================================
    // @ 0x822F1940 (338 insns). The inverse of ActivateCell: drop the cell from the active
    // list, then take every prop in it out of the recently-broken set, the sim, contact
    // generation and the scene. As in ActivateCell, the `SetActive(false)` store sits
    // inside the per-prop loop in the shipped binary.
    void PropCellManager::DeactivateCell(PropCellRecord* lpCell, const PropPhysicsDataHeader* lpTypes,
                                         RecentlyBrokenPropsArray* lpRecentlyBroken,
                                         PropEntityIO::OutputBuffer_PreScene* lpOutput,
                                         BrnReplays::PropEntitySerialiser* lpSerialiser,
                                         s32 liZoneId, u16 lu16ZoneStartIndex)
    {
        PropVolumeInstanceID lVolumeInstanceID;
        lVolumeInstanceID.mVolumeInstanceId.muId = static_cast<u64>(E_ENTITYTYPE_PROP) << 56;

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "Deactivating cell: " << lpCell->GetId() << "\n";
        }

        CGS_ASSERT(lpCell->IsActive(), "lpCell->IsActive()");

        // Swap-erase this cell out of the active list.
        bool lbFound = false;
        for (s32 liActive = 0; liActive < miNumActiveCells; ++liActive)
        {
            if (maActiveCells[liActive].GetId() == lpCell->GetId())
            {
                --miNumActiveCells;
                maActiveCells[liActive] = maActiveCells[miNumActiveCells];
                lbFound = true;
                break;
            }
        }
        CGS_ASSERT(lbFound, "lbFound");

        s32 liPropIndexInZone = lpCell->GetStartIndex() - static_cast<s32>(lu16ZoneStartIndex);

        for (s32 liProp = lpCell->GetStartIndex(); liProp < lpCell->GetEndIndex(); ++liProp)
        {
            PropEntityInstance* lpProp = &mpaProps[liProp];

            CGS_ASSERT(lpProp->mu16ZoneIndex == liZoneId, "lpProp->GetZone() == liZoneId");

            const PropTypeData* lpType = lpTypes->GetType(lpProp->muTypeId);
            lVolumeInstanceID.SetEntityIndex(static_cast<u16>(liProp));
            const PropEntityID lPropEntityID = lVolumeInstanceID.GetPropEntityID();

            // A prop leaving the world must not stay in the recently-broken replay set.
            if (lpRecentlyBroken->Find(lPropEntityID) != RecentlyBrokenPropsArray::KU_INVALID)
            {
                lpRecentlyBroken->Erase(lPropEntityID);
            }

            CGS_ASSERT(lpProp->mu8State < E_STATE_COUNT, "mu8State < E_STATE_COUNT");

            if (lpProp->mu8State < E_SMASHED)
            {
                const u8 lu8State = lpProp->mu8State;
                if (lu8State == E_STATIC || lu8State == E_LEANING ||
                    lu8State == E_PHYSICAL || lu8State == E_SMASHED)
                {
                    RemovePropFromSim(lpProp, lpType, lVolumeInstanceID,
                                      GetPropInputInterface(lpOutput));
                }
                if ((lpProp->mu8Flags & KU_ADDED_TO_CONTACT_GEN_BIT) != 0)
                {
                    RemovePropFromContactGeneration(lpProp, lpType, lVolumeInstanceID,
                                                    GetSceneInterface(lpOutput));
                }
                // The scene removal is gated on BOTH the moved bit and the in-scene bit
                // (asm tests bit 26 == KU_MOVED_BIT then bit 30 == KU_ADDED_TO_SCENE_BIT).
                if ((lpProp->mu8Flags & KU_MOVED_BIT) != 0 &&
                    (lpProp->mu8Flags & KU_ADDED_TO_SCENE_BIT) != 0)
                {
                    RemovePropFromScene(lpProp, lpType, lVolumeInstanceID,
                                        GetSceneInterface(lpOutput), lpSerialiser,
                                        static_cast<u16>(liZoneId), liPropIndexInZone);
                }
            }
            else
            {
                RemovePropPartsFromSimIfPhysical(lpProp, lpType, lVolumeInstanceID, lpOutput);
                if ((lpProp->mu8Flags & KU_ADDED_TO_CONTACT_GEN_BIT) != 0)
                {
                    RemovePropPartsFromContactGeneration(lpProp, lpType, lVolumeInstanceID,
                                                         GetSceneInterface(lpOutput));
                }
                if ((lpProp->mu8Flags & KU_ADDED_TO_SCENE_BIT) != 0)
                {
                    RemovePropPartsFromScene(lpProp, lpType, lVolumeInstanceID,
                                             GetSceneInterface(lpOutput), lpSerialiser,
                                             static_cast<u16>(liZoneId), liPropIndexInZone);
                }
            }

            lpCell->SetActive(false);
            ++liPropIndexInZone;
        }
    }

    // ========================================================================
    // @ 0x822E0128 (396 insns). THE function that makes a prop exist visually: flag the
    // prop as in-scene, tell the replay serialiser (record side only), sanity-check the
    // world transform, then push the entity into the scene manager's octree.
    //
    // A prop enters the scene STATIC and NON-PHYSICAL -- the four leading tripwires assert
    // it is not already in the scene or contact generation, is not smashed and is not
    // physical. No physics involvement at load time.
    void PropCellManager::AddPropToScene(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                         PropVolumeInstanceID lVolumeInstanceID,
                                         InSceneUpdateInterface* lpScene,
                                         BrnReplays::PropEntitySerialiser* lpSerialiser,
                                         u16 lu16ZoneId, s32 liPropIndexInZone)
    {
        const Matrix44Affine lTransform = lpProp->mWorldTransform;

        if (mu16NumberOfPropEntitiesInScene + 1u >= KU_MAX_PROP_ENTITIES_IN_SCENE)
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "Too many prop entities\n";
            }
            return;
        }

        CGS_ASSERT((lpProp->mu8Flags & KU_ADDED_TO_SCENE_BIT) == 0, "!lpProp->IsAddedToScene()");
        CGS_ASSERT((lpProp->mu8Flags & KU_ADDED_TO_CONTACT_GEN_BIT) == 0, "!lpProp->IsAddedToContactGen()");
        CGS_ASSERT(!lpProp->IsSmashed(),  "!lpProp->IsSmashed()");
        CGS_ASSERT(!lpProp->IsPhysical(), "!lpProp->IsPhysical()");

        lpProp->mu8Flags |= KU_ADDED_TO_SCENE_BIT;

        if (!lpSerialiser->IsPlaying())
        {
            lpSerialiser->SetPropAddedToScene(lu16ZoneId, static_cast<u32>(liPropIndexInZone), 1);
        }

        CGS_ASSERT(IsValid(lTransform), "IsValid(lTransform)");
        CGS_ASSERT(lTransform.Pos().x < KF_MAX_VALID_POSITION_ALONG_AXIS,
                   "lTransform.Pos().GetX() < KF_MAX_VALID_POSITION_ALONG_AXIS");
        CGS_ASSERT(lTransform.Pos().y < KF_MAX_VALID_POSITION_ALONG_AXIS,
                   "lTransform.Pos().GetY() < KF_MAX_VALID_POSITION_ALONG_AXIS");
        CGS_ASSERT(lTransform.Pos().z < KF_MAX_VALID_POSITION_ALONG_AXIS,
                   "lTransform.Pos().GetZ() < KF_MAX_VALID_POSITION_ALONG_AXIS");
        CGS_ASSERT(lTransform.Pos().x > -KF_MAX_VALID_POSITION_ALONG_AXIS,
                   "lTransform.Pos().GetX() > -KF_MAX_VALID_POSITION_ALONG_AXIS");
        CGS_ASSERT(lTransform.Pos().y > -KF_MAX_VALID_POSITION_ALONG_AXIS,
                   "lTransform.Pos().GetY() > -KF_MAX_VALID_POSITION_ALONG_AXIS");
        CGS_ASSERT(lTransform.Pos().z > -KF_MAX_VALID_POSITION_ALONG_AXIS,
                   "lTransform.Pos().GetZ() > -KF_MAX_VALID_POSITION_ALONG_AXIS");

        ++mu16NumberOfPropEntitiesInScene;

        const PropEntityID lPropEntityID = lVolumeInstanceID.GetPropEntityID();
        lpScene->AddEntity(ToSceneEntityId(lPropEntityID),
                           KU_PROP_SCENE_ENTITY_TYPE_FLAG,
                           lTransform.Pos(),
                           lpType->GetBoundingRadius());
    }

    // ========================================================================
    // @ 0x822E09F0 (100 insns). Take a whole (non-smashed, non-physical) prop back out of
    // the scene octree.
    void PropCellManager::RemovePropFromScene(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                              PropVolumeInstanceID lVolumeInstanceID,
                                              InSceneUpdateInterface* lpScene,
                                              BrnReplays::PropEntitySerialiser* lpSerialiser,
                                              u16 lu16ZoneId, s32 liPropIndexInZone)
    {
        (void)lpType;   // the shipped body never reads the type descriptor

        CGS_ASSERT((lpProp->mu8Flags & KU_ADDED_TO_SCENE_BIT) != 0, "lpProp->IsAddedToScene()");
        CGS_ASSERT((lpProp->mu8Flags & KU_ADDED_TO_CONTACT_GEN_BIT) == 0, "!lpProp->IsAddedToContactGen()");
        CGS_ASSERT(lpProp->mu8State < E_STATE_COUNT, "mu8State < E_STATE_COUNT");
        CGS_ASSERT(lpProp->mu8State != E_PHYSICAL, "!lpProp->IsPhysical()");

        lpProp->mu8Flags &= static_cast<u8>(~KU_ADDED_TO_SCENE_BIT);

        if (!lpSerialiser->IsPlaying())
        {
            lpSerialiser->SetPropAddedToScene(lu16ZoneId, static_cast<u32>(liPropIndexInZone), 0);
        }

        --mu16NumberOfPropEntitiesInScene;

        const PropEntityID lPropEntityID = lVolumeInstanceID.GetPropEntityID();
        lpScene->RemoveEntity(ToSceneEntityId(lPropEntityID), 0);
    }

    // ========================================================================
    // @ 0x822E0B80 (137 insns). The smashed-prop twin: one scene entity per PART comes out.
    // Part indices are 1-based (the packed PropEntityID reserves 0 for "the whole prop").
    void PropCellManager::RemovePropPartsFromScene(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                                   PropVolumeInstanceID lVolumeInstanceID,
                                                   InSceneUpdateInterface* lpScene,
                                                   BrnReplays::PropEntitySerialiser* lpSerialiser,
                                                   u16 lu16ZoneId, s32 liPropIndexInZone)
    {
        CGS_ASSERT((lpProp->mu8Flags & KU_ADDED_TO_SCENE_BIT) != 0, "lpProp->IsAddedToScene()");
        CGS_ASSERT((lpProp->mu8Flags & KU_ADDED_TO_CONTACT_GEN_BIT) == 0, "!lpProp->IsAddedToContactGen()");
        CGS_ASSERT(lpProp->mu8State < E_STATE_COUNT, "mu8State < E_STATE_COUNT");
        CGS_ASSERT(lpProp->mu8State >= E_SMASHED, "lpProp->IsSmashed()");

        lpProp->mu8Flags &= static_cast<u8>(~KU_ADDED_TO_SCENE_BIT);

        if (!lpSerialiser->IsPlaying())
        {
            lpSerialiser->SetPropAddedToScene(lu16ZoneId, static_cast<u32>(liPropIndexInZone), 0);
        }

        const u32 luNumberOfParts = lpType->GetNumberOfParts();
        for (u32 luPart = 1; luPart <= luNumberOfParts; ++luPart)
        {
            lVolumeInstanceID.SetPartIndex(luPart);

            --mu16NumberOfPropEntitiesInScene;

            const PropEntityID lPartEntityID = lVolumeInstanceID.GetPropEntityID();
            lpScene->RemoveEntity(ToSceneEntityId(lPartEntityID), 0);
        }
    }

    // ========================================================================
    // @ 0x822E0758 (166 insns). The smashed-prop twin of AddPropToScene: instead of one
    // scene entity for the whole prop, one entity PER PART goes in, each placed at the
    // part's rest offset transformed into the prop's world frame.
    //
    // UNPARKED 2026-08-12 (link-closure pass). The blocker was that PropPartTypeData's
    // leading Vector3 had no accessor; PropTypeData's home now exposes it as GetOffset()
    // (record +0x00), so the per-part offset is reachable by name and this whole body
    // lands. It is LOAD-BEARING: PropZoneManager::LoadProp calls it for every prop that
    // loads already smashed.
    //
    // ⚠️ TWO console-constant traps sidestepped here, both flagged by the wave's recurring
    // bug:
    //   (1) the part-type array stride. The asm walks it with `r27 += 0x30` -- 48 bytes,
    //       the CONSOLE size of PropPartTypeData. On x64 the record is 64 bytes (its
    //       embedded volume pointer widened 4->8; pinned by PropPartTypeData::_AssertLayout).
    //       So this indexes GetPartVolumeGroups()[i] and lets the compiler pick the stride.
    //   (2) the part radius. `lfs f1, 0x28(r30)` is the CONSOLE offset of mfSphereRadius,
    //       which on the host has moved to +0x30 behind that same widened pointer; read by
    //       name through GetBoundingRadius().
    //
    // The transform maths (asm 0x822E090C-0x822E0940) is the textbook affine point
    // transform, with the same IDA operand-order trap the prop InitialiseFromData banner
    // documents: `vmaddfp vD,vA,vB,vC` prints in raw field order but means vD = vA*vC + vB.
    // Decoded, it is
    //     partPos = M.Right()*offset.x + M.Up()*offset.y + M.At()*offset.z + M.Pos()
    // and the part inherits the prop's ORIENTATION unchanged -- the three axis rows are
    // block-copied straight across (`stvx128 v127/v126/v125` into part+0/+0x10/+0x20) and
    // only the translation row differs. Kept as 4-lane Vector3 expressions rather than a
    // TransformPoint call because the console carries the w lane through, same as
    // PropEntityInstance::InitialiseFromData.
    void PropCellManager::AddPropPartsToScene(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                              PropVolumeInstanceID lVolumeInstanceID,
                                              InSceneUpdateInterface* lpScene,
                                              BrnReplays::PropEntitySerialiser* lpSerialiser,
                                              u16 lu16ZoneId, s32 liPropIndexInZone)
    {
        const u32 luNumberOfParts = lpType->GetNumberOfParts();

        // Budget check, asm 0x822E0784-0x822E07A0: `numInScene + numParts - 1` compared
        // UNSIGNED against 0x1D4C. Reproduced with the same unsigned arithmetic (the -1
        // matters: it is what lets a prop whose parts exactly fill the remaining budget
        // still go in).
        const u32 luEntitiesAfterAdd =
            static_cast<u32>(mu16NumberOfPropEntitiesInScene) + luNumberOfParts - 1u;
        if (luEntitiesAfterAdd >= KU_MAX_PROP_ENTITIES_IN_SCENE)
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "Too many prop entities\n";
            }
            return;
        }

        CGS_ASSERT((lpProp->mu8Flags & KU_ADDED_TO_SCENE_BIT) == 0, "!lpProp->IsAddedToScene()");
        CGS_ASSERT((lpProp->mu8Flags & KU_ADDED_TO_CONTACT_GEN_BIT) == 0, "!lpProp->IsAddedToContactGen()");
        CGS_ASSERT(lpProp->IsSmashed(), "lpProp->IsSmashed()");

        lpProp->mu8Flags |= KU_ADDED_TO_SCENE_BIT;

        if (!lpSerialiser->IsPlaying())
        {
            lpSerialiser->SetPropAddedToScene(lu16ZoneId, static_cast<u32>(liPropIndexInZone), 1);
        }

        // The asm guards the loop with `lbz 0x5D(type); cmplwi 0; beq end` and then runs it
        // as a do-while over 1-based part indices; the guarded do-while is exactly this for.
        const Matrix44Affine& lrPropTransform = lpProp->mWorldTransform;

        for (u32 luPart = 1; luPart <= luNumberOfParts; ++luPart)
        {
            const BrnPhysics::Props::PropPartTypeData& lrPartType =
                lpType->GetPartVolumeGroups()[luPart - 1];

            lVolumeInstanceID.SetPartIndex(luPart);

            const Vector3& lrOffset = lrPartType.GetOffset();
            Vector3 lPartPosition = lrPropTransform.xAxis * lrOffset.x;
            lPartPosition = lrPropTransform.yAxis * lrOffset.y + lPartPosition;
            lPartPosition = lrPropTransform.zAxis * lrOffset.z + lPartPosition;
            lPartPosition = lrPropTransform.wAxis + lPartPosition;

            // The console re-applies the part index to the extracted entity id even though
            // the volume-instance id it came from already carries it (asm 0x822E0944 then
            // 0x822E095C). Redundant, but it is a real call in the shipped body -- and its
            // owner tripwire is one of the ones that fires -- so it stays.
            PropEntityID lPartEntityID = lVolumeInstanceID.GetPropEntityID();
            lPartEntityID.SetPartIndex(luPart);

            PropPartEntityInstance* lpPart = GetPart(lPartEntityID);
            lpPart->mWorldTransform.xAxis = lrPropTransform.xAxis;
            lpPart->mWorldTransform.yAxis = lrPropTransform.yAxis;
            lpPart->mWorldTransform.zAxis = lrPropTransform.zAxis;
            lpPart->mWorldTransform.wAxis = lPartPosition;

            ++mu16NumberOfPropEntitiesInScene;

            lpScene->AddEntity(ToSceneEntityId(lPartEntityID),
                               KU_PROP_SCENE_ENTITY_TYPE_FLAG,
                               lPartPosition,
                               lrPartType.GetBoundingRadius());
        }
    }

    // ========================================================================
    // @ 0x822DFF48 (68 insns). Release the prop's physical slot and tell the prop manager
    // to drop its rigid body; the prop drops back to E_MOVED.
    void PropCellManager::RemovePropFromSim(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                            PropVolumeInstanceID lVolumeInstanceID,
                                            PropInputInterface* lpPropInput)
    {
        (void)lpType;   // the shipped body never reads the type descriptor

        const u8 lu8State = lpProp->mu8State;
        CGS_ASSERT(lu8State == E_STATIC || lu8State == E_LEANING ||
                   lu8State == E_PHYSICAL || lu8State == E_SMASHED,
                   "lpProp->IsAddedToSim()");
        CGS_ASSERT((lpProp->mu8Flags & KU_PHYSICS_ENABLED_BIT) != 0, "lpProp->IsPhysicsEnabled()");

        const s32 liPhysicsIndex = lpProp->mu8PhysicsIndex;
        FreePhysicalPropSlot(liPhysicsIndex);

        --miNumPropsInSim;
        CGS_ASSERT(miNumPropsInSim >= 0, "miNumPropsInSim >= 0");

        lpPropInput->RemovePropInstance(lVolumeInstanceID.GetPropEntityID(), liPhysicsIndex);

        lpProp->mu8State = E_MOVED;
    }

    // ========================================================================
    // @ 0x822E0058 (55 insns). For a smashed prop: release every part that is currently
    // simulated. Part slots live at mpaPropParts[mu16PartsIndex + (partIndex - 1)]; the
    // part id spliced into the volume-instance id is 1-based.
    void PropCellManager::RemovePropPartsFromSimIfPhysical(PropEntityInstance* lpProp,
                                                           const PropTypeData* lpType,
                                                           PropVolumeInstanceID lVolumeInstanceID,
                                                           PropEntityIO::OutputBuffer_PreScene* lpOutput)
    {
        PropPartEntityInstance* lpaParts = &mpaPropParts[lpProp->mu16PartsIndex];

        const u32 luNumberOfParts = lpType->GetNumberOfParts();
        for (u32 luPart = 1; luPart <= luNumberOfParts; ++luPart)
        {
            PropPartEntityInstance& lrPart = lpaParts[luPart - 1];

            lVolumeInstanceID.SetPartIndex(luPart);

            if (!lrPart.mbPhysical)
            {
                continue;
            }

            --miNumPartsInSim;
            FreePhysicalPartSlot(lrPart.mu8PhysicsIndex);

            const PropEntityID lPartEntityID = lVolumeInstanceID.GetPropEntityID();
            GetPropInputInterface(lpOutput)->RemovePartInstance(lPartEntityID,
                                                                lrPart.mu8PhysicsIndex);
            lrPart.mbPhysical = false;
        }
    }

    // ========================================================================
    // @ 0x822A9FA8 (22 insns). Would adding this type's PART volumes (and dropping its
    // whole-prop volumes) still leave the scene inside the prop-volume budget?
    bool PropCellManager::CanAddPartVolumes(const PropTypeData* lpType)
    {
        u32 luPartVolumes = 0;
        const u32 luNumberOfParts = lpType->GetNumberOfParts();
        for (u32 luPart = 0; luPart < luNumberOfParts; ++luPart)
        {
            luPartVolumes += lpType->GetPartVolumeGroups()[luPart].GetNumberOfVolumes();
        }

        // The whole-prop volumes go away as the part volumes arrive, so only the delta
        // counts (asm subtracts mu8NumberOfVolumes then truncates to 16 bits).
        const u16 lu16Delta = static_cast<u16>(luPartVolumes - lpType->GetNumberOfVolumes());
        return (static_cast<u32>(lu16Delta) + mu16NumberOfPropVolumesInScene)
               < KU_MAX_PROP_VOLUMES_IN_SCENE;
    }

    // ========================================================================
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

    // @ 0x822A4130. Linear scan of the loaded-cell registry for lCellId.
    // The X360 compares the two u16 halves separately (`lhz` at +0 and +2), which is
    // exactly PropCellId::operator== -- see the cell-id convention note in the header.
    bool PropCellManager::IsCellLoaded(PropCellId lCellId) const
    {
        for (s32 liCell = 0; liCell < miNumLoadedCells; ++liCell)
        {
            if (maCells[liCell].GetId() == lCellId)
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
    // active, tests every prop in the cell's [start,end) slot range against a sphere
    // centred on lv3Position with radius (lvClearRadius + the type's bounding radius). A prop
    // that overlaps, is not a traffic light, and is not one of three special street-furniture
    // graphics ids is removed from the sim (sim-resident states {STATIC,LEANING,PHYSICAL,
    // SMASHED}), from contact generation (ADDED_TO_CONTACT_GEN flag), and from the scene
    // (ADDED_TO_SCENE flag). The two bounds asserts on the type id are inlined by the asm
    // (reproduced by GetType); the state assert follows. lvClearRadius is a broadcast VecFloat.
    void PropCellManager::ClearPropsNearPosition(Vector3 lv3Position, VecFloat lvClearRadius,
                                                 const PropPhysicsDataHeader* lpTypes,
                                                 PropInputInterface* lpPropInput,
                                                 InSceneUpdateInterface* lpScene,
                                                 BrnReplays::PropEntitySerialiser* lpSerialiser,
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
                if (maTargetList[liTarget] == lrCell.GetId())
                {
                    lbInTargetList = true;
                    break;
                }
            }

            if (!lrCell.IsActive() || !lbInTargetList)
            {
                continue;
            }

            for (s32 liPropSlot = lrCell.GetStartIndex(); liPropSlot < lrCell.GetEndIndex(); ++liPropSlot)
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
                    // Zone id and index-within-zone both come off the PROP (asm
                    // `lhz r9,0x46(prop)` for the zone, then start[zone] for the bias).
                    const u16 lu16ZoneId = lrProp.mu16ZoneIndex;
                    const s32 liPropIndexInZone =
                        liPropSlot - static_cast<s32>(lpuZoneStartIndices[lu16ZoneId]);
                    RemovePropFromScene(&lrProp, lpType, lVolumeInstanceID, lpScene, lpSerialiser,
                                        lu16ZoneId, liPropIndexInZone);
                }
            }
        }
    }
}

// ============================================================================
// FOLDED FROM BrnPropCellManager_wQ4.cpp (wave Q4) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/BrnPropCellManager_wQ4.cpp
//
// WAVE Q4 (2026-08-18) -- the four BrnWorld::PropCellManager CONTACT-GENERATION bodies.
// These are the functions that make a loaded prop COLLIDABLE: they register each of the
// prop's (or, once smashed, each of its parts') collision volumes with the scene manager
// so the broad phase can produce a car-vs-prop overlap at all.
//
//     AddPropToContactGeneration            @0x822DF6C8  (199 insns)
//     AddPropPartsToContactGeneration       @0x822DF9D8  (186 insns)
//     RemovePropFromContactGeneration       @0x822C6318  ( 72 insns)
//     RemovePropPartsFromContactGeneration  @0x822C6430  ( 79 insns)
//
// They live in this partfile rather than in BrnPropCellManager.cpp only because that TU is
// concurrently owned by another session; they are ordinary members of PropCellManager and
// belong to the same ledger TU (class:BrnWorld::PropCellManager).
//
// ---- WHY THEY WERE PARKED, AND WHAT RETIRED THE PARK ------------------------------------
// All four splice a VOLUME INDEX into the LOW BYTE of the 64-bit PropVolumeInstanceID
// (`clrrdi r,id,8; or index`) and hand the WHOLE 64-BIT WORD to the scene. The committed
// InSceneUpdateInterface collision entry points took a 32-bit CgsSceneManager::EntityId,
// which would have DISCARDED that index -- every volume of a prop keying to one scene id.
// Wave Q4 landed the two missing pieces, both DWARF-shaped and both additive:
//   * CgsSceneManager::VolumeInstanceId::SetVolumeIndex/GetVolumeIndex/GetEntityIDOwner
//     (DWARF CgsVolumeInstanceId.h:92 / :115 / :106) and the wrapper's
//     BrnWorld::PropVolumeInstanceID::SetVolumeNumber/GetVolumeNumber
//     (DWARF BrnPropEntityID.h:181 / :199) -- the asm-attested splice.
//   * InSceneUpdateInterface::AddVolumeInstance(VolumeInstanceId, VolumeId, const
//     Matrix44Affine&) @0x822CB650 and AddForCollision(VolumeInstanceId, CullingGroup,
//     BodyState, Vector3, EAddForCollisionCacheOptions) @0x822B1860 -- the DecFIGS DWARF's
//     ONLY spelling of those two producers (CgsSceneManagerIO_SceneUpdate.h:464 / :476;
//     there is NO EntityId form in the DWARF at all). The 64-bit RemoveForCollision /
//     RemoveVolumeInstance were already present as header inlines (ghost-car wave).
//
// ---- SOURCE OF TRUTH --------------------------------------------------------------------
// Every body below is decoded from the raw `assembly` array of the per-address IDA JSON
// under .ida-exports/BURNOUT_X360_ARTIST.XEX (the Hex-Rays pseudocode was used only to
// recover the two long assert literals, which it prints in full). Console byte offsets are
// quoted as PROVENANCE ONLY -- every field is reached BY NAME (gotcha 1).
//
// ---- THE TWO MAGIC ARGUMENTS AddForCollision IS HANDED -----------------------------------
// The three scalars are hoisted out of the loop by the compiler and stored straight into the
// stack event record, so the asm shows them as bare immediates rather than as call arguments:
//     AddPropToContactGeneration       0x822DF824  stw 7 -> event+0x18 (mCullGroup)
//                                      0x822DF82C  stb 2 -> event+0x1C (muBodyState)
//                                      0x822DF830  stb 2 -> event+0x1D (muCacheOptions)
//     AddPropPartsToContactGeneration  0x822DFB14  stw 8 -> event+0x18 (mCullGroup)
//                                      0x822DFB24  stb 2 -> event+0x1C
//                                      0x822DFB28  stb 2 -> event+0x1D
// 2 == rw::physics::FROZEN_BODY (rigidbody.h) and 2 == E_DO_NOT_ADD_TO_CACHE_MANAGER
// (CgsSceneManagerIO_EventAddForCollision.h) -- both named below rather than left as 2.
//
// ⚠️ THE CULLING GROUP IS **7 FOR PROPS AND 8 FOR PROP PARTS** -- they are two different
// groups, not a transcription slip. Independently corroborated by the console's own culling
// table: WorldModule::Prepare stage 3 (BrnWorldModule.cpp:877-886, itself transcribed from
// @0x827D53B0) disables exactly ten pairs, and they are {7,8} x {1,6,5,3,9} -- i.e. the two
// prop groups against the same five world groups. Nothing else in the table uses 7 or 8.
// No named enum for these ids exists anywhere in the DWARF or the tree, so they are named
// here as file-local constants with that attestation rather than spelled as literals four
// times.

namespace BrnWorld
{
    namespace
    {
        // See the header banner: the two prop culling-group ids, attested by the immediates
        // at 0x822DF824 (7) / 0x822DFB14 (8) and cross-checked against the ten culling-group
        // pairs WorldModule::Prepare posts.
        const CgsSceneManager::SceneManagerIO::InEventAddForCollision::CullingGroup
            KU_PROP_CULLING_GROUP = 7;
        const CgsSceneManager::SceneManagerIO::InEventAddForCollision::CullingGroup
            KU_PROP_PART_CULLING_GROUP = 8;
    }

    // ========================================================================
    // @ 0x822DF6C8. Register a whole (un-smashed) prop's collision volumes with the scene,
    // one InEventAddVolumeInstance + one InEventAddForCollision per volume.
    //
    // ASM SPINE (0x822DF6C8..0x822DF9D4):
    //   0x822DF6F0..0x822DF740  four lvx128/stvx128 lanes copy lpProp->mWorldTransform into a
    //                           64-byte stack local ONCE, before any tripwire; that local is
    //                           what every AddVolumeInstance in the loop is handed (r6).
    //   0x822DF6F8/0x822DF714   `lbz 0x4A(prop)` + `rlwinm 0,29,29` == mu8Flags & 4
    //                           -> assert "!lpProp->IsAddedToContactGen()", line 280.
    //   0x822DF76C..0x822DF790  `lhz 0x90C(this)` > 0xC80 -> the budget assert, line 282.
    //   0x822DF794..0x822DF7DC  `lbz 0x5E(type)` + count >= 0xC80 -> "Too many prop volumes\n"
    //                           and RETURN (no flag is set, nothing is queued).
    //   0x822DF7E0..0x822DF7EC  `clrlwi r10,r11,31` == mu8Flags & 1 -> return when clear.
    //   0x822DF7F0/0x822DF7FC   `ori 4` -> set KU_ADDED_TO_CONTACT_GEN_BIT.
    //   0x822DF800..0x822DF808  zero-volume early out.
    //   0x822DF884..0x822DF9C4  the per-volume loop.
    //
    // ⚠️ MEASURED, NOT ASSUMED -- BOTH EARLY RETURNS LEAVE THE PROP UN-FLAGGED. The budget
    // guard (`blt` at 0x822DF7A4) and the physics-enabled guard (`beq` at 0x822DF7EC) both
    // branch to the shared epilogue at 0x822DF9C8, and the `ori 4` that sets
    // KU_ADDED_TO_CONTACT_GEN_BIT is at 0x822DF7F0 -- i.e. AFTER both. A prop that is over
    // budget, or whose physics is disabled, is not marked as added and so is not removed
    // later either; that is the shipped behaviour, not an oversight to smooth over.
    void PropCellManager::AddPropToContactGeneration(PropEntityInstance* lpProp,
                                                     const PropTypeData* lpType,
                                                     PropVolumeInstanceID lVolumeInstanceID,
                                                     InSceneUpdateInterface* lpScene)
    {
        // Copied once, up front, exactly where the asm copies it (before the first tripwire).
        const Matrix44Affine lTransform = lpProp->mWorldTransform;

        CGS_ASSERT(!lpProp->IsAddedToContactGen(), "!lpProp->IsAddedToContactGen()");    // :280
        CGS_ASSERT(mu16NumberOfPropVolumesInScene <= KU_MAX_PROP_VOLUMES_IN_SCENE,
                   "mu16NumberOfPropVolumesInScene <= BrnPhysics::Props::KU_MAX_PROP_COLLISION_VOLUMES_IN_SCENE"); // :282

        const u32 luNumberOfVolumes = lpType->GetNumberOfVolumes();

        if (luNumberOfVolumes + static_cast<u32>(mu16NumberOfPropVolumesInScene)
                >= KU_MAX_PROP_VOLUMES_IN_SCENE)
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "Too many prop volumes\n";
            }
            return;
        }

        // `lbz 0x4A(prop) ; clrlwi 31` -- bit 0 == KU_PHYSICS_ENABLED_BIT. No named accessor
        // for this bit is declared on PropEntityInstance (IsAddedToScene / IsAddedToContactGen
        // / IsAnimated / IsLamppost are, this one is not), so the mask is spelled by name.
        if ((lpProp->mu8Flags & KU_PHYSICS_ENABLED_BIT) == 0)
        {
            return;
        }

        lpProp->mu8Flags |= KU_ADDED_TO_CONTACT_GEN_BIT;

        if (luNumberOfVolumes == 0)
        {
            return;
        }

        // `vspltisw128 v127, 0` at 0x822DF810 -- hoisted out of the loop, stored into every
        // event's mPadding lane (+0x00) at 0x822DF948.
        Vector3 lPadding;
        lPadding.SetZero();

        // Default-constructed: the ctor's fold is `lis r7,3 ; std r7,var_128(r1)` at
        // 0x822DF704/0x822DF724 -- E_ENTITYTYPE_PROP seeded at VolumeId's owner bits [16..23],
        // which is what PropVolumeID::Set's own owner tripwire then reads back.
        PropVolumeID lVolumeID;

        for (u32 luVolume = 0; luVolume < luNumberOfVolumes; ++luVolume)
        {
            // 0x822DF888/0x822DF894 -- `lhz 0x44(prop)` is PropEntityInstance::muTypeId.
            lVolumeID.Set(lpProp->muTypeId, static_cast<u8>(luVolume));

            // 0x822DF8A0/0x822DF8AC -- the splice this whole cluster was parked on.
            lVolumeInstanceID.SetVolumeNumber(static_cast<u8>(luVolume));

            // 0x822DF898/0x822DF8A4/0x822DF8B4 -- the count is bumped here, between the
            // splice and the two owner tripwires, not after the queue writes.
            ++mu16NumberOfPropVolumesInScene;

            // The two conversions are materialised into named locals so their tripwires fire
            // in the asm's order (VolumeId's line-387 check at 0x822DF8B8, then
            // VolumeInstanceId's line-455 check at 0x822DF8E0). Passing the wrappers straight
            // into the call would leave that order unspecified in C++.
            const CgsSceneManager::VolumeId         lSceneVolumeId   = lVolumeID;
            const CgsSceneManager::VolumeInstanceId lSceneInstanceId = lVolumeInstanceID;
            lpScene->AddVolumeInstance(lSceneInstanceId, lSceneVolumeId, lTransform); // 0x822DF910

            // 0x822DF914 -- the SECOND line-455 check: a second conversion of the same
            // wrapper for the second producer's argument.
            const CgsSceneManager::VolumeInstanceId lCollisionInstanceId = lVolumeInstanceID;
            lpScene->AddForCollision(lCollisionInstanceId,
                                     KU_PROP_CULLING_GROUP,
                                     rw::physics::FROZEN_BODY,
                                     lPadding,
                                     CgsSceneManager::SceneManagerIO::E_DO_NOT_ADD_TO_CACHE_MANAGER);
        }
    }

    // ========================================================================
    // @ 0x822DF9D8. The SMASHED-prop twin: the whole-prop volumes are gone and each PART
    // contributes its own volume group instead.
    //
    // ASM SPINE (0x822DF9D8..0x822DFCB0):
    //   0x822DFA10  bl CanAddPartVolumes(lpType); on false -> "Too many prop volumes\n", return.
    //   0x822DFA58..0x822DFA64  `ori 4` unconditionally (NO IsAddedToContactGen tripwire here,
    //                           and NO physics-enabled guard -- both are the Add twin's, not
    //                           this one's; transcribed, not harmonised).
    //   0x822DFA68  `lbz 0x5D(type)` == muNumberOfParts -> zero-part early out.
    //   0x822DFA6C  `lbz 0x5E(type)` == muNumberOfVolumes, used as the RUNNING PropVolumeID
    //               volume NUMBER base: the parts' volume numbers continue after the
    //               whole-prop volumes (`addi r25,r25,1` per part volume at 0x822DFC60).
    //   0x822DFACC..0x822DFCA0  outer loop over parts, inner loop over that part's volumes.
    //
    // ⚠️ TWO DIFFERENT INDICES, DELIBERATELY -- do not collapse them into one counter.
    // The PropVolumeID is given the RUNNING number (r25, continuing past the prop's own
    // volumes): `clrlwi r5, r25, 24` at 0x822DFB34 is PropVolumeID::Set's second argument.
    // The PropVolumeInstanceID is given the PER-PART volume index (r29, restarting at 0 for
    // every part): `clrlwi r11, r29, 24` at 0x822DFB30 is what the `or` at 0x822DFB44
    // splices into the id's low byte.
    void PropCellManager::AddPropPartsToContactGeneration(PropEntityInstance* lpProp,
                                                          PropPartEntityInstance* lpFirstPart,
                                                          const PropTypeData* lpType,
                                                          PropVolumeInstanceID lVolumeInstanceID,
                                                          InSceneUpdateInterface* lpScene)
    {
        if (!CanAddPartVolumes(lpType))
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "Too many prop volumes\n";
            }
            return;
        }

        lpProp->mu8Flags |= KU_ADDED_TO_CONTACT_GEN_BIT;

        const u32 luNumberOfParts = lpType->GetNumberOfParts();
        if (luNumberOfParts == 0)
        {
            return;
        }

        // The running PropVolumeID volume number, seeded past the whole-prop volumes.
        u32 luVolumeNumber = lpType->GetNumberOfVolumes();

        Vector3 lPadding;                 // `vspltisw128 v127, 0` @0x822DFB04
        lPadding.SetZero();

        PropVolumeID lVolumeID;           // owner seed folded at 0x822DF9F4/0x822DFA0C

        const BrnPhysics::Props::PropPartTypeData* lpaPartTypes = lpType->GetParts();

        for (u32 luPart = 0; luPart < luNumberOfParts; ++luPart)
        {
            // 0x822DFACC/0x822DFADC -- part indices inside the packed handle are 1-BASED
            // (index 0 means "the whole prop"), so the argument is luPart + 1.
            lVolumeInstanceID.SetPartIndex(luPart + 1u);

            const u32 luPartVolumes = lpaPartTypes[luPart].GetNumberOfVolumes();

            for (u32 luVolume = 0; luVolume < luPartVolumes; ++luVolume)
            {
                lVolumeInstanceID.SetVolumeNumber(static_cast<u8>(luVolume));      // 0x822DFB40/44
                lVolumeID.Set(lpProp->muTypeId, static_cast<u8>(luVolumeNumber));  // 0x822DFB48

                const CgsSceneManager::VolumeId         lSceneVolumeId   = lVolumeID;          // :387
                const CgsSceneManager::VolumeInstanceId lSceneInstanceId = lVolumeInstanceID;  // :455
                // r6 == var_FC, which walks lpFirstPart at the 0x50 PropPartEntityInstance
                // stride (0x822DFC94); the part's world transform is at its offset 0.
                lpScene->AddVolumeInstance(lSceneInstanceId, lSceneVolumeId,
                                           lpFirstPart[luPart].mWorldTransform);   // 0x822DFBB0

                const CgsSceneManager::VolumeInstanceId lCollisionInstanceId = lVolumeInstanceID; // :455
                lpScene->AddForCollision(lCollisionInstanceId,
                                         KU_PROP_PART_CULLING_GROUP,
                                         rw::physics::FROZEN_BODY,
                                         lPadding,
                                         CgsSceneManager::SceneManagerIO::E_DO_NOT_ADD_TO_CACHE_MANAGER);

                // 0x822DFC54..0x822DFC64 -- here the count bump follows the queue write
                // (the Add twin bumps it before). Transcribed per-function, not harmonised.
                ++mu16NumberOfPropVolumesInScene;
                ++luVolumeNumber;
            }
        }
    }

    // ========================================================================
    // @ 0x822C6318. Undo AddPropToContactGeneration volume by volume.
    //
    // ASM SPINE: `rlwinm 0,29,29` tripwire -> assert "lpProp->IsAddedToContactGen()" line 435
    // (0x822C6338..0x822C6364); `andi. 0xFB` clears KU_ADDED_TO_CONTACT_GEN_BIT
    // (0x822C6370/74); loop over `lbz 0x5E(type)` volumes doing splice -> owner tripwire ->
    // RemoveForCollision -> owner tripwire -> RemoveVolumeInstance -> count -= 1
    // (`add r11,r11,0xFFFF` on the u16 at 0x822C6410).
    //
    // ⚠️ CALL ORDER: RemoveForCollision FIRST, then RemoveVolumeInstance (0x822C63D8 then
    // 0x822C6404). The PART twin below calls them the OTHER WAY ROUND. That asymmetry is in
    // the shipped binary; both are transcribed as they are.
    void PropCellManager::RemovePropFromContactGeneration(PropEntityInstance* lpProp,
                                                          const PropTypeData* lpType,
                                                          PropVolumeInstanceID lVolumeInstanceID,
                                                          InSceneUpdateInterface* lpScene)
    {
        CGS_ASSERT(lpProp->IsAddedToContactGen(), "lpProp->IsAddedToContactGen()");   // :435

        lpProp->mu8Flags &= static_cast<u8>(~KU_ADDED_TO_CONTACT_GEN_BIT);

        const u32 luNumberOfVolumes = lpType->GetNumberOfVolumes();
        for (u32 luVolume = 0; luVolume < luNumberOfVolumes; ++luVolume)
        {
            lVolumeInstanceID.SetVolumeNumber(static_cast<u8>(luVolume));    // 0x822C63A0/A4

            const CgsSceneManager::VolumeInstanceId lRemoveCollisionId = lVolumeInstanceID; // :455
            lpScene->RemoveForCollision(lRemoveCollisionId);                 // 0x822C63D8

            const CgsSceneManager::VolumeInstanceId lRemoveInstanceId = lVolumeInstanceID;  // :455
            lpScene->RemoveVolumeInstance(lRemoveInstanceId);                // 0x822C6404

            --mu16NumberOfPropVolumesInScene;
        }
    }

    // ========================================================================
    // @ 0x822C6430. The smashed-prop twin of the remove above.
    //
    // ⚠️ MEASURED: there is NO "lpProp->IsAddedToContactGen()" tripwire in this body -- the
    // flag is cleared unconditionally at 0x822C643C..0x822C6458 (`lbz 0x4A(r4)`, `andi. 0xFB`,
    // `stb`) with no compare and no assert block anywhere before it. Its whole-prop sibling
    // does assert. Not harmonised.
    //
    // ⚠️ CALL ORDER (the mirror of the note above): RemoveVolumeInstance FIRST (0x822C64EC),
    // then RemoveForCollision (0x822C6518).
    //
    // Note the shipped signature carries NO lpFirstPart -- removal needs no transform, so it
    // never touches the part instances, only the part TYPE volume counts (`lwz 0x40(type)`
    // walked at the 0x30 console stride, count at `lbz 0x2C`).
    void PropCellManager::RemovePropPartsFromContactGeneration(PropEntityInstance* lpProp,
                                                               const PropTypeData* lpType,
                                                               PropVolumeInstanceID lVolumeInstanceID,
                                                               InSceneUpdateInterface* lpScene)
    {
        lpProp->mu8Flags &= static_cast<u8>(~KU_ADDED_TO_CONTACT_GEN_BIT);

        const u32 luNumberOfParts = lpType->GetNumberOfParts();
        if (luNumberOfParts == 0)
        {
            return;
        }

        const BrnPhysics::Props::PropPartTypeData* lpaPartTypes = lpType->GetParts();

        for (u32 luPart = 0; luPart < luNumberOfParts; ++luPart)
        {
            lVolumeInstanceID.SetPartIndex(luPart + 1u);   // 1-based, as in the Add twin

            const u32 luPartVolumes = lpaPartTypes[luPart].GetNumberOfVolumes();
            for (u32 luVolume = 0; luVolume < luPartVolumes; ++luVolume)
            {
                lVolumeInstanceID.SetVolumeNumber(static_cast<u8>(luVolume));  // 0x822C64B4/B8

                const CgsSceneManager::VolumeInstanceId lRemoveInstanceId = lVolumeInstanceID; // :455
                lpScene->RemoveVolumeInstance(lRemoveInstanceId);              // 0x822C64EC

                const CgsSceneManager::VolumeInstanceId lRemoveCollisionId = lVolumeInstanceID; // :455
                lpScene->RemoveForCollision(lRemoveCollisionId);               // 0x822C6518

                --mu16NumberOfPropVolumesInScene;
            }
        }
    }
}
