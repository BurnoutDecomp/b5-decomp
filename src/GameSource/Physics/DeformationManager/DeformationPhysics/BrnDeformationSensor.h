#pragma once

#include <cstddef>                                       // offsetof -- the StoredContact layout pins
#include "types.hpp"
#include "BrnCommonTypes.h"                              // Vector3, Vector3Plus, Matrix44Affine, VecFloat
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"  // CgsSceneManager::VolumeInstanceId (mVolInstId, DWARF :269)
#include "GameSource/Physics/ContactSpies/BrnContactId.h"  // BrnPhysics::ContactId
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnCollidableBody.h"          // CollidableBody (canonical base) + ImpulseParams
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnSharedDeformationEnums.h"  // ENextSensorDirection

// BrnPhysics::Deformation::DeformationSensor, homed at its mirrored DWARF path. The sensor IS a
// CollidableBody (it derives the canonical base, BrnCollidableBody.h) and carries the streamed spec
// pointer, this-frame point-displacement / biggest-impulse vector, the stored-contacts array, the
// car-on-car impulse contact, a contact spy, a scene volume-instance id, the live contact count, the
// local/world collision spheres and the accumulated scratch amount.
//
// DWARF MEMBER ORDER (BrnDeformationSensor.h:262-273): mpSpec, mPointDisplacement_BiggestImpulseThisFrame,
// maStoredContacts[3], mImpulseContact, mContactSpy, mVolInstId, miNumStoredContacts, mpLocalSpaceSphere,
// mpWorldSpaceSphere, mfScratchAmount.
//
// RECONCILIATION -- one Wave-2 consumer still constrains this header:
//   * BrnIKBodyPart.cpp (Wave-2) :: GetMaxSensorImpulse() -- bodied against the real DWARF member
//     mPointDisplacement_BiggestImpulseThisFrame (the +0x10 16-byte vector the detach test splats its
//     w lane from), replacing the previous declared-only stub over the opaque leading run.
//
// StoredContact carries its seven real DWARF members host-natively; there is no opaque POD and no
// `StoredContactView` (an 80-byte view over a 64-byte record ran past the end -- see the layout pin
// in BrnDeformationSensor.cpp). Console +0x34 IS mpOtherVehicle (a 4-byte console pointer), so the
// old mu32NonWorldFlag alias is gone. Proven twice --
//   ClearNonWorldContacts        @0x825C1064 `addi r6,r3,0x54` + `lwz r10,0(r6)`  (sensor+0x54 ==
//                                contact[0]+0x34) then `addi r6,r6,0x40` per contact;
//   AddContactsToPenetrationSolver @0x825E1EAC `addi r30,r29,0x24` (r29 == contact+0x10, so r30 ==
//                                contact+0x34) + `lwz r11,0(r30)` -> non-zero picks the VEHICLE list.
// Same word, same meaning: "non-world" == "has an other vehicle". ClearNonWorldContacts therefore
// now reads `maStoredContacts[i].mpOtherVehicle != nullptr` by name.
//
// X360 pointers are 32-bit; on the 64-bit host every embedded pointer widens, so absolute byte
// offsets do NOT hold on the host and are never hard-coded. Members are pinned BY NAME; sensor-array
// indexing uses sizeof(DeformationSensor) for stride, so the exact host byte size is not
// load-bearing (console 432 == 0x1B0; the host figure is larger and is nobody's constant). GROW the
// reconstructed scratch overlay into the real DWARF members as further sensor TUs land -- do not fork.

namespace CgsSceneManager
{
	// Penetration / contact handling reaches the solver only through DeformationSensor::
	// AddContactsToPenetrationSolver, which takes a PenetrationSolver* -- but a PotentialContact
	// (ValidateAndAddContact's input) is a CgsSceneManager contact record passed by const ref.
	struct PotentialContact;
}

// Collision sphere; its leading 16 bytes are centre.xyz + radius.w. Its canonical home is
// CgsGeometric (GameShared/GameClasses/Geometric/Primitives/CgsSphere.h); referenced only by
// pointer here, so a forward declaration suffices.
namespace CgsGeometric
{
	struct Sphere;
}

namespace BrnPhysics
{
namespace Deformation
{
	// ⚠️ FORK FIXED 2026-08-14 (walls wave): this used to be `struct Sphere;` declared INSIDE
	// namespace Deformation -- which, despite its own "avoid forking the real type" comment,
	// IS the fork: it minted the distinct type BrnPhysics::Deformation::Sphere, so every
	// signature in this header (Prepare's two sphere args, mpLocal/WorldSpaceSphere) mangled
	// against a type no other TU uses. The PS3 mangle for DeformationSensor::Prepare
	// (@0x744...ResetSensors' callee: `...PN12CgsGeometric6SphereES7_...`) says CgsGeometric::
	// Sphere, and the arrays these pointers point into (DeformableObject::maLocal/World-
	// SensorSpheres) are declared CgsGeometric::Sphere. The alias below keeps every use in
	// this header source-identical while restoring the one true type.
	using CgsGeometric::Sphere;

	// The full streamed sensor spec lives in SharedClasses/Physics/Deformation/BrnSensorSpec.h; the
	// sensor holds it only by const pointer (mpSpec), so a forward declaration suffices and avoids an
	// include cycle (the streamed spec embeds sensor specs, not sensors).
	struct SensorSpec;

	// The penetration solver the sensor feeds its contacts into (AddContactsToPenetrationSolver param);
	// referenced only by pointer, so forward-declared (home: BrnPenetrationSolver.h).
	struct PenetrationSolver;

	// The two-vehicle deformation system passes contacts between cars by pointer, so the
	// contact record references the *other* car and its sensor only by pointer -- forward
	// declarations suffice and avoid a header cycle (DeformableObject embeds DeformationSensor
	// by value, and DeformableObject itself is reconstructed in BrnDeformableObject.h which
	// includes THIS header).
	struct DeformableObject;
	struct DeformationSensor;

	// ADDITIVE GROW (car-car-impulse group): one stored CAR-ON-CAR impulse contact record --
	// distinct from StoredContact above (which is the post-physics scratch contact). This is the
	// record DeformableObject::ApplyCarCarImpulse consumes: the two contact points (one on each
	// body), the shared surface normal, the other vehicle + its sensor, the sub-frame impact time
	// and the packed contact id. The member SEQUENCE + names + types are DWARF-authoritative
	// (references/DecFIGS/dwarfdump/.../BrnDeformationSensor.h:57, struct StoredImpulseContact,
	// offsets mPointOnA/mPointOnB/mNormal/mpOtherVehicle/mpOtherSensor/mfImpactTimeInFrame/
	// mContactId). GetInverse swaps the A/B roles (point-on-A<->point-on-B, negated normal) so the
	// SAME impulse can be applied to the other car with the contact reversed.
	struct StoredImpulseContact
	{
		Vector3               mPointOnA;            // contact point on this body
		Vector3               mPointOnB;            // contact point on the other body
		Vector3               mNormal;              // shared surface normal
		DeformableObject*     mpOtherVehicle;       // the other car
		DeformationSensor*    mpOtherSensor;        // the other car's sensor that owns the contact
		f32                   mfImpactTimeInFrame;  // sub-frame time of impact (0..1)
		BrnPhysics::ContactId mContactId;           // packed contact identity

		// DWARF :68. Produce the role-swapped contact (A<->B, normal reversed) used to apply the
		// equal-and-opposite impulse to the other vehicle. Owned by the StoredImpulseContact TU --
		// declared-only (ApplyCarCarImpulse calls it BY NAME; the per-TU gate needs only the decl).
		void GetInverse(StoredImpulseContact& lrInverse) const;
	};

	// One stored contact record -- the post-physics penetration contact ValidateAndAddContact keeps
	// (3 deepest per sensor) and AddContactsToPenetrationSolver drains. The member SEQUENCE + names +
	// types are DWARF-authoritative (references/DecFIGS/dwarfdump/.../BrnDeformationSensor.h:42).
	//
	// HOST-NATIVE. The console packs mfProjectedDist +0x30 / mpOtherVehicle +0x34 / mpOtherSensor +0x38 /
	// mbValid +0x3C into 64 bytes only because its pointers are 4 bytes. On the host the two pointers
	// widen, so the record is 80 bytes -- and that is the CORRECT host layout, exactly as every other
	// reconstructed record in this tree. Nothing indexes it by byte offset; the console offsets below
	// are documentation for reading the asm, never arithmetic.
	//   ASM WITNESS (AddContactsToPenetrationSolver @0x825E1D20, r29 == contact+0x10, stride 0x40):
	//     0x825E1DA4 lvx128 v0,r29,-0x10          -> mLocalPointOnA  (contact +0x00)
	//     0x825E1E28 lvx128 v0,r0,r29             -> mLocalPointOnB  (contact +0x10)
	//     0x825E1EB4 lvx128 v0,r30,-0x14 (r30==r29+0x24) -> mNormal   (contact +0x20)
	//     0x825E1FCC lfs    f0,0x30(r3)           -> mfProjectedDist (contact +0x30)
	//     0x825E2270 lwz    r11,0x34(r31)         -> mpOtherVehicle  (contact +0x34)
	//     0x825E224C lwz    r11,0x38(r31)         -> mpOtherSensor   (contact +0x38)
	//     0x825E2224 lbz    r11,0x3C(r31)         -> mbValid  (a BYTE load: it is a bool, not a u32)
	struct StoredContact
	{
		Vector3            mLocalPointOnA;   // console +0x00 -- sphere-relative point on THIS body
		// ⚠️ THE FIELD HAS TWO SPACES, BY DESIGN; mixing them is the +-1667 m launch. For a VEHICLE
		// contact (mpOtherVehicle != 0) this is
		// sphere-relative in the OTHER CAR's body space, exactly like mLocalPointOnA is in this
		// one's -- ValidateAndAddContact @0x825E1940..0x825E19B8 builds it, and Solve()'s vehicle
		// loop transforms it by body B. For a WORLD contact it is a RAW WORLD point, because the
		// world has no transform and Solve()'s world loop deliberately does not transform it.
		Vector3            mLocalPointOnB;   // console +0x10 -- see above: other-body local, or world
		Vector3            mNormal;          // console +0x20 -- shared surface normal
		f32                mfProjectedDist;  // console +0x30 -- swept depth along the normal (sort key)
		DeformableObject*  mpOtherVehicle;   // console +0x34 -- null => WORLD contact, else vehicle
		DeformationSensor* mpOtherSensor;    // console +0x38 -- the other car's sensor
		bool               mbValid;          // console +0x3C

		// DWARF :53. The world-vs-vehicle discriminator both console consumers spell as the +0x34
		// word being non-zero (see the header banner's two asm witnesses).
		bool IsVehicleContact() const { return mpOtherVehicle != 0; }
	};

	// HOST layout pin. Not the console's 64 -- three 16-byte Vector3 + f32 + 4 pad + two 8-byte
	// pointers + bool, 16-aligned == 80. It exists to make the record's size a CHECKED fact rather
	// than an assumption: the defect this replaced was an 80-byte view laid over a 64-byte record.
	static_assert(sizeof(StoredContact) == 80, "host StoredContact is 80 bytes (two widened pointers)");
	static_assert(alignof(StoredContact) == 16, "StoredContact is 16-aligned (its leading Vector3)");
	static_assert(offsetof(StoredContact, mLocalPointOnB) == 16, "mLocalPointOnB @ +16");
	static_assert(offsetof(StoredContact, mNormal) == 32, "mNormal @ +32");
	static_assert(offsetof(StoredContact, mfProjectedDist) == 48, "mfProjectedDist @ +48");

	// DWARF BrnDeformationSensor.h:97. DeformationSensor IS a CollidableBody (canonical base, vptr at
	// console +0x0). The vptr occupies the leading word; the named DWARF members begin at console +0x4.
	struct DeformationSensor : public CollidableBody
	{
		static const u32 KU_MAX_STORED_CONTACTS = 3;   // DWARF: maStoredContacts[3]

		// ---- DWARF leading members (console +0x04 onward) --------------------------------------
		// DWARF :262. The streamed sensor spec (rest offset, per-direction limits, radius, links). Held
		// by const pointer; console +0x4 (immediately after the vptr).
		const SensorSpec* mpSpec;

		// DWARF :265. This-frame point-displacement / biggest-impulse vector (Vector3Plus: xyz =
		// displacement, w = the biggest impulse magnitude this frame). Console +0x10 -- the 16-byte
		// vector IKBodyPart::CheckSensorForcesForJointDetachment @ 0x825C17F8 loads and broadcasts the
		// w lane of (vspltw v0,v0,3) into its peak-impulse max-fold. GetMaxSensorImpulse() returns it.
		Vector3Plus mPointDisplacement_BiggestImpulseThisFrame;

		// console +0x20 -- stored-contacts array. DWARF :266. Console stride 0x40 (4-byte pointers);
		// HOST stride is sizeof(StoredContact) == 80 and is never spelled as a constant.
		StoredContact maStoredContacts[KU_MAX_STORED_CONTACTS];

		// ---- the DWARF mImpulseContact / mContactSpy / mVolInstId region ------------------------
		// ⭐⭐ OVERLAY PROMOTED 2026-08-14 (walls leg 4). The former named-scratch overlay
		// (mfMaxPointDisplacement @+0x118 / mu32PostPhysicsReset @+0x180 / two reserved spans) is
		// retired onto the REAL DWARF members, byte-witnessed by the PS3 ValidateAndAddContact
		// latch (@0x6CC794..0x6CC804) + GetImpulse (@0x6B4ED4):
		//   * +0xE0..+0x11F is mImpulseContact (StoredImpulseContact, DWARF :267) -- the EARLIEST-
		//     IMPACT record of the frame. Its mfImpactTimeInFrame sits at console +0x118: the old
		//     overlay name "mfMaxPointDisplacement" was THIS field misnamed -- "reset to 100.0"
		//     is the DISARM sentinel (GetImpulse rejects times > 1.0), and the latch's
		//     `*(+0x118) > candidate` compare keeps the EARLIEST impact, not a max displacement.
		//   * +0x120..+0x18F is mContactSpy (OutContactSpy, DWARF :268), promoted FLAT here (the
		//     homed CgsPhysics OutContactSpy layout is not byte-frozen in-tree): the two spy
		//     accumulators (+0x120/+0x130, fed by ApplySensorImpulse), the latched contact normal
		//     (+0x140), point-on-A (+0x150), point-on-B (+0x160), the two volume-instance id words
		//     (+0x170/+0x178, latched as `u32 id << 32`), and the spy contact id (+0x180 -- the old
		//     overlay name "mu32PostPhysicsReset"; zeroing it each post-physics IS the spy reset).
		StoredImpulseContact mImpulseContact;   // console +0xE0 (DWARF :267); time +0x118 disarmed to 100.0
		f32 maPostPhysicsVec0[4];       // console +0x120 (288) -- spy accumulated impulse (zeroed post-physics)
		f32 maPostPhysicsVec1[4];       // console +0x130 (304) -- spy accumulated dir*motion (zeroed post-physics)
		Vector3 mSpyNormal;             // console +0x140 -- latched contact normal
		Vector3 mSpyPointOnA;           // console +0x150 -- latched potential-contact point on A
		Vector3 mSpyPointOnB;           // console +0x160 -- latched potential-contact point on B
		u64 mSpyVolumeInstanceIdA;      // console +0x170 -- latched (u64)muVolumeInstanceIdA.word << 32
		u64 mSpyVolumeInstanceIdB;      // console +0x178 -- latched (u64)muVolumeInstanceIdB.word << 32
		u32 mSpyContactId;              // console +0x180 -- latched contact id (zeroed post-physics)

		// Console-only span between mSpyContactId end (+0x184) and mVolInstId (+0x190): the console's
		// 8-byte alignment pad in front of mVolInstId. KEPT so the flat spy block still reads
		// one-for-one against the asm offsets quoted above; it is inert padding on the host, where
		// the compiler would insert its own. No host arithmetic depends on it.
		u8 maReserved3[0x190 - 0x184];

		// ⭐ PROMOTED out of the opaque span 2026-08-14 (deformation-mount wave): the DWARF
		// mVolInstId (:269), console +0x190. DeformableObject::ResetSensors @0x82623D60 writes it
		// per sensor: `ld r10,0x6710(this)` (the object's 8-byte mHandlingBodyID) -> keep the HIGH
		// dword (`clrrdi r10,r10,32` == the entity word) -> `or` in the sensor's collidable-body
		// index -> `std 0x1AE0(r7)` == sensor+0x190. The PS3 twin (@0x7446FC) names the member
		// verbatim: v34->mVolInstId.muId = (entityWord) | sceneIndex.
		CgsSceneManager::VolumeInstanceId mVolInstId;   // console +0x190 (DWARF :269)

		// ---- DWARF trailing members (console +0x198 onward) ------------------------------------
		// DWARF :270 miNumStoredContacts. KEPT under the committed name mi32NumStoredContacts (the
		// committed ClearNonWorldContacts spells it that way; the DWARF name miNumStoredContacts differs
		// only in the i32 width tag).
		s32 mi32NumStoredContacts;      // console +0x198 (408) -- live contact count  (DWARF :270 miNumStoredContacts)

		Sphere* mpLocalSpaceSphere;   // console +0x19C (412)  (DWARF :271)
		Sphere* mpWorldSpaceSphere;   // console +0x1A0 (416)  (DWARF :272)
		f32     mfScratchAmount;      // console +0x1A4 (420)  (DWARF :273) — accumulated scratch / damage

		// ---- trivial accessors (bodied) --------------------------------------------------------
		const Sphere* GetLocalSpaceSphere() const { return mpLocalSpaceSphere; }
		const Sphere* GetWorldSpaceSphere() const { return mpWorldSpaceSphere; }
		f32           GetScratchAmount()   const { return mfScratchAmount; }

		// The local-space sensor sphere's centre (xyz of mpLocalSpaceSphere->mPositionRadius). The
		// deformation debug component reads this into its sensor-position sliders (asm OnSelectedSensorChange
		// loads the leading Vector4 of *(sensor+412)). Declared-only here because Sphere is only
		// forward-declared in this header; bodied where the full Sphere layout is in scope.
		const Vector4& GetLocalSphereCentre() const;

		// The per-sensor biggest-impulse vector the joint-detach test reads. Now bodied against the real
		// DWARF member (its w lane carries the magnitude the detach band compares). Called by
		// BrnIKBodyPart.cpp (Wave-2) as GetDeformationSensorA()->GetMaxSensorImpulse().w.
		const VecFloat& GetMaxSensorImpulse() const
		{
			return reinterpret_cast<const VecFloat&>(mPointDisplacement_BiggestImpulseThisFrame);
		}

		// ---- Wave-3 sensor methods (DECLARED-ONLY; bodies in the sensor TUs) --------------------
		// DWARF :97. Default constructor (zero-init via ClearVariables).
		DeformationSensor();

		// DWARF BrnDeformationSensor.cpp:90. Bind the sensor to its spec + local/world spheres and place
		// it in the body frame. (Matrix44Affine + the four trailing Vector3Plus/Vector3 args are the
		// world transform + the seeded displacement/offset vectors.)
		bool Prepare(const SensorSpec* lpSpec, Sphere* lpLocalSphere, Sphere* lpWorldSphere,
		             Matrix44Affine lWorldTransform, Vector3Plus lDisplacement,
		             Vector3 lOffsetA, Vector3 lOffsetB, Vector3 lOffsetC);

		// DWARF BrnDeformationSensor.cpp:268 -- CollidableBody override. Apply one impulse to this sensor.
		virtual void ApplyLocalImpulse(ImpulseParams* lpImpulseParams);

		// DWARF BrnDeformationSensor.cpp:202 -- CollidableBody override. Receive a passed-on impulse from
		// a neighbouring body (the trailing VecFloat is the chain's remaining magnitude).
		virtual void RecievePassedOnImpulse(const ImpulseParams* lpImpulseParams, VecFloat lvfPassedMagnitude);

		// DWARF BrnDeformationSensor.cpp:465. Validate a candidate contact (cull / clamp) and, if kept,
		// store it in maStoredContacts. Returns true if a contact was added.
		// ⭐ 2026-08-23 (traffic wave 4, SOLVER wave): parameter names taken from the DWARF
		// (BrnPhysicsUnity2.cpp:10334). The transform is the INVERSE (world -> this car), which the
		// old name `lWorldTransform` said the opposite of; `lpOtherCar` is the DWARF's spelling.
		bool ValidateAndAddContact(Matrix44Affine lInverseVehicleTransform,
		                           const CgsSceneManager::PotentialContact& lrPotential,
		                           ContactId lContactId,
		                           DeformableObject* lpOtherCar,
		                           DeformationSensor* lpOtherSensor);

		// ⭐ 2026-08-14 (walls leg 4). Read this frame's earliest-impact record: false when disarmed
		// (mImpulseContact.mfImpactTimeInFrame > 1.0, the 100.0 sentinel), else copy the record out
		// and return true. PS3 @0x6B4ED4 (25 insns) is the whole body; X360 inlines it into
		// UpdateContacts. Bodied in BrnDeformationSensor.cpp.
		bool GetImpulse(StoredImpulseContact& lrOutContact);

		// DWARF BrnDeformationSensor.cpp:718. Push this sensor's stored contacts into the shared
		// penetration solver (as vehicle or world contacts per the body indices). const.
		//
		// ⭐ 2026-08-23 (traffic wave 4, SOLVER wave) -- PARAMETER NAMES WERE SWAPPED/WRONG, now the
		// DWARF's (BrnPhysicsUnity2.cpp:6563: lpSolver, lpDefObjBase, liWorldObjectIndex,
		// liParentObjectIndex, lbVehicleWheelsAllHaveTraction). a4 carries the WORLD pseudo-body
		// index (the caller passes KI_MAX_DEFORMATION_MODELS == 28), a5 carries THIS car's model
		// index -- the old `liBodyIndex`/`liWorldIndex` said the reverse. And a6 is not a "world"
		// flag at all: it is the CALLING car's all-wheels-have-traction bit, one half of the
		// normal-flattening gate in the vehicle arm (asm 0x825E21A4 lbz arg_3F + 0x825E22E4 lbz
		// +0x135B). Values always reached the solver in the right slots; only the names lied.
		void AddContactsToPenetrationSolver(PenetrationSolver* lpSolver, DeformableObject* lpDefObjBase,
		                                    s32 liWorldObjectIndex, s32 liParentObjectIndex,
		                                    bool lbVehicleWheelsAllHaveTraction) const;

		// DeformationSensor::ClearNonWorldContacts @ 0x825C1050 (COMMITTED body in BrnDeformationSensor.cpp).
		// Compact the stored-contacts array (remove contacts whose mpOtherVehicle is set) and reset the
		// post-physics scratch state. Caller (X360 xref): DeformableObject::UpdatePostPhysics.
		void ClearNonWorldContacts();
	};
}
}
