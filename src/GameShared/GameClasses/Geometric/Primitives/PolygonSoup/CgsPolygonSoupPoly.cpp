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
    using rw::math::vpu::VectorIntrinsic;

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

        inline VectorIntrinsic VectorFromWords(u32 lu0, u32 lu1, u32 lu2, u32 lu3)
        {
            rw::math::vpu::detail::VectorIntrinsicUnion lValue;
            lValue.scalarUInt[0] = lu0;
            lValue.scalarUInt[1] = lu1;
            lValue.scalarUInt[2] = lu2;
            lValue.scalarUInt[3] = lu3;
            return lValue.vector;
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

        // The 12-byte record can start at any four-byte boundary. Match the native
        // lvlx/lvrx pair, then use the recovered CRT vperm words to gather code bytes
        // 8..11 into the four VMX lanes.
        const VectorIntrinsic lvPoly = __loadunalignedvector(this);
        const VectorIntrinsic lvCodePermute = VectorFromWords(
            0x08080808u, 0x09090909u, 0x0A0A0A0Au, 0x0B0B0B0Bu);
        const VectorIntrinsic lvCodeMask = VectorFromWords(
            KU32_EDGE_COSINE_CODE_MASK, KU32_EDGE_COSINE_CODE_MASK,
            KU32_EDGE_COSINE_CODE_MASK, KU32_EDGE_COSINE_CODE_MASK);
        const VectorIntrinsic lvCodeClamp = VectorFromWords(
            KU32_EDGE_COSINE_CODE_CLAMP, KU32_EDGE_COSINE_CODE_CLAMP,
            KU32_EDGE_COSINE_CODE_CLAMP, KU32_EDGE_COSINE_CODE_CLAMP);
        const VectorIntrinsic lvShiftBase = VectorFromWords(
            KU32_EDGE_COSINE_SHIFT_BASE, KU32_EDGE_COSINE_SHIFT_BASE,
            KU32_EDGE_COSINE_SHIFT_BASE, KU32_EDGE_COSINE_SHIFT_BASE);
        const u32 luNumeratorBits = 0x411DE9E7u;
        const VectorIntrinsic lvNumerator = VectorFromWords(
            luNumeratorBits, luNumeratorBits, luNumeratorBits, luNumeratorBits);
        const VectorIntrinsic lvOne = __vcfsx(__vspltisw(1), 0);

        const VectorIntrinsic lvCodes = __vminuw(
            __vand(__vperm(lvPoly, lvPoly, lvCodePermute), lvCodeMask), lvCodeClamp);
        const VectorIntrinsic lvIntegerDenominator = __vslw(lvShiftBase, lvCodes);
        const VectorIntrinsic lvDenominator = __vcuxwfp(lvIntegerDenominator, 0);

        VectorIntrinsic lvReciprocal = __vrefp(lvDenominator);
        for (s32 liStep = 0; liStep < 2; ++liStep)
        {
            const VectorIntrinsic lvResidual = __vnmsubfp(lvReciprocal, lvDenominator, lvOne);
            lvReciprocal = __vmaddfp(lvReciprocal, lvResidual, lvReciprocal);
        }
        const VectorIntrinsic lvProduct = __vmulfp(lvNumerator, lvReciprocal);
        const VectorIntrinsic lvEdgeCosines = __vsubfp(lvOne, lvProduct);

        lOutEdge0 = VecFloat(lvEdgeCosines, 0);
        lOutEdge1 = VecFloat(lvEdgeCosines, 1);
        lOutEdge2 = VecFloat(lvEdgeCosines, 2);
        lOutEdge3 = VecFloat(lvEdgeCosines, 3);

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
        // The native vector arm and scalar arm retain their separate X360 shift and
        // reciprocal paths; compare their stored values with the original epsilon.
        for (s32 liEdge = 0; liEdge < 4; ++liEdge)
        {
            const f32 lfDelta = lapOut[liEdge]->GetFloat() - lafScalar[liEdge];
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
        [(sizeof(((CgsGeometric::PolygonSoupPoly*)0)->muSurfaceTag) == 4) ? 1 : -1];
    typedef char KI_POLYGON_SOUP_POLY_EDGE_CODES_ARE_4
        [(sizeof(((CgsGeometric::PolygonSoupPoly*)0)->mauCompressedEdgeCosines) == 4) ? 1 : -1];
}
