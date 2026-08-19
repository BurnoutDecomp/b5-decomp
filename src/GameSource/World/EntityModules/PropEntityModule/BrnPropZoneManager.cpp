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
#include "SharedClasses/Physics/Props/BrnPropConstants.h"        // BrnPhysics::Props::KVF_PROP_FLOOR (NEW 2026-08-18)
#include "SharedClasses/Physics/Props/BrnPhysicsPropZoneData.h"   // PropZoneData, PropCellData, PropCellId
#include "SharedClasses/Physics/Props/BrnPhysicsPropInstanceData.h" // PropInstanceData (the 80B serialised record LoadProp reads)
#include "SharedClasses/Physics/Props/BrnPropGraphicsList.h"        // PropGraphics / PropPartGraphics (PropGraphicsManager table)
#include "GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.h" // PropInputInterface::RemoveAllPropsAndParts
#include "GameSource/Replays/Serialisers/BrnReplayPropEntitySerialiser.h"  // PropEntitySerialiser::GetStaticLayout / PropSerialiserFrame
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h" // OutputBuffer_PreScene / _PostPhysics / _PrePhysics
#include "GameSource/World/EntityModules/PropEntityModule/SharedIO/BrnPropToTrafficInterface.h" // PropToTrafficInterface (SendTrafficLightRestoreEvents)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint ([DIAG] BRN_PROP_DIAG, wave Q6)
#include <stdlib.h>                                          // getenv    ([DIAG] BRN_PROP_DIAG, wave Q6)

namespace BrnWorld
{
    using BrnPhysics::Props::PropPhysicsDataHeader;
    using BrnPhysics::Props::PropTypeData;
    using BrnPhysics::Props::PropZoneData;
    using BrnPhysics::Props::PropCellData;
    using BrnPhysics::Props::PropCellId;
    using BrnPhysics::Props::PropInstanceData;

    // ========================================================================
    // Host layout tripwires (never called).
    // ------------------------------------------------------------------------
    // Deliberately NOT asserting the console byte offsets in the header banner: pointer
    // widening moves every one of them on x64 (see the banner's CONSOLE-vs-HOST warning).
    // What IS load-bearing is that the per-zone tables have the shapes the bodies index by
    // name, and that PropGraphicsManager's slot stride is the HOST stride (16 on x64), not
    // the console's 8 -- the old header banner claimed 8, which is why this pin exists.
    void PropZoneManager::_AssertLayout()
    {
        static_assert(sizeof(((PropZoneManager*)0)->mauStartIndexOfZone)    == 2 * KU_MAX_ZONES, "mauStartIndexOfZone[500] u16");
        static_assert(sizeof(((PropZoneManager*)0)->mauNumberOfPropsInZone) == 2 * KU_MAX_ZONES, "mauNumberOfPropsInZone[500] u16");
        static_assert(sizeof(((PropZoneManager*)0)->mauStartIndexOfParts)   == 2 * KU_MAX_ZONES, "mauStartIndexOfParts[500] u16");
        static_assert(sizeof(((PropZoneManager*)0)->mauNumberOfPartsInZone) == 2 * KU_MAX_ZONES, "mauNumberOfPartsInZone[500] u16");
        // The pools the X360 addresses as `base + 80*index`: the stride must come from the
        // element type, and it does (both records are pinned at 80 by their own home).
        static_assert(sizeof(PropEntityInstance)     == 80, "prop pool stride 80");
        static_assert(sizeof(PropPartEntityInstance) == 80, "part pool stride 80");
        static_assert(sizeof(PropInstanceData)       == 80, "serialised prop record stride 80");
        // maRotationParams: the X360 strides it by 6 and writes +0/+2/+3/+4. Those four
        // field offsets are pointer-free, so they DO hold on the host.
        static_assert(offsetof(PropEntityRotationParams, miPropIndex) == 0, "rot params +0");
        static_assert(offsetof(PropEntityRotationParams, mnRotSpeed)  == 2, "rot params +2");
        static_assert(offsetof(PropEntityRotationParams, muMinAngle)  == 3, "rot params +3");
        static_assert(offsetof(PropEntityRotationParams, muMaxAngle)  == 4, "rot params +4");
    }

    void PropGraphicsManager::_AssertLayout()
    {
        // HOST-sized, not console-sized: the reference slot is { pointer, u8 } padded to
        // the pointer's alignment, i.e. 2 * sizeof(void*) (16 on x64, 8 on the X360). All
        // indexing goes through maPropGraphicsReferences[type], so this stride is whatever
        // the host compiler picks -- these pins just make that explicit and stop anyone
        // reintroducing the "8-byte slot" console constant.
        static_assert(offsetof(PropGraphicsReference, mpPropGraphics) == 0,               "graphics ref slot +0");
        static_assert(offsetof(PropGraphicsReference, mu8RefCount)    == sizeof(void*),   "refcount right after the pointer");
        static_assert(sizeof(PropGraphicsReference)                   == 2 * sizeof(void*), "host slot stride");
        static_assert(sizeof(((PropGraphicsManager*)0)->maPropGraphicsReferences)
                      == KU_MAX_PROP_TYPES * sizeof(PropGraphicsReference), "500 slots");
    }

    // ---- UpdateInstance displacement / world-floor thresholds ---------------------------
    // ⭐⭐ BOTH VALUES CORRECTED 2026-08-18 (wave Q round 2). Both constants used to carry a
    // FLAG saying the float "is NOT in .ida-exports (rodata at 0x82FAD840 is not dumped)" and
    // a placeholder magnitude. The claim was wrong about RECOVERABILITY, and both placeholders
    // were wrong about the value -- one by 15x, one by 50x. A per-address FUNCTION export
    // cannot show them because neither address is rodata at all: BOTH are .bss (sixteen zero
    // bytes in the image) filled at run time by unnamed dynamic initialisers. Read out of the
    // IDB with headless idat in this pass:
    //
    //   unk_82FAD840 <- initialiser @0x82C4B478:
    //       lis r11, flt_8200D4F8@ha ; lfs f0, flt_8200D4F8@l(r11)
    //       stfs f0,-0x10(r1) ; lvlx v0 ; vspltw v0,v0,0 ; stvx128 v0 -> unk_82FAD840
    //     flt_8200D4F8 == 0xC47A0000 == -1000.0f     (was modelled as -15000.0f)
    //
    //   unk_82FAD4D0 <- initialiser @0x82C4C2B8 (identical splat shape):
    //     flt_820147FC == 0x3F000000 ==     0.5f     (was modelled as 0.01f)
    //
    // Both are 4-lane splats of a single scalar, which is why a scalar models them exactly.
    //
    // unk_82FAD840 (read @0x822F0B90 here, and by PropEntityModule::ProcessPotentialContact-
    // WithPart @0x822EEE48 and ::ChangePropState @0x822EF5E8): the Y-position floor.
    // UpdateInstance computes `lbBelowWorldFloor = (KVF_PROP_FLOOR > pos.y)` to gate pulling a
    // frozen prop out of the scene. NOW HOMED at its DWARF address
    // (SharedClasses/Physics/Props/BrnPropConstants.h:176, `const VecFloat KVF_PROP_FLOOR`) and
    // used from there so the three consumers cannot drift apart again.
    //
    // unk_82FAD4D0 (read @0x822F150C, this function only): the per-axis threshold compared
    // against the ABSOLUTE VALUE OF THE INCOMING LINEAR VELOCITY -- NOT of the transform.
    //
    // ⭐ OPERAND CORRECTED 2026-08-19 (wave Q6 write-back audit). The previous revision of
    // this comment (and of the gate in UpdateInstance) said the compare was against
    // |lTransform| / the transform's first basis row. The asm says otherwise, unambiguously:
    //   0x822F0948  vmr128   v127, v1        ; v127 latches the FIRST VMX ARGUMENT
    //   0x822F1518  vandc128 v0,  v127, v0   ; |v127|   (v0 = the 0x80000000 sign mask)
    //   0x822F1524  vcmpgtfp. v0, v12, v13   ; |v127| > unk_82FAD4D0
    // lTransform arrives as a hidden POINTER in r5 (every one of its rows is reached with
    // `lvx128 v0, r0, r28` / `r28+0x10` / `+0x20` / `+0x30`), so v1 is not the transform: it
    // is the third source parameter, `Vector3 lLinearVelocity`. Its producer confirms it --
    // PropEntityModule::UpdateProps @0x822FB2A0 loads `lvx128 v1, r30, 0x40`, and
    // UpdatePropEvent::mLinearVelocity is at +0x40 (mAngularVelocity, which rides v2, is at
    // +0x50 and is never read by this function).
    // The old spelling was not merely a mislabel: a basis row of an orthonormal transform has
    // max|component| >= 1/sqrt(3) == 0.577 > 0.5, so the gate was TRUE for every prop on its
    // very first update and KU_MOVED_BIT was set unconditionally.
    // The lane bookkeeping around the compare is `vrlimi128 v12, v0, 1, 1`, which overwrites
    // the w lane with a copy of another lane -- i.e. it removes Vector3's undefined 4th
    // component from the test, leaving exactly "any of |x|,|y|,|z| exceeds the threshold".
    // 0.5 m/s, not the 0.01 the pre-round-2 placeholder assumed. No DWARF name is attested
    // for the constant, so it keeps this file's descriptive one.
    static const f32 KF_FIRST_MOVE_THRESHOLD = 0.5f;   // MEASURED: flt_820147FC via 0x82C4C2B8

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

        // X360 `lwz r30,0(r29)` -- the record's FIRST word is its prop type id. Read BY
        // NAME now that BrnPropGraphicsList.h homes the layout (muTypeId @ +0), replacing
        // the previous `*reinterpret_cast<const u32*>(lpPropGraphics)` offset-read.
        const u32 luType = lpPropGraphics->muTypeId;
        CGS_ASSERT(luType < KU_MAX_PROP_TYPES, "luType < BrnPhysics::Props::KU_MAX_PROP_TYPES");

        PropGraphicsReference& lrReference = maPropGraphicsReferences[luType];
        if (lrReference.mpPropGraphics != nullptr)
        {
            ++lrReference.mu8RefCount;
        }
        else
        {
            // The X360 inlines AddPropGraphics here; de-inlined to the named helper.
            AddPropGraphics(lpPropGraphics);
        }
        return true;
    }

    // ========================================================================
    // PropGraphicsManager::AddPropGraphics (DWARF BrnPropZoneManager.h:473)
    // ------------------------------------------------------------------------
    // The install half Register @0x822A9DE8 folds inline: seat the record in its own type
    // slot with a fresh reference count of one (asm `stw r29,0(r11); li r10,1; stb r10,4(r11)`).
    void PropGraphicsManager::AddPropGraphics(const PropGraphics* lpPropGraphics)
    {
        CGS_ASSERT(lpPropGraphics != nullptr, "lpPropGraphics");

        const u32 luType = lpPropGraphics->muTypeId;
        CGS_ASSERT(luType < KU_MAX_PROP_TYPES, "luType < BrnPhysics::Props::KU_MAX_PROP_TYPES");

        PropGraphicsReference& lrReference = maPropGraphicsReferences[luType];
        lrReference.mpPropGraphics = lpPropGraphics;
        lrReference.mu8RefCount    = 1;
    }

    // ========================================================================
    // PropGraphicsManager::GetPropGraphics (DWARF BrnPropZoneManager.h:455)
    // ------------------------------------------------------------------------
    // The registered graphics record for a prop type, or null when nothing has registered
    // that type yet (an unpopulated slot holds a null pointer -- Register/AddPropGraphics
    // are the only writers, and Construct-time zeroing is the initial state).
    //
    // FLAG (shape-only attestation): the X360 emits no out-of-line body -- this accessor is
    // a header inline folded at its call sites -- so the DWARF gives the signature and the
    // member set gives the body. There is no invented policy here: the table maps type id
    // to slot, which is exactly what Register @0x822A9DE8 builds.
    const PropGraphicsManager::PropGraphics* PropGraphicsManager::GetPropGraphics(u32 luType) const
    {
        CGS_ASSERT(luType < KU_MAX_PROP_TYPES, "luType < BrnPhysics::Props::KU_MAX_PROP_TYPES");
        return maPropGraphicsReferences[luType].mpPropGraphics;
    }

    // ========================================================================
    // PropGraphicsManager::GetPropPartGraphics (DWARF BrnPropZoneManager.h:460)
    // ------------------------------------------------------------------------
    // The graphics record for one destructible part of a prop type. PropGraphics::mpParts
    // points at this prop's FIRST PropPartGraphics inside the list's part table (the parts
    // of a prop are stored contiguously, grouped by owning type -- BrnPropGraphicsList.h,
    // and PropGraphicsList::FixUp @0x8267DB38 rebases exactly that per-prop pointer), so
    // part n of the prop is mpParts[n].
    //
    // FLAG (shape-only attestation, as GetPropGraphics above): no out-of-line X360 body.
    const PropGraphicsManager::PropPartGraphics*
    PropGraphicsManager::GetPropPartGraphics(u32 luPropType, u32 luPropPartType) const
    {
        const PropGraphics* lpPropGraphics = GetPropGraphics(luPropType);
        if (lpPropGraphics == nullptr)
        {
            return nullptr;
        }
        return &lpPropGraphics->mpParts[luPropPartType];
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
    // PropZoneManager::IsZoneLoaded @ 0x822A4390
    // ------------------------------------------------------------------------
    // A zone is loaded iff its prop start index is not the unloaded sentinel.
    bool PropZoneManager::IsZoneLoaded(u16 lu16ZoneId) const
    {
        CGS_ASSERT(lu16ZoneId < KU_MAX_ZONES, "luZoneIndex < BrnPhysics::Props::KU_MAX_ZONES");
        return mauStartIndexOfZone[lu16ZoneId] != KU_UNLOADED_ZONE;
    }

    // ========================================================================
    // PropZoneManager::GetNumberOfPropsInZone / GetNumberOfPartsInZone
    // ------------------------------------------------------------------------
    // DWARF BrnPropZoneManager.h:101/:105. Header inlines (the X360 folds the two `lhzx`
    // loads at every call site -- e.g. GetProp @0x822A41A8's own bounds assert loads
    // mauNumberOfPropsInZone the same way), so only the per-zone table read is attested.
    u32 PropZoneManager::GetNumberOfPropsInZone(u16 lu16ZoneId) const
    {
        CGS_ASSERT(lu16ZoneId < KU_MAX_ZONES, "luZoneId < BrnPhysics::Props::KU_MAX_ZONES");
        return mauNumberOfPropsInZone[lu16ZoneId];
    }

    u32 PropZoneManager::GetNumberOfPartsInZone(u16 lu16ZoneId) const
    {
        CGS_ASSERT(lu16ZoneId < KU_MAX_ZONES, "luZoneId < BrnPhysics::Props::KU_MAX_ZONES");
        return mauNumberOfPartsInZone[lu16ZoneId];
    }

    // ========================================================================
    // PropZoneManager::GetProp @ 0x822A41A8
    // ------------------------------------------------------------------------
    // Resolve (zone id, index-within-zone) to its prop-pool slot. Four tripwires, then the
    // slot address -- there is NO null return path in the shipped body.
    PropEntityInstance* PropZoneManager::GetProp(u16 lu16ZoneId, u32 luPropIndex)
    {
        CGS_ASSERT(lu16ZoneId < KU_MAX_ZONES, "luZoneId < 500");

        const u16 lu16StartIndex = mauStartIndexOfZone[lu16ZoneId];
        CGS_ASSERT(lu16StartIndex != KU_UNLOADED_ZONE, "luStartIndex != KU_UNLOADED_ZONE");
        CGS_ASSERT(luPropIndex < mauNumberOfPropsInZone[lu16ZoneId],
                   "luInstanceId < mauNumberOfPropsInZone[luZoneId]");

        const u32 luPoolIndex = lu16StartIndex + luPropIndex;
        CGS_ASSERT(luPoolIndex < KU_MAX_LOADED_PROP_INSTANCES,
                   "luStartIndex + luInstanceId < BrnPhysics::Props::KU_MAX_LOADED_PROP_INSTANCES");

        return &maProps[luPoolIndex];
    }

    // ========================================================================
    // PropZoneManager::GetPart @ 0x822A4298
    // ------------------------------------------------------------------------
    // Resolve (zone id, prop-index-within-zone, part-index-within-prop) to its part-pool
    // slot. The X360 resolves the owning prop first (same start-index math as GetProp),
    // reads its first-part slot (`lhz r11, 0x9C8(r11)` == maProps[i].mu16PartsIndex) and
    // adds the part index -- so the part pool is indexed by the PROP's mu16PartsIndex, not
    // by the zone's part start index.
    PropPartEntityInstance* PropZoneManager::GetPart(u16 lu16ZoneId, u16 lu16PropIndex, u16 lu16PartIndex)
    {
        CGS_ASSERT(lu16ZoneId < KU_MAX_ZONES, "luZoneId < BrnPhysics::Props::KU_MAX_ZONES");

        const u16 lu16StartIndex = mauStartIndexOfZone[lu16ZoneId];
        CGS_ASSERT(lu16StartIndex != KU_UNLOADED_ZONE, "luStartIndex != KU_UNLOADED_ZONE");
        CGS_ASSERT(lu16PropIndex < mauNumberOfPropsInZone[lu16ZoneId],
                   "luPropIndex < mauNumberOfPropsInZone[luZoneId]");

        const PropEntityInstance& lrProp = maProps[lu16StartIndex + lu16PropIndex];
        return &maParts[lrProp.mu16PartsIndex + lu16PartIndex];
    }

    // ========================================================================
    // PropZoneManager::GetZone @ 0x822C5FF0 (24 insns)
    // ------------------------------------------------------------------------
    // The entity-id keyed zone lookup: owner tripwire, then the owning prop slot's zone
    // index. The X360 addresses it as `maProps_base(0x980) + 80*idx + 70`; here that is
    // the named member of the named slot, and the stride is sizeof(PropEntityInstance).
    //
    // Returned SIGNED: every caller sign-extends (`extsh`) and compares against -1, which
    // is how "this entity has no loaded zone" is spelled. (KU_UNLOADED_ZONE is 65535 ==
    // (s16)-1, so the sentinel survives the narrowing intact -- that is the whole trick.)
    s16 PropZoneManager::GetZone(PropEntityID lEntityId) const
    {
        lEntityId.AssertIsProp();
        return static_cast<s16>(maProps[lEntityId.GetEntityIndex()].mu16ZoneIndex);
    }

    // ========================================================================
    // PropZoneManager::GetProp @ 0x822CDA28 (90 insns; unnamed in IDA)
    // ------------------------------------------------------------------------
    // The PropEntityID-keyed overload of GetProp. Unlike the (zone, index-within-zone)
    // pair above, the packed id already carries a GLOBAL prop-pool index, so the body is
    // five tripwires and one array subscript.
    //
    // Identified by its baked assert texts ("!lEntityId.IsPart()" @BrnPropZoneManager.h:534
    // == 0x216, "lEntityId.GetEntityIndex() < ...KU_MAX_LOADED_PROP_INSTANCES" @:537,
    // "GetZone( lEntityId ) != -1" @:540, "IsZoneLoaded( GetZone( lEntityId ) )" @:541).
    // The owner tripwire fires TWICE in the asm (0x822CDA88 and 0x822CDAD8) because the
    // source calls two different id accessors that each assert; reproduced as the two
    // AssertIsProp()-carrying calls that produce it.
    //
    // ⚠️ CONSOLE-CONSTANT TRAP: the asm computes the slot as `idx*5 << 4` (== idx*80) added
    // to `this + 0x980`. Neither number appears here -- 80 is sizeof(PropEntityInstance) on
    // the HOST and 0x980 is where maProps happens to land on the CONSOLE. The subscript
    // below is the faithful form; the pins in _AssertLayout() keep the element stride honest.
    PropEntityInstance* PropZoneManager::GetProp(PropEntityID lEntityId)
    {
        // "!lEntityId.IsPart()" -- a whole-prop id must carry a zero part index
        // (asm 0x822CDA3C: `clrlwi r11, r31, 22` == muValue & KU_PART_INDEX_MASK).
        CGS_ASSERT(lEntityId.GetPartIndex() == 0, "!lEntityId.IsPart()");
        lEntityId.AssertIsProp();

        const u32 luEntityIndex = lEntityId.GetEntityIndex();
        CGS_ASSERT(luEntityIndex < KU_MAX_LOADED_PROP_INSTANCES,
                   "lEntityId.GetEntityIndex() < BrnPhysics::Props::KU_MAX_LOADED_PROP_INSTANCES");

        const s16 li16Zone = GetZone(lEntityId);
        CGS_ASSERT(li16Zone != -1, "GetZone( lEntityId ) != -1");
        CGS_ASSERT(IsZoneLoaded(static_cast<u16>(li16Zone)), "IsZoneLoaded( GetZone( lEntityId ) )");

        return &maProps[luEntityIndex];
    }

    // ========================================================================
    // PropZoneManager::GetPart @ 0x822CDB90 (81 insns; unnamed in IDA)
    // ------------------------------------------------------------------------
    // The PropEntityID-keyed overload of GetPart. The part-pool slot is the OWNING PROP's
    // first-part slot plus the id's 1-based part index, minus one:
    //
    //     asm 0x822CDC74  lhz   r30, 0x9C8(r30)   ; maProps[idx].mu16PartsIndex (0x980+72)
    //     asm 0x822CDC98  clrlwi r10, r30, 16     ; (u16) that
    //     asm 0x822CDC9C  clrlwi r11, r31, 22     ; lEntityId.GetPartIndex()
    //     asm 0x822CDCA0  add    r11, r11, r10
    //     asm 0x822CDCA4  addis  r11, r11, 1      ; + 0x10000  )  the two together are
    //     asm 0x822CDCA8  addi   r11, r11, -1     ; - 1        )  "(x - 1) mod 65536",
    //     asm 0x822CDCAC  clrlwi r11, r11, 16     ;               i.e. u16 wraparound
    //
    // The +0x10000/-1/mask triple is the compiler spelling `(u16)(partsIndex + partIndex - 1)`
    // without letting the intermediate go negative -- so the subtraction is done in u16, and
    // that wraparound is REPRODUCED here rather than "cleaned up" into a signed subtract.
    // Part indices are 1-based (0 means "the whole prop"), which is why the -1 is there at all.
    //
    // ⚠️ Same console-constant trap as GetProp: the console's `+435968` part-pool base and
    // its 80-byte stride are absent by design; this indexes maParts by name.
    PropPartEntityInstance* PropZoneManager::GetPart(PropEntityID lEntityId)
    {
        lEntityId.AssertIsProp();

        const u32 luEntityIndex = lEntityId.GetEntityIndex();
        CGS_ASSERT(luEntityIndex < KU_MAX_LOADED_PROP_INSTANCES,
                   "lEntityId.GetEntityIndex() < BrnPhysics::Props::KU_MAX_LOADED_PROP_INSTANCES");

        CGS_ASSERT(GetZone(lEntityId) != -1, "GetZone( lEntityId ) != -1");

        const u16 lu16FirstPart = maProps[luEntityIndex].mu16PartsIndex;
        const u16 lu16PartSlot  = static_cast<u16>(lu16FirstPart + lEntityId.GetPartIndex() - 1u);

        return &maParts[lu16PartSlot];
    }

    // ========================================================================
    // PropZoneManager::RecordHitProp @ 0x822BCC00
    // ------------------------------------------------------------------------
    // Flag one prop as hit in the persistent progression bit set. The flat bit index is the
    // same 600*zone + index addressing HasPropBeenHit reads back. The X360 body is mostly
    // the "Recording hit prop. Zone index: ... Prop index: ... array index: ..." debug
    // stream plus the three tripwires; the state change is the single SetBit.
    void PropZoneManager::RecordHitProp(s32 liZoneIndex, s32 liPropIndex)
    {
        CGS_ASSERT(static_cast<u32>(liPropIndex) < KU_MAX_PROP_INSTANCES_PER_ZONE,
                   "Zone / Prop index");
        CGS_ASSERT(static_cast<u32>(liZoneIndex) < KU_MAX_ZONES,
                   "liZoneIndex < static_cast<int32_t>(BrnPhysics::Props::KU_MAX_ZONES)");

        const u32 luBitIndex =
            KU_MAX_PROP_INSTANCES_PER_ZONE * static_cast<u32>(liZoneIndex) + static_cast<u32>(liPropIndex);
        CGS_ASSERT(luBitIndex < 300000u, "invalid index");
        maPreviouslyHitProps.SetBit(luBitIndex);
    }

    // ========================================================================
    // PropZoneManager::RemoveAllPropsAndParts @ 0x822DEF50
    // ------------------------------------------------------------------------
    // The streaming teardown (PropEntityModule::UpdateInstanceStreaming @0x82308330): drop
    // every loaded prop and part in one shot, without walking them. The X360 body is a flat
    // run of stores because it INLINES the container clears and PropCellManager::Clear; the
    // DecFIGS hint for this function names them all
    // (BitArray<9>::UnSetAll x2, BitArray<5400>::UnSetAll x2, BitArray<100>::UnSetAll,
    //  PropCellManager::Clear, BitArray<15>::UnSetAll, then GetSceneInputInterface ->
    //  RemoveAllEntities and GetPropInputInterface -> RemoveAllPropsAndParts), so they are
    // restored as the named calls / named-member stores here.
    //
    // ⚠️ PropCellManager::Clear is NOT declared by the cell-manager home yet, so its inlined
    // body is reproduced here as the named-member resets the asm performs (offsets 0x708,
    // 0x76C, 0x778, 0x780, 0x904, 0x908, 0x90C, 0x90E == miNumLoadedCells,
    // miNumActiveCells, mPhysicalProps, mPhysicalParts, miNumPropsInSim, miNumPartsInSim,
    // mu16NumberOfPropVolumesInScene, mu16NumberOfPropEntitiesInScene). Collapse this back
    // into mCellManager.Clear() when that TU lands it.
    void PropZoneManager::RemoveAllPropsAndParts(PropEntityIO::OutputBuffer_PreScene* lpOutput)
    {
        CGS_ASSERT(lpOutput != nullptr, "lpOutput");

        maUsedProps.UnSetAll();
        maUsedParts.UnSetAll();
        maDontRespawnProps.UnSetAll();
        maRespawnDifferentProps.UnSetAll();
        mUsedRotationParams.UnSetAll();

        // --- inlined PropCellManager::Clear ---
        mCellManager.miNumLoadedCells = 0;
        mCellManager.miNumActiveCells = 0;
        mCellManager.mPhysicalProps.UnSetAll();
        mCellManager.mPhysicalParts.UnSetAll();
        mCellManager.miNumPropsInSim = 0;
        mCellManager.miNumPartsInSim = 0;
        mCellManager.mu16NumberOfPropVolumesInScene  = 0;
        mCellManager.mu16NumberOfPropEntitiesInScene = 0;

        // Mark every zone unloaded. The X360 re-stores the (loop-invariant) zero into
        // mu16NumberOfLoadedProps on every iteration; hoisted out here.
        mu16NumberOfLoadedProps = 0;
        for (u32 luZoneId = 0; luZoneId < KU_MAX_ZONES; ++luZoneId)
        {
            mauStartIndexOfZone[luZoneId]    = KU_UNLOADED_ZONE;
            mauNumberOfPropsInZone[luZoneId] = 0;
            mauStartIndexOfParts[luZoneId]   = KU_UNLOADED_ZONE;
            mauNumberOfPartsInZone[luZoneId] = 0;
        }

        // Ask the scene to drop every entity, and the physics side to drop every body.
        InSceneUpdateInterface* lpScene =
            reinterpret_cast<InSceneUpdateInterface*>(lpOutput->GetSceneInputInterface());
        // FLAG RETIRED 2026-08-12 (link-closure pass): the payload byte now has somewhere
        // to go. The X360 stages the queued InEventRemoveAllEntities with `stb 3` at
        // 0x822DF038, and 3 is E_ENTITYTYPE_PROP -- this asks the scene to drop every
        // entity owned by PROPS, not literally every entity in the world. The event's
        // mu8Owner member and RemoveAllEntities' uint8_t parameter are both DWARF-attested
        // (CgsSceneManagerIO_SceneUpdate.h:65 / :452); see that header's banner.
        lpScene->RemoveAllEntities(static_cast<u8>(E_ENTITYTYPE_PROP));

        BrnPhysics::Props::PropInputInterface* lpPropInput =
            reinterpret_cast<BrnPhysics::Props::PropInputInterface*>(lpOutput->GetPropInputInterface());
        lpPropInput->RemoveAllPropsAndParts();
    }

    // ========================================================================
    // PropZoneManager::AllocatePropInstancesBlock @ 0x822DF050
    // ------------------------------------------------------------------------
    // Find the first free prop-pool slot (first clear bit of maUsedProps), mark it used, and
    // return its start index (slot * KU_SIZE_OF_PROP_ZONE_SLOT). The X360 inlines
    // BitArray<9>::GetFirstClearBit (the cntlzd lowest-clear-bit idiom) and SetBit at the call
    // site; restored to the named container methods here.
    s32 PropZoneManager::AllocatePropInstancesBlock(u32 luSizeOfBlock)
    {
        (void)luSizeOfBlock;
        CGS_ASSERT(luSizeOfBlock < KU_MAX_PROP_INSTANCES_PER_ZONE,
                   "luSizeOfBlock < BrnPhysics::Props::KU_MAX_PROP_INSTANCES_PER_ZONE");

        const s32 liSlotIndex = maUsedProps.GetFirstClearBit();
        CGS_ASSERT(liSlotIndex != -1, "liSlotIndex != -1");

        CGS_ASSERT(static_cast<u32>(liSlotIndex) < KU_NUM_ZONE_SLOTS, "invalid index");
        maUsedProps.SetBit(static_cast<u32>(liSlotIndex));

        return static_cast<s32>(KU_SIZE_OF_PROP_ZONE_SLOT) * liSlotIndex;
    }

    // ========================================================================
    // PropZoneManager::AllocatePartInstancesBlock @ 0x822DF1F8
    // ------------------------------------------------------------------------
    // The part-pool twin of the above (maUsedParts, KU_SIZE_OF_PART_ZONE_SLOT slots).
    s32 PropZoneManager::AllocatePartInstancesBlock(u32 luSizeOfBlock)
    {
        (void)luSizeOfBlock;
        CGS_ASSERT(luSizeOfBlock < KU_MAX_PROP_PARTS_PER_ZONE,
                   "luSizeOfBlock < BrnPhysics::Props::KU_MAX_PROP_PARTS_PER_ZONE");

        const s32 liSlotIndex = maUsedParts.GetFirstClearBit();
        CGS_ASSERT(liSlotIndex != -1, "liSlotIndex != -1");

        CGS_ASSERT(static_cast<u32>(liSlotIndex) < KU_NUM_ZONE_SLOTS, "invalid index");
        maUsedParts.SetBit(static_cast<u32>(liSlotIndex));

        return static_cast<s32>(KU_SIZE_OF_PART_ZONE_SLOT) * liSlotIndex;
    }

    // ========================================================================
    // PropZoneManager::HasPropBeenHit @ 0x822BC920
    // ------------------------------------------------------------------------
    // Test whether a given prop (addressed by zone + index-within-zone) has its previously-hit
    // bit set. The flat bit index is KU_MAX_PROP_INSTANCES_PER_ZONE (600) * zoneIndex + propIndex
    // into maPreviouslyHitProps (BitArray<300000>).
    bool PropZoneManager::HasPropBeenHit(u32 luZoneIndex, u32 luPropIndex) const
    {
        CGS_ASSERT(luZoneIndex < KU_MAX_ZONES, "luZoneIndex < BrnPhysics::Props::KU_MAX_ZONES");

        const u32 luBitIndex = KU_MAX_PROP_INSTANCES_PER_ZONE * luZoneIndex + luPropIndex;
        CGS_ASSERT(luBitIndex < 300000u, "invalid index");
        return maPreviouslyHitProps.IsBitSet(luBitIndex);
    }

    // ========================================================================
    // PropZoneManager::GetRespawnType @ 0x822BC4D0
    // ------------------------------------------------------------------------
    // Classify how a prop should respawn from its two per-prop respawn bit sets (indexed by the
    // prop's entity index):
    //   E_DONT_RESPAWN    (1) when the prop is flagged "don't respawn"       (maDontRespawnProps set)
    //   E_RESPAWN_CHANGED (2) when the prop is flagged "respawn a different" (maRespawnDifferentProps set)
    //   E_RESPAWN         (0) otherwise (respawn normally)
    // DWARF (BrnPropZoneManager.h:176) attests the return type as BrnPhysics::Props::eRespawnType.
    // The X360 inlines PropEntityID::GetOwner()/GetEntityIndex() (packed-word bit math) and
    // BitArray::IsBitSet at each call site; restored to the named methods. Each branch also fires
    // a cross-check assert (the OTHER set must be clear).
    BrnPhysics::Props::eRespawnType PropZoneManager::GetRespawnType(PropEntityID lId) const
    {
        CGS_ASSERT(lId.GetOwner() == E_ENTITYTYPE_PROP, "mEntityId.GetOwner() == E_ENTITYTYPE_PROP");
        const u32 luEntityIndex = lId.GetEntityIndex();
        CGS_ASSERT(luEntityIndex < 5400u, "invalid index");

        if (maDontRespawnProps.IsBitSet(luEntityIndex))
        {
            CGS_ASSERT(lId.GetOwner() == E_ENTITYTYPE_PROP, "mEntityId.GetOwner() == E_ENTITYTYPE_PROP");
            CGS_ASSERT(luEntityIndex < 5400u, "invalid index");
            CGS_ASSERT(!maRespawnDifferentProps.IsBitSet(luEntityIndex),
                       "!maRespawnDifferentProps.IsBitSet(lId.GetEntityIndex())");
            return BrnPhysics::Props::E_DONT_RESPAWN;
        }

        CGS_ASSERT(lId.GetOwner() == E_ENTITYTYPE_PROP, "mEntityId.GetOwner() == E_ENTITYTYPE_PROP");
        CGS_ASSERT(luEntityIndex < 5400u, "invalid index");
        if (maRespawnDifferentProps.IsBitSet(luEntityIndex))
        {
            const u32 luRespawnEntityIndex = lId.GetEntityIndex();
            CGS_ASSERT(luRespawnEntityIndex < 5400u, "invalid index");
            CGS_ASSERT(!maDontRespawnProps.IsBitSet(luRespawnEntityIndex),
                       "!maDontRespawnProps.IsBitSet(lId.GetEntityIndex())");
            return BrnPhysics::Props::E_RESPAWN_CHANGED;
        }

        return BrnPhysics::Props::E_RESPAWN;
    }

    // ========================================================================
    // PropZoneManager::GetHitPropsFromZone @ 0x822BCA60
    // ------------------------------------------------------------------------
    // Extract the KU_MAX_PROP_INSTANCES_PER_ZONE(600)-bit run of the previously-hit bit set that
    // belongs to one zone (starting at bit 600*zoneIndex) into a caller-supplied 10-word
    // (10*64 = 640-bit) buffer, LSB-aligned. The X360 inlines BitArray<300000>::GetBitRange -- a
    // cross-field shift-extract with the trailing 24-bit tail masked (600 = 9*64 + 24) -- at the
    // call site; restored to the named container method here (bounds asserts
    // CgsBitArray.h:862/874/914 collapse into it).
    void PropZoneManager::GetHitPropsFromZone(u64* lpaHitProps, u32 luZoneIndex) const
    {
        maPreviouslyHitProps.GetBitRange(KU_MAX_PROP_INSTANCES_PER_ZONE * luZoneIndex,
                                         KU_MAX_PROP_INSTANCES_PER_ZONE, lpaHitProps);
    }

    // ========================================================================
    // PropZoneManager::SendTrafficLightRestoreEvents @ 0x822CDDE0
    // ------------------------------------------------------------------------
    // Drain the per-load list of traffic-light instance ids that need restoring
    // (mauTrafficLightsToRestore, an Array<u32,80> filled by LoadProp) into the pre-physics
    // output buffer's prop->traffic interface -- one RequestTrafficLightRestore per id -- then
    // clear the list. Called once per frame by PropEntityModule::PrePhysicsUpdate.
    //
    // The X360 fetches the interface via OutputBuffer_PrePhysics::GetPropToTrafficInterface()
    // (write-lock accessor @0x822B9A80, member @+11296 -- returns the opaque storage that we
    // reinterpret to the real interface, the same idiom RemovePropFromSim uses for
    // GetPropInputInterface). It then INLINES PropToTrafficInterface::RequestTrafficLightRestore
    // at the loop body (the `luInstanceID != 0` assert is BrnPropToTrafficInterface.h:163, and
    // the AddEvent targets the interface's mTrafficLightRestoreQueue @ +0x8C). We restore that
    // inlined logical call as an explicit RequestTrafficLightRestore(id) -- matching the sibling
    // RequestTrafficLightKnockDown de-inlining precedent. Re-reading GetLength() each iteration
    // reproduces the X360's per-pass count reload + the container's inlined constructed-check.
    void PropZoneManager::SendTrafficLightRestoreEvents(PropEntityIO::OutputBuffer_PrePhysics* lpOutput)
    {
        CGS_ASSERT(lpOutput != nullptr, "lpOutput");

        // Write-lock handle to the prop->traffic hand-off interface (X360 returns this+11296).
        PropEntityIO::PropToTrafficInterface* lpPropToTrafficInterface =
            reinterpret_cast<PropEntityIO::PropToTrafficInterface*>(lpOutput->GetPropToTrafficInterface());
        CGS_ASSERT(lpPropToTrafficInterface != nullptr, "lpPropToTrafficInterface");

        for (u32 luIndex = 0; luIndex < mauTrafficLightsToRestore.GetLength(); ++luIndex)
        {
            const u32 luInstanceID = mauTrafficLightsToRestore.GetItem(luIndex);
            // Inlined in the X360 as: assert luInstanceID != 0, then AddEvent onto the interface's
            // mTrafficLightRestoreQueue (@+0x8C). Restored as the logical call.
            lpPropToTrafficInterface->RequestTrafficLightRestore(luInstanceID);
        }

        // Reset the pending-restore list (X360 stores 0 into the count word @ this+841232).
        mauTrafficLightsToRestore.Clear();
    }

    // ========================================================================
    // PropZoneManager::AddPropPartsToScene @ 0x822F06A0
    // ------------------------------------------------------------------------
    // Thin forwarder to mCellManager: compute the prop's index within its zone (its volume
    // entity index minus the zone's prop start index), assert it is non-negative, and
    // forward along with the zone id.
    void PropZoneManager::AddPropPartsToScene(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                              PropVolumeInstanceID lVolumeInstanceID, InSceneUpdateInterface* lpScene,
                                              BrnReplays::PropEntitySerialiser* lpSerialiser)
    {
        const u16 lu16ZoneId         = lpProp->mu16ZoneIndex;
        const s32 liPropIndexInZone  =
            static_cast<s32>(lVolumeInstanceID.GetEntityIndex()) - mauStartIndexOfZone[lu16ZoneId];
        CGS_ASSERT(liPropIndexInZone >= 0, "liPropIndexInZone >= 0");

        mCellManager.AddPropPartsToScene(lpProp, lpType, lVolumeInstanceID, lpScene,
                                         lpSerialiser, lu16ZoneId, liPropIndexInZone);
    }

    // ========================================================================
    // PropZoneManager::RemovePropFromScene @ 0x822F0740
    // ------------------------------------------------------------------------
    void PropZoneManager::RemovePropFromScene(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                              PropVolumeInstanceID lVolumeInstanceID, InSceneUpdateInterface* lpScene,
                                              BrnReplays::PropEntitySerialiser* lpSerialiser)
    {
        const u16 lu16ZoneId         = lpProp->mu16ZoneIndex;
        const s32 liPropIndexInZone  =
            static_cast<s32>(lVolumeInstanceID.GetEntityIndex()) - mauStartIndexOfZone[lu16ZoneId];
        CGS_ASSERT(liPropIndexInZone >= 0, "liPropIndexInZone >= 0");

        mCellManager.RemovePropFromScene(lpProp, lpType, lVolumeInstanceID, lpScene,
                                         lpSerialiser, lu16ZoneId, liPropIndexInZone);
    }

    // ========================================================================
    // PropZoneManager::RemovePropPartsFromScene @ 0x822F0880
    // ------------------------------------------------------------------------
    void PropZoneManager::RemovePropPartsFromScene(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                                   PropVolumeInstanceID lVolumeInstanceID, InSceneUpdateInterface* lpScene,
                                                   BrnReplays::PropEntitySerialiser* lpSerialiser)
    {
        const u16 lu16ZoneId         = lpProp->mu16ZoneIndex;
        const s32 liPropIndexInZone  =
            static_cast<s32>(lVolumeInstanceID.GetEntityIndex()) - mauStartIndexOfZone[lu16ZoneId];
        CGS_ASSERT(liPropIndexInZone >= 0, "liPropIndexInZone >= 0");

        mCellManager.RemovePropPartsFromScene(lpProp, lpType, lVolumeInstanceID, lpScene,
                                              lpSerialiser, lu16ZoneId, liPropIndexInZone);
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
    // ⭐ WAVE Q KEYSTONE (2026-08-18) -- the BREAKABLE-PROP forwarder block.
    // ------------------------------------------------------------------------
    // Every body below is a DecFIGS-declared PropZoneManager method the X360 compiler FOLDED
    // into its caller (which is why IDA shows those callers calling PropCellManager directly
    // with `r3 = module + 0x280` -- &mZoneManager and &mCellManager are the same address,
    // PropCellManager being embedded at offset 0). De-inlined here so the break pipeline never
    // has to reach through the private mCellManager / mauStartIndexOfZone. The header carries
    // the per-method DWARF line and the X360 fold witness.
    //
    // LAYOUT DISCIPLINE: not one console byte offset appears below. The only index arithmetic
    // is `maParts[ lpProp->mu16PartsIndex ]`, which the compiler scales by
    // sizeof(PropPartEntityInstance) -- NOT by the console's literal 80.
    // ========================================================================

    // DWARF :185 -- X360 0x822CDD48 (unnamed `sub_822CDD48`).
    // `return propIndexInZone < KU_MAX_NON_PERSISTENT_PROPS_PER_ZONE && HasPropBeenHit(zone, propIndexInZone);`
    // The `cmplwi r5, 0x258` gate is UNSIGNED, so a negative (below-base) index also fails it.
    bool PropZoneManager::HasPropBeenHit(PropEntityID lEntityId) const
    {
        const s16 li16ZoneId = GetZone(lEntityId);
        // GetEntityIndex() carries the owner tripwire the asm fires here
        // ("mEntityId.GetOwner() == E_ENTITYTYPE_PROP", BrnPropEntityID.h:278) -- it is the
        // SAME check, inlined; do not add a second AssertIsProp().
        const u32 luPropIndexInZone =
            lEntityId.GetEntityIndex() - static_cast<u32>(mauStartIndexOfZone[static_cast<u16>(li16ZoneId)]);
        if (luPropIndexInZone >= KU_MAX_NON_PERSISTENT_PROPS_PER_ZONE)
        {
            return false;
        }
        return HasPropBeenHit(static_cast<u32>(li16ZoneId), luPropIndexInZone);
    }

    // DWARF :189 -- X360 0x822CDCD0 (unnamed `sub_822CDCD0`). Note the asm loads
    // mauStartIndexOfZone[zone] BEFORE the owner tripwire and applies NO 0x258 gate.
    void PropZoneManager::RecordHitProp(PropEntityID lEntityId)
    {
        const s16 li16ZoneId       = GetZone(lEntityId);
        // The asm loads mauStartIndexOfZone[zone] BEFORE the owner tripwire (0x822CDCF4..
        // 0x822CDD04 precede the `beq` at 0x822CDD08), so the read is hoisted here too. The
        // tripwire itself is GetEntityIndex()'s, inlined -- not a second, separate assert.
        const u16 lu16ZoneStart    = mauStartIndexOfZone[static_cast<u16>(li16ZoneId)];

        RecordHitProp(static_cast<s32>(li16ZoneId),
                      static_cast<s32>(lEntityId.GetEntityIndex()) - static_cast<s32>(lu16ZoneStart));
    }

    // DWARF :208 (park P5) -- folded into PreSceneUpdate @0x8230ADF4.
    void PropZoneManager::UpdateCollisionStreaming(Vector3 lv3Position, const PropPhysicsDataHeader* lpTypes,
                                                   PropCellManager::RecentlyBrokenPropsArray* lpRecentlyBroken,
                                                   PropEntityIO::OutputBuffer_PreScene* lpOutput,
                                                   bool lbInReplay,
                                                   BrnReplays::PropEntitySerialiser* lpSerialiser)
    {
        mCellManager.Update(lv3Position, lpTypes, lpRecentlyBroken, lpOutput,
                            lbInReplay, lpSerialiser, mauStartIndexOfZone);
    }

    // DWARF :346 (park P4) -- folded into PreSceneUpdate @0x8230AD00.
    void PropZoneManager::ClearPropsNearPosition(Vector3 lv3Position, VecFloat lvClearRadius,
                                                 const PropPhysicsDataHeader* lpTypes,
                                                 PropCellManager::PropInputInterface* lpPropInput,
                                                 InSceneUpdateInterface* lpScene,
                                                 BrnReplays::PropEntitySerialiser* lpSerialiser)
    {
        mCellManager.ClearPropsNearPosition(lv3Position, lvClearRadius, lpTypes, lpPropInput,
                                            lpScene, lpSerialiser, mauStartIndexOfZone);
    }

    // DWARF :255 / :269 / :276 -- pure pass-throughs.
    void PropZoneManager::AddPropToContactGeneration(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                                     PropVolumeInstanceID lVolumeInstanceID,
                                                     InSceneUpdateInterface* lpScene)
    {
        mCellManager.AddPropToContactGeneration(lpProp, lpType, lVolumeInstanceID, lpScene);
    }

    void PropZoneManager::RemovePropFromContactGeneration(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                                          PropVolumeInstanceID lVolumeInstanceID,
                                                          InSceneUpdateInterface* lpScene)
    {
        mCellManager.RemovePropFromContactGeneration(lpProp, lpType, lVolumeInstanceID, lpScene);
    }

    void PropZoneManager::RemovePropPartsFromContactGeneration(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                                               PropVolumeInstanceID lVolumeInstanceID,
                                                               InSceneUpdateInterface* lpScene)
    {
        mCellManager.RemovePropPartsFromContactGeneration(lpProp, lpType, lVolumeInstanceID, lpScene);
    }

    // DWARF :262 -- the one forwarder that is NOT a pass-through: it resolves the prop's FIRST
    // part instance. BreakPropIntoParts @0x822FB25C..0x822FB288 is the asm witness (see the
    // header note); the console's literal 80-byte stride is replaced by the array subscript.
    void PropZoneManager::AddPropPartsToContactGeneration(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                                          PropVolumeInstanceID lVolumeInstanceID,
                                                          InSceneUpdateInterface* lpScene)
    {
        mCellManager.AddPropPartsToContactGeneration(lpProp, &maParts[lpProp->mu16PartsIndex],
                                                     lpType, lVolumeInstanceID, lpScene);
    }

    // DWARF :283 (park P3) -- pure pass-through.
    void PropZoneManager::RemovePropPartsFromSimIfPhysical(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                                           PropVolumeInstanceID lVolumeInstanceID,
                                                           PropEntityIO::OutputBuffer_PreScene* lpOutput)
    {
        mCellManager.RemovePropPartsFromSimIfPhysical(lpProp, lpType, lVolumeInstanceID, lpOutput);
    }

    // The replay record-side snapshot. X360 PostPhysicsUpdate @0x823032F8 calls
    // PropCellManager::RecordPropPositions with `r3 = module + 0x280`.
    void PropZoneManager::RecordPropPositions(BrnReplays::PropEntitySerialiser* lpSerialiser,
                                              const PropPhysicsDataHeader* lpTypes)
    {
        mCellManager.RecordPropPositions(lpSerialiser, lpTypes);
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
    //
    // ⚠️ SIGNATURE + INTERFACE CORRECTED 2026-08-12 (prop-spawn wave). Two defects:
    //   1. The declaration modelled only the DWARF's four PS3 parameters, so the replay
    //      flag (r7) and the replay serialiser (r8) the X360 threads down into LoadProp
    //      were both missing -- LoadProp cannot pick between HasPropBeenHit and
    //      PropSerialiserFrame::WasPropPreviouslyHit without them. Both are restored (see
    //      the header for the register evidence), and the return type drops to void
    //      (DWARF :131; the X360 epilogue sets no result register).
    //   2. The scene interface was obtained as
    //         reinterpret_cast<InSceneUpdateInterface*>(lpOutput->GetPropInputInterface())
    //      -- a cast between two UNRELATED interfaces AND the wrong member. The X360 calls
    //      `sub_822B9738(lpOutput)`, whose body is `addi r3, r28, 0x420` == this + 1056 ==
    //      &mSceneInputInterface (the DecFIGS hint for RemoveAllPropsAndParts names that
    //      accessor OutputBuffer_PreScene::GetSceneInputInterface). +819824 -- what
    //      GetPropInputInterface returns -- is a different member entirely. Now fetched
    //      from the correct accessor; the residual cast is only the project's standard
    //      "opaque foreign-type storage -> its real type" idiom that every consumer of
    //      these IO buffers uses, and disappears once BrnPropEntityModuleIO.h names the
    //      member with its real type.
    void PropZoneManager::LoadZone(const PropZoneData* lpZoneData, const PropPhysicsDataHeader* lpTypes,
                                   Vector3 lPlayerPosition, PropEntityIO::OutputBuffer_PreScene* lpOutput,
                                   bool lbReplayActive, BrnReplays::PropEntitySerialiser* lpSerialiser)
    {
        const u16 lu16ZoneId       = lpZoneData->GetZoneId();
        const u32 luNumberOfProps  = lpZoneData->GetNumberOfProps();
        const u32 luNumberOfParts  = lpZoneData->GetNumberOfInstances() - luNumberOfProps;

        // The scene-input interface that LoadProp adds the loaded props' entities to
        // (X360 sub_822B9738 == OutputBuffer_PreScene::GetSceneInputInterface, this+1056).
        InSceneUpdateInterface* lpScene =
            reinterpret_cast<InSceneUpdateInterface*>(lpOutput->GetSceneInputInterface());

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
                     lpZoneData, lpTypes, lPlayerPosition, lpScene,
                     lbReplayActive, lpSerialiser);
        }

        // Post-conditions the X360 asserts: every prop and every part landed in the slot.
        CGS_ASSERT((liPropPoolIndex - liPropStartIndex) == liNumberOfPropsInZone,
                   "Current instance / start index / number of props in zone");
        CGS_ASSERT((liPartPoolIndex - liPartStartIndex) == static_cast<s32>(luNumberOfParts),
                   "(liPartPoolIndex - liPartsStartIndex) == liNumPartsInZone");
    }

    // ========================================================================
    // PropZoneManager::LoadProp @ 0x822F2EF0  (947 instructions -- the core spawner)
    // ------------------------------------------------------------------------
    // Turn ONE serialised PropInstanceData record into a live prop entity, plus its part
    // instances, and hand it to the scene. Called once per prop by LoadZone (4x-unrolled
    // there). Both pool cursors are in/out by pointer: the prop cursor advances by exactly
    // one, the part cursor by the type's part count.
    //
    // The shipped body is large because the compiler inlines: the cell-range validation
    // stream, BitArray<100>::GetFirstClearBit/SetBit, three BitArray<5400> set/clear pairs,
    // PropVolumeInstanceID::SetEntityIndex's packing, the identity-matrix stores for every
    // part, the VMX distance test, and every CgsDev::StrStream assert/log message. All of
    // those are restored to their logical named form here; the debug streams collapse to
    // CGS_ASSERT (the X360 log lines have no run-time effect outside dev builds and are
    // dropped per project convention).
    //
    // ASM-LEVEL DECODES worth recording:
    //   * The prop's serialised record is `lpZoneData->GetInstances()[liZoneDataPropIndex]`
    //     (`lwz r10,8(zone)` + `*80`); its world position is the transform's wAxis
    //     (`addi r24, rec, 0x30` feeding GetCellId's f1/f2 = x and z lanes).
    //   * Rotation-slot gate: `lbz r11,0x4A(rec); clrrwi r11,r11,6` then equality against
    //     0 / 0x40 / 0x80 -- i.e. the two-bit axis selector is X, Y or Z. 0xC0
    //     (knNoAngularRotation) is the ONLY encoding that skips the slot. That is exactly
    //     PropInstanceData::IsAnimated().
    //   * The rotation slot's three payload bytes are written to +2/+4/+3 of the 6-byte
    //     PropEntityRotationParams from record bytes 0x4A/0x4B/0x4C -- note the CROSSOVER:
    //     record mn8MaxAngle (0x4B) lands in muMaxAngle (+4) and mn8MinAngle (0x4C) lands
    //     in muMinAngle (+3), so the two are NOT written in address order. Transcribing the
    //     asm's store order without the names would swap them.
    //   * The entity handle is built as `(index << 10) | 0x03000000` (`slwi r11,r26,10;
    //     oris r6,r11,0x300`) -- owner byte E_ENTITYTYPE_PROP, 14-bit entity index, part 0.
    //   * The respawn switch is on GetRespawnTypeForProp's return: 0 == E_RESPAWN clears
    //     BOTH respawn bits; 1 == E_DONT_RESPAWN clears "respawn different" and SETS "don't
    //     respawn"; 2 == E_RESPAWN_CHANGED does the reverse and swaps in the record's
    //     alternative prop type; >=3 asserts "Shouldn't get here".
    //   * The "already hit?" question is answered from the REPLAY frame
    //     (PropSerialiserFrame::WasPropPreviouslyHit, via GetStaticLayout() + 0x3A20 ==
    //     &mLiveFrame) when the replay stage is active, otherwise from the live
    //     progression bit set (HasPropBeenHit). This is the whole reason LoadZone/LoadProp
    //     needed the two extra parameters.
    //   * Every part is stamped with an IDENTITY transform (the four `lvx128` lanes are
    //     built from flt_82001C98 == 1.0f / flt_82001CC0 == 0.0f, with a ZERO w row) and
    //     with the PROP's type id (`lhz r9,0x44(prop)` -> part +0x40), its index within the
    //     prop, and the zone id.
    //   * The proximity gate is `vmsum3fp128` (squared distance player->prop) compared
    //     against `(KVF_MIN_DIST_FROM_PLAYER + type->GetBoundingRadius())^2` with
    //     `vcmpgtfp.`; the CR6 "all lanes true" bit is what the branch reads. Traffic
    //     lights (type +0x60 == 1) and the three overhead-sign graphics ids are exempt.
    void PropZoneManager::LoadProp(s32* lpiPropPoolIndex, s32* lpiPartPoolIndex, s32 liZoneDataPropIndex,
                                   const PropZoneData* lpZoneData, const PropPhysicsDataHeader* lpTypes,
                                   Vector3 lPlayerPosition, InSceneUpdateInterface* lpScene,
                                   bool lbReplayActive, BrnReplays::PropEntitySerialiser* lpSerialiser)
    {
        const s32 liPropPoolIndex  = *lpiPropPoolIndex;
        const s32 liFirstPartIndex = *lpiPartPoolIndex;
        const u16 lu16ZoneId       = lpZoneData->GetZoneId();

        const PropInstanceData& lrInstanceData = lpZoneData->GetInstances()[liZoneDataPropIndex];
        const PropTypeData* lpTypeData = lpTypes->GetType(lrInstanceData.GetTypeID());

        // The volume-instance handle this prop's entity owns. The X360 seeds the packed
        // 64-bit word with the prop owner byte (`li r11,3; sldi r11,r11,56; std`) -- the
        // compiler's fold of the default-constructed prop handle -- then splices in the
        // entity index.
        PropVolumeInstanceID lVolumeInstanceID;
        lVolumeInstanceID.mVolumeInstanceId.muId =
            static_cast<u64>(E_ENTITYTYPE_PROP)
            << (PropVolumeInstanceID::KU_ENTITY_ID_BASE + PropEntityID::KU_OWNER_BASE);
        lVolumeInstanceID.SetEntityIndex(static_cast<u16>(liPropPoolIndex));

        CGS_ASSERT(liZoneDataPropIndex < static_cast<s32>(KU_MAX_PROP_INSTANCES_PER_ZONE),
                   "liZoneDataIndex < static_cast< int32_t > ( BrnPhysics::Props::KU_MAX_PROP_INSTANCES_PER_ZONE )");

        // ---- debug-only cell validation ------------------------------------------------
        // Map the prop's world XZ to its grid cell and prove the zone's cell table agrees
        // that this instance index belongs to that cell. Pure tripwires: no state changes.
        {
            const Vector3& lPropPosition = lrInstanceData.GetWorldTransform().Pos();
            const PropCellId lCellId = lpZoneData->GetCellId(lPropPosition.x, lPropPosition.z);
            CGS_ASSERT(lCellId.IsValid(), "lCellId.IsValid()");

            bool lbFoundCell = false;
            const s32 liNumCells = lpZoneData->GetNumCells();
            for (s32 liCellIndex = 0; liCellIndex < liNumCells; ++liCellIndex)
            {
                const PropCellData& lrCell = *lpZoneData->GetCellData(liCellIndex);
                if (lrCell.GetId() == lCellId)
                {
                    lbFoundCell = true;
                    const s32 liCellStart = lrCell.GetStartIndex();
                    const s32 liCellEnd   = liCellStart + lrCell.GetCount();
                    CGS_ASSERT(liZoneDataPropIndex >= liCellStart && liZoneDataPropIndex < liCellEnd,
                               "Cellid / Prop pos");
                }
            }
            CGS_ASSERT(lbFoundCell, "lbFoundCell");
        }

        // ---- claim the prop pool slot --------------------------------------------------
        PropEntityInstance* lpProp = &maProps[liPropPoolIndex];
        lpProp->Construct();

        // ---- claim an animated-rotation parameter slot, if this prop rotates -----------
        s32 liRotationParamsIndex = -1;
        if (lrInstanceData.IsAnimated())
        {
            liRotationParamsIndex = mUsedRotationParams.GetFirstClearBit();
            if (liRotationParamsIndex != -1 &&
                static_cast<u32>(liRotationParamsIndex) < KU_MAX_ROTATION_PARAMS)
            {
                CGS_ASSERT(static_cast<u32>(liRotationParamsIndex) < KU_MAX_ROTATION_PARAMS,
                           "Index / Number of bits");
                mUsedRotationParams.SetBit(static_cast<u32>(liRotationParamsIndex));

                PropEntityRotationParams& lrRotationParams = maRotationParams[liRotationParamsIndex];
                lrRotationParams.miPropIndex = static_cast<s16>(liPropPoolIndex);
                lrRotationParams.mnRotSpeed  = lrInstanceData.GetRotVelocity();
                lrRotationParams.muMaxAngle  = lrInstanceData.GetMaxAngle();
                lrRotationParams.muMinAngle  = lrInstanceData.GetMinAngle();
            }
            else
            {
                // The pool is full (GetFirstClearBit == -1, or the bit index ran past the
                // 100-slot capacity): the prop loads without animation.
                liRotationParamsIndex = -1;
            }
        }

        // ---- initialise the instance ---------------------------------------------------
        CGS_ASSERT(static_cast<u32>(liPropPoolIndex) < (1u << PropEntityID::KU_NUM_BITS_FOR_ENTITY_NUM),
                   "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");

        PropEntityID lPropEntityId;
        lPropEntityId.mEntityId.muValue =
            (static_cast<u32>(E_ENTITYTYPE_PROP) << PropEntityID::KU_OWNER_BASE)
            | (static_cast<u32>(liPropPoolIndex) << PropEntityID::KU_ENTITY_INDEX_BASE);

        lpProp->InitialiseFromData(&lrInstanceData, lpTypeData, lPropEntityId,
                                   lu16ZoneId, liRotationParamsIndex);

        // ---- respawn classification ----------------------------------------------------
        bool lbLoadProp = true;
        const BrnPhysics::Props::eRespawnType leRespawnType =
            lpZoneData->GetRespawnTypeForProp(liZoneDataPropIndex);

        if (leRespawnType == BrnPhysics::Props::E_RESPAWN)
        {
            // Ordinary prop: it respawns intact, so neither respawn bit is set.
            CGS_ASSERT(static_cast<u32>(liPropPoolIndex) < KU_MAX_LOADED_PROP_INSTANCES, "luIndex < NUMBITS");
            maDontRespawnProps.UnSetBit(static_cast<u32>(liPropPoolIndex));
            maRespawnDifferentProps.UnSetBit(static_cast<u32>(liPropPoolIndex));
        }
        else if (leRespawnType == BrnPhysics::Props::E_DONT_RESPAWN)
        {
            CGS_ASSERT(liZoneDataPropIndex < static_cast<s32>(KU_MAX_NON_PERSISTENT_PROPS_PER_ZONE)
                       || !HasPropBeenHit(lu16ZoneId, static_cast<u32>(liZoneDataPropIndex)),
                       "liZoneDataIndex < static_cast< int32_t > ( BrnPhysics::Props::"
                       "KU_MAX_NON_PERSISTENT_PROPS_PER_ZONE ) || !HasPropBeenHit( liZoneId, liZoneDataIndex )");

            CGS_ASSERT(static_cast<u32>(liPropPoolIndex) < KU_MAX_LOADED_PROP_INSTANCES, "luIndex < NUMBITS");
            maRespawnDifferentProps.UnSetBit(static_cast<u32>(liPropPoolIndex));
            maDontRespawnProps.SetBit(static_cast<u32>(liPropPoolIndex));

            // A prop that has already been hit and must not respawn is simply not loaded.
            const bool lbPreviouslyHit = lbReplayActive
                ? lpSerialiser->GetStaticLayout()->mLiveFrame.WasPropPreviouslyHit(
                      lu16ZoneId, static_cast<u32>(liZoneDataPropIndex))
                : HasPropBeenHit(lu16ZoneId, static_cast<u32>(liZoneDataPropIndex));
            if (lbPreviouslyHit)
            {
                // X360 log: "Prop: <n> in zone: <z> has been previously hit. Not respawning."
                lbLoadProp = false;
            }
        }
        else if (leRespawnType == BrnPhysics::Props::E_RESPAWN_CHANGED)
        {
            CGS_ASSERT(static_cast<u32>(liPropPoolIndex) < KU_MAX_LOADED_PROP_INSTANCES, "luIndex < NUMBITS");
            maRespawnDifferentProps.SetBit(static_cast<u32>(liPropPoolIndex));
            maDontRespawnProps.UnSetBit(static_cast<u32>(liPropPoolIndex));

            const PropTypeData* lpAlternativeTypeData = lpTypes->GetType(lrInstanceData.GetAlternativeType());
            CGS_ASSERT(lpAlternativeTypeData != nullptr, "lpAlternativeTypeData != NULL");
            CGS_ASSERT(lpTypeData != lpAlternativeTypeData, "lpTypeData != lpAlternativeTypeData");

            const bool lbPreviouslyHit = lbReplayActive
                ? lpSerialiser->GetStaticLayout()->mLiveFrame.WasPropPreviouslyHit(
                      lu16ZoneId, static_cast<u32>(liZoneDataPropIndex))
                : HasPropBeenHit(lu16ZoneId, static_cast<u32>(liZoneDataPropIndex));
            if (lbPreviouslyHit)
            {
                // Respawn as the wreck/alternative variant instead. The X360 stores the
                // record's alternative type as a HALFWORD into the instance's type id
                // (`lhz r11,0x48(rec); sth r11,0x44(prop)`) -- a 32-bit store would clobber
                // mu16ZoneIndex, which InitialiseFromData has already written.
                lpTypeData = lpAlternativeTypeData;
                lpProp->muTypeId = static_cast<u16>(lrInstanceData.GetAlternativeType());
                // X360 log: "... has been previously hit. Respawning changed."
            }
        }
        else
        {
            CGS_ASSERT(false, "Shouldn't get here");
        }

        // ---- build this prop's part instances ------------------------------------------
        const s32 liLastPartIndex = liFirstPartIndex + static_cast<s32>(lpTypeData->GetNumberOfParts());
        lpProp->mu16PartsIndex = static_cast<u16>(liFirstPartIndex);
        CGS_ASSERT(liFirstPartIndex != -1, "liFirstPartIndex != -1");

        s32 liPartPoolIndex = liFirstPartIndex;
        while (liPartPoolIndex < liLastPartIndex)
        {
            PropPartEntityInstance& lrPart = maParts[liPartPoolIndex];

            lrPart.mWorldTransform.SetIdentity();
            lrPart.muPartId        = static_cast<u16>(liPartPoolIndex - liFirstPartIndex);
            lrPart.mu16ZoneIndex   = lu16ZoneId;
            lrPart.muTypeId        = lpProp->muTypeId;
            lrPart.mbPhysical      = false;

            ++liPartPoolIndex;
        }

        // ---- "too close to the player" gate --------------------------------------------
        // Squared distance from the player to the prop, against the squared exclusion
        // radius (KVF_MIN_DIST_FROM_PLAYER grown by the prop type's bounding sphere).
        // Restored from the VMX form: `vsubfp128` then `vmsum3fp128` is the 3-lane dot
        // product of the delta with itself, and `vaddfp`/`vmulfp128` square the broadcast
        // (minDist + radius).
        {
            const Vector3& lPropPosition = lpProp->mWorldTransform.Pos();
            const f32 lfDeltaX = lPlayerPosition.x - lPropPosition.x;
            const f32 lfDeltaY = lPlayerPosition.y - lPropPosition.y;
            const f32 lfDeltaZ = lPlayerPosition.z - lPropPosition.z;
            const f32 lfDistanceSquared =
                lfDeltaX * lfDeltaX + lfDeltaY * lfDeltaY + lfDeltaZ * lfDeltaZ;

            const f32 lfExclusionRadius = KF_MIN_DIST_FROM_PLAYER + lpTypeData->GetBoundingRadius();

            if ((lfExclusionRadius * lfExclusionRadius) > lfDistanceSquared)
            {
                // Traffic lights and the three overhead-sign prop types must always load,
                // however close the player is (the same three graphics ids UpdateInstance
                // special-cases: 500950 / 500930 / 506050). FOLDED 2026-08-19 (wave Q6) onto
                // PropTypeData::IsOverheadSign() -- the shared header homes that predicate and
                // names this site as one of the four hand-spelled copies to retire.
                if (!lpTypeData->IsTrafficLight() && !lpTypeData->IsOverheadSign())
                {
                    // X360 log: "Not loading prop because its too close to player <id>".
                    lbLoadProp = false;
                }
            }
        }

        // ---- publish to the scene ------------------------------------------------------
        if (lbLoadProp)
        {
            const u16 lu16PropZoneId = lpProp->mu16ZoneIndex;
            const s32 liPropIndexInZone =
                static_cast<s32>(lVolumeInstanceID.GetEntityIndex()) - mauStartIndexOfZone[lu16PropZoneId];
            CGS_ASSERT(liPropIndexInZone >= 0, "liPropIndexInZone >= 0");

            mCellManager.AddPropToScene(lpProp, lpTypeData, lVolumeInstanceID, lpScene,
                                        lpSerialiser, lu16PropZoneId, liPropIndexInZone);
        }

        // A traffic light that loads while its knocked-down state is pending needs a
        // restore request; SendTrafficLightRestoreEvents drains this list next frame.
        if (lpTypeData->IsTrafficLight())
        {
            mauTrafficLightsToRestore.Append(lpProp->muInstanceID);
        }

        // ---- advance the caller's pool cursors -----------------------------------------
        *lpiPropPoolIndex = liPropPoolIndex + 1;
        *lpiPartPoolIndex = liPartPoolIndex;
    }

    // ========================================================================
    // PropZoneManager::UnloadZone @ 0x82303790
    // ------------------------------------------------------------------------
    // Remove every prop and part of the zone from scene/sim/contact-generation, free any
    // animated-rotation slots they held, deallocate the zone's prop/part pool slots, mark
    // the zone unloaded, and unlink the zone's traffic-light restore list node.
    void PropZoneManager::UnloadZone(u16 lu16ZoneId, const PropPhysicsDataHeader* lpTypes,
                                     PropCellManager::RecentlyBrokenPropsArray* lpRecentlyBrokenProps,
                                     PropEntityIO::OutputBuffer_PreScene* lpOutput,
                                     BrnReplays::PropEntitySerialiser* lpSerialiser)
    {
        CGS_ASSERT(IsZoneLoaded(lu16ZoneId), "IsZoneLoaded(luZoneId)");

        const s32 liStartIndex   = mauStartIndexOfZone[lu16ZoneId];
        const s32 liNumProps     = mauNumberOfPropsInZone[lu16ZoneId];
        const s32 liEndIndex     = liStartIndex + liNumProps;

        // Tear down the zone's cells first (this also publishes recently-broken props).
        // The X360 resolves the PropPhysicsResourcePtr to its header before the call
        // (`bl sub_822CA148` == ResourcePtr::Get) -- this recon already takes the resolved
        // PropPhysicsDataHeader*, so that hop is not modelled.
        mCellManager.RemoveCells(static_cast<s16>(lu16ZoneId), lpTypes, lpRecentlyBrokenProps,
                                 lpOutput, lpSerialiser, static_cast<u16>(liStartIndex));

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
                    reinterpret_cast<InSceneUpdateInterface*>(lpOutput->GetSceneInputInterface());
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
                    reinterpret_cast<InSceneUpdateInterface*>(lpOutput->GetSceneInputInterface());
                if (lpProp->mu8State >= E_SMASHED)
                {
                    RemovePropPartsFromScene(lpProp, lpType, lVolumeInstanceID, lpScene, lpSerialiser);
                    mCellManager.RemovePropPartsFromSimIfPhysical(lpProp, lpType, lVolumeInstanceID, lpOutput);
                }
                else
                {
                    RemovePropFromScene(lpProp, lpType, lVolumeInstanceID, lpScene, lpSerialiser);
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
    //   * the first-move gate sets KU_MOVED_BIT only when the compare
    //     vcmpgtfp(|lLinearVelocity|, unk_82FAD4D0) SUCCEEDS (>=1 axis exceeds) and the bit
    //     is unset.  (⭐ the OPERAND is the velocity, not the transform -- see the
    //     KF_FIRST_MOVE_THRESHOLD block at the top of this file for the asm that pins it.)
    //   * both per-volume scene-push loops iterate to PropTypeData's volume counts
    //     (whole-prop +0x5E; per-part group +0x2C), NOT a hard-coded 0.
    // The first-move edge publishes THREE events, not two (⭐ Q6 round-1 fix): the first,
    // PropBecamePhysicalEvent (@0x822F1560/0x822F1564), is now BODIED -- its payload type,
    // queue and onward Append to the sound module all exist in the tree. The other two
    // (PropVFXLocatorEvent::AddEventSafe @0x822F15C8, HitOverheadSignEvent::AddEvent
    // @0x822F162C) and the ResourcePtr-list tail splice remain FLAGGED (see inline).
    // The prop-index math (entity index -> prop pool slot) and the part-copy loop are
    // reproduced as the X360 performs them.
    //
    // ⚠️⚠️ RE-AUDITED STORE-FOR-STORE against the 1032-instruction body on 2026-08-19 (wave
    // Q6), because this function had never actually executed: its only producer,
    // PropManager::OutputUpdatedProps @0x82627EC8, was an inert conductor gate until this
    // wave. Three real divergences were found and are fixed here, each marked ⭐ Q6 inline:
    //   1. the first-move gate read the transform instead of lLinearVelocity (0x822F1518);
    //   2. the whole-prop arm was missing the `if (!IsSmashed())` guard the console branches
    //      on at 0x822F1414-0x822F1420 -- without it a whole-prop event that arrives for an
    //      already-smashed prop runs the retirement bookkeeping (miNumPropsInSim--,
    //      FreePhysicalPropSlot on a stale slot) and republishes a stale pose;
    //   3. the Y LOWER bound assert (console line 762, " fell out of the world ") was absent,
    //      so the one tripwire that names a runaway prop never fired.
    void PropZoneManager::UpdateInstance(PropEntityID lEntityId, Matrix44Affine lTransform,
                                         Vector3 lLinearVelocity, Vector3 lAngularVelocity, bool lbFrozen,
                                         const PropPhysicsDataHeader* lpTypeData, f32 lfTimeStep,
                                         PropEntityIO::OutputBuffer_PostPhysics* lpOutput,
                                         BrnReplays::PropEntitySerialiser* lpSerialiser)
    {
        // lAngularVelocity really is dead in this body: it arrives in v2 and the X360 never
        // touches v2 again (the only vector-register argument the body latches is v1, at
        // 0x822F0948). lLinearVelocity IS read -- by the first-move gate below.
        (void)lAngularVelocity;

        CGS_ASSERT(IsValid(lTransform), "RwMath::IsValid( lTransform )");

        // "Below world floor" predicate (X360 v108 @ 0x822F0B80-0x822F0BA4):
        //   vcmpgtfp v0, [unk_82FAD840], splat(lTransform.Pos().y)
        // i.e. BrnPhysics::Props::KVF_PROP_FLOOR > pos.y  -- true when the prop's translation Y has
        // dropped below the rodata floor constant. (The sibling
        // PropEntityModule::ProcessPotentialContactWithPart @ 0x822EEDA8 compares the SAME
        // unk_82FAD840 against a transform's Y lane, confirming it is a world-floor / Y
        // threshold, not a NaN check.) The frozen-prop branch below uses this to decide
        // whether the prop must be pulled out of the scene / contact generation this frame.
        const f32 lfPositionY = lTransform.Pos().y;
        const bool lbBelowWorldFloor = (BrnPhysics::Props::KVF_PROP_FLOOR > lfPositionY);

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

            // Part pool slot = the prop's first-part index + (partId - 1).
            //
            // ⚠️ CORRECTED wave Q6 (was `+ luPartId`, i.e. one slot too high -- the writer
            // and the renderer were on different part slots). The console hides the -1 in
            // the ADDRESS BASE, not in the index, which is what made it easy to misread:
            //   UpdateInstance  @0x822F0920: idx = (partId & 0x3FF) + mu16PartsIndex
            //                                (0x822F1764/0x822F1770), *80 (0x822F1778-8C),
            //                                then base `addis 7 / addi -0x5950`
            //                                (0x822F178C/0x822F1794) == 0x6A6B0 == 435888.
            //   GetPart(PropEntityID) @0x822CDB90 and GetPart(zone,idx) @0x822A4298 both use
            //                                base `addis 7 / addi -0x5900`
            //                                (0x822CDCC0 / 0x822A437C) == 0x6A700 == 435968,
            //                                which is the maParts pool base recorded in
            //                                BrnPropZoneManager.h.
            //   435968 - 435888 == 80 == exactly one sizeof(PropPartEntityInstance) stride,
            //   so UpdateInstance's base is &maParts[-1] and its index is one high: the
            //   addressed element is maParts[firstPart + partId - 1].
            // The PropEntityID overload spells the same -1 out in its index arithmetic
            // instead (0x822CDCA0 add / 0x822CDCA4 addis +0x10000 / 0x822CDCA8 addi -1 /
            // 0x822CDCAC clrlwi 16 -- the +0x10000 only keeps the borrow out of the high
            // half before the u16 truncation), and the tree already spells it that way at
            // BrnPropZoneManager.cpp:487 and BrnPropCellManager.cpp:836.
            // luPartId is >= 1 inside this arm (the `luPartId != 0` gate above), so the
            // subtraction cannot underflow and the console's u16 wrap is unreachable.
            // The matching READER is PropZoneManager::GetPart(PropEntityID) -- keep the two
            // in step if either is ever touched again.
            const u32 luPartPoolIndex = static_cast<u32>(lpProp->mu16PartsIndex) + luPartId - 1u;
            PropPartEntityInstance* lpPart = &maParts[luPartPoolIndex];

            // [DIAG] NOT IN THE X360 BINARY -- captured before the store so the [Q6-move]
            // line below can report the frame's vertical displacement.
            const f32 lfQ6PreviousPartY = lpPart->mWorldTransform.Pos().y;

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

            // ---- [DIAG] NOT IN THE X360 BINARY -- wave Q6 "the money line" ---------------
            // ⛔ DELETE-WHEN smashed parts are confirmed moving on screen. Set BRN_PROP_DIAG.
            // Rate-limited to the first 16 part updates of the run (one line each), because
            // the whole question the wave exists to answer is answered by the first few:
            //   dy != 0 over consecutive lines  -> physics IS reaching the world transform;
            //   volumes == 0 or contactGenBit=0 -> the per-volume scene push below is being
            //                                      skipped, so the part will RENDER moving
            //                                      while its collision volumes stay behind
            //                                      (scout.md §4 item 5: BreakPropIntoParts
            //                                      clears that bit and
            //                                      AddPropPartsToContactGeneration only
            //                                      re-sets it when CanAddPartVolumes passes).
            {
                static const bool sbPropDiag = (getenv("BRN_PROP_DIAG") != 0);
                static s32        siQ6MoveLines = 0;
                if (sbPropDiag && CgsDev::Log::gpDebugPrint != 0 && siQ6MoveLines < 16)
                {
                    ++siQ6MoveLines;
                    *CgsDev::Log::gpDebugPrint
                        << "[Q6-move] part " << lEntityId.GetValue()
                        << " dy=" << (lpPart->mWorldTransform.Pos().y - lfQ6PreviousPartY)
                        << " volumes=" << static_cast<u32>(lVolumeGroup.GetNumberOfVolumes())
                        << " contactGenBit="
                        << (((lpProp->mu8Flags & KU_ADDED_TO_CONTACT_GEN_BIT) != 0) ? 1 : 0)
                        << "\n";
                }
            }

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
            // +/-KF_MAX_VALID_POSITION_ALONG_AXIS. Restored from the per-axis vcmpgtfp
            // asserts as component comparisons against the transform's translation.
            //
            // The two bound constants are MEASURED, not assumed: flt_8201D2BC == 15000.0f
            // and flt_8201D204 == -15000.0f, read back out of the sibling consumer
            // PropCellManager::AddPropToScene @0x822E0128, which folds the identical assert
            // block and whose export renders both literals.
            //
            // The SIX asserts and their console source lines, in emission order:
            //   0x822F0F5C  15000 >  pos.x   :747      0x822F1020  15000 >  pos.y   :748
            //   0x822F10DC  15000 >  pos.z   :749      0x822F119C  pos.x > -15000   :750
            //   0x822F1254  pos.z > -15000   :751      0x822F130C  pos.y > -15000   :762
            // ⭐ Q6 FIX: the sixth (the Y LOWER bound) was MISSING. It is the only one of the
            // six with its own message -- the console streams
            //   "Prop " << entityIndex << " with resource id: " << type->mResourceId
            //           << " instance id: " << prop->muInstanceID << " position: " << pos
            //           << " in zone " << prop->mu16ZoneIndex << " fell out of the world \n "
            // (0x822F1364..0x822F1408) -- and it is the one tripwire that names a runaway
            // prop, i.e. exactly what wave Q6 needs to see if parts free-fall. Its branch
            // polarity is inverted relative to its five siblings because the console swaps the
            // vcmpgtfp operands (`-15000 > pos.y` fires) instead of negating the result; the
            // condition asserted is the same `pos.y > -15000`. CGS_ASSERT takes a plain
            // string, so only the trailing literal survives -- the same collapse every other
            // StrStream assert in this file takes.
            static const f32 KF_MAX_VALID_POSITION_ALONG_AXIS = 15000.0f;
            const Vector3& lPosition = lTransform.Pos();
            CGS_ASSERT(lPosition.x < KF_MAX_VALID_POSITION_ALONG_AXIS, "Exploding prop, resource id");
            CGS_ASSERT(lPosition.y < KF_MAX_VALID_POSITION_ALONG_AXIS, "Exploding prop, resource id");
            CGS_ASSERT(lPosition.z < KF_MAX_VALID_POSITION_ALONG_AXIS, "Exploding prop, resource id");
            CGS_ASSERT(lPosition.x > -KF_MAX_VALID_POSITION_ALONG_AXIS, "Exploding prop, resource id");
            CGS_ASSERT(lPosition.z > -KF_MAX_VALID_POSITION_ALONG_AXIS, "Exploding prop, resource id");
            CGS_ASSERT(lPosition.y > -KF_MAX_VALID_POSITION_ALONG_AXIS, "fell out of the world");

            // ⭐ Q6 FIX -- THE MISSING GUARD (X360 0x822F1410-0x822F1420):
            //     mr r3, r31 ; bl PropEntityInstance::IsSmashed
            //     clrlwi r11, r3, 24 ; cmplwi cr6, r11, 0 ; bne cr6, loc_822F18F0
            // i.e. after the assert block the console RE-TESTS IsSmashed() and branches
            // straight to the epilogue when it is true. The recon had only the assert at the
            // head of this arm, so on a release-shaped run (asserts non-fatal) a whole-prop
            // event arriving for an already-smashed prop would fall through into the
            // retirement bookkeeping below -- decrementing miNumPropsInSim a second time and
            // calling FreePhysicalPropSlot with a slot index the break already recycled.
            // That ordering is reachable: BreakPropIntoParts runs in PreScene while the
            // physics read-back that produced this event ran in the PREVIOUS frame's
            // post-physics, so a frozen whole-prop event can outlive the break by one frame.
            // Spelled as an early return because loc_822F18F0 is the epilogue: everything the
            // console still does there is the ResourcePtr user-list splice this recon
            // deliberately does not model (see the note at the foot of this function).
            if (lpProp->IsSmashed())
            {
                return;
            }

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
                        RemovePropFromScene(lpProp, lpType, lVolumeInstanceID, lpScene, lpSerialiser);
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
                // absolute value of the INCOMING LINEAR VELOCITY (`vandc128 v0, v127, signMask`
                // with v127 == v1 == the third parameter, latched at 0x822F0948), replaces the
                // undefined 4th lane with a copy of another (`vrlimi128 v12, v0, 1, 1`), and
                // compares it against the threshold vector unk_82FAD4D0 with vcmpgtfp. The asm
                // extracts the CR6.2 "all-false / no-lane-greater" bit and `bne` SKIPS the
                // move-set when it is set; so it sets KU_MOVED_BIT (and fires the move events)
                // only when the compare SUCCEEDS -- i.e. AT LEAST ONE axis EXCEEDS the
                // threshold -- AND the prop was not already flagged moved
                // (`if (allFalseBit || (flags & 0x20)) skip; else { ori 0x20; stb }`).
                //
                // ⭐ Q6 FIX: this read `lTransform.Right()` -- the transform's first basis row
                // -- which is a unit vector, so max|component| >= 0.577 always cleared the
                // 0.5 threshold and every prop was flagged "moved" on its first update. See
                // the KF_FIRST_MOVE_THRESHOLD block at the top of this file for the full asm
                // derivation, and BrnPropEvents.h for the +0x40 mLinearVelocity offset the
                // producer loads v1 from.
                //
                // NaN polarity (gotcha 4): vcmpgtfp yields false for an unordered lane, and so
                // does `x > T || x < -T` -- the pair of compares is the NaN-correct spelling of
                // the console's fabs-then-compare, where std::fabs would clear a NaN's sign.
                const Vector3& lVelocity = lLinearVelocity;
                const bool lbVelocityExceedsThreshold =
                       (lVelocity.x < -KF_FIRST_MOVE_THRESHOLD || lVelocity.x > KF_FIRST_MOVE_THRESHOLD)
                    || (lVelocity.y < -KF_FIRST_MOVE_THRESHOLD || lVelocity.y > KF_FIRST_MOVE_THRESHOLD)
                    || (lVelocity.z < -KF_FIRST_MOVE_THRESHOLD || lVelocity.z > KF_FIRST_MOVE_THRESHOLD);

                if (lbVelocityExceedsThreshold && (lpProp->mu8Flags & KU_MOVED_BIT) == 0)
                {
                    lpProp->mu8Flags = static_cast<u8>(lpProp->mu8Flags | KU_MOVED_BIT);

                    // ⭐ Q6 FIX (round 1 verifier, wdiag #2): THE FIRST of the THREE console
                    // publishes at this edge was absent, with no FLAG and no park note -- the
                    // comment below used to say there were only two. It is a dropped side effect,
                    // not an accepted park: nothing about it was blocked.
                    // Measured at 0x822F1554-0x822F1578, immediately after the
                    // `stb` that sets KU_MOVED_BIT and BEFORE the VFX-locator staging:
                    //   0x822F1560  bl 0x822B9F18  -- OutputBuffer_PostPhysics::
                    //                                 GetPropBecamePhysicalEventQueue()
                    //                                 (write-lock assert, then `addi r3,r28,0x10`
                    //                                  == &mPropBecamePhysicalEventQueue, the
                    //                                  console +0x10 member pinned in
                    //                                  BrnPropEntityModuleIO.h:399/:508)
                    //   0x822F1564  bl 0x822C95C8  -- BaseEventQueue<T>::AllocateEventSafe()
                    //                                 (asserts mpEvents != NULL at
                    //                                  CgsBaseEventQueue.h:381, returns
                    //                                  &mpEvents[miLength] at a 16-byte stride and
                    //                                  bumps miLength, or NULL when the 20-entry
                    //                                  queue is full)
                    //   0x822F156C  cmplwi r3,0 / beq -- the console DOES test the slot and, when
                    //                                 the queue is full, branches past the store
                    //                                 without asserting. That null test is part
                    //                                 of the binary and is reproduced here; do
                    //                                 not "improve" it into an assert.
                    //   0x822F1574  lvx128 v0,r31,0x30 / stvx128 v0,r0,r3
                    //                              -- stores the prop's PRE-UPDATE world
                    //                                 translation (mWorldTransform's +0x30 row;
                    //                                 the new lTransform is not installed until
                    //                                 below) into the single Vector3 mPosition of
                    //                                 PropEntityIO::PropBecamePhysicalEvent.
                    // This one is NOT flagged/parked: the payload type, the queue typedef, the
                    // getter body and the World-module Append onward to the sound module
                    // (BrnWorldModuleIO_UpdateOutputBuffer.cpp:553 -> BrnRootSoundModuleIO.cpp:52)
                    // all exist, so this is the sole producer of a fully-wired end-to-end path.
                    {
                        PropEntityIO::OutputBuffer_PostPhysics::PropBecamePhysicalEventQueue*
                            lpBecamePhysicalQueue = lpOutput->GetPropBecamePhysicalEventQueue();
                        PropEntityIO::PropBecamePhysicalEvent* lpEvent =
                            lpBecamePhysicalQueue->AllocateEventSafe();
                        if (lpEvent != 0)
                        {
                            lpEvent->mPosition = lpProp->mWorldTransform.Pos();
                        }
                    }

                    // FLAGGED EVENT PUBLISHES -- the REMAINING TWO of the console's three (the
                    // first, PropBecamePhysicalEvent, is bodied above). The gate above is now
                    // asm-faithful; these two payload types are large unreconstructed IO records
                    // this TU does not own:
                    //   * PropEntityIO::PropVFXLocatorEvent::AddEventSafe @ 0x822F15C8 -- pushes
                    //     a VFX-locator record onto the post-physics output buffer's queue. Its
                    //     payload, read straight off the staging stores at 0x822F1590-0x822F15BC,
                    //     is { Matrix44Affine = the prop's CURRENT mWorldTransform (i.e. the pose
                    //     BEFORE this frame's transform is installed below), u32 = the prop's
                    //     muTypeId (`lhz r10, 0x44(prop)`), u32 = 0 }.
                    //     ⚠️ CORRECTED 2026-08-19: this note used to call that second word the
                    //     "zone id". +0x44 is muTypeId; mu16ZoneIndex is +0x46 and is not read
                    //     here (a wrong comment is a real defect).
                    //   * for the three overhead-sign prop graphics ids (lpType->GetGraphicsId(),
                    //     console +0x58, compared against 0x7A4D6 / 0x7A4C2 / 0x7B8C2 at
                    //     0x822F15D0-0x822F1604), additionally
                    //     BrnGameState::GameStateModuleIO::HitOverheadSignEvent::AddEvent
                    //     @ 0x822F162C, whose payload is the prop's ENTITY INDEX as a single
                    //     byte (`stb r15, var_1C0` -- r15 is the 14-bit index, truncated).
                    // FLAGGED: not reproduced -- both targets fork event-queue / GameState IO
                    // types outside this TU. The id test itself is kept live, now folded onto
                    // PropTypeData::IsOverheadSign() (the shared header homes that predicate and
                    // names this site as one of the copies to retire).
                    const bool lbOverheadSign = lpType->IsOverheadSign();
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
