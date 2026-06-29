#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                              // Vector3, Vector3Plus, Matrix44Affine, VecFloat
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
// RECONCILIATION (committed-TU safety) -- IMPORTANT, two committed/Wave-2 consumers constrain this header:
//   (1) BrnDeformationSensor.cpp (committed) :: ClearNonWorldContacts reads, BY NAME:
//         maStoredContacts[i].mu32NonWorldFlag, mi32NumStoredContacts, mfMaxPointDisplacement,
//         maPostPhysicsVec0[4], maPostPhysicsVec1[4], mu32PostPhysicsReset.
//       Those names are KEPT verbatim (renaming them would break the committed .cpp). The committed
//       StoredContact opaque-64-byte model is likewise KEPT (ClearNonWorldContacts copies it as a
//       64-byte blob and only tests the +0x34 flag).
//       >>> FLAG: the DWARF gives the REAL StoredContact members (mLocalPointOnA/mLocalPointOnB/mNormal/
//       mfProjectedDist/mpOtherVehicle/mpOtherSensor/mbValid) and does NOT separately name
//       mfMaxPointDisplacement / maPostPhysicsVec0 / maPostPhysicsVec1 / mu32PostPhysicsReset -- those
//       were inferred from ClearNonWorldContacts' offset writes and physically OVERLAY the DWARF's
//       mImpulseContact / mContactSpy / mVolInstId region (+0x118..+0x180). They are retained as a
//       reconstructed scratch overlay (NOT promoted to the DWARF members) PRECISELY so the committed
//       ClearNonWorldContacts keeps compiling unchanged. Promote them when ClearNonWorldContacts is
//       re-derived against the real members.
//   (2) BrnIKBodyPart.cpp (Wave-2) :: GetMaxSensorImpulse() -- now bodied against the real DWARF member
//       mPointDisplacement_BiggestImpulseThisFrame (the +0x10 16-byte vector the detach test splats its
//       w lane from), replacing the previous declared-only stub over the opaque leading run.
//
// X360 pointers are 32-bit; on the 64-bit host the trailing Sphere* widen, so absolute byte offsets do
// not all hold on the host. Members are pinned BY NAME, never raw offset; sensor-array indexing uses
// sizeof(DeformationSensor) for stride, so the exact host byte size is not load-bearing. GROW the
// reconstructed scratch overlay into the real DWARF members as further sensor TUs land -- do not fork.

namespace CgsSceneManager
{
	// Penetration / contact handling reaches the solver only through DeformationSensor::
	// AddContactsToPenetrationSolver, which takes a PenetrationSolver* -- but a PotentialContact
	// (ValidateAndAddContact's input) is a CgsSceneManager contact record passed by const ref.
	struct PotentialContact;
}

namespace BrnPhysics
{
namespace Deformation
{
	// Collision sphere; its leading 16 bytes are centre.xyz + radius.w. Its canonical home
	// lives in the collision code — TagPoint::Construct only needs the centre, which it
	// reads as the sphere's leading Vector4, so a forward declaration suffices here and we
	// avoid forking the real type.
	struct Sphere;

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

	// One stored contact record. ClearNonWorldContacts copies it as eight 64-bit words (a 64-byte blob)
	// and tests the "non-world" flag at byte +0x34 within the record; the committed .cpp reaches no
	// other field, so the record is KEPT as a 64-byte POD with just that flag named so that committed
	// body compiles unchanged.
	//
	// FLAG (DWARF vs committed model): the DWARF (BrnDeformationSensor.h:42) gives the REAL members --
	//   mLocalPointOnA (Vector3), mLocalPointOnB (Vector3), mNormal (Vector3), mfProjectedDist (f32),
	//   mpOtherVehicle (DeformableObject*), mpOtherSensor (DeformationSensor*), mbValid (bool),
	//   + IsVehicleContact() const.
	// The committed mu32NonWorldFlag at +0x34 corresponds to the mbValid / world-vs-vehicle discriminator
	// region. The opaque model is retained (rather than promoted to the DWARF members) to keep the
	// committed ClearNonWorldContacts byte-stable; promote it together with re-deriving that body.
	struct StoredContact
	{
		u8  maHead[0x34];        // contact +0x00 .. +0x33 (opaque; real: mLocalPointOnA/B, mNormal, mfProjectedDist)
		u32 mu32NonWorldFlag;    // contact +0x34 (console sensor +0x54 for contact[0]); real: world/valid discriminator
		u8  maTail[0x40 - 0x38]; // contact +0x38 .. +0x3F (opaque; real: mpOtherVehicle/mpOtherSensor/mbValid)
	};

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

		// console +0x20 -- stored-contacts array (64-byte stride per contact). DWARF :266.
		StoredContact maStoredContacts[KU_MAX_STORED_CONTACTS];

		// ---- RECONSTRUCTED SCRATCH OVERLAY (kept for committed ClearNonWorldContacts) -----------
		// FLAG: in the DWARF the bytes from the contacts array end through the count are
		//   mImpulseContact (StoredImpulseContact, :267), mContactSpy (OutContactSpy, :268) and
		//   mVolInstId (CgsSceneManager::VolumeInstanceId, :269). The committed ClearNonWorldContacts
		// instead writes named scratch fields at console +0x118 / +0x120 / +0x130 / +0x180 (inferred
		// from its stores). To keep that committed body byte-stable, the inferred scratch members are
		// retained HERE (overlaying the DWARF mImpulseContact/mContactSpy/mVolInstId region) rather than
		// promoting the DWARF members. The opaque spans bridge to each named scratch offset relative to
		// the contacts-array base. PROMOTE to the DWARF members when ClearNonWorldContacts is re-derived.
		u8 maReserved1[0x118 - (0x20 + KU_MAX_STORED_CONTACTS * sizeof(StoredContact))];

		f32 mfMaxPointDisplacement;     // console +0x118 (280) -- reset to 100.0  (overlay; DWARF mImpulseContact region)
		f32 maPostPhysicsVec0[4];       // console +0x120 (288) -- zeroed (16 bytes)
		f32 maPostPhysicsVec1[4];       // console +0x130 (304) -- zeroed (16 bytes)

		// Opaque span between maPostPhysicsVec1 end (+0x140) and mu32PostPhysicsReset (+0x180).
		u8 maReserved2[0x180 - 0x140];

		u32 mu32PostPhysicsReset;       // console +0x180 (384) -- zeroed (overlay; DWARF mContactSpy/mVolInstId region)

		// Opaque span between mu32PostPhysicsReset end (+0x184) and the count (+0x198).
		u8 maReserved3[0x198 - 0x184];

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
		bool ValidateAndAddContact(Matrix44Affine lWorldTransform,
		                           const CgsSceneManager::PotentialContact& lrPotential,
		                           ContactId lContactId,
		                           DeformableObject* lpOtherVehicle,
		                           DeformationSensor* lpOtherSensor);

		// DWARF BrnDeformationSensor.cpp:718. Push this sensor's stored contacts into the shared
		// penetration solver (as vehicle or world contacts per the body indices). const.
		void AddContactsToPenetrationSolver(PenetrationSolver* lpSolver, DeformableObject* lpObject,
		                                    s32 liBodyIndex, s32 liWorldIndex, bool lbWorld) const;

		// DeformationSensor::ClearNonWorldContacts @ 0x825C1050 (COMMITTED body in BrnDeformationSensor.cpp).
		// Compact the stored-contacts array (remove contacts whose mu32NonWorldFlag is set) and reset the
		// post-physics scratch state. Caller (X360 xref): DeformableObject::UpdatePostPhysics.
		void ClearNonWorldContacts();
	};
}
}
