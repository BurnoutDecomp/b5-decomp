#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h"
#include "SharedClasses/Physics/Deformation/BrnBodyPartBBoxSpec.h"   // BodyPartBBoxSpec::HackCheckHandedness (FixUp)

// BrnPhysics::Deformation::StreamedDeformationSpec.
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX. This file owns the two checked
// element accessors (GetGlassPaneSpec / GetWheelSpec) plus the four streamed-spec maintenance
// routines:
//
//   FixDown  @ 0x82631118  -- rebase embedded pointers to base-relative offsets (serialise out)
//   FixUp    @ 0x82630E18  -- rebase embedded pointers back to absolute (serialise in) + handedness
//   GetBoundingBox @ 0x825BA9E8 -- AABB over the deformation-sensor spheres
//   TransformToNewCOMSpace @ 0x825E3148 -- re-express all geometry in a new centre-of-mass frame
//
// X360 LAYOUT NOTE (offset authority): the spec's X360 image uses 32-bit pointers, so the asm
// indexes members by raw byte offset. The byte offsets the asm uses map onto this struct's members
// as follows (all proven by the per-function asm; the sensor base @ +272, the sensor count @ +1618,
// mCurrentCOMOffset @ +1632 and mMeshOffset @ +1648 are all internally consistent across the three
// VMX functions):
//
//   +4   maTagPointData        +8   miNumberOfTagPoints     (TagPointSpec stride 80)
//   +12  maDrivenPointData     +16  miNumberOfDrivenPoints  (IKDrivenPointSpec stride 32)
//   +20  maIKPartData          +24  miNumberOfIKParts       (IKBodyPartSpec stride 480)
//   +28  maGlassPaneData       +32  miNumGlassPanes
//   +36  mGenericTags(count@+36, ptr@+40)   +44 mCameraTags(count,ptr)   +52 mLightTags(count,ptr)
//   +272 maDeformationSensorSpecs[20] (SensorSpec stride 64; mInitialOffset @ +0, mfRadius @ +40)
//   +1618 mu8NumDeformationSensors   +1632 mCurrentCOMOffset   +1648 mMeshOffset
//
//   Within one IKBodyPartSpec (stride 480): +64 BodyPartBBoxSpec (HackCheckHandedness target),
//   +448 the embedded joint-spec array pointer (rebased by FixUp/FixDown), +452 the joint count.
//   Each DeformationJointSpec is 64 bytes with its translatable vector at element +0.
//   Within one LocatorPointSpec (stride 80): +68 miIkPartIndex.
//
// Because several sibling spec types (IKBodyPartSpec, DeformationJointSpec) are not yet homed, the
// fix-up / COM-shift code touches their asm-authoritative byte offsets through raw pointer
// arithmetic -- this is the canonical serialisation-fix-up idiom and is store-for-store faithful to
// the X360 control flow (same branches, same null-preserve tests, same strides, same call order).
//
// The asserts are non-gating tripwires: execution continues past a failed assert exactly as the asm
// does (BeginAssert/FireAssert/EndAssert with no early-out).
namespace BrnPhysics
{
namespace Deformation
{
    // ---- byte-offset constants the X360 serialiser/COM code indexes with -------------------------
    namespace
    {
        // IKBodyPartSpec internal offsets / stride (asm-authoritative; type not yet homed).
        static const u32 KU_IK_PART_STRIDE          = 480;   // sizeof(IKBodyPartSpec)
        static const u32 KU_IK_PART_BBOX_OFFSET     = 64;    // &IKBodyPartSpec::mBBoxSpec
        static const u32 KU_IK_PART_JOINTS_PTR      = 448;   // IKBodyPartSpec::mpaJointSpecs (embedded ptr)
        static const u32 KU_IK_PART_JOINTS_COUNT    = 452;   // IKBodyPartSpec::miNumberOfJoints
        static const u32 KU_JOINT_STRIDE            = 64;    // sizeof(DeformationJointSpec)

        // Sibling streamed-spec strides (asm-authoritative).
        static const u32 KU_TAG_POINT_STRIDE        = 80;    // sizeof(TagPointSpec) (padded)
        static const u32 KU_TAG_POINT_POS_OFFSET    = 32;    // TagPointSpec::mInitialPositionAndDetachThreshold
        static const u32 KU_DRIVEN_POINT_STRIDE     = 32;    // sizeof(IKDrivenPointSpec)
        static const u32 KU_LOCATOR_STRIDE          = 80;    // sizeof(LocatorPointSpec)
        static const u32 KU_LOCATOR_IKPART_OFFSET   = 68;    // LocatorPointSpec::miIkPartIndex

        // Deformation-sensor stride / fields (matches SensorSpec; the COM/AABB code uses these).
        static const u32 KU_SENSOR_STRIDE           = 64;    // sizeof(SensorSpec)
        static const u32 KU_SENSOR_RADIUS_OFFSET    = 40;    // SensorSpec::mfRadius

        // The squared dead-band TransformToNewCOMSpace compares |oldCOM - newCOM|^2 against to skip a
        // no-op re-frame (asm @0x825E3148: vmsum3fp128 magSq -> vandc sign-mask -> vcmpgtfp against a
        // splatted constant loaded from X360 rodata stru_8208F620, lane 0).
        // FLAGGED PLACEHOLDER -- the concrete rodata bytes are NOT in the exports and are NOT
        // fabricated here. Modelled at the renderware rwmath scale (rw::math::vpu::IsZero default
        // tolerance is 1.0e-6f) purely so the near-zero gate behaves sanely. This value is NOT
        // load-bearing: the COM shift it gates is itself a no-op at zero delta, so an imperfect epsilon
        // only changes how often the skip fires, never the result. Replace with the real rodata when homed.
        static const f32 KF_COM_MOVE_DEADBAND_SQ_PLACEHOLDER = 1.0e-6f;

        // Add a 16-byte vector lane-by-lane (the vaddfp the asm runs on xyz; w lane folded the same
        // way the SIMD register is, harmlessly).
        inline void AddVector(Vector3& lAccum, const Vector3& lDelta)
        {
            lAccum.x += lDelta.x;
            lAccum.y += lDelta.y;
            lAccum.z += lDelta.z;
            lAccum.w += lDelta.w;
        }
        inline void SubVector(Vector3& lAccum, const Vector3& lDelta)
        {
            lAccum.x -= lDelta.x;
            lAccum.y -= lDelta.y;
            lAccum.z -= lDelta.z;
            lAccum.w -= lDelta.w;
        }
    }

    // X360 @ 0x825B32D8 (Hex-Rays emitted the symbol with an empty short-name; identified as
    // GetGlassPaneSpec from BrnStreamedDeformationSpec.h:230 -- stride 112 == sizeof(GlassPaneSpec),
    // bound miNumGlassPanes, asserts at source lines 397/398).
    //   asm: cmpw liIndex, miNumGlassPanes (@+32); blt skip assert "liIndex < miNumGlassPanes" (:18D=397)
    //        cmpwi liIndex, 0; bge skip assert "liIndex >= 0" (:18E=398)
    //        result = 112 * liIndex + maGlassPaneData (@+28)
    const GlassPaneSpec* StreamedDeformationSpec::GetGlassPaneSpec(s32 liIndex) const
    {
        CGS_ASSERT(liIndex < miNumGlassPanes, "liIndex < miNumGlassPanes");   // BrnStreamedDeformationSpec.h:397
        CGS_ASSERT(liIndex >= 0, "liIndex >= 0");                             // BrnStreamedDeformationSpec.h:398
        return &maGlassPaneData[liIndex];
    }

    // X360 @ 0x822A0328 (GetWheelSpec, BrnStreamedDeformationSpec.h:256). eNumWheels == 4.
    //   asm: cmpwi liWheel, 4; blt skip assert "liWheel < eNumWheels" (:101=257)
    //        result = 48 * liWheel + this + 80   (&maWheelSpecs[liWheel])
    const WheelSpec* StreamedDeformationSpec::GetWheelSpec(s32 liWheel) const
    {
        CGS_ASSERT(liWheel < 4, "liWheel < eNumWheels");   // BrnStreamedDeformationSpec.h:257
        return &maWheelSpecs[liWheel];
    }

    // X360 @ 0x825B3258 -- StreamedDeformationSpec::GetDrivenPartSpec (DWARF
    // BrnStreamedDeformationSpec.h:222, returns const IKBodyPartSpec*). Identified by the caller
    // DeformableObject::PrepareIKPart (DWARF BrnDeformableObject.cpp:822 -> :833 calls
    // StreamedDeformationSpec::GetDrivenPartSpec) and by stride 0x1E0==480==sizeof(IKBodyPartSpec),
    // bound miNumberOfIKParts (@+0x18), base maIKPartData (@+0x14), asserts at source lines 378/379.
    //   asm: cmpw liIndex, miNumberOfIKParts; blt skip assert "liIndex < miNumberOfIKParts" (:17A=378)
    //        cmpwi liIndex, 0; bge skip assert "liIndex >= 0" (:17B=379)
    //        result = 480 * liIndex + maIKPartData
    // IKBodyPartSpec is forward-declared in this TU (full layout not needed to home it), so the
    // element address is computed via KU_IK_PART_STRIDE -- the raw-offset idiom the rest of this TU
    // uses for the un-homed sibling spec types.
    const IKBodyPartSpec* StreamedDeformationSpec::GetDrivenPartSpec(s32 liIndex) const
    {
        CGS_ASSERT(liIndex < miNumberOfIKParts, "liIndex < miNumberOfIKParts");   // BrnStreamedDeformationSpec.h:378
        CGS_ASSERT(liIndex >= 0, "liIndex >= 0");                                  // BrnStreamedDeformationSpec.h:379
        const char* lpBase = reinterpret_cast<const char*>(maIKPartData.Get());
        return reinterpret_cast<const IKBodyPartSpec*>(lpBase + KU_IK_PART_STRIDE * liIndex);
    }

    // ⭐ ADDED 2026-08-14 (walls wave): the two per-index POINT-spec accessors, the exact siblings
    // of GetDrivenPartSpec above (same checked shape, same forward-declared-element stride idiom;
    // strides 80 / 32 are this TU's own banner-attested table strides, the same numbers FixUp's
    // rebase loops walk). They un-block ResetDeformation's tag/driven rebuild loops in
    // BrnDeformableObject_Lifecycle.cpp, whose "spec accessors not exposed" FLAGs were stale --
    // this header/TU has modelled the tables since the spec landed; only these two reads were
    // missing.
    const TagPointSpec* StreamedDeformationSpec::GetTagPointSpec(s32 liIndex) const
    {
        CGS_ASSERT(liIndex < miNumberOfTagPoints, "liIndex < miNumberOfTagPoints");
        CGS_ASSERT(liIndex >= 0, "liIndex >= 0");
        const char* lpBase = reinterpret_cast<const char*>(maTagPointData.Get());
        return reinterpret_cast<const TagPointSpec*>(lpBase + KU_TAG_POINT_STRIDE * liIndex);
    }

    const IKDrivenPointSpec* StreamedDeformationSpec::GetDrivenPointSpec(s32 liIndex) const
    {
        CGS_ASSERT(liIndex < miNumberOfDrivenPoints, "liIndex < miNumberOfDrivenPoints");
        CGS_ASSERT(liIndex >= 0, "liIndex >= 0");
        const char* lpBase = reinterpret_cast<const char*>(maDrivenPointData.Get());
        return reinterpret_cast<const IKDrivenPointSpec*>(lpBase + KU_DRIVEN_POINT_STRIDE * liIndex);
    }

    // ---- LocatorPointSpecList batch accessors ---------------------------------------------------

    // X360 @ 0x825E32F0. Return-by-value copy of the locator at luTag (X360 hidden struct-return:
    // dest ptr in r3, this in r4, luTag in r5). Bounds-asserts luTag < muNumLocators
    // (BrnStreamedDeformationSpec.cpp:481, non-gating tripwire), then copies the 64-byte matrix
    // followed by the three trailing scalar members store-for-store (the 9 padding bytes 71..79 of
    // the 80-byte stride are not copied, matching the asm which only touches +0..+70).
    LocatorPointSpec LocatorPointSpecList::CreateLo(u32 luTag) const
    {
        CGS_ASSERT(luTag < muNumLocators, "luTag < muNumLocators");   // BrnStreamedDeformationSpec.cpp:481

        const LocatorPointSpec& lSrc = mpaLocatorPoints[luTag];

        LocatorPointSpec lResult;
        lResult.mLocatorMatrix = lSrc.mLocatorMatrix;   // 4x lvx128/stvx128 (64 bytes)
        lResult.meTagPointType = lSrc.meTagPointType;   // lwz/stw @+0x40 (u32)
        lResult.miIkPartIndex  = lSrc.miIkPartIndex;    // lhz/sth @+0x44 (s16)
        lResult.mu8SkinPoint   = lSrc.mu8SkinPoint;     // lbz/stb @+0x46 (u8)
        return lResult;
    }

    // X360 @ 0x82704930. Return the locator's tag-point type. Bounds-asserts luTag < muNumLocators
    // (BrnStreamedDeformationSpec.h:94, non-gating tripwire), then loads meTagPointType (element +64).
    ETagPointType LocatorPointSpecList::GetLocatorTy(u32 luTag) const
    {
        CGS_ASSERT(luTag < muNumLocators, "luTag < muNumLocators");   // BrnStreamedDeformationSpec.h:94
        return mpaLocatorPoints[luTag].meTagPointType;
    }

    // X360 @ 0x825B31E0. Return a pointer to the locator's 4x4 affine transform. The matrix is the
    // first member, so the element address IS the matrix address (asm returns the element base, no
    // added offset). Bounds-asserts luTag < muNumLocators (BrnStreamedDeformationSpec.h:98).
    const Matrix44Affine* LocatorPointSpecList::GetLocatorXf(u32 luTag) const
    {
        CGS_ASSERT(luTag < muNumLocators, "luTag < muNumLocators");   // BrnStreamedDeformationSpec.h:98
        return &mpaLocatorPoints[luTag].mLocatorMatrix;
    }

    // X360 @ 0x825BA9E8. Fill an axis-aligned bounding box that encloses every deformation-sensor
    // sphere. The asm seeds two SIMD accumulators (max = +0, min = +0) then, for each of the
    // mu8NumDeformationSensors sensors, expands them by the sensor's offset +/- its radius:
    //     vaddfp v10 = offset + radius;  vsubfp v0 = offset - radius
    //     vmaxfp v12 = max(v12, v10);    vminfp v11 = min(v11, v0)
    // Finally it stores min -> box+0 (mMin) and max -> box+16 (mMax).
    //
    // MODELLED-vs-asm: the X360 splats the per-iteration radius from a stack slot (the Hex-Rays
    // `&back_chain` artefact). The radius is the per-sensor mfRadius (the only per-sensor scalar the
    // dossier's lfSensorRadius hint names); it is read here from maDeformationSensorSpecs[i].mfRadius.
    void StreamedDeformationSpec::GetBoundingBox(CgsGeometric::AxisAlignedBox& lBoxOut) const
    {
        // vspltisw v12,0 ; vmr v11,v12 -- both accumulators start at the zero vector.
        Vector4 lvMaxPositions = { 0.0f, 0.0f, 0.0f, 0.0f };
        Vector4 lvMinPositions = { 0.0f, 0.0f, 0.0f, 0.0f };

        if ( mu8NumDeformationSensors )
        {
            u32 lu8Index = 0;
            do
            {
                const SensorSpec& lSensor = maDeformationSensorSpecs[lu8Index];
                ++lu8Index;

                const Vector3& lvOffset = lSensor.mInitialOffset;
                const f32      lfRadius = lSensor.mfRadius;          // vspltw of the radius scalar

                // offset + radius / offset - radius, then per-lane max / min into the accumulators.
                const f32 lfMaxX = lvOffset.x + lfRadius, lfMinX = lvOffset.x - lfRadius;
                const f32 lfMaxY = lvOffset.y + lfRadius, lfMinY = lvOffset.y - lfRadius;
                const f32 lfMaxZ = lvOffset.z + lfRadius, lfMinZ = lvOffset.z - lfRadius;

                if ( lfMaxX > lvMaxPositions.x ) lvMaxPositions.x = lfMaxX;
                if ( lfMaxY > lvMaxPositions.y ) lvMaxPositions.y = lfMaxY;
                if ( lfMaxZ > lvMaxPositions.z ) lvMaxPositions.z = lfMaxZ;

                if ( lfMinX < lvMinPositions.x ) lvMinPositions.x = lfMinX;
                if ( lfMinY < lvMinPositions.y ) lvMinPositions.y = lfMinY;
                if ( lfMinZ < lvMinPositions.z ) lvMinPositions.z = lfMinZ;
            }
            while ( lu8Index < mu8NumDeformationSensors );
        }

        // stvx128 v11 -> box+0 (min) ; stvx128 v12 -> box+16 (max)
        lBoxOut.mMin = lvMinPositions;
        lBoxOut.mMax = lvMaxPositions;
    }

    // X360 @ 0x825E3148. Re-express every streamed geometry point in a new centre-of-mass frame.
    //
    // The asm first tests whether the COM actually moved: it forms (mCurrentCOMOffset - lCOMOffset),
    // takes its squared magnitude (vmsum3fp128), masks the sign bit (vandc) and compares it against a
    // splatted rodata epsilon (vcmpgtfp). That predicate == !RwMath::IsZero(|old - new|^2). Only when
    // the COM moved does it apply the shift; otherwise it leaves the spec untouched.
    //
    // delta = lCOMOffset - mCurrentCOMOffset  (vsubfp v0 = v1 - v13).
    //   mMeshOffset            -= delta
    //   each sensor.mInitialOffset            += delta
    //   each tag-point initial position xyz   += delta   (w detach-threshold lane preserved, vrlimi128)
    //   each driven-point position            += delta
    //   each IK-part joint vector             += delta
    //   mCurrentCOMOffset                      = lCOMOffset   (stvx128 v1 -> +1632)
    void StreamedDeformationSpec::TransformToNewCOMSpace(Vector3 lCOMOffset)
    {
        // lOldToNewCOMOffset = mCurrentCOMOffset - lCOMOffset ; IsZero(|.|^2) test.
        const Vector3 lOldMinusNew = {
            mCurrentCOMOffset.x - lCOMOffset.x,
            mCurrentCOMOffset.y - lCOMOffset.y,
            mCurrentCOMOffset.z - lCOMOffset.z,
            0.0f
        };
        const f32 lfMagnitudeSquared =
            lOldMinusNew.x * lOldMinusNew.x +
            lOldMinusNew.y * lOldMinusNew.y +
            lOldMinusNew.z * lOldMinusNew.z;

        // back_chain[0] = (magSq > epsilon)  ->  the COM moved.
        if ( lfMagnitudeSquared > KF_COM_MOVE_DEADBAND_SQ_PLACEHOLDER )
        {
            // delta = lCOMOffset - mCurrentCOMOffset (v0 = v1 - v13).
            const Vector3 lDelta = {
                lCOMOffset.x - mCurrentCOMOffset.x,
                lCOMOffset.y - mCurrentCOMOffset.y,
                lCOMOffset.z - mCurrentCOMOffset.z,
                lCOMOffset.w - mCurrentCOMOffset.w
            };

            // mMeshOffset -= delta.
            SubVector(mMeshOffset, lDelta);

            // Every deformation sensor's initial offset += delta.
            if ( mu8NumDeformationSensors )
            {
                s32 liIndex = 0;
                do
                {
                    AddVector(maDeformationSensorSpecs[liIndex].mInitialOffset, lDelta);
                    ++liIndex;
                }
                while ( liIndex < mu8NumDeformationSensors );
            }

            // Every tag-point initial position xyz += delta; the w (detach-threshold) lane is
            // preserved (the asm reloads the original vector and vrlimi128's the xyz back in).
            if ( miNumberOfTagPoints > 0 )
            {
                s32 liIndex = 0;
                char* lpTagBase = reinterpret_cast<char*>(maTagPointData.Get());
                do
                {
                    f32* lpPos = reinterpret_cast<f32*>(
                        lpTagBase + KU_TAG_POINT_STRIDE * liIndex + KU_TAG_POINT_POS_OFFSET);
                    lpPos[0] += lDelta.x;
                    lpPos[1] += lDelta.y;
                    lpPos[2] += lDelta.z;
                    // lpPos[3] (detach threshold) intentionally left unchanged (vrlimi128 w-preserve).
                    ++liIndex;
                }
                while ( liIndex < miNumberOfTagPoints );
            }

            // Every driven-point position += delta.
            if ( miNumberOfDrivenPoints > 0 )
            {
                s32 liIndex = 0;
                char* lpDrivenBase = reinterpret_cast<char*>(maDrivenPointData.Get());
                do
                {
                    f32* lpPos = reinterpret_cast<f32*>(lpDrivenBase + KU_DRIVEN_POINT_STRIDE * liIndex);
                    lpPos[0] += lDelta.x;
                    lpPos[1] += lDelta.y;
                    lpPos[2] += lDelta.z;
                    lpPos[3] += lDelta.w;
                    ++liIndex;
                }
                while ( liIndex < miNumberOfDrivenPoints );
            }

            // Every joint vector of every IK part += delta.
            if ( miNumberOfIKParts > 0 )
            {
                s32 liPart = 0;
                char* lpPartBase = reinterpret_cast<char*>(maIKPartData.Get());
                do
                {
                    char* lpPart      = lpPartBase + KU_IK_PART_STRIDE * liPart;
                    const s32 liNumJoints = *reinterpret_cast<const s32*>(lpPart + KU_IK_PART_JOINTS_COUNT);
                    if ( liNumJoints > 0 )
                    {
                        char* lpJointBase = *reinterpret_cast<char**>(lpPart + KU_IK_PART_JOINTS_PTR);
                        s32 liJoint = 0;
                        do
                        {
                            f32* lpJoint = reinterpret_cast<f32*>(lpJointBase + KU_JOINT_STRIDE * liJoint);
                            lpJoint[0] += lDelta.x;
                            lpJoint[1] += lDelta.y;
                            lpJoint[2] += lDelta.z;
                            lpJoint[3] += lDelta.w;
                            ++liJoint;
                        }
                        while ( liJoint < liNumJoints );
                    }
                    ++liPart;
                }
                while ( liPart < miNumberOfIKParts );
            }

            // mCurrentCOMOffset = lCOMOffset (stvx128 v1 -> +1632).
            mCurrentCOMOffset = lCOMOffset;
        }
    }

    // X360 @ 0x82631118. Serialise-out pointer relocation: rebase every embedded absolute pointer to
    // a base-relative offset by subtracting lpBaseAddress. Null pointers are preserved as null (the
    // asm guards each subtract with a "!= 0" test before clobbering with 0). The per-IK-part embedded
    // joint-spec pointer at +448 is rebased the same way. The empty count-walks over the three locator
    // lists are reproduced (they have no observable store, matching the inlined LocatorPointSpecList
    // fix-down whose per-element body is a no-op).
    void StreamedDeformationSpec::FixDown(void* lpBaseAddress)
    {
        const ptrdiff_t liBase = reinterpret_cast<ptrdiff_t>(lpBaseAddress);

        // Each IK part's embedded joint-spec pointer (+448): rebase, preserving null.
        if ( miNumberOfIKParts > 0 )
        {
            s32 liIndex = 0;
            char* lpPartBase = reinterpret_cast<char*>(maIKPartData.Get());
            do
            {
                char**    lppJoints = reinterpret_cast<char**>(
                    lpPartBase + KU_IK_PART_STRIDE * liIndex + KU_IK_PART_JOINTS_PTR);
                ptrdiff_t liPtr = reinterpret_cast<ptrdiff_t>(*lppJoints);
                *lppJoints = (liPtr != 0)
                    ? reinterpret_cast<char*>(liPtr - liBase)
                    : reinterpret_cast<char*>(0);
                ++liIndex;
            }
            while ( liIndex < miNumberOfIKParts );
        }

        // mGenericTags: vestigial count-walk (no store), then rebase the array pointer (preserve null).
        for ( u32 luIndex = 0; luIndex < mGenericTags.muNumLocators; ++luIndex ) { /* no-op */ }
        {
            // [x64] the slot is FOUR bytes (see the header banner): rebase the SLOT, never a
            // host pointer, or the store truncates a widened value back into it.
            const u32 luSlot = mGenericTags.mpaLocatorPoints.muSlot;
            mGenericTags.mpaLocatorPoints.muSlot = (luSlot != 0)
                ? static_cast<u32>(luSlot - static_cast<u32>(liBase))
                : 0u;
        }

        // mLightTags (FixDown order: Generic, Light, Camera).
        for ( u32 luIndex = 0; luIndex < mLightTags.muNumLocators; ++luIndex ) { /* no-op */ }
        {
            // [x64] the slot is FOUR bytes (see the header banner): rebase the SLOT, never a
            // host pointer, or the store truncates a widened value back into it.
            const u32 luSlot = mLightTags.mpaLocatorPoints.muSlot;
            mLightTags.mpaLocatorPoints.muSlot = (luSlot != 0)
                ? static_cast<u32>(luSlot - static_cast<u32>(liBase))
                : 0u;
        }

        // mCameraTags.
        for ( u32 luIndex = 0; luIndex < mCameraTags.muNumLocators; ++luIndex ) { /* no-op */ }
        {
            // [x64] the slot is FOUR bytes (see the header banner): rebase the SLOT, never a
            // host pointer, or the store truncates a widened value back into it.
            const u32 luSlot = mCameraTags.mpaLocatorPoints.muSlot;
            mCameraTags.mpaLocatorPoints.muSlot = (luSlot != 0)
                ? static_cast<u32>(luSlot - static_cast<u32>(liBase))
                : 0u;
        }

        // The four top-level table pointers: unconditional subtract (no null guard in the asm).
        maIKPartData.muSlot = static_cast<u32>(maIKPartData.muSlot - static_cast<u32>(liBase));
        maDrivenPointData.muSlot = static_cast<u32>(maDrivenPointData.muSlot - static_cast<u32>(liBase));
        maGlassPaneData.muSlot = static_cast<u32>(maGlassPaneData.muSlot - static_cast<u32>(liBase));
        maTagPointData.muSlot = static_cast<u32>(maTagPointData.muSlot - static_cast<u32>(liBase));
    }

    // X360 @ 0x82630E18. Serialise-in pointer relocation: the inverse of FixDown. Add lpBaseAddress
    // to every embedded base-relative offset to recover an absolute pointer; null offsets are
    // preserved as null. After rebasing, every IK part has its bounding-box handedness re-checked,
    // and every locator's miIkPartIndex is bounds-checked against miNumberOfIKParts.
    void StreamedDeformationSpec::FixUp(void* lpBaseAddress)
    {
        const ptrdiff_t liBase = reinterpret_cast<ptrdiff_t>(lpBaseAddress);

        // The four unconditionally-rebased top-level table pointers (no null guard in the asm):
        // asm computes v5=IK, v6=Driven, v7=Glass, then result[1]+=a2 for Tag.
        maIKPartData.muSlot = static_cast<u32>(maIKPartData.muSlot + static_cast<u32>(liBase));
        maDrivenPointData.muSlot = static_cast<u32>(maDrivenPointData.muSlot + static_cast<u32>(liBase));
        maGlassPaneData.muSlot = static_cast<u32>(maGlassPaneData.muSlot + static_cast<u32>(liBase));
        maTagPointData.muSlot = static_cast<u32>(maTagPointData.muSlot + static_cast<u32>(liBase));

        // mGenericTags: rebase the list pointer (preserve null), then the vestigial count-walk.
        {
            // [x64] the slot is FOUR bytes (see the header banner): rebase the SLOT, never a
            // host pointer, or the store truncates a widened value back into it.
            const u32 luSlot = mGenericTags.mpaLocatorPoints.muSlot;
            mGenericTags.mpaLocatorPoints.muSlot = (luSlot != 0)
                ? static_cast<u32>(luSlot + static_cast<u32>(liBase))
                : 0u;
        }
        for ( u32 luIndex = 0; luIndex < mGenericTags.muNumLocators; ++luIndex ) { /* no-op */ }

        // mLightTags (FixUp order: Generic, Light, Camera).
        {
            // [x64] the slot is FOUR bytes (see the header banner): rebase the SLOT, never a
            // host pointer, or the store truncates a widened value back into it.
            const u32 luSlot = mLightTags.mpaLocatorPoints.muSlot;
            mLightTags.mpaLocatorPoints.muSlot = (luSlot != 0)
                ? static_cast<u32>(luSlot + static_cast<u32>(liBase))
                : 0u;
        }
        for ( u32 luIndex = 0; luIndex < mLightTags.muNumLocators; ++luIndex ) { /* no-op */ }

        // mCameraTags.
        {
            // [x64] the slot is FOUR bytes (see the header banner): rebase the SLOT, never a
            // host pointer, or the store truncates a widened value back into it.
            const u32 luSlot = mCameraTags.mpaLocatorPoints.muSlot;
            mCameraTags.mpaLocatorPoints.muSlot = (luSlot != 0)
                ? static_cast<u32>(luSlot + static_cast<u32>(liBase))
                : 0u;
        }
        for ( u32 luIndex = 0; luIndex < mCameraTags.muNumLocators; ++luIndex ) { /* no-op */ }

        // Each IK part: rebase its embedded joint-spec pointer (+448, preserve null), then re-run the
        // body-part bounding-box handedness fix-up on its BodyPartBBoxSpec (+64).
        if ( miNumberOfIKParts > 0 )
        {
            s32 liIndex = 0;
            char* lpPartBase = reinterpret_cast<char*>(maIKPartData.Get());
            do
            {
                char*  lpPart    = lpPartBase + KU_IK_PART_STRIDE * liIndex;
                char** lppJoints = reinterpret_cast<char**>(lpPart + KU_IK_PART_JOINTS_PTR);
                ptrdiff_t liPtr = reinterpret_cast<ptrdiff_t>(*lppJoints);
                *lppJoints = (liPtr != 0)
                    ? reinterpret_cast<char*>(liPtr + liBase)
                    : reinterpret_cast<char*>(0);

                reinterpret_cast<BodyPartBBoxSpec*>(lpPart + KU_IK_PART_BBOX_OFFSET)->HackCheckHandedness();
                ++liIndex;
            }
            while ( liIndex < miNumberOfIKParts );
        }

        // Bounds-check every locator's IK-part index against miNumberOfIKParts. The three checks are
        // non-gating tripwires (BeginAssert/FireAssert/EndAssert, execution continues) and run in the
        // FixUp order Generic, Light, Camera (matching the asm's three assert loops at :340/:347/:354).
        {
            char* lpBase = reinterpret_cast<char*>(mGenericTags.mpaLocatorPoints.Get());
            for ( u32 luLoop = 0; luLoop < mGenericTags.muNumLocators; ++luLoop )
            {
                const s16 li16IkPartIndex =
                    *reinterpret_cast<const s16*>(lpBase + KU_LOCATOR_STRIDE * luLoop + KU_LOCATOR_IKPART_OFFSET);
                CGS_ASSERT(li16IkPartIndex < miNumberOfIKParts,
                           "lpSpecList->GetLocatorSpec(liLoop)->miIkPartIndex < miNumberOfIKParts");
            }
        }
        {
            char* lpBase = reinterpret_cast<char*>(mLightTags.mpaLocatorPoints.Get());
            for ( u32 luLoop = 0; luLoop < mLightTags.muNumLocators; ++luLoop )
            {
                const s16 li16IkPartIndex =
                    *reinterpret_cast<const s16*>(lpBase + KU_LOCATOR_STRIDE * luLoop + KU_LOCATOR_IKPART_OFFSET);
                CGS_ASSERT(li16IkPartIndex < miNumberOfIKParts,
                           "lpSpecList->GetLocatorSpec(liLoop)->miIkPartIndex < miNumberOfIKParts");
            }
        }
        {
            char* lpBase = reinterpret_cast<char*>(mCameraTags.mpaLocatorPoints.Get());
            for ( u32 luLoop = 0; luLoop < mCameraTags.muNumLocators; ++luLoop )
            {
                const s16 li16IkPartIndex =
                    *reinterpret_cast<const s16*>(lpBase + KU_LOCATOR_STRIDE * luLoop + KU_LOCATOR_IKPART_OFFSET);
                CGS_ASSERT(li16IkPartIndex < miNumberOfIKParts,
                           "lpSpecList->GetLocatorSpec(liLoop)->miIkPartIndex < miNumberOfIKParts");
            }
        }
    }
}
}
