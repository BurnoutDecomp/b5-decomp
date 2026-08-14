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
		// ⭐⭐ REWRITTEN 2026-08-14 (walls leg 4, at-rest probe): the old model transformed pointA
		// with the ROTATION ONLY and skipped the sphere-centre rebase -- the record's local point
		// missed both the translation and the sensor origin (the at-rest solver pop measured it).
		// The PS3 words (@0x6CC104..0x6CC1A4) are:
		//   localA = lInverseVehicleTransform * pointA   (FULL affine: rows * splats + ROW3)
		//            - mpLocalSpaceSphere->centre        (vsubfp v10, v10, *(this+0x19C))
		//   localB = pointB RAW                          (stvx v8 -- world point, untransformed)
		//   normal = the (head-normalised) contact normal
		//   projDist = lane-sum of (pointA + mPointDisplacement_BiggestImpulseThisFrame.xyz
		//              - pointB)  (vaddfp/vsubfp then the vsldoi horizontal sum; v29 == ZERO, so
		//              the surrounding vmaddfp are register moves -- NOT a dot product)
		StoredContact lCandidate;
		StoredContactView& lView = AsContactView(lCandidate);

		const Vector3& lR  = lWorldTransform.Right();
		const Vector3& lU  = lWorldTransform.Up();
		const Vector3& lAt = lWorldTransform.At();
		const Vector3& lT  = lWorldTransform.Pos();

		Vector3 lLocalA{
			lPointOnA.x * lR.x + lPointOnA.y * lU.x + lPointOnA.z * lAt.x + lT.x,
			lPointOnA.x * lR.y + lPointOnA.y * lU.y + lPointOnA.z * lAt.y + lT.y,
			lPointOnA.x * lR.z + lPointOnA.y * lU.z + lPointOnA.z * lAt.z + lT.z, 0.0f };
		if ( mpLocalSpaceSphere != nullptr )
		{
			const Vector4& lrCentre = SphereVec(mpLocalSpaceSphere);
			lLocalA.x -= lrCentre.x;
			lLocalA.y -= lrCentre.y;
			lLocalA.z -= lrCentre.z;
		}
		lView.mLocalPointOnA = lLocalA;
		lView.mLocalPointOnB = lPointOnB;
		lView.mNormal        = lNormal;

		// projDist: the lane-sum key (see the block note above).
		const Vector3Plus& lrDisp = mPointDisplacement_BiggestImpulseThisFrame;   // this+0x10
		lView.mfProjectedDist = (lPointOnA.x + lrDisp.x - lPointOnB.x)
		                      + (lPointOnA.y + lrDisp.y - lPointOnB.y)
		                      + (lPointOnA.z + lrDisp.z - lPointOnB.z);
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

		// --- (4) the EARLIEST-IMPACT latch + contact-spy record -----------------------------------
		// ⭐⭐ REWRITTEN 2026-08-14 (walls leg 4), byte-witnessed against the PS3 body
		// (@0x6CC28C..0x6CC804; the X360 twin inlines the same stores). The old landed model
		// ("biggest-impulse displacement latch") had the ARITHMETIC but landed the stores on
		// overlay names at the wrong slots -- float writes over the record's POINTER fields.
		// Latent while nothing read the record; UpdateContacts (landed this wave) reads it.
		//
		// Candidate impact time:
		//   v12 = dot3( (otherCentre - pointB), thisNormal )       (penetration term)
		//   v10 = dot3( thisNormal, thisNormal ) (+ the other-sensor term when lpOtherSensor != 0)
		//   lfImpactTime = max( penetration / basis, 0 )           (vrefp + 2 Newton refines, vmaxfp 0)
		// The latch (PS3 0x6CC744..0x6CC790): basis > 0 AND lfImpactTime <= 1.0 AND
		// lfImpactTime >= 0 AND mImpulseContact.mfImpactTimeInFrame > lfImpactTime -- the record
		// starts each frame DISARMED at 100.0, so the first valid contact arms it and later
		// SMALLER times replace it: the EARLIEST impact of the frame wins.
		Vector3 lOtherCentre = { 0.0f, 0.0f, 0.0f, 0.0f };
		if ( lpOtherSensor && lpOtherSensor->mpLocalSpaceSphere )
		{
			const Vector4& lc = SphereVec(lpOtherSensor->mpLocalSpaceSphere);
			lOtherCentre = Vector3{ lc.x, lc.y, lc.z, 0.0f };
		}
		const f32 lfPenetration = Dot3(Sub3(lOtherCentre, lPointOnB), lNormal);

		f32 lfBasis = Dot3(lNormal, lNormal);
		if ( lpOtherSensor )
		{
			lfBasis += Dot3(lNormal, lNormal);   // + other-sensor contribution (vaddfp v10,v10,v0)
		}
		const f32 lfRatio      = (lfBasis != 0.0f) ? (lfPenetration / lfBasis) : 0.0f;
		const f32 lfImpactTime = (lfRatio > 0.0f) ? lfRatio : 0.0f;   // vmaxfp v13, v0, zero

		if ( lfBasis > 0.0f                                        // vcmpgtfp v10 > 0 (dword_100A564 == 0.0)
		     && lfImpactTime <= 1.0f                               // vcmpgefp 1.0 >= v13
		     && lfImpactTime >= 0.0f                               // vcmpgefp v13 >= 0
		     && mImpulseContact.mfImpactTimeInFrame > lfImpactTime ) // stored (+0x118) > candidate
		{
			// The impulse record (sensor +0xE0..+0x11F), field for field per the PS3 stores:
			mImpulseContact.mPointOnA           = lPointOnA;       // stvx v9,  this,0xE0  (potential +0)
			mImpulseContact.mPointOnB           = lPointOnB;       // stvx v0,  this,0xF0  (potential +0x10)
			mImpulseContact.mNormal             = lNormal;         // stvx v30, this,0x100 (potential +0x20)
			mImpulseContact.mpOtherVehicle      = lpOtherVehicle;  // stw  this,0x110
			mImpulseContact.mpOtherSensor       = lpOtherSensor;   // stw  this,0x114
			mImpulseContact.mfImpactTimeInFrame = lfImpactTime;    // stw  this,0x118
			mImpulseContact.mContactId          = lContactId;      // stw  this,0x11C

			// The contact-spy record (sensor +0x140..+0x183):
			const u64* lpIds = reinterpret_cast<const u64*>(&lpPotential[3]);   // +0x30 idA, +0x38 idB
			mSpyNormal            = lNormal;                                    // stvx v30, this,0x140
			mSpyPointOnA          = lPointOnA;                                  // stvx (potential+0),  this,0x150
			mSpyPointOnB          = lPointOnB;                                  // stvx (potential+0x10),this,0x160
			mSpyVolumeInstanceIdA = lpIds[0] & 0xFFFFFFFF00000000ull;           // std (idA word)<<32, this,0x170
			mSpyVolumeInstanceIdB = lpIds[1] & 0xFFFFFFFF00000000ull;           // std (idB word)<<32, this,0x178
			mSpyContactId         = static_cast<u32>(lContactId);               // stw r16, this,0x180 (operator u32)
		}

		bool lbAccepted = false;

		// Final boolean: the two outer gates at LABEL_29. The contact is accepted (true) when the
		// projected separation is positive AND the displacement is within tolerance, OR when the
		// penetration term clears its own positive gate. Otherwise false.
		if ( lfPenetration > 0.0f && lfBasis >= 0.0f )
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
