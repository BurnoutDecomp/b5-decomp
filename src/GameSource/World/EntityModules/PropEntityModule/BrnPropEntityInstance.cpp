// ============================================================================
// Bodies for BrnWorld::PropEntityInstance, reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct          @ 0x822A1A98
//   InitialiseFromData @ 0x822B80E8
//   IsSmashed          @ 0x822B82C8
//   IsPhysical         @ 0x822B8338
//   SetState           @ 0x822A1BC0
//   SetPhysicsIndex    @ 0x822A1C20
//
// Member names / offsets are the DecFIGS DWARF layout (BrnPropEntityInstance.h), each
// offset asm-verified for this TU. The X360 compiler folds the world-transform
// initialisation into a VMX cascade and de-inlines the range asserts into
// BeginAssert/FireAssert/EndAssert triples; those collapse to CGS_ASSERT with the
// original rodata message (file/line dropped per project convention).
//
// Authority for declaration shape is the DecFIGS DWARF; authority for control flow,
// offsets, assert strings and line numbers is the X360 pseudocode + disassembly.

#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityInstance.h"

#include "SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h"   // PropTypeData (IsLamppost / +0x5D smashable)
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"          // BrnWorld::PropEntityID (by-value param -- needs complete type)
#include "GameShared/GameClasses/Core/CgsAssert.h"                // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"                        // rw::math::vpu::Dot (UpdateConstraints axis dots)

#include <cmath>   // std::acos / std::fabs

namespace BrnWorld
{
    namespace vpu = rw::math::vpu;

    // @ 0x822A1A98. Default-construct a loaded prop slot: identity world transform, and
    // every scalar field cleared (rotation-params index defaults to -1 == "none"). Note the
    // X360 body leaves muInstanceID (+64) and mu16PartsIndex (+72) untouched -- they are
    // filled by InitialiseFromData / the zone loader afterwards.
    void PropEntityInstance::Construct()
    {
        // Identity Matrix44Affine (rodata flt_82001C98 == 1.0f diagonal lanes,
        // flt_82001CC0 == 0.0f off-diagonal + wAxis) stored to +0/+16/+32/+48.
        // Matrix44Affine::SetIdentity() (vendor rw math home) produces exactly this
        // rodata-attested pattern (1.0-diagonal upper-3x3, all-zero wAxis).
        mWorldTransform.SetIdentity();

        muTypeId               = 0;    // +68  (sth 0x44)
        mu16ZoneIndex          = 0;    // +70  (sth 0x46)
        mu8Flags               = 0;    // +74  (stb 0x4A)
        mi8RotationParamsIndex = -1;   // +75  (stb 0x4B, r31 == -1)
        mu8PhysicsIndex        = 0;    // +76  (stb 0x4C)
        mu8State               = 0;    // +77  (stb 0x4D)  E_NON_PHYSICAL
        mu8NextState           = 0;    // +78  (stb 0x4E)
    }

    // @ 0x822B80E8. Populate this prop instance from its loaded PropInstanceData record and
    // the shared PropTypeData descriptor. Sets the zone index, seeds mu8Flags from the
    // instance-data flags (animated / smashable / lamppost), composes the world transform,
    // copies the instance id / type id, and range-checks the rotation-params index.
    //
    // The PropEntityID argument is passed for the caller's entity-id bookkeeping; the X360
    // body derives the stored instance/type ids from the PropInstanceData record instead and
    // never reads liEntityID, so it is intentionally unused here.
    void PropEntityInstance::InitialiseFromData(const BrnPhysics::Props::PropInstanceData* lpInstanceData,
                                                const BrnPhysics::Props::PropTypeData*     lpTypeData,
                                                PropEntityID                               /*liEntityID*/,
                                                u16                                        lu16ZoneIndex,
                                                s32                                        liRotationParamsIndex)
    {
        const u8* lpInstanceBytes = reinterpret_cast<const u8*>(lpInstanceData);

        mu16ZoneIndex = lu16ZoneIndex;   // +70

        // Instance-data flags live at instance+0x40 (a 32-bit field). muTypeId is its low
        // half; bit 26 (0x04000000) clear enables physics for this prop (stb value 1 ==
        // KU_PHYSICS_ENABLED_BIT).
        const u32 luInstanceFlags = *reinterpret_cast<const u32*>(lpInstanceBytes + 0x40);
        mu8Flags = 0;                    // +74 (cleared first, stb 0x4A r27==0)
        muTypeId = static_cast<u16>(luInstanceFlags);   // +68 (low half, sth 0x44)
        if ((luInstanceFlags & 0x04000000u) == 0)
        {
            mu8Flags = KU_PHYSICS_ENABLED_BIT;           // 1 == physics enabled
        }

        // World transform: copy the instance-data affine (4 rows @ instance +0/+16/+32/+48)
        // into this, then recompute the position row (wAxis) with the VMX fused cascade the
        // X360 emits, using a scale vector broadcast from PropTypeData+0x10:
        //     v13 = row0 * s.x
        //     v13 = row1 * v13 + s.y       (vmaddfp v12*v13 + v8)
        //     v0  = row2 * v13 + s.z       (vmaddfp v11*v13 + v0)
        //     row3 = row3 + v0             (vaddfp)
        // i.e. per lane: wAxis[i] = wAxis[i] + row2[i]*(row1[i]*(row0[i]*s.x) + s.y) + s.z.
        // Reproduced lane-for-lane; the operand register wiring is preserved exactly (the
        // fused expression does not factor into a recognisable affine transform).
        const Matrix44Affine& lrInstanceTransform =
            *reinterpret_cast<const Matrix44Affine*>(lpInstanceBytes);
        mWorldTransform.xAxis = lrInstanceTransform.xAxis;
        mWorldTransform.yAxis = lrInstanceTransform.yAxis;
        mWorldTransform.zAxis = lrInstanceTransform.zAxis;
        mWorldTransform.wAxis = lrInstanceTransform.wAxis;
        {
            const Vector4& lrScale = *reinterpret_cast<const Vector4*>(
                reinterpret_cast<const u8*>(lpTypeData) + 0x10);
            const f32 lfScaleX = lrScale.x;
            const f32 lfScaleY = lrScale.y;
            const f32 lfScaleZ = lrScale.z;

            const Vector3& lrRow0 = mWorldTransform.xAxis;
            const Vector3& lrRow1 = mWorldTransform.yAxis;
            const Vector3& lrRow2 = mWorldTransform.zAxis;
            Vector3&       lrRow3 = mWorldTransform.wAxis;

            lrRow3.x += lrRow2.x * (lrRow1.x * (lrRow0.x * lfScaleX) + lfScaleY) + lfScaleZ;
            lrRow3.y += lrRow2.y * (lrRow1.y * (lrRow0.y * lfScaleX) + lfScaleY) + lfScaleZ;
            lrRow3.z += lrRow2.z * (lrRow1.z * (lrRow0.z * lfScaleX) + lfScaleY) + lfScaleZ;
            lrRow3.w += lrRow2.w * (lrRow1.w * (lrRow0.w * lfScaleX) + lfScaleY) + lfScaleZ;
        }

        // Instance id comes from instance+0x44.
        muInstanceID = *reinterpret_cast<const u32*>(lpInstanceBytes + 0x44);   // +64

        // Smashable flag: PropTypeData+0x5D non-zero -> mark smashable.
        if (*(reinterpret_cast<const u8*>(lpTypeData) + 0x5D) != 0)
        {
            mu8Flags |= KU_SMASHABLE_BIT;   // 8
        }

        if (lpTypeData->IsLamppost())
        {
            mu8Flags |= KU_LAMPPOST_BIT;    // 64
        }

        mu8State = 0;                       // +77  E_NON_PHYSICAL (stb 0x4D)

        CGS_ASSERT(liRotationParamsIndex >= -1, "liRotationParamsIndex >= -1");
        CGS_ASSERT(liRotationParamsIndex < 127, "liRotationParamsIndex < 127");
        CGS_ASSERT(liRotationParamsIndex < 100,
                   "liRotationParamsIndex < static_cast< int32_t > ( BrnPhysics::Props::KU_MAX_PROPS_TO_UPDATE )");

        // A prop that carries rotation params must be animated (top two flag bits of the
        // instance-data byte @ +0x4A: 0x00 / 0x40 / 0x80 are the valid animated encodings).
        if (mi8RotationParamsIndex != -1)
        {
            const u32 luAnimatedBits = *(lpInstanceBytes + 0x4A) & 0xC0u;
            const bool lbAnimated =
                (luAnimatedBits == 0x00u) || (luAnimatedBits == 0x40u) || (luAnimatedBits == 0x80u);
            CGS_ASSERT(lbAnimated,
                       "(mi8RotationParamsIndex == -1) || lpInstanceData->IsAnimated()");
        }

        mi8RotationParamsIndex = static_cast<s8>(liRotationParamsIndex);   // +75
        mu8NextState           = 0;                                       // +78
    }

    // @ 0x822B82C8. True once the prop has broken up (state >= E_SMASHED). branchless in the
    // X360 (subfc/subfe carry trick) -> a plain unsigned compare here.
    bool PropEntityInstance::IsSmashed() const
    {
        CGS_ASSERT(mu8State < E_STATE_COUNT, "mu8State < E_STATE_COUNT");
        return mu8State >= E_SMASHED;
    }

    // @ 0x822B8338. True when the prop is in the plain physical state (state == E_PHYSICAL).
    bool PropEntityInstance::IsPhysical() const
    {
        CGS_ASSERT(mu8State < E_STATE_COUNT, "mu8State < E_STATE_COUNT");
        return mu8State == E_PHYSICAL;
    }

    // @ 0x822A1BC0. Set the prop's simulation state (range-checked against E_STATE_COUNT).
    void PropEntityInstance::SetState(EPropState leState)
    {
        CGS_ASSERT(leState < E_STATE_COUNT, "leState < E_STATE_COUNT");
        mu8State = static_cast<u8>(leState);   // +77
    }

    // @ 0x822A1C20. Assign the prop's slot in the physics pool (0 .. KU_MAX_PHYSICAL_PROPS-1).
    void PropEntityInstance::SetPhysicsIndex(s32 liPhysicsIndex)
    {
        CGS_ASSERT(liPhysicsIndex >= 0, "liPhysicsIndex >= 0");
        CGS_ASSERT(liPhysicsIndex < 15,
                   "liPhysicsIndex < static_cast< int32_t > ( BrnPhysics::Props::KU_MAX_PHYSICAL_PROPS )");
        mu8PhysicsIndex = static_cast<u8>(liPhysicsIndex);   // +76
    }

    // @ 0x822A9D38. Speed-modulation ease curve for an animated prop swinging between its
    // min/max angle limits. Returns a [0.1 .. 1.0] multiplier applied to the base rotation
    // speed: full speed (1.0) across the central band of travel, easing down toward the
    // limits so the prop slows as it approaches either end.
    //
    // Store-for-store from the X360 asm. When the min limit is numerically above the max
    // (the range wraps through 360 deg), the true/max angles are lifted by +360 so the
    // comparisons run on an unwrapped scale (rodata flt_82004928 == 360.0).
    f32 PropEntityInstance::CalculateEaseInSpeedModulation(f32 lfTrueAngle,
                                                           f32 lfMinAngle,
                                                           f32 lfMaxAngle)
    {
        // Unwrap a range that crosses the 360->0 seam.
        if (lfMinAngle > lfMaxAngle)
        {
            if (lfTrueAngle < lfMinAngle)
                lfTrueAngle = lfTrueAngle + 360.0f;
            lfMaxAngle = lfMaxAngle + 360.0f;
        }

        // Normalised position of the true angle within [min,max], mapped to [-1 .. +1]:
        //   mid = (max-min)*0.5 + min ;  pos = (true - mid) * 2 / (max - min).
        const f32 lfRange    = lfMaxAngle - lfMinAngle;
        const f32 lfMidPoint = lfRange * 0.5f + lfMinAngle;
        const f32 lfNormalisedPos = ((lfTrueAngle - lfMidPoint) * 2.0f) / lfRange;

        // Central dead-band -> full speed.
        if (lfNormalisedPos > -0.75f && lfNormalisedPos < 0.75f)
            return 1.0f;

        // Outside the band: ramp the attenuation back up toward 1.0 as |pos| grows past 0.75,
        //   atten = fabs( -(( |pos| - 0.75 ) * 4 - 1) )   (rodata 4.0, 1.0), clamped [0.1,1.0].
        f32 lfAttenuation = std::fabs(-(((std::fabs(lfNormalisedPos) - 0.75f) * 4.0f) - 1.0f));
        if (lfAttenuation < 0.1f)
            return 0.1f;
        if (lfAttenuation > 1.0f)
            return 1.0f;
        return lfAttenuation;
    }

    // @ 0x822A9A30. Re-evaluate an animated prop's swing limits for this frame and decide
    // which way it should turn. Reads the prop's current true angle back out of its world
    // transform (acos of a diagonal element, disambiguated by a second axis dot), converts
    // the packed min/max limit bytes to degrees, and flips the animated-direction flag
    // (mu8Flags bit KU_ANIMATED_DIRECTION_BIT) when the prop reaches an end stop. Returns the
    // (possibly negated) angular speed to drive the prop with this frame.
    //
    // The probe-vector + Dot form mirrors the X360 vmsum3fp cascade exactly: each branch
    // builds a unit axis vector on the stack and dots it against the transform rows, which
    // reduces to a single matrix element per dot. The rotation-axis selector is the top two
    // bits of PropEntityRotationParams::mnRotSpeed.
    f32 PropEntityInstance::UpdateConstraints(f32 lfAngularSpeed,
                                              f32* lpTrueAngle,
                                              f32* lpMinAngle,
                                              f32* lpMaxAngle,
                                              const PropEntityRotationParams* lpRotationParams)
    {
        CGS_ASSERT(lpTrueAngle != nullptr, "lpTrueAngle");
        CGS_ASSERT(lpMinAngle  != nullptr, "lpMinAngle");
        CGS_ASSERT(lpMaxAngle  != nullptr, "lpMaxAngle");

        // Rodata scales: acos result -> degrees, and packed 8-bit angle -> degrees (256 -> 360).
        const f32 KF_RAD_TO_DEG  = 57.288353f;   // flt_82017B38
        const f32 KF_BYTE_TO_DEG = 1.4117647f;   // flt_82017B34 == 360/255

        const Matrix44Affine& lrTransform = mWorldTransform;

        // Select the rotation axis from mnRotSpeed's top two bits and recover the true angle
        // (acos of the aligned axis element) plus the sign-disambiguation dot.
        const u32 luRotAxis =
            static_cast<u8>(lpRotationParams->mnRotSpeed) & 0xC0u;

        f32 lfCosAngle;
        f32 lfSecondaryAngle;
        if (luRotAxis == 0x40u)
        {
            const Vector3 lProbe{ 0.0f, 0.0f, 1.0f, 0.0f };
            lfCosAngle       = vpu::Dot(lProbe, lrTransform.zAxis);   // zAxis.z
            lfSecondaryAngle = vpu::Dot(lProbe, lrTransform.xAxis);   // xAxis.z
        }
        else if (luRotAxis == 0x00u)
        {
            const Vector3 lProbe{ 0.0f, 1.0f, 0.0f, 0.0f };
            lfCosAngle       = vpu::Dot(lProbe, lrTransform.yAxis);   // yAxis.y
            lfSecondaryAngle = vpu::Dot(lProbe, lrTransform.zAxis);   // zAxis.y
        }
        else
        {
            const Vector3 lProbe{ 0.0f, 1.0f, 0.0f, 0.0f };
            lfCosAngle       = vpu::Dot(lProbe, lrTransform.yAxis);   // yAxis.y
            lfSecondaryAngle = vpu::Dot(lProbe, lrTransform.xAxis);   // xAxis.y
        }

        f32 lfTrueAngle = static_cast<f32>(std::acos(lfCosAngle)) * KF_RAD_TO_DEG;
        if (lfSecondaryAngle > 0.0f)
            lfTrueAngle = 360.0f - lfTrueAngle;
        *lpTrueAngle = lfTrueAngle;

        // Packed limit bytes (muMinAngle @+3, muMaxAngle @+4) -> degrees.
        f32 lfMinAngleToTest = static_cast<f32>(lpRotationParams->muMinAngle) * KF_BYTE_TO_DEG;
        *lpMinAngle = lfMinAngleToTest;
        const f32 lfMaxDeg = static_cast<f32>(lpRotationParams->muMaxAngle) * KF_BYTE_TO_DEG;
        *lpMaxAngle = lfMaxDeg;

        // Upper limit to compare the true angle against. When the range wraps (min > max), the
        // upper limit is lifted by +360 and the true angle is unwrapped past the midpoint seam.
        f32 lfMaxAngleToTest = lfMaxDeg;
        f32 lfTrueAngleToTest = *lpTrueAngle;
        if (lfMinAngleToTest > lfMaxDeg)
        {
            lfMaxAngleToTest = lfMaxDeg + 360.0f;
            const f32 lfMidPoint = (lfMinAngleToTest - lfMaxDeg) * 0.5f + lfMaxDeg;
            if (lfTrueAngleToTest < lfMidPoint)
                lfTrueAngleToTest = *lpTrueAngle + 360.0f;
        }

        // Direction bookkeeping: KU_ANIMATED_DIRECTION_BIT (0x10) records which end the prop is
        // swinging toward. Flip it at the reached end stop and return the (signed) speed.
        const u8 luFlags = mu8Flags;   // this+0x4A
        if ((luFlags & KU_ANIMATED_DIRECTION_BIT) != 0)
        {
            if (lfTrueAngleToTest > lfMaxAngleToTest)
            {
                mu8Flags = static_cast<u8>(luFlags & ~KU_ANIMATED_DIRECTION_BIT);
                return -lfAngularSpeed;
            }
            return lfAngularSpeed;
        }

        if (lfTrueAngleToTest < lfMinAngleToTest)
        {
            mu8Flags = static_cast<u8>(luFlags | KU_ANIMATED_DIRECTION_BIT);
            return lfAngularSpeed;
        }
        return -lfAngularSpeed;
    }
}
