#include "types.hpp"

#include <cmath>

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x825BD208
//   (CgsGeometric::Triangle4::AOSTriangle::IsValid)
//
// The X360 body is hand-vectorised VMX/AltiVec (lvx128 / vcmpeqfp. / vmsum3fp128 /
// vrsqrtefp + Newton-Raphson refine / vcmpgtfp). Hex-Rays could not lower the
// intrinsics, so this is a SEMANTIC reconstruction of the recovered intent rather
// than a line-by-line port. Lowered to portable scalar float math:
//
//   Layout (AOS, 16-byte rows; `_R3` is the float base):
//     row 0  floats [0..3]   vertex A   (xyz used)
//     row 1  floats [4..7]   vertex B   (xyz used)
//     row 2  floats [8..11]  vertex C   (xyz used)
//     row 3  floats [12..15] face normal (xyz used, unit-length tested)
//     scalar floats [16],[17],[18]      plane/range values
//
//   Checks (AND of all):
//     1. The xyz of all four rows are finite. The VMX idiom `vcmpeqfp v,v,v`
//        (x == x) is a NaN test; the per-row vrlimi/vcmpeqfp follow-ups are the
//        same finiteness predicate broadcast across lanes.
//     2. The face normal (row 3) is unit length within an epsilon. The asm does
//        vmsum3fp128 (dot3) -> vrsqrtefp + two Newton-Raphson steps -> compares
//        |len - 1.0| against an .rdata epsilon (unk_82FB9EE0, value not in the
//        export; inferred small tolerance).
//     3. Each trailing scalar is in range:
//          [16] in (-1.1, 1.1)         (literals -1.1 / 1.1 in the asm)
//          [17] in (-1.1, 2.0999999)   (literals -1.1 / 2.0999999 in the asm)
//          [18] in (-1.1, 1.1)
//
// Called only by AOSTriangle::AssertIsValid (debug validation), so this is a
// best-effort faithful predicate. Flagged for a strong review: the unit-length
// epsilon and the exact lane-equality micro-checks are inferred, not byte-exact.

namespace CgsGeometric
{
    namespace Triangle4
    {
        // Inferred unit-length tolerance for the face normal (.rdata constant
        // unk_82FB9EE0 was not valued in the export).
        static const f32 KF_UNIT_LENGTH_EPSILON = 0.01f;

        struct AOSTriangle
        {
            f32 maRows[4][4];   // 3 vertices + face normal (xyzw per row)
            f32 mfRange0;       // [16]
            f32 mfRange1;       // [17]
            f32 mfRange2;       // [18]

            bool IsValid() const;
        };

        namespace
        {
            inline bool RowXyzFinite(const f32* lpRow)
            {
                // vcmpeqfp self-compare: a component equals itself iff it is not NaN.
                return lpRow[0] == lpRow[0]
                    && lpRow[1] == lpRow[1]
                    && lpRow[2] == lpRow[2];
            }
        }

        bool AOSTriangle::IsValid() const
        {
            // 1. all four rows have finite xyz
            for (int liRow = 0; liRow < 4; ++liRow)
            {
                if (!RowXyzFinite(maRows[liRow]))
                    return false;
            }

            // 2. face normal (row 3) is unit length within epsilon
            const f32* lpNormal = maRows[3];
            const f32 lfLenSq = lpNormal[0] * lpNormal[0]
                              + lpNormal[1] * lpNormal[1]
                              + lpNormal[2] * lpNormal[2];
            const f32 lfLen = sqrtf(lfLenSq);
            if (fabsf(lfLen - 1.0f) > KF_UNIT_LENGTH_EPSILON)
                return false;

            // 3. trailing scalars within their recovered ranges
            if (!(mfRange0 > -1.1f && mfRange0 < 1.1f))
                return false;
            if (!(mfRange1 > -1.1f && mfRange1 < 2.0999999f))
                return false;
            if (!(mfRange2 > -1.1f && mfRange2 < 1.1f))
                return false;

            return true;
        }
    }
}
