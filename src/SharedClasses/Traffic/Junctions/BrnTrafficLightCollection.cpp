// ============================================================================
// BrnTrafficLightCollection.cpp
//
// Bodies for BrnTraffic::TrafficLightCollection -- the read-only, fixed-up view
// over a track's baked traffic-light corona data. Reconstructed store-for-store
// from BURNOUT_X360_ARTIST.XEX:
//   CalcInstanceTransform             @ 0x82753910
//   CalcArbitraryAmberCoronaTransform @ 0x82757478
//   GetCoronaPosition                 @ 0x82753820
//   GetCoronaState                    @ 0x8274F510
//   GetInstanceIndexForInstanceID     @ 0x8274F590
//   GetInstanceType                   @ 0x8274F438
//   GetTrafficLightType               @ 0x8274F4A0
//   BrnTraffic::ExpandPosPlusYRotToTransform @ 0x823610B8
//                                     declared in BrnTrafficSharedMaths.h and defined here
//                                     (see its own banner below)
//
// The type + member offsets are homed in BrnTrafficLightCollection.h. Assert
// strings are the X360 rodata literals, verbatim (file/line args dropped).
// ============================================================================

#include "SharedClasses/Traffic/Junctions/BrnTrafficLightCollection.h"

#include "GameShared/GameClasses/Development/CgsStrStream.h"   // CgsDev::StrStream
#include "rw/math/vpu/vector3_operation.h"                    // IsValid
#include "rw/math/vpu/matrix44affine_operation.h"             // TransformPoint

#include <cmath>                                              // std::floor

namespace BrnTraffic
{
    namespace
    {
        // ARTIST 0x823611A8..0x8236135C uses the SDK VMX SinCos kernel.
        void TrafficSinCos(f32 lfAngle, f32& lrfSin, f32& lrfCos)
        {
            XMVECTOR lSin, lCos;
            XMVectorSinCos(&lSin, &lCos, XMVectorReplicate(lfAngle));
            lrfSin = lSin.x;
            lrfCos = lCos.x;
        }

    }

    // -- BrnTraffic::ExpandPosPlusYRotToTransform @ 0x823610B8 (183 insns) ---------
    //
    // ⚠️ HOME. The DWARF declares this in SharedClasses/Traffic/BrnTrafficSharedMaths.h:81
    //    (`extern Matrix44Affine ExpandPosPlusYRotToTransform(Vector3Plus)`), and the
    //    console's own baked assert file literal agrees verbatim:
    //    "..\..\..\SharedClasses\Traffic/BrnTrafficSharedMaths.h" (rodata 0x8202D714),
    //    with the two assert lines 0x53 == 83 and 0x54 == 84 -- i.e. the body is defined
    //    in that header, two lines below its declaration. The declaration is homed in
    //    BrnTrafficSharedMaths.h; this out-of-line body remains beside its collection
    //    caller. The trigger caller also uses that shared declaration.
    //
    //
    //
    // SIGNATURE. DecFIGS declares Vector3Plus by value. ARTIST passes that packed value
    //    in v1 and supplies the Matrix44Affine result pointer in r3. Both real callers
    //    follow that convention; the genuine SDK Vector3Plus supplies the value ABI.
    //
    //
    //
    // WHAT IT DOES: expand a packed (position.xyz, yRotation.w) into the affine that
    // places a traffic-light instance -- a rotation about Y by the packed angle, with the
    // packed vector itself as the translation row.
    //
    // MEASURED, store for store (asm 0x823610B8..0x82361390):
    //   0x823610C8..0x8236115C  IsValid(lPosPlusYRot.GetVector3()) -- three `vspltw`+
    //       `vcmpeqfp.` self-equality NaN tests on lanes 0/1/2, AND'ed, assert line 0x53.
    //   0x82361168..0x8236119C  IsValid(lPosPlusYRot.GetPlus())    -- the same test on
    //       lane 3, assert line 0x54.  (TWO asserts, not four: the three lane compares of
    //       the first are one IsValid(Vector3).)
    //   0x823611A8..0x8236135C  ONE inlined XMVectorSinCos of lane 3 -- see TrafficSinCos.
    //   0x82361340..0x82361388  the row assembly, all through one permute control vector
    //       at 0x82CDA350 = { 00 01 02 03 | 14 15 16 17 | 00 01 02 03 | 00 01 02 03 },
    //       i.e. vperm(A,B,mask) == { A.x, B.y, A.x, A.x }, each row then getting its z
    //       lane overwritten by `vrlimi128 <dst>, <src>, 2, 0` (mask 2 == the z lane):
    //         yAxis (+0x10) = vperm(0, 1.0)         -> ( 0,  1,  0,  0 )
    //         zAxis (+0x20) = vperm(sin, 0), z<-cos -> ( sin, 0, cos, sin )
    //         xAxis (+0x00) = vperm(cos, 0), z<--sin-> ( cos, 0, -sin, cos )
    //         wAxis (+0x30) = lPosPlusYRot           (`stvx128 v1,r0,r11` @0x82361388)
    //       The sign flip is `vxor v13,v13,v12` @0x82361364 against the 0x80000000 mask
    //       `vspltisw v12,-1 ; vslw v12,v12,v12`.
    //
    // ⚠️ THE W LANES OF xAxis AND zAxis ARE NOT ZERO. The permute leaves A.x in lanes 0, 2
    //    and 3 and only lane 2 is then overwritten, so the console really does store
    //    xAxis.w == cos and zAxis.w == sin. Reproduced rather than tidied: an affine's row
    //    w lanes are don't-care to every consumer here, but "faithful" means faithful.
    // ⚠️ The console ALSO stores a zero vector to +0x30 at 0x82361370, before overwriting
    //    it with the input at 0x82361388 -- a dead store from the row-assembly macro.
    //    Only the surviving value is written here.
    Matrix44Affine ExpandPosPlusYRotToTransform(Vector3Plus lPosPlusYRot)
    {
        CGS_ASSERT(rw::math::vpu::IsValid(lPosPlusYRot.GetVector3()),
                   "RwMath::IsValid( lPosPlusYRot.GetVector3() )");   // baked line 83
        // The SDK's SCALAR IsValid has no home in this tree (rw::math::vpu only homes the
        // Vector3 / Quaternion forms, and growing that vendor header is out of this
        // cluster's ownership), so the console's `vcmpeqfp.` self-equality NaN test on
        // lane 3 is written out directly. Same predicate, same assert string.
        CGS_ASSERT(lPosPlusYRot.GetPlus() == lPosPlusYRot.GetPlus(),
                   "RwMath::IsValid( lPosPlusYRot.GetPlus() )");      // baked line 84

        f32 lfSin = 0.0f;
        f32 lfCos = 0.0f;
        TrafficSinCos(lPosPlusYRot.GetPlus(), lfSin, lfCos);

        Matrix44Affine lResult;
        lResult.xAxis = Vector3(XMVectorSet(lfCos, 0.0f, -lfSin, lfCos));
        lResult.yAxis = Vector3(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
        lResult.zAxis = Vector3(XMVectorSet(lfSin, 0.0f, lfCos, lfSin));
        lResult.wAxis = Vector3(__lvx(&lPosPlusYRot, 0));
        return lResult;
    }

    // -- CalcInstanceTransform @ 0x82753910 ---------------------------------------
    Matrix44Affine TrafficLightCollection::CalcInstanceTransform(u32 luInstance) const
    {
        CGS_ASSERT(luInstance < muNumTrafficLights, "luInstance < muNumTrafficLights");
        CGS_ASSERT(mpaPosAndYRotations, "mpaPosAndYRotations");

        return ExpandPosPlusYRotToTransform(mpaPosAndYRotations[luInstance]);
    }

    // -- CalcArbitraryAmberCoronaTransform @ 0x82757478 ---------------------------
    // World transform of the given instance's first AMBER corona: the instance's own
    // rotation with the corona's world position in the translation row. Falls back to
    // identity (and asserts, with the instance id streamed into the message) when the
    // instance has no amber corona.
    const Matrix44Affine TrafficLightCollection::CalcArbitraryAmberCoronaTransform(u32 luInstance) const
    {
        CGS_ASSERT(luInstance < muNumTrafficLights, "luInstance < muNumTrafficLights");

        const Matrix44Affine lInstanceTransform = CalcInstanceTransform(luInstance);

        const u32 luType = GetInstanceType(luInstance);
        const TrafficLightType* lpType = GetTrafficLightType(luType);

        const u32 luGlobalCoronaBegin = lpType->muCoronaOffset;
        const u32 luGlobalCoronaEnd   = static_cast<u32>(lpType->muCoronaOffset) + lpType->muNumCoronas;

        for (u32 luGlobalCorona = luGlobalCoronaBegin; luGlobalCorona < luGlobalCoronaEnd; ++luGlobalCorona)
        {
            CGS_ASSERT(luGlobalCorona < muNumCoronas, "luCorona < muNumCoronas");
            CGS_ASSERT(mpaCoronaTypes[luGlobalCorona] < E_TRAFFICLIGHTSTATE_COUNT,
                       "mpaCoronaTypes[luCorona] < E_TRAFFICLIGHTSTATE_COUNT");

            if (GetCoronaState(luGlobalCorona) == E_TRAFFICLIGHTSTATE_AMBER)
            {
                const Vector3 lLocalCoronaPos = GetCoronaPosition(luGlobalCorona);

                // Copy the instance rotation rows; set the translation row to the corona
                // world position (TransformPoint = xAxis*x + yAxis*y + zAxis*z + wAxis).
                Matrix44Affine lResult(lInstanceTransform);
                lResult.Pos() = rw::math::vpu::TransformPoint(lInstanceTransform, lLocalCoronaPos);
                return lResult;
            }
        }

        // No amber corona on this instance: fire the assert with the instance id, then
        // fall back to the identity transform.
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "No amber light on instance " << luInstance;
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }

        Matrix44Affine lIdentity;
        lIdentity.SetIdentity();
        return lIdentity;
    }

    // -- GetCoronaPosition @ 0x82753820 -------------------------------------------
    Vector3 TrafficLightCollection::GetCoronaPosition(u32 luCorona) const
    {
        CGS_ASSERT(luCorona < muNumCoronas, "luCorona < muNumCoronas");
        CGS_ASSERT(rw::math::vpu::IsValid(mpaCoronaPositions[luCorona]),
                   "RwMath::IsValid( mpaCoronaPositions[luCorona] )");

        return mpaCoronaPositions[luCorona];
    }

    // -- GetCoronaState @ 0x8274F510 (32 insns) -----------------------------------
    // NOT a console header inline as this header's own
    // banner assumed: it is a real out-of-line symbol, found via xrefs_from of
    // RenderCoronasForInstance @0x827571B8 and dumped with headless idat on a private
    // .i64 copy (scratchpad/waveQ7/ida_q7t/). It has no .ida-exports JSON and no ledger
    // row -- an exporter-run gap. Its two baked assert lines 0xDE == 222 and 0xDF == 223
    // are already listed in this header's banner, which is what confirms the home.
    //   lhz  r11,4(this) ; cmplw luCorona,r11 ; blt -> assert "luCorona < muNumCoronas"  :222
    //   lwz  r11,0x18(this) ; lbzx r11,r11,luCorona ; cmplwi r11,3 ; blt
    //        -> assert "mpaCoronaTypes[luCorona] < E_TRAFFICLIGHTSTATE_COUNT"            :223
    //   lwz  r11,0x18(this) ; lbzx r3,r11,luCorona ; return
    // The console RE-LOADS mpaCoronaTypes for the return (0x8274F554 and 0x8274F580 are
    // two separate `lwz 0x18`), so the two subscripts are written out rather than cached.
    ETrafficLightState TrafficLightCollection::GetCoronaState(u32 luCorona) const
    {
        CGS_ASSERT(luCorona < muNumCoronas, "luCorona < muNumCoronas");
        CGS_ASSERT(mpaCoronaTypes[luCorona] < E_TRAFFICLIGHTSTATE_COUNT,
                   "mpaCoronaTypes[luCorona] < E_TRAFFICLIGHTSTATE_COUNT");

        return static_cast<ETrafficLightState>(mpaCoronaTypes[luCorona]);
    }

    // -- GetInstanceIndexForInstanceID @ 0x8274F590 -------------------------------
    // Resolve a persistent instance ID to its dense index via the ID hash table.
    // Returns -1 if the ID is not present. mauInstanceHashOffsets is a u16[129] at
    // member offset 0x20 (u16 index 16), which is why the asm addresses the two
    // bucket offsets as this[luHash+16]/[luHash+17].
    s32 TrafficLightCollection::GetInstanceIndexForInstanceID(u32 luInstanceID) const
    {
        const u32 luHash = luInstanceID & KU_INSTANCE_ID_HASH_MASK;
        CGS_ASSERT((luHash + 1) < KU_INSTANCE_ID_HASH_TABLE_SIZE,
                   "(luHash + 1) < KU_INSTANCE_ID_HASH_TABLE_SIZE");

        const u32 luBeginIndex = mauInstanceHashOffsets[luHash];
        const u32 luEndIndex   = mauInstanceHashOffsets[luHash + 1];
        CGS_ASSERT(luEndIndex >= luBeginIndex, "luEndIndex >= luBeginIndex");
        CGS_ASSERT(luEndIndex <= muNumTrafficLights, "luEndIndex <= muNumTrafficLights");

        for (u32 luIndex = luBeginIndex; luIndex < luEndIndex; ++luIndex)
        {
            if (mpauInstanceHashTable[luIndex] == luInstanceID)
            {
                return mpauInstanceHashToIndexLookup[luIndex];
            }
        }

        return -1;
    }

    // -- GetInstanceType @ 0x8274F438 ---------------------------------------------
    u32 TrafficLightCollection::GetInstanceType(u32 luInstance) const
    {
        CGS_ASSERT(luInstance < muNumTrafficLights, "luInstance < muNumTrafficLights");

        return mpauInstanceTypes[luInstance];
    }

    // -- GetTrafficLightType @ 0x8274F4A0 -----------------------------------------
    const TrafficLightType* TrafficLightCollection::GetTrafficLightType(u32 luType) const
    {
        CGS_ASSERT(luType < muNumTrafficLightTypes, "luType < muNumTrafficLightTypes");

        return &mpaTrafficLightTypes[luType];
    }

    // -- RenderCoronasForInstance @ 0x827571B8 ------------------------------------
    // Submit this instance's coronas (one per active-state corona) to the world
    // corona buffer. Back-face + distance culled against the camera, with a gentle
    // distance-based size boost so distant lights stay legible.
    void TrafficLightCollection::RenderCoronasForInstance(
        u32 luInstance,
        u32 luActiveStates,
        BrnCoronaManager::BrnSubmissionInterface* lpCoronaSubmissionInterface,
        Vector3 lCameraPosition,
        Vector3 lCameraDirection,
        VecFloat lfCullDistSq) const
    {
        // corona colour state -> corona archetype (RED->Red=7, AMBER->Amber=6, GREEN->Green=5).
        static const BrnCoronaType KAE_STATE_TO_CORONATYPE_MAPPING[E_TRAFFICLIGHTSTATE_COUNT] =
        {
            eCoronaTypeTrafficLightRed,    // E_TRAFFICLIGHTSTATE_RED
            eCoronaTypeTrafficLightAmber,  // E_TRAFFICLIGHTSTATE_AMBER
            eCoronaTypeTrafficLightGreen,  // E_TRAFFICLIGHTSTATE_GREEN
        };
        // corona colour state -> active-mask bit (1 << state); AND'd with luActiveStates to gate.
        static const u8 KAU_TRAFFICLIGHTSTATE_BITS[E_TRAFFICLIGHTSTATE_COUNT] = { 1, 2, 4 };

        CGS_ASSERT(luInstance < muNumTrafficLights, "luInstance < muNumTrafficLights");

        const Matrix44Affine lInstanceTransform = CalcInstanceTransform(luInstance);

        // Camera -> instance delta (the affine's translation row is the instance world pos).
        const Vector3 lDir = rw::math::vpu::operator-(lInstanceTransform.Pos(), lCameraPosition);

        // Back-face cull: skip the instance when it faces away from the camera view direction.
        if (rw::math::vpu::Dot(lCameraDirection, lDir) <= 0.0f)
            return;

        // Distance cull.
        if (rw::math::vpu::MagnitudeSquared(lDir) > static_cast<f32>(lfCullDistSq.mV.x))
            return;

        // Distance-based size boost: scale = Lerp(1, 2, max(dist - 8, 0) * K).
        // K is the console's lazily-cached broadcast of 1/142 (flt_820BE730).
        static const f32 KF_CORONA_SCALE_CONSTANT = 0.0070422534f;
        const f32 lfDistFromCamera = rw::math::vpu::Magnitude(lDir);
        const f32 lfClampedDist    = (lfDistFromCamera - 8.0f) > 0.0f ? (lfDistFromCamera - 8.0f) : 0.0f;
        const f32 lfSizeParam      = lfClampedDist * KF_CORONA_SCALE_CONSTANT;
        // Lerp(1.0f, 2.0f, lfSizeParam) == 1.0f + (2.0f - 1.0f) * lfSizeParam.
        const f32 lfLightScale     = 1.0f + (2.0f - 1.0f) * lfSizeParam;

        const u32 luType = GetInstanceType(luInstance);
        const TrafficLightType* lpType = GetTrafficLightType(luType);

        const u32 luGlobalCoronaBegin = lpType->muCoronaOffset;
        const u32 luGlobalCoronaEnd   = static_cast<u32>(lpType->muCoronaOffset) + lpType->muNumCoronas;

        for (u32 luGlobalCorona = luGlobalCoronaBegin; luGlobalCorona < luGlobalCoronaEnd; ++luGlobalCorona)
        {
            const ETrafficLightState leState = GetCoronaState(luGlobalCorona);
            if ((KAU_TRAFFICLIGHTSTATE_BITS[leState] & luActiveStates) != 0)
            {
                const Vector3 lLocalCoronaPos = GetCoronaPosition(luGlobalCorona);
                const ETrafficLightState leCoronaState = GetCoronaState(luGlobalCorona);

                const Vector3 lCoronaPosition =
                    rw::math::vpu::TransformPoint(lInstanceTransform, lLocalCoronaPos);

                // Direction = the instance's forward (transform zAxis); opacity fixed at 1.0f.
                lpCoronaSubmissionInterface->AddCorona(
                    lCoronaPosition,
                    lInstanceTransform.zAxis,
                    lfLightScale,
                    1.0f,
                    KAE_STATE_TO_CORONATYPE_MAPPING[leCoronaState]);
            }
        }
    }
}
