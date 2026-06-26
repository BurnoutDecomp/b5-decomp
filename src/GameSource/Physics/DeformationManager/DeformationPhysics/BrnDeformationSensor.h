#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                              // Vector3
#include "GameSource/Physics/ContactSpies/BrnContactId.h"  // BrnPhysics::ContactId

// Reconstructed slice of BrnPhysics::Deformation::DeformationSensor, homed at its mirrored
// DWARF path. The full sensor derives CollidableBody and carries the spec pointer, point
// displacement, stored contacts, a contact spy, a volume-instance id and scratch state.
// This pass models the members two TUs reach:
//   - TagPoint::Construct reads the three trailing members mpLocalSpaceSphere (Sphere*),
//     mpWorldSpaceSphere (Sphere*) and mfScratchAmount (f32, GetScratchAmount()).
//   - DeformationSensor::ClearNonWorldContacts @ 0x825C1050 compacts the stored-contacts
//     array (dropping contacts that are flagged "non-world") and resets the post-physics
//     scratch state. The fields it touches are modelled by name below.
//
// CONSOLE OFFSETS (X360, 4-byte pointers) used by ClearNonWorldContacts:
//   +0x20 (32)  -- stored-contacts array base, 64-byte (0x40) stride per contact
//   +0x54 (84)  -- the per-contact "is non-world" flag word (offset 0x34 within contact[0])
//   +0x118 (280)-- mfMaxPointDisplacement, reset to 100.0
//   +0x120 (288)-- a 16-byte vector, zeroed
//   +0x130 (304)-- a 16-byte vector, zeroed
//   +0x180 (384)-- a 32-bit count/flag, zeroed
//   +0x198 (408)-- mi32NumStoredContacts, the live contact count
//   +0x19C (412)/+0x1A0 (416)/+0x1A4 (420) -- mpLocalSpaceSphere / mpWorldSpaceSphere /
//     mfScratchAmount (TagPoint::Construct).
//
// X360 pointers are 32-bit; on the 64-bit host the two trailing Sphere* widen, so the
// absolute byte offsets above do NOT all hold on the host (the leading run up to the
// count is built from byte-exact-width primitives so it stays console-shaped, but the
// trailing pointers add host padding). Members are pinned BY NAME, never raw offset, and
// the sensor-array indexing in TagPoint/IKDrivenPoint uses sizeof(DeformationSensor) for
// stride consistently, so the exact host byte size is not load-bearing. GROW the still-
// opaque spans into their real members as further sensor TUs land — do not fork.

namespace BrnPhysics
{
namespace Deformation
{
	// Collision sphere; its leading 16 bytes are centre.xyz + radius.w. Its canonical home
	// lives in the collision code — TagPoint::Construct only needs the centre, which it
	// reads as the sphere's leading Vector4, so a forward declaration suffices here and we
	// avoid forking the real type.
	struct Sphere;

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

	// One stored contact record. ClearNonWorldContacts copies it as eight 64-bit words
	// (a 64-byte blob) and tests the "non-world" flag at byte +0x34 within the record; no
	// other field is reached in this pass, so the record is modelled as a 64-byte POD with
	// just that flag named. GROW into the real contact members when a contact-producing TU
	// lands. The 8 leading bytes carried in the moved qword whose high word seeds the
	// compaction index are part of this opaque payload.
	struct StoredContact
	{
		u8  maHead[0x34];        // contact +0x00 .. +0x33 (opaque)
		u32 mu32NonWorldFlag;    // contact +0x34 (console sensor +0x54 for contact[0])
		u8  maTail[0x40 - 0x38]; // contact +0x38 .. +0x3F (opaque)
	};

	struct DeformationSensor
	{
		// FLAG (best-effort capacity): the stored-contacts array base is console +0x20 and
		// the next named field (mfMaxPointDisplacement) is console +0x118 (280). The 248
		// bytes between hold the contacts array plus any inter-field padding; at a 64-byte
		// stride that bounds the capacity, but no asm/DWARF pins the exact maximum here.
		// The contacts array is modelled with the bounding capacity and a trailing opaque
		// span so the named post-physics block stays at its console offset relative to the
		// array base. mi32NumStoredContacts is the authoritative live count.
		static const u32 KU_MAX_STORED_CONTACTS = 3;

		// Opaque leading run (CollidableBody base + spec ptr + point displacement + ...)
		// preceding the stored-contacts array at console +0x20 (32).
		u8 maReserved0[0x20];

		// console +0x20 -- stored-contacts array (64-byte stride per contact).
		StoredContact maStoredContacts[KU_MAX_STORED_CONTACTS];

		// Opaque span between the contacts array end and mfMaxPointDisplacement (+0x118).
		u8 maReserved1[0x118 - (0x20 + KU_MAX_STORED_CONTACTS * sizeof(StoredContact))];

		f32 mfMaxPointDisplacement;     // console +0x118 (280) -- reset to 100.0
		f32 maPostPhysicsVec0[4];       // console +0x120 (288) -- zeroed (16 bytes)
		f32 maPostPhysicsVec1[4];       // console +0x130 (304) -- zeroed (16 bytes)

		// Opaque span between maPostPhysicsVec1 end (+0x140) and mu32PostPhysicsReset (+0x180).
		u8 maReserved2[0x180 - 0x140];

		u32 mu32PostPhysicsReset;       // console +0x180 (384) -- zeroed

		// Opaque span between mu32PostPhysicsReset end (+0x184) and the count (+0x198).
		u8 maReserved3[0x198 - 0x184];

		s32 mi32NumStoredContacts;      // console +0x198 (408) -- live contact count

		Sphere* mpLocalSpaceSphere;   // console +0x19C (412)
		Sphere* mpWorldSpaceSphere;   // console +0x1A0 (416)
		f32     mfScratchAmount;      // console +0x1A4 (420) — accumulated scratch / damage

		const Sphere* GetLocalSpaceSphere() const { return mpLocalSpaceSphere; }
		const Sphere* GetWorldSpaceSphere() const { return mpWorldSpaceSphere; }
		f32           GetScratchAmount()   const { return mfScratchAmount; }

		// DeformationSensor::ClearNonWorldContacts @ 0x825C1050. Compact the stored-contacts
		// array (remove contacts whose mu32NonWorldFlag is set) and reset the post-physics
		// scratch state. Caller (X360 xref): DeformableObject::UpdatePostPhysics.
		void ClearNonWorldContacts();
	};
}
}
