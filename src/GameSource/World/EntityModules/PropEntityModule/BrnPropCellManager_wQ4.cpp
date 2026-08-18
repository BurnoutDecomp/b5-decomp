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
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropCellManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"          // gpDebugPrint / gxMessageFilterFlags
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"   // PropTypeData / PropPartTypeData accessors
#include "rw/physics/rigidbody.h"                                   // rw::physics::FROZEN_BODY

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
