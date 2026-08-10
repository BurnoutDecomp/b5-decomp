#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupPoly.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// ============================================================================
// CgsGeometric::PolygonSoupPoly -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   CgsGeometric::PolygonSoupPoly::GetEdgeCosineUncompressed @ 0x82839568  (38)
//   CgsGeometric::PolygonSoupPoly::LoadEdgeCosines           @ 0x8283A120 (534)
//
// ⭐⭐ THE TWO ARE THE SAME FORMULA, DECODED INDEPENDENTLY AND MONTHS APART, and
// that is worth writing down: GetEdgeCosineUncompressed was reconstructed from a
// 38-instruction scalar body long before this wave; LoadEdgeCosines was decoded
// this wave out of a 534-instruction VMX body whose five vector constants are
// ZERO in the image and had to be recovered through their runtime initialisers.
// They agree exactly -- same numerator (flt_820DF65C), same 1.0f (flt_82001C98),
// same `8 << (code & 0x1F)`. Neither was derived from the other.
// ============================================================================

namespace CgsGeometric
{
    // ------------------------------------------------------------------------
    // Every constant below was read out of the X360 image.
    //
    //   flt_820DF65C  0x411DE9E7 = 9.86960506439209f   (the numerator)
    //   flt_82001C98  0x3F800000 = 1.0f
    //   flt_820DF4D0  0x34000000 = +1.1920929e-07f     (+FLT_EPSILON, assert hi)
    //   flt_82002514  0xB4000000 = -1.1920929e-07f     (-FLT_EPSILON, assert lo)
    //   flt_82002138  0x3C23D70A = 0.01f               (streamed into the message)
    //
    // ⚠️ CORRECTION, evidenced: GetEdgeCosineUncompressed's banner used to call
    // this numerator "pi^2". It is ONE ULP ABOVE float32(pi*pi): the image holds
    // 0x411DE9E7 and float32(pi^2) is 0x411DE9E6. The literal value below is
    // bit-identical to the image (and to the one that banner already carried);
    // only the claim that it IS pi^2 is withdrawn. It is deliberately not spelled
    // as a pi expression.
    //
    // The five VECTOR constants LoadEdgeCosines lvx128-loads all sit in a
    // zero-filled region (0x83000000..0x83040000 is 256 KB of zeros -- they are
    // NOT .rdata literals), so each was resolved through its C++ dynamic
    // initialiser, found by scanning all 30,084 export JSONs for the address:
    //
    //   unk_83039350  0x82C6DB00  CreateIntegerVector(0x08080808, 0x09090909,
    //                                                 0x0A0A0A0A, 0x0B0B0B0B)
    //                 -> a vperm control that splats byte poly[8+k] across word k
    //   unk_83039390  0x82C6DA98  splat(0x1F)     -> the 5-bit code mask
    //   unk_83039360  0x82C6DB90  splat(0x1C)     -> the vminuw shift clamp
    //   unk_83039210  0x82C6DB50  splat(8)        -> the vslw shift base
    //   unk_830393C0  0x82C6DAC8  splat(flt_820DF65C)
    // ------------------------------------------------------------------------
    namespace
    {
        const u32 KU32_EDGE_COSINE_CODE_MASK  = 0x1Fu;   // unk_83039390
        const u32 KU32_EDGE_COSINE_CODE_CLAMP = 0x1Cu;   // unk_83039360 (vminuw)
        const u32 KU32_EDGE_COSINE_SHIFT_BASE = 8u;      // unk_83039210 (vslw)

        const f32 KF_EDGE_COSINE_NUMERATOR = 9.86960506439209f;   // flt_820DF65C

        // flt_820DF4D0 / flt_82002514 -- the two bounds of the VMX-vs-scalar
        // agreement assert (CgsPolygonSoupPoly.h lines 244..247).
        const f32 KF_EDGE_COSINE_TOLERANCE_HI =  1.1920928955078125e-07f;
        const f32 KF_EDGE_COSINE_TOLERANCE_LO = -1.1920928955078125e-07f;

        // The VMX arm of LoadEdgeCosines, per edge code.
        //   vand  -> code & 0x1F
        //   vminuw-> min(code, 0x1C)
        //   vslw  -> 8u << code            (32-bit, hence the clamp)
        //   vcuxwfp128 (UNSIGNED convert, UIMM 0)
        //   vrefp128 + two Newton-Raphson steps -> 1/x
        //   vmulfp128 by the numerator, then vsubfp128 from 1.0f
        // ⚠️ PC LOWERING, FLAGGED: `vrefp128` + 2 NR is a ~23-bit reciprocal. A
        // true divide is used instead -- an x86 `rcpps` is a DIFFERENT estimate,
        // so imitating the shape would not be more faithful than being exact. The
        // MULTIPLY is kept (`K * (1/x)`, two roundings) rather than folded into a
        // divide, because the console multiplies and because that is precisely
        // what the tolerance assert below compares against the true divide.
        inline f32 DecodeEdgeCosineVMX(u8 lu8Code)
        {
            u32 luCode = static_cast<u32>(lu8Code) & KU32_EDGE_COSINE_CODE_MASK;
            if (luCode > KU32_EDGE_COSINE_CODE_CLAMP)
            {
                luCode = KU32_EDGE_COSINE_CODE_CLAMP;
            }
            const u32 luDenominator = KU32_EDGE_COSINE_SHIFT_BASE << luCode;
            const f32 lfDenominator = static_cast<f32>(luDenominator);
            return 1.0f - KF_EDGE_COSINE_NUMERATOR * (1.0f / lfDenominator);
        }

        inline void SplatInto(VecFloat& lrOut, f32 lfValue)
        {
            lrOut.x = lfValue;   // vspltw + stvx128 -- all four lanes equal
            lrOut.y = lfValue;
            lrOut.z = lfValue;
            lrOut.w = lfValue;
        }
    }

    // ------------------------------------------------------------------------
    // GetEdgeCosineUncompressed @ 0x82839568
    //
    //   ; assert liEdge in [0,4)  (CgsPolygonSoupPoly.h:156)
    //   add   r11, r31, r30        ; r11 = this + liEdge
    //   li    r10, 8
    //   lbz   r11, 8(r11)          ; byte = mauCompressedEdgeCosines[liEdge]
    //   clrldi r11, r11, 59        ; shift = byte & 0x1F  (low 5 bits)
    //   sld   r3, r10, r11         ; lValue = (u64)8 << shift
    //   bl    __u64tod             ; (double)lValue
    //   frsp  f13, f1              ; round to f32
    //   lfs   f0, flt_820DF65C     ; 9.86960506439209f (value read from the image)
    //   fdivs f13, f0, f13         ; numerator / value
    //   lfs   f0, flt_82001C98     ; 1.0f
    //   fsubs f1, f0, f13          ; 1.0f - (numerator / value)
    //   blr                        ; returns the f32 result widened to double in f1
    //
    // The single byte at +0x08+liEdge is a compressed exponent: its low 5 bits
    // give a power-of-two scale applied to 8, and the cosine is recovered as
    // 1 - K / (8 << shift). All float arithmetic is single-precision (frsp/
    // fdivs/fsubs); the result is returned as a double.
    //
    // ⭐ A logarithmic angle quantisation: code 0 gives -0.2337, 1 gives 0.3831,
    // 2 gives 0.6916, 3 gives 0.8458 ... tending to 1.0. Fine resolution near
    // cos == 1 (nearly-flat edges) and coarse for sharp ones. The range
    // [-0.2337, 1.0) sits inside the (-1.1, 1.1) window CgsTriangle4.cpp's
    // AOSTriangle::IsValid checks the outer two edge cosines against.
    // ⚠️ THIS SCALAR FORM HAS NO CLAMP: `sld` is a 64-BIT shift, so codes 29..31
    // do not wrap to zero the way LoadEdgeCosines' 32-bit `vslw` would without
    // its `vminuw` at 28. Both land within FLT_EPSILON of 1.0, which is why the
    // console's own cross-check tolerates the difference.
    // ------------------------------------------------------------------------
    double PolygonSoupPoly::GetEdgeCosineUncompressed(u32 liEdge) const
    {
        CGS_ASSERT(liEdge < 4u, "liEdge >= 0 && liEdge < 4");

        const u32 luShift = mauCompressedEdgeCosines[liEdge] & KU32_EDGE_COSINE_CODE_MASK;
        const u64 luValue = static_cast<u64>(KU32_EDGE_COSINE_SHIFT_BASE) << luShift;

        const f32 lfValue  = static_cast<f32>(luValue);                          // u64tod + frsp
        const f32 lfCosine = 1.0f - (KF_EDGE_COSINE_NUMERATOR / lfValue);        // fdivs + fsubs

        return static_cast<double>(lfCosine);
    }

    // ------------------------------------------------------------------------
    // LoadEdgeCosines @0x8283A120 (534)
    //
    //   for k in 0..3:  out[k] = splat(1.0f - 9.86960506439209f
    //                                        / (8 << min(poly[8+k] & 0x1F, 28)))
    //
    // Of the 534 instructions ~55 are the computation above and ~480 are a
    // SECOND, SCALAR computation of the same four values (`__u64tod` / `frsp` /
    // `fdivs` / `fsubs` -- i.e. GetEdgeCosineUncompressed's arithmetic, inlined
    // four times) plus EIGHT asserts that cross-check the two arms against each
    // other. Both halves are reproduced; the debug half is what the eight
    // `li r5, 0xEE..0xF7` assert lines belong to.
    // ------------------------------------------------------------------------
    void PolygonSoupPoly::LoadEdgeCosines(VecFloat& lOutEdge0, VecFloat& lOutEdge1,
                                          VecFloat& lOutEdge2, VecFloat& lOutEdge3) const
    {
        VecFloat* const lapOut[4] = { &lOutEdge0, &lOutEdge1, &lOutEdge2, &lOutEdge3 };

        for (s32 liEdge = 0; liEdge < 4; ++liEdge)
        {
            SplatInto(*lapOut[liEdge], DecodeEdgeCosineVMX(mauCompressedEdgeCosines[liEdge]));
        }

        // --- the debug half -------------------------------------------------
        // The console recomputes all four scalars first (0x8283A214..0x8283A29C),
        // fires the four NaN asserts (lines 238..241), and only then runs the
        // four agreement asserts (lines 244..247). Same order here.
        f32 lafScalar[4];
        for (s32 liEdge = 0; liEdge < 4; ++liEdge)
        {
            lafScalar[liEdge] = static_cast<f32>(GetEdgeCosineUncompressed(static_cast<u32>(liEdge)));
        }

        // `fcmpu cr6, fN, fN` + `bne` -- a value equals itself iff it is not NaN.
        // CgsPolygonSoupPoly.h:238 / :239 / :240 / :241. The console streams the
        // offending float into the assert buffer; per this family's convention
        // (CgsTriangle4.cpp) the literal is carried and the stream is not.
        CGS_ASSERT(lafScalar[0] == lafScalar[0], "LoadEdgeCosines: edge 0 cosine is NaN");
        CGS_ASSERT(lafScalar[1] == lafScalar[1], "LoadEdgeCosines: edge 1 cosine is NaN");
        CGS_ASSERT(lafScalar[2] == lafScalar[2], "LoadEdgeCosines: edge 2 cosine is NaN");
        CGS_ASSERT(lafScalar[3] == lafScalar[3], "LoadEdgeCosines: edge 3 cosine is NaN");

        // `vsubfp` the stored output against a splat of the scalar, then compare
        // lane 0 against +/-FLT_EPSILON. CgsPolygonSoupPoly.h:244 / :245 / :246 / :247.
        // ⭐ NOT a vacuous self-comparison on this port: the two arms still differ
        // (`K * (1/x)` is two roundings, `K / x` is one), and the scalar arm has
        // no shift clamp.
        for (s32 liEdge = 0; liEdge < 4; ++liEdge)
        {
            const f32 lfDelta = lapOut[liEdge]->x - lafScalar[liEdge];
            CGS_ASSERT(lfDelta <= KF_EDGE_COSINE_TOLERANCE_HI
                    && lfDelta >= KF_EDGE_COSINE_TOLERANCE_LO,
                       "LoadEdgeCosines: vector and scalar edge cosines disagree");
        }
    }
}

// The record is SERIALISED (it lives inside WORLDCOL.BIN) and pointer-free, so
// its console size is a contract this host must keep exactly.
namespace
{
    typedef char KI_POLYGON_SOUP_POLY_IS_12_BYTES
        [(sizeof(CgsGeometric::PolygonSoupPoly) == 12) ? 1 : -1];
    typedef char KI_POLYGON_SOUP_POLY_TAG_IS_U32
        [(sizeof(CgsGeometric::PolygonSoupPoly::muSurfaceTag) == 4) ? 1 : -1];
    typedef char KI_POLYGON_SOUP_POLY_EDGE_CODES_ARE_4
        [(sizeof(CgsGeometric::PolygonSoupPoly::mauCompressedEdgeCosines) == 4) ? 1 : -1];
}
