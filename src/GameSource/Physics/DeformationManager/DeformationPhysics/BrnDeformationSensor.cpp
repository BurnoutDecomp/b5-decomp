#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformationSensor.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint / gxMessageFilterFlags (the ApplyLocalImpulse gate)
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnAbsorptionTable.h"     // AbsorptionTable
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPenetrationSolver.h"   // PenetrationSolver
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnCollidableBody.h"      // ImpulseParams
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnImpulsePasser.h"       // ImpulsePasser::PassOnImpulse
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"    // DeformableObject
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"                 // VehiclePhysics::GetTransform / GetAllWheelsHaveTraction (the vehicle arm's normal flattening)
#include "SharedClasses/Physics/Deformation/BrnSensorSpec.h"  // SensorSpec (mInitialOffset, maDirectionParams, GetAbsorptionLevel)
#include "rw/math/vpu/vector3_operation.h"                    // rw::math::vpu::Magnitude (Prepare's guarded |d|)

#include <cmath>   // std::pow -- ApplyLocalImpulse's vlogefp/vexptefp + minimax refinement IS powf

// [T5-sens] DIAG state, DEFINED in BrnPhysicalTrafficManager_UpdateTrafficPhysics.cpp.
// NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
namespace BrnPhysics { namespace Vehicle {
    extern s32 gT5RamFramesLeft; extern s32 gT5RamGlobalIndex; extern s32 gT5ApplyOwner; extern s32 gT5ApplyGlobal;
} }
                   // (the image's coefficient rows are the log2 / 2^-x series; see its banner)
#include <cstdlib> // getenv -- the opt-in [impulse] bring-up probe only

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
//   * StoredContact is NOT one of them -- it is a real host-native struct in the header and every
//     field here is reached BY NAME. Its console offsets (+0/+16/+32 points, +48 mfProjectedDist,
//     +52 mpOtherVehicle, +56 mpOtherSensor, +60 mbValid, stride 0x40) are documented on the struct
//     as asm-reading aids only. See the layout pin below.
//   * PotentialContact (CgsSceneManager) is forward-declared; ValidateAndAddContact reads its two
//     contact points + normal from its leading Vector4 lanes (offsets +0 / +16 / +32) through a view.
//
// RODATA STATUS (updated 2026-08-24, deform-land wave): the impulse-direction table reads
// KA_IMPULSE_DIRECTIONS (BrnCollidableBody.cpp, recovered walls leg 4); the compression-limit
// factor table (&unk_82FB9560) and the two Prepare hit-direction rows (KV_POS_X/NEG_X) carry
// their recovered static-init values below; KVF_60_HTZ is real (60.0). The old
// KsaHitDirection / KVF_VELOCITY_FACTOR placeholders are DELETED (the latter was an invention
// of the pre-2026-08-24 RecievePassedOnImpulse paraphrase).
//
// ASSERTS are non-gating tripwires (BeginAssert/FireAssert/EndAssert == one CGS_ASSERT): in the asm
// execution continues past a failed assert, so the C++ falls through identically.

namespace BrnPhysics
{
namespace Deformation
{
	namespace
	{
		// LAYOUT PIN -- StoredContact is HOST-NATIVE (80 B, static_assert-pinned in the header) and
		// every reader/writer reaches its members BY NAME. Do NOT reintroduce the old
		// `StoredContactView` reinterpret_cast over a 64-byte X360 POD: the console fits
		// f32 +48 / ptr +52 / ptr +56 / bool +60 into 64 bytes only because its pointers are 4 bytes;
		// on x64 those members land at 48 / 56 / 64 / 72, so mpOtherSensor and mbValid sat PAST THE END
		// of the record and aliased maStoredContacts[i+1]'s leading points -- garbage pointer, float
		// bit-pattern in mbValid, and an access violation in the vehicle arm of the contact walk.

		// Leading Vector4 view of an opaque Sphere (centre.xyz + radius.w).
		inline Vector4& SphereVec(Sphere* lpSphere)
		{
			return *reinterpret_cast<Vector4*>(lpSphere);
		}

		// FLAGGED-0 PLACEHOLDER for the per-direction hit-direction table (&unk_82FB9680). Indexed
		// 16*direction => one 16-byte vec4 per ENextSensorDirection (the DWARF KV_*_HIT_DIRECTION
		// pair). Six signed body axes. Honest zeros (NEVER fabricated); the indexing shape is exact.
		// ⚠️⚠️ 2026-08-15 (walls leg 8): THIS TABLE IS ALREADY REAL IN THE TREE UNDER ANOTHER NAME.
		// ⭐ RETIRED 2026-08-24 (deform-land wave, P3): the zeroed KsaHitDirection duplicate is
		// GONE -- RecievePassedOnImpulse reads the recovered KA_IMPULSE_DIRECTIONS table
		// (unk_82FB9680, BrnCollidableBody.cpp) directly, exactly as the X360 does. The "other
		// factor KVF_VELOCITY_FACTOR" that the old note said must land with it turned out to be
		// an INVENTION of the previous paraphrase: the asm's factors are the params' own
		// mvfInverseInertia (+0x60) and mvfTimeStep (+0x70). Both placeholders are deleted.

		// FLAGGED-0 PLACEHOLDERS for the two rows DeformationSensor::Prepare selects between on the
		// sign of lDamagePoint.x (X360 &unk_82FB82C0 / &unk_82FB9F20; the PS3 relocations NAME them
		// KV_POS_X_HIT_DIRECTION / KV_NEG_X_HIT_DIRECTION). Same family as KsaHitDirection above and
		// zero for the same reason: all three are DYNAMIC-INIT globals, so the image bytes are zero
		// (x360rd.py reads 0.0 across +/-0x20 of both addresses) and only their initialisers carry
		// the values. Prepare multiplies them into the DAMAGE displacement, whose other factor
		// (lDamageScale) is itself the initial-damage scalar -- zero at a fresh spawn -- so a zero
		// here is inert on the live path AND matches what the shipped console computes there. It is
		// NOT inert after a crash reset with damage; retire it together with KsaHitDirection.
		// ⚠️ 0 is safe here only because the term is a MULTIPLICAND inside a clamp that straddles
		// zero (GetInitialCompressionScalesAndLimits emits lPosLimits >= 0 >= lNegLimits). The BASE
		// this displacement is added to is spec->mInitialOffset and is NOT flagged -- dropping that
		// was walls leg 11's bug.
		// ⭐ RECOVERED 2026-08-24 (deform-land wave, headless idat static-init decode):
		//   unk_82FB82C0 <- writer 0x82C5DAD0: { flt_8200D5FC = -0.7, flt_82001CC0 = 0,
		//                                        flt_820037C8 = -1.0, 0 }
		//   unk_82FB9F20 <- writer 0x82C5DB10: { flt_82004C68 = +0.7, 0, -1.0, 0 }
		// i.e. the preset-damage "hit" comes from ahead-and-inward on the struck side, angled
		// down (-1 z lane in the sensor's damage frame). The old zeros muted the whole preset
		// (junkyard 0.85) damage displacement -- the term multiplies lDamageScale, which is
		// NON-zero on the unlock/junkyard spawn path landed this wave.
		static const VecFloat KV_POS_X_HIT_DIRECTION = { -0.7f, 0.0f, -1.0f, 0.0f };   // &unk_82FB82C0 (init 0x82C5DAD0)
		static const VecFloat KV_NEG_X_HIT_DIRECTION = { +0.7f, 0.0f, -1.0f, 0.0f };   // &unk_82FB9F20 (init 0x82C5DB10)

		// ⭐ REAL 2026-08-15 (walls leg 8). &unk_82FB9560, indexed 16*absorptionSet -- the PS3 exports
		// name it `AbsorptionTable::savfCompressionLimitFactor` (that is what it scales: the
		// per-direction compression limit, NOT the absorption). It reads zero in both images because
		// it is a DYNAMIC-INIT global; its values live only in its initialiser, which is branch-free
		// and was executed off the image: 0x82C5DF40..0x82C5DFD4 stores five splatted rows from
		// flt_82001C98 / flt_82001C98 / flt_820945DC / flt_820092CC / flt_82001C98.
		// ⚠️⚠️ THE OLD ZERO WAS NOT INERT -- this term is a MULTIPLIER, so its identity element is
		// 1.0, not 0.0. With zeros the compression limit was identically zero and every sensor had
		// zero room to deform, for every absorption set. (Third instance of this shape this week,
		// after the AbsorptionTable rows and the oversteer grip lerp.)
		// ⭐ The recovered values line up with the enum they index: the player's extreme crash is
		// allowed the MOST compression (1.5) and shutdown next (1.25).
		static const VecFloat KsaAbsorptionScale[E_ABSORPTIONSETS_NUM] =
		{
			{ 1.0f,  1.0f,  1.0f,  1.0f  },   // E_ABSORPTIONSET_NORMAL               flt_82001C98 = 1.0
			{ 1.0f,  1.0f,  1.0f,  1.0f  },   // E_ABSORPTIONSET_AI_CRASHING          flt_82001C98 = 1.0
			{ 1.5f,  1.5f,  1.5f,  1.5f  },   // E_ABSORPTIONSET_PLAYER_EXTREME_CRASH flt_820945DC = 1.5
			{ 1.25f, 1.25f, 1.25f, 1.25f },   // E_ABSORPTIONSET_SHUTDOWN             flt_820092CC = 1.25
			{ 1.0f,  1.0f,  1.0f,  1.0f  },   // E_ABSORPTIONSET_INVINCIBLE           flt_82001C98 = 1.0
		};

		// DWARF BrnDeformationSensor.cpp:198/264 -- the two file-static VecFloats.
		// ⭐ KVF_60_HTZ REAL 2026-08-15 (walls leg 8): &unk_82FB7F60, likewise dynamic-init. Its
		// initialiser @0x82C5DB58..0x82C5DB7C loads flt_82092BC4 (== 60.0), splats it (vspltw v0,v0,0)
		// and stores it -- so the constant is exactly the 60 Hz console frame rate its name claims.
		// ApplyLocalImpulse uses it as powf's EXPONENT scale (60 * mvfTimeStep), which is what makes
		// the absorption frame-rate independent: at 60 Hz the exponent is 1 and the absorption factor
		// is exactly the table value. ⭐ That matters here specifically because this build is
		// deliberately DECOUPLED from the console's 60 fps lock.
		static const VecFloat KVF_60_HTZ        = { 60.0f, 60.0f, 60.0f, 60.0f };
		// ⭐ KVF_VELOCITY_FACTOR DELETED 2026-08-24 (deform-land wave, P3): it was an invention
		// of the old RecievePassedOnImpulse paraphrase, not an unrecovered constant -- the asm's
		// factors at that site are the params' own mvfInverseInertia and mvfTimeStep.

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
		// is 26496 (asm line ~904: (*(v133+52) - a3) / 26496).
		// ⛔ DO NOT USE IT IN HOST POINTER ARITHMETIC. It is the X360 sizeof(DeformableObject);
		// the host array is allocated with the HOST sizeof (BrnDeformationManager.cpp:113), so
		// dividing a host byte difference by this yields a bogus index. The derive at :~940 is a
		// typed pointer difference. Kept only as the documented
		// console figure, e.g. for reading the asm's `mulli 26496` seats.
		static const s32 KI_DEFORMABLE_OBJECT_STRIDE = 26496;   // reference only -- see above

		// rw::physics::ImpulsePasser the chain forwards onto. Referenced only by the X360 symbol
		// PassOnImpulse (external/unknown callee); declared here by its observed call shape so the
		// per-TU gate links against the decl. (mpImpulsePasser is an ImpulseParams member.)
	}

	// =============================================================================================
	// DeformationSensor() -- DWARF BrnDeformationSensor.h:97.
	//
	// The ctor zero-inits the sensor (the DWARF routes it through ClearVariables @ 0x82?? --
	// declared-only). NO ctor asm is present in the X360 exports, but the PS3 twin SHIPS the
	// ClearVariables body (@0x6B5F28) and it is the authoritative store list: mfMaxPointDisplacement
	// = 100.0 (NOT 0 -- `lfs f0, dword_100A62C(r2); stfs f0, 0x118(this)`; the same rest seed Prepare
	// re-writes), the two post-physics vectors + reset flag zeroed, count/spec/sphere-pointers/
	// scratch zeroed.
	// ClearVariables does NOT touch mPointDisplacement_BiggestImpulseThisFrame or the contact
	// records; the zero here is the ctor's own baseline (kept).
	// =============================================================================================
	DeformationSensor::DeformationSensor()
	{
		mpSpec = nullptr;
		mPointDisplacement_BiggestImpulseThisFrame.SetZero();

		mImpulseContact.mfImpactTimeInFrame = 100.0f;   // PS3 ClearVariables @0x6B5F28: *(this+0x118) = 100.0 (disarm; walls leg 4 rename)
		for ( s32 li = 0; li < 4; ++li )
		{
			maPostPhysicsVec0[li] = 0.0f;
			maPostPhysicsVec1[li] = 0.0f;
		}
		mSpyContactId = 0;   // spy reset (walls leg 4 rename)

		mi32NumStoredContacts = 0;
		mpLocalSpaceSphere = nullptr;
		mpWorldSpaceSphere = nullptr;
		mfScratchAmount = 0.0f;
	}

	// =============================================================================================
	// GetImpulse -- ⭐ 2026-08-14 (walls leg 4). PS3 @0x6B4ED4 (25 insns) is the whole body; the
	// X360 build inlines it into UpdateContacts' per-sensor read. Byte-exact flow:
	//   lfs f0, 0x118(this)               -- mImpulseContact.mfImpactTimeInFrame
	//   fcmpu vs 1.0 (dword_100A560); if (time > 1.0) return false   -- the 100.0 DISARM sentinel
	//   else copy the four 16-byte rows +0xE0/+0xF0/+0x100/+0x110 out -> the full 64-byte record
	// (the host copy is the widened struct assignment; same fields, host pointer widths).
	// =============================================================================================
	bool DeformationSensor::GetImpulse(StoredImpulseContact& lrOutContact)
	{
		if ( mImpulseContact.mfImpactTimeInFrame > 1.0f )
		{
			return false;
		}
		lrOutContact = mImpulseContact;
		return true;
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
	// ⭐⭐⭐ 2026-08-16 (walls leg 11) -- THE SPHERE BLOCK WAS A NO-OP AND THE SPHERE IS THE SENSOR.
	// The old model read the local sphere's EXISTING centre, subtracted the spec offset, discarded
	// the result and wrote the centre back unchanged; it never wrote either RADIUS. Since
	// ResetSensors is the only writer of DeformableObject::maLocalSensorSpheres[24], every one of
	// the car's 20 body sensors kept the pool's zero: all 20 spheres sat at the car origin with
	// radius 0. Measured live before the fix (BRN_SPHERE_PROBE=1, one boot):
	//   [sphere] 0  local 0 0 0 r 0 | world 2987.006104 -3.256206 -2011.382202 r 0 | SPEC off 0.342648 -0.261114 0.015267 r 0.375660
	//   [sphere] 19 local 0 0 0 r 0 | world 2987.006104 -3.256206 -2011.382202 r 0 | SPEC off 0.602071 -0.571243 -1.324382 r 0.437122
	// -- twenty identical world spheres, twenty distinct specs sitting unread. That is why one wall
	// contact reached all twenty sensors with byte-identical points (walls leg 10's open defect):
	// twenty COINCIDENT spheres legitimately generate twenty identical contacts.
	//
	// THE CONSOLE BLOCK, byte-witnessed on both (the PS3 prototype NAMES every argument, so no
	// operand-order judgement is left: Prepare(spec, lpLocalSphere, lpWorldSphere,
	// lVehicleTransform, lDamageScale, lPosLimits, lNegLimits, lDamagePoint)):
	//   X360 0x8260A35C lvx128 v9, r0, r4   | PS3 0x6C1A68 lvx v5, 0, spec    -- spec->mInitialOffset (+0)
	//   X360 0x8260A360 vsubfp v13, v9, v4  | PS3 0x6C1A74 vsubfp v0, v5, v31 -- d = offset - lDamagePoint
	//        (rsqrt + 2 Newton refines + the vsel zero-guard == |d|)
	//   X360 0x8260A3A4..3B0                | PS3 0x6C1AE8..AF4               -- w = clamp(1 - |d|*0.5, 0, 1)
	//   X360 0x8260A3C8/3F4 sign test on lDamagePoint.x picks &unk_82FB82C0 / &unk_82FB9F20; the PS3
	//        relocations NAME them KV_POS_X_HIT_DIRECTION / KV_NEG_X_HIT_DIRECTION, and pick
	//        maDirectionParams[1] (spec+0x14) / [0] (spec+0x10) with it; both then read [5] (+0x24).
	//   X360 0x8260A420/438/44C vrlimi 8/4/2 | PS3 the three vperm passes -- limit = (xTerm, 0, zTerm)
	//   X360 0x8260A458 vmulfp v12,v12,v1(lDamageScale) ; 0x45C * w
	//   X360 0x8260A460 vmaxfp v13,v13,v3(lNegLimits) ; 0x464 vminfp v13,v13,v2(lPosLimits)
	//   X360 0x8260A468 vaddfp v13, v9, v13 | PS3 0x6C1BA4 vaddfp v0, v5, v0  -- CENTRE = offset + disp
	//   X360 0x8260A470 stvx128 -> lpLocalSphere (w kept, then overwritten below)
	//   X360 0x8260A478/0x8260A498 lfs f0, 0x28(spec) -> BOTH spheres' w == spec->mfRadius
	//   X360 0x8260A4B8..4F0 the row cascade -- world sphere centre = transform * local centre
	//   X360 0x8260A4F8 vrlimi128 v13, v0(zero), 1, 0 | PS3 0x6C1C5C vperm<0,1,2,7> -- (this+0x10).w = 0
	// `BrnSensorSpec.h` attests mInitialOffset @console+0 and mfRadius @console+40 (0x28), so both
	// loads land on those two members by NAME, not by offset arithmetic.
	// ⭐ The two hit-direction rows are RECOVERED as of 2026-08-24 (deform-land wave, P2):
	// KV_POS_X_HIT_DIRECTION = {-0.7, 0, -1, 0} / KV_NEG_X_HIT_DIRECTION = {+0.7, 0, -1, 0}
	// (static-init writers 0x82C5DAD0 / 0x82C5DB10). On a PRESET-damage spawn (junkyard 0.85)
	// the damage displacement is now real; at a fresh zero-damage spawn the initial-damage
	// scalar still zeroes it, exactly as the shipped console computes.
	// ⛔ The BASE is a different matter and was the bug: `existing + 0` is not `mInitialOffset + 0`.
	// =============================================================================================
	bool DeformationSensor::Prepare(const SensorSpec* lpSpec, Sphere* lpLocalSphere, Sphere* lpWorldSphere,
	                                Matrix44Affine lWorldTransform, Vector3Plus lDisplacement,
	                                Vector3 lOffsetA, Vector3 lOffsetB, Vector3 lOffsetC)
	{
		// --- named-member stores (exact) ----------------------------------------------------------
		mImpulseContact.mfImpactTimeInFrame = 100.0f;    // *(this+280) = 100.0 (disarm; walls leg 4 rename)

		for ( s32 li = 0; li < 4; ++li )                 // stvx128 0 -> +288 / +304
		{
			maPostPhysicsVec0[li] = 0.0f;
			maPostPhysicsVec1[li] = 0.0f;
		}
		mSpyContactId = 0;                               // *(this+384) = 0 (spy reset; walls leg 4 rename)

		mi32NumStoredContacts = 0;                       // *(this+408) = 0
		mpSpec             = lpSpec;                      // *(this+4)   = lpSpec
		mpLocalSpaceSphere = lpLocalSphere;              // *(this+412) = lpLocalSphere
		mpWorldSpaceSphere = lpWorldSphere;              // *(this+416) = lpWorldSphere

		// *(this+420) = the PLUS (w) lane of lDamageScale, preceded by an initial *(this+420) = 0.0.
		// ⭐ FLAG RETIRED 2026-08-16 (walls leg 11): the old note called the lane "a best-effort
		// guess". Both consoles spell it out -- each spills the Vector3Plus argument whole and then
		// reloads its FOURTH float:
		//   X360 0x8260A30C stvx128 v1, r0, arg_40   then 0x8260A354 lfs f0, arg_4C(r1)   (+0xC)
		//   PS3  0x6C1A48   stvx    v2, 0, arg_60    then 0x6C1A70   lwz r0, arg_6C(r1)   (+0xC)
		// and the PS3 prototype names that argument lDamageScale, whose w lane
		// GetInitialCompressionScalesAndLimits fills with the SCRATCH ratio (KV3P_*_COMPRESSION_
		// SCRATCH_RATIO, w = 0.8 / 0.75 / 0.0). The guess was right, and it is now witnessed.
		mfScratchAmount = lDisplacement.GetPlus();       // *(this+420) = lDamageScale.w

		// --- sphere placement ---------------------------------------------------------------------
		// Argument roles (PS3 prototype names, in the frozen header's argument order):
		//   lDisplacement == lDamageScale   lOffsetA == lPosLimits
		//   lOffsetB      == lNegLimits     lOffsetC == lDamagePoint
		if ( lpLocalSphere && lpSpec )
		{
			Vector4& lLocalCentre = SphereVec(lpLocalSphere);

			// d = spec->mInitialOffset - lDamagePoint, then the guarded |d| (vmsum3fp128 magSq ->
			// vrsqrtefp + 2 Newton refines -> magSq * rsqrt == |d|, vsel'd to 0 when magSq == 0).
			const Vector3& lSpecOffset = lpSpec->mInitialOffset;
			const Vector3  lToPoint    = Sub3(lSpecOffset, lOffsetC);
			const f32      lfMagSq     = Dot3(lToPoint, lToPoint);
			const f32      lfDistance  = ( lfMagSq != 0.0f ) ? rw::math::vpu::Magnitude(lToPoint) : 0.0f;

			// The falloff weight around the damage point: clamp(1 - |d| * 0.5, 0, 1). The two
			// constants are vcfsx(1,0) == 1.0 and vcfsx(1,1) == 0.5, not rodata.
			f32 lfWeight = 1.0f - lfDistance * 0.5f;
			lfWeight = ( lfWeight > 0.0f ) ? lfWeight : 0.0f;     // vmaxfp v13, v0, v13
			lfWeight = ( lfWeight < 1.0f ) ? lfWeight : 1.0f;     // vminfp v13, v12, v13

			// The per-direction limit row: the sign of lDamagePoint.x picks BOTH the hit-direction
			// row and which of the two lateral compression limits it scales. y is left at zero by
			// the console's own vrlimi/vperm assembly; z always uses maDirectionParams[5].
			const bool lbPosX = ( lOffsetC.x > 0.0f );            // vspltw + vcmpgtfp. on lane 0
			const VecFloat& lrHitDir = lbPosX ? KV_POS_X_HIT_DIRECTION : KV_NEG_X_HIT_DIRECTION;
			const f32 lfDirX = lbPosX ? lpSpec->maDirectionParams[1].mCompressionLimits    // spec+0x14
			                          : lpSpec->maDirectionParams[0].mCompressionLimits;   // spec+0x10
			const f32 lfDirZ = lpSpec->maDirectionParams[5].mCompressionLimits;            // spec+0x24

			const Vector3 lLimit{ lrHitDir.x * lfDirX, 0.0f, lrHitDir.z * lfDirZ, 0.0f };

			// displacement = clamp(limit * lDamageScale * weight, lNegLimits, lPosLimits).
			Vector3 lDisp{ lLimit.x * lDisplacement.x * lfWeight,
			               lLimit.y * lDisplacement.y * lfWeight,
			               lLimit.z * lDisplacement.z * lfWeight, 0.0f };
			lDisp.x = ( lDisp.x > lOffsetB.x ) ? lDisp.x : lOffsetB.x;   // vmaxfp vs lNegLimits
			lDisp.y = ( lDisp.y > lOffsetB.y ) ? lDisp.y : lOffsetB.y;
			lDisp.z = ( lDisp.z > lOffsetB.z ) ? lDisp.z : lOffsetB.z;
			lDisp.x = ( lDisp.x < lOffsetA.x ) ? lDisp.x : lOffsetA.x;   // vminfp vs lPosLimits
			lDisp.y = ( lDisp.y < lOffsetA.y ) ? lDisp.y : lOffsetA.y;
			lDisp.z = ( lDisp.z < lOffsetA.z ) ? lDisp.z : lOffsetA.z;

			// ⭐⭐⭐ GATE RETIRED, walls leg 12 (2026-08-16). Leg 11 landed this arithmetic behind a
			// `gbPlaceSensorSpheres = false` because switching it on parked the car on its belly:
			// the sensor spheres came out 2*COM == 0.807 m below the wheels, hovering the car 0.764 m
			// with a per-frame correction that exactly cancelled gravity. That was NOT a defect in
			// this function -- it was ONE MISSING `vxor` in DeformationManager::ProcessAddDeformation
			// ModelEvents, which must pass TransformToNewCOMSpace the NEGATED centre of mass
			// (X360 0x82644B00, PS3 0x76ADFC; the full argument is written out at that call site).
			// With that sign restored the sensors and the wheels are both rebased by -COM, the
			// authored frame survives intact, and the two `if ( gbPlaceSensorSpheres )` guards are
			// deleted -- exactly the "two `if`s" leg 11's RETIRE-WHEN promised, and nothing else.
			// The placed body-local sphere: centre = mInitialOffset + displacement, radius from the
			// spec (the two `lfs f0, 0x28(spec)` w-lane inserts, one per sphere).
			lLocalCentre.x = lSpecOffset.x + lDisp.x;
			lLocalCentre.y = lSpecOffset.y + lDisp.y;
			lLocalCentre.z = lSpecOffset.z + lDisp.z;
			lLocalCentre.w = lpSpec->mfRadius;

			if ( lpWorldSphere )
			{
				Vector4& lWorldCentre = SphereVec(lpWorldSphere);
				lWorldCentre.w = lpSpec->mfRadius;

				const Vector3& lR = lWorldTransform.Right();
				const Vector3& lU = lWorldTransform.Up();
				const Vector3& lAt = lWorldTransform.At();
				const Vector3& lP = lWorldTransform.Pos();
				lWorldCentre.x = lLocalCentre.x * lR.x + lLocalCentre.y * lU.x + lLocalCentre.z * lAt.x + lP.x;
				lWorldCentre.y = lLocalCentre.x * lR.y + lLocalCentre.y * lU.y + lLocalCentre.z * lAt.y + lP.y;
				lWorldCentre.z = lLocalCentre.x * lR.z + lLocalCentre.y * lU.z + lLocalCentre.z * lAt.z + lP.z;
				// w untouched by the row cascade (vrlimi128 v13, v8, 1, 0 keeps the radius above).
			}
		}

		// The tail store both consoles end on: the biggest-impulse displacement's w lane is zeroed
		// (X360 0x8260A4F8 vrlimi128 v13, v0, 1, 0 -> stvx128 to this+0x10; PS3 0x6C1C5C
		// vperm<0,1,2,7> against the zero register -> stvx to this+0x10).
		mPointDisplacement_BiggestImpulseThisFrame.w = 0.0f;

		return true;   // the asm returns 1 unconditionally.
	}

	// =============================================================================================
	// RecievePassedOnImpulse @ 0x825E11F8 -- CollidableBody override.
	//
	// An impulse passed down the chain deposits displacement into this sensor -- clamped to the
	// sensor's REMAINING per-direction compression room -- then forwards the (unreduced) magnitude
	// to the next body via ImpulsePasser::PassOnImpulse. See the 1:1 note below for the field map;
	// the old observable-flow sketch here described the pre-2026-08-24 paraphrase and was wrong on
	// three factors (consumed-room dot, inverse-inertia/timestep, hit-direction table).
	// =============================================================================================
	// ⭐⭐ REWRITTEN 1:1 2026-08-24 (deform-land wave, P3) from the full asm
	// 0x825E11F8..0x825E131C (headless dump, scratchpad land_asm.txt). The previous body was a
	// paraphrase that (a) projected the hit direction against `sphereCentre - initialOffset` and
	// called it a SPEED -- the asm's dot is the compression CONSUMED SO FAR along this direction,
	// used SUBTRACTIVELY against the room limit; (b) multiplied by an invented KVF_VELOCITY_FACTOR
	// where the asm uses the params' own mvfInverseInertia (+0x60) and mvfTimeStep (+0x70);
	// (c) read the FLAGGED-zero KsaHitDirection duplicate where the asm reads the RECOVERED
	// KA_IMPULSE_DIRECTIONS table (unk_82FB9680). Every factor below is a homed ImpulseParams
	// field or a recovered table row; nothing is silently zero any more.
	void DeformationSensor::RecievePassedOnImpulse(const ImpulseParams* lpImpulseParams, VecFloat lvfPassedMagnitude)
	{
		const ENextSensorDirection leDir = lpImpulseParams->meImpulseDirection;   // params +0x00
		const s32  liDir = static_cast<s32>(leDir);

		// The per-direction unit axis (lvx128 &unk_82FB9680 + 16*dir == KA_IMPULSE_DIRECTIONS[dir]).
		const Vector3& lrAxis = KA_IMPULSE_DIRECTIONS[liDir];

		// Room limit: max(spec per-direction compression limit, 0.01)      (vmaxfp vs splat 0.01)
		//           * savfCompressionLimitFactor[absorption set]           (unk_82FB9560[set])
		//           * params->mvfAllowedCompressionFactor                  (params +0x90).
		const f32 lfCompLimit = mpSpec ? mpSpec->maDirectionParams[liDir].mCompressionLimits : 0.0f;
		const f32 lfClampedLimit = lfCompLimit > KF_NORMAL_TOLERANCE ? lfCompLimit : KF_NORMAL_TOLERANCE;
		const f32 lfRoom = lfClampedLimit
		                 * KsaAbsorptionScale[lpImpulseParams->meAbsorptionSet].x
		                 * lpImpulseParams->mvfAllowedCompressionFactor.x;

		// Compression consumed so far: max( dot3(axis, currentDisplacement), 0 ) where
		// currentDisplacement = local sphere lead - spec rest offset (vsubfp v12; vmsum3fp; vmaxfp 0).
		f32 lfConsumed = 0.0f;
		if ( mpLocalSpaceSphere && mpSpec )
		{
			const Vector4& lCentre = SphereVec(mpLocalSpaceSphere);
			const Vector3 lDisp = Sub3(Vector3{ lCentre.x, lCentre.y, lCentre.z, 0.0f },
			                           mpSpec->mInitialOffset);
			const f32 lfDot = Dot3(Vector3{ lrAxis.x, lrAxis.y, lrAxis.z, 0.0f }, lDisp);
			lfConsumed = lfDot > 0.0f ? lfDot : 0.0f;
		}

		// AbsorptionTable consult (params set +0xB4, spec absorption level +0x33).
		const u8 lu8AbsorptionLevel = mpSpec ? mpSpec->GetAbsorptionLevel() : 0;
		const VecFloat lvfAbsorption =
			AbsorptionTable::GetAbsorption(lpImpulseParams->meAbsorptionSet, lu8AbsorptionLevel);

		// Applied displacement = magnitude
		//                      * min(params->mvfMaximumAllowedAbsorption, absorption)  (vminfp)
		//                      * params->mvfInverseInertia * params->mvfTimeStep,
		// clamped to the REMAINING room (vminfp against room - consumed) -- the remaining-room
		// clamp is what lets a dent SPREAD: once this sensor is full the amount goes to zero here
		// while the full magnitude still passes on below.
		f32 lfAbsorb = lvfAbsorption.x < lpImpulseParams->mvfMaximumAllowedAbsorption.x
		             ? lvfAbsorption.x : lpImpulseParams->mvfMaximumAllowedAbsorption.x;
		f32 lfAmount = lvfPassedMagnitude.x * lfAbsorb
		             * lpImpulseParams->mvfInverseInertia.x
		             * lpImpulseParams->mvfTimeStep.x;
		const f32 lfRemaining = lfRoom - lfConsumed;
		if ( lfAmount > lfRemaining )
		{
			lfAmount = lfRemaining;
		}

		// Accumulate: local sphere lead += axis * amount (vmaddfp; vrlimi keeps the w/radius lane).
		if ( mpLocalSpaceSphere )
		{
			Vector4& lCentre = SphereVec(mpLocalSpaceSphere);
			lCentre.x += lrAxis.x * lfAmount;
			lCentre.y += lrAxis.y * lfAmount;
			lCentre.z += lrAxis.z * lfAmount;
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
	// ValidateAndAddContact @ 0x825E1788 -- MOVED to the MOUNTED
	// slice TU BrnDeformationSensor_ValidateAndAddContact.cpp: it is the storage callee of the
	// mounted DeformationManager contact-bridge slice (ReadPotentialContact /
	// ReadPotentialVehicleWorldContact) and this TU's other bodies carry link demands of their
	// own (AbsorptionTable / ImpulsePasser / PenetrationSolver). Body verbatim there, with the
	// needed file-scope helpers duplicated (internal linkage; no ODR exposure). Fold back when
	// this TU mounts.
	// =============================================================================================
	// =============================================================================================
	// AddContactsToPenetrationSolver @ 0x825E1D20 -- push this sensor's stored contacts into the
	// shared PenetrationSolver, split into world contacts and vehicle contacts. const.
	//
	// Flow (this == result/v7, a2 == lpSolver, a3 == lpDefObjBase, a4 == liWorldObjectIndex,
	//       a5 == liParentObjectIndex, a6 == lbVehicleWheelsAllHaveTraction -- DWARF names, see the
	//       header's note on the argument names):
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
	//      contact indices are (indexA = liParentObjectIndex == a5, indexB = liWorldObjectIndex == a4).
	//   4) Feed the VEHICLE contacts into the solver's VEHICLE region (a2 + 2336 / running count
	//      *(a2+260896) == miNumVehicleContacts), via PenetrationSolver::AddVehicleContact BY NAME.
	//      For each: assert mbValid / mpOtherSensor / mpOtherVehicle (lines 842/843/844), then the
	//      other-car solver index is derived (mpOtherVehicle - lpDefObjBase) / sizeof(DeformableObject)
	//      (asm line ~904, /26496). The contact indices are (indexA = liParentObjectIndex == a5,
	//      indexB = liOtherCarIndex). The vehicle arm ALSO flattens the normal against the other
	//      car's up axis when both cars have all wheels on the ground (0x825E22C4..0x825E231C) --
	//      see the flattening block at that code.
	// The X360 does NOT call AddObject; it writes the solver's contact arrays + running counts inline.
	// Modelled through the matching PenetrationSolver methods (AddWorldContact / AddVehicleContact);
	// the partition + dedupe control flow is reproduced; the dense VMX point math is modelled per-lane.
	// =============================================================================================
	// =============================================================================================
	// ApplyLocalImpulse -- X360 **sub_825E1320** (0x825E1320..0x825E1787, 1128 bytes == 282
	// instructions). This is the CollidableBody override the sensor vtable needs (slot 0) and the
	// HEAD of the ordinary wall-contact momentum path.
	//
	// ⭐ THE CHAIN THIS FUNCTION HEADS (verified by address, walls leg 7):
	//   DeformableObject::ApplySensorImpulse's six-direction loop -> (vtable slot 0, @0x82607F5C)
	//   -> THIS -> ImpulsePasser::PassOnImpulse -> impulse-passer slot 0 == the car's own
	//   &mVehicleBody (bound in ResetDeformation @0x8263A598) -> VehicleRigidBody::
	//   RecievePassedOnImpulse @0x8260DFA0 -> (not crashing + mbWorldContact) -> VehiclePhysics::
	//   ApplyWallContactImpulse @0x825FEA18 -> ExternalPhysicsBody::AddWorldSpace{,Angular}Impulse.
	// ⚠️ ApplySensorImpulse's OTHER impulse arm (step 5, post-loop) is gated behind IsCrashing()
	// (+0x710): that is the CRASH response, not this one. Do not confuse the two.
	//
	// ================================ HOW THE BODY WAS SETTLED ===================================
	// The X360 symbol is a genuine export hole (no 0x825E1320.json; the census neighbours
	// 0x825E11F8 / 0x825E1788 both exist), so it was decoded FROM THE IMAGE, and cross-read against
	// the PS3 twin @0x74D3A0, which IS exported with full mnemonics and NAMED globals.
	// ⚠️ EARLIER WAVES DISMISSED THE PS3 TWIN FOR THE WRONG REASON: its Hex-Rays output opens
	// "local variable allocation has failed", but that is the PSEUDOCODE only -- its ### ASSEMBLY ###
	// is complete and names saaAbsorptionSets / savfCompressionLimitFactor / saDirectionSpeedModifier
	// / KVF_60_HTZ and the assert strings. Both witnesses were used; they agree step for step.
	// The 282-vs-569 instruction gap is fully explained: X360 calls the three AbsorptionTable
	// accessors out-of-line and has hardware vlogefp / vexptefp / vmsum3fp128, where the PS3 inlines
	// the accessors and spells the dot product as vsldoi/vaddfp/vspltw.
	//
	// ⭐ IDENTITY -- FOUR INDEPENDENT WITNESSES, not position:
	//   1. the .pdata function census lists it unnamed between RecievePassedOnImpulse (0x825E11F8)
	//      and ValidateAndAddContact (0x825E1788);
	//   2. it is the only function in the image calling all three AbsorptionTable accessors
	//      (GetAbsorption @0x825E1350, GetSpeedForMaxAbsorbtion @0x825E14D0,
	//      GetProportionToSpeed @0x825E14E0);
	//   3. ⭐ its own assert arms name the file: the string at 0x820928D8 is
	//      ".../Physics/DeformationManager/DeformationPhysics/BrnDeformationSensor.cpp";
	//   4. its two assert LINE numbers (319 @0x825E14B0 and 348 @0x825E16F8) match the PS3 twin's
	//      (0x13F == 319, 0x15C == 348) exactly.
	//
	// ⭐⭐ TWO IDA PRINTING TRAPS THAT INVERT THE MATHS -- settled before anything was believed:
	//   * `vmaddfp vD, vX, vY, vZ` as IDA prints it means **vD = vX*vZ + vY** (position 2 is the
	//     ADDEND -- encoding order, not the spec's vD,vA,vC,vB syntax). Altivec has NO vmulfp, so
	//     the compiler spells a plain vector multiply as vmaddfp against a zero vector held in a
	//     stack slot; on the PS3 twin the zero sits in print position 2 in 10 of 10 occurrences,
	//     and the Newton-Raphson reciprocal (vrefp; vnmsubfp; vmaddfp) only reads correctly this
	//     way. Under the other reading the whole function is multiply-by-zero. ⭐ The X360 has a
	//     real `vmulfp128`, which independently confirms it.
	//   * an `rA == 0` field prints as `r0` but means the ISA's LITERAL ZERO, not r0's contents
	//     (which here hold set*16). Proven on the X360 raw word 0x825E1390 == 7DA03C0E.
	//
	// ⭐ THE POWER LAW IS PROVEN NUMERICALLY, NOT ASSERTED. vlogefp @0x825E1598 / vexptefp
	// @0x825E15FC are estimate instructions refined by minimax rows read off the image:
	//   0x82014AD0 = { 1, -0.693147182, 0.240226462, -0.055503644 }  ==  the 2^-x series
	//                { 1, -ln2, ln^2(2)/2, -ln^3(2)/6 } to six significant figures;
	//   0x82014AC0 carries that same series' next four terms;
	//   0x82014AF0 opens 1.44268966 == 1/ln2, the log2 series' leading coefficient.
	// ⇒ the block is exp2(y*log2(x)) == powf(x, y), and is reconstructed as ONE std::pow call --
	// which is what the source said before the compiler inlined it.
	// ⭐⭐ THE EXPONENT IS `60.0f * mvfTimeStep`. At the console's 60 Hz it is exactly 1 and the
	// absorption factor is exactly the table value: the pow is PURELY the frame-rate correction.
	// That is why KVF_60_HTZ exists, and it is load-bearing for THIS build specifically, which is
	// deliberately decoupled from the 60 fps lock.
	//
	// ⭐ EVERY OFFSET THE ASM TOUCHES LANDS ON A NAME THIS TREE'S HEADERS ALREADY GAVE IT (and the
	// names came from DWARF independently of this body) -- params +0x00/+0x10/+0x40/+0x60/+0x70/
	// +0x80/+0x90/+0xA0/+0xB0/+0xB4, spec +0x00/+0x10+4*dir/+0x2C+dir/+0x33, this +0x19C.
	// ⭐ And the physics comes out DIMENSIONALLY COHERENT without being made so: `absorbed *
	// mvfInverseInertia` is a velocity and `* mvfTimeStep` is a displacement -- which is exactly
	// what is then added to a position. A wrong operand order does not produce that.
	//
	// ⭐ THE CHAIN FORWARD IS CONFIRMED BY BEHAVIOUR (it was flagged UNVERIFIED by walls leg 7,
	// being merely positional). `bl 0x825BA400` @0x825E1770 is ImpulsePasser::PassOnImpulse: that
	// callee (itself an export hole, so its body was decoded too) truncates arg1 to u8, asserts
	// index < 25 (line 156), loads mapCollidableBodies[index], asserts it non-null (line 157), and
	// calls VTABLE SLOT 1 on it -- exactly PassOnImpulse's shape, and the call site fills the same
	// three slots the PS3 twin fills before its NAMED PassOnImpulse call.
	//
	// ⭐ ARGUMENT/RETURN REGISTER, MEASURED: GetAbsorption @0x825C0E70 ends `vspltw v1,v0,0; return`
	// and PassOnImpulse reads its VecFloat argument out of v1 ⇒ on X360 v1 is both the vector
	// return and the first vector argument. That is what makes `vmulfp128 v1, v0, v11` @0x825E173C
	// the third argument -- and v11 there is `vcsxwfp128 v11, v126, 1` == 0.5f (the same instruction
	// with immediate 0 earlier yields 1.0f; the PS3 spells the pair `vcfsx ..,1` / `vcfsx ..,0`).
	// ⚠️⚠️ SO THE PASSED-ON MAGNITUDE IS **HALF THE ABSORBED IMPULSE**, not the remainder. Walls
	// leg 7 recorded "the remainder is what passes on" from the params write-back alone; the actual
	// argument is `absorbed * 0.5f`, and BOTH platforms agree on it.
	//
	// MODELLING (the established house idiom, cf. RecievePassedOnImpulse above): every scalar here
	// is a splatted VecFloat, so the per-lane SIMD is modelled as explicit scalar lane math; only
	// the hit direction and the sphere centre are genuine vectors. The null guards and the
	// direction-range early-out are PC-side tripwires, not console behaviour -- the console
	// dereferences unconditionally, and the direction cannot be out of range on the live path
	// (ApplySensorImpulse's loop is `cmpwi r11,6`), so the guard is unreachable and documents the
	// invariant. Asserts are non-gating tripwires: execution continues past a failure, as in the asm.
	// =============================================================================================
	void DeformationSensor::ApplyLocalImpulse(ImpulseParams* lpImpulseParams)
	{
		// ---- (A) the three AbsorptionTable consults, in the asm's call order -------------------
		const EAbsorptionSets leSet = lpImpulseParams->meAbsorptionSet;             // params +0xB4
		const u8 lu8AbsorptionLevel = mpSpec ? mpSpec->GetAbsorptionLevel() : 0;    // spec   +0x33

		// bl GetAbsorption @0x825E1350 -> v1, parked in v124 (vmr128 v124,v1 @0x825E13AC).
		const VecFloat lvfAbsorption = AbsorptionTable::GetAbsorption(leSet, lu8AbsorptionLevel);

		const ENextSensorDirection leDirection = lpImpulseParams->meImpulseDirection;   // params +0x00
		const s32 liDir = static_cast<s32>(leDirection);
		if ( liDir < 0 || liDir >= static_cast<s32>(E_NSD_NUM) )
			return;   // PC-side tripwire only; ApplySensorImpulse's loop is 0..5.

		// lvx128 v122, [&unk_82FB9680 + 16*dir] -- CollidableBody::GetDirectionVector INLINED by the
		// X360 compiler (the PS3 twin calls it out of line @0x74D4AC). Same table either way.
		const Vector3 lHitDir = KA_IMPULSE_DIRECTIONS[liDir];

		// ---- (B) how much compression room is left along this axis -----------------------------
		Vector3 lFromRest = { 0.0f, 0.0f, 0.0f, 0.0f };
		Vector3 lToLimit  = { 0.0f, 0.0f, 0.0f, 0.0f };
		if ( mpLocalSpaceSphere && mpSpec )
		{
			const Vector4& lCentre = SphereVec(mpLocalSpaceSphere);
			const Vector3 lCentre3 = { lCentre.x, lCentre.y, lCentre.z, 0.0f };
			lFromRest = Sub3(lCentre3, mpSpec->mInitialOffset);                       // vsubfp v9,v0,v9
			lToLimit  = Sub3(lpImpulseParams->mLimitVector, lCentre3);                // vsubfp v0,v11,v0
		}

		// max(spec per-direction compression limit, 0.01) * savfCompressionLimitFactor[set]
		//                                               * mvfAllowedCompressionFactor
		// (lvlx + vspltw @0x825E13E0/E4; vmaxfp @0x825E13F0; vmulfp128 @0x825E13F8 / @0x825E1400)
		const f32 lfSpecLimit = mpSpec ? mpSpec->maDirectionParams[liDir].mCompressionLimits : 0.0f;
		f32 lfCompressionLimit = lfSpecLimit > KF_NORMAL_TOLERANCE ? lfSpecLimit : KF_NORMAL_TOLERANCE;
		lfCompressionLimit *= KsaAbsorptionScale[leSet].x;
		lfCompressionLimit *= lpImpulseParams->mvfAllowedCompressionFactor.x;

		// vmsum3fp128 v12 (@0x825E13FC) then vmaxfp128 v12,v12,v127 (@0x825E1404): how far the
		// sphere has ALREADY travelled along this axis, floored at zero.
		const f32 lfUsed = Dot3(lHitDir, lFromRest);
		const f32 lfClampedUsed = lfUsed > 0.0f ? lfUsed : 0.0f;

		// vmsum3fp128 v0 (@0x825E13F4): how far it may still travel before hitting mLimitVector.
		const f32 lfToLimit = Dot3(lToLimit, lHitDir);

		// vsubfp v13 (@0x825E1408); vminfp v0 (@0x825E140C); vmaxfp128 v121,v0,v127 (@0x825E1410).
		const f32 lfHeadroom = lfCompressionLimit - lfClampedUsed;
		f32 lfRoom = lfHeadroom < lfToLimit ? lfHeadroom : lfToLimit;
		if ( lfRoom < 0.0f )
			lfRoom = 0.0f;

		// vcmpgefp128. v11,v7,v127 @0x825E13D8 -> the assert arm @0x825E142C, source line 319. The
		// console builds the message with the offending float appended; the static text is verbatim.
		CGS_ASSERT(lpImpulseParams->mvfVelocityAlongNormal.x >= 0.0f,
		           "Applying impulse with -ve velocity: ");

		// ---- (C) the absorption fraction, made frame-rate independent --------------------------
		// bl GetSpeedForMaxAbsorbtion(set, level, dir) @0x825E14D0 -- the accessor ITSELF applies
		// saDirectionSpeedModifier[dir] (vmulfp128 v1,v13,v0 @0x825C0FAC), so it is not applied again
		// here. bl GetProportionToSpeed(set, level) @0x825E14E0.
		const VecFloat lvfSpeedForMax = AbsorptionTable::GetSpeedForMaxAbsorbtion(leSet, lu8AbsorptionLevel, liDir);
		const VecFloat lvfProportion  = AbsorptionTable::GetProportionToSpeed(leSet, lu8AbsorptionLevel);

		// vrefp128 + two Newton-Raphson steps (@0x825E14EC..0x825E1544) converge to the reciprocal;
		// vmulfp128 @0x825E1548 then vminfp @0x825E155C clamps the ratio at 1.
		f32 lfSpeedRatio = lpImpulseParams->mvfVelocityAlongNormal.x / lvfSpeedForMax.x;
		if ( lfSpeedRatio > 1.0f )
			lfSpeedRatio = 1.0f;

		// vmulfp128 v13,v13,v124 / v13,v13,v1 then vmaddfp128 v13,v124,v3,v13 (@0x825E1560..0x825E156C),
		// where v3 == 1 - proportion (vsubfp v3,v11,v1 @0x825E1538):
		//     absorption*(1 - proportion)  +  absorption*proportion*speedRatio
		const f32 lfFraction = lvfAbsorption.x * (1.0f - lvfProportion.x)
		                     + lvfAbsorption.x * lvfProportion.x * lfSpeedRatio;

		// vminfp v9,v7,v13 @0x825E1570 -- v7 is params +0xA0.
		const f32 lfMaxAllowed = lpImpulseParams->mvfMaximumAllowedAbsorption.x;
		const f32 lfBase = lfMaxAllowed < lfFraction ? lfMaxAllowed : lfFraction;

		// the inlined powf (@0x825E14E4..0x825E16C0): base == lfBase, exponent == 60 * timeStep.
		const f32 lfExponent = KVF_60_HTZ.x * lpImpulseParams->mvfTimeStep.x;
		const f32 lfAbsorbFactor = std::pow(lfBase, lfExponent);

		// vcmpgtfp128. @0x825E16C4 and vcmpgefp128. @0x825E16DC -> the assert arm @0x825E16F0,
		// source line 348. String verbatim from 0x82096020.
		CGS_ASSERT(lfAbsorbFactor < 1.0f && lfAbsorbFactor >= 0.0f,
		           "lvfAbsorption < RwMathVPU::GetVecFloat_One() && lvfAbsorption >= RwMathVPU::GetVecFloat_Zero()");

		// ---- (D) absorb, deform, and hand the rest down the chain -------------------------------
		// vmulfp128 v0,v0,v125 @0x825E1728 -- the part of the impulse this sensor takes.
		const f32 lfAbsorbed = lpImpulseParams->mvfImpulseMagnitude.x * lfAbsorbFactor;

		// vmulfp128 v12,v0,v12 (* mvfInverseInertia) then v13,v12,v13 (* mvfTimeStep)
		// (@0x825E1738 / @0x825E1740): impulse -> velocity -> displacement this step.
		// vminfp128 v13,v13,v121 @0x825E1744 clamps it to the room computed in (B).
		f32 lfMove = lfAbsorbed * lpImpulseParams->mvfInverseInertia.x * lpImpulseParams->mvfTimeStep.x;
		if ( lfMove > lfRoom )
			lfMove = lfRoom;

		// vmaddfp128 v10,v122,v13,v10 @0x825E1748 then vrlimi128 v10,v9,1,1 @0x825E174C: move the
		// local sphere centre along the hit direction, KEEPING the w lane (the radius) untouched.
		if ( mpLocalSpaceSphere )
		{
			Vector4& lCentre = SphereVec(mpLocalSpaceSphere);
			lCentre.x += lHitDir.x * lfMove;
			lCentre.y += lHitDir.y * lfMove;
			lCentre.z += lHitDir.z * lfMove;
			// w (radius) preserved -- vrlimi128 re-inserts it from the pre-update copy.
		}

		// vsubfp v0,v13,v0 then stvx128 @0x825E175C/0x825E1760 -- what this sensor took is removed
		// from the magnitude the rest of the chain will see.
		lpImpulseParams->mvfImpulseMagnitude.x -= lfAbsorbed;
		lpImpulseParams->mvfImpulseMagnitude.y -= lfAbsorbed;
		lpImpulseParams->mvfImpulseMagnitude.z -= lfAbsorbed;
		lpImpulseParams->mvfImpulseMagnitude.w -= lfAbsorbed;

		// bl 0x825BA400 @0x825E1770 == ImpulsePasser::PassOnImpulse(spec.maNextSensor[dir], params,
		// absorbed * 0.5f). ⭐ Slot 0 of the passer's body map is the car's own VehicleRigidBody,
		// which is how an ordinary world contact reaches the momentum bank.
		const u8 lu8NextSlot = mpSpec ? mpSpec->maNextSensor[liDir] : 0;

		// ---- [impulse] PC bring-up instrument -- DELETE WHEN the wall test is banked -----------
		// OPT-IN (BRN_IMPULSE_PROBE=1) so a default run and every golden gate stay byte-identical.
		// Prints the FIRST few entries with every operand this body consumed, in ONE run -- the
		// campaign rule is to read values a probe prints directly rather than infer them across runs.
		// ⚠️ READ `mag` CAREFULLY: the probe sits AFTER the `mvfImpulseMagnitude -= lfAbsorbed`
		// above, so `mag` is the REMAINING magnitude the chain forwards, not the incoming one. The
		// incoming magnitude is `mag + absorbed`, and `absorbed / (mag + absorbed)` is `factor`.
		// (Cost of not knowing this: an `absorbed/mag` ratio of 0.25 that should have been 0.2.)
		{
			static s32 siImpulseProbe = -1;
			if ( siImpulseProbe < 0 )
			{
				const char* lpcEnv = getenv( "BRN_IMPULSE_PROBE" );
				siImpulseProbe = ( lpcEnv != 0 && lpcEnv[0] != '0' ) ? 1 : 0;
			}
			// ⚠️ TWO WINDOWS (2026-08-16, walls leg 9): the +Y floor support runs this body ~20 times
			// a frame while the car merely rests, so a flat cap is spent long before anything
			// interesting happens. Window 1 proves the body runs at all; window 2 keeps only the
			// applies along a HORIZONTAL body axis with a real magnitude -- what a wall face gives
			// and what the ground does not.
			static u32 suCalls      = 0;
			static u32 suHorizontal = 0;
			++suCalls;
			const bool lbHorizontalDir = ( liDir != 2 && liDir != 3 );
			const f32  lfProbeMag      = lpImpulseParams->mvfImpulseMagnitude.x;
			const bool lbInteresting   = ( suCalls <= 30u )
			                          || ( lbHorizontalDir && lfProbeMag > 1.0f
			                               && ++suHorizontal <= 600u );
			// [T5-sens] arm (traffic crash wave, 2026-09-02): while the [T5-ram] window is open, every
			// apply into the HIT TRAFFIC CAR's sensors prints regardless of the two windows above.
			static u32 suT5Lines = 0;
			const bool lbT5Traffic = ( BrnPhysics::Vehicle::gT5RamFramesLeft > 0
			                        && BrnPhysics::Vehicle::gT5ApplyOwner == 2
			                        && BrnPhysics::Vehicle::gT5ApplyGlobal == BrnPhysics::Vehicle::gT5RamGlobalIndex
			                        && ++suT5Lines <= 400u );
			if ( siImpulseProbe == 1 && CgsDev::Log::gpDebugPrint != 0 && ( lbInteresting || lbT5Traffic ) )
			{
				*CgsDev::Log::gpDebugPrint
					<< "[impulse] n " << static_cast<s32>(suCalls)
					<< " owner " << BrnPhysics::Vehicle::gT5ApplyOwner
					<< " global " << BrnPhysics::Vehicle::gT5ApplyGlobal
					<< " specLim " << lfSpecLimit << " compLim " << lfCompressionLimit
					<< " used " << lfClampedUsed << " toLimit " << lfToLimit
					<< " dir " << liDir << " set " << static_cast<s32>(leSet)
					<< " lvl " << static_cast<s32>(lu8AbsorptionLevel)
					<< " mag " << lpImpulseParams->mvfImpulseMagnitude.x
					<< " vAlongN " << lpImpulseParams->mvfVelocityAlongNormal.x
					<< " dt " << lpImpulseParams->mvfTimeStep.x
					<< " absn " << lvfAbsorption.x << " prop " << lvfProportion.x
					<< " spdMax " << lvfSpeedForMax.x << " ratio " << lfSpeedRatio
					<< " maxAllow " << lfMaxAllowed << " base " << lfBase
					<< " expo " << lfExponent << " factor " << lfAbsorbFactor
					<< " absorbed " << lfAbsorbed
					<< " invI " << lpImpulseParams->mvfInverseInertia.x
					<< " room " << lfRoom << " move " << lfMove
					<< " nextSlot " << static_cast<s32>(lu8NextSlot)
					<< " passer " << ( lpImpulseParams->mpImpulsePasser != 0 ? 1 : 0 )
					<< " world " << ( lpImpulseParams->mbWorldContact ? 1 : 0 )
					<< "\n";
			}
		}
		const f32 lfPassedOn = lfAbsorbed * 0.5f;

		// ⭐⭐⭐ CHAIN-FORWARD RELEASED 2026-08-16 (walls leg 9) -- and the leg-8 gate banner that stood
		// here was WRONG about why. It is worth recording the correction, because the wrong diagnosis
		// would have cost the next leg a whole wave.
		//
		// LEG 8 WROTE: "no C++ constructor ever runs over the model pool, so every embedded
		// DeformationSensor's vptr is uninitialised". ⛔ FALSE. DeformationManager::Prepare has
		// placement-newed all 28 models since commit 583acffc ("Deformation Wave 5"):
		//     for (li = KU_MAX_DEFORMATION_MODELS-1; li >= 0; --li) new (&mpaModels[li]) DeformableObject();
		// which runs every member's constructor -- the 20 by-value maDeformationSensors and the
		// by-value mVehicleBody included. Their vptrs were never in doubt.
		//
		// ⭐⭐ THE ACTUAL DEFECT was one line UPSTREAM, in the params builders: `ImpulseParams` is a
		// 192-byte POD declared as a bare local, and `mpImpulsePasser` (+0xB0) had FOUR read sites in
		// the tree and ZERO write sites. So the test below ran on UNINITIALISED STACK -- a non-null
		// garbage pointer -- and PassOnImpulse then read `mapCollidableBodies[i]` off a garbage
		// `this`. That is the 0xC0000005, and it needs no unconstructed object to explain it.
		// ⚠️ It also explains leg 8's "zeroing the map turned a garbage-AV into a null-AV": the wild
		// passer pointer aliased the model pool, which leg 8's ClearVariables change had just zeroed.
		// The clue that looked like confirmation was an artefact of the fix being measured.
		// ⇒ BrnDeformableObject.cpp now writes `lParams.mpImpulsePasser = &mImpulsePasser` in BOTH
		//   builders (X360 @0x82625118/0x82625128 and @0x826251B0/0x826251B8; PS3 @0x746F6C/0x746F98),
		//   along with the four other stores that block was dropping.
		// ⭐ The `if (mpImpulsePasser)` guard is KEPT even though the console forwards
		// unconditionally: it is now a real, meaningful test on a field that is really written, and
		// on the host it is the difference between a located defect and a wild jump if a future
		// builder ever forgets the store again.
		if ( lpImpulseParams->mpImpulsePasser )
		{
			lpImpulseParams->mpImpulsePasser->PassOnImpulse(
				lu8NextSlot, lpImpulseParams,
				Vector4{ lfPassedOn, lfPassedOn, lfPassedOn, lfPassedOn });
		}
	}

	void DeformationSensor::AddContactsToPenetrationSolver(PenetrationSolver* lpSolver,
	                                                       DeformableObject* lpDefObjBase,
	                                                       s32 liWorldObjectIndex, s32 liParentObjectIndex,
	                                                       bool lbVehicleWheelsAllHaveTraction) const
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

			// IsValid tripwires on the three contact vectors (non-gating; per-lane self-compare).
			CGS_ASSERT(IsValid3(lContact.mLocalPointOnA), "RwMathVPU::IsValid(lContact.mLocalPointOnA)");  // *(slot+0)  line 733
			CGS_ASSERT(IsValid3(lContact.mLocalPointOnB), "RwMathVPU::IsValid(lContact.mLocalPointOnB)");  // *(slot+16) line 734
			CGS_ASSERT(IsValid3(lContact.mNormal),        "RwMathVPU::IsValid(lContact.mNormal)");         // *(slot+32) line 735

			// The asm's *_R30 discriminator is mpOtherVehicle (contact +52): non-zero -> the VEHICLE
			// list (*v18), zero -> the WORLD list (*v17). (Inverted vs the prior reconstruction.)
			if ( lContact.IsVehicleContact() )
			{
				lauVehicleContacts[liNumVehicle++] = static_cast<u16>(li);   // *v18 path (vehicle list)
			}
			else
			{
				lauWorldContacts[liNumWorld++] = static_cast<u16>(li);       // *v17 path (world list)
			}
		}

		if ( lpSolver == nullptr || lpDefObjBase == nullptr )
		{
			return;
		}

		// --- (2) de-duplicate the world list: merge contacts sharing a near point -----------------
		// (the vmsum3fp128 squared-distance test against the per-contact weight; modelled as a
		// pairwise near-point merge keeping the deeper contact.)
		for ( s32 li = 0; li + 1 < liNumWorld; ++li )
		{
			const StoredContact& lA = maStoredContacts[lauWorldContacts[li]];
			for ( s32 lj = li + 1; lj < liNumWorld; )
			{
				const StoredContact& lB = maStoredContacts[lauWorldContacts[lj]];
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
		// loop index. indexA = liParentObjectIndex (a5), indexB = liWorldObjectIndex (a4).
		s32 liNumWorldContacts = lpSolver->GetNumWorldContacts();   // *(a2 + 260900)
		for ( s32 li = 0; li < liNumWorld; ++li )
		{
			const StoredContact& lContact = maStoredContacts[lauWorldContacts[li]];

			CGS_ASSERT(liNumWorldContacts < KI_MAX_PENETRATION_CONTACTS,
			           "liNumWorldContacts < KI_MAX_PENETRATION_CONTACTS");   // line 816

			// The console RE-ADDS this sensor's
			// LOCAL sphere centre to the stored point (PS3 @0x6C11A8 `vaddfp v31, v0, v29`, v29 ==
			// *(mpLocalSpaceSphere)) -- the stored record is sphere-relative (ValidateAndAddContact
			// subtracts the centre when it stores). The miss was a constant per-contact depth bias
			// (22 x ~0.31m == the measured +6.8m at-rest pop).
			Vector3 lPointA = lContact.mLocalPointOnA;
			if ( mpLocalSpaceSphere != nullptr )
			{
				const Vector4& lrC = *reinterpret_cast<const Vector4*>(mpLocalSpaceSphere);
				lPointA.x += lrC.x; lPointA.y += lrC.y; lPointA.z += lrC.z;
			}
			lpSolver->AddWorldContact(lPointA, lContact.mLocalPointOnB, lContact.mNormal,
			                          liParentObjectIndex, liWorldObjectIndex);
			++liNumWorldContacts;
		}

		// --- (4) feed the VEHICLE contacts into the solver vehicle region -------------------------
		// indexA = liParentObjectIndex (a5); indexB = the other car's solver body index, derived as
		// (mpOtherVehicle - lpDefObjBase) / sizeof(DeformableObject) (asm line ~904, /26496).
		for ( s32 li = 0; li < liNumVehicle; ++li )
		{
			const StoredContact& lContact = maStoredContacts[lauVehicleContacts[li]];

			// asm order: lbz +0x3C (mbValid), lwz +0x38 (mpOtherSensor), lwz +0x34 (mpOtherVehicle).
			CGS_ASSERT(lContact.mbValid, "lContact.mbValid");                          // line 842
			CGS_ASSERT(lContact.mpOtherSensor != nullptr, "lContact.mpOtherSensor");    // line 843
			CGS_ASSERT(lContact.mpOtherVehicle != nullptr, "lContact.mpOtherVehicle");  // line 844

			// (mpOtherVehicle - lpDefObjBase) / sizeof(DeformableObject) -- the other car's index into
			// the solver's body array.
			// ⚠ TYPED pointer difference, never the console constant KI_DEFORMABLE_OBJECT_STRIDE
			// (26496 == the X360 sizeof(DeformableObject)): the array both pointers come from is
			// allocated with the HOST sizeof (BrnDeformationManager.cpp:113), so dividing a host byte
			// difference by 26496 yields a garbage index -- an access violation in
			// PenetrationSolver::AddVehicleContact on the first car-car contact.
			const s32 liOtherCarIndex = static_cast<s32>(lContact.mpOtherVehicle - lpDefObjBase);

			// Same sphere-relative rebase as the world loop (PS3 @0x6C0D08/0x6C0D2C): pointA gets
			// THIS sensor's local centre back; pointB gets the OTHER sensor's local centre back.
			// mLocalPointOnB is sphere-relative in the OTHER car's body space -- re-adding the other
			// sensor's LOCAL centre to a WORLD point would be meaningless. Storing a raw world point
			// here made Solve() transform it a second time: the +-1667 m launch.
			Vector3 lPointA = lContact.mLocalPointOnA;
			if ( mpLocalSpaceSphere != nullptr )
			{
				const Vector4& lrC = *reinterpret_cast<const Vector4*>(mpLocalSpaceSphere);
				lPointA.x += lrC.x; lPointA.y += lrC.y; lPointA.z += lrC.z;
			}
			Vector3 lPointB = lContact.mLocalPointOnB;
			if ( lContact.mpOtherSensor != nullptr && lContact.mpOtherSensor->mpLocalSpaceSphere != nullptr )
			{
				const Vector4& lrC =
					*reinterpret_cast<const Vector4*>(lContact.mpOtherSensor->mpLocalSpaceSphere);
				lPointB.x += lrC.x; lPointB.y += lrC.y; lPointB.z += lrC.z;
			}

			// THE NORMAL FLATTENING. When BOTH cars have all four wheels on the ground the
			// console strips the OTHER car's UP component out of the contact normal, so a car-car
			// penetration pushes the pair apart HORIZONTALLY and cannot launch a grounded car
			// vertically. Asm (@0x825E22C4..0x825E231C):
			//   lwz    r11, 0x194C(mpOtherVehicle)  ; the other car's VehiclePhysics
			//   lvx128 v13, r11, 0x20               ; vehPhys+0x20 == mTransform.yAxis (UP)
			//   lbz    r11, 0x135B(r11)             ; vehPhys+0x135B == mbAllWheelsHaveTraction
			//   vmsum3fp128 v11, v0, v13            ; dot(normal, otherUp)
			//   vmulfp128   v13, v13, v11
			//   vsubfp      v0,  v0,  v13           ; flattened = normal - otherUp*dot
			//   vsel v0, normal, flattened, mask(a6 == lbVehicleWheelsAllHaveTraction)
			//   vsel v0, normal, that,      mask(other car's mbAllWheelsHaveTraction)
			// MASK POLARITY IS PROVEN, NOT ASSUMED: both selects index the shared 16-byte-pair table
			// byte_8327F240 through `cntlzw; rlwinm ..,31,27,27 (+xori 0x10)`, and the SAME table +
			// index idiom is an EQUALITY test in BrnEffects::BrnCrashTriangleCache::
			// InsertTriangleIntoCache @0x8227B2D0 (`addi r7,r9,-3; cntlzw; rlwinm` -> 0x10 when the
			// value matches, then vsel takes the NEW operand). So table[0x10] is all-ones == TRUE:
			// here index 0x10 is selected when the traction byte is NON-zero, i.e. flatten iff BOTH
			// cars are fully on their wheels. A vsel is per-lane and both masks are uniform, so the
			// host spelling is a plain scalar `if`.
			Vector3 lNormal = lContact.mNormal;
			const Vehicle::VehiclePhysics* lpOtherPhysics =
				( lContact.mpOtherVehicle != nullptr ) ? lContact.mpOtherVehicle->GetVehiclePhysics() : nullptr;
			if ( lbVehicleWheelsAllHaveTraction && lpOtherPhysics != nullptr
			     && lpOtherPhysics->GetAllWheelsHaveTraction() )
			{
				const Vector3& lrOtherUp = lpOtherPhysics->GetTransform().Up();
				const f32 lfAlongUp = Dot3(lNormal, lrOtherUp);
				lNormal.x -= lrOtherUp.x * lfAlongUp;
				lNormal.y -= lrOtherUp.y * lfAlongUp;
				lNormal.z -= lrOtherUp.z * lfAlongUp;
			}

			lpSolver->AddVehicleContact(lPointA, lPointB, lNormal,
			                            liParentObjectIndex, liOtherCarIndex);
		}
	}

	// =============================================================================================
	// ClearNonWorldContacts @ 0x825C1050.
	//
	// Reconstructed store-for-store from the X360 ARTIST asm. It does two things, in order:
	//  1) Swap-remove compaction of the stored-contacts array: walk the live contacts by index;
	//     whenever a contact is a VEHICLE contact, overwrite it with the last live contact (the asm
	//     copies the whole 64-byte record as eight 64-bit words from (count<<6)+this-0x20, i.e.
	//     contact[count-1] -- on the host that is a struct assignment of the 80-byte record),
	//     decrement the live count, and re-test the slot just refilled before advancing.
	//     ⭐ 2026-08-23: the test used to be the reconstructed alias `mu32NonWorldFlag` on an opaque
	//     record. The asm reads it at sensor+0x54 (`addi r6,r3,0x54` @0x825C1064, then +0x40 per
	//     contact) == contact[i]+0x34 == mpOtherVehicle -- the SAME word
	//     AddContactsToPenetrationSolver uses to pick its vehicle list (`lwz r11,0(r30)`, r30 ==
	//     contact+0x34). So "non-world" IS "has an other vehicle", and the alias is retired onto the
	//     real member. Same instruction, same semantics, no behaviour change on either build.
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
				if ( maStoredContacts[liIndex].IsVehicleContact() )
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
		mImpulseContact.mfImpactTimeInFrame = 100.0f;   // disarm (walls leg 4 rename)
		for ( int i = 0; i < 4; ++i )
		{
			maPostPhysicsVec0[i] = 0.0f;
			maPostPhysicsVec1[i] = 0.0f;
		}
		mSpyContactId = 0;   // spy reset (walls leg 4 rename)
	}

	// =============================================================================================
	// StoredImpulseContact::GetInverse (DWARF :68) -- 2026-08-14 (walls leg 4). The role-swapped
	// record for the equal-and-opposite car-car apply, per its own declaration gloss: point-on-A
	// <-> point-on-B, normal negated; ownership/time/id fields carry over. Dead at runtime today
	// (no car-car contacts on the junkyard path).
	// =============================================================================================
	void StoredImpulseContact::GetInverse(StoredImpulseContact& lrInverse) const
	{
		lrInverse.mPointOnA = mPointOnB;
		lrInverse.mPointOnB = mPointOnA;
		lrInverse.mNormal   = Vector3{ -mNormal.x, -mNormal.y, -mNormal.z, 0.0f };
		lrInverse.mpOtherVehicle      = mpOtherVehicle;
		lrInverse.mpOtherSensor       = mpOtherSensor;
		lrInverse.mfImpactTimeInFrame = mfImpactTimeInFrame;
		lrInverse.mContactId          = mContactId;
	}

}
}
