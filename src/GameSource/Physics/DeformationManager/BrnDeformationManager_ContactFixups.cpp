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
    // FixupBodyPartVehicleContact @ 0x825A0B88
    //
    // Repair a potential DETACHED-BODY-PART-vs-VEHICLE contact so the detached part collides with
    // the live deformable vehicle: validate the owner tags + the part-slot bound, then (if the part
    // slot and its owning model slot are both live) re-key the contact's A id to the part's volume
    // instance and its B id to the owning car model's handling-body word. Returns true iff re-keyed,
    // false otherwise (slot not live -> contact dropped).
    // ==========================================================================================
    bool DeformationManager::FixupBodyPartVehicleContact(
        CgsSceneManager::SceneManagerIO::PotentialContact* lpContact)
    {
        CGS_ASSERT(GetVolumeInstanceOwner(lpContact->muVolumeInstanceIdA) == KU_OWNER_RACECAR_DEFORMABLE_PART,
                   "lpPotContact->muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART");
        CGS_ASSERT(GetVolumeInstanceOwner(lpContact->muVolumeInstanceIdB) == KU_OWNER_RACECAR,
                   "lpPotContact->muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR");
        CGS_ASSERT(lpContact->muPolyTagA < KU_MAX_DETACHED_PARTS,
                   "lpPotContact->muPolyTagA < KU_MAX_DETACHED_PARTS");

        // The destination detached-part pool slot (poly tag A). If the slot is not live -> drop.
        const s32 liPartIndex = static_cast<s32>(lpContact->muPolyTagA);
        if (!mDetachedPartManager.IsPartIndexUsed(liPartIndex))
        {
            return false;
        }

        // The owning car model slot (poly tag B). Tripwire-bound, then tested live.
        const u32 luModelIndex = lpContact->muPolyTagB;
        CGS_ASSERT(luModelIndex < static_cast<u32>(KI_MAX_DEFORMATION_MODELS), "invalid index : ");
        if (!mModelsAdded.IsBitSet(luModelIndex))
        {
            return false;
        }

        // ⭐ FLAG CLOSED 2026-08-06 (FixUpVehicleContacts wave). The two 8-byte id writes are now
        // byte-decoded from the X360 asm (@0x825A0D40..0x825A0D7C):
        //   * idA: the console fetches the part (`GetPartFromIndex(low 16 bits of muPolyTagA)`,
        //     bl @0x825A0D4C) and stores its 64-bit BurnoutBodyPartID whole
        //     (`ld 0x1D0(part) ; std 0x30(contact)`) -- the packed part handle IS the part's
        //     volume-instance id. Exposed as PhysicalBodyPart::GetContactVolumeInstanceId().
        //   * idB: `ld 0x6710(mpaModels[muPolyTagB]) ; std 0x38(contact)` -- the model's
        //     {mHandlingBodyID, mGlobalEntityId} pair read as one 8-byte volume-instance word
        //     (the banked "+26384 is an 8-byte word" lead, confirmed). Exposed as
        //     DeformableObject::GetHandlingBodyVolumeInstanceId().
        const PhysicalBodyPart* lpPart =
            mDetachedPartManager.GetPartFromIndex(static_cast<u16>(lpContact->muPolyTagA));
        lpContact->muVolumeInstanceIdA = lpPart->GetContactVolumeInstanceId();
        lpContact->muVolumeInstanceIdB = mpaModels[luModelIndex].GetHandlingBodyVolumeInstanceId();
        return true;
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
        CgsSceneManager::SceneManagerIO::PotentialContact* lpContact)
    {
        const u32 luOwnerA = GetVolumeInstanceOwner(lpContact->muVolumeInstanceIdA);
        CGS_ASSERT(luOwnerA == KU_OWNER_DETACHED_RACECAR_WHEEL || luOwnerA == KU_OWNER_DETACHED_TRAFFIC_WHEEL,
                   "lpPotContact->muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_DETACHED_RACECAR_WHEEL || "
                   "lpPotContact->muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_DETACHED_TRAFFIC_WHEEL");
        CGS_ASSERT(GetVolumeInstanceOwner(lpContact->muVolumeInstanceIdB) == KU_OWNER_RACECAR,
                   "lpPotContact->muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR");
        CGS_ASSERT(lpContact->muPolyTagA < KU_MAX_DETACHED_WHEELS,
                   "lpPotContact->muPolyTagA < KU_MAX_DETACHED_WHEELS");

        const u16 lu16Slot = static_cast<u16>(lpContact->muPolyTagA);
        if (!mDetachedWheelManager.IsSlotUsed(lu16Slot))
        {
            return false;
        }

        const u32 luModelIndex = lpContact->muPolyTagB;
        CGS_ASSERT(luModelIndex < static_cast<u32>(KI_MAX_DEFORMATION_MODELS), "invalid index : ");
        if (!mModelsAdded.IsBitSet(luModelIndex))
        {
            return false;
        }

        // Re-key the contact off the live detached wheel + its owning car model. The wheel's
        // volume-instance id IS publicly reachable (PhysicalWheel::GetVolumeInstanceId()), so A is
        // re-keyed faithfully; B is the owning model's handling-body word.
        const PhysicalWheel* lpWheel = mDetachedWheelManager.GetWheel(lu16Slot);
        lpContact->muVolumeInstanceIdA = lpWheel->GetVolumeInstanceId();

        // ⭐ FLAG CLOSED 2026-08-06 (FixUpVehicleContacts wave). The B-id write is byte-decoded
        // from the X360 asm (@0x825A0F7C..0x825A0F98): `ld 0x6710(mpaModels[muPolyTagB]) ;
        // std 0x38(contact)` -- the owning model's {mHandlingBodyID, mGlobalEntityId} pair read
        // as ONE 8-byte volume-instance word (the banked "+26384 is an 8-byte word" lead,
        // confirmed), not the 32-bit GetHandlingBodyID() the old flag assumed. Exposed as
        // DeformableObject::GetHandlingBodyVolumeInstanceId().
        lpContact->muVolumeInstanceIdB = mpaModels[luModelIndex].GetHandlingBodyVolumeInstanceId();
        return true;
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
        CGS_ASSERT(lpOutPhysicalCarPartContact != nullptr, "lpOutPhysicalCarPartContact != NULL");   // :1960
        CGS_ASSERT(lpInContact != nullptr, "lpInContact != NULL");                                   // :1961

        const u32 luSpyOwnerA = static_cast<u32>(lpInContact->mIDA >> 56);
        CGS_ASSERT(luSpyOwnerA == KU_OWNER_DETACHED_RACECAR_WHEEL || luSpyOwnerA == KU_OWNER_DETACHED_TRAFFIC_WHEEL,
                   "lpInContact->mIDA.GetEntityId().GetOwner() == BrnWorld::E_ENTITYTYPE_DETACHED_RACECAR_WHEEL || "
                   "lpInContact->mIDA.GetEntityId().GetOwner() == BrnWorld::E_ENTITYTYPE_DETACHED_TRAFFIC_WHEEL");   // :1965

        const u32 luPotOwnerA = GetVolumeInstanceOwner(lpInPotentialContact->muVolumeInstanceIdA);
        CGS_ASSERT(luPotOwnerA == KU_OWNER_DETACHED_RACECAR_WHEEL || luPotOwnerA == KU_OWNER_DETACHED_TRAFFIC_WHEEL,
                   "lpInPotentialContact->muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_DETACHED_RACECAR_WHEEL || "
                   "lpInPotentialContact->muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_DETACHED_TRAFFIC_WHEEL");   // :1967

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
        CGS_ASSERT(lpOutPhysicalCarPartContact != nullptr, "lpOutPhysicalCarPartContact != NULL");   // :1932
        CGS_ASSERT(lpInContact != nullptr, "lpInContact != NULL");                                   // :1933

        const u32 luSpyOwnerA = static_cast<u32>(lpInContact->mIDA >> 56);
        CGS_ASSERT(luSpyOwnerA == KU_OWNER_RACECAR_DEFORMABLE_PART || luSpyOwnerA == KU_OWNER_TRAFFIC_DEFORMABLE_PART,
                   "lpInContact->mIDA.GetEntityId().GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART || "
                   "lpInContact->mIDA.GetEntityId().GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_DEFORMABLE_PART");   // :1937

        const u32 luPotOwnerA = GetVolumeInstanceOwner(lpInPotentialContact->muVolumeInstanceIdA);
        CGS_ASSERT(luPotOwnerA == KU_OWNER_RACECAR_DEFORMABLE_PART || luPotOwnerA == KU_OWNER_TRAFFIC_DEFORMABLE_PART,
                   "Potential contact IDs don't match. PotCont owner is ");   // :1940 (streamed on console)

        // The detached-part pool slot rides the packed handle's low 16 bits.
        const u16 lu16PartSlot = static_cast<u16>(lpInContact->mIDA & 0xFFFFu);
        const PhysicalBodyPart* lpPart = mDetachedPartManager.GetPartFromIndex(lu16PartSlot);
        CGS_ASSERT(lpPart != nullptr, "lpPart");   // :1947

        // The event tail.
        lpOutPhysicalCarPartContact->meType =
            static_cast<BrnPhysics::ContactSpy::EBodyParts>(lpPart->GetIKPart()->GetPartType());
        lpOutPhysicalCarPartContact->mbIsHinged = lpPart->IsJoinedToVehicle();
        lpOutPhysicalCarPartContact->mVelocity  = lpPart->GetLinearVelocity();
    }

}
}
