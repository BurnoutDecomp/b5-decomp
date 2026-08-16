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
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint -- the opt-in [latch] probe only
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"    // DeformableObject

#include <cstdlib>   // getenv    -- the opt-in [latch] bring-up probe only
#include <cstdint>   // uintptr_t -- the opt-in [latch] bring-up probe only

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
	// the EARLIEST-IMPACT record. Returns true if the contact was kept.
	//
	// Flow (this == v0/r31, lrPotential == r5/r28, lpOtherSensor == r13/r26):
	//   1) Normalise lrPotential.mNormal; tripwire assert |mag(normal)-1| ~ 0 (line 473). Non-gating.
	//   2) Build a candidate StoredContact: mLocalPointOnA/B from the transformed contact points,
	//      mNormal, mfProjectedDist, mpOtherVehicle, mpOtherSensor, mbValid=1.
	//   3) Insert: if mi32NumStoredContacts < 3 append + increment; else (==3 full) replace the slot
	//      with the smallest mfProjectedDist (keep the 3 deepest contacts).
	//   4) Form the swept impact time t = penetrationDepth / closingDisplacement, both measured
	//      ALONG THE CONTACT NORMAL; if it is in [0,1] and earlier than the record's stored time,
	//      latch mImpulseContact + the contact spy. Then return on the basis/t gates.
	// The VMX projection/normalise math is modelled per-lane; the contact-array insertion + count
	// updates + named scratch writes are exact.
	//
	// ⭐⭐⭐ 2026-08-16 (walls leg 10). Step (4) is THE gate on the whole deformation impulse path:
	// mImpulseContact is the only thing DeformationSensor::GetImpulse reads, GetImpulse is the only
	// feed into DeformableObject::UpdateContacts, and UpdateContacts is the only caller of
	// ApplyCarWorldImpulse. Its arithmetic was wrong in three places (see the block notes below),
	// which is why a wall was detected by the penetration solver for nine legs and never produced
	// a single impulse. Measured before the fix, at 30.4 m/s into the junkyard wall:
	//   [latch] WALLFACE nrm 0.512652 0 0.858597 | pen 208.821777 basis 1.000000 t 208.821747
	//           latched 0        <- "t" was a WORLD COORDINATE, so the t <= 1.0 gate could not pass
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
		//   projDist = dot3( normal, pointA + mPointDisplacement_BiggestImpulseThisFrame.xyz
		//              - pointB )  -- see the corrected note at the projDist store below
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

		// projDist: the swept penetration depth ALONG THE NORMAL.
		// ⭐⭐ FIXED 2026-08-16 (walls leg 10). This was a plain LANE SUM, on the strength of a
		// comment reading "v29 == ZERO, so the surrounding vmaddfp are register moves -- NOT a dot
		// product". That reading put the addend in the wrong print position: PS3 @0x6CC164
		// `vmaddfp v0, v5, v29, v0` is `v0 = v5*v0 + v29` with **v5 == v30 == the NORMAL**
		// (`vmr v5, v30` @0x6CC0AC), and the vsldoi/vaddfp chain then horizontal-sums it.
		// The X360 twin removes all doubt by using the explicit dot instruction:
		//   0x825E18D4 vaddfp      v6,  v13(pointOnA), v6(this+0x10)
		//   0x825E1910 vsubfp      v6,  v6,  v0(pointOnB)
		//   0x825E1920 vmsum3fp128 v12, v12(NORMAL), v6      <- a dot product with the normal
		// It is the key the "keep the 3 deepest" insertion sorts on, and a lane sum of world
		// coordinates is not a depth: at the junkyard wall it was ~ -3000 for every contact.
		const Vector3Plus& lrDisp = mPointDisplacement_BiggestImpulseThisFrame;   // this+0x10
		const Vector3 lSweptDelta = { lPointOnA.x + lrDisp.x - lPointOnB.x,
		                              lPointOnA.y + lrDisp.y - lPointOnB.y,
		                              lPointOnA.z + lrDisp.z - lPointOnB.z, 0.0f };
		lView.mfProjectedDist = Dot3(lNormal, lSweptDelta);
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
		// ⭐⭐⭐ REWRITTEN 2026-08-16 (walls leg 10) -- BOTH TERMS WERE WRONG, and the pair is why a
		// wall took no momentum for nine legs. Byte-witnessed on BOTH consoles; the X360 spells the
		// dot products with `vmsum3fp128`, so there is no operand-order judgement left to make:
		//   X360 0x825E1AD0  vsubfp      v12, v11(potential+0),    v0(potential+0x10)
		//        0x825E1AE4  vmsum3fp128 v12, v12,                 v124(NORMAL)   <- NUMERATOR
		//        0x825E1AD8  lvx128      v13, r0, r4   with r4 = r31+0x10 (0x825E18A8)
		//        0x825E1ADC  vxor128     v0,  v124, <0x80000000>                  <- -NORMAL
		//        0x825E1AE0  vmsum3fp128 v0,  v13(thisDisp),       v0(-NORMAL)    <- BASIS
		//        0x825E1AF0  vmsum3fp128 v13, (otherSensor+0x10),  v124(+NORMAL)  <- BASIS +=
		//   PS3  0x6CC28C..0x6CC2FC is the same body in plain Altivec (r18 = this+0x10 @0x6CC0F8).
		//
		// The numerator is the PENETRATION DEPTH along the normal and the basis is the CLOSING
		// DISPLACEMENT along it, so the quotient is the fraction of this frame's sweep at which the
		// surfaces meet -- an impact TIME in [0,1], which is exactly what the three gates below and
		// GetImpulse's own > 1.0 sentinel expect.
		//
		// ⛔ WHAT WAS HERE. The numerator read `dot3(otherSensorCentre - pointOnB, normal)` and the
		// basis was `dot3(normal, normal)` (== 1.0, doubled when a second sensor was present). Two
		// separate faults in one expression:
		//   * `otherSensorCentre` is an INVENTION -- this function never touches the other sensor's
		//     SPHERE, only its DISPLACEMENT -- and it is identically zero for a world contact, so
		//     the numerator degenerated to `-dot3(pointOnB, normal)`: A WORLD COORDINATE. Measured
		//     live at the junkyard wall it was 208.82 where the console's is 1.905 m.
		//   * a basis of 1.0 is not a length at all, so the quotient was never a time. With
		//     `t == 208.82` the `t <= 1.0` gate below could not pass, the latch never armed,
		//     mImpulseContact stayed at its 100.0 disarm sentinel, GetImpulse returned false and
		//     ApplyCarWorldImpulse was never called for a wall. That was the whole blockage.
		// ⚠️ The basis is signed and one-sided BY DESIGN: only a contact the sensor is closing on
		// has basis > 0, which is how a separating contact is rejected. Do not "fix" it to fabs.
		const f32 lfPenetration = Dot3(Sub3(lPointOnA, lPointOnB), lNormal);

		f32 lfBasis = -Dot3(lrDisp.GetVector3(), lNormal);   // vmsum3fp128 v0, thisDisp, -NORMAL
		if ( lpOtherSensor )
		{
			// + the other sensor's own sweep along the SAME normal (vaddfp v10,v10,v0).
			lfBasis += Dot3(
				lpOtherSensor->mPointDisplacement_BiggestImpulseThisFrame.GetVector3(), lNormal);
		}
		// The console divides unconditionally (vrefp + 2 Newton refines) and lets the `basis > 0`
		// gate discard the result; guarding the zero here is value-identical and avoids the inf.
		const f32 lfRatio      = (lfBasis != 0.0f) ? (lfPenetration / lfBasis) : 0.0f;
		const f32 lfImpactTime = (lfRatio > 0.0f) ? lfRatio : 0.0f;   // vmaxfp v13, v0, zero

		const bool lbLatch = ( lfBasis > 0.0f                          // vcmpgtfp v10 > 0 (dword_100A564 == 0.0)
		                       && lfImpactTime <= 1.0f                 // vcmpgefp 1.0 >= v13
		                       && lfImpactTime >= 0.0f                 // vcmpgefp v13 >= 0
		                       && mImpulseContact.mfImpactTimeInFrame > lfImpactTime ); // stored (+0x118) > cand

		// ---- [latch] PC bring-up instrument -- DELETE WHEN the wall test is banked ---------------
		// OPT-IN (BRN_LATCH_PROBE=1) so a default run and every golden gate stay byte-identical.
		//
		// ⭐ WHY THIS PROBE EXISTS. Nine legs measured this chain from DOWNSTREAM ("|vz| fell",
		// "route WALL fired") and every one of those signals turned out to be produced by the
		// GROUND. `mImpulseContact` is the ONLY feed into the deformation impulse path
		// (GetImpulse -> UpdateContacts -> ApplyCarWorldImpulse), so the question "why does a wall
		// take no momentum" reduces exactly to "does a wall-normal contact arm this latch". One
		// line per candidate answers it, and prints BOTH the arithmetic the tree runs and the
		// arithmetic the two consoles run, so the comparison is inside a single run rather than
		// across two.
		// Two windows (leg 9's rule -- cap the EVENTS you are counting, not the calls you filter):
		// a short opening window that proves the probe can speak, plus a window reserved for
		// horizontal normals, which is the rare event actually being hunted.
		{
			static s32 siLatchProbe = -1;
			if ( siLatchProbe < 0 )
			{
				const char* lpcEnv = getenv( "BRN_LATCH_PROBE" );
				siLatchProbe = ( lpcEnv != 0 && lpcEnv[0] != '0' ) ? 1 : 0;
			}
			static u32 suSeen = 0;
			static u32 suWall = 0;
			++suSeen;
			const f32  lfAbsNY   = ( lNormal.y < 0.0f ) ? -lNormal.y : lNormal.y;
			const bool lbWallish = ( lfAbsNY < 0.6f );   // the [wall] probe's own discriminator
			if ( siLatchProbe == 1 && CgsDev::Log::gpDebugPrint != 0
			     && ( suSeen <= 24u || ( lbWallish && ++suWall <= 400u ) ) )
			{
				// ⚠️ THE SENSOR IDENTITY IS PART OF THE EVIDENCE, not decoration: consecutive
				// candidates carrying byte-identical contact points are either one contact offered
				// to many sensors (each of which will raise its own full-strength impulse) or the
				// same sensor re-offered the same contact. Those have different fixes, and without
				// `sensor` in the line the two are indistinguishable in the log.
				// ⭐ walls leg 11: the VOLUME-INSTANCE LOW BYTE is the console's ownership key
				// (ReadPotentialVehicleWorldContact hands the whole id to
				// GetDeformationSensorFromVolumeInstance @0x825B4338, whose first instruction is
				// `clrlwi r11, r4, 24` -- the low byte, minus one, IS the sensor index). Printing it
				// beside the sensor address turns "20 sensors saw one contact" from an inference
				// into a reading: distinct low bytes mean twenty distinct CONTACTS were generated,
				// not one contact fanned out by a broken lookup.
				const u64* lpProbeIds = reinterpret_cast<const u64*>(&lpPotential[3]);
				*CgsDev::Log::gpDebugPrint
					<< "[latch] n " << static_cast<s32>(suSeen)
					<< " sensor " << static_cast<s32>(reinterpret_cast<uintptr_t>(this) & 0xFFFFFu)
					<< " volA " << static_cast<s32>(lpProbeIds[0] & 0xFFu)
					<< ( lbWallish ? " WALLFACE" : " floor" )
					<< " nrm " << lNormal.x << " " << lNormal.y << " " << lNormal.z
					<< " pA " << lPointOnA.x << " " << lPointOnA.y << " " << lPointOnA.z
					<< " pB " << lPointOnB.x << " " << lPointOnB.y << " " << lPointOnB.z
					<< " disp " << lrDisp.x << " " << lrDisp.y << " " << lrDisp.z
					<< " | pen " << lfPenetration << " basis " << lfBasis
					<< " t " << lfImpactTime
					<< " stored " << mImpulseContact.mfImpactTimeInFrame
					<< " latched " << static_cast<s32>(lbLatch ? 1 : 0)
					<< "\n";
			}
		}

		if ( lbLatch )
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

		// Final boolean -- ⭐ CORRECTED 2026-08-16 (walls leg 10). The first arm read
		// `lfPenetration > 0 && lfBasis >= 0`; the console's is the SAME PAIR OF GATES THE LATCH
		// USES, minus the freshness test. Cross-witnessed:
		//   X360 0x825E1CA0  vcmpgtfp.    v0, v0(BASIS), <0.0>       -> beq to the second arm
		//        0x825E1CB8  vcmpgefp128. v0, v126(1.0), v13(t)      -> bne to `li r11, 1`
		//        0x825E1CF0  vcmpgtfp.    v0, <0.01>,    v12(NUMERATOR)
		//   PS3  0x6CC380/0x6CC38C (basis > 0, then 1.0 >= t) and 0x6CC43C (0.01 > numerator).
		// The second arm was already right, and its constant is not merely NUMERICALLY the same
		// 0.01 -- it is the SAME LOAD: X360 keeps it in f31 from the prologue and spends it on the
		// normal-magnitude assert at 0x825E1864 and here at 0x825E1CD0 (PS3: dword_100A5F8 both
		// times). `.rdata` reads 0x82002138 == 0.0099999998 and 0x82001CC0 == 0.0.
		if ( lfBasis > 0.0f && lfImpactTime <= 1.0f )
		{
			lbAccepted = true;                             // 0x825E1D04 li r11, 1
		}
		else if ( KF_NORMAL_TOLERANCE > lfPenetration )
		{
			lbAccepted = true;                             // 0x825E1CF0 -> the same li r11, 1
		}

		return lbAccepted;
	}
}
}
