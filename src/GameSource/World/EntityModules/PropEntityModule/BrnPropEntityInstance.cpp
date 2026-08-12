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

#include "SharedClasses/Physics/Props/BrnPhysicsPropInstanceData.h" // PropInstanceData (the serialised record)
#include "SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h"   // PropTypeData (mCOMOffset / parts / IsLamppost)
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"          // BrnWorld::PropEntityID (by-value param -- needs complete type)
#include "GameShared/GameClasses/Core/CgsAssert.h"                // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"                        // rw::math::vpu::Dot (UpdateConstraints axis dots)

#include <cmath>    // std::acos / std::fabs
#include <cstddef>  // offsetof (the host layout pins at the foot of this file)

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
    // instance-data flags (physics / smashable / lamppost), composes the world transform
    // about the type's centre of mass, copies the instance id / type id, and range-checks
    // the rotation-params index.
    //
    // The PropEntityID argument is passed for the caller's entity-id bookkeeping; the X360
    // body derives the stored instance/type ids from the PropInstanceData record instead and
    // never reads liEntityID, so it is intentionally unused here.
    //
    // Every read below goes through a named member of the real serialised record
    // (BrnPhysicsPropInstanceData.h) -- the earlier raw `(const u8*)lpInstanceData + 0x40`
    // pokes are gone, and so are the two PropTypeData ones (+0x10 / +0x5D).
    void PropEntityInstance::InitialiseFromData(const BrnPhysics::Props::PropInstanceData* lpInstanceData,
                                                const BrnPhysics::Props::PropTypeData*     lpTypeData,
                                                PropEntityID                               /*liEntityID*/,
                                                u16                                        lu16ZoneIndex,
                                                s32                                        liRotationParamsIndex)
    {
        mu16ZoneIndex = lu16ZoneIndex;   // +70 (sth 0x46)

        // muTypeIdAndFlags packs { flags:6 | typeId:26 }. The X360 loads the whole word once
        // and stores its LOW HALF into muTypeId (`lwz 0x40(r30)` / `sth 0x44(r31)`) -- the
        // type id is 26-bit so its low half is the same value; then re-tests flag bit 0
        // (raw 0x04000000, `rlwinm r11,r11,0,5,5`): physics is enabled when DISABLEPHYSICS
        // is CLEAR.
        mu8Flags = 0;                                                  // +74 (stb 0x4A, r27==0)
        muTypeId = static_cast<u16>(lpInstanceData->GetTypeID());      // +68 (sth 0x44)
        if ((lpInstanceData->GetFlags() & BrnPhysics::Props::KI_PROP_FLAG_DISABLEPHYSICS) == 0)
        {
            mu8Flags = KU_PHYSICS_ENABLED_BIT;   // 1 == physics enabled
        }

        // World transform: copy the serialised affine (4 rows @ instance +0/+16/+32/+48),
        // then shift the translation row to the prop type's centre of mass.
        //
        // ⚠️ OPERAND-ORDER TRAP (this block was previously transcribed backwards). IDA prints
        // AltiVec `vmaddfp` in RAW FIELD ORDER `vD, vA, vB, vC` while the semantics are
        // `vD = vA*vC + vB` (the convention settled three independent ways in
        // BrnBehaviourGameplayExternal.h:419). So 0x822B8184..0x822B8194
        //     vmulfp128 v13, v13, v9      ; v13 = xAxis * splat(com.x)
        //     vmaddfp   v13, v12, v13, v8 ; v13 = yAxis * splat(com.y) + v13
        //     vmaddfp   v0,  v11, v13, v0 ; v0  = zAxis * splat(com.z) + v13
        //     vaddfp    v0,  v10, v0      ; v0  = wAxis + v0
        // is the textbook affine point transform
        //     wAxis += xAxis*com.x + yAxis*com.y + zAxis*com.z
        // and NOT the nonsense nested product the field order reads as literally. The splats
        // come from `lvx128 v0, r5, r8` (r8 == 0x10) == PropTypeData::mCOMOffset (DWARF
        // BrnPhysicsPropTypeData.h:125, console +0x10), which is exactly what
        // E_PHYSICAL_WITH_EXTRA_COM_OFFSET / HACKShouldMoveComOffset are about.
        //
        // Kept as a 4-lane expression (not vpu::TransformPoint) because the console stores
        // the full 16-byte result: the w lane is carried through, where TransformPoint
        // forces w = 0.
        mWorldTransform = lpInstanceData->GetWorldTransform();
        {
            const Vector3& lrComOffset = lpTypeData->GetCOMOffset();

            Vector3 lvComInWorld = mWorldTransform.xAxis * lrComOffset.x;
            lvComInWorld = mWorldTransform.yAxis * lrComOffset.y + lvComInWorld;
            lvComInWorld = mWorldTransform.zAxis * lrComOffset.z + lvComInWorld;

            mWorldTransform.wAxis = mWorldTransform.wAxis + lvComInWorld;
        }

        muInstanceID = lpInstanceData->GetInstanceID();   // +64 (lwz 0x44(r30) / stw 0x40)

        // Smashable == the prop type has breakable parts. The X360 folds the accessor to
        // `lbz r11,0x5D(r5)` + a bool-ify (subfic/subfe/clrlwi), i.e. muNumberOfParts != 0
        // (DWARF BrnPhysicsPropTypeData.h:144 / :101 PropTypeData::IsSmashable).
        if (lpTypeData->GetNumberOfParts() != 0)
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

        // A prop that carries rotation params must be animated. Note the X360 tests the
        // member that is STILL UNASSIGNED at this point (`lbz r11,0x4B(r31)` before the
        // `stb r28,0x4B(r31)` below), not the incoming argument -- reproduced as-is.
        if (mi8RotationParamsIndex != -1)
        {
            CGS_ASSERT(lpInstanceData->IsAnimated(),
                       "(mi8RotationParamsIndex == -1) || lpInstanceData->IsAnimated()");
        }

        mi8RotationParamsIndex = static_cast<s8>(liRotationParamsIndex);   // +75 (stb 0x4B)
        mu8NextState           = 0;                                        // +78 (stb 0x4E)
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

    // ========================================================================
    // The small field accessors (link-closure pass, 2026-08-12).
    // ------------------------------------------------------------------------
    // Only GetState was emitted OUT OF LINE by the X360 (@0x822A1B68, below); the other
    // four are header inlines there, so each is attested by the load the console folds
    // into its call sites rather than by a body of its own:
    //
    //   GetZone()          -> mu16ZoneIndex (+70/0x46)
    //        GenerateDispatchLists @0x822FB868: `lhz r28, 0x46(r29)` on the pointer
    //        GetProp(id) @0x822CDA28 just returned -- the very first thing that TU does
    //        with a prop, and the value it then bounds-checks against -1.
    //   GetTypeId()        -> muTypeId (+68/0x44)
    //        same site, next instruction: `lhz r30, 0x44(r29)`, then `cmplwi r30, 0x1F4`
    //        (== KU_MAX_PROP_TYPES 500) before it indexes the type table.
    //        (Contrast the PART record, whose muTypeId is at +64/0x40 -- `lhz r28,
    //        0x40(r31)` after GetPart @0x822FBAE8. Different struct, different offset.)
    //   GetInstanceID()    -> muInstanceID (+64/0x40)
    //        the field InitialiseFromData writes with `stw ... 0x40(r31)` @0x822B80E8.
    //   GetWorldTransform()-> mWorldTransform (+0)
    //        every consumer passes the instance pointer itself where a Matrix44Affine* is
    //        wanted (e.g. RenderPropAndCoronas), and the four `lvx128`/`stvx128` pairs in
    //        AddPropPartsToScene @0x822E090C address prop+0/+0x10/+0x20/+0x30.
    //
    // ⚠️ These read named members; the console displacements above are documentation only
    // (PropEntityInstance is host-laid-out -- see the pins at the foot of this file).
    u16 PropEntityInstance::GetZone() const
    {
        return mu16ZoneIndex;
    }

    u32 PropEntityInstance::GetTypeId() const
    {
        return muTypeId;
    }

    u32 PropEntityInstance::GetInstanceID() const
    {
        return muInstanceID;
    }

    u16 PropEntityInstance::GetFirstPartIndex() const
    {
        return mu16PartsIndex;
    }

    const Matrix44Affine& PropEntityInstance::GetWorldTransform() const
    {
        return mWorldTransform;
    }

    Matrix44Affine& PropEntityInstance::GetWorldTransform()
    {
        return mWorldTransform;
    }

    // @ 0x822A1B68. The one accessor the X360 kept out of line -- because of the range
    // tripwire (asm: `lbz r11,0x4D(r3); cmplwi r11,7; blt ...` then the
    // BeginAssert/FireAssert("mu8State < E_STATE_COUNT", BrnPropEntityInstance.h, 653)/
    // EndAssert triple), after which it returns the same byte.
    EPropState PropEntityInstance::GetState() const
    {
        CGS_ASSERT(mu8State < E_STATE_COUNT, "mu8State < E_STATE_COUNT");
        return static_cast<EPropState>(mu8State);
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

    // ========================================================================
    // Host layout tripwires (never called; namespace scope because every member the
    // accessors above read is public).
    // ------------------------------------------------------------------------
    // These two records are POINTER-FREE, so -- unusually for this project -- the console
    // displacements and the host offsets DO coincide, and pinning them is worth doing:
    // the accessors above are one-line member reads whose only way of being wrong is
    // returning the wrong field, and these asserts are what makes "wrong field" a
    // compile error rather than a silent corruption. Anyone who inserts a member (or
    // widens one) will fail here instead of shipping a prop that renders at the wrong
    // type/zone.
    static_assert(offsetof(PropEntityInstance, mWorldTransform)        ==  0, "prop mWorldTransform @+0  (lvx128 prop+0/0x10/0x20/0x30)");
    static_assert(offsetof(PropEntityInstance, muInstanceID)           == 64, "prop muInstanceID @+64/0x40 (stw 0x40 in InitialiseFromData)");
    static_assert(offsetof(PropEntityInstance, muTypeId)               == 68, "prop muTypeId @+68/0x44 (lhz 0x44 @0x822FB86C)");
    static_assert(offsetof(PropEntityInstance, mu16ZoneIndex)          == 70, "prop mu16ZoneIndex @+70/0x46 (lhz 0x46 @0x822FB868)");
    static_assert(offsetof(PropEntityInstance, mu16PartsIndex)         == 72, "prop mu16PartsIndex @+72/0x48 (lhz 0x9C8 in GetPart @0x822CDC74)");
    static_assert(offsetof(PropEntityInstance, mu8Flags)               == 74, "prop mu8Flags @+74/0x4A");
    static_assert(offsetof(PropEntityInstance, mi8RotationParamsIndex) == 75, "prop mi8RotationParamsIndex @+75/0x4B");
    static_assert(offsetof(PropEntityInstance, mu8PhysicsIndex)        == 76, "prop mu8PhysicsIndex @+76/0x4C");
    static_assert(offsetof(PropEntityInstance, mu8State)               == 77, "prop mu8State @+77/0x4D (lbz 0x4D in GetState @0x822A1B68)");
    static_assert(offsetof(PropEntityInstance, mu8NextState)           == 78, "prop mu8NextState @+78/0x4E");

    // The PART record's type id sits at +64, NOT +68 -- the two structs are deliberately
    // different, and confusing them is exactly the bug this pin exists to catch
    // (GenerateDispatchLists reads `lhz 0x40` / `lhz 0x42` off a part @0x822FBAE8).
    static_assert(offsetof(PropPartEntityInstance, mWorldTransform) ==  0, "part mWorldTransform @+0");
    static_assert(offsetof(PropPartEntityInstance, muTypeId)        == 64, "part muTypeId @+64/0x40");
    static_assert(offsetof(PropPartEntityInstance, muPartId)        == 66, "part muPartId @+66/0x42");
    static_assert(offsetof(PropPartEntityInstance, mu16ZoneIndex)   == 68, "part mu16ZoneIndex @+68/0x44");
}
