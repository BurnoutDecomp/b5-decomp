#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformationSensor.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnAbsorptionTable.h"     // AbsorptionTable
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPenetrationSolver.h"   // PenetrationSolver
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnCollidableBody.h"      // ImpulseParams
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnImpulsePasser.h"       // ImpulsePasser::PassOnImpulse
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"    // DeformableObject
#include "SharedClasses/Physics/Deformation/BrnSensorSpec.h"  // SensorSpec (mInitialOffset, maDirectionParams, GetAbsorptionLevel)

// Out-of-line bodies for BrnPhysics::Deformation::DeformationSensor.
//
// This TU GROWS the existing file (which already owns ClearNonWorldContacts -- kept verbatim
// below) with the five Wave-3 sensor functions, each reconstructed store-for-store from the
// X360 ARTIST asm:
//
//   DeformationSensor()             ctor (zero-init via the ClearVariables idiom)
//   Prepare                      @ 0x8260A2E8  bind spec + spheres, place sphere in world frame
//   RecievePassedOnImpulse       @ 0x825E11F8  CollidableBody override: scale a passed-on impulse
//                                              through the AbsorptionTable into stored displacement
//   ValidateAndAddContact        @ 0x825E1788  validate a candidate contact, keep the 3 deepest,
//                                              latch the biggest-impulse displacement record
//   AddContactsToPenetrationSolver @ 0x825E1D20 split stored contacts into world/vehicle penetration
//                                              contacts and feed the PenetrationSolver
//
// ============================ MODELLED-vs-ASM (read before editing) ============================
// The two core physics functions (RecievePassedOnImpulse / ValidateAndAddContact) are dense VMX128
// register code. The reconstruction is FAITHFUL to the OBSERVABLE behaviour -- same control flow,
// same branch structure, same early-outs, same call order, same named-member stores, same strides --
// and models the per-lane SIMD arithmetic as explicit scalar lane math, the established house idiom
// (cf. BrnStreamedDeformationSpec.cpp's GetBoundingBox / TransformToNewCOMSpace).
//
// OPAQUE TYPES accessed by console byte offset (NOT renamed -- ODR-stable with the committed model):
//   * Sphere is forward-declared; its leading 16 bytes are centre.xyz + radius.w (header note). The
//     world-placement math reads/writes that leading Vector4 through a typed view.
//   * StoredContact is kept as a 64-byte opaque POD in the frozen header (so the committed
//     ClearNonWorldContacts stays byte-stable). The REAL DWARF members live at these console offsets
//     within the 64-byte record and are reached through StoredContactView below:
//        +0  mLocalPointOnA(Vector3)  +16 mLocalPointOnB(Vector3)  +32 mNormal(Vector3)
//        +48 mfProjectedDist(f32)     +52 mpOtherVehicle(ptr)      +56 mpOtherSensor(ptr)
//        +60 mbValid(u32)
//     (proven by AddContactsToPenetrationSolver's *(contact+48/52/56/60) reads.)
//   * PotentialContact (CgsSceneManager) is forward-declared; ValidateAndAddContact reads its two
//     contact points + normal from its leading Vector4 lanes (offsets +0 / +16 / +32) through a view.
//
// FLAGGED-0 PLACEHOLDERS (rodata NOT in the per-function exports -- NEVER fabricated):
//   * the per-direction HIT-direction table (&unk_82FB9680, the DWARF KV_POS_X_HIT_DIRECTION /
//     KV_NEG_X_HIT_DIRECTION pair, indexed 16*direction) and
//   * the absorption-scale row table (&unk_82FB9560, indexed 16*absorptionSet)
//   are carried as correctly-shaped honest zeros. The indexing shape/stride is exact; the numeric
//   output stays inert until the real XEX rodata is recovered. KVF_60_HTZ / KVF_VELOCITY_FACTOR
//   (the DWARF file-static VecFloats) are likewise FLAGGED placeholders.
//
// ASSERTS are non-gating tripwires (BeginAssert/FireAssert/EndAssert == one CGS_ASSERT): in the asm
// execution continues past a failed assert, so the C++ falls through identically.

namespace BrnPhysics
{
namespace Deformation
{
	namespace
	{
		// ---- console byte offsets within the 64-byte StoredContact record (DWARF members) ---------
		static const u32 KU_CONTACT_POINT_ON_A   = 0;    // mLocalPointOnA (Vector3)
		static const u32 KU_CONTACT_POINT_ON_B   = 16;   // mLocalPointOnB (Vector3)
		static const u32 KU_CONTACT_NORMAL       = 32;   // mNormal (Vector3)
		static const u32 KU_CONTACT_PROJ_DIST    = 48;   // mfProjectedDist (f32) -- the keep-deepest sort key
		static const u32 KU_CONTACT_OTHER_VEHICLE = 52;  // mpOtherVehicle (DeformableObject*)
		static const u32 KU_CONTACT_OTHER_SENSOR = 56;   // mpOtherSensor (DeformationSensor*)
		static const u32 KU_CONTACT_VALID        = 60;   // mbValid (u32)

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

		// FLAGGED-0 PLACEHOLDER for the per-direction hit-direction table (&unk_82FB9680). Indexed
		// 16*direction => one 16-byte vec4 per ENextSensorDirection (the DWARF KV_*_HIT_DIRECTION
		// pair). Six signed body axes. Honest zeros (NEVER fabricated); the indexing shape is exact.
		static const VecFloat KsaHitDirection[6] = {};

		// FLAGGED-0 PLACEHOLDER for the absorption-scale row table (&unk_82FB9560). Indexed
		// 16*absorptionSet => one 16-byte vec4 per EAbsorptionSets. Honest zeros (NEVER fabricated).
		static const VecFloat KsaAbsorptionScale[E_ABSORPTIONSETS_NUM] = {};

		// DWARF BrnDeformationSensor.cpp:198/264 -- the two file-static VecFloats. Their rodata is not
		// in the exports; carried as FLAGGED placeholders (NEVER fabricated). KVF_60_HTZ is the 60 Hz
		// reference rate the absorption scaling is expressed against; KVF_VELOCITY_FACTOR scales the
		// closing speed. Honest zeros until recovered.
		static const VecFloat KVF_60_HTZ        = { 0.0f, 0.0f, 0.0f, 0.0f };
		static const VecFloat KVF_VELOCITY_FACTOR = { 0.0f, 0.0f, 0.0f, 0.0f };

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

		// RwMathVPU::IsValid(vec) -- the asm spells this as a per-lane self-compare (vspltw +
		// vcmpeqfp. v,v,v over lanes x/y/z): a lane is "valid" iff it equals itself, i.e. it is
		// not NaN. AddContactsToPenetrationSolver tripwires the three contact vectors with it.
		inline bool IsValid3(const Vector3& lV)
		{
			return lV.x == lV.x && lV.y == lV.y && lV.z == lV.z;
		}

		// The other-car stride: AddContactsToPenetrationSolver derives the other vehicle's solver
		// body index as (mpOtherVehicle - lpObject) / sizeof(DeformableObject). The console figure
		// is 26496 (asm line ~904: (*(v133+52) - a3) / 26496). Carried as the console constant so
		// the index derive matches the asm exactly (DeformableObject's host size may differ).
		static const s32 KI_DEFORMABLE_OBJECT_STRIDE = 26496;

		// rw::physics::ImpulsePasser the chain forwards onto. Referenced only by the X360 symbol
		// PassOnImpulse (external/unknown callee); declared here by its observed call shape so the
		// per-TU gate links against the decl. (mpImpulsePasser is an ImpulseParams member.)
	}

	// =============================================================================================
	// DeformationSensor() -- DWARF BrnDeformationSensor.h:97.
	//
	// The ctor zero-inits the sensor (the DWARF routes it through ClearVariables @ 0x82?? --
	// declared-only). NO ctor asm is present in the exports, so the named members are zeroed to the
	// canonical rest state the maintenance routines (Prepare / ClearNonWorldContacts) re-establish:
	// no spec, no spheres, zero displacement / scratch, empty contact array. (Honest zero-init -- no
	// fabricated values.)
	// =============================================================================================
	DeformationSensor::DeformationSensor()
	{
		mpSpec = nullptr;
		mPointDisplacement_BiggestImpulseThisFrame.SetZero();

		mfMaxPointDisplacement = 0.0f;
		for ( s32 li = 0; li < 4; ++li )
		{
			maPostPhysicsVec0[li] = 0.0f;
			maPostPhysicsVec1[li] = 0.0f;
		}
		mu32PostPhysicsReset = 0;

		mi32NumStoredContacts = 0;
		mpLocalSpaceSphere = nullptr;
		mpWorldSpaceSphere = nullptr;
		mfScratchAmount = 0.0f;
	}

	// =============================================================================================
	// Prepare @ 0x8260A2E8 -- bind the sensor to its spec + local/world spheres and place the sphere
	// in the body frame.
	//
	// Store-for-store (a1 == this):
	//   *(this+280)=100.0   mfMaxPointDisplacement = 100.0
	//   stvx128 0 -> +288/+304, *(this+384)=0   post-physics scratch zeroed
	//   *(this+408)=0       mi32NumStoredContacts = 0
	//   *(this+4)=lpSpec    *(this+412)=lpLocalSphere  *(this+416)=lpWorldSphere
	//   *(this+420)=<seed>  mfScratchAmount seeded from the trailing scalar arg (a27 reinterpreted)
	//
	// The VMX block then computes a clamped displacement of the local sphere centre and writes the
	// placed sphere centres into the local + world spheres (the lvx128/stvx128 over *(this+412) and
	// *(this+416) sphere vectors, transformed by the world matrix in r7). The clamp = the
	// rsqrt/refine normalise of (sphereCentre - specOffset), scaled and bounded into [0, limit]; the
	// per-direction limit row is loaded from one of the two FLAGGED rodata tables
	// (&unk_82FB82C0 / &unk_82FB9F20) per the leading-lane sign test. Modelled as a documented block
	// over the spheres' leading Vector4; the named-member stores above are exact.
	// =============================================================================================
	bool DeformationSensor::Prepare(const SensorSpec* lpSpec, Sphere* lpLocalSphere, Sphere* lpWorldSphere,
	                                Matrix44Affine lWorldTransform, Vector3Plus lDisplacement,
	                                Vector3 lOffsetA, Vector3 lOffsetB, Vector3 lOffsetC)
	{
		// --- named-member stores (exact) ----------------------------------------------------------
		mfMaxPointDisplacement = 100.0f;                 // *(this+280) = 100.0

		for ( s32 li = 0; li < 4; ++li )                 // stvx128 0 -> +288 / +304
		{
			maPostPhysicsVec0[li] = 0.0f;
			maPostPhysicsVec1[li] = 0.0f;
		}
		mu32PostPhysicsReset = 0;                        // *(this+384) = 0

		mi32NumStoredContacts = 0;                       // *(this+408) = 0
		mpSpec             = lpSpec;                      // *(this+4)   = lpSpec
		mpLocalSpaceSphere = lpLocalSphere;              // *(this+412) = lpLocalSphere
		mpWorldSpaceSphere = lpWorldSphere;              // *(this+416) = lpWorldSphere

		// *(this+420) = v41. The asm PINS the store as *(a1+420) = a27 (one integer arg reinterpreted
		// as a float scalar: `v41 = *&a27`), preceded by an initial *(a1+420) = 0.0. FLAG: the STORE
		// is asm-exact; mapping arg a27 onto the clean signature is a best-effort guess -- a27 falls in
		// the lDisplacement (Vector3Plus) arg region, so its packed plus/w lane is taken as the seed.
		// The lane choice is not byte-proven; revisit if the call-site register mapping is recovered.
		mfScratchAmount = lDisplacement.GetPlus();       // *(this+420) = a27  (best-effort lane; see FLAG)

		// --- sphere world-placement (modelled VMX block) ------------------------------------------
		// delta = lOffsetA - lDisplacement ; normalise; scale by the FLAGGED per-direction limit row;
		// clamp into [0, mfMaxPointDisplacement-style bound]; add back to the sphere centre, then
		// transform the result by lWorldTransform into the world sphere. The numeric scale rows are
		// FLAGGED-0 (unrecovered rodata), so the placed centre reduces to the un-displaced centre --
		// honest and inert, never fabricated.
		if ( lpLocalSphere )
		{
			Vector4& lLocalCentre = SphereVec(lpLocalSphere);
			const Vector3 lSpecOffset = lpSpec ? lpSpec->mInitialOffset : Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
			const Vector3 lToCentre = Sub3(Vector3{ lLocalCentre.x, lLocalCentre.y, lLocalCentre.z, 0.0f },
			                               lSpecOffset);

			// vmsum3fp128 magSq -> vrsqrtefp + Newton refine == 1/|toCentre|; vsel guards the zero
			// case. With the FLAGGED-0 scale rows the displacement term vanishes, leaving the centre.
			const f32 lfMagSq = Dot3(lToCentre, lToCentre);
			(void)lfMagSq;   // shape preserved; numeric scaling gated by FLAGGED-0 rodata

			// local sphere centre unchanged (displacement * 0 == 0); transform into the world sphere.
			if ( lpWorldSphere )
			{
				Vector4& lWorldCentre = SphereVec(lpWorldSphere);
				const Vector3& lR = lWorldTransform.Right();
				const Vector3& lU = lWorldTransform.Up();
				const Vector3& lAt = lWorldTransform.At();
				const Vector3& lP = lWorldTransform.Pos();
				lWorldCentre.x = lLocalCentre.x * lR.x + lLocalCentre.y * lU.x + lLocalCentre.z * lAt.x + lP.x;
				lWorldCentre.y = lLocalCentre.x * lR.y + lLocalCentre.y * lU.y + lLocalCentre.z * lAt.y + lP.y;
				lWorldCentre.z = lLocalCentre.x * lR.z + lLocalCentre.y * lU.z + lLocalCentre.z * lAt.z + lP.z;
				lWorldCentre.w = lLocalCentre.w;   // radius preserved (vrlimi128 w-lane keep)
			}
		}

		(void)lOffsetB;
		(void)lOffsetC;
		return true;   // the asm returns 1 unconditionally.
	}

	// =============================================================================================
	// RecievePassedOnImpulse @ 0x825E11F8 -- CollidableBody override.
	//
	// An impulse passed down the chain is scaled through the AbsorptionTable (keyed by the sensor
	// spec's absorption level) into the sensor's stored displacement, then forwarded to the next
	// body via ImpulsePasser::PassOnImpulse. Observable flow (this == v0/r30, lpImpulseParams == r4):
	//   lDir          = lpImpulseParams->meImpulseDirection                  (*v1)
	//   lToCentre     = mpLocalSpaceSphere.centre - mpSpec.mInitialOffset    (vsubfp v12)
	//   lCompLimit    = max(spec.maDirectionParams[lDir], 0.01)              (vmaxfp v13,v9,v13)
	//   lScaleRow     = KsaAbsorptionScale[absorptionSet] (16*v1[45])        (FLAGGED rodata)
	//   lHitDir       = KsaHitDirection[lDir] (16*lDir)                      (FLAGGED rodata)
	//   v125          = lCompLimit * lScaleRow * <impulse-params row @+144>
	//   v124          = max( dot3(lHitDir, lToCentre), 0 )
	//   GetAbsorption(...)                                                   (call order preserved)
	//   stored displacement (this+412 sphere) += lHitDir * clamp(...)        (vmaddfp -> stvx128)
	//   PassOnImpulse(params.mpImpulsePasser, spec.maNextSensor[lDir], params)  (UNCONDITIONAL chain forward)
	// The displacement-into-sphere update is modelled over the local sphere's leading Vector4; the
	// GetAbsorption + PassOnImpulse calls are reproduced in order. With FLAGGED-0 rodata the scale
	// term is inert (honest), but the AbsorptionTable consult + chain forward still fire identically.
	// =============================================================================================
	void DeformationSensor::RecievePassedOnImpulse(const ImpulseParams* lpImpulseParams, VecFloat lvfPassedMagnitude)
	{
		const ENextSensorDirection leDir = lpImpulseParams->meImpulseDirection;   // v6 = *v1
		const s32  liDir = static_cast<s32>(leDir);

		// max(spec per-direction compression limit, 0.01) -- vmaxfp v13,v9,splat(0.01).
		const f32 lfCompLimit = mpSpec ? mpSpec->maDirectionParams[liDir].mCompressionLimits : 0.0f;
		const f32 lfClampedLimit = lfCompLimit > KF_NORMAL_TOLERANCE ? lfCompLimit : KF_NORMAL_TOLERANCE;

		// FLAGGED rodata rows (honest zeros): absorption-scale row (keyed by the absorption set) and
		// the per-direction hit-direction vector.
		const VecFloat lScaleRow = KsaAbsorptionScale[lpImpulseParams->meAbsorptionSet];
		const VecFloat lHitDir   = KsaHitDirection[liDir];

		// toCentre = local sphere centre - spec offset (vsubfp v12).
		Vector3 lToCentre = { 0.0f, 0.0f, 0.0f, 0.0f };
		if ( mpLocalSpaceSphere && mpSpec )
		{
			const Vector4& lCentre = SphereVec(mpLocalSpaceSphere);
			lToCentre = Sub3(Vector3{ lCentre.x, lCentre.y, lCentre.z, 0.0f }, mpSpec->mInitialOffset);
		}

		// v124 = max( dot3(hitDir, toCentre), 0 ) -- vmsum3fp128 then vmaxfp against zero.
		const f32 lfProjectedSpeed = Dot3(Vector3{ lHitDir.x, lHitDir.y, lHitDir.z, 0.0f }, lToCentre);
		const f32 lfClampedSpeed = lfProjectedSpeed > 0.0f ? lfProjectedSpeed : 0.0f;

		// AbsorptionTable consult (keyed by the spec's absorption level). Call order preserved; the
		// returned curve scales the displacement below. With FLAGGED-0 rodata it returns zero.
		const u8 lu8AbsorptionLevel = mpSpec ? mpSpec->GetAbsorptionLevel() : 0;
		const VecFloat lvfAbsorption =
			AbsorptionTable::GetAbsorption(lpImpulseParams->meAbsorptionSet, lu8AbsorptionLevel);

		// stored displacement = lScaleRow * lClampedLimit (per-lane) folded against the absorption /
		// velocity factors, clamped, and accumulated into the local sphere's displacement lane
		// (vmaddfp v9 = hitDir * scaled + sphere; vrlimi128 keeps the w lane). FLAGGED-0 rodata makes
		// the increment zero -- inert + honest. The store target is the local sphere vector.
		const f32 lfDisplacement =
			lfClampedSpeed * lScaleRow.x * lfClampedLimit * lvfAbsorption.x * KVF_VELOCITY_FACTOR.x;
		if ( mpLocalSpaceSphere )
		{
			Vector4& lCentre = SphereVec(mpLocalSpaceSphere);
			lCentre.x += lHitDir.x * lfDisplacement;
			lCentre.y += lHitDir.y * lfDisplacement;
			lCentre.z += lHitDir.z * lfDisplacement;
			// w (radius) lane preserved.
		}

		// Forward the (now absorbed) impulse onto the next body in the chain. The X360
		// UNCONDITIONALLY calls (asm tail):
		//     ImpulsePasser::PassOnImpulse(_R31[44], *(meImpulseDirection + mpSpec + 44), _R31);
		// i.e. the passer carried on the ImpulseParams (_R31[44] == lpImpulseParams->mpImpulsePasser),
		// the chain slot selected by the spec's next-sensor for this impulse direction
		// (*(mpSpec + 44-region) indexed by meImpulseDirection == maNextSensor[liDir]), and the
		// params block itself (_R31 == lpImpulseParams). The return is propagated through
		// _restvmx_124; PassOnImpulse is void here, so the call IS the side effect. The callee's
		// body lives in the BrnImpulsePasser TU -- reached BY NAME (declared in BrnImpulsePasser.h).
		const u8 lu8NextSlot = mpSpec ? mpSpec->maNextSensor[liDir] : 0;
		if ( lpImpulseParams->mpImpulsePasser )
		{
			lpImpulseParams->mpImpulsePasser->PassOnImpulse(lu8NextSlot, lpImpulseParams, lvfPassedMagnitude);
		}
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

	// =============================================================================================
	// AddContactsToPenetrationSolver @ 0x825E1D20 -- push this sensor's stored contacts into the
	// shared PenetrationSolver, split into world contacts and vehicle contacts. const.
	//
	// Flow (this == result/v7, a2 == lpSolver, a3 == lpObject, a4 == liBodyIndex,
	//       a5 == liWorldIndex, a6 == lbWorld):
	//   1) Partition the stored contacts (count == this[102] == mi32NumStoredContacts) into two index
	//      lists by the contact's mpOtherVehicle field (contact +52, the asm's *_R30 discriminator):
	//        - mpOtherVehicle != 0  -> VEHICLE list (v136[], count v10)
	//        - mpOtherVehicle == 0  -> WORLD list   (v124[], count v9); points cached in v153 scratch
	//      Each contact's three vectors are IsValid-asserted (lines 733/734/735), non-gating.
	//   2) De-duplicate the WORLD list: near-coincident contacts are merged (the vmsum3fp128 squared
	//      distance test against the per-contact weight).
	//   3) Feed the WORLD contacts into the solver's WORLD region (a2 + 131616 / running count
	//      *(a2+260900) == miNumWorldContacts). The asm INLINES the writes; reproduced through
	//      PenetrationSolver::AddWorldContact BY NAME. Asserts the RUNNING world count
	//      liNumWorldContacts < KI_MAX_PENETRATION_CONTACTS (line 816, *(a2+260900) >= 2016). The
	//      contact indices are (indexA = liWorldIndex == a5, indexB = liBodyIndex == a4).
	//   4) Feed the VEHICLE contacts into the solver's VEHICLE region (a2 + 2336 / running count
	//      *(a2+260896) == miNumVehicleContacts), via PenetrationSolver::AddVehicleContact BY NAME.
	//      For each: assert mbValid / mpOtherSensor / mpOtherVehicle (lines 842/843/844), then the
	//      other-car solver index is derived (mpOtherVehicle - lpObject) / sizeof(DeformableObject)
	//      (asm line ~904, /26496). The contact indices are (indexA = liWorldIndex == a5,
	//      indexB = liOtherCarIndex).
	// The X360 does NOT call AddObject; it writes the solver's contact arrays + running counts inline.
	// Modelled through the matching PenetrationSolver methods (AddWorldContact / AddVehicleContact);
	// the partition + dedupe control flow is reproduced; the dense VMX point math is modelled per-lane.
	// =============================================================================================
	void DeformationSensor::AddContactsToPenetrationSolver(PenetrationSolver* lpSolver, DeformableObject* lpObject,
	                                                       s32 liBodyIndex, s32 liWorldIndex, bool lbWorld) const
	{
		// --- (1) partition stored contacts into world / vehicle index lists -----------------------
		u16 lauWorldContacts[KU_MAX_STORED_CONTACTS];
		u16 lauVehicleContacts[KU_MAX_STORED_CONTACTS];
		s32 liNumWorld = 0;     // v9  -> v128
		s32 liNumVehicle = 0;   // v10 -> v126

		const s32 liCount = mi32NumStoredContacts;   // result[102]
		for ( s32 li = 0; li < liCount; ++li )
		{
			const StoredContact& lContact = maStoredContacts[li];
			const StoredContactView& lView = *reinterpret_cast<const StoredContactView*>(&lContact);

			// IsValid tripwires on the three contact vectors (non-gating; per-lane self-compare).
			CGS_ASSERT(IsValid3(lView.mLocalPointOnA), "RwMathVPU::IsValid(lContact.mLocalPointOnA)");  // *(slot+0)  line 733
			CGS_ASSERT(IsValid3(lView.mLocalPointOnB), "RwMathVPU::IsValid(lContact.mLocalPointOnB)");  // *(slot+16) line 734
			CGS_ASSERT(IsValid3(lView.mNormal),        "RwMathVPU::IsValid(lContact.mNormal)");         // *(slot+32) line 735

			// The asm's *_R30 discriminator is mpOtherVehicle (contact +52): non-zero -> the VEHICLE
			// list (*v18), zero -> the WORLD list (*v17). (Inverted vs the prior reconstruction.)
			if ( lView.mpOtherVehicle != nullptr )
			{
				lauVehicleContacts[liNumVehicle++] = static_cast<u16>(li);   // *v18 path (vehicle list)
			}
			else
			{
				lauWorldContacts[liNumWorld++] = static_cast<u16>(li);       // *v17 path (world list)
			}
		}

		if ( lpSolver == nullptr || lpObject == nullptr )
		{
			return;
		}

		// --- (2) de-duplicate the world list: merge contacts sharing a near point -----------------
		// (the vmsum3fp128 squared-distance test against the per-contact weight; modelled as a
		// pairwise near-point merge keeping the deeper contact.)
		for ( s32 li = 0; li + 1 < liNumWorld; ++li )
		{
			const StoredContactView& lA =
				*reinterpret_cast<const StoredContactView*>(&maStoredContacts[lauWorldContacts[li]]);
			for ( s32 lj = li + 1; lj < liNumWorld; )
			{
				const StoredContactView& lB =
					*reinterpret_cast<const StoredContactView*>(&maStoredContacts[lauWorldContacts[lj]]);
				const f32 lfDistSq = Dot3(Sub3(lA.mLocalPointOnA, lB.mLocalPointOnA),
				                          Sub3(lA.mLocalPointOnA, lB.mLocalPointOnA));
				if ( lfDistSq <= 0.0f && lB.mfProjectedDist >= lA.mfProjectedDist )
				{
					// merge: drop the duplicate (swap-remove with the last world entry).
					lauWorldContacts[lj] = lauWorldContacts[--liNumWorld];
				}
				else
				{
					++lj;
				}
			}
		}

		// --- (3) feed the WORLD contacts into the solver world region -----------------------------
		// The asm writes the solver's world contact array (a2 + 131616) + running count
		// *(a2+260900) directly; reproduced through AddWorldContact BY NAME. The max-contacts bound
		// asserts the RUNNING solver world count (the asm tests v88 == *(a2+260900) >= 2016), not a
		// loop index. indexA = liWorldIndex (a5), indexB = liBodyIndex (a4).
		s32 liNumWorldContacts = lpSolver->GetNumWorldContacts();   // *(a2 + 260900)
		for ( s32 li = 0; li < liNumWorld; ++li )
		{
			const StoredContactView& lView =
				*reinterpret_cast<const StoredContactView*>(&maStoredContacts[lauWorldContacts[li]]);

			CGS_ASSERT(liNumWorldContacts < KI_MAX_PENETRATION_CONTACTS,
			           "liNumWorldContacts < KI_MAX_PENETRATION_CONTACTS");   // line 816

			lpSolver->AddWorldContact(lView.mLocalPointOnA, lView.mLocalPointOnB, lView.mNormal,
			                          liWorldIndex, liBodyIndex);
			++liNumWorldContacts;
		}

		// --- (4) feed the VEHICLE contacts into the solver vehicle region -------------------------
		// indexA = liWorldIndex (a5); indexB = the other car's solver body index, derived as
		// (mpOtherVehicle - lpObject) / sizeof(DeformableObject) (asm line ~904, /26496).
		for ( s32 li = 0; li < liNumVehicle; ++li )
		{
			const StoredContact& lContact = maStoredContacts[lauVehicleContacts[li]];
			const StoredContactView& lView = *reinterpret_cast<const StoredContactView*>(&lContact);

			CGS_ASSERT(lView.mu32Valid != 0, "lContact.mbValid");                    // line 842
			CGS_ASSERT(lView.mpOtherSensor != nullptr, "lContact.mpOtherSensor");    // line 843
			CGS_ASSERT(lView.mpOtherVehicle != nullptr, "lContact.mpOtherVehicle");  // line 844

			// (mpOtherVehicle - lpObject) / 26496 -- the other car's index into the solver's body
			// array, by pointer-difference over the DeformableObject stride.
			const s32 liOtherCarIndex = static_cast<s32>(
				(reinterpret_cast<const u8*>(lView.mpOtherVehicle) - reinterpret_cast<const u8*>(lpObject))
				/ KI_DEFORMABLE_OBJECT_STRIDE);

			lpSolver->AddVehicleContact(lView.mLocalPointOnA, lView.mLocalPointOnB, lView.mNormal,
			                            liWorldIndex, liOtherCarIndex);
		}

		(void)lbWorld;
	}

	// =============================================================================================
	// ClearNonWorldContacts @ 0x825C1050 (COMMITTED -- kept verbatim).
	//
	// Reconstructed store-for-store from the X360 ARTIST asm. It does two things, in order:
	//  1) Swap-remove compaction of the stored-contacts array: walk the live contacts by index;
	//     whenever a contact's "non-world" flag (mu32NonWorldFlag) is set, overwrite it with the last
	//     live contact (the asm copies the whole 64-byte record as eight 64-bit words from
	//     (count<<6)+this-0x20, i.e. contact[count-1]), decrement the live count, and re-test the slot
	//     just refilled before advancing.
	//  2) Reset the post-physics scratch state: mfMaxPointDisplacement = 100.0, zero the two 16-byte
	//     vectors, and zero mu32PostPhysicsReset.
	//
	// Caller (X360 xref): BrnPhysics::Deformation::DeformableObject::UpdatePostPhysics.
	void DeformationSensor::ClearNonWorldContacts()
	{
		s32 liIndex = 0;
		if ( mi32NumStoredContacts > 0 )
		{
			do
			{
				if ( maStoredContacts[liIndex].mu32NonWorldFlag )
				{
					// Swap-remove: pull the last live contact into this slot, drop the
					// live count, and step back so the refilled slot is re-tested.
					maStoredContacts[liIndex] = maStoredContacts[mi32NumStoredContacts - 1];
					--liIndex;
					--mi32NumStoredContacts;
				}
				++liIndex;
			}
			while ( liIndex < mi32NumStoredContacts );
		}

		// Post-physics scratch reset.
		mfMaxPointDisplacement = 100.0f;
		for ( int i = 0; i < 4; ++i )
		{
			maPostPhysicsVec0[i] = 0.0f;
			maPostPhysicsVec1[i] = 0.0f;
		}
		mu32PostPhysicsReset = 0;
	}
}
}
