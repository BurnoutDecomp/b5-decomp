#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                        // CGS_ASSERT
#include "GameShared/GameClasses/Geometric/Primitives/CgsBox.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"                        // CgsGeometric::Sphere (centre.xyz + radius.w)
#include "GameShared/GameClasses/Geometric/Primitives/CgsSweptSphere.h"                   // CgsGeometric::SweptSphere
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"                // CgsGeometric::AxisAlignedBox (SetDeformableBBox's argument)
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"                      // CgsSceneManager::VolumeInstanceId
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"              // VehiclePhysics (mfSpeedMPH/IsCrashing/GetTransform/mpAttribs)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                                // gpDebugPrint / gxMessageFilterFlags (the null-attribs gate)
#include "GameSource/World/BrnEntityTypes.h"                                              // BrnWorld::E_ENTITYTYPE_RACECAR (the UpdateDeformedBBox owner gate)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehicleAttribs.h"              // VehicleAttribs::mBaseAttribs.mDrivetimeDeformLimits ([deform-bbox] attr lanes, DIAG)

#include <xtl.h>
#include <xboxmath.h>
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
		static const Vector4 KVF_SWEPT_SPHERE_SPEED_SCALE(0.447039992f, 0.447039992f, 0.447039992f, 0.447039992f);

		// unk_82FB9B30 <- flt_82002138 = 0.01, static-init splat @0x82C5DAA0.
		static const Vector4 KVF_DEFORMED_BBOX_TOLERANCE(0.00999999978f, 0.00999999978f, 0.00999999978f, 0.00999999978f);

		// ⭐ RECOVERED 2026-08-03. unk_82FB95E0's initialiser @0x82C5B798 is NOT a splat -- it builds a
		// genuinely PER-AXIS row from three different .rdata scalars: flt_82004014 (0.1),
		// flt_82004740 (0.3) and flt_820047C8 (0.05), with lane w left at 0. Writing the usual splat here
		// would have shrunk all three axes by the same amount, which is exactly what this constant does
		// not do -- the DWARF name KVF_CAR_BBOX_SHRINK describes a car-shaped box, and the numbers are
		// car-shaped too (deepest shrink along the middle axis).
		static const Vector4 KVF_CAR_BBOX_SHRINK(0.100000001f, 0.300000012f, 0.0500000007f, 0.0f);

        inline u32 SpecNumSensors(const StreamedDeformationSpec* lpSpec)
        {
            return static_cast<u32>(lpSpec->GetNumDeformationSensors());
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

        const __vector4 vSpeed = __vmulfp(lpPhysics->GetSpeedMPH().mV, KVF_SWEPT_SPHERE_SPEED_SCALE.mV);
        unsigned int luSpeedCR = 0;
        __vcmpgtfpR(vSpeed, XMVectorReplicate(KF_MIN_SWEPT_SPHERE_SPEED), &luSpeedCR);
        if (!XMComparisonAllTrue(luSpeedCR))
            return false;

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
	// The source uses the real Box::Set and named streamed-spec members.
	// =============================================================================================
	void DeformableObject::GetBoundingBox(CgsGeometric::Box* lpBoxOut)
    {
        Matrix44Affine lTransform = mVehicleBody.GetVehiclePhysics()->GetTransform();
        lTransform.wAxis.mV = __vaddfp(lTransform.wAxis.mV, mpDeformationSpec->mCurrentCOMOffset.mV);
        lpBoxOut->Set(lTransform, mpDeformationSpec->mHandlingBodyDimensions, VecFloat(XMVectorZero()));
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
	// SimpleVehiclePhysics::GetDeformableAABB() supplies the actual stored bounds.
	// =============================================================================================
	void DeformableObject::GetAlignedDeformedBoundingBox(CgsGeometric::Box* lpBoxOut)
    {
        const BrnPhysics::Vehicle::VehiclePhysics* lpPhysics = mVehicleBody.GetVehiclePhysics();
        Matrix44Affine lTransform = lpPhysics->GetTransform();
        const CgsGeometric::AxisAlignedBox& lBounds = lpPhysics->GetDeformableAABB();
        const __vector4 vHalf = XMVectorReplicate(0.5f);
        const __vector4 vCentre = __vmulfp(__vaddfp(lBounds.mMin.mV, lBounds.mMax.mV), vHalf);
        const Vector3 lDimensions(__vsubfp(__vmulfp(__vsubfp(lBounds.mMax.mV, lBounds.mMin.mV), vHalf), KVF_CAR_BBOX_SHRINK.mV));
        __vector4 vWorld = __vmulfp(lTransform.xAxis.mV, __vspltw(vCentre, 0));
        vWorld = __vmaddfp(lTransform.yAxis.mV, __vspltw(vCentre, 1), vWorld);
        vWorld = __vmaddfp(lTransform.zAxis.mV, __vspltw(vCentre, 2), vWorld);
        lTransform.wAxis.mV = __vaddfp(lTransform.wAxis.mV, vWorld);
        lpBoxOut->Set(lTransform, lDimensions, VecFloat(XMVectorZero()));
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
        __vector4 vMax = XMVectorZero();
        __vector4 vMin = XMVectorZero();
        const u32 luNumSensors = SpecNumSensors(mpDeformationSpec);
        for (u32 i = 0; i < luNumSensors; ++i)
        {
            const __vector4 vSphere = maDeformationSensors[i].mpLocalSpaceSphere->mData.mV;
            const __vector4 vRadius = __vspltw(vSphere, 3);
            vMax = __vmaxfp(vMax, __vaddfp(vSphere, vRadius));
            vMin = __vminfp(vMin, __vsubfp(vSphere, vRadius));
        }
        CgsGeometric::AxisAlignedBox lBounds;
        lBounds.mMin.mV = vMin;
        lBounds.mMax.mV = vMax;
        mVehicleBody.GetVehiclePhysics()->SetDeformableBBox(lBounds);
        if (GetHandlingBodyIdHighByte() == static_cast<u8>(BrnWorld::E_ENTITYTYPE_RACECAR))
        {
            const __vector4 vDMin = __vsubfp(mDriveTimeBBoxLimitMin.mV, vMin);
            const __vector4 vDMax = __vsubfp(mDriveTimeBBoxLimitMax.mV, vMax);
            const __vector4 vOnes = __vspltisw(-1);
            const __vector4 vNegTolerance = __vxor(KVF_DEFORMED_BBOX_TOLERANCE.mV, __vslw(vOnes, vOnes));
            unsigned int cr = 0;
            __vcmpgtfpR(vNegTolerance, __vspltw(vDMin, 0), &cr);
            bool lbBeyond = XMComparisonAllTrue(cr);
            if (!lbBeyond) { __vcmpgtfpR(vNegTolerance, __vspltw(vDMin, 1), &cr); lbBeyond = XMComparisonAllTrue(cr); }
            if (!lbBeyond) { __vcmpgtfpR(vNegTolerance, __vspltw(vDMin, 2), &cr); lbBeyond = XMComparisonAllTrue(cr); }
            if (!lbBeyond) { __vcmpgtfpR(__vspltw(vDMax, 0), KVF_DEFORMED_BBOX_TOLERANCE.mV, &cr); lbBeyond = XMComparisonAllTrue(cr); }
            if (!lbBeyond) { __vcmpgtfpR(__vspltw(vDMax, 1), KVF_DEFORMED_BBOX_TOLERANCE.mV, &cr); lbBeyond = XMComparisonAllTrue(cr); }
            if (!lbBeyond) { __vcmpgtfpR(__vspltw(vDMax, 2), KVF_DEFORMED_BBOX_TOLERANCE.mV, &cr); lbBeyond = XMComparisonAllTrue(cr); }
            AsRaceCarPhysics()->SetDeformedBeyondDriveTimeLimitsInCrash(lbBeyond);
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
        __vector4 vMin = XMVectorZero();
        __vector4 vMax = XMVectorZero();
        const u32 luNumSensors = SpecNumSensors(mpDeformationSpec);
        mDriveTimeBBoxLimitMin.mV = vMin;
        mDriveTimeBBoxLimitMax.mV = vMax;
        for (u32 i = 0; i < luNumSensors; ++i)
        {
            CGS_ASSERT(i < SpecNumSensors(mpDeformationSpec), "liSensorIndex < mu8NumDeformationSensors");
            const SensorSpec& lSensor = mpDeformationSpec->maDeformationSensorSpecs[i];
            const __vector4 vRadius = XMVectorReplicate(lSensor.mfRadius);
            vMax = __vmaxfp(vMax, __vaddfp(lSensor.mInitialOffset.mV, vRadius));
            vMin = __vminfp(vMin, __vsubfp(lSensor.mInitialOffset.mV, vRadius));
            mDriveTimeBBoxLimitMin.mV = vMin;
            mDriveTimeBBoxLimitMax.mV = vMax;
        }
        BrnPhysics::Vehicle::VehiclePhysics* lpPhysics = mVehicleBody.GetVehiclePhysics();
        CGS_ASSERT(lpPhysics != 0, "mVehicleBody.GetVehiclePhysics() != NULL");
        const __vector4 vLimits = lpPhysics->GetAttribs()->mBaseAttribs.mDrivetimeDeformLimits.mV;
        vMax = __vrlimi(vMax, __vsubfp(vMax, __vspltw(vLimits, 0)), 8, 0);
        vMax = __vrlimi(vMax, __vsubfp(vMax, __vspltw(vLimits, 1)), 4, 0);
        vMax = __vrlimi(vMax, __vsubfp(vMax, __vspltw(vLimits, 3)), 2, 0);
        vMin = __vrlimi(vMin, __vaddfp(vMin, __vspltw(vLimits, 0)), 8, 0);
        vMin = __vrlimi(vMin, __vaddfp(vMin, XMVectorZero()), 4, 0);
        vMin = __vrlimi(vMin, __vaddfp(vMin, __vspltw(vLimits, 2)), 2, 0);
        mDriveTimeBBoxLimitMin.mV = vMin;
        mDriveTimeBBoxLimitMax.mV = vMax;
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

		const CgsGeometric::Sphere* lpSource;
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

		// Copy the selected complete sphere.
        return *lpSource;
	}
}
}
