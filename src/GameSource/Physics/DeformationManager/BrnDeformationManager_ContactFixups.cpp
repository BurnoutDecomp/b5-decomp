// ============================================================================
// GameSource/Physics/DeformationManager/BrnDeformationManager_ContactFixups.cpp
//
// BrnPhysics::Deformation::DeformationManager -- the FOUR contact-spy bridge callees:
//     FixupBodyPartVehicleContact     @ 0x825A0B88
//     FixupWheelVehicleContact        @ 0x825A0D98
//     CreateDetachedWheelContactEvent @ 0x825B95B0
//     CreateDetachedPartContactEvent  @ 0x825DD628
// MOVED VERBATIM 2026-08-06 (bridge de-facade wave) out of the still-unmounted
// BrnDeformationManager_Contacts.cpp so PhysicsModule::ProcessContactSpy / StoreContact can
// link without dragging that TU's ~19-symbol DeformableObject / PenetrationSolver /
// DeformationSensor closure. Same slice-TU pattern as BrnDeformationManager_Construct.cpp.
// Fold back into the home TU when it mounts. Banners and bodies are the home TU's, verbatim
// (the two Fixup id-rewrite FLAGS included).
// ============================================================================

#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                                // CGS_ASSERT
#include "GameSource/World/BrnEntityTypes.h"                                                       // BrnWorld::E_ENTITYTYPE_*

// The header the two Fixup*VehicleContact bodies are inline in on the original (string 0x8208C7E0).
#define BRN_DEFORMATIONMANAGER_H_FILE "..\\..\\..\\GameSource\\Physics/DeformationManager/BrnDeformationManager.h"
// The unity path ARTIST's asserts name (string 0x820923C0).
#define BRN_DEFORMATIONMANAGER_CPP_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Physics/DeformationManager/BrnDeformationManager.cpp"
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"                     // CgsSceneManager::SceneManagerIO::PotentialContact
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"                          // CgsPhysics::PhysicsSimulationIO::OutContactSpy
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"                                   // ContactSpy::PhysicalCarPartContact (+EBodyParts placeholder)
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPart.h"         // PhysicalBodyPart (accessor inlines)
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"          // DeformableObject (GetHandlingBodyVolumeInstanceId -- the B-id rewrite)
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnIKBodyPart.h"               // IKBodyPart (GetPartType inline)
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalWheel.h"            // PhysicalWheel (GetVolumeInstanceId inline)

namespace BrnPhysics
{
namespace Deformation
{
    namespace
    {
        // Owner-byte selectors + pool bounds (duplicated from the home TU's anonymous
        // namespace -- file-scope constants, no ODR surface).
        const u32 KU_OWNER_RACECAR                 = 1;
        const u32 KU_OWNER_RACECAR_DEFORMABLE_PART = 6;
        const u32 KU_OWNER_TRAFFIC_DEFORMABLE_PART = 7;
        const u32 KU_OWNER_DETACHED_RACECAR_WHEEL  = 9;
        const u32 KU_OWNER_DETACHED_TRAFFIC_WHEEL  = 10;
        const u32 KU_MAX_DETACHED_PARTS  = 0x32; // 50
        const u32 KU_MAX_DETACHED_WHEELS = 0x70; // 112
        const s32 KI_MAX_DEFORMATION_MODELS = 28;

        inline u32 GetVolumeInstanceOwner(const CgsSceneManager::VolumeInstanceId& lrId)
        {
            return static_cast<u32>(lrId.muId >> 56) & 0xFFu;
        }
    }

    // ==========================================================================================
    // REFUTED 2026-09-13 (takedown lane S1) -- DO NOT "FIX" EITHER FIXUP TO REMAP THE PART OR
    // WHEEL ID ONTO ITS OWNING VEHICLE.
    // A standing lead read the two bodies below as the place where the console turns a deformable
    // part / detached wheel id into the owning car's scene id, so that a car-vs-car contact would
    // reach the race-car spy queue with a RACECAR owner on its B side. Read off the instructions,
    // each body writes exactly TWO ids and neither of them is that remap:
    //     idA <- the part's own packed handle (part +0x1D0), or the wheel's own packed handle
    //            (wheel +0x70). The owner stays RACECAR_/TRAFFIC_DEFORMABLE_PART or
    //            DETACHED_*_WHEEL, because the routine that packs those handles stamps those
    //            owner tags itself.
    //     idB <- the owning car model's handling-body word (model +0x6710), owner RACECAR.
    // So a race-car contact spy built from a fixed-up contact carries the PART (or the WHEEL) on
    // its B side once the A/B-swapped reciprocal store runs -- never a race car. The producer of
    // a race-car-vs-race-car spy is elsewhere; see the note in
    // BrnVehicleManager_ProcessContactSpies.cpp.
    // ==========================================================================================

    // ==========================================================================================
    // FixupBodyPartVehicleContact @ 0x825A0B88
    //
    // Repair a potential DETACHED-BODY-PART-vs-VEHICLE contact so the detached part collides with
    // the live deformable vehicle: validate the owner tags + the part-slot bound, then (if the part
    // slot and its owning model slot are both live) re-key the contact's A id to the part's volume
    // instance and its B id to the owning car model's handling-body word. Returns true iff re-keyed,
    // false otherwise (slot not live -> contact dropped).
    // ==========================================================================================
    bool DeformationManager::FixupBodyPartVehicleContact(
        CgsSceneManager::SceneManagerIO::PotentialContact* lpPotContact)
    {
        // Inline in BrnDeformationManager.h on the original (DWARF h:301): ARTIST's copy asserts on
        // the header's lines 589/590/592. The owner reads go through the 32-bit entity word
        // (`ld; rldicl 32,32; srwi 24`) as the assert strings spell them.
        CGS_ASSERT_AT(lpPotContact->muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART,
                      "lpPotContact->muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART",
                      BRN_DEFORMATIONMANAGER_H_FILE, 589);
        CGS_ASSERT_AT(lpPotContact->muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR,
                      "lpPotContact->muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR",
                      BRN_DEFORMATIONMANAGER_H_FILE, 590);
        CGS_ASSERT_AT(lpPotContact->muPolyTagA < KU_MAX_DETACHED_PARTS,
                      "lpPotContact->muPolyTagA < KU_MAX_DETACHED_PARTS",
                      BRN_DEFORMATIONMANAGER_H_FILE, 592);

        // Re-key the contact only when the detached-part slot (poly tag A) and its owning model
        // slot (poly tag B) are both live; one shared `return false` (0x825A0D88). The poly tags are
        // re-read at each use (0x825A0D40 / 0x825A0D60). idA <- the part's whole 64-bit id
        // (`ld 0x1D0(part)`), idB <- the model's handling-body word (`ld 0x6710(model)`).
        if (mDetachedPartManager.IsPartIndexUsed(lpPotContact->muPolyTagA) &&
            mModelsAdded.IsBitSet(lpPotContact->muPolyTagB))
        {
            lpPotContact->muVolumeInstanceIdA =
                mDetachedPartManager.GetPartFromIndex(static_cast<u16>(lpPotContact->muPolyTagA))
                    ->GetContactVolumeInstanceId();
            lpPotContact->muVolumeInstanceIdB =
                mpaModels[lpPotContact->muPolyTagB].GetHandlingBodyVolumeInstanceId();
            return true;
        }
        return false;
    }

    // ==========================================================================================
    // FixupWheelVehicleContact @ 0x825A0D98
    //
    // The detached-WHEEL sibling of FixupBodyPartVehicleContact: validate the owner tags + the
    // wheel-slot bound, then (if the wheel slot and its owning model slot are both live) re-key the
    // contact's A id to the detached wheel's volume instance and its B id to the owning car model's
    // handling-body word. Returns true iff re-keyed.
    // ==========================================================================================
    bool DeformationManager::FixupWheelVehicleContact(
        CgsSceneManager::SceneManagerIO::PotentialContact* lpPotContact)
    {
        // Inline in BrnDeformationManager.h on the original (DWARF h:305): asserts on the header's
        // lines 622/623/625.
        CGS_ASSERT_AT(lpPotContact->muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_DETACHED_RACECAR_WHEEL ||
                      lpPotContact->muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_DETACHED_TRAFFIC_WHEEL,
                      "lpPotContact->muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_DETACHED_RACECAR_WHEEL || "
                      "lpPotContact->muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_DETACHED_TRAFFIC_WHEEL",
                      BRN_DEFORMATIONMANAGER_H_FILE, 622);
        CGS_ASSERT_AT(lpPotContact->muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR,
                      "lpPotContact->muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR",
                      BRN_DEFORMATIONMANAGER_H_FILE, 623);
        CGS_ASSERT_AT(lpPotContact->muPolyTagA < KU_MAX_DETACHED_WHEELS,
                      "lpPotContact->muPolyTagA < KU_MAX_DETACHED_WHEELS",
                      BRN_DEFORMATIONMANAGER_H_FILE, 625);

        // As FixupBodyPartVehicleContact, for a detached wheel (idA <- `ld 0x70(wheel)`).
        if (mDetachedWheelManager.IsSlotUsed(static_cast<u16>(lpPotContact->muPolyTagA)) &&
            mModelsAdded.IsBitSet(lpPotContact->muPolyTagB))
        {
            lpPotContact->muVolumeInstanceIdA =
                mDetachedWheelManager.GetWheel(static_cast<u16>(lpPotContact->muPolyTagA))
                    ->GetVolumeInstanceId();
            lpPotContact->muVolumeInstanceIdB =
                mpaModels[lpPotContact->muPolyTagB].GetHandlingBodyVolumeInstanceId();
            return true;
        }
        return false;
    }

    // ==========================================================================================
    // CreateDetachedWheelContactEvent @ 0x825B95B0
    //
    // Build a detached-WHEEL contact event from a spy + potential contact. Asserts every input is
    // non-null and that the spy's / potential contact's A id owner is a detached racecar/traffic
    // wheel, then writes the event tail: mVelocity zeroed (stvx of the vspltisw-0 splat at +96),
    // meType = 91 (stw at +112; the DWARF EBodyParts enumerator `eWHEEL = 91`,
    // SharedClasses/Physics/BrnPhysicsPartTypes.h -- the in-tree ContactSpy::EBodyParts enum is
    // still the one-value placeholder, so the asm literal is carried with its DWARF name here),
    // and the trailing word at +116 zeroed (mbIsHinged = false; the console's stw covers the
    // three tail pad bytes too).
    //
    // ⭐ COMPLETED 2026-08-06 (bridge de-facade wave): the FLAG that stood here ("the ContactSpy
    // event family is not homed in-tree ... re-emit once the records are homed") EXPIRED --
    // PhysicalCarPartContact / OutContactSpy / PotentialContact are homed, the arg types are
    // fully qualified in the header (the local fwd-decl fork is retired), and the owner asserts
    // + tail stores are emitted per the X360 body (export 0x825B95B0, re-read this wave).
    // Caller: PhysicsModule::StoreContact @0x825A5DB0 (detached-wheel arm), AFTER its
    // BaseContact::Construct stamped the event head.
    // ==========================================================================================
    void DeformationManager::CreateDetachedWheelContactEvent(
        BrnPhysics::ContactSpy::PhysicalCarPartContact* lpOutPhysicalCarPartContact,
        const CgsPhysics::PhysicsSimulationIO::OutContactSpy* lpInContact,
        const CgsSceneManager::SceneManagerIO::PotentialContact* lpInPotentialContact)
    {
        CGS_ASSERT_AT(lpOutPhysicalCarPartContact != 0, "lpOutPhysicalCarPartContact != NULL",
                      BRN_DEFORMATIONMANAGER_CPP_FILE, 1960);
        CGS_ASSERT_AT(lpInContact != 0, "lpInContact != NULL", BRN_DEFORMATIONMANAGER_CPP_FILE, 1961);

        // The owner tests read the 32-bit entity word (`ld 0x50; rldicl 32,32; srwi 24`), as the
        // assert strings spell them: mIDA.GetEntityId().GetOwner() (mIDA is a DWARF RigidBodyId)
        // and muVolumeInstanceIdA.GetEntityIDOwner().
        CGS_ASSERT_AT(lpInContact->mIDA.GetEntityId().GetOwner() == BrnWorld::E_ENTITYTYPE_DETACHED_RACECAR_WHEEL ||
                      lpInContact->mIDA.GetEntityId().GetOwner() == BrnWorld::E_ENTITYTYPE_DETACHED_TRAFFIC_WHEEL,
                      "lpInContact->mIDA.GetEntityId().GetOwner() == BrnWorld::E_ENTITYTYPE_DETACHED_RACECAR_WHEEL || "
                      "lpInContact->mIDA.GetEntityId().GetOwner() == BrnWorld::E_ENTITYTYPE_DETACHED_TRAFFIC_WHEEL",
                      BRN_DEFORMATIONMANAGER_CPP_FILE, 1965);

        CGS_ASSERT_AT(lpInPotentialContact->muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_DETACHED_RACECAR_WHEEL ||
                      lpInPotentialContact->muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_DETACHED_TRAFFIC_WHEEL,
                      "lpInPotentialContact->muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_DETACHED_RACECAR_WHEEL || "
                      "lpInPotentialContact->muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_DETACHED_TRAFFIC_WHEEL",
                      BRN_DEFORMATIONMANAGER_CPP_FILE, 1967);

        // The event tail (the BaseContact head was stamped by the caller).
        lpOutPhysicalCarPartContact->mVelocity.SetZero();   // stvx zero splat @+96
        lpOutPhysicalCarPartContact->meType     = static_cast<BrnPhysics::ContactSpy::EBodyParts>(91);   // eWHEEL (DWARF BrnPhysicsPartTypes.h)
        lpOutPhysicalCarPartContact->mbIsHinged = false;   // the console's 4-byte zero @+116
    }

    // ==========================================================================================
    // CreateDetachedPartContactEvent @ 0x825DD628  (DWARF BrnDeformationManager.h:164)
    //
    // ⭐ ADDED 2026-08-06 (bridge de-facade wave). The detached-PART sibling of the wheel body
    // above; caller PhysicsModule::StoreContact @0x825A5DB0 (RACECAR/TRAFFIC_DEFORMABLE_PART
    // arm), AFTER its BaseContact::Construct stamped the event head. Reconstructed
    // branch-for-branch from the X360 asm (export 0x825DD628.json, read this wave):
    //   * null tripwires (:1932/:1933);
    //   * spy owner tripwire: mIDA's owner byte must be RACECAR_ or TRAFFIC_DEFORMABLE_PART
    //     (:1937);
    //   * potential-contact owner tripwire (:1940) -- console streams "Potential contact IDs
    //     don't match. PotCont owner is %d" through gpcMessageBuffer; lowered to CGS_ASSERT
    //     with the static prefix per the standing rule;
    //   * lpPart = mDetachedPartManager.GetPart(low 16 bits of mIDA's LOW dword) (asm
    //     `ld 0x50(spy) ; clrlwi r4,r11,16` -- the part-slot field of the packed handle),
    //     tripwired non-null (:1947);
    //   * event tail: meType = lpPart->GetIKPart()->GetPartType() (the asm's exact
    //     part+476 -> IKPart+8 -> spec+476 pointer chain, all three legs the committed
    //     accessors' own documented seats); mbIsHinged = lpPart->IsJoinedToVehicle() (part
    //     +484 byte); mVelocity = the part body's velocity row (lvx part+0x40 ->
    //     stvx event+96 -- PhysicalBodyPart::GetLinearVelocity, the mRwBody row at +0x40).
    // ==========================================================================================
    void DeformationManager::CreateDetachedPartContactEvent(
        BrnPhysics::ContactSpy::PhysicalCarPartContact* lpOutPhysicalCarPartContact,
        const CgsPhysics::PhysicsSimulationIO::OutContactSpy* lpInContact,
        const CgsSceneManager::SceneManagerIO::PotentialContact* lpInPotentialContact)
    {
        CGS_ASSERT_AT(lpOutPhysicalCarPartContact != 0, "lpOutPhysicalCarPartContact != NULL",
                      BRN_DEFORMATIONMANAGER_CPP_FILE, 1932);
        CGS_ASSERT_AT(lpInContact != 0, "lpInContact != NULL", BRN_DEFORMATIONMANAGER_CPP_FILE, 1933);

        CGS_ASSERT_AT(lpInContact->mIDA.GetEntityId().GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART ||
                      lpInContact->mIDA.GetEntityId().GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_DEFORMABLE_PART,
                      "lpInContact->mIDA.GetEntityId().GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART || "
                      "lpInContact->mIDA.GetEntityId().GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_DEFORMABLE_PART",
                      BRN_DEFORMATIONMANAGER_CPP_FILE, 1937);

        // Streamed (DWARF cpp:1938 lStrStream): the owner byte goes through operator<<(s32).
        CGS_ASSERT_STREAM_AT(lpInPotentialContact->muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART ||
                             lpInPotentialContact->muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_DEFORMABLE_PART,
                             "Potential contact IDs don't match. PotCont owner is "
                                 << lpInPotentialContact->muVolumeInstanceIdA.GetEntityIDOwner(),
                             BRN_DEFORMATIONMANAGER_CPP_FILE, 1940);

        // DWARF locals lBodyId (cpp:1943), liPartPoolIndex (:1945), lpPart (:1946): the
        // detached-part pool slot is the id's UserIDB (`ld 0x50; clrlwi r4, r11, 16`).
        BurnoutBodyPartID lBodyId;
        lBodyId = lpInContact->mIDA;
        u16 liPartPoolIndex = lBodyId.GetPartPoolIndex();
        const PhysicalBodyPart* lpPart = mDetachedPartManager.GetPartFromIndex(liPartPoolIndex);
        CGS_ASSERT_AT(lpPart != 0, "lpPart", BRN_DEFORMATIONMANAGER_CPP_FILE, 1947);

        // The event tail.
        lpOutPhysicalCarPartContact->meType =
            static_cast<BrnPhysics::ContactSpy::EBodyParts>(lpPart->GetIKPart()->GetPartType());
        lpOutPhysicalCarPartContact->mbIsHinged = lpPart->IsJoinedToVehicle();
        lpOutPhysicalCarPartContact->mVelocity  = lpPart->GetLinearVelocity();
    }

}
}
