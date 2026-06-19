#ifndef SDKS_PACKAGES_ICE_ICEMATH_HPP
#define SDKS_PACKAGES_ICE_ICEMATH_HPP

#include "types.hpp"
#include "rw/math/vpu/types.h"   // rw::math::vpu::Vector3 / Vector4 / Matrix44

// ============================================================================
// SDKs/Packages/ICE/ICEMath.hpp
//
// ICE::ICEMath -- the ICE camera-take scalar/vector/angle math surface
// (rounding, clamping, int<->float conversion, trig over the fixed-point
// ICE::Angle, the 4x4 ICE Matrix4 identity/affine-inverse, and the printf-style
// StringPrintf formatter). The bodies live in the ICE math TU
// (SDKs/Packages/ICE/ICEMath.cpp); the per-TU `cl /c` gate does not link, so the
// non-trivial entry points are DECLARATION-ONLY here.
//
// HISTORY: grown in place from the keystone minimal slice (which declared exactly
// Round(f32) / Clamp(f32,f32,f32) inline / IntToFloat(s32), the surface the
// committed ICEData.cpp bodies call) to the full DWARF ICEMath API. The keystone
// call sites -- ICEMath::Round and ICEMath::Clamp(f32,f32,f32) -- keep their exact
// original signatures, so ICEData.cpp / ICEDataEnums.cpp still compile unchanged.
//
// The full free-function set, the ICE constants, and `struct ICE::Angle` come from
// references/DecFIGS/dwarfdump/SDKs/Packages/ICE/ICEMath.hpp (gated on the X360
// ledger). Only the entry points the X360 build attests are reconstructed with
// bodies in the sibling .cpp; the remaining DWARF declarations are surfaced so a
// using-TU can extend without re-homing ICEMath.
//
// `Matrix4` is ICE's own row-major 4x4 float matrix (64 bytes, four 16-byte rows),
// distinct from the renderware rw::math::vpu::Matrix44 the conversion helpers map
// to/from. We model it on the rwmath VPU vector lanes to keep the 16-byte SIMD
// row alignment the X360 asm relies on (lvx128/stvx128 on 16-aligned rows).
// ============================================================================

namespace ICE
{
    // ICE's row-major 4x4 float matrix: four 16-byte-aligned rows (Vector4 lanes).
    // Layout matches the X360 take-matrix the lvx128/stvx128 idiom walks at stride
    // 16. Named `Matrix4` per the DWARF (ICEMath::Identity / RotateX / ... take a
    // `Matrix4*`); kept distinct from rw::math::vpu::Matrix44.
    typedef rw::math::vpu::Vector3 Vector3;
    typedef rw::math::vpu::Vector4 Vector4;
    typedef rw::math::vpu::Matrix44 Matrix44;

    struct alignas(16) Matrix4
    {
        Vector4 maRows[4];
    };

    // VecFloat: a scalar broadcast through a SIMD float register. The X360 build
    // passes the value in the x lane of a vector register (the asm splats lane 0);
    // on PC we model it as the named 4-lane vector so the call shape is preserved.
    typedef rw::math::vpu::Vector4 VecFloat;

    // --- ICE angle/distance constants ---------------------------------------
    // Full circle expressed in the u16 fixed-point Angle unit (65536 == 2*pi).
    extern const f32 TWO_PI_ANGLE;   // 65536.0
    extern const f32 TWO_PI_DEG;     // 360.0
    extern const f32 TWO_PI;         // 6.2831855  (2*pi, single-precision)
    const s32 ICE_FPS = 30;          // value attested inline by the DWARF
    extern const f32 MILE;           // metres per mile (value not in the exports)

    // ------------------------------------------------------------------------
    // ICE::Angle -- a u16 fixed-point angle. The full circle is 65536, so the
    // unit is 2*pi/65536 radians. Stored as the raw u16; conversions are inline
    // where the rule is exact and owned here, out-of-line (SetFromVecFloat) where
    // the body lives in the TU.
    // ------------------------------------------------------------------------
    struct Angle
    {
    public:
        Angle() : mu16Angle(0) {}
        Angle(u16 lu16Angle) : mu16Angle(lu16Angle) {}
        // Construct from a float radians value (delegates to SetFromVecFloat,
        // defined in the TU). DECLARATION-ONLY.
        explicit Angle(f32 lfRadians);

        operator u16() const { return mu16Angle; }
        Angle operator-() const { return Angle((u16)(0u - mu16Angle)); }
        Angle& operator=(u16 lu16Angle) { mu16Angle = lu16Angle; return *this; }

        // mu16Angle * (2*pi/65536). Exact, owned here -> inline.
        f32 ConvertToFloatRad() const
        {
            return (f32)mu16Angle * (6.2831855f / 65536.0f);
        }

        // Set this angle from a radians value carried in a VecFloat (x lane).
        // Body in ICEMath.cpp (radians -> degrees -> revolutions -> u16 unit, with
        // a wrap into [0, TWO_PI_ANGLE)). DECLARATION-ONLY.
        void SetFromVecFloat(VecFloat lvfRadians);

    private:
        u16 mu16Angle;
    };

namespace ICEMath
{
    // === Scalar conversion / rounding =======================================

    // Round-to-nearest. DECLARATION-ONLY: the exact half-way rule is owned in the
    // TU (truncate-toward-zero baseline, bias down for already-larger results,
    // then +1 when the fractional part is >= 0.5). Keystone call site:
    // ICEElementDescription::Prepare.
    f32 Round(f32 lfValue);

    // Truncate toward zero / floor / round-to-int. DECLARATION-ONLY (TU bodies).
    f32 Truncate(f32 lfValue);
    f32 Floor(f32 lfValue);
    s32 RoundFloatToInt(f32 lfValue);

    // Signed-int to float. Plain numeric cast -> inline (keystone-era, kept).
    inline f32 IntToFloat(s32 liValue)
    {
        return (f32)liValue;
    }
    inline s32 FloatToInt(f32 lfValue)
    {
        return (s32)lfValue;
    }
    inline u32 FloatToUInt(f32 lfValue)
    {
        return (u32)lfValue;
    }

    // === Absolute value / clamp / min-max ===================================

    inline s32 Abs(s32 liValue) { return liValue < 0 ? -liValue : liValue; }
    inline f32 Abs(f32 lfValue) { return lfValue < 0.0f ? -lfValue : lfValue; }

    // Clamp lfValue into [lfLo, lfHi]. Trivially obvious -> inline. KEYSTONE
    // SIGNATURE: ICEData.cpp's Prepare asserts call this exact overload; do not
    // change it.
    inline f32 Clamp(f32 lfValue, f32 lfLo, f32 lfHi)
    {
        f32 lfResult = (lfValue < lfLo) ? lfLo : lfValue;
        return (lfResult > lfHi) ? lfHi : lfResult;
    }
    inline s32 Clamp(s32 liValue, s32 liLo, s32 liHi)
    {
        s32 liResult = (liValue < liLo) ? liLo : liValue;
        return (liResult > liHi) ? liHi : liResult;
    }

    // Max @0x8252D658 / Min: simple comparisons. Max(s32) has a TU body (matched
    // to the X360 asm); Max/Min(f32) are declared from the DWARF surface.
    s32 Max(s32 liA, s32 liB);   // body in ICEMath.cpp
    f32 Max(f32 lfA, f32 lfB);
    f32 Min(f32 lfA, f32 lfB);

    // Integer signed modulo. DECLARATION-ONLY.
    s32 SignedMod(s32 liA, s32 liB);

    // === Trig over the fixed-point ICE::Angle ===============================
    // Sin @0x8252AA50 / Cos: angle -> revolutions (*1/65536) -> radians (*2*pi),
    // then the platform XMVectorSin/Cos polynomial. DECLARATION-ONLY (TU bodies).
    f32 Sin(Angle leAngle);
    f32 Cos(Angle leAngle);

    // SinCos @0x82531600: fills *lpfSin and *lpfCos for an Angle in one pass.
    // DECLARATION-ONLY (TU body).
    void SinCos(f32* lpfSin, f32* lpfCos, Angle leAngle);

    // ATan @0x8252ABD8: arctangent of a 2D (y, x) pair -> ICE::Angle.
    // DECLARATION-ONLY (TU body).
    Angle ATan(f32 lfY, f32 lfX);

    f32 Sqrt(f32 lfValue);   // DECLARATION-ONLY

    // === String helpers =====================================================
    s32 StrLen(const char* lpcString);          // DECLARATION-ONLY

    // printf-style formatter -> writes into lpcBuffer. Forwards the va-list to the
    // rwcore Vsprintf. DECLARATION-ONLY (TU body).
    void StringPrintf(char* lpcBuffer, const char* lpcFormat, ...);

    // === Vector3 helpers (DECLARATION-ONLY; surfaced from the DWARF) =========
    void  Copy(Vector3* lpDst, const Vector3* lpSrc);
    void  Normalize(Vector3* lpDst, const Vector3* lpSrc);
    f32   Dot(const Vector3* lpA, const Vector3* lpB);
    void  Cross(Vector3* lpDst, const Vector3* lpA, const Vector3* lpB);
    void  Add(Vector3* lpDst, const Vector3* lpA, const Vector3* lpB);
    void  Sub(Vector3* lpDst, const Vector3* lpA, const Vector3* lpB);
    void  Scale(Vector3* lpDst, const Vector3* lpSrc, f32 lfScale);
    void  ScaleAdd(Vector3* lpDst, const Vector3* lpA, const Vector3* lpB, f32 lfScale);
    f32   Length(const Vector3* lpVec);
    f32   DistBetween(const Vector3* lpA, const Vector3* lpB);
    s32   MemCmp(const void* lpA, const void* lpB, u32 luSize);
    void  Vector3ToVector4(Vector3 lvSrc, Vector4& lrDst, f32 lfW);

    // === Matrix4 (the ICE 4x4) ==============================================
    // Identity @0x8252AB30: write the 4x4 identity into *lpMatrix. TU body.
    void Identity(Matrix4* lpMatrix);

    // InvertTransformationMatrix @0x8252CAF0: invert an affine transform
    // (transpose the 3x3 rotation, negate-and-rotate the translation). TU body.
    void InvertTransformationMatrix(Matrix4* lpDst, const Matrix4* lpSrc);

    void RotateX(Matrix4* lpDst, const Matrix4* lpSrc, Angle leAngle);   // DECL-ONLY
    void RotateY(Matrix4* lpDst, const Matrix4* lpSrc, Angle leAngle);   // DECL-ONLY
    void RotateZ(Matrix4* lpDst, const Matrix4* lpSrc, Angle leAngle);   // DECL-ONLY

    // === Camera / lens ======================================================
    extern const f32 ASPECT_X;   // value not in the exports
    extern const f32 LENS_BASE;  // value not in the exports
    Angle ConvertLensLengthToFovAngle(f32 lfLensLength);   // DECL-ONLY

    // === Angle <-> unit conversions =========================================
    namespace Angles
    {
        // DegToAng @0x8252D668: degrees -> the u16 Angle unit. TU body.
        Angle DegToAng(f32 lfDegrees);

        f32   RadToDeg(f32 lfRadians);   // DECL-ONLY
        f32   RevToRad(f32 lfRevs);      // DECL-ONLY
        Angle RevToAng(f32 lfRevs);      // DECL-ONLY
        f32   AngToDeg(Angle leAngle);   // DECL-ONLY
        f32   AngToFloat(Angle leAngle); // DECL-ONLY
    }

    f32 FixToFloat(Angle leAngle);   // DECL-ONLY
}
}

#endif // SDKS_PACKAGES_ICE_ICEMATH_HPP
