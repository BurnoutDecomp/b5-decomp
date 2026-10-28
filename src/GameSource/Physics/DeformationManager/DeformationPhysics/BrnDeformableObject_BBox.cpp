#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                        // CGS_ASSERT
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"                        // CgsGeometric::Sphere (centre.xyz + radius.w)
#include "GameShared/GameClasses/Geometric/Primitives/CgsSweptSphere.h"                   // CgsGeometric::SweptSphere
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"                // CgsGeometric::AxisAlignedBox (SetDeformableBBox's argument)
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"                      // CgsSceneManager::VolumeInstanceId
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"              // VehiclePhysics (mfSpeedMPH/IsCrashing/GetTransform/mpAttribs)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                                // gpDebugPrint / gxMessageFilterFlags (the null-attribs gate)
#include "GameSource/World/BrnEntityTypes.h"                                              // BrnWorld::E_ENTITYTYPE_RACECAR (the UpdateDeformedBBox owner gate)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehicleAttribs.h"              // VehicleAttribs::mBaseAttribs.mDrivetimeDeformLimits ([deform-bbox] attr lanes, DIAG)

#include <cstdlib>                                                                        // getenv (the [sweptsel] opt-in probe)
#include <cmath>                                                                          // std::fabs ([deform-bbox] change detection, DIAG only)

namespace BrnPhysics
{
namespace Deformation
{
	// [deform-bbox] DIAG only -- defined in BrnDeformableObject_Update.cpp: ApplySensorImpulse calls
	// whose limit rows came from the drive-time pair ([0]) vs the crash pair ([1]).
	// Console-inline accessor used by the deformation debug component.  The live deformed
	// AABB is owned by the attached SimpleVehiclePhysics at its named mDeformableAABB seat.
	void DeformableObject::GetDeformedBoundingBox(CgsGeometric::AxisAlignedBox* lpBoxOut)
	{
		*lpBoxOut = mVehicleBody.GetVehiclePhysics()->GetDeformableAABB();
	}

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
		// reached by name below (IsCrashing()/GetSpeedMPH()), retained here for the asm cross-ref only.
		// ⚠️ THAT SENTENCE WAS FALSE FOR THE SPEED UNTIL 2026-09-06 (widening-sweep wave): IsCrashing
		// was by name, the speed was NOT -- a `VehicleSpeedMPH()` helper below read the console
		// offset 0x6C0 raw off a `const char*` view, with the comment "private member, header not
		// editable". `SimpleVehiclePhysics::GetSpeedMPH()` is PUBLIC and DWARF-declared
		// (BrnSimpleVehiclePhysics.h:425), so the excuse never held. [[gates-are-stale-not-dead]]

		// ⛔⛔ RETIRED 2026-09-06 (widening-sweep wave) -- KU_DEFORMED_BBOX_GATE_WORD_OFFSET = 26384.
		// This was an X64-WIDENING GHOST and the banner beside it was wrong on both counts.
		// It said "*(this + 26384), HIGH byte == 1. No separately-named member in the frozen DWARF
		// sequence". There IS a named member: console +26384 is `mHandlingBodyID`
		// (BrnDeformableObject.h:626, the 8-byte CgsPhysics::RigidBodyId), which this repo's own
		// idfield_sweep.py has hard-coded as the HANDLING field at 0x6710 since it was written, and
		// whose owner byte is `GetHandlingBodyIdHighByte()` -- already called on the very next line
		// of the same log statement.
		// ⭐ AND THE HOST OFFSET IS NOWHERE NEAR 26384. Arithmetic, from the seats the glass wave
		// MEASURED out of the live object (0dc332c0): maDrivenPoints sits at host 22992 with a host
		// stride of 64, so the array spans 22992 .. 22992 + 128*64 == 31184. mHandlingBodyID is
		// declared AFTER it, so it is at >= 31184 -- at least 4,800 bytes past this read. Host byte
		// 26384 is therefore still INSIDE maDrivenPoints, at element (26384-22992)/64 == 53 exactly.
		// The witness's `gateRaw` column has been printing the top byte of maDrivenPoints[53]'s
		// first word and labelling it an entity-owner selector, on every logged row of every run,
		// right beside the correct `gateOwner`. [[diagnostics-that-lie]] [[x64-widening-ghost]]
		// Found by tools/re/widening_sweep.py, which is the point of that tool existing.

		// (The deformed-AABB corner pair the console stores at vehiclePhysics+0x6D0/+0x6E0 used to be
		// reached from here by those two literals. RETIRED 2026-09-03: it goes through the DWARF-declared
		// SimpleVehiclePhysics::SetDeformableBBox(const AxisAlignedBox&) now -- see UpdateDeformedBBox.)

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

		// flt_82001CC0 == 0.0f, read from the image (.rdata; `lfs f0, flt_82001CC0` @0x82609EDC).
		// CalculateDriveTimeLimits adds it -- not an attrib lane -- to the drive-time box's min.y.
		static const f32 KF_DRIVE_TIME_LIMIT_MIN_Y_ADDEND = 0.0f;

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

		// VehiclePhysics::mfSpeedMPH, BY NAME.
		// ⭐ CORRECTED 2026-09-06 (widening-sweep wave). This used to be
		//     *reinterpret_cast<const f32*>(reinterpret_cast<const char*>(lpPhysics) + 1728)
		// with the comment "private member, header not editable". Both halves were wrong:
		// `SimpleVehiclePhysics::GetSpeedMPH()` is a PUBLIC, DWARF-declared accessor
		// (BrnSimpleVehiclePhysics.h:425, X360-attested via UpdateRaceCarState @0x825EC808), and
		// the whole rest of the tree already spells this `->GetSpeedMPH().x` (PropManager_wQ2_01,
		// BrnPhysicalTrafficManager_CrashResponse x4, BrnRendererModule).
		// ⚠️ STATED PLAINLY, BECAUSE IT MATTERS: THIS WAS NOT A GHOST. 1728 == 0x6C0 IS the host
		// offsetof -- measured and pinned by `static_assert(offsetof(VehiclePhysics, mfSpeedMPH)
		// == X360LayoutCheck::KU_A_SPEEDMPH)` in VehiclePhysics_layout_check.cpp, because
		// SimpleVehiclePhysics' own block is pointer-free and the leaf vptr's 4->8 widening is
		// absorbed by the 16-aligned base. The raw read produced the RIGHT NUMBER. It is retired
		// because it is a raw-offset hack over a homed member (AGENTS.md forbids exactly this),
		// and because it was the one read standing between an unpinned literal and a silent
		// breakage the day someone carves a pointer out of that block.
		// ⚠️ Also a real type correction: mfSpeedMPH is a `VecFloat` (a BROADCAST splat -- see
		// GetSpeed()'s note), not an f32. Lane .x is the scalar; the old 4-byte read happened to
		// land on it only because the splat makes every lane equal.
		inline f32 VehicleSpeedMPH(const BrnPhysics::Vehicle::VehiclePhysics* lpPhysics)
		{
			return lpPhysics->GetSpeedMPH().x;
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
	// ⭐ &unk_83017FE0 == splat(0.447039992), the MPH -> m/s factor (see the file banner), so this reads
	// "forward speed in m/s > 6 m/s" == 13.4 mph. NOT a placeholder -- the stale "FLAGGED-0" note that
	// stood here was retired 2026-09-03; mbDoSweptSphereTests / IsCrashing reads + early-out structure
	// are byte-faithful.
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
	// ⭐ KVF_CAR_BBOX_SHRINK (&unk_82FB95E0) is RECOVERED, not FLAGGED-0 -- its initialiser @0x82C5B798
	// is a genuinely PER-AXIS row {0.1, 0.3, 0.05, 0} built from flt_82004014 / flt_82004740 /
	// flt_820047C8 (see :140 and the file banner). The stale "unrecovered rodata -> FLAGGED-0" note that
	// stood here was retired 2026-09-03. The deformed AABB corner pair is read BY NAME through
	// SimpleVehiclePhysics::GetDeformableAABB() (console +0x6D0 / +0x6E0). Box::Set modelled as the
	// raw-offset write (transform @ +0..+48, dims @ +64), per the committed
	// BrnPhysicalBodyPart::GetBoundingBox precedent. ✅ That layout is CONFIRMED, not provisional --
	// CgsGeometric::Box is homed at GameShared/GameClasses/Geometric/Primitives/CgsBox.h (see the
	// file banner).
	// =============================================================================================
	void DeformableObject::GetAlignedDeformedBoundingBox(CgsGeometric::Box* lpBoxOut)
	{
		const BrnPhysics::Vehicle::VehiclePhysics* lpPhysics = mVehicleBody.GetVehiclePhysics();
		const Matrix44Affine& lTransform = lpPhysics->GetTransform();

		// Deformed AABB corners the vehicle physics maintains (SetDeformableBBox: min @ +0x6D0,
		// max @ +0x6E0). Reached BY NAME since 2026-09-03; the two raw console-offset casts that stood
		// here were the read half of the same producer/consumer-by-literal pair UpdateDeformedBBox had.
		const CgsGeometric::AxisAlignedBox& lrDeformedAABB = lpPhysics->GetDeformableAABB();
		const Vector4& lvMin = lrDeformedAABB.mMin;
		const Vector4& lvMax = lrDeformedAABB.mMax;

		// halfDims = (max - min) * 0.5 - KVF_CAR_BBOX_SHRINK ; centre = (min + max) * 0.5.
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
	//       beyond = any axis where (dMin < -tol) or (dMax > tol)   (tol = &unk_82FB9B30 == 0.01, recovered)
	//       *(vehiclePhysics + 5174) = beyond                       (SetDeformedBeyondDriveTimeLimitsInCrash)
	// The accumulation, the SetDeformableBBox producer store, the gate, and the beyond-limits store target
	// are byte-faithful (member reads by name; the gate word + the bool store by asm-proven console offset).
	// ⭐ 2026-09-03: SetDeformableBBox IS homed now (BrnSimpleVehiclePhysics.h:298, DWARF-declared), so the
	// producer no longer writes through +0x6D0/+0x6E0 literals; and the tolerance is 0.01, not a placeholder.
	//
	// ⭐⭐ WHO REACHES THE VERDICT AT ALL. The owner-byte gate is a RACE-CAR gate: `bnelr cr6` at
	// 0x825E0DC4 returns for every owner != E_ENTITYTYPE_RACECAR. A TRAFFIC car (owner 2) therefore gets
	// its deformed AABB stored and then LEAVES -- it never computes dMin/dMax, never writes
	// mbDeformedBeyondDriveTimeLimitsInCrash, and has no drive-time "wrecked" verdict of any kind. That is
	// the CONSOLE's design, not a gap in this port: measured 2026-09-03, a run logs 259 calls with
	// gateOwner 1 and 315 with gateOwner 2, and only the first group reaches the compare. A traffic car's
	// crash severity is decided somewhere else entirely -- VehicleManager::DecideOutcomeOfRaceCarTrafficContact
	// @0x825C70A0, an impulse/mass/speed test (CRASH / CHECK / SLAM), never an extent test.
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

		// GetVehiclePhysics()->SetDeformableBBox(box) -- store the accumulated deformed AABB back into
		// the attached vehicle physics. This is the producer of the mDeformableAABB corner pair
		// GetAlignedDeformedBoundingBox, IsFrontCornerClip, PredictCarCarIntersection, the
		// UpdateSkinningOffsets clamp and the [T5-ram] witness all read back. The asm INLINES the
		// setter here in the first block (DWARF BrnDeformableObject.cpp:166 names the call;
		// 0x825E0D7C..0x825E0DB0: `lwz r10,0x194C(r3)` then `addi r10,r10,0x6D0` then four ld/std
		// pairs == one whole 32-byte AxisAlignedBox).
		//
		// ⭐ CORRECTED 2026-09-03 (traffic crash-severity wave). This used to be a pair of RAW HOST
		// BYTE WRITES through `(char*)physics + 0x6D0 / + 0x6E0` -- console literals applied to the
		// x64 object -- carrying a FLAG that said "SetDeformableBBox is not yet homed". The DWARF
		// declares it (BrnSimpleVehiclePhysics.h:298, `void SetDeformableBBox(const AxisAlignedBox&)`)
		// and it is now homed, so the write goes BY NAME like every read of it already did. The two
		// offsets were in fact correct on the host -- measured 0x6D0/0x6E0, and now static_asserted in
		// VehiclePhysics_layout_check.cpp -- so this is a faithfulness fix, NOT a behaviour fix, and
		// nothing downstream of it changes. Recorded so the next wave does not re-open the question:
		// a producer-by-literal + consumers-by-name pair is a widening ghost waiting to happen, and
		// this one was investigated as a suspect and cleared by measurement, not by argument.
		BrnPhysics::Vehicle::VehiclePhysics* lpDeformPhysics = mVehicleBody.GetVehiclePhysics();
		CgsGeometric::AxisAlignedBox lDeformedBox;
		lDeformedBox.mMin = lvMinPositions;
		lDeformedBox.mMax = lvMaxPositions;
		lpDeformPhysics->SetDeformableBBox(lDeformedBox);

		// ⭐⭐ CORRECTED 2026-09-02 (deformation wave). The gate is NOT a "deformed in crash" flag:
		//     0x825E0DB4  ld    r11, 0x6710(r3)      ; the 8-byte handling RigidBodyId (+26384)
		//     0x825E0DB8  srdi  r11, r11, 32
		//     0x825E0DBC  srwi  r11, r11, 24         ; bits 56..63 == the entity OWNER byte
		//     0x825E0DC0  cmplwi cr6, r11, 1         ; == BrnWorld::E_ENTITYTYPE_RACECAR
		//     0x825E0DC4  bnelr cr6
		// i.e. "this deformable object is a RACE CAR's" (traffic and props skip the drive-time
		// verdict). This body used to read it as `*(u32*)(this+26384) >> 24` -- a leftover of the
		// 4-byte handle stand-in that BrnDeformableObject.h:282 retired on 2026-08-11. On the x64
		// port that reads bits 24..31 of the LOW dword of the id (the index half), never the owner
		// byte, so the compare below never ran and mbDeformedBeyondDriveTimeLimitsInCrash could not
		// be written: every crash, however hard, read back mbIsDriveable == true (measured: a 153 mph
		// head-on -> [crash-verdict] DRIVE_AWAY, run crashwave_w3). Same two shifts, named.
		const bool lbHandlingBodyIsRaceCar =
			( GetHandlingBodyIdHighByte() == static_cast<u8>(BrnWorld::E_ENTITYTYPE_RACECAR) );

		// -----------------------------------------------------------------------------------------
		if ( lbHandlingBodyIsRaceCar )
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
			// (dMax > tol) i.e. deformedMax pushed above the limit. tol == 0.01 per axis (recovered).
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
		CGS_ASSERT(lpPhysics != 0,
		           "mVehicleBody.GetVehiclePhysics() != NULL");   // BrnDeformableObject.cpp:4075 (non-gating)

		// limits = mpAttribs->mBaseAttribs.mDrivetimeDeformLimits (`*(vehiclePhysics+0x720) + 0x40`).
		// ⭐⭐ CORRECTED 2026-09-02 (deformation wave): read the NAMED member. This body used to
		// dereference the CONSOLE byte offset 0x720 on the x64 VehiclePhysics object, where the
		// widened layout does not keep mpAttribs -- so it read a zero word and took the NULL arm on
		// EVERY reset (run deformw_B1: the gate fired for owner 1 at present 1491 while the witness
		// in the same ResetDeformation read GetAttribs() == {0.05,0.2,0.15,0.4}). Result: the
		// drive-time box was never widened; lim == rest box for the whole run, so with the owner
		// gate fixed a 1 cm dent would have read as "beyond limits".
		const BrnPhysics::Vehicle::VehicleAttribs* lpAttribs = lpPhysics->GetAttribs();
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
			}
			mDriveTimeBBoxLimitMin = lvMin;
			mDriveTimeBBoxLimitMax = lvMax;
			return;
		}
		const Vector4& lvLimits = lpAttribs->mBaseAttribs.mDrivetimeDeformLimits;   // attribs + 0x40

		// Widen per axis (the recovered vrlimi128 lane writes):
		//   max.x -= limits.x ; max.y -= limits.y ; max.z -= limits.w
		lvMax.x -= lvLimits.x;
		lvMax.y -= lvLimits.y;
		lvMax.z -= lvLimits.w;
		//   min.x += limits.x ; min.y += 0.0 (flt_82001CC0) ; min.z += limits.z
		// ⭐ CORRECTED 2026-09-02 (deformation wave): the min.y addend is NOT limits.x. The console
		// stacks a scalar loaded from .rdata and splats it:
		//     0x82609ECC  lis  r11, flt_82001CC0@ha
		//     0x82609EDC  lfs  f0, flt_82001CC0@l(r11)     ; 0x82001CC0 == 0x00000000 == 0.0f (image)
		//     0x82609EE8  stfs f0, var_60(r1)              ; + three `stw r28(0)` -> {c,0,0,0}
		//     0x82609F14  lvx128 v13, var_60 ; 0x82609F18 vspltw v9, v13, 0
		//     0x82609F74  vaddfp v0, v0, v9 ; 0x82609F78 vrlimi128 v13, v0, 4, 0   ; -> min.y
		// limits.x is v13 (`vspltw v13, v13, 0` @0x82609F20 off the attribs row) and feeds ONLY
		// min.x (0x82609F60) and max.x (0x82609F28). The PS3 twin @0x6BD0C4 agrees in shape: its y
		// lane adds a TOC scalar (dword_100A564) stacked at var_60, not an attrib lane. So the floor
		// of the drive-time box is the rest floor itself: a car whose underside sensors lift by more
		// than the 0.01 tolerance is beyond limits in -Y, whatever mDrivetimeDeformLimits.x says.
		lvMin.x += lvLimits.x;
		lvMin.y += KF_DRIVE_TIME_LIMIT_MIN_Y_ADDEND;
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
