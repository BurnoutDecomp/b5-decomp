#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle4.h"

#include <cmath>

// ============================================================================
// CgsGeometric::Triangle4 -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   CgsGeometric::Triangle4::GetAOSTriangle       @ 0x825B2808
//   CgsGeometric::Triangle4::AOSTriangle::IsValid @ 0x825BD208
//
// Both X360 bodies are dense hand-vectorised VMX/AltiVec (lvx128 / vperm /
// vrlimi128 / vmsum3fp128 / vrsqrtefp + Newton-Raphson / vcmpeqfp). Hex-Rays
// could not lower the intrinsics, so these are SEMANTIC reconstructions of the
// recovered intent (the project's established Triangle4/Line/PackedOobb
// precedent), preserving the observed memory layout and store order, lowered to
// portable scalar float math. Cited by X360 address only.
// ============================================================================

namespace CgsGeometric
{
    // K_TRIANGLE_NORMAL_TOLERANCE. The .rdata value is not present in the
    // export; a small unit-length epsilon is used here and FLAGGED.
    const VecFloat K_TRIANGLE_NORMAL_TOLERANCE = { 0.01f, 0.01f, 0.01f, 0.01f };

    namespace
    {
        // vcmpeqfp self-compare per lane: a component equals itself iff not NaN.
        inline bool Vec3Finite(const Vector3& lrV)
        {
            return lrV.x == lrV.x && lrV.y == lrV.y && lrV.z == lrV.z;
        }
    }

    // ------------------------------------------------------------------------
    // GetAOSTriangle @ 0x825B2808
    //
    // Extracts triangle `liLane` out of the SoA batch into roOut. The asm uses a
    // per-element `lvsl`-built permute (selector = liLane*4 bytes) to gather lane
    // `liLane` of each component vector; lowered here to a direct lane index.
    //
    // Store order in the asm (preserved logically here):
    //   roOut.mVertex0 (+0x00)   <- (mVertex0X[i], mVertex0Y[i], mVertex0Z[i])
    //   roOut.mVertex1 (+0x10)   <- (mVertex1X[i], mVertex1Y[i], mVertex1Z[i])
    //   roOut.mVertex2 (+0x20)   <- (mVertex2X[i], mVertex2Y[i], mVertex2Z[i])
    //   roOut.mNormal  (+0x30)   <- normalize( edgeB x edgeA )
    //   roOut.mfEdgeCosine0/1/2 (+0x40/+0x44/+0x48) <- mEdgeNCosigns[i]
    //
    // where edgeB = mVertex1 - mVertex0 and edgeA = mVertex2 - mVertex1 (the two
    // vsubfp results the asm feeds, with the 0x63 swizzles, into the cross-product
    // nmsub idiom). The normal is normalised via vrsqrtefp + two Newton-Raphson
    // refinement steps; reproduced here with a single rsqrt.
    // ------------------------------------------------------------------------
    void Triangle4::GetAOSTriangle(s32 liLane, AOSTriangle& roOut) const
    {
        const s32 i = liLane;

        // Gather lane `i` of each SoA component into the AOS vertices.
        roOut.mVertex0.x = (&mVertex0X.x)[i];
        roOut.mVertex0.y = (&mVertex0Y.x)[i];
        roOut.mVertex0.z = (&mVertex0Z.x)[i];
        roOut.mVertex0.w = 0.0f;

        roOut.mVertex1.x = (&mVertex1X.x)[i];
        roOut.mVertex1.y = (&mVertex1Y.x)[i];
        roOut.mVertex1.z = (&mVertex1Z.x)[i];
        roOut.mVertex1.w = 0.0f;

        roOut.mVertex2.x = (&mVertex2X.x)[i];
        roOut.mVertex2.y = (&mVertex2Y.x)[i];
        roOut.mVertex2.z = (&mVertex2Z.x)[i];
        roOut.mVertex2.w = 0.0f;

        // edges (matching the two vsubfp results)
        const f32 lfBx = roOut.mVertex1.x - roOut.mVertex0.x; // edgeB = V1 - V0
        const f32 lfBy = roOut.mVertex1.y - roOut.mVertex0.y;
        const f32 lfBz = roOut.mVertex1.z - roOut.mVertex0.z;
        const f32 lfAx = roOut.mVertex2.x - roOut.mVertex1.x; // edgeA = V2 - V1
        const f32 lfAy = roOut.mVertex2.y - roOut.mVertex1.y;
        const f32 lfAz = roOut.mVertex2.z - roOut.mVertex1.z;

        // normal = edgeB x edgeA (the vpermwi 0x63 swizzle + vnmsubfp idiom)
        f32 lfNx = lfBy * lfAz - lfBz * lfAy;
        f32 lfNy = lfBz * lfAx - lfBx * lfAz;
        f32 lfNz = lfBx * lfAy - lfBy * lfAx;

        // normalise: lenSq = dot3(n,n) (vmsum3fp128); inv = 1/sqrt(lenSq)
        // (vrsqrtefp + two Newton-Raphson refinements).
        const f32 lfLenSq = lfNx * lfNx + lfNy * lfNy + lfNz * lfNz;
        const f32 lfInv = (lfLenSq > 0.0f) ? (1.0f / std::sqrt(lfLenSq)) : 0.0f;
        lfNx *= lfInv;
        lfNy *= lfInv;
        lfNz *= lfInv;

        roOut.mNormal.x = lfNx;
        roOut.mNormal.y = lfNy;
        roOut.mNormal.z = lfNz;
        roOut.mNormal.w = 0.0f;

        // trailing per-lane edge cosines (lfsx loads of lane `i` of each Cosigns vec)
        roOut.mfEdgeCosine0 = (&mEdge0Cosigns.x)[i];
        roOut.mfEdgeCosine1 = (&mEdge1Cosigns.x)[i];
        roOut.mfEdgeCosine2 = (&mEdge2Cosigns.x)[i];
    }

    // ------------------------------------------------------------------------
    // AOSTriangle::IsValid @ 0x825BD208
    //
    // Debug validity predicate. The X360 body is VMX (lvx128 of the four AOS rows,
    // vcmpeqfp. self-compares for finiteness, vmsum3fp128 dot3 + vrsqrtefp + NR for
    // the normal unit-length test, and vcmpgtfp range checks on the trailing
    // scalars against .rdata literals -1.1 / 1.1 / 2.0999999). Lowered to scalar:
    //
    //   1. all four xyz rows are finite (x==x lane self-compare)
    //   2. the face normal is unit length within K_TRIANGLE_NORMAL_TOLERANCE
    //   3. the three edge cosines lie in their recovered ranges:
    //        cosine0 in (-1.1, 1.1)
    //        cosine1 in (-1.1, 2.0999999)
    //        cosine2 in (-1.1, 1.1)
    //
    // FLAGGED: the unit-length epsilon (.rdata unk_82FB9EE0, unvalued in the
    // export) is inferred. The range literals are read from the asm.
    // ------------------------------------------------------------------------
    bool Triangle4::AOSTriangle::IsValid() const
    {
        if (!Vec3Finite(mVertex0)) return false;
        if (!Vec3Finite(mVertex1)) return false;
        if (!Vec3Finite(mVertex2)) return false;
        if (!Vec3Finite(mNormal))  return false;

        const f32 lfLenSq = mNormal.x * mNormal.x
                          + mNormal.y * mNormal.y
                          + mNormal.z * mNormal.z;
        const f32 lfLen = std::sqrt(lfLenSq);
        if (std::fabs(lfLen - 1.0f) > K_TRIANGLE_NORMAL_TOLERANCE.x)
            return false;

        if (!(mfEdgeCosine0 > -1.1f && mfEdgeCosine0 < 1.1f))        return false;
        if (!(mfEdgeCosine1 > -1.1f && mfEdgeCosine1 < 2.0999999f))  return false;
        if (!(mfEdgeCosine2 > -1.1f && mfEdgeCosine2 < 1.1f))        return false;

        return true;
    }
}
