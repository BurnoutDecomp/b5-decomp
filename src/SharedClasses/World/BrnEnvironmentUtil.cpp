#include "SharedClasses/World/BrnEnvironmentUtil.h"
#include "rw/math/vpu/vector3_operation.h"   // rw::math::vpu::Normalize / Cross / Negate (skyrig)

#include <cmath>   // sinf / cosf / fabsf

// =============================================================================
// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnWorld::EnvironmentSettings::ComputeKeyLightDirection @ 0x82678AB0
//
// Called by EnvironmentManager::CalcKeyLightDirection @0x827B0638 and (twice) by
// EnvironmentManager::GenerateShaderConstants @0x827D0098 -- once with the raw time of
// day for the UNBIASED direction and once with the sun-elevation-clamped time for the
// biased one.
//
// NOTE ON THE TWO SIBLINGS IN THIS FILE'S DWARF HOME (ComputeSkyColour @0x8267C5C0 and
// ComputeIrradianceRigFromSky @0x8267C948): the ledger records them as blocked on an
// "UNDECODED .rdata vperm permute mask (unk_82CDA450) ... mask VALUES unrecoverable
// without fabrication". That is no longer true. The 16 bytes at 0x82CDA450 are
//     00 01 02 03 | 18 19 1A 1B | 00 01 02 03 | 00 01 02 03
// which, for a vperm whose two source operands are the same register, selects source
// words {0, 2, 0, 0} -- i.e. it is the ordinary (X, Z, X, X) lane gather used to take a
// direction's XZ-plane length. The same gather, on the same mask, is what
// BrnGraphics::Im3dSkyDome::SetConstants uses to build "KeyLightDirAndXZLength", and
// there the destination shader constant's own name confirms the reading independently.
// The two sky-scattering functions are therefore unblocked work, not blocked work; they
// are simply outside this wave's scope.
// =============================================================================

namespace
{
    // The X360 rodata this function loads, all now dumped from the ARTIST image:
    const f32 KF_DEG_TO_RAD  = 0.017453292f;   // flt_820A3674
    const f32 KF_HALF_PI     = 1.5707964f;     // flt_820A3684
    const f32 KF_TWO_OVER_PI = 0.63661975f;    // flt_820A82C4
    const f32 KF_ONE         = 1.0f;           // flt_82001C98
    const f32 KF_ZERO        = 0.0f;           // flt_82001CC0
}

namespace BrnWorld
{
namespace EnvironmentSettings
{

// @ 0x82678AB0
Vector3 ComputeKeyLightDirection( f32 lfSunAngleRad,
                                  f32 lfRigRotationDeg,
                                  f32 lfTiltAtHorizonDeg,
                                  f32 lfTiltAtMiddayDeg )
{
    // Tilt: a triangular ramp that is 0 at either horizon and 1 at midday
    // (lfSunAngleRad == pi/2), used to lerp horizon tilt -> midday tilt.
    const f32 lfTiltAtHorizonRad = lfTiltAtHorizonDeg * KF_DEG_TO_RAD;
    const f32 lfTiltDelta        = (lfTiltAtMiddayDeg * KF_DEG_TO_RAD) - lfTiltAtHorizonRad;
    const f32 lfMiddayFraction   =
        KF_ONE - std::fabs((lfSunAngleRad - KF_HALF_PI) * KF_TWO_OVER_PI);
    const f32 lfTiltRad          = (lfMiddayFraction * lfTiltDelta) + lfTiltAtHorizonRad;

    const f32 lfRigRotationRad   = lfRigRotationDeg * KF_DEG_TO_RAD;

    // The base direction, in the sun rig's own frame: the light travels TOWARD the scene,
    // so it is the negation of the direction to the sun.
    Vector3 lDirection;
    lDirection.x = -std::cos(lfSunAngleRad);
    lDirection.y = -std::sin(lfSunAngleRad);
    lDirection.z = KF_ZERO;
    lDirection.w = KF_ZERO;

    // Rotate about X by the tilt, then about Y by the rig heading. The X360 emits the two
    // row-vector * matrix products as vmulfp128 / vmaddfp ladders (0x82678BDC..0x82678C08);
    // they are lowered here to the equivalent lane math, per the project's vendor-SIMD rule.
    const f32 lfSinTilt = std::sin(lfTiltRad);
    const f32 lfCosTilt = std::cos(lfTiltRad);
    const f32 lfSinRig  = std::sin(lfRigRotationRad);
    const f32 lfCosRig  = std::cos(lfRigRotationRad);

    // v * RotationX(tilt).  XMMatrixRotationX @0x822034B0 stores its rows as
    //   row0 (1, 0, 0, 0) / row1 (0, cos, sin, 0) / row2 (0, -sin, cos, 0)
    // (decoded from the vpermwi128 immediates 0xEA / 0xE3 / 0xDB at 0x8220352C..0x82203540),
    // and the caller's ladder forms x*row0 + y*row1 + z*row2 -- a ROW-vector * matrix product.
    const f32 lfTiltedX = lDirection.x;
    const f32 lfTiltedY = (lDirection.y * lfCosTilt) - (lDirection.z * lfSinTilt);
    const f32 lfTiltedZ = (lDirection.y * lfSinTilt) + (lDirection.z * lfCosTilt);

    // (v * RotationX) * RotationY(rig).  XMMatrixRotationY @0x82203560 stores
    //   row0 (cos, 0, -sin, 0) / row1 (0, 1, 0, 0) / row2 (sin, 0, cos, 0)
    // (vpermwi128 0xB7 / 0xBA / 0x3B at 0x822035DC..0x822035F0), again applied as a row vector.
    Vector3 lResult;
    lResult.x = (lfTiltedX * lfCosRig) + (lfTiltedZ * lfSinRig);
    lResult.y = lfTiltedY;
    lResult.z = (lfTiltedZ * lfCosRig) - (lfTiltedX * lfSinRig);
    lResult.w = KF_ZERO;
    return lResult;
}

}
}

// ===================================================================================================
// [skycolour, step 9] BrnWorld::EnvironmentSettings::ComputeSkyColour @ 0x8267C5C0
//
// APPEND-ONLY BLOCK. It opens its own BrnWorld::EnvironmentSettings scope, so it can be pasted at the
// END of SharedClasses/World/BrnEnvironmentUtil.cpp with no surgery on what is already there. It
// deliberately does NOT re-declare KF_ZERO / KF_ONE: the file already owns them in ITS anonymous
// namespace, and an anonymous namespace re-opened in the same TU is the SAME namespace.
//
// THE ANALYTIC SKY MODEL, evaluated on the CPU for one direction. It is the identical model the sky
// dome's own pixel shader runs per pixel -- BrnSkyDomeManager::Render (BrnSkyDomeManager.cpp:647-651,
// 728-731) hands Im3dSkyDome::SetConstants exactly these five inputs, in this order:
//     GetTopColourDrk() / GetHorColourPow() / GetSunColourPow() / GetHorBleedSclPow()
//     + GetKeyLightDirection()
// so the CPU-side irradiance rig and the drawn dome cannot disagree. The .w packing of the three
// colour vectors is the keyframe's own ScatteringData layout (BrnEnvScatteringData.h): SkyTopColour +
// mfSkyDrk, SkyHorColour + mfSkyHorPow, SkySunColour + mfSkySunPow, and the Vector3
// (mfSkyHorBleedScl, mfSkyHorBleedPow, mfSkySunBleedPow).
//
// PARAMETER NAMES are the ORIGINAL's, from references/Feb-2007/BrnEntityModuleUnity/SharedClasses/
// World/BrnEnvironmentUtil.h (which declares this exact six-parameter form:
//   ComputeSkyColour(Vector4::InParam lTopColourDrk, Vector4::InParam lHorColourPow,
//                    Vector4::InParam lSunColourPow, Vector3::InParam lHorBleedSclPow,
//                    Vector3::InParam lSunDir,       Vector3::InParam lDir)).
// The DecFIGS DWARF (references/DecFIGS/dwarfdump/SharedClasses/World/BrnEnvironmentUtil.cpp:6, source
// line 50) attests the same shape by type: Vector3 (Vector4, Vector4, Vector4, Vector3, Vector3,
// Vector3). The X360 rides them in v1..v6 (VMX register args, homed to arg_10/20/30/40/50/60 at
// 0x8267C5F0..0x8267C624), which is what pins WHICH is which:
//     v1 -> arg_10, read at +0x1C  == lTopColourDrk.w    (mfSkyDrk)
//     v2 -> arg_20, read at +0x3?  == lHorColourPow.w    (mfSkyHorPow)   [vspltw v12, v11, 3]
//     v3 -> arg_30 (also kept live in v127), read at +0x3C == lSunColourPow.w (mfSkySunPow)
//     v4 -> arg_40, read at +0x44 / +0x48 and lane 0     == lHorBleedSclPow.{x,y,z}
//     v5 -> arg_50, read at +0x54 and as a whole vector  == lSunDir
//     v6 -> arg_60, read at +0x64 and as a whole vector  == lDir
// The ONLY caller is ComputeIrradianceRigFromSky @0x8267C948 (JSON xrefs_to), which passes
// -lKeyLightDir as lSunDir (vxor with the 0x80000000 broadcast built by `vslw128 vX, v127, v127`
// after `vspltisw128 v127, -1`, at 0x8267C9F8 / 0x8267CAC0 / ...), i.e. lSunDir points TOWARD the sun.
//
// WHAT THE ASM DOES, in order (0x8267C5C0..0x8267C944):
//   1. 0x8267C630-0x8267C6CC  two XZ-plane lengths. The vperm mask at unk_82CDA450 is
//      00 01 02 03 | 18 19 1A 1B | 00 01 02 03 | 00 01 02 03; with both vperm sources the same
//      register that is the lane gather (X, Z, X, X). Square, add lane0+lane1, then sqrt as
//      s * rsqrt(s) with vrsqrtefp + TWO Newton steps, vsel'ing 0 where vcmpeqfp says s == 0.0f.
//   2. 0x8267C6D0-0x8267C714  if (xzLen(lSunDir) * xzLen(lDir)) >= 0.001f (flt_82013F90) the azimuth
//      cosine is (lDir.x*lSunDir.x + lDir.z*lSunDir.z) / that product; otherwise it is 0.0f
//      (flt_82001CC0). NOTE the polarity: `bge` TAKES the compute arm, so the degenerate case is
//      strictly-less-than.
//   3. 0x8267C71C-0x8267C748  the horizontal (azimuth-only) halo:
//      pow((cosAzimuth + 1.0f) * 0.5f, lHorBleedSclPow.y). 1.0f = flt_82001C98, 0.5f = flt_820A4620
//      (the SAME 0.5f ScatteringData::Construct @0x82675010 loads -- see the shared xref in
//      DATA_DUMP.md). pow is the CRT double core sub_82C09970 (`pow` @0x82674CD0 tail-calls it;
//      its first act is `if (y == 0.0) return 1.0`), and the result is rounded with frsp -- so this
//      is powf spelled as (float)pow((double)x,(double)y). There is NO vexptefp/vlogefp anywhere in
//      this function: the brief's "VMX log2/exp2" shape does not occur, it is four scalar CRT calls.
//   4. 0x8267C778 / 0x8267C7D0  `fsel fD, -v, 0.0f, v` twice -- branch-free max(v, 0) on lDir.y and
//      on dot3(lSunDir, lDir).
//   5. 0x8267C784-0x8267C828  the vertical ramp's exponent is MODULATED by the halo:
//         lHorColourPow.w * (1.0f + lHorBleedSclPow.x * (1.0f - lSunDir.y) * halo)
//      and the ramp itself is pow(max(lDir.y, 0), that). This is what "HorBleed" means: near the
//      sun's AZIMUTH the horizon colour bleeds further up the dome.
//   6. 0x8267C82C-0x8267C864  two sun lobes off the SATURATED dot: pow(dotSat, lSunColourPow.w)
//      (tight) and pow(dotSat, lHorBleedSclPow.z) (wide).
//   7. 0x8267C86C-0x8267C8E8  the darkening factor uses the RAW, UNSATURATED dot:
//         lTopColourDrk.w * ((dot + 1.0f) * 0.5f) + (1.0f - lTopColourDrk.w)
//      == 1 facing the sun, (1 - Drk) facing away.
//   8. 0x8267C884-0x8267C920  three vsubfp/vmaddfp lerps, then one vmulfp128 by the factor.
//
// THE W LANE. Every lerp is a WHOLE-REGISTER vmaddfp and the caller stores the result with a full
// 16-byte `stvx128 v1, r0, r29` (0x8267CA78 and friends), so the console DOES write a w lane -- the
// incidental lerp of the packed scalars (HorPow -> SunPow -> Drk -> SunPow) times the darkening. It
// is reproduced here rather than zeroed so nothing is dropped, but no consumer reads it: the six
// outputs feed GlobalIrradianceManager, which is Vector3 maths.
//
// PC DEVIATIONS (both numeric, both flagged):
//   * sqrt: the console's vrsqrtefp + 2 Newton steps is an ESTIMATE refined to ~full float precision;
//     std::sqrt is the exact value. Agreement is ~1-2 ulp; the zero case is bit-identical because the
//     console vsel's an exact 0.0f on vcmpeqfp(s, 0). This is the SAME de-optimisation, with the same
//     wording, that BrnMathUtils.cpp already ships ("FLAG (VMX->portable): rsqrt+NR -> exact
//     std::sqrt", :44 and :69) -- and BrnIm3d.cpp's SetConstants packs the very same XZ length with a
//     plain scalar `fsqrts`, so the exact form is the console's own idiom too.
//   * pow: MSVC's double pow and the X360 CRT's are both correctly-rounded-ish but not bit-identical
//     implementations. Same input form (double in, frsp out), sub-ulp-class disagreement.
//     BrnMathUtils.cpp:112 already pins sub_82C09970 == std::pow, and that TU is mounted.
//   Neither can change the SIGN or the ORDERING of anything downstream, only the last bits.
//
// WHY THE XZ LENGTH IS A FILE-LOCAL HELPER AND NOT BrnMath::Magnitude2D @0x822B1DD8. That function
// exists, is mounted, and computes sqrt(x*x + z*z) -- but it is NOT what is inlined here, on two
// counts, and calling it would ADD behaviour the binary does not have:
//   (a) Magnitude2D opens with CGS_ASSERT(rw::math::vpu::IsValid(lVector)); there is no IsValid
//       cascade anywhere in 0x8267C5C0..0x8267C944 (the only vcmpeqfp are the two zero tests).
//   (b) Magnitude2D has no zero guard; the form inlined here vsel's an exact 0.0f when the squared
//       length is exactly 0.0f (0x8267C664/0x8267C698 and 0x8267C69C/0x8267C6C8).
// The shape the compiler folded in is most likely `Magnitude(Flatten(dir))` over the rwmath 2-lane
// types; it is spelled out locally rather than guessed at as a named call.
//
// ⚠ A LATENT DOMAIN ERROR THAT IS THE CONSOLE'S, NOT OURS -- DO NOT "FIX" IT SILENTLY.
// The halo base (cosAzimuth + 1.0f) * 0.5f is NEVER clamped: between the fdivs at 0x8267C714 and the
// `bl sub_82C09970` at 0x8267C740 there is no fsel, no vmaxfp, nothing. But cosAzimuth is a
// dot-over-product quotient, so at the EXACT anti-sun azimuth it rounds to -1.000000119 (0xBF800001)
// rather than -1, the base becomes -5.96e-08 (0xB3800000), and pow(negative, non-integer) takes the
// CRT's domain arm (sub_82C09970 @0x82C09A58: `_d_inttype(y)` != 1 && != 2 -> load dbl_82F94648 and
// return). Measured, both with exact sqrt and with a replica of the console's rsqrt+2NR ladder:
// 135 of 360 sun azimuths make the base negative (work/trace.cpp output in the report).
// It MATTERS because ComputeIrradianceRigFromSky's SECOND call -- lShadowFillColour -- samples
// exactly that direction (0x8267CA84..0x8267CB1C builds normalize(keyLight.x, 0, keyLight.z), which
// is -normalize(lSunDir.xz)). It has never shipped as a visible bug because the whole rig is gated
// off: mbSetIrradianceFromSky / mbSetScattColsFromSky (+0x6F4/+0x6F5) are FALSE at Construct and only
// the debug component / the tool flip them. Reproduced faithfully here; the decision about whether
// the PC build wants a documented guard belongs to whoever turns that flag on.
// ===================================================================================================

namespace
{
    // The X360 rodata this function loads, all dumped from the ARTIST image (DATA_DUMP.md).
    // KF_ZERO (flt_82001CC0) and KF_ONE (flt_82001C98) are ALREADY defined above in this TU's
    // anonymous namespace by the ComputeKeyLightDirection block -- reused, not redeclared.
    const f32 KF_HALF                  = 0.5f;     // flt_820A4620 @0x820A4620, loaded 0x8267C738
    const f32 KF_MIN_XZ_LENGTH_PRODUCT = 0.001f;   // flt_82013F90 @0x82013F90, loaded 0x8267C6DC

    // 0x8267C630..0x8267C6CC, twice (once per direction).
    //
    // The length of a direction's projection onto the XZ (ground) plane. The X360 gathers the
    // (X, Z, X, X) lanes with the vperm mask at unk_82CDA450, squares, adds lanes 0+1, and takes the
    // square root as `s * rsqrt(s)` from vrsqrtefp plus two Newton-Raphson refinements, selecting an
    // exact 0.0f where vcmpeqfp reports s == 0.0f. std::sqrt is the exact form of the same quantity;
    // the explicit zero test preserves the console's exact-zero arm.
    f32 ComputeXZLength( Vector3::InParam lVector )
    {
        const f32 lfLengthSquared = (lVector.x * lVector.x) + (lVector.z * lVector.z);
        return (lfLengthSquared == KF_ZERO) ? KF_ZERO : std::sqrt(lfLengthSquared);
    }

    // 0x8267C778 and 0x8267C7D0: `fsel fD, -v, 0.0f, v`, the compiler's branch-free max(v, 0).
    // Spelled with the SAME predicate rather than as std::fmax so the -0.0f and NaN arms match the
    // hardware exactly (fsel takes the 0.0f arm only when -v >= 0.0f, so a NaN falls through as-is).
    f32 MaxWithZero( f32 lfValue )
    {
        return (-lfValue >= KF_ZERO) ? KF_ZERO : lfValue;
    }

    // The four `bl sub_82C09970` sites (0x8267C740 / 0x8267C828 / 0x8267C83C / 0x8267C854), each
    // followed by frsp. sub_82C09970 is the CRT's double-precision pow core -- `pow` @0x82674CD0 is
    // one of its 30 callers and it opens with `if (y == 0.0) return 1.0` (dbl_82001CA8 / dbl_82001CA0).
    // So the console computes powf as (float)pow((double)base, (double)exponent); kept in that form
    // rather than calling powf, which MSVC may evaluate in single precision.
    f32 PowF32( f32 lfBase, f32 lfExponent )
    {
        return static_cast<f32>( std::pow( static_cast<f64>(lfBase), static_cast<f64>(lfExponent) ) );
    }

    // vsubfp + vmaddfp: `to` is reached as `from + (to - from) * blend`, on all four lanes, exactly
    // as the X360 emits it at 0x8267C884/0x8267C8BC, 0x8267C8EC/0x8267C900 and 0x8267C918/0x8267C91C.
    Vector4 LerpVector4( Vector4::InParam lFrom, Vector4::InParam lTo, f32 lfBlend )
    {
        return Vector4{ ((lTo.x - lFrom.x) * lfBlend) + lFrom.x,
                        ((lTo.y - lFrom.y) * lfBlend) + lFrom.y,
                        ((lTo.z - lFrom.z) * lfBlend) + lFrom.z,
                        ((lTo.w - lFrom.w) * lfBlend) + lFrom.w };
    }
}

namespace BrnWorld
{
namespace EnvironmentSettings
{

// @ 0x8267C5C0
Vector3 ComputeSkyColour( Vector4::InParam lTopColourDrk,
                          Vector4::InParam lHorColourPow,
                          Vector4::InParam lSunColourPow,
                          Vector3::InParam lHorBleedSclPow,
                          Vector3::InParam lSunDir,
                          Vector3::InParam lDir )
{
    // --- the horizontal sun halo: how close this direction's AZIMUTH is to the sun's ---------------
    // Both directions are flattened onto the ground plane and the cosine of the angle between the
    // flattened vectors is taken. Straight up (or a sun exactly at the zenith) degenerates, and the
    // console guards it with a product-of-lengths floor rather than with two separate tests.
    const f32 lfDirXZLength      = ComputeXZLength( lDir );
    const f32 lfSunDirXZLength   = ComputeXZLength( lSunDir );
    const f32 lfXZLengthProduct  = lfSunDirXZLength * lfDirXZLength;

    f32 lfCosAzimuth = KF_ZERO;
    if ( lfXZLengthProduct >= KF_MIN_XZ_LENGTH_PRODUCT )
    {
        lfCosAzimuth = ((lDir.x * lSunDir.x) + (lDir.z * lSunDir.z)) / lfXZLengthProduct;
    }

    // (cos + 1) * 0.5 maps [-1, 1] to [0, 1] -- 1 at the sun's azimuth, 0 opposite it. NOT clamped:
    // the X360 goes straight from fdivs (0x8267C714) into `bl sub_82C09970` (0x8267C740) with no
    // fsel/vmaxfp in between, so at the exact anti-sun azimuth the quotient's last-bit error makes
    // this base a small NEGATIVE and pow takes the CRT's domain arm. See the warning in the banner.
    const f32 lfHorBleed = PowF32( (lfCosAzimuth + KF_ONE) * KF_HALF, lHorBleedSclPow.y );

    // --- the vertical horizon -> zenith ramp, widened toward the sun's azimuth ---------------------
    // A LOW exponent lifts the horizon colour high up the dome; the bleed term lowers the exponent's
    // effect near the sun and is itself scaled down as the sun climbs (1 - lSunDir.y), so a midday
    // sun barely bleeds at all.
    const f32 lfHorPow = lHorColourPow.w *
                         ( KF_ONE + (lHorBleedSclPow.x * (KF_ONE - lSunDir.y) * lfHorBleed) );
    const f32 lfTopBlend = PowF32( MaxWithZero( lDir.y ), lfHorPow );

    // --- the two sun lobes ------------------------------------------------------------------------
    const f32 lfSunDot    = (lSunDir.x * lDir.x) + (lSunDir.y * lDir.y) + (lSunDir.z * lDir.z);
    const f32 lfSunDotSat = MaxWithZero( lfSunDot );
    const f32 lfSunCore   = PowF32( lfSunDotSat, lSunColourPow.w );      // tight: the sun itself
    const f32 lfSunBleed  = PowF32( lfSunDotSat, lHorBleedSclPow.z );    // wide: the sky around it

    // --- darkening away from the sun (the keyframe's mfSkyDrk) ------------------------------------
    // Uses the RAW dot, not the saturated one, so it keeps falling through the anti-solar hemisphere:
    // 1.0 looking at the sun, (1 - Drk) looking directly away.
    const f32 lfDarkening = (lTopColourDrk.w * ((lfSunDot + KF_ONE) * KF_HALF)) +
                            (KF_ONE - lTopColourDrk.w);

    // --- composite: horizon -> sun-widened, then up into the zenith, then the sun core ------------
    Vector4 lColour = LerpVector4( lHorColourPow, lSunColourPow, lfSunBleed );
    lColour         = LerpVector4( lColour,       lTopColourDrk, lfTopBlend );
    lColour         = LerpVector4( lColour,       lSunColourPow, lfSunCore );

    // The w lane is the console's incidental by-product of lerping whole registers (see the banner);
    // it is carried, not read.
    return Vector3{ lColour.x * lfDarkening,
                    lColour.y * lfDarkening,
                    lColour.z * lfDarkening,
                    lColour.w * lfDarkening };
}

}
}

// ===================================================================================================
// [skyrig, step 9] BrnWorld::EnvironmentSettings::ComputeIrradianceRigFromSky @ 0x8267C948
//
// APPEND-ONLY BLOCK, to be pasted at the END of SharedClasses/World/BrnEnvironmentUtil.cpp AFTER the
// skycolour block. It opens its own BrnWorld::EnvironmentSettings scope and adds no file-scope names
// at all (its two axis constants are function locals), so it cannot collide with what is already
// there. It deliberately does NOT re-declare KF_ZERO / KF_ONE / KF_HALF / PowF32 / MaxWithZero.
//
// CONDUCTOR NOTE -- the one include. This body names the two SDK helpers the X360 compiler inlined
// (rw::math::vpu::Normalize and rw::math::vpu::Cross), which live in
// vendor/renderware/include/rw/math/vpu/vector3_operation.h. That header is NOT reachable from
// BrnEnvironmentUtil.cpp today (BrnCommonTypes.h pulls only rw/math/vpu/types.h), so the #include is
// carried at the top of THIS block. It is `#pragma once`-guarded and at file scope, so it is legal
// where it lands; hoist it up into the .cpp's include list if you prefer it there.
// ===================================================================================================


// ---------------------------------------------------------------------------------------------------
// WHAT IT IS
//
// The six FILL LIGHTS of the ambient "irradiance rig" -- +sun / -sun / left / right / up / down --
// re-derived from the analytic sky model instead of being authored in the keyframe. Six taps of
// ComputeSkyColour, one per rig face. It writes exactly the six LightingData colour slots the
// keyframe would otherwise have supplied (BrnEnvLightingData.h mv3{Key,Shadow,Left,Right,Up,Down}
// FillColour), which GlobalIrradianceManager then folds into the two irradiance quadrics.
//
// [FLAG UNREACHABLE IN THE RETAIL FLOW] The only caller, EnvironmentManager::GenerateShaderConstants
// @0x827D0098, gates the whole call on `mbSetIrradianceFromSky` (EnvironmentManager +0x6F5):
//     0x827D04CC  lbz r11, 0x6F5(r31) ; 0x827D04D0 cmplwi cr6, r11, 0 ; 0x827D04D4 beq -> skip
// A grep of the ENTIRE X360 export for that byte (`grep -l "0x6F5(" *.json`) returns exactly two
// functions: EnvironmentManager::Construct, which stores ZERO into it (`stb r30, 0x6F5(r31)`
// @0x827CA734), and GenerateShaderConstants, which only reads it. Nothing in the shipped code path
// ever sets it. HOW TO REACH IT: the debug UI. DebugComponent::OnActivate @0x827B2408 registers the
// byte as a bool debug variable named "Set Irradiance rig from Sky" in the debug group
// "Irradiance rig" (`addi r4, r11, 0x6F5` @0x827B3378, r11 = the component's manager pointer at
// +0x0C), next to its "Set scatt colours from sky" twin on +0x6F4 (@0x827B2B60). Flip that variable
// and this function runs. It is reconstructed in full because it is real console code, and because
// the sky model it samples is the same one the sky dome actually draws (skycolour's banner).
// BrnWorldModule.cpp:4283-4286 already records the matching bring-up decision: the rig is treated as
// OFF and the keyframe's authored fill colours are used.
//
// SHAPE
//   DWARF references/DecFIGS/dwarfdump/SharedClasses/World/BrnEnvironmentUtil.cpp:9 (source line 100):
//     void ComputeIrradianceRigFromSky(Vector3&, Vector3&, Vector3&, Vector3&, Vector3&, Vector3&,
//                                      Vector4, Vector4, Vector4, Vector3, Vector3)
//   references/Feb-2007/BrnEntityModuleUnity/SharedClasses/World/BrnEnvironmentUtil.h declares the
//   same eleven-parameter form WITH THE ORIGINAL NAMES, which are adopted verbatim:
//     lKeyFillColour, lShadowFillColour, lLeftFillColour, lRightFillColour, lUpFillColour,
//     lDownFillColour, lTopColourDrk, lHorColourPow, lSunColourPow, lHorBleedSclPow, lKeyLightDir.
//   Those names are CONFIRMED against the X360 call site rather than trusted: GenerateShaderConstants
//   passes r3..r8 = EnvironmentManager +0x610/+0x620/+0x640/+0x630/+0x650/+0x660, which are
//   mLighting@0x5F0 + 0x20/0x30/0x40/0x50/0x60/0x70 == mv3KeyFillColour / mv3ShadowFillColour /
//   mv3LeftFillColour / mv3RightFillColour / mv3UpFillColour / mv3DownFillColour. Note the deliberate
//   crossover: parameter 3 (Left) is the FOURTH store the body performs and lands on +0x640;
//   parameter 4 (Right) is the THIRD store and lands on +0x630.
//   The console passes the five in-parameters BY VALUE in v1..v5 (VMX register arguments). They are
//   spelled Vector4::InParam / Vector3::InParam here to match the PC vendor typedef and skycolour's
//   sibling declaration; the out-parameters are plain Vector3& (Feb-2007 spells them
//   `Vector3::OutParam`, a typedef the PC vendor types.h does not carry -- it expands to Vector3&).
//
// WHAT THE ASM DOES (0x8267C948..0x8267CD6C; full instruction map in
// scratch/postfx_step9_final/skyrig/work/ASM_MAP.md)
//   There is NO sample loop and NO integration -- six straight-line `bl ComputeSkyColour` at
//   0x8267CA68 / CB1C / CC50 / CCA8 / CCF8 / CD50, each with ONE direction, each result stored whole
//   (`stvx128`, 16 bytes) into one out-pointer. No weights, no cosine term, no normalisation pass.
//   * every call gets the SAME first four arguments (v1..v4 are saved into v124/v123/v122/v121 at
//     0x8267C9A8..0x8267C9F0 and restored before each call) and lSunDir = -lKeyLightDir
//     (`vxor` with the 0x80000000 broadcast built by `vslw128 vX,v127,v127` after
//     `vspltisw128 v127,-1`, at 0x8267C9F8 / CAC0 / CBFC / CCA4 / CCF0 / CD48).
//   * the horizontal directions are built with `vperm` through the control word at unk_82CDA350
//     (00 01 02 03 | 14 15 16 17 | 00 01 02 03 | 00 01 02 03 == take vA.x, vB.y, vA.x, vA.x) with
//     vB = a 0.0f broadcast, followed by `vrlimi128 vD, splat(z), 2, 0` (mask 2 = the Z word). Net
//     effect: build (v.x, 0, v.z, v.x) -- the XZ (ground-plane) projection with Y forced to zero.
//     Since the reconstruction is de-optimised to named lane maths, NO copy of that constant is
//     emitted here, so there is nothing for the irradiance / envblend groups to collide with.
//   * `Normalize` is the SDK's `vrsqrtefp` + TWO Newton-Raphson steps
//     (t = 1 - L*e^2; e' = e + 0.5*e*t), exactly the ladder CameraUtils.cpp documents at
//     0x8220C774..0x8220C7B0. FLAG: unlike ComputeSkyColour, this function's inlined copies have NO
//     vcmpeqfp/vsel zero-guard; the vendor Normalize used here returns the zero vector for a
//     zero-length input (only reachable if the key light points exactly straight up or down).
//   * `Cross` is the SDK permute form yzx(a*yzx(b) - yzx(a)*b) at 0x8267CBD0..0x8267CBDC, with
//     `a` = the sunward horizontal direction and `b` = world up -- i.e. Cross(dir, up), the opposite
//     operand order from CameraUtils' Cross(up, zAxis). That is what the binary does and it is kept;
//     the consequence is only which of the two horizontal side taps is called "Right" (the analytic
//     sky is near-symmetric about the sun's vertical plane, so Left and Right differ only through the
//     azimuth term's sign, which is squared away by the cos -- they come out equal in practice).
//
// PC DEVIATIONS, all three cosmetic and all flagged:
//   1. IDENTITY SCALE DROPPED. Faces 1/2/3 end with one more `vmulfp128 vD, dir, splat(1.0f)`
//      (0x8267CA60 / CB14 / CC48). The 1.0f is flt_82001C98, routed into a vector register the only
//      way PPC can (store to the var_E0 Vector3 slot, lvx, vspltw lane 0) for the SDK's
//      Mult(Vector3, VecFloat). Multiplying by one is dropped.
//   2. W LANE. The console's permute leaves the direction's w lane holding the residue (+/-K.x) and
//      then divides it by the length; the reconstruction writes w = 0. ComputeSkyColour reads only
//      x/y/z of lDir (the (X,Z,X,X) gather, the y lane, and a vmsum3fp128 dot3), so nothing reads it.
//      Same standing FLAG the tree already carries for Cross() in CameraUtils.cpp.
//   3. RECIPROCAL SQUARE ROOT. The vendor Normalize is the de-optimised exact 1/std::sqrt rather than
//      the console's estimate-plus-two-Newton-steps -- a touch tighter, never looser.
//   Face 3 re-derives the sunward horizontal direction from scratch in the asm (it re-loads
//   lKeyLightDir out of its stack home and re-runs the perm + Normalize); the value is bit-identical
//   to face 1's, so it is reused here instead of recomputed. Nothing else about the control flow is
//   rearranged: the six calls stay in emission order.
// ---------------------------------------------------------------------------------------------------

namespace BrnWorld
{
namespace EnvironmentSettings
{

// @ 0x8267C948
void ComputeIrradianceRigFromSky( Vector3& lKeyFillColour,
                                  Vector3& lShadowFillColour,
                                  Vector3& lLeftFillColour,
                                  Vector3& lRightFillColour,
                                  Vector3& lUpFillColour,
                                  Vector3& lDownFillColour,
                                  Vector4::InParam lTopColourDrk,
                                  Vector4::InParam lHorColourPow,
                                  Vector4::InParam lSunColourPow,
                                  Vector3::InParam lHorBleedSclPow,
                                  Vector3::InParam lKeyLightDir )
{
    // The two world axes the X360 materialises as Vector3 literals in the var_E0 stack slot:
    // (0, 1, 0, 0) at 0x8267CB24-0x8267CB68 and again at 0x8267CCB0-0x8267CCCC, and (0, -1, 0, 0)
    // at 0x8267CD00-0x8267CD20 (the -1.0f is flt_820037C8). Function-local so this block adds no
    // file-scope name to the shared TU.
    const Vector3 KV_WORLD_UP   = { 0.0f,  1.0f, 0.0f, 0.0f };
    const Vector3 KV_WORLD_DOWN = { 0.0f, -1.0f, 0.0f, 0.0f };

    // ComputeSkyColour wants the direction TOWARD the sun; the key light direction is the direction
    // the light TRAVELS (sun -> scene), so every one of the six calls passes its negation.
    const Vector3 lSunDir = rw::math::vpu::Negate( lKeyLightDir );

    // --- 1. key fill: the horizon in the sun's own azimuth ------------------------------ (+0x610)
    // The sunward direction flattened onto the ground plane. This is the brightest, warmest tap.
    const Vector3 lKeyFillDir =
        rw::math::vpu::Normalize( Vector3{ -lKeyLightDir.x, 0.0f, -lKeyLightDir.z, 0.0f } );
    lKeyFillColour = ComputeSkyColour( lTopColourDrk, lHorColourPow, lSunColourPow,
                                       lHorBleedSclPow, lSunDir, lKeyFillDir );

    // --- 2. shadow fill: the horizon directly opposite the sun -------------------------- (+0x620)
    const Vector3 lShadowFillDir =
        rw::math::vpu::Normalize( Vector3{ lKeyLightDir.x, 0.0f, lKeyLightDir.z, 0.0f } );
    lShadowFillColour = ComputeSkyColour( lTopColourDrk, lHorColourPow, lSunColourPow,
                                          lHorBleedSclPow, lSunDir, lShadowFillDir );

    // --- 3. right fill: the horizon 90 degrees round from the sun ----------------------- (+0x630)
    // Cross(sunward-horizontal, up) -- horizontal by construction, so the second Normalize only
    // undoes rounding.
    const Vector3 lRightFillDir =
        rw::math::vpu::Normalize( rw::math::vpu::Cross( lKeyFillDir, KV_WORLD_UP ) );
    lRightFillColour = ComputeSkyColour( lTopColourDrk, lHorColourPow, lSunColourPow,
                                         lHorBleedSclPow, lSunDir, lRightFillDir );

    // --- 4. left fill: the right tap mirrored through the Y axis ------------------------ (+0x640)
    // The asm negates the x/z/w lanes and copies the ORIGINAL y lane back in
    // (`vrlimi128 v13, splat(+R.y), 4, 0` @0x8267CC8C, mask 4 = word 1). R.y is identically zero for
    // a cross with world up, but the lane-wise mirror is what the source wrote, so it is kept.
    const Vector3 lLeftFillDir = { -lRightFillDir.x,  lRightFillDir.y,
                                   -lRightFillDir.z, -lRightFillDir.w };
    lLeftFillColour = ComputeSkyColour( lTopColourDrk, lHorColourPow, lSunColourPow,
                                        lHorBleedSclPow, lSunDir, lLeftFillDir );

    // --- 5. up fill: straight up ------------------------------------------------------- (+0x650)
    lUpFillColour = ComputeSkyColour( lTopColourDrk, lHorColourPow, lSunColourPow,
                                      lHorBleedSclPow, lSunDir, KV_WORLD_UP );

    // --- 6. down fill: straight down --------------------------------------------------- (+0x660)
    // Below the horizon the sky model's vertical ramp saturates at the horizon colour, darkened by
    // the anti-solar term -- this is the darkest tap, which is what makes the rig read as ambient
    // occlusion from the ground.
    lDownFillColour = ComputeSkyColour( lTopColourDrk, lHorColourPow, lSunColourPow,
                                        lHorBleedSclPow, lSunDir, KV_WORLD_DOWN );
}

}
}
