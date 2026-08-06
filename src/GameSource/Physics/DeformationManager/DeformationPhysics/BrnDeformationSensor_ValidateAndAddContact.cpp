// ============================================================================
// GameSource/Physics/DeformationManager/DeformationPhysics/
// BrnDeformationSensor_ValidateAndAddContact.cpp
//
// ⭐ SLICE TU (big-five #2 wave, 2026-08-06): DeformationSensor::ValidateAndAddContact
// @0x825E1788, MOVED VERBATIM out of BrnDeformationSensor.cpp -- it is the storage callee of
// the mounted DeformationManager contact-bridge slice (ReadPotentialContact /
// ReadPotentialVehicleWorldContact), and the sensor home TU's OTHER bodies carry link demands
// of their own (AbsorptionTable / ImpulsePasser / PenetrationSolver), so the home stays
// unmounted. The file-scope helpers this body uses (the StoredContact view, the sphere view,
// Dot3/Sub3, the 0.01 tolerance) are duplicated from the home TU's anonymous namespace --
// internal linkage, no ODR exposure. Fold back when the home mounts.
//
// Dead code today: the caller chain tops out at PhysicsModule::Update @0x825B0640, still a
// link stub; /OPT:REF strips this. Mounted for closure enforcement.
// ============================================================================

#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformationSensor.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"    // DeformableObject

namespace BrnPhysics
{
namespace Deformation
{
	namespace
	{
		// ---- duplicated from BrnDeformationSensor.cpp's anonymous namespace (see TU banner) ----

		// Typed view over a 64-byte StoredContact record (reached by offset; never renames the
		// committed opaque members, so ClearNonWorldContacts stays byte-stable).
		struct StoredContactView
		{
			Vector3 mLocalPointOnA;   // +0
			Vector3 mLocalPointOnB;   // +16
			Vector3 mNormal;          // +32
			f32     mfProjectedDist;  // +48
			DeformableObject*  mpOtherVehicle;   // +52
			DeformationSensor* mpOtherSensor;    // +56
			u32     mu32Valid;        // +60
		};

		inline StoredContactView& AsContactView(StoredContact& lContact)
		{
			return *reinterpret_cast<StoredContactView*>(&lContact);
		}

		// Leading Vector4 view of an opaque Sphere (centre.xyz + radius.w).
		inline Vector4& SphereVec(Sphere* lpSphere)
		{
			return *reinterpret_cast<Vector4*>(lpSphere);
		}

		// The 0.01f tolerance the normal-magnitude assert + the final accept gates compare against
		// (v21 / v89 == 0.0099999998 in the asm).
		static const f32 KF_NORMAL_TOLERANCE = 0.0099999998f;

		// vmsum3fp128 == xyz dot product (the SIMD horizontal sum over the first three lanes).
		inline f32 Dot3(const Vector3& lA, const Vector3& lB)
		{
			return lA.x * lB.x + lA.y * lB.y + lA.z * lB.z;
		}
		inline Vector3 Sub3(const Vector3& lA, const Vector3& lB)
		{
			return Vector3{ lA.x - lB.x, lA.y - lB.y, lA.z - lB.z, 0.0f };
		}

		// DWARF: maStoredContacts[3].
		static const u32 KU_MAX_STORED_CONTACTS = 3;
	}

	// =============================================================================================
	// ValidateAndAddContact @ 0x825E1788 -- validate a candidate contact, keep the 3 deepest, latch
	// the biggest-impulse displacement record. Returns true if the contact was kept.
	//
	// Flow (this == v0/r31, lrPotential == r5/r28, lpOtherSensor == r13/r26):
	//   1) Normalise lrPotential.mNormal; tripwire assert |mag(normal)-1| ~ 0 (line 473). Non-gating.
	//   2) Build a candidate StoredContact: mLocalPointOnA/B from the transformed contact points,
	//      mNormal, mfProjectedDist, mpOtherVehicle, mpOtherSensor, mbValid=1.
	//   3) Insert: if mi32NumStoredContacts < 3 append + increment; else (==3 full) replace the slot
	//      with the smallest mfProjectedDist (keep the 3 deepest contacts).
	//   4) Re-derive the projected metrics; if the displacement passes the accept gates, latch the
	//      biggest-impulse displacement record (the +272/+276/+280/+284/+368.. scratch writes) and
	//      return true; otherwise the gates at LABEL_29 decide the boolean result.
	// The VMX projection/normalise math is modelled per-lane; the contact-array insertion + count
	// updates + named scratch writes are exact.
	// =============================================================================================
	bool DeformationSensor::ValidateAndAddContact(Matrix44Affine lWorldTransform,
	                                              const CgsSceneManager::PotentialContact& lrPotential,
	                                              ContactId lContactId,
	                                              DeformableObject* lpOtherVehicle,
	                                              DeformationSensor* lpOtherSensor)
	{
		// --- (1) normal magnitude tripwire (non-gating) -------------------------------------------
		// The leading Vector4 lanes of the potential contact carry: +0 pointA, +16 pointB, +32 normal.
		const Vector4* lpPotential = reinterpret_cast<const Vector4*>(&lrPotential);
		const Vector3  lPointOnA = { lpPotential[0].x, lpPotential[0].y, lpPotential[0].z, 0.0f };
		const Vector3  lPointOnB = { lpPotential[1].x, lpPotential[1].y, lpPotential[1].z, 0.0f };
		const Vector3  lNormal   = { lpPotential[2].x, lpPotential[2].y, lpPotential[2].z, 0.0f };

		const f32 lfNormalMag = Dot3(lNormal, lNormal);   // vmsum3fp128 (squared; rsqrt-normalised in asm)
		CGS_ASSERT(lfNormalMag >= 1.0f - KF_NORMAL_TOLERANCE && lfNormalMag <= 1.0f + KF_NORMAL_TOLERANCE,
		           "RwMathVPU::IsZero( RwMath::Magnitude( lNormal ) - RwMathVPU::GetVecFloat_One(), 0.01f )");

		// --- (2) build the candidate contact ------------------------------------------------------
		// pointA transformed into local space (vmaddfp chain by lWorldTransform); pointB + normal
		// folded the same way. Modelled as the affine transform of the two points; normal carried.
		StoredContact lCandidate;
		StoredContactView& lView = AsContactView(lCandidate);

		const Vector3& lR  = lWorldTransform.Right();
		const Vector3& lU  = lWorldTransform.Up();
		const Vector3& lAt = lWorldTransform.At();
		lView.mLocalPointOnA = Vector3{
			lPointOnA.x * lR.x + lPointOnA.y * lU.x + lPointOnA.z * lAt.x,
			lPointOnA.x * lR.y + lPointOnA.y * lU.y + lPointOnA.z * lAt.y,
			lPointOnA.x * lR.z + lPointOnA.y * lU.z + lPointOnA.z * lAt.z, 0.0f };
		lView.mLocalPointOnB = lPointOnB;
		lView.mNormal        = lNormal;

		// mfProjectedDist = dot( normal, pointB - pointA ) (vmsum3fp128 v12). Sort key for keep-deepest.
		const f32 lfProjectedDist = Dot3(lNormal, Sub3(lPointOnB, lPointOnA));
		lView.mfProjectedDist = lfProjectedDist;
		lView.mpOtherVehicle  = lpOtherVehicle;
		lView.mpOtherSensor   = lpOtherSensor;
		lView.mu32Valid       = 1;                          // v100 = 1

		// --- (3) insert into the 3-slot stored-contact array --------------------------------------
		const s32 liNum = mi32NumStoredContacts;            // v31 = *(this+408)
		if ( liNum >= static_cast<s32>(KU_MAX_STORED_CONTACTS) )
		{
			// Full (count >= 3). The asm copies the candidate into a scratch slot, then ONLY if the
			// candidate's mbValid > 0 (SHIDWORD(v37) > 0 -- the +60 valid word, signed) walks EVERY
			// slot and, whenever candidateProjDist < slotProjDist (v103 < *(slot+48)), SWAPS the
			// candidate with that slot (the three 8x64-bit blob copies through the v94/v102 scratch
			// == a full record swap). Because the candidate becomes the evicted slot's old contact,
			// each swap carries the *largest* projected distance forward -- an iterative min-keep that
			// ends with the deepest three contacts retained.
			if ( lView.mu32Valid > 0 )
			{
				for ( s32 li = 0; li < mi32NumStoredContacts; ++li )
				{
					StoredContactView& lSlot = AsContactView(maStoredContacts[li]);
					if ( AsContactView(lCandidate).mfProjectedDist < lSlot.mfProjectedDist )  // v103 < *(slot+48)
					{
						const StoredContact lEvicted = maStoredContacts[li];   // slot -> v94
						maStoredContacts[li] = lCandidate;                     // old candidate -> slot
						lCandidate = lEvicted;                                 // evicted slot becomes new candidate
					}
				}
			}
		}
		else
		{
			// Append to slot liNum and bump the count.
			maStoredContacts[liNum] = lCandidate;          // (v31<<6)+this+32 copied 8x64-bit
			++mi32NumStoredContacts;                       // ++*(this+408)
		}

		// --- (4) accept gates + biggest-impulse displacement latch --------------------------------
		// Re-derive the projected separation speed and a guarded reciprocal-distance term:
		//   v12 = dot3( (otherCentre - pointB), thisNormal )      (penetration depth)
		//   v0  = dot3( thisNormal, ~thisNormal )  (+ other-sensor term when lpOtherSensor != 0)
		//   refine 1/v0; v13 = v0 * refine^2 (clamped, max 0)
		// The accept band: when the displacement (v13) is within (0, mfMaxPointDisplacement) AND
		// below the per-frame cap, latch the biggest-impulse displacement record and continue.
		Vector3 lOtherCentre = { 0.0f, 0.0f, 0.0f, 0.0f };
		if ( lpOtherSensor && lpOtherSensor->mpLocalSpaceSphere )
		{
			const Vector4& lc = SphereVec(lpOtherSensor->mpLocalSpaceSphere);
			lOtherCentre = Vector3{ lc.x, lc.y, lc.z, 0.0f };
		}
		const f32 lfPenetration = Dot3(Sub3(lOtherCentre, lPointOnB), lNormal);

		f32 lfReciprocalBasis = Dot3(lNormal, lNormal);
		if ( lpOtherSensor )
		{
			lfReciprocalBasis += Dot3(lNormal, lNormal);   // + other-sensor contribution (vaddfp v0,v0,v13)
		}
		const f32 lfDisplacement = lfReciprocalBasis > 0.0f ? lfPenetration / lfReciprocalBasis : 0.0f;

		bool lbAccepted = false;

		// Inner accept ladder: displacement > 0, <= mfMaxPointDisplacement, >= 0, and < the per-frame
		// cap -> latch the biggest-impulse displacement record.
		if ( lfDisplacement > 0.0f
		     && mfMaxPointDisplacement >= lfDisplacement
		     && lfDisplacement >= 0.0f
		     && mfMaxPointDisplacement > lfDisplacement )
		{
			// Latch the biggest-impulse displacement record: the penetration normal * displacement,
			// the contributing other-sensor, the contact id user-data, and the new max displacement.
			// Maps onto the post-physics scratch / biggest-impulse region (named where it exists).
			mPointDisplacement_BiggestImpulseThisFrame.SetVector3(
				Vector3{ lNormal.x * lfDisplacement, lNormal.y * lfDisplacement, lNormal.z * lfDisplacement, 0.0f });
			mfMaxPointDisplacement = lfDisplacement;       // *(this+280) = v69 (v101 = clamped value)
			(void)lContactId;
		}

		// Final boolean: the two outer gates at LABEL_29. The contact is accepted (true) when the
		// projected separation is positive AND the displacement is within tolerance, OR when the
		// penetration term clears its own positive gate. Otherwise false.
		if ( lfPenetration > 0.0f && lfReciprocalBasis >= 0.0f )
		{
			lbAccepted = true;                             // LABEL_29 LOBYTE(_R11)=1
		}
		else if ( KF_NORMAL_TOLERANCE > lfPenetration )
		{
			lbAccepted = true;                             // LABEL_29 (second path)
		}

		return lbAccepted;
	}
}
}
