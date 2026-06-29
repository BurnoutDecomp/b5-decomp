// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/BrnPropZoneManager.cpp
//
// BrnWorld::PropZoneManager -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// This TU bodies the 11 X360-emitted functions of the prop zone manager (plus the one
// PropGraphicsManager accessor the X360 emitted out of line, Register):
//
//   PropGraphicsManager::Register              @ 0x822A9DE8
//   PropZoneManager::Construct                 @ 0x822F0568
//   PropZoneManager::LoadZone                  @ 0x822FC168
//   PropZoneManager::UnloadZone                @ 0x82303790
//   PropZoneManager::UpdateInstance            @ 0x822F0920
//   PropZoneManager::AddPropPartsToScene       @ 0x822F06A0
//   PropZoneManager::RemovePropFromScene       @ 0x822F0740
//   PropZoneManager::RemovePropPartsFromScene  @ 0x822F0880
//   PropZoneManager::RemovePropFromSim         @ 0x822F07E0
//   PropZoneManager::DeallocatePropInstancesBlock @ 0x822C5C40
//   PropZoneManager::DeallocatePartInstancesBlock @ 0x822C5E18
//
// Notes on faithfulness:
//   - The X360 build inlines CgsContainers::BitArray<N> bit math and the CgsDev::StrStream
//     "<<" debug-message construction at every call site. We restore the logical calls
//     (BitArray::IsBitSet/UnSetBit, CGS_ASSERT) -- the inlined assert message streams have
//     no run-time effect (the asserts only fire in dev builds), so they collapse to a plain
//     CGS_ASSERT with the original message text. This is the project's standard de-inlining.
//   - LoadZone's per-prop loop is 4x-unrolled by the compiler; re-rolled here into one for.
//   - UpdateInstance's transform-validity checks are emitted as inlined RwMath::IsValid /
//     vcmpgtfp SIMD lane tests; restored to IsValid(...) / scalar component comparisons.
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropZoneManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h" // PropPhysicsDataHeader::GetType, PropTypeData
#include "SharedClasses/Physics/Props/BrnPhysicsPropZoneData.h"   // PropZoneData
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h" // OutputBuffer_PreScene / _PostPhysics

namespace BrnWorld
{
    using BrnPhysics::Props::PropPhysicsDataHeader;
    using BrnPhysics::Props::PropTypeData;
    using BrnPhysics::Props::PropZoneData;

    // ---- UpdateInstance displacement / world-floor thresholds (X360 rodata) -------------
    // unk_82FAD840 (@0x822F0B80): a Y-position floor. UpdateInstance computes
    // `lbBelowWorldFloor = (unk_82FAD840 > pos.y)` to gate pulling a frozen prop out of the
    // scene. The same constant is read by ProcessPotentialContactWithPart @ 0x822EEDA8.
    // FLAGGED: the exact float is NOT in .ida-exports (rodata at 0x82FAD840 is not dumped --
    // same situation as BrnStuntManager's unk_82FADED0). Modelled as a large negative Y so a
    // prop that has fallen far below the world reads "below floor"; the predicate STRUCTURE
    // (const > pos.y) is the asm-attested part. Replace the magnitude when rodata lands.
    static const f32 KF_UNDER_WORLD_THRESHOLD_Y = -15000.0f; // FLAG: rodata unk_82FAD840 magnitude

    // unk_82FAD4D0 (@0x822F1504): a per-axis displacement threshold vector compared against
    // |lTransform| (vandc strips the sign bits -> absolute value) to detect the prop's first
    // real move this frame. UpdateInstance sets KU_MOVED_BIT only when this compare SUCCEEDS
    // (at least one axis exceeds the threshold) AND the bit was previously unset. The four lanes of
    // the rodata vector are NOT in .ida-exports; FLAGGED: modelled as a small per-axis epsilon
    // so any non-trivial displacement trips the "moved" path. Predicate STRUCTURE is asm-pinned.
    static const f32 KF_FIRST_MOVE_THRESHOLD = 0.01f;        // FLAG: rodata unk_82FAD4D0 magnitude

    // RwMath::IsValid(matrix) -- the X360 inlines this as a per-lane vcmpeqfp self-compare
    // (a finite float equals itself; a NaN does not). Restored here as a finite check over
    // every translation/basis lane of the affine transform.
    static bool IsValid(const Matrix44Affine& lTransform)
    {
        const Vector3& lR = lTransform.Right();
        const Vector3& lU = lTransform.Up();
        const Vector3& lA = lTransform.At();
        const Vector3& lP = lTransform.Pos();
        return (lR.x == lR.x) && (lR.y == lR.y) && (lR.z == lR.z)
            && (lU.x == lU.x) && (lU.y == lU.y) && (lU.z == lU.z)
            && (lA.x == lA.x) && (lA.y == lA.y) && (lA.z == lA.z)
            && (lP.x == lP.x) && (lP.y == lP.y) && (lP.z == lP.z);
    }

    // ========================================================================
    // PropGraphicsManager::Register @ 0x822A9DE8
    // ------------------------------------------------------------------------
    // Install a prop-graphics record under its own type id. The X360 reads the type id
    // from the FIRST word of the PropGraphics record (`lwz r30,0(r29)`), bounds-checks it
    // against KU_MAX_PROP_TYPES, then either bumps the slot's ref count (if already
    // populated) or installs the pointer with ref count 1. Always returns true.
    bool PropGraphicsManager::Register(const PropGraphics* lpPropGraphics)
    {
        CGS_ASSERT(lpPropGraphics != nullptr, "lpPropGraphics");

        // The PropGraphics record begins with its prop type id (X360 `*lpPropGraphics`).
        const u32 luType = *reinterpret_cast<const u32*>(lpPropGraphics);
        CGS_ASSERT(luType < KU_MAX_PROP_TYPES, "luType < BrnPhysics::Props::KU_MAX_PROP_TYPES");

        PropGraphicsReference& lrReference = maPropGraphicsReferences[luType];
        if (lrReference.mpPropGraphics != nullptr)
        {
            ++lrReference.mu8RefCount;
        }
        else
        {
            lrReference.mpPropGraphics = lpPropGraphics;
            lrReference.mu8RefCount    = 1;
        }
        return true;
    }

    // ========================================================================
    // PropZoneManager::Construct @ 0x822F0568
    // ------------------------------------------------------------------------
    // Reset every per-zone tracking array to "unloaded", zero the loaded-prop count,
    // wire the embedded PropCellManager's prop/part pool pointers to maProps/maParts,
    // clear the used-slot/respawn/rotation bit arrays, and Construct the traffic-light
    // restore list.
    void PropZoneManager::Construct()
    {
        mu16NumberOfLoadedProps = 0;

        for (u32 luZoneId = 0; luZoneId < KU_MAX_ZONES; ++luZoneId)
        {
            mauStartIndexOfZone[luZoneId]    = KU_UNLOADED_ZONE;
            mauStartIndexOfParts[luZoneId]   = KU_UNLOADED_ZONE;
            mauNumberOfPropsInZone[luZoneId] = 0;
            mauNumberOfPartsInZone[luZoneId] = 0;
        }

        // Construct the embedded cell manager, handing it back-pointers into our prop/part
        // pools (the X360 inlines PropCellManager::Construct here -- it stores
        // mpaProps @ this+0x770 = this+2432 == &maProps[0], and
        // mpaPropParts @ this+0x774, zeroes miNumLoadedCells / the in-sim counters /
        // the in-scene counts, and clears mPhysicalProps/Parts).
        //
        // FINDING (offset reconciliation): the asm computes the stored mpaPropParts as
        // `this + 435968` (0x822F0568: `addis r7,r31,7; addi r7,r7,-0x5900` -> +0x6A700),
        // which is 96 bytes PAST the modelled maParts base (console +435872). The same
        // +offset shows in UpdateInstance's part addressing (`...+435888`). This 96-byte
        // console gap is a layout artifact of the shipped X360 part pool that the PC model
        // (which lays maParts out as a clean PropPartEntityInstance[] right after maProps,
        // and does NOT byte-match console offsets due to pointer widening -- see the .h
        // banner) does not reproduce. Semantically both the asm pointer and `&maParts[0]`
        // address the part pool base, so the faithful reconstruction passes &maParts[0]
        // (named, host-correct) rather than a raw `this+435968` offset hack. Documented,
        // not reproduced: closing the gap would require an un-attested padding member whose
        // size/meaning is not exercised by any bodied function in this pass.
        mCellManager.Construct(&maProps[0], &maParts[0]);

        maUsedProps.UnSetAll();
        maUsedParts.UnSetAll();
        mUsedRotationParams.UnSetAll();
        // maDontRespawnProps / maRespawnDifferentProps are cleared per-zone span by Construct
        // (X360 memset(this+797424,0,680) and memset(this+798104,0,680) -- the two 5400-bit
        // arrays). UnSetAll clears the whole array, matching the zeroed span.
        maDontRespawnProps.UnSetAll();
        maRespawnDifferentProps.UnSetAll();

        mauTrafficLightsToRestore.Construct();
    }

    // ========================================================================
    // PropZoneManager::DeallocatePropInstancesBlock @ 0x822C5C40
    // ------------------------------------------------------------------------
    // Free the prop-pool slot a zone occupied: assert the block is a whole slot, derive
    // the slot index (start / KU_SIZE_OF_PROP_ZONE_SLOT), assert that slot is currently
    // marked used, then clear its used bit.
    void PropZoneManager::DeallocatePropInstancesBlock(u32 luStartOfBlock, u32 luSizeOfBlock)
    {
        CGS_ASSERT(luSizeOfBlock < KU_MAX_PROP_INSTANCES_PER_ZONE,
                   "luSizeOfBlock < BrnPhysics::Props::KU_MAX_PROP_INSTANCES_PER_ZONE");
        CGS_ASSERT(luStartOfBlock % KU_SIZE_OF_PROP_ZONE_SLOT == 0,
                   "luStartOfBlock % KU_SIZE_OF_PROP_ZONE_SLOT == 0");

        const u32 luSlotIndex = luStartOfBlock / KU_SIZE_OF_PROP_ZONE_SLOT;
        CGS_ASSERT(luSlotIndex < KU_NUM_ZONE_SLOTS, "invalid index");
        CGS_ASSERT(maUsedProps.IsBitSet(luSlotIndex), "maUsedProps.IsBitSet(liSlotIndex)");
        maUsedProps.UnSetBit(luSlotIndex);
    }

    // ========================================================================
    // PropZoneManager::DeallocatePartInstancesBlock @ 0x822C5E18
    // ------------------------------------------------------------------------
    // The part-pool twin of the above (KU_SIZE_OF_PART_ZONE_SLOT slots, maUsedParts).
    void PropZoneManager::DeallocatePartInstancesBlock(u32 luStartOfBlock, u32 luSizeOfBlock)
    {
        CGS_ASSERT(luSizeOfBlock < KU_MAX_PROP_PARTS_PER_ZONE,
                   "luSizeOfBlock < BrnPhysics::Props::KU_MAX_PROP_PARTS_PER_ZONE");
        CGS_ASSERT(luStartOfBlock % KU_SIZE_OF_PART_ZONE_SLOT == 0,
                   "luStartOfBlock % KU_SIZE_OF_PART_ZONE_SLOT == 0");

        const u32 luSlotIndex = luStartOfBlock / KU_SIZE_OF_PART_ZONE_SLOT;
        CGS_ASSERT(luSlotIndex < KU_NUM_ZONE_SLOTS, "invalid index");
        CGS_ASSERT(maUsedParts.IsBitSet(luSlotIndex), "maUsedParts.IsBitSet(liSlotIndex)");
        maUsedParts.UnSetBit(luSlotIndex);
    }

    // ========================================================================
    // PropZoneManager::AddPropPartsToScene @ 0x822F06A0
    // ------------------------------------------------------------------------
    // Thin forwarder to mCellManager: compute the prop's index within its zone (its volume
    // entity index minus the zone's prop start index), assert it is non-negative, and
    // forward along with the zone id.
    void PropZoneManager::AddPropPartsToScene(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                              PropVolumeInstanceID lVolumeInstanceID, InSceneUpdateInterface* lpScene)
    {
        const u16 lu16ZoneId         = lpProp->mu16ZoneIndex;
        const s32 liPropIndexInZone  =
            static_cast<s32>(lVolumeInstanceID.GetEntityIndex()) - mauStartIndexOfZone[lu16ZoneId];
        CGS_ASSERT(liPropIndexInZone >= 0, "liPropIndexInZone >= 0");

        mCellManager.AddPropPartsToScene(lpProp, lpType, lVolumeInstanceID, lpScene,
                                         lu16ZoneId, liPropIndexInZone);
    }

    // ========================================================================
    // PropZoneManager::RemovePropFromScene @ 0x822F0740
    // ------------------------------------------------------------------------
    void PropZoneManager::RemovePropFromScene(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                              PropVolumeInstanceID lVolumeInstanceID, InSceneUpdateInterface* lpScene)
    {
        const u16 lu16ZoneId         = lpProp->mu16ZoneIndex;
        const s32 liPropIndexInZone  =
            static_cast<s32>(lVolumeInstanceID.GetEntityIndex()) - mauStartIndexOfZone[lu16ZoneId];
        CGS_ASSERT(liPropIndexInZone >= 0, "liPropIndexInZone >= 0");

        mCellManager.RemovePropFromScene(lpProp, lpType, lVolumeInstanceID, lpScene,
                                         lu16ZoneId, liPropIndexInZone);
    }

    // ========================================================================
    // PropZoneManager::RemovePropPartsFromScene @ 0x822F0880
    // ------------------------------------------------------------------------
    void PropZoneManager::RemovePropPartsFromScene(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                                   PropVolumeInstanceID lVolumeInstanceID, InSceneUpdateInterface* lpScene)
    {
        const u16 lu16ZoneId         = lpProp->mu16ZoneIndex;
        const s32 liPropIndexInZone  =
            static_cast<s32>(lVolumeInstanceID.GetEntityIndex()) - mauStartIndexOfZone[lu16ZoneId];
        CGS_ASSERT(liPropIndexInZone >= 0, "liPropIndexInZone >= 0");

        mCellManager.RemovePropPartsFromScene(lpProp, lpType, lVolumeInstanceID, lpScene,
                                              lu16ZoneId, liPropIndexInZone);
    }

    // ========================================================================
    // PropZoneManager::RemovePropFromSim @ 0x822F07E0
    // ------------------------------------------------------------------------
    // Assert the prop is in a valid (physical) state, then forward to mCellManager with
    // the output buffer's prop-input interface.
    void PropZoneManager::RemovePropFromSim(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                            PropVolumeInstanceID lVolumeInstanceID,
                                            PropEntityIO::OutputBuffer_PreScene* lpOutput)
    {
        CGS_ASSERT(lpProp->mu8State < E_STATE_COUNT, "mu8State < E_STATE_COUNT");
        CGS_ASSERT(lpProp->mu8State == E_PHYSICAL, "lpProp->IsPhysical()");

        PropCellManager::PropInputInterface* lpPropInput =
            reinterpret_cast<PropCellManager::PropInputInterface*>(lpOutput->GetPropInputInterface());
        mCellManager.RemovePropFromSim(lpProp, lpType, lVolumeInstanceID, lpPropInput);
    }

    // ========================================================================
    // PropZoneManager::LoadZone @ 0x822FC168
    // ------------------------------------------------------------------------
    // Allocate one prop-pool slot + one part-pool slot for the zone, record its start
    // indices / counts, register its cells, then load every prop (and its parts) into the
    // pools via LoadProp. Returns the prop start index.
    //
    // The X360 build 4x-unrolls the prop loop and inlines the bit/stream math; re-rolled
    // and de-inlined here.
    s32 PropZoneManager::LoadZone(const PropZoneData* lpZoneData, const PropPhysicsDataHeader* lpTypes,
                                  Vector3 lPlayerPosition, PropEntityIO::OutputBuffer_PreScene* lpOutput)
    {
        const u16 lu16ZoneId       = lpZoneData->GetZoneId();
        const u32 luNumberOfProps  = lpZoneData->GetNumberOfProps();
        const u32 luNumberOfParts  = lpZoneData->GetNumberOfInstances() - luNumberOfProps;

        // The scene-input interface that LoadProp adds the loaded props' entities to.
        InSceneUpdateInterface* lpScene =
            reinterpret_cast<InSceneUpdateInterface*>(lpOutput->GetPropInputInterface());

        CGS_ASSERT(!IsZoneLoaded(lu16ZoneId), "!IsZoneLoaded( liZoneId )");

        s32 liPropStartIndex = AllocatePropInstancesBlock(luNumberOfProps);
        s32 liPartStartIndex = AllocatePartInstancesBlock(luNumberOfParts);

        CGS_ASSERT(liPropStartIndex != -1, "No free space to load zone");
        CGS_ASSERT(liPartStartIndex != -1, "No free space to load zone");

        mauStartIndexOfZone[lu16ZoneId]    = static_cast<u16>(liPropStartIndex);
        mauNumberOfPropsInZone[lu16ZoneId] = static_cast<u16>(luNumberOfProps);
        mauStartIndexOfParts[lu16ZoneId]   = static_cast<u16>(liPartStartIndex);
        mauNumberOfPartsInZone[lu16ZoneId] = static_cast<u16>(luNumberOfParts);

        mCellManager.AddCells(lpZoneData, liPropStartIndex);

        mu16NumberOfLoadedProps = static_cast<u16>(mu16NumberOfLoadedProps + luNumberOfProps);
        CGS_ASSERT(mu16NumberOfLoadedProps < KU_MAX_LOADED_PROP_INSTANCES, "Number of props");

        // Reset the running prop/part pool write cursors LoadProp advances (the X360
        // passes &liPropPoolIndex / &liPartPoolIndex by reference, seeded to the slot
        // starts; *(this+841232) -- mauTrafficLightsToRestore count -- is also reset).
        s32 liPropPoolIndex = liPropStartIndex;
        s32 liPartPoolIndex = liPartStartIndex;
        mauTrafficLightsToRestore.Clear();

        const s32 liNumberOfPropsInZone = static_cast<s32>(luNumberOfProps);
        for (s32 liZoneDataPropIndex = 0; liZoneDataPropIndex < liNumberOfPropsInZone; ++liZoneDataPropIndex)
        {
            LoadProp(&liPropPoolIndex, &liPartPoolIndex, liZoneDataPropIndex,
                     lpZoneData, lpTypes, lPlayerPosition, lpScene);
        }

        // Post-conditions the X360 asserts: every prop and every part landed in the slot.
        CGS_ASSERT((liPropPoolIndex - liPropStartIndex) == liNumberOfPropsInZone,
                   "Current instance / start index / number of props in zone");
        CGS_ASSERT((liPartPoolIndex - liPartStartIndex) == static_cast<s32>(luNumberOfParts),
                   "(liPartPoolIndex - liPartsStartIndex) == liNumPartsInZone");

        return liPropStartIndex;
    }

    // ========================================================================
    // PropZoneManager::UnloadZone @ 0x82303790
    // ------------------------------------------------------------------------
    // Remove every prop and part of the zone from scene/sim/contact-generation, free any
    // animated-rotation slots they held, deallocate the zone's prop/part pool slots, mark
    // the zone unloaded, and unlink the zone's traffic-light restore list node.
    void PropZoneManager::UnloadZone(u16 lu16ZoneId, const PropPhysicsDataHeader* lpTypes,
                                     void* lpRecentlyBrokenProps, PropEntityIO::OutputBuffer_PreScene* lpOutput)
    {
        CGS_ASSERT(IsZoneLoaded(lu16ZoneId), "IsZoneLoaded(luZoneId)");

        const s32 liStartIndex   = mauStartIndexOfZone[lu16ZoneId];
        const s32 liNumProps     = mauNumberOfPropsInZone[lu16ZoneId];
        const s32 liEndIndex     = liStartIndex + liNumProps;

        // Tear down the zone's cells first (this also publishes recently-broken props).
        mCellManager.RemoveCells(static_cast<s16>(lu16ZoneId), liStartIndex, liNumProps,
                                 lpTypes, lpRecentlyBrokenProps, lpOutput);

        mu16NumberOfLoadedProps = static_cast<u16>(mu16NumberOfLoadedProps - mauNumberOfPropsInZone[lu16ZoneId]);

        const u32 luNumberOfPropTypes = lpTypes->GetNumberOfPropTypes();
        for (s32 liPropIndex = liStartIndex; liPropIndex < liEndIndex; ++liPropIndex)
        {
            PropEntityInstance* lpProp = &maProps[liPropIndex];
            const u32 luTypeId = lpProp->muTypeId;

            CGS_ASSERT(luTypeId < KU_MAX_PROP_TYPES, "liTypeId < KU_MAX_PROP_TYPES");
            CGS_ASSERT(luTypeId < luNumberOfPropTypes, "liTypeId < muNumberOfPropTypes");
            const PropTypeData* lpType = lpTypes->GetType(luTypeId);

            // The volume-instance handle of this prop (entity index = its pool index).
            PropVolumeInstanceID lVolumeInstanceID;
            lVolumeInstanceID.SetEntityIndex(static_cast<u16>(liPropIndex));

            // Pull it out of contact generation if it was added to it.
            if ((lpProp->mu8Flags & KU_ADDED_TO_CONTACT_GEN_BIT) != 0)
            {
                CGS_ASSERT(lpProp->mu8State < E_STATE_COUNT, "mu8State < E_STATE_COUNT");
                InSceneUpdateInterface* lpScene =
                    reinterpret_cast<InSceneUpdateInterface*>(lpOutput->GetPropInputInterface());
                // X360 branch on (mu8State >= E_SMASHED): a smashed prop has live parts.
                if (lpProp->mu8State >= E_SMASHED)
                {
                    mCellManager.RemovePropPartsFromContactGeneration(lpProp, lpType, lVolumeInstanceID, lpScene);
                }
                else
                {
                    mCellManager.RemovePropFromContactGeneration(lpProp, lpType, lVolumeInstanceID, lpScene);
                }
            }

            // Pull it out of the scene (and sim) if it was added to the scene.
            if ((lpProp->mu8Flags & KU_ADDED_TO_SCENE_BIT) != 0)
            {
                CGS_ASSERT(lpProp->mu8State < E_STATE_COUNT, "mu8State < E_STATE_COUNT");
                InSceneUpdateInterface* lpScene =
                    reinterpret_cast<InSceneUpdateInterface*>(lpOutput->GetPropInputInterface());
                if (lpProp->mu8State >= E_SMASHED)
                {
                    RemovePropPartsFromScene(lpProp, lpType, lVolumeInstanceID, lpScene);
                    mCellManager.RemovePropPartsFromSimIfPhysical(lpProp, lpType, lVolumeInstanceID, lpOutput);
                }
                else
                {
                    RemovePropFromScene(lpProp, lpType, lVolumeInstanceID, lpScene);
                    CGS_ASSERT(lpProp->mu8State < E_STATE_COUNT, "mu8State < E_STATE_COUNT");
                    if (lpProp->mu8State == E_PHYSICAL)
                    {
                        RemovePropFromSim(lpProp, lpType, lVolumeInstanceID, lpOutput);
                    }
                }
            }

            // Free any animated-rotation parameter slot the prop held.
            const s8 li8RotationParamsIndex = lpProp->mi8RotationParamsIndex;
            if (li8RotationParamsIndex != -1)
            {
                CGS_ASSERT(static_cast<u32>(li8RotationParamsIndex) < KU_MAX_ROTATION_PARAMS, "invalid index");
                CGS_ASSERT(mUsedRotationParams.IsBitSet(li8RotationParamsIndex),
                           "mUsedRotationParams.IsBitSet( lpProp->GetRotationParamsIndex() )");
                mUsedRotationParams.UnSetBit(li8RotationParamsIndex);
            }
        }

        // Deallocate the prop/part pool slots and mark the zone unloaded.
        DeallocatePropInstancesBlock(mauStartIndexOfZone[lu16ZoneId], mauNumberOfPropsInZone[lu16ZoneId]);
        DeallocatePartInstancesBlock(mauStartIndexOfParts[lu16ZoneId], mauNumberOfPartsInZone[lu16ZoneId]);

        mauStartIndexOfZone[lu16ZoneId]    = KU_UNLOADED_ZONE;
        mauStartIndexOfParts[lu16ZoneId]   = KU_UNLOADED_ZONE;
        mauNumberOfPropsInZone[lu16ZoneId] = 0;
        mauNumberOfPartsInZone[lu16ZoneId] = 0;

        // NOTE: the X360 tail also unlinks the passed PropPhysicsResourcePtr from its
        // resource's intrusive user list (`*(types+12)/(types+16)` prev/next splice). That
        // is the CgsResource::ResourcePtr smart-pointer's own lifecycle bookkeeping, not
        // zone-manager logic; this recon takes the already-resolved PropPhysicsDataHeader*
        // and does not model the ResourcePtr wrapper, so that splice is intentionally not
        // reproduced here (FLAGGED -- belongs to the ResourcePtr type, recovered by its TU).
    }

    // ========================================================================
    // PropZoneManager::UpdateInstance @ 0x822F0920
    // ------------------------------------------------------------------------
    // Apply one physics-result for a prop instance: validate the transform, look up the
    // prop, test a "below world floor" predicate (translation Y vs rodata unk_82FAD840) and
    // a per-axis +/-15000 sanity assert, and -- for the non-frozen path -- detect the prop's
    // first move, set KU_MOVED_BIT + publish the move events, then copy the new transform and
    // push the entity + per-volume transforms to the scene-update interface.
    //
    // FAITHFULNESS: the shipped X360 body is extremely large because the compiler inlines
    // RwMath::IsValid (per-lane vcmpeqfp -- restored as IsValid()), the per-axis +/-15000
    // bounds asserts (vcmpgtfp), and the StrStream assert messages (collapsed to CGS_ASSERT).
    // Three predicates are NOT validity checks and are restored from the asm exactly:
    //   * lbBelowWorldFloor = (unk_82FAD840 > pos.y)            [frozen-prop scene removal]
    //   * the first-move gate sets KU_MOVED_BIT only when the displacement compare
    //     vcmpgtfp(|lTransform|, unk_82FAD4D0) SUCCEEDS (>=1 axis exceeds) and the bit is unset.
    //   * both per-volume scene-push loops iterate to PropTypeData's volume counts
    //     (whole-prop +0x5E; per-part group +0x2C), NOT a hard-coded 0.
    // Two move events (PropVFXLocatorEvent::AddEventSafe @0x822F15C8, HitOverheadSignEvent::
    // AddEvent @0x822F162C) and the ResourcePtr-list tail splice remain FLAGGED (see inline).
    // The prop-index math (entity index -> prop pool slot) and the part-copy loop are
    // reproduced as the X360 performs them.
    void PropZoneManager::UpdateInstance(PropEntityID lEntityId, Matrix44Affine lTransform,
                                         Vector3 lLinearVelocity, Vector3 lAngularVelocity, bool lbFrozen,
                                         const PropPhysicsDataHeader* lpTypeData, f32 lfTimeStep,
                                         PropEntityIO::OutputBuffer_PostPhysics* lpOutput)
    {
        (void)lLinearVelocity;
        (void)lAngularVelocity;

        CGS_ASSERT(IsValid(lTransform), "RwMath::IsValid( lTransform )");

        // "Below world floor" predicate (X360 v108 @ 0x822F0B80-0x822F0BA4):
        //   vcmpgtfp v0, [unk_82FAD840], splat(lTransform.Pos().y)
        // i.e. KF_UNDER_WORLD_THRESHOLD_Y > pos.y  -- true when the prop's translation Y has
        // dropped below the rodata floor constant. (The sibling
        // PropEntityModule::ProcessPotentialContactWithPart @ 0x822EEDA8 compares the SAME
        // unk_82FAD840 against a transform's Y lane, confirming it is a world-floor / Y
        // threshold, not a NaN check.) The frozen-prop branch below uses this to decide
        // whether the prop must be pulled out of the scene / contact generation this frame.
        const f32 lfPositionY = lTransform.Pos().y;
        const bool lbBelowWorldFloor = (KF_UNDER_WORLD_THRESHOLD_Y > lfPositionY);

        // The destination scene-update interface (X360 OutputBuffer_PostPhysics::GetScene...).
        InSceneUpdateInterface* lpScene =
            reinterpret_cast<InSceneUpdateInterface*>(lpOutput->GetSceneInputInterface());

        const u32 luPropIndex = lEntityId.GetEntityIndex();
        CGS_ASSERT(luPropIndex < KU_MAX_LOADED_PROP_INSTANCES,
                   "liPropIndex < static_cast<int32_t>( BrnPhysics::Props::KU_MAX_LOADED_PROP_INSTANCES )");

        PropEntityInstance* lpProp = &maProps[luPropIndex];
        const PropTypeData* lpType = lpTypeData->GetType(lpProp->muTypeId);

        // The prop must already be in the scene (flag bit 1) and past the static state.
        CGS_ASSERT((lpProp->mu8Flags & KU_ADDED_TO_SCENE_BIT) != 0,
                   "Entity id: lpProp added to scene");
        CGS_ASSERT(lpProp->mu8State < E_STATE_COUNT, "mu8State < E_STATE_COUNT");
        CGS_ASSERT(lpProp->mu8State > E_STATIC, "lpProp->GetState() > E_STATIC");

        const u32 luPartId = lEntityId.GetPartIndex();
        if (luPartId != 0)
        {
            // --- updating one physical PART of a smashed prop ---
            CGS_ASSERT(lpProp->IsSmashed(), "lpProp->IsSmashed()");

            // Part pool slot = the prop's first-part index + (partId - 1) ... the X360
            // computes (a15 & 0x3FF) + mu16PartsIndex; partId already carries the +1, so
            // the addressed part is maParts[firstPart + partId].
            const u32 luPartPoolIndex = static_cast<u32>(lpProp->mu16PartsIndex) + luPartId;
            PropPartEntityInstance* lpPart = &maParts[luPartPoolIndex];

            // Install the new world transform into the part.
            lpPart->mWorldTransform = lTransform;

            // Look up the part's PropTypeData and the volume-group descriptor for this
            // part. X360 0x822F17C0-0x822F17F8: GetType(part->muTypeId), then index its
            // part-volume-group array (PropTypeData console +0x40) by the part's muPartId
            // at a 48-byte stride. The descriptor's +0x2C count bounds the scene push.
            const PropTypeData* lpPartType = lpTypeData->GetType(lpPart->muTypeId);
            const BrnPhysics::Props::PropPartVolumeGroup& lVolumeGroup =
                lpPartType->GetPartVolumeGroups()[lpPart->muPartId];

            // The volume-instance handle for this part's prop.
            PropVolumeInstanceID lVolumeInstanceID;
            lVolumeInstanceID.SetPropEntityId(lEntityId);

            if (lbFrozen)
            {
                // Freezing: drop the part out of the sim and free its physical slot.
                --mCellManager.miNumPartsInSim;
                CGS_ASSERT(lpPart->mbPhysical, "lpPart->mbPhysical");
                const u8 lu8PhysicsIndex = lpPart->mu8PhysicsIndex;
                lpPart->mbPhysical = false;
                mCellManager.FreePhysicalPartSlot(lu8PhysicsIndex);
            }
            else
            {
                mCellManager.IncrementPartsTimeInSim(lpPart->mu8PhysicsIndex, lfTimeStep);
                // Push the part's entity position + per-volume transforms to the scene.
                lpScene->SetEntityPosition(static_cast<EntityId>(lEntityId).muValue,
                                           lpPart->mWorldTransform);
                // X360 0x822F1890: gate on (mu8Flags & KU_ADDED_TO_CONTACT_GEN_BIT) and a
                // non-zero per-part volume count, then loop to lVolumeGroup.GetNumberOfVolumes()
                // (console +0x2C): `do { Set(volume); SetVolumeInstanceTransform }
                // while (volume < *(group+0x2C))`.
                if ((lpProp->mu8Flags & KU_ADDED_TO_CONTACT_GEN_BIT) != 0)
                {
                    const u32 luNumVolumes = lVolumeGroup.GetNumberOfVolumes();
                    for (u32 luVolume = 0; luVolume < luNumVolumes; ++luVolume)
                    {
                        lVolumeInstanceID.Set(lEntityId, static_cast<u8>(luVolume));
                        lpScene->SetVolumeInstanceTransform(lVolumeInstanceID.mVolumeInstanceId,
                                                            lpPart->mWorldTransform);
                    }
                }
            }
        }
        else
        {
            // --- updating the whole (non-smashed) prop ---
            CGS_ASSERT(!lpProp->IsSmashed(), "!lpProp->IsSmashed()");
            CGS_ASSERT(IsValid(lTransform), "IsValid(lTransform)");

            // "Fell out of the world" / sanity bounds: each position axis must be within
            // +/-KF_MAX_VALID_POSITION_ALONG_AXIS (Y under-bound logs which zone the prop
            // fell out of -- debug only, no early-out). Restored from the per-axis vcmpgtfp
            // asserts as component comparisons against the transform's translation.
            static const f32 KF_MAX_VALID_POSITION_ALONG_AXIS = 15000.0f;
            const Vector3& lPosition = lTransform.Pos();
            CGS_ASSERT(lPosition.x < KF_MAX_VALID_POSITION_ALONG_AXIS, "Exploding prop, resource id");
            CGS_ASSERT(lPosition.y < KF_MAX_VALID_POSITION_ALONG_AXIS, "Exploding prop, resource id");
            CGS_ASSERT(lPosition.z < KF_MAX_VALID_POSITION_ALONG_AXIS, "Exploding prop, resource id");
            CGS_ASSERT(lPosition.x > -KF_MAX_VALID_POSITION_ALONG_AXIS, "Exploding prop, resource id");
            CGS_ASSERT(lPosition.z > -KF_MAX_VALID_POSITION_ALONG_AXIS, "Exploding prop, resource id");

            // The volume-instance handle this prop's entity owns.
            PropVolumeInstanceID lVolumeInstanceID;

            if (lbFrozen)
            {
                // Freezing this prop: it leaves the sim. Decrement the live-prop count,
                // drop its state back to E_MOVED, and free its physical slot.
                --mCellManager.miNumPropsInSim;
                lpProp->mu8State = static_cast<u8>(E_MOVED);
                mCellManager.FreePhysicalPropSlot(lpProp->mu8PhysicsIndex);

                // If the prop has dropped below the world floor this frame, pull it out of
                // the scene / contact generation (the X360 gates this on the below-floor
                // predicate `unk_82FAD840 > pos.y` computed above).
                if (lbBelowWorldFloor)
                {
                    lVolumeInstanceID.Set(lEntityId, 0);
                    if ((lpProp->mu8Flags & KU_ADDED_TO_CONTACT_GEN_BIT) != 0)
                    {
                        mCellManager.RemovePropFromContactGeneration(lpProp, lpType, lVolumeInstanceID, lpScene);
                    }
                    if ((lpProp->mu8Flags & KU_ADDED_TO_SCENE_BIT) != 0)
                    {
                        RemovePropFromScene(lpProp, lpType, lVolumeInstanceID, lpScene);
                    }
                }
            }
            else
            {
                // Still simulating: accrue its time-in-sim.
                mCellManager.IncrementPropsTimeInSim(lpProp->mu8PhysicsIndex, lfTimeStep);
            }

            if (!lbFrozen)
            {
                // First-move detection (X360 0x822F1500-0x822F1638). The build takes the
                // absolute value of the incoming transform's first basis row
                // (`vandc(lTransform, signMask)`), rearranges two lanes, and compares it
                // against the rodata threshold vector unk_82FAD4D0 with vcmpgtfp. The asm extracts
                // the CR6.2 "all-false / no-lane-greater" bit and `bne` SKIPS the move-set when it is
                // set; so it sets KU_MOVED_BIT (and fires the move events) only when the compare
                // SUCCEEDS -- i.e. AT LEAST ONE axis EXCEEDS the threshold -- AND the prop was not
                // already flagged moved (`if (allFalseBit || (flags & 0x20)) skip; else { ori 0x20; stb }`).
                const Vector3& lRow0 = lTransform.Right();
                const bool lbDisplacementExceedsThreshold =
                       (lRow0.x < -KF_FIRST_MOVE_THRESHOLD || lRow0.x > KF_FIRST_MOVE_THRESHOLD)
                    || (lRow0.y < -KF_FIRST_MOVE_THRESHOLD || lRow0.y > KF_FIRST_MOVE_THRESHOLD)
                    || (lRow0.z < -KF_FIRST_MOVE_THRESHOLD || lRow0.z > KF_FIRST_MOVE_THRESHOLD);

                if (lbDisplacementExceedsThreshold && (lpProp->mu8Flags & KU_MOVED_BIT) == 0)
                {
                    lpProp->mu8Flags = static_cast<u8>(lpProp->mu8Flags | KU_MOVED_BIT);

                    // FLAGGED EVENT PUBLISHES (gate above is now asm-faithful; the event
                    // payload types are large unreconstructed IO records this TU does not own):
                    //   * PropEntityIO::PropVFXLocatorEvent::AddEventSafe @ 0x822F15C8 -- pushes
                    //     a VFX-locator record (the prop's transform rows + zone id) onto the
                    //     post-physics output buffer's queue.
                    //   * for overhead-sign prop graphics ids 500950 / 500930 / 506050
                    //     (lpType->GetGraphicsId(), console +0x58), additionally
                    //     BrnGameState::GameStateModuleIO::HitOverheadSignEvent::AddEvent
                    //     @ 0x822F162C with the prop index.
                    // FLAGGED: not reproduced -- both targets fork event-queue / GameState IO
                    // types outside this TU. The overhead-sign id set is asm-attested here:
                    const u32 luGraphicsId = lpType->GetGraphicsId();
                    const bool lbOverheadSign =
                        (luGraphicsId == 500950u || luGraphicsId == 500930u || luGraphicsId == 506050u);
                    (void)lbOverheadSign;
                }

                // Install the new world transform and push the entity position to the scene.
                lpProp->mWorldTransform = lTransform;
                lVolumeInstanceID.Set(lEntityId, 0);
                lpScene->SetEntityPosition(static_cast<EntityId>(lEntityId).muValue, lpProp->mWorldTransform);

                // Push every per-volume transform when the prop is in contact generation.
                // X360 0x822F16A0: gate on (mu8Flags & KU_ADDED_TO_CONTACT_GEN_BIT) and a
                // non-zero whole-prop volume count, then loop to lpType->GetNumberOfVolumes()
                // (console PropTypeData +0x5E): `do { Set(volume); SetVolumeInstanceTransform }
                // while (volume < *(type+0x5E))`.
                if ((lpProp->mu8Flags & KU_ADDED_TO_CONTACT_GEN_BIT) != 0)
                {
                    const u32 luNumVolumes = lpType->GetNumberOfVolumes();
                    for (u32 luVolume = 0; luVolume < luNumVolumes; ++luVolume)
                    {
                        lVolumeInstanceID.Set(lEntityId, static_cast<u8>(luVolume));
                        lpScene->SetVolumeInstanceTransform(lVolumeInstanceID.mVolumeInstanceId,
                                                            lpProp->mWorldTransform);
                    }
                }
            }
        }
    }
    // NOTE (X360 tail @ 0x822F18F0-0x822F191C): the build unlinks the passed
    // PropPhysicsResourcePtr (the 5th register arg `a5`) from its resource's intrusive
    // doubly-linked user list and re-points it at itself:
    //     prev = *(a5+12); if (prev) *(prev+16) = *(a5+16);   // splice next over us
    //     next = *(a5+16); if (next) *(next+12) = *(a5+12);   // splice prev over us
    //     *(a5+16) = a5; *(a5+12) = a5;                       // isolate (self-link)
    // This is CgsResource::ResourcePtr's own copy/destroy bookkeeping (prev@+12 / next@+16),
    // NOT zone-manager logic. This recon takes the already-resolved PropPhysicsDataHeader*
    // (not the ResourcePtr wrapper), so those +12/+16 members are not in this TU's model;
    // the splice is intentionally not reproduced (FLAGGED -- belongs to the ResourcePtr type,
    // recovered by its own TU; reproducing it here would require re-typing the parameter and
    // every call site across TUs).
}
