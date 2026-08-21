#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                        // CGS_ASSERT
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"                        // CgsGeometric::Sphere (centre.xyz + radius.w)
#include "GameShared/GameClasses/Geometric/Primitives/CgsSweptSphere.h"                   // CgsGeometric::SweptSphere
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"                      // CgsSceneManager::VolumeInstanceId
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"              // VehiclePhysics (mfSpeedMPH/IsCrashing/GetTransform/mpAttribs)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                                // gpDebugPrint / gxMessageFilterFlags (the null-attribs gate)

#include <cstdlib>                                                                        // getenv (the [sweptsel] opt-in probe)

// ============================================================================================
// GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject_BBox.cpp
//
// BrnPhysics::Deformation::DeformableObject -- the BROADPHASE + DEFORMED-BOUNDS group. Eight
// functions, each reconstructed store-for-store / branch-for-branch from BURNOUT_X360_ARTIST.XEX:
//
//   GetSweptSpheres                       @ 0x825B4230  expose maSweptSpheres[] for continuous collision
//   IsUsingSweptSpheres                   @ 0x825B42A8  gate continuous-collision on speed + not-crashing
//   GetAlignedDeformedBoundingBox         @ 0x825E86C8  oriented deformed box (centre = mid of body bbox)
//   GetBoundingBox                        @ 0x825E8620  oriented body box from spec dims + handling-body xform
//   UpdateDeformedBBox                    @ 0x825E0D20  accumulate per-sensor deformed AABB; latch "beyond
//                                                       drive-time limits in crash"
//   CalculateDriveTimeLimits              @ 0x82609DC8  sensor-spec AABB widened by the per-car drive-time
//                                                       deform limits -> mDriveTimeBBoxLimitMin/Max
//   GetDeformationSphereFromVolumeInstance@ 0x825B4428  map a scene volume-instance id -> a sensor sphere
//   GetDeformationSensorFromVolumeInstance@ 0x825B4338  map a scene volume-instance id -> a DeformationSensor
//                                                       (the dossier's truncated "GetDeform"; its asm
//                                                       returns a DeformationSensor*, so it is the
//                                                       FromVolumeInstance sensor accessor, NOT the
//                                                       AxisAlignedBox GetDeformedBoundingBox -- see FLAG)
//
// ============================ MODELLED-vs-ASM (read before editing) ============================
// The X360 build is dense VMX128 register code. The reconstructions are FAITHFUL to the OBSERVABLE
// behaviour -- same control flow, same early-outs, same branch structure, same call order, same
// strides, same named-member stores -- and model the per-lane SIMD arithmetic as explicit scalar
// lane math (the established house idiom: cf. BrnStreamedDeformationSpec.cpp::GetBoundingBox and
// BrnPhysicalBodyPart.cpp::GetBoundingBox).
//
// OPAQUE TYPES accessed by console byte offset (proven by the asm; NOT renamed):
//   * CgsGeometric::Box is forward-declared only in the frozen header. The asm's
//     CgsGeometric::Box::Set(box, Matrix44Affine, Vector3Plus) is modelled by writing the
//     oriented-box transform rows (+0/+16/+32/+48) and dims+fatness (+64) into the out-box through
//     a raw-offset layout -- identical to BrnPhysicalBodyPart::GetBoundingBox.
//     ✅ FLAG CLEARED 2026-08-19 (wave Q6, cluster `addprim`): the layout is no longer provisional
//     and the guess was RIGHT. Box now has a real home -- GameShared/GameClasses/Geometric/
//     Primitives/CgsBox.h, which is the console's OWN home (Box::Set @0x825E6918 passes the file
//     string "..\GameShared\GameClasses\Geometric/Primitives/CgsBox.h", dumped from the image) --
//     and its DWARF-authoritative members are exactly +0x00 Matrix44Affine mTransform and +0x40
//     Vector3Plus mDimensionsAndFatness. FOLLOW-UP, deliberately NOT done here: these raw-offset
//     writes can now be replaced by a by-name Box::Set call. That is a separate de-duplication, not
//     a comment fix.
//   * StreamedDeformationSpec::mu8NumDeformationSensors is private to a sibling (already-committed)
//     header that must not be edited here; the asm reads it as `*(mpDeformationSpec + 1618)`. It is
//     read through that same console offset (+1618), exactly as the asm does (the committed
//     BrnStreamedDeformationSpec.cpp pins +1618 == mu8NumDeformationSensors).
//   * The per-car drive-time deform limits the asm widens the bbox by are read off the attached
//     VehiclePhysics as `*(vehiclePhysics + 0x720)` (== mpAttribs, BY NAME via GetAttribsPtr below)
//     then the Vector4 at +0x40 within the attribs (== mBaseAttribs.mDrivetimeDeformLimits). Those
//     are LIVE per-car attrib data (NOT rodata), reached by the asm-proven console offsets.
//
// FLAGGED-0 PLACEHOLDERS (rodata NOT in the per-function exports -- NEVER fabricated):
//   * IsUsingSweptSpheres scales the body speed vector by &unk_83017FE0 before the > 6.0 test. That
//     scale vector's value is not in the exports; it is carried as a correctly-shaped honest zero
//     (KVF_SWEPT_SPHERE_SPEED_SCALE). The 6.0 threshold IS asm-visible and is the real constant. With
//     the scale FLAGGED-0 the product is 0 and the speed gate fails (inert + honest) until the rodata
//     is recovered -- the early-outs / member reads around it stay byte-faithful.
//   * UpdateDeformedBBox compares the deformed extents against the drive-time limits with a per-axis
//     tolerance vector loaded from &unk_82FB9B30. Carried FLAGGED-0 (KVF_DEFORMED_BBOX_TOLERANCE):
//     the comparison then reduces to a plain "deformed corner crossed the drive-time corner" test,
//     exactly the zero-tolerance limit of the asm.
//
// ASSERTS are non-gating tripwires (BeginAssert/FireAssert/EndAssert == one CGS_ASSERT): in the asm
// execution continues past a failed assert, so the C++ falls through identically. NO file/line.

namespace BrnPhysics
{
namespace Deformation
{
	namespace
	{
		// ---- console byte offsets (asm-proven) -------------------------------------------------
		// StreamedDeformationSpec::mu8NumDeformationSensors -- `*(mpDeformationSpec + 1618)`.
		static const u32 KU_SPEC_NUM_DEFORMATION_SENSORS_OFFSET = 1618;

		// VehiclePhysics::mpAttribs -- `*(vehiclePhysics + 0x720)`; then the Vector4 drive-time deform
		// limits at +0x40 within VehicleAttribs::mBaseAttribs (== mDrivetimeDeformLimits).
		static const u32 KU_VEHICLE_ATTRIBS_PTR_OFFSET    = 0x720;   // 1824
		static const u32 KU_DRIVETIME_DEFORM_LIMITS_OFFSET = 0x40;   // 64

		// VehiclePhysics::IsCrashing flag -- `*(vehiclePhysics + 0x710)`; mfSpeedMPH @ +0x6C0. Both are
		// reached by name below (IsCrashing()/the speed read), retained here for the asm cross-ref only.

		// DeformableObject state word UpdateDeformedBBox gates on -- `*(this + 26384)`, HIGH byte == 1.
		// No separately-named member in the frozen DWARF sequence; read by the asm-proven console offset.
		static const u32 KU_DEFORMED_BBOX_GATE_WORD_OFFSET = 26384;

		// The deformed-AABB corner pair SimpleVehiclePhysics::SetDeformableBBox(min,max) stores into the
		// attached vehicle physics: min @ vehiclePhysics+0x6D0, max @ vehiclePhysics+0x6E0. These are the
		// exact corners GetAlignedDeformedBoundingBox reads back (its +0x6D0/+0x6E0 reads). The setter was
		// INLINED in UpdateDeformedBBox's first block (asm: stack-stage the accumulated min/max, then copy
		// both Vector4s into the body), so it is modelled here as that same pair of stores by console offset.
		static const u32 KU_DEFORMABLE_BBOX_MIN_OFFSET = 0x6D0;   // 1744
		static const u32 KU_DEFORMABLE_BBOX_MAX_OFFSET = 0x6E0;   // 1760

		// KI_MAX_NUM_WHEEL_POINTS_PER_DEFORMABLE_OBJECT -- the four wheel sensor slots that follow the
		// streamed deformation sensors (the asm's `+ 4` / `< 4` bounds, DWARF asserts).
		static const s32 KI_MAX_NUM_WHEEL_POINTS_PER_DEFORMABLE_OBJECT = 4;

		// The continuous-collision minimum speed (asm-visible literal `v11[0] = 6.0`).
		static const f32 KF_MIN_SWEPT_SPHERE_SPEED = 6.0f;

		// ⭐⭐ RECOVERED 2026-08-03, and this one identifies ITSELF. unk_83017FE0 is a static-init splat
		// (@0x82C6D160) of flt_82F31928, and flt_82F31928 is 0.447039992 -- the MPH->m/s constant this
		// image uses in ~300 places. So the "speed-scale vector" is a UNIT CONVERSION: the asm splats
		// lane 0 of (mfSpeedMPH * 0.44704), i.e. the speed in m/s, and compares it against
		// KF_MIN_SWEPT_SPHERE_SPEED = 6.0 just above -- 6 m/s, not 6 MPH. The two constants only make
		// sense together, and they agree.
		static const Vector4 KVF_SWEPT_SPHERE_SPEED_SCALE = { 0.447039992f, 0.447039992f, 0.447039992f, 0.447039992f };

		// unk_82FB9B30 <- flt_82002138 = 0.01, static-init splat @0x82C5DAA0.
		static const Vector4 KVF_DEFORMED_BBOX_TOLERANCE = { 0.00999999978f, 0.00999999978f, 0.00999999978f, 0.00999999978f };

		// ⭐ RECOVERED 2026-08-03. unk_82FB95E0's initialiser @0x82C5B798 is NOT a splat -- it builds a
		// genuinely PER-AXIS row from three different .rdata scalars: flt_82004014 (0.1),
		// flt_82004740 (0.3) and flt_820047C8 (0.05), with lane w left at 0. Writing the usual splat here
		// would have shrunk all three axes by the same amount, which is exactly what this constant does
		// not do -- the DWARF name KVF_CAR_BBOX_SHRINK describes a car-shaped box, and the numbers are
		// car-shaped too (deepest shrink along the middle axis).
		static const Vector4 KVF_CAR_BBOX_SHRINK = { 0.100000001f, 0.300000012f, 0.0500000007f, 0.0f };

		// VehiclePhysics::mfSpeedMPH -- `*(vehiclePhysics + 0x6C0)`. Private in the homed VehiclePhysics
		// (no public getter, header not editable here); read off the asm-proven console offset.
		static const u32 KU_VEHICLE_SPEED_MPH_OFFSET = 0x6C0;   // 1728

		// Leading Vector4 view of an opaque Sphere (centre.xyz + radius.w) -- the same idiom the
		// committed BrnDeformationSensor.cpp uses to read a Sphere by its packed leading vector. The
		// sensor's sphere pointers are the forward-declared BrnPhysics::Deformation::Sphere (incomplete
		// here) -- the leading 16 bytes are the packed centre/radius Vector4 either way (CgsGeometric::
		// Sphere shares that layout), so the read is taken through a void* to stay type-agnostic.
		inline const Vector4& SphereVec(const void* lpSphere)
		{
			return *reinterpret_cast<const Vector4*>(lpSphere);
		}

		// Read the streamed spec's deformation-sensor count off the asm-proven console offset (+1618).
		inline u32 SpecNumSensors(const StreamedDeformationSpec* lpSpec)
		{
			return *(reinterpret_cast<const u8*>(lpSpec) + KU_SPEC_NUM_DEFORMATION_SENSORS_OFFSET);
		}

		// Read VehiclePhysics::mfSpeedMPH by console offset (+0x6C0); private member, header not editable.
		inline f32 VehicleSpeedMPH(const BrnPhysics::Vehicle::VehiclePhysics* lpPhysics)
		{
			return *reinterpret_cast<const f32*>(reinterpret_cast<const char*>(lpPhysics) + KU_VEHICLE_SPEED_MPH_OFFSET);
		}
	}

	// =============================================================================================
	// GetSweptSpheres @ 0x825B4230
	//
	// Expose this car's swept (continuous-collision) sphere array. The asm:
	//   if (!mbDoSweptSphereTests)  assert "mbDoSweptSphereTests"            (non-gating)
	//   *lppSpheresOut = this + 384;          (&maSweptSpheres[0]; maWorldSensorSpheres[24] @ +0)
	//   return *(mpDeformationSpec + 1618) + 4;   (num deformation sensors + KI_MAX_NUM_WHEEL_POINTS)
	// Returns the swept-sphere count (one per deformation sensor + four wheel points).
	// =============================================================================================
	s32 DeformableObject::GetSweptSpheres(const CgsGeometric::SweptSphere** lppSpheresOut)
	{
		CGS_ASSERT(mbDoSweptSphereTests, "mbDoSweptSphereTests");   // BrnDeformableObject.h:446 (non-gating)

		*lppSpheresOut = maSweptSpheres;   // *a2 = a1 + 384
		return static_cast<s32>(SpecNumSensors(mpDeformationSpec)) + KI_MAX_NUM_WHEEL_POINTS_PER_DEFORMABLE_OBJECT;
	}

	// =============================================================================================
	// IsUsingSweptSpheres @ 0x825B42A8
	//
	// Continuous-collision is active when the swept-sphere tests are enabled AND the car is moving
	// fast enough AND it is not currently crashing. The asm:
	//   if (!mbDoSweptSphereTests) return 0;
	//   speedScaled = (*(vehiclePhysics + 0x6C0) /*mfSpeedMPH vector*/) * &unk_83017FE0;  (lane 0)
	//   if (!(speedScaled.x > 6.0)) return 0;
	//   return !*(vehiclePhysics + 0x710);                 (== !IsCrashing())
	// FLAG: the speed-scale vector (&unk_83017FE0) is FLAGGED-0 -- the speed gate is honest-inert until
	// recovered; mbDoSweptSphereTests / IsCrashing reads + early-out structure are byte-faithful.
	// =============================================================================================
	bool DeformableObject::IsUsingSweptSpheres()
	{
		if ( !mbDoSweptSphereTests )
		{
			return false;
		}

		const BrnPhysics::Vehicle::VehiclePhysics* lpPhysics = mVehicleBody.GetVehiclePhysics();

		// (mfSpeedMPH * KVF_SWEPT_SPHERE_SPEED_SCALE).x compared against the asm-visible 6.0 floor.
		const f32 lfSpeedScaled = VehicleSpeedMPH(lpPhysics) * KVF_SWEPT_SPHERE_SPEED_SCALE.x;

		// ---- [sweptsel] PC bring-up instrument -- DELETE WITH THE REST OF THE WALL PROBES ----
		// OPT-IN (BRN_WALL_PROBE=1), pure read, no behaviour change. It exists to retire ONE
		// standing inference: twelve legs of this campaign used a control that REVERSED into a
		// wall at 31 m/s and collided, and the explanation offered was "mfSpeedMPH is forward-
		// signed, so reverse never trips this test". That was reasoned, never read. This prints
		// the raw field so the sign is witnessed instead.
		{
			static s32 siSweptSelProbe = -1;
			if ( siSweptSelProbe < 0 )
			{
				const char* lpcEnv = getenv( "BRN_WALL_PROBE" );
				siSweptSelProbe = ( lpcEnv != nullptr && lpcEnv[0] != '0' ) ? 1 : 0;
			}
			static u32 suSweptSelCalls = 0u;
			++suSweptSelCalls;
			if ( siSweptSelProbe == 1 && CgsDev::Log::gpDebugPrint != nullptr
			     && ( suSweptSelCalls % 300u ) == 0u )
			{
				*CgsDev::Log::gpDebugPrint
					<< "[sweptsel] mfSpeedMPH " << VehicleSpeedMPH( lpPhysics )
					<< " -> m/s " << lfSpeedScaled
					<< " floor " << KF_MIN_SWEPT_SPHERE_SPEED
					<< ( ( lfSpeedScaled > KF_MIN_SWEPT_SPHERE_SPEED ) ? " SWEPT" : " INPLACE" )
					<< "\n";
			}
		}
		// ---- end [sweptsel] ------------------------------------------------------------------

		if ( !(lfSpeedScaled > KF_MIN_SWEPT_SPHERE_SPEED) )
		{
			return false;
		}

		// return !IsCrashing()  (`*(vehiclePhysics + 0x710)`).
		return !lpPhysics->IsCrashing();
	}

	// =============================================================================================
	// GetBoundingBox @ 0x825E8620
	//
	// Write the car's oriented body bounding box. The asm assembles a CgsGeometric::Box from:
	//   * orientation = the handling body transform rows (mVehicleBody.GetVehiclePhysics()->mTransform
	//     rows @ vehiclePhysics+0x10/+0x20/+0x30, i.e. right/up/at),
	//   * centre      = vehiclePhysics.pos (transform row3 @ +0x40) + *(mpDeformationSpec + 1632)
	//                   (mCurrentCOMOffset; the asm `lvx128 v0, mpDeformationSpec, 1632`),
	//   * dims+fatness = *(mpDeformationSpec + 64) (mHandlingBodyDimensions), fatness lane = 0,
	// then CgsGeometric::Box::Set(box, {right,up,at,centre}, dimsAndFatness). Box is forward-declared
	// only, so the Set is modelled as a raw-offset write (transform @ +0/+16/+32/+48, dims @ +64),
	// matching BrnPhysicalBodyPart::GetBoundingBox. ✅ That layout is CONFIRMED, not provisional --
	// see the file banner: CgsGeometric::Box now lives at GameShared/GameClasses/Geometric/
	// Primitives/CgsBox.h with mTransform @ +0x00 and mDimensionsAndFatness @ +0x40. Still open here:
	// mpDeformationSpec vectors at console +64 / +1632 read by offset (sibling spec's private
	// members not editable here).
	// =============================================================================================
	void DeformableObject::GetBoundingBox(CgsGeometric::Box* lpBoxOut)
	{
		const BrnPhysics::Vehicle::VehiclePhysics* lpPhysics = mVehicleBody.GetVehiclePhysics();
		const Matrix44Affine& lTransform = lpPhysics->GetTransform();

		const char* lpSpec = reinterpret_cast<const char*>(mpDeformationSpec);
		const Vector3& lvSpecDims    = *reinterpret_cast<const Vector3*>(lpSpec + 64);     // mHandlingBodyDimensions
		const Vector3& lvSpecCentreOff = *reinterpret_cast<const Vector3*>(lpSpec + 1632); // mCurrentCOMOffset

		// centre = transform.pos + spec rigid-body offset (vaddfp v0 = v13 + v0).
		const Vector3 lvCentre = {
			lTransform.wAxis.x + lvSpecCentreOff.x,
			lTransform.wAxis.y + lvSpecCentreOff.y,
			lTransform.wAxis.z + lvSpecCentreOff.z,
			0.0f
		};

		// dims+fatness packed (the fatness/w lane is seeded 0.0 by the asm's v27[0]=0.0 + memset tail).
		const Vector3 lvDimsAndFatness = { lvSpecDims.x, lvSpecDims.y, lvSpecDims.z, 0.0f };

		// CgsGeometric::Box::Set(box, {right=transform.right, up, at, centre}, dimsAndFatness).
		char* lpBox = reinterpret_cast<char*>(lpBoxOut);
		*reinterpret_cast<Vector3*>(lpBox +  0) = lTransform.xAxis;   // right
		*reinterpret_cast<Vector3*>(lpBox + 16) = lTransform.yAxis;   // up
		*reinterpret_cast<Vector3*>(lpBox + 32) = lTransform.zAxis;   // at
		*reinterpret_cast<Vector3*>(lpBox + 48) = lvCentre;          // centre
		*reinterpret_cast<Vector3*>(lpBox + 64) = lvDimsAndFatness;  // dims (+ fatness lane)
	}

	// =============================================================================================
	// GetAlignedDeformedBoundingBox @ 0x825E86C8
	//
	// Write the car's deformed bounding box as a Box oriented to the handling-body basis. The asm reads
	// the deformed AABB the vehicle physics maintains (SetDeformableBBox stores it at vehiclePhysics
	// +0x6D0 / +0x6E0) and the handling-body transform rows, then:
	//   min = *(vehiclePhysics + 1744 /*0x6D0*/) ; max = *(vehiclePhysics + 1760 /*0x6E0*/)
	//   halfDims = (max - min) * 0.5  -  KVF_CAR_BBOX_SHRINK   (vmulfp128 by 0.5; vsubfp by &unk_82FB95E0)
	//   centreBody = (min + max) * 0.5
	//   centreWorld = transform.right*cx + transform.up*cy + transform.at*cz + transform.pos
	//   CgsGeometric::Box::Set(box, {right, up, at, centreWorld}, {halfDims, fatness = 0})
	//
	// FLAG: KVF_CAR_BBOX_SHRINK (&unk_82FB95E0) is unrecovered rodata -> FLAGGED-0 (the shrink term
	// vanishes; honest). The deformed AABB corner pair is read off the asm-proven vehiclePhysics console
	// offsets (+0x6D0 / +0x6E0; not a named VehiclePhysics member here). Box::Set modelled as the
	// raw-offset write (transform @ +0..+48, dims @ +64), per the committed
	// BrnPhysicalBodyPart::GetBoundingBox precedent. ✅ That layout is CONFIRMED, not provisional --
	// CgsGeometric::Box is homed at GameShared/GameClasses/Geometric/Primitives/CgsBox.h (see the
	// file banner).
	// =============================================================================================
	void DeformableObject::GetAlignedDeformedBoundingBox(CgsGeometric::Box* lpBoxOut)
	{
		const BrnPhysics::Vehicle::VehiclePhysics* lpPhysics = mVehicleBody.GetVehiclePhysics();
		const Matrix44Affine& lTransform = lpPhysics->GetTransform();

		// Deformed AABB corners the vehicle physics maintains (SetDeformableBBox: min @ +0x6D0, max @ +0x6E0).
		const char* lpPhysBytes = reinterpret_cast<const char*>(lpPhysics);
		const Vector3& lvMin = *reinterpret_cast<const Vector3*>(lpPhysBytes + 0x6D0);
		const Vector3& lvMax = *reinterpret_cast<const Vector3*>(lpPhysBytes + 0x6E0);

		// halfDims = (max - min) * 0.5 - KVF_CAR_BBOX_SHRINK (FLAGGED-0) ; centre = (min + max) * 0.5.
		const Vector3 lvHalfDims = {
			(lvMax.x - lvMin.x) * 0.5f - KVF_CAR_BBOX_SHRINK.x,
			(lvMax.y - lvMin.y) * 0.5f - KVF_CAR_BBOX_SHRINK.y,
			(lvMax.z - lvMin.z) * 0.5f - KVF_CAR_BBOX_SHRINK.z,
			0.0f
		};
		const Vector3 lvCentreBody = {
			(lvMax.x + lvMin.x) * 0.5f,
			(lvMax.y + lvMin.y) * 0.5f,
			(lvMax.z + lvMin.z) * 0.5f,
			0.0f
		};
		// centreWorld = transform * centreBody (the vmaddfp cascade right*cx + up*cy + at*cz + pos).
		const Vector3 lvCentreWorld = {
			lTransform.xAxis.x * lvCentreBody.x + lTransform.yAxis.x * lvCentreBody.y +
				lTransform.zAxis.x * lvCentreBody.z + lTransform.wAxis.x,
			lTransform.xAxis.y * lvCentreBody.x + lTransform.yAxis.y * lvCentreBody.y +
				lTransform.zAxis.y * lvCentreBody.z + lTransform.wAxis.y,
			lTransform.xAxis.z * lvCentreBody.x + lTransform.yAxis.z * lvCentreBody.y +
				lTransform.zAxis.z * lvCentreBody.z + lTransform.wAxis.z,
			0.0f
		};

		const Vector3 lvDimsAndFatness = { lvHalfDims.x, lvHalfDims.y, lvHalfDims.z, 0.0f };

		char* lpBox = reinterpret_cast<char*>(lpBoxOut);
		*reinterpret_cast<Vector3*>(lpBox +  0) = lTransform.xAxis;   // right
		*reinterpret_cast<Vector3*>(lpBox + 16) = lTransform.yAxis;   // up
		*reinterpret_cast<Vector3*>(lpBox + 32) = lTransform.zAxis;   // at
		*reinterpret_cast<Vector3*>(lpBox + 48) = lvCentreWorld;     // centre
		*reinterpret_cast<Vector3*>(lpBox + 64) = lvDimsAndFatness;  // half-dims (+ fatness lane)
	}

	// =============================================================================================
	// UpdateDeformedBBox @ 0x825E0D20
	//
	// Accumulate the deformed axis-aligned bounding box over the car's deformation sensors (each
	// sensor's LOCAL collision sphere, centre +/- radius), then -- when the per-car "deformed in
	// crash" gate is set -- latch whether the deformed box has grown past the drive-time limits.
	// The asm:
	//   maxAccum = 0 ; minAccum = 0
	//   for (i = 0; i < mpDeformationSpec.mu8NumDeformationSensors; ++i):
	//       sphere = maDeformationSensors[i].mpLocalSpaceSphere     (`*(this + 432*i + 6892)`)
	//       r = splat(sphere.w)                                      (vspltw v0,v0,3)
	//       maxAccum = max(maxAccum, sphere.centre + r)
	//       minAccum = min(minAccum, sphere.centre - r)
	//   GetVehiclePhysics()->SetDeformableBBox(minAccum, maxAccum)  (INLINED: stores min @ vehiclePhysics
	//       +0x6D0, max @ +0x6E0 -- the producer of the corner pair GetAlignedDeformedBoundingBox reads back)
	//   if (HIBYTE(*(this + 26384)) == 1):
	//       dMin = mDriveTimeBBoxLimitMin - minAccum ; dMax = mDriveTimeBBoxLimitMax - maxAccum
	//       beyond = any axis where (dMin < -tol) or (dMax > tol)   (tol = &unk_82FB9B30, FLAGGED-0)
	//       *(vehiclePhysics + 5174) = beyond                       (SetDeformedBeyondDriveTimeLimitsInCrash)
	// FLAG: the per-axis tolerance vector is FLAGGED-0, so "beyond" becomes the plain corner-crossed
	// test (the zero-tolerance limit of the asm). The accumulation, the SetDeformableBBox producer store,
	// the gate, and the beyond-limits store target are byte-faithful (member reads by name; the deformed-bbox
	// corner pair, the gate word + the bool store by asm-proven console offset). FLAG: SetDeformableBBox is
	// not yet homed -> expressed as its inlined +0x6D0/+0x6E0 byte-stores until that setter lands.
	// =============================================================================================
	void DeformableObject::UpdateDeformedBBox()
	{
		// vspltisw v13,0 ; vmr v12,v13 -- both accumulators start at the zero vector.
		Vector4 lvMaxPositions = { 0.0f, 0.0f, 0.0f, 0.0f };
		Vector4 lvMinPositions = { 0.0f, 0.0f, 0.0f, 0.0f };

		const u32 lu32NumSensors = SpecNumSensors(mpDeformationSpec);
		if ( lu32NumSensors )
		{
			u32 lu32Index = 0;
			do
			{
				const void* lpSphere = maDeformationSensors[lu32Index].mpLocalSpaceSphere;
				++lu32Index;

				const Vector4& lvSphere = SphereVec(lpSphere);
				const f32 lfRadius = lvSphere.w;   // vspltw v0,v0,3 (radius lane)

				const f32 lfMaxX = lvSphere.x + lfRadius, lfMinX = lvSphere.x - lfRadius;
				const f32 lfMaxY = lvSphere.y + lfRadius, lfMinY = lvSphere.y - lfRadius;
				const f32 lfMaxZ = lvSphere.z + lfRadius, lfMinZ = lvSphere.z - lfRadius;

				if ( lfMaxX > lvMaxPositions.x ) lvMaxPositions.x = lfMaxX;   // vmaxfp v13
				if ( lfMaxY > lvMaxPositions.y ) lvMaxPositions.y = lfMaxY;
				if ( lfMaxZ > lvMaxPositions.z ) lvMaxPositions.z = lfMaxZ;

				if ( lfMinX < lvMinPositions.x ) lvMinPositions.x = lfMinX;   // vminfp v12
				if ( lfMinY < lvMinPositions.y ) lvMinPositions.y = lfMinY;
				if ( lfMinZ < lvMinPositions.z ) lvMinPositions.z = lfMinZ;
			}
			while ( lu32Index < lu32NumSensors );
		}

		// GetVehiclePhysics()->SetDeformableBBox(lvMinPositions, lvMaxPositions) -- store the accumulated
		// deformed AABB back into the attached vehicle physics. This is the producer of the +0x6D0/+0x6E0
		// corner pair GetAlignedDeformedBoundingBox reads back. The asm INLINES the setter here in the first
		// block (DWARF BrnDeformableObject.cpp:143-166; dossier store at lines 7039-7050: stack-stage v12/v13
		// then copy both Vector4s into the body), so it is modelled as that same pair of stores (min @ +0x6D0,
		// max @ +0x6E0) by the asm-proven console offsets. FLAG: SimpleVehiclePhysics::SetDeformableBBox is not
		// yet homed; expressed as its inlined byte-stores until that setter lands.
		BrnPhysics::Vehicle::VehiclePhysics* lpDeformPhysics = mVehicleBody.GetVehiclePhysics();
		char* lpDeformPhysBytes = reinterpret_cast<char*>(lpDeformPhysics);
		*reinterpret_cast<Vector4*>(lpDeformPhysBytes + KU_DEFORMABLE_BBOX_MIN_OFFSET) = lvMinPositions;
		*reinterpret_cast<Vector4*>(lpDeformPhysBytes + KU_DEFORMABLE_BBOX_MAX_OFFSET) = lvMaxPositions;

		// HIBYTE(*(this + 26384)) == 1 -- the "deformed in crash" gate.
		const u32 lu32GateWord =
			*reinterpret_cast<const u32*>(reinterpret_cast<const char*>(this) + KU_DEFORMED_BBOX_GATE_WORD_OFFSET);
		if ( static_cast<u8>(lu32GateWord >> 24) == 1 )
		{
			// dMin = driveMin - deformedMin ; dMax = driveMax - deformedMax.
			const Vector3 lDMin = {
				mDriveTimeBBoxLimitMin.x - lvMinPositions.x,
				mDriveTimeBBoxLimitMin.y - lvMinPositions.y,
				mDriveTimeBBoxLimitMin.z - lvMinPositions.z,
				0.0f
			};
			const Vector3 lDMax = {
				mDriveTimeBBoxLimitMax.x - lvMaxPositions.x,
				mDriveTimeBBoxLimitMax.y - lvMaxPositions.y,
				mDriveTimeBBoxLimitMax.z - lvMaxPositions.z,
				0.0f
			};

			// beyond if any axis where (-tol > dMin) i.e. deformedMin pushed below the limit, or
			// (dMax > tol) i.e. deformedMax pushed above the limit. tol == FLAGGED-0 vector.
			const f32 lfNegTolX = -KVF_DEFORMED_BBOX_TOLERANCE.x;
			const f32 lfNegTolY = -KVF_DEFORMED_BBOX_TOLERANCE.y;
			const f32 lfNegTolZ = -KVF_DEFORMED_BBOX_TOLERANCE.z;

			bool lbBeyond = false;
			if ( lfNegTolX > lDMin.x ) lbBeyond = true;                 // vcmpgtfp. ( -tol > dMin.x )
			else if ( lfNegTolY > lDMin.y ) lbBeyond = true;
			else if ( lfNegTolZ > lDMin.z ) lbBeyond = true;
			else if ( lDMax.x > KVF_DEFORMED_BBOX_TOLERANCE.x ) lbBeyond = true;   // vcmpgtfp. ( dMax.x > tol )
			else if ( lDMax.y > KVF_DEFORMED_BBOX_TOLERANCE.y ) lbBeyond = true;
			else if ( lDMax.z > KVF_DEFORMED_BBOX_TOLERANCE.z ) lbBeyond = true;

            // Breaker @0x825E0EBC inlines RaceCarPhysics::
            // SetDeformedBeyondDriveTimeLimitsInCrash as `stb +0x1436`.
            BrnPhysics::Vehicle::RaceCarPhysics* lpPhysics = AsRaceCarPhysics();
            lpPhysics->SetDeformedBeyondDriveTimeLimitsInCrash(lbBeyond);
        }
	}

	// =============================================================================================
	// CalculateDriveTimeLimits @ 0x82609DC8
	//
	// Compute the per-car drive-time deformed-bbox clamp band: the axis-aligned box that encloses
	// every deformation-sensor spec sphere (offset +/- radius), then widened per axis by the car's
	// drive-time deform limits. Stored into mDriveTimeBBoxLimitMin / mDriveTimeBBoxLimitMax. The asm:
	//   maxAccum = 0 ; minAccum = 0
	//   for (i = 0; i < mpDeformationSpec.mu8NumDeformationSensors; ++i):
	//       assert i < mu8NumDeformationSensors                         (non-gating)
	//       offset = maDeformationSensorSpecs[i].mInitialOffset  (`*((i<<6) + spec + 272)`)
	//       r      = maDeformationSensorSpecs[i].mfRadius        (`*((i<<6) + spec + 312)`)
	//       maxAccum = max(maxAccum, offset + r) ; minAccum = min(minAccum, offset - r)
	//   assert mVehicleBody.GetVehiclePhysics() != NULL              (non-gating)
	//   limits = mpAttribs->mBaseAttribs.mDrivetimeDeformLimits  (`*(*(this+6476)+1824) + 64`)
	//       max.x -= limits.x ; max.y -= limits.y ; max.z -= limits.w
	//       min.x += limits.x ; min.y += limitX ; min.z += limits.z
	// (the lane mapping is the recovered vrlimi128/vspltw sequence; see the FLAG on min.y below.)
	// =============================================================================================
	void DeformableObject::CalculateDriveTimeLimits()
	{
		// stvx128 0 -> both accumulators (mDriveTimeBBoxLimitMin == r30, mDriveTimeBBoxLimitMax == r29).
		Vector3 lvMax = { 0.0f, 0.0f, 0.0f, 0.0f };
		Vector3 lvMin = { 0.0f, 0.0f, 0.0f, 0.0f };

		const StreamedDeformationSpec* lpSpec = mpDeformationSpec;
		const u32 lu32NumSensors = SpecNumSensors(lpSpec);
		if ( lu32NumSensors )
		{
			const char* lpSpecBase = reinterpret_cast<const char*>(lpSpec);
			u32 lu32Index = 0;
			do
			{
				CGS_ASSERT(lu32Index < SpecNumSensors(mpDeformationSpec),
				           "liSensorIndex < mu8NumDeformationSensors");   // BrnStreamedDeformationSpec.h:201 (non-gating)

				// maDeformationSensorSpecs[i].mInitialOffset @ spec+272 (stride 64), .mfRadius @ +312.
				const char* lpSensorSpec = lpSpecBase + (static_cast<u32>(lu32Index) << 6) + 272;
				const Vector3& lvOffset = *reinterpret_cast<const Vector3*>(lpSensorSpec);
				const f32 lfRadius = *reinterpret_cast<const f32*>(lpSpecBase + (static_cast<u32>(lu32Index) << 6) + 312);
				++lu32Index;

				const f32 lfMaxX = lvOffset.x + lfRadius, lfMinX = lvOffset.x - lfRadius;
				const f32 lfMaxY = lvOffset.y + lfRadius, lfMinY = lvOffset.y - lfRadius;
				const f32 lfMaxZ = lvOffset.z + lfRadius, lfMinZ = lvOffset.z - lfRadius;

				if ( lfMaxX > lvMax.x ) lvMax.x = lfMaxX;
				if ( lfMaxY > lvMax.y ) lvMax.y = lfMaxY;
				if ( lfMaxZ > lvMax.z ) lvMax.z = lfMaxZ;

				if ( lfMinX < lvMin.x ) lvMin.x = lfMinX;
				if ( lfMinY < lvMin.y ) lvMin.y = lfMinY;
				if ( lfMinZ < lvMin.z ) lvMin.z = lfMinZ;
			}
			while ( lu32Index < lu32NumSensors );
		}

		BrnPhysics::Vehicle::VehiclePhysics* lpPhysics = mVehicleBody.GetVehiclePhysics();
		CGS_ASSERT(lpPhysics != nullptr,
		           "mVehicleBody.GetVehiclePhysics() != NULL");   // BrnDeformableObject.cpp:4075 (non-gating)

		// limits = mpAttribs->mBaseAttribs.mDrivetimeDeformLimits (`*(vehiclePhysics+0x720) + 0x40`).
		const char* lpAttribs =
			*reinterpret_cast<const char* const*>(reinterpret_cast<const char*>(lpPhysics) + KU_VEHICLE_ATTRIBS_PTR_OFFSET);
		// [marked deviation, 2026-08-14 deformation-mount wave] mpAttribs NULL-guard. On the
		// console mpAttribs is never null here (VehiclePhysics::Construct seeds it at the embedded
		// attrib set before any car exists); on this build the per-car Construct chain is still
		// gated behind VehicleManager::PrepareData, so the CREATE-time ResetDeformation reaches
		// this read before SetAttributes has seated the pointer (boot-measured AV READING 0x40,
		// 2026-08-14 04:56 run). Zero limits = no widening, loudly, once; real attribs apply on
		// the next reset once seated.
		if (lpAttribs == 0)
		{
			static bool sbLoggedNullAttribsGate = false;
			if (!sbLoggedNullAttribsGate)
			{
				sbLoggedNullAttribsGate = true;
				if (CgsDev::Message::gxMessageFilterFlags & 1)
					*CgsDev::Log::gpDebugPrint
						<< "conductor gate: CalculateDriveTimeLimits with NULL mpAttribs (per-car "
						   "VehiclePhysics::Construct still gated in PrepareData) -- drive-time "
						   "limits left unwidened [FLAG PC boot gate]. Reported once\n";
			}
			mDriveTimeBBoxLimitMin = lvMin;
			mDriveTimeBBoxLimitMax = lvMax;
			return;
		}
		const Vector4& lvLimits =
			*reinterpret_cast<const Vector4*>(lpAttribs + KU_DRIVETIME_DEFORM_LIMITS_OFFSET);

		// Widen per axis (the recovered vrlimi128 lane writes):
		//   max.x -= limits.x ; max.y -= limits.y ; max.z -= limits.w
		lvMax.x -= lvLimits.x;
		lvMax.y -= lvLimits.y;
		lvMax.z -= lvLimits.w;
		//   min.x += limits.x ; min.y += limits.x ; min.z += limits.z
		// FLAG: the asm seeds min.y's addend from a separately-stacked GetDriveTimeDeformLimitX VecFloat
		// (the `v9 = splat(stack,0)`); its lane resolves to limits.x in the recovered sequence. The
		// min.y addend is the one lane not byte-pinned -- revisit if the call-site register map is recovered.
		lvMin.x += lvLimits.x;
		lvMin.y += lvLimits.x;
		lvMin.z += lvLimits.z;

		mDriveTimeBBoxLimitMin = lvMin;
		mDriveTimeBBoxLimitMax = lvMax;
	}

	// =============================================================================================
	// GetDeformationSensorFromVolumeInstance @ 0x825B4338
	//
	// Map a scene volume-instance id back to one of the car's DeformationSensors. The asm receives the
	// volume index as a u8 (the caller extracts it from the VolumeInstanceId); sensorIndex = u8 - 1.
	//   assert 0 <= sensorIndex < mpDeformationSpec.GetNumDeformationSensors() + KI_MAX_NUM_WHEEL_POINTS
	//   if (sensorIndex >= numDeformationSensors):              -- a WHEEL point
	//       wheel = sensorIndex - numDeformationSensors
	//       assert wheel < KI_MAX_NUM_WHEEL_POINTS
	//       assert mau8WheelToSensorMap[wheel] < numDeformationSensors
	//       return maDeformationSensors[ mau8WheelToSensorMap[wheel] ]   (`432*(map+15)+this`)
	//   else:
	//       return maDeformationSensors[ sensorIndex ]                   (`432*(sensorIndex+15)+this`)
	//
	// FLAG (name): the dossier truncates this symbol to "GetDeform"; its asm returns a DeformationSensor*
	// and its callers are ReadPotentialContact / ReadPotentialVehicleWorldContact, so it is the frozen
	// header's GetDeformationSensorFromVolumeInstance (:415), NOT GetDeformedBoundingBox (:545). The u8
	// is taken as the low byte of the VolumeInstanceId (the asm receives it pre-extracted) -- see FLAG.
	// =============================================================================================
	DeformationSensor& DeformableObject::GetDeformationSensorFromVolumeInstance(CgsSceneManager::VolumeInstanceId lId)
	{
		// FLAG: the asm gets the volume index as a u8 already split out of the 64-bit id; modelled here
		// as the id's low byte. Revisit if the caller-side field extraction is byte-recovered.
		const s32 liSensorIndex = static_cast<s32>(static_cast<u8>(lId.muId)) - 1;

		const s32 liNumDeformationSensors = static_cast<s32>(SpecNumSensors(mpDeformationSpec));

		CGS_ASSERT(liSensorIndex >= 0 && liSensorIndex < liNumDeformationSensors + KI_MAX_NUM_WHEEL_POINTS_PER_DEFORMABLE_OBJECT,
		           "liSensorIndex >= 0 && liSensorIndex < mpDeformationSpec->GetNumDeformationSensors() + KI_MAX_NUM_WHEEL_POINTS_PER_DEFORMABLE_OBJECT");   // BrnDeformableObject.h:1041

		if ( liSensorIndex >= liNumDeformationSensors )
		{
			const s32 liWheel = liSensorIndex - liNumDeformationSensors;
			CGS_ASSERT(liWheel < KI_MAX_NUM_WHEEL_POINTS_PER_DEFORMABLE_OBJECT,
			           "liSensorIndex < KI_MAX_NUM_WHEEL_POINTS_PER_DEFORMABLE_OBJECT");   // BrnDeformableObject.h:1050
			CGS_ASSERT(mau8WheelToSensorMap[liWheel] < liNumDeformationSensors,
			           "mau8WheelToSensorMap[liSensorIndex] < mpDeformationSpec->GetNumDeformationSensors()");   // BrnDeformableObject.h:1051
			return maDeformationSensors[mau8WheelToSensorMap[liWheel]];
		}

		return maDeformationSensors[liSensorIndex];
	}

	// =============================================================================================
	// GetDeformationSphereFromVolumeInstance @ 0x825B4428
	//
	// Map a scene volume-instance id back to one of the car's collision spheres. Same index decode as
	// GetDeformationSensorFromVolumeInstance, but it returns a copy of the Sphere (sret). The asm:
	//   sensorIndex = u8 - 1
	//   assert 0 <= sensorIndex < numDeformationSensors + KI_MAX_NUM_WHEEL_POINTS
	//   if (sensorIndex >= numDeformationSensors):              -- a WHEEL point
	//       assert (sensorIndex - numDeformationSensors) < KI_MAX_NUM_WHEEL_POINTS
	//       src = &maWorldSensorSpheres[sensorIndex]            (`16*sensorIndex + this`)
	//   else:
	//       src = maDeformationSensors[sensorIndex].mpWorldSpaceSphere   (`*(432*sensorIndex + this + 6896)`)
	//   *out = *src   (two 64-bit-word copy of the 16-byte sphere)
	//
	// FLAG: for the wheel branch the asm copies a Sphere straight out of the leading maWorldSensorSpheres
	// array (the wheel sphere overlays the world sensor sphere slot at +16*sensorIndex); for the body
	// branch it follows the sensor's mpWorldSpaceSphere pointer. The u8->index decode FLAG is shared with
	// GetDeformationSensorFromVolumeInstance.
	// =============================================================================================
	CgsGeometric::Sphere DeformableObject::GetDeformationSphereFromVolumeInstance(CgsSceneManager::VolumeInstanceId lId)
	{
		const s32 liSensorIndex = static_cast<s32>(static_cast<u8>(lId.muId)) - 1;

		const s32 liNumDeformationSensors = static_cast<s32>(SpecNumSensors(mpDeformationSpec));

		CGS_ASSERT(liSensorIndex >= 0 && liSensorIndex < liNumDeformationSensors + KI_MAX_NUM_WHEEL_POINTS_PER_DEFORMABLE_OBJECT,
		           "liSensorIndex >= 0 && liSensorIndex < mpDeformationSpec->GetNumDeformationSensors() + KI_MAX_NUM_WHEEL_POINTS_PER_DEFORMABLE_OBJECT");   // BrnDeformableObject.h:1069

		const void* lpSource;
		if ( liSensorIndex >= liNumDeformationSensors )
		{
			CGS_ASSERT(liSensorIndex - liNumDeformationSensors < KI_MAX_NUM_WHEEL_POINTS_PER_DEFORMABLE_OBJECT,
			           "liSensorIndex - mpDeformationSpec->GetNumDeformationSensors() < KI_MAX_NUM_WHEEL_POINTS_PER_DEFORMABLE_OBJECT");   // BrnDeformableObject.h:1077
			// `v7 = 16 * sensorIndex + this` -- the wheel sphere overlays the leading world-sphere array.
			lpSource = &maWorldSensorSpheres[liSensorIndex];
		}
		else
		{
			// `v7 = *(432 * sensorIndex + this + 6896)` -- maDeformationSensors[sensorIndex].mpWorldSpaceSphere.
			lpSource = maDeformationSensors[liSensorIndex].mpWorldSpaceSphere;
		}

		// *out = *src (the two 64-bit-word copy of the packed 16-byte sphere). The source Sphere (a
		// forward-declared BrnPhysics::Deformation::Sphere for the body branch) and the returned
		// CgsGeometric::Sphere share the leading centre.xyz/radius.w Vector4 layout, copied through it.
		CgsGeometric::Sphere lResult;
		lResult.mPositionRadius = SphereVec(lpSource);
		return lResult;
	}
}
}
