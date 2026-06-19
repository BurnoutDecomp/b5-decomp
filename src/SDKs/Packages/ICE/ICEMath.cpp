// ============================================================================
// SDKs/Packages/ICE/ICEMath.cpp
//
// ICE::ICEMath -- the ICE camera-take math TU. Reconstructed per-function from the
// X360 ARTIST 2007-02 build (class:ICE::ICEMath / ICE::ICEMath::Angles /
// ICE::Angle, plus the StringPrintf body in this source's DecFIGS TU) for semantic
// parity (not byte-matching). The header was grown to the full DWARF surface; the
// bodies for the attested entry points land here.
//
// FIXED-POINT ANGLE MODEL: ICE::Angle is a u16 where the full circle is 65536, so
// 1 unit == 2*pi/65536 radians == 360/65536 degrees. The console build does the
// trig in SIMD lanes (the value broadcast through a vector register); the parity
// reconstruction performs the identical scalar arithmetic, since only lane 0 is
// observed at the call sites.
//
// PLATFORM TRIG NOTE: Sin / Cos / SinCos route the angle (after scaling to
// radians) through the X360 XMVector{Sin,Cos} polynomial approximations, whose
// coefficient tables live in .rodata that is NOT in the per-function exports (see
// the FLAGs below). Those tables cannot be recovered, so the polynomial is modelled
// with the C library's sin/cos -- semantically equivalent to the intended result,
// flagged where it diverges from a byte-exact reproduction.
// ============================================================================

#include "SDKs/Packages/ICE/ICEMath.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "rw/core/stdc/stdc.h"                        // rw::core::stdc::Vsprintf

#include <cmath>     // std::sin / std::cos / std::atan2 (XMVector poly stand-ins)
#include <cstdarg>   // va_list (StringPrintf)

namespace ICE
{
// ---------------------------------------------------------------------------
// ICE angle/distance/lens constants.
//
// Values recovered from the X360 float literals where the asm uses them:
//   TWO_PI_ANGLE = 65536.0   (flt_8207A934, SetFromVecFloat clamp bound)
//   TWO_PI       = 6.2831855 (flt_82001C94, Sin's revolutions->radians scale)
//   TWO_PI_DEG   = 360.0     (the full-circle degree count, 1/360 == flt_82004920)
// MILE / ASPECT_X / LENS_BASE have no literal in the recovered functions.
// ---------------------------------------------------------------------------
const f32 TWO_PI_ANGLE = 65536.0f;
const f32 TWO_PI_DEG   = 360.0f;
const f32 TWO_PI       = 6.2831855f;
// FLAG: MILE -- metres-per-mile constant; value not present in any recovered
// ICEMath function, so it cannot be recovered here. Defined to 0.0f as a
// placeholder so the extern links; correct it when a using-TU reveals the literal.
const f32 MILE = 0.0f;   // FLAG: un-recovered constant (no literal in exports)

namespace ICEMath
{
// FLAG: ASPECT_X / LENS_BASE -- camera/lens constants; no literal in the recovered
// functions (ConvertLensLengthToFovAngle is not in scope). Placeholders.
const f32 ASPECT_X  = 0.0f;   // FLAG: un-recovered constant
const f32 LENS_BASE = 0.0f;   // FLAG: un-recovered constant
}

// ===========================================================================
// ICE::Angle::SetFromVecFloat  @0x8252A948
//
// Set this u16 fixed-point angle from a radians value carried in lvfRadians.x.
//   degrees  = radians * (180/pi)          (flt_8207A908 = 57.29578)
//   revs     = degrees * (1/360)           (flt_82004920 = 0.0027777778)
//   angle    = revs    * TWO_PI_ANGLE      (flt_8207A934 = 65536.0)
// The asm asserts the scaled value is < TWO_PI_ANGLE, then -- if the radians input
// was negative (the vcmpgtfp128 of 0 > radians) -- wraps by adding TWO_PI_ANGLE so
// the result lands in [0, 65536). Finally fctidz truncates toward zero and the low
// 16 bits are stored.
// ===========================================================================
void Angle::SetFromVecFloat(VecFloat lvfRadians)
{
    const f32 lfRadians = lvfRadians.x;

    // radians -> degrees -> revolutions -> angle units.
    const f32 lfDegrees = lfRadians * (180.0f / 3.1415927f);    // 57.29578
    f32 lfAngle = (lfDegrees * (1.0f / 360.0f)) * TWO_PI_ANGLE;  // *0.0027777778 *65536

    CGS_ASSERT(lfAngle < TWO_PI_ANGLE, "lfTemp<TWO_PI_ANGLE");

    // Wrap a negative input into [0, TWO_PI_ANGLE).
    if (lfRadians < 0.0f)
    {
        lfAngle += TWO_PI_ANGLE;
    }

    // fctidz: truncate toward zero; the store keeps the low 16 bits.
    mu16Angle = (u16)(s64)lfAngle;
}

// Angle(f32 radians) ctor: same path as SetFromVecFloat.
Angle::Angle(f32 lfRadians)
    : mu16Angle(0)
{
    VecFloat lvfRadians;
    lvfRadians.x = lfRadians;
    lvfRadians.y = lvfRadians.z = lvfRadians.w = lfRadians;
    SetFromVecFloat(lvfRadians);
}

namespace ICEMath
{
// ===========================================================================
// Round  @0x8252AAD0
//
// Round-half-up built on a truncate-then-floor:
//   lfTrunc = (f32)(s32)lfValue                 (fctiwz/fcfid/frsp: trunc toward 0)
//   if (lfTrunc > lfValue) lfTrunc -= 1.0       (floor: fix negatives)
//   return (lfValue - lfTrunc) >= 0.5 ? lfTrunc + 1.0 : lfTrunc
// i.e. floor(x), then add 1 when the fractional remainder reaches 0.5.
// (flt_82001C98 = 1.0, flt_82001DA0 = 0.5)
// ===========================================================================
f32 Round(f32 lfValue)
{
    f32 lfFloor = (f32)(s32)lfValue;   // truncate toward zero
    if (lfFloor > lfValue)
    {
        lfFloor -= 1.0f;               // convert truncation into a true floor
    }
    return ((lfValue - lfFloor) >= 0.5f) ? (lfFloor + 1.0f) : lfFloor;
}

// ===========================================================================
// Max  @0x8252D658  -- integer max: (a > b) ? a : b.
// ===========================================================================
s32 Max(s32 liA, s32 liB)
{
    return (liA > liB) ? liA : liB;
}

// ===========================================================================
// Sin  @0x8252AA50
//
// Sine of a fixed-point ICE::Angle. Zero short-circuits to 0. Otherwise:
//   revs    = angle * (1/65536)     (flt_8207ACF0 = 0.000015258789)
//   radians = revs  * TWO_PI        (flt_82001C94 = 6.2831855)
//   result  = XMVectorSin(radians)
//
// FLAG: the XMVectorSin polynomial coefficient table lives in platform .rodata not
// present in the exports -- it cannot be recovered byte-exactly. Modelled with the
// C library sinf, which is the same mathematical function the polynomial
// approximates (semantic parity).
// ===========================================================================
f32 Sin(Angle leAngle)
{
    const u16 lu16Angle = (u16)leAngle;
    if (lu16Angle == 0)
    {
        return 0.0f;
    }
    const f32 lfRadians = ((f32)lu16Angle * (1.0f / 65536.0f)) * TWO_PI;
    return std::sin(lfRadians);   // FLAG: XMVectorSin poly table un-recovered
}

// ===========================================================================
// Cos  @ (companion to Sin; same angle->radians scaling, XMVectorCos).
//
// Not in the per-function export scope of this slice, but declared in the surface
// and trivially the cosine companion. FLAG: same un-recovered XMVectorCos table.
// ===========================================================================
f32 Cos(Angle leAngle)
{
    const u16 lu16Angle = (u16)leAngle;
    const f32 lfRadians = ((f32)lu16Angle * (1.0f / 65536.0f)) * TWO_PI;
    return std::cos(lfRadians);   // FLAG: XMVectorCos poly table un-recovered
}

// ===========================================================================
// SinCos  @0x82531600
//
// Compute sine and cosine of an ICE::Angle in one pass, writing *lpfSin / *lpfCos.
// The angle is scaled to radians exactly as in Sin (zero short-circuits the scale
// to 0 radians), then the X360 XMVectorSinCos polynomial fills both lanes.
//
// FLAG: the SinCos coefficient tables (unk_82000C60 / unk_82000C00 / unk_82000C10 /
// unk_82000BD0 / unk_82000BE0 / unk_82000BF0 / unk_82000C20 and the XMVector
// permute masks unk_82CDA400 / unk_82CDA3C0) are platform .rodata NOT in the
// per-function exports -- un-recoverable. The structure (single radians input ->
// two outputs) is reconstructed; the polynomial is modelled with the C library
// sinf/cosf (semantic parity with the intended result).
// ===========================================================================
void SinCos(f32* lpfSin, f32* lpfCos, Angle leAngle)
{
    const u16 lu16Angle = (u16)leAngle;
    f32 lfRadians = 0.0f;
    if (lu16Angle != 0)
    {
        lfRadians = ((f32)lu16Angle * (1.0f / 65536.0f)) * TWO_PI;
    }
    // FLAG: XMVectorSinCos polynomial tables un-recovered; library trig stands in.
    *lpfSin = std::sin(lfRadians);
    *lpfCos = std::cos(lfRadians);
}

// ===========================================================================
// ATan  @0x8252ABD8
//
// Arctangent of a 2D (y, x) pair into an ICE::Angle. The console code computes the
// principal-branch atan via the XMVectorATan polynomial on y/x, then adds pi (for
// x<0) or +/-pi quadrant corrections using the sign of y -- the constant table here
// is {pi/2, pi} (flt_8207A90C = 1.5707964, flt_8207A914 = 3.1415927). The full
// four-quadrant result (radians) is then handed to Angle::SetFromVecFloat.
//
// FLAG: the XMVectorATan polynomial coefficient table is platform .rodata not in
// the exports -- un-recoverable. The four-quadrant structure (atan2 of y,x then
// SetFromVecFloat) is reconstructed; modelled with the C library atan2f, which is
// exactly the four-quadrant arctangent the asm assembles by hand.
// ===========================================================================
Angle ATan(f32 lfY, f32 lfX)
{
    // FLAG: XMVectorATan poly table un-recovered; atan2f is the equivalent
    // four-quadrant arctangent the quadrant-fixup asm reconstructs.
    const f32 lfRadians = std::atan2(lfY, lfX);

    Angle leResult;
    VecFloat lvfRadians;
    lvfRadians.x = lfRadians;
    lvfRadians.y = lvfRadians.z = lvfRadians.w = lfRadians;
    leResult.SetFromVecFloat(lvfRadians);
    return leResult;
}

// ===========================================================================
// Identity  @0x8252AB30
//
// Write the 4x4 identity matrix into *lpMatrix. The asm stores four 16-byte rows
// built from the literals 1.0 (flt_82001C98) on the diagonal and 0.0
// (flt_82001CC0) elsewhere.
// ===========================================================================
void Identity(Matrix4* lpMatrix)
{
    lpMatrix->maRows[0] = { 1.0f, 0.0f, 0.0f, 0.0f };
    lpMatrix->maRows[1] = { 0.0f, 1.0f, 0.0f, 0.0f };
    lpMatrix->maRows[2] = { 0.0f, 0.0f, 1.0f, 0.0f };
    lpMatrix->maRows[3] = { 0.0f, 0.0f, 0.0f, 1.0f };
}

// ===========================================================================
// InvertTransformationMatrix  @0x8252CAF0
//
// Invert an affine transformation matrix. The rotation part is orthonormal, so the
// inverse rotation is the TRANSPOSE of the upper-left 3x3 (the vmrghw/vmrglw lane
// interleaves transpose the rows into columns). The inverse translation is
// -translation rotated by the inverse rotation:  t_inv = -(t . R)  per axis, where
// R's columns are the original rows (the vxor with the sign mask negates the
// translation row, and the vmaddfp chain dots it through the transposed basis).
//
// Rows 0..2 hold the basis vectors (x/y/z axes in their xyz lanes); row 3 holds the
// translation. The destination's row 3 w-lane stays the affine 1 (set by row 3's
// own identity-style w; here we carry the translation's negated-rotated form and a
// 1 in w to keep the affine bottom row).
// ===========================================================================
void InvertTransformationMatrix(Matrix4* lpDst, const Matrix4* lpSrc)
{
    const Vector4& lrX = lpSrc->maRows[0];   // x axis
    const Vector4& lrY = lpSrc->maRows[1];   // y axis
    const Vector4& lrZ = lpSrc->maRows[2];   // z axis
    const Vector4& lrT = lpSrc->maRows[3];   // translation

    // Inverse rotation = transpose of the 3x3 basis.
    Vector4 lInvX = { lrX.x, lrY.x, lrZ.x, 0.0f };
    Vector4 lInvY = { lrX.y, lrY.y, lrZ.y, 0.0f };
    Vector4 lInvZ = { lrX.z, lrY.z, lrZ.z, 0.0f };

    // Inverse translation = -(t . R) projected onto each original basis axis.
    Vector4 lInvT;
    lInvT.x = -(lrT.x * lrX.x + lrT.y * lrX.y + lrT.z * lrX.z);
    lInvT.y = -(lrT.x * lrY.x + lrT.y * lrY.y + lrT.z * lrY.z);
    lInvT.z = -(lrT.x * lrZ.x + lrT.y * lrZ.y + lrT.z * lrZ.z);
    lInvT.w = 1.0f;

    lpDst->maRows[0] = lInvX;
    lpDst->maRows[1] = lInvY;
    lpDst->maRows[2] = lInvZ;
    lpDst->maRows[3] = lInvT;
}

// ===========================================================================
// StringPrintf  @0x8252CC38
//
// printf-style formatter: spill the variadic args and forward to the rwcore
// vsprintf wrapper writing into lpcBuffer. The X360 body manually copies the GPR
// register-save area into a contiguous frame slot and passes its address as the
// va_list; on PC the standard va_start/va_end captures the same argument run.
//
// FLAG: rw::core::stdc::Vsprintf is the homed rwcore vararg entry point, declared
// in rw/core/stdc/stdc.h but its BODY is not yet reconstructed (rwcore stdc TU) --
// the per-TU `cl /c` gate compiles against the declaration; the forward is the
// faithful structure.
// ===========================================================================
void StringPrintf(char* lpcBuffer, const char* lpcFormat, ...)
{
    va_list lvaArgs;
    va_start(lvaArgs, lpcFormat);
    rw::core::stdc::Vsprintf(lpcBuffer, lpcFormat, lvaArgs);   // FLAG: Vsprintf body un-homed
    va_end(lvaArgs);
}

namespace Angles
{
// ===========================================================================
// DegToAng  @0x8252D668
//
// Convert degrees to the u16 Angle unit. The asm first scales degrees to radians
// (flt_8207A904 = 0.017453292 = pi/180), then hands the radians to
// Angle::SetFromVecFloat (which re-derives degrees->revs->angle units). So this is
// just: build a radians VecFloat from the degrees and delegate.
// ===========================================================================
Angle DegToAng(f32 lfDegrees)
{
    const f32 lfRadians = lfDegrees * (3.1415927f / 180.0f);   // 0.017453292

    Angle leResult;
    VecFloat lvfRadians;
    lvfRadians.x = lfRadians;
    lvfRadians.y = lvfRadians.z = lvfRadians.w = lfRadians;
    leResult.SetFromVecFloat(lvfRadians);
    return leResult;
}
} // namespace Angles

} // namespace ICEMath
} // namespace ICE
