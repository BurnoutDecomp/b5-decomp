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
//   BrnTraffic::ExpandPosPlusYRotToTransform @ 0x823610B8  (
//                                     a FREE function whose DWARF home is a header this
//                                     tree has no mirror of; see its own banner below)
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
        // ====================================================================
        // The console's shared VMX SinCos kernel, de-SIMD'd.
        //
        // ExpandPosPlusYRotToTransform inlines the X360 SDK's XMVectorSinCos: a
        // 2*pi range reduction followed by two twelve-term series over the reduced
        // angle, all four-lane-splatted so every lane carries the same scalar. The
        // coefficient rows live in the image at 0x82000BD0/BE0/BF0 (sin) and
        // 0x82000C00/C10/C20 (cos), with 0x82000C60 = { pi, 2*pi, 1/pi, 1/(2*pi) };
        // the function splats 0x82000C64 (2*pi) and 0x82000C6C (1/(2*pi)).
        // EVERY VALUE BELOW WAS DUMPED FROM THE IMAGE (headless idat on a private
        // .i64 copy, scratchpad/waveQ7/ida_q7t/out_expand.json) -- the hex word is
        // quoted beside each float so the decimal spelling cannot drift.
        //
        // ⚠️ THIS IS THE SECOND COPY OF THIS KERNEL IN THE TREE. The first is
        //    `SvpSinCos` in GameSource/Physics/VehicleManager/VehiclePhysics/
        //    BrnSimpleVehiclePhysics.cpp, TU-static for the same reason: the
        //    committed rw::math::vpu headers do not home SinCos, and neither this
        //    TU (SharedClasses) nor that one (GameSource) may grow the vendor
        //    header. The two tables are bit-identical by construction -- both are
        //    the same image rows. FOLD THEM into a single rw::math::vpu home the
        //    day someone owns that header.
        //
        // DIVERGENCES, both inherited from the sibling and both stated on purpose:
        //   * the console's range reduction is `vrfin` (round-to-nearest, ties to
        //     even); this is `floor(x + 0.5f)`, which differs only on an exact .5
        //     tie of an already-inexact quotient;
        //   * the console evaluates each series as a splat-and-madd lattice of
        //     independent powers (x^3, x^5, ... x^23 built by vmulfp128 pairs);
        //     this is Horner over x^2. Same terms, same coefficients,
        //     float-rounding-equivalent order.
        // Not sinf/cosf: reproducing the console's own polynomial keeps the traffic
        // light transforms numerically on the console's curve, not the CRT's.
        // ====================================================================
        const f32 KAF_SIN_COEFFS[12] =
        {
            1.0f,             // 0x3F800000
            -1.66666672e-1f,  // 0xBE2AAAAB   -1/3!
            8.33333377e-3f,   // 0x3C088889    1/5!
            -1.98412701e-4f,  // 0xB9500D01   -1/7!
            2.75573188e-6f,   // 0x3638EF1D    1/9!
            -2.50521079e-8f,  // 0xB2D7322B   -1/11!
            1.60590444e-10f,  // 0x2F309231    1/13!
            -7.64716361e-13f, // 0xAB573F9F   -1/15!
            2.81145736e-15f,  // 0x274A963C    1/17!
            -8.22063508e-18f, // 0xA317A4DA   -1/19!
            1.95729415e-20f,  // 0x1EB8DC78    1/21!
            -3.86817030e-23f  // 0x9A3B0DA1   -1/23!
        };
        const f32 KAF_COS_COEFFS[12] =
        {
            1.0f,             // 0x3F800000
            -5.0e-1f,         // 0xBF000000   -1/2!
            4.16666679e-2f,   // 0x3D2AAAAB    1/4!
            -1.38888892e-3f,  // 0xBAB60B61   -1/6!
            2.48015876e-5f,   // 0x37D00D01    1/8!
            -2.75573200e-7f,  // 0xB493F27E   -1/10!
            2.08767581e-9f,   // 0x310F76C8    1/12!
            -1.14707454e-11f, // 0xAD49CBA5   -1/14!
            4.77947726e-14f,  // 0x29573F9F    1/16!
            -1.56192068e-16f, // 0xA53413C3   -1/18!
            4.11031759e-19f,  // 0x20F2A15D    1/20!
            -8.89679096e-22f  // 0x9C8671CB   -1/22!
        };
        const f32 KF_TWO_PI     = 6.28318548f;   // 0x40C90FDB @0x82000C64
        const f32 KF_INV_TWO_PI = 0.159154937f;  // 0x3E22F983 @0x82000C6C

        void TrafficSinCos(f32 lfAngle, f32& lrfSin, f32& lrfCos)
        {
            // XMVectorModAngles: r = angle - 2*pi * round(angle / (2*pi)).
            // asm: `vmulfp128 v0,v12,v11` (angle * 1/2pi), `vrfin v0,v0`,
            //      `vnmsubfp v8,<2pi>,<round>,<angle>` at 0x823611E0..0x82361200.
            const f32 lfR  = lfAngle - KF_TWO_PI * std::floor(lfAngle * KF_INV_TWO_PI + 0.5f);
            const f32 lfR2 = lfR * lfR;

            f32 lfSinPoly = KAF_SIN_COEFFS[11];
            f32 lfCosPoly = KAF_COS_COEFFS[11];
            for (s32 li = 10; li >= 0; --li)
            {
                lfSinPoly = lfSinPoly * lfR2 + KAF_SIN_COEFFS[li];
                lfCosPoly = lfCosPoly * lfR2 + KAF_COS_COEFFS[li];
            }

            lrfSin = lfR * lfSinPoly;
            lrfCos = lfCosPoly;
        }
    }

    // -- BrnTraffic::ExpandPosPlusYRotToTransform @ 0x823610B8 (183 insns) ---------
    //
    // ⚠️ HOME. The DWARF declares this in SharedClasses/Traffic/BrnTrafficSharedMaths.h:81
    //    (`extern Matrix44Affine ExpandPosPlusYRotToTransform(Vector3Plus)`), and the
    //    console's own baked assert file literal agrees verbatim:
    //    "..\..\..\SharedClasses\Traffic/BrnTrafficSharedMaths.h" (rodata 0x8202D714),
    //    with the two assert lines 0x53 == 83 and 0x54 == 84 -- i.e. the body is defined
    //    in that header, two lines below its declaration. THAT HEADER HAS NO MIRROR IN
    //    THIS TREE. The body lands here instead, beside its only in-tree caller
    //    (CalcInstanceTransform), and the declaration it satisfies is the one already
    //    committed in BrnTrafficLightCollection.h. MOVE IT to a real
    //    SharedClasses/Traffic/BrnTrafficSharedMaths.h the day that header lands.
    //
    // ⚠️ SIGNATURE. The committed in-tree declaration takes `const Vector3Plus&`, and so
    //    does the retail mangled name (`...@@YA?AUMatrix44Affine@vpu@math@rw@@
    //    AEBUVector3Plus@345@@Z` -- AEB == const reference). The DWARF DIE spells the
    //    parameter by value. The reference form is the committed one and is kept:
    //    changing it would change the mangled name and re-open the link hole.
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
    Matrix44Affine ExpandPosPlusYRotToTransform(const Vector3Plus& lPosPlusYRot)
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
        lResult.xAxis = { lfCos, 0.0f, -lfSin, lfCos };
        lResult.yAxis = {  0.0f, 1.0f,   0.0f,  0.0f };
        lResult.zAxis = { lfSin, 0.0f,  lfCos, lfSin };
        lResult.wAxis = { lPosPlusYRot.x, lPosPlusYRot.y, lPosPlusYRot.z, lPosPlusYRot.w };
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
        if (rw::math::vpu::MagnitudeSquared(lDir) > static_cast<f32>(lfCullDistSq.x))
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
