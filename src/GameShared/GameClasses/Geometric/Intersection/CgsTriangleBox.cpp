#include "GameShared/GameClasses/Geometric/Intersection/CgsTriangleBox.h"

#include <cmath>
#include <cstring>   // std::memcpy (ClipBoxEdgeAgainstTriangle's mask bit pattern)
#include <limits>    // std::numeric_limits (ClipBoxEdgeAgainstTriangle's refined reciprocal)

// ============================================================================
// CgsGeometric::BT::TriangleData::Construct -- reconstructed from
// BURNOUT_X360_ARTIST.XEX @ 0x82839D30.
//
// The X360 body is dense hand-vectorised VMX: three vsubfp edge subtractions,
// then FOUR interleaved cross-product-plus-normalize chains (one for the face
// normal, three for the outward in-plane edge normals) built from the same
// repeated idiom --
//
//     v8 = vpermwi128(edgeB, 0x63)          # yzx-rotate edgeB
//     v7 = vpermwi128(edgeA, 0x63)          # yzx-rotate edgeA
//     v8 = edgeA * v8                       # edgeA * yzx(edgeB)
//     vD = vnmsubfp(v7, v8, edgeB)          # edgeB - yzx(edgeA) * (edgeA * yzx(edgeB))
//     ... vmsum3fp128 (dot3) + vrsqrtefp + two Newton-Raphson refinement steps ...
//
// -- the identical instruction sequence (vpermwi128 x2 / vmulfp128 / vnmsubfp
// then vmsum3fp128 / vrsqrtefp + 2x NR) this project already reconstructs as
// a SEMANTIC cross-product + rsqrt-Newton-Raphson normalize in
// CgsGeometric::Triangle4::GetAOSTriangle (edgeB=mVertex1-mVertex0,
// edgeA=mVertex2-mVertex1) and in CgsGraphics::PackedOobb::ToMatrix's
// quaternion normalize -- see those TUs for the established precedent this
// follows. Hex-Rays cannot lower these intrinsics to a signature, so this is
// a SEMANTIC reconstruction of the recovered intent -- portable per-lane
// float math on the named Vector3/VecFloat members -- preserving the
// observed store order and each output's data dependency, not a literal
// per-VMX-register translation. The vrsqrtefp + Newton-Raphson chain
// converges to the exact reciprocal-square-root (same convention as every
// prior VMX-normalize reconstruction in this project), so each normalize is
// lowered to 1.0f/std::sqrt(lenSq) guarded by a >0 check (matching
// CgsTriangle4::GetAOSTriangle's and CgsPackedOobb::ToMatrix's normalize
// guard for a degenerate zero-length input).
//
// STORE ORDER (proves the layout in the header, matches the DWARF for this
// source path):
//   this+0x00 <- lVert0                                (mVertex0)
//   this+0x10 <- lVert1                                (mVertex1)
//   this+0x20 <- lVert2                                (mVertex2)
//   this+0x50 <- mEdge12 = lVert2 - lVert1
//   this+0x40 <- mEdge01 = lVert1 - lVert0
//   this+0x60 <- mEdge20 = lVert0 - lVert2
//   this+0x30 <- mNormal = normalize(cross(mEdge01, mEdge12))
//   this+0x70 <- mOuter01 = normalize(cross(mEdge01, mNormal))
//   this+0xA0 <- mPlaneOffset = dot3(mVertex0, mNormal)
//   this+0x80 <- mOuter12 = normalize(cross(mEdge12, mNormal))
//   this+0x90 <- mOuter20 = normalize(cross(mEdge20, mNormal))
//
// The mOuterXX vectors are the standard "triangle prism" separating-axis clip
// planes: each lies in the triangle's plane (perpendicular to mNormal),
// perpendicular to its own edge, forming the outward side walls
// ClipBoxEdgeAgainstTriangle/ClipTriEdgeAgainstBox clip a box edge against.
//
// FLAGGED: the asm interleaves the three mOuterXX chains' VMX scratch
// registers for instruction scheduling (independent computations sharing the
// v8..v13 register file across instructions); this reconstruction reproduces
// the store order and each output's data dependency (proven by which edge/
// normal registers feed each nmsub/msum3 chain), not the register schedule
// itself, per this project's established VMX semantic-lowering precedent.
// ============================================================================

namespace CgsGeometric
{
    namespace BT
    {
        namespace
        {
            inline Vector3 EdgeSubtract(const Vector3& lrA, const Vector3& lrB)
            {
                // vsubfp
                Vector3 lResult;
                lResult.x = lrA.x - lrB.x;
                lResult.y = lrA.y - lrB.y;
                lResult.z = lrA.z - lrB.z;
                lResult.w = 0.0f;
                return lResult;
            }

            // Cross product via the recovered edge-subtraction VMX idiom
            // (vpermwi128 yzx-rotate x2 / vmulfp128 / vnmsubfp), lowered to
            // the standard scalar cross product.
            inline Vector3 EdgeCross(const Vector3& lrA, const Vector3& lrB)
            {
                Vector3 lResult;
                lResult.x = lrA.y * lrB.z - lrA.z * lrB.y;
                lResult.y = lrA.z * lrB.x - lrA.x * lrB.z;
                lResult.z = lrA.x * lrB.y - lrA.y * lrB.x;
                lResult.w = 0.0f;
                return lResult;
            }

            // Normalize via vmsum3fp128 (dot3) + vrsqrtefp with two
            // Newton-Raphson refinement steps -- converges to the exact
            // 1/sqrt(lenSq); guarded the same way as every other VMX-normalize
            // reconstruction in this project (CgsTriangle4/CgsPackedOobb).
            inline Vector3 EdgeNormalize(const Vector3& lrV)
            {
                const f32 lfLenSq = lrV.x * lrV.x + lrV.y * lrV.y + lrV.z * lrV.z;
                const f32 lfInv = (lfLenSq > 0.0f) ? (1.0f / std::sqrt(lfLenSq)) : 0.0f;

                Vector3 lResult;
                lResult.x = lrV.x * lfInv;
                lResult.y = lrV.y * lfInv;
                lResult.z = lrV.z * lfInv;
                lResult.w = 0.0f;
                return lResult;
            }

            inline VecFloat EdgeDot(const Vector3& lrA, const Vector3& lrB)
            {
                // vmsum3fp128
                const f32 lfDot = lrA.x * lrB.x + lrA.y * lrB.y + lrA.z * lrB.z;
                return VecFloat{ lfDot, lfDot, lfDot, lfDot };
            }
        }

        void TriangleData::Construct(const Vector3& lVert0, const Vector3& lVert1, const Vector3& lVert2)
        {
            mVertex0 = lVert0;
            mVertex1 = lVert1;
            mVertex2 = lVert2;

            mEdge01 = EdgeSubtract(lVert1, lVert0);
            mEdge12 = EdgeSubtract(lVert2, lVert1);
            mEdge20 = EdgeSubtract(lVert0, lVert2);

            mNormal = EdgeNormalize(EdgeCross(mEdge01, mEdge12));

            mOuter01 = EdgeNormalize(EdgeCross(mEdge01, mNormal));
            mPlaneOffset = EdgeDot(mVertex0, mNormal);
            mOuter12 = EdgeNormalize(EdgeCross(mEdge12, mNormal));
            mOuter20 = EdgeNormalize(EdgeCross(mEdge20, mNormal));
        }
    }
}

// ============================================================================
// CgsGeometric::ClipBoxEdgeAgainstTriangle (56 instructions, straight-line VMX).
//
//   Arguments: the TriangleData, the edge's two BoxVertexData, lPoint.
//   lPlaneLength = end.mPlaneOffset - start.mPlaneOffset;
//   lParam       = start.mPlaneOffsetFromTriangle * (1 / lPlaneLength)
//                  (`vrefp` + two Newton-Raphson steps, then vmulfp128);
//   lPosition    = (end.mPosition - start.mPosition) * lParam + start.mPosition, stored
//                  to lPoint before any test;
//   lrU/lrV/lrW  = (lPosition - mVertexN) . mOuterNN for the three walls (vmsum3fp128),
//                  each tested as (0 >= x);
//   result       = (all three | none of the three) & (lPlaneLength != 0)
//                  & (lParam >= 0) & !(lParam > 1).
// Local names are the declaration's own.
// PC LOWERING: the refined reciprocal is a divide, except that a zero or infinite
// lPlaneLength gives the console's NaN (its first Newton-Raphson step multiplies 0 by
// infinity), which reaches lPoint; the fused multiply-add is a multiply then an add.
// Compared with an interpreter of the instruction stream on 1,000 random edges (a sixth of
// them parallel to the plane): identical masks and points.
// ============================================================================

namespace CgsGeometric
{
    rw::math::vpu::MaskScalar ClipBoxEdgeAgainstTriangle(const BT::TriangleData&  lTriangleData,
                                                         const BT::BoxVertexData& lLineStartVertex,
                                                         const BT::BoxVertexData& lLineEndVertex,
                                                         Vector3&                 lPoint)
    {
        const Vector3& lLineStart = lLineStartVertex.mPosition;
        const Vector3& lLineEnd   = lLineEndVertex.mPosition;

        const f32 lfStartPlaneOffset = lLineStartVertex.mPlaneOffset.x;
        const f32 lfEndPlaneOffset   = lLineEndVertex.mPlaneOffset.x;
        const f32 lfStartTriOffset   = lLineStartVertex.mPlaneOffsetFromTriangle.x;

        const f32 lfPlaneLength = lfEndPlaneOffset - lfStartPlaneOffset;
        const f32 lfReciprocal  = 1.0f / lfPlaneLength;
        const f32 lfParam       = lfStartTriOffset
                                * ((std::isfinite(lfReciprocal) && lfReciprocal != 0.0f)
                                       ? lfReciprocal : std::numeric_limits<f32>::quiet_NaN());

        Vector3 lPosition;
        lPosition.x = (lLineEnd.x - lLineStart.x) * lfParam + lLineStart.x;
        lPosition.y = (lLineEnd.y - lLineStart.y) * lfParam + lLineStart.y;
        lPosition.z = (lLineEnd.z - lLineStart.z) * lfParam + lLineStart.z;
        lPosition.w = (lLineEnd.w - lLineStart.w) * lfParam + lLineStart.w;
        lPoint = lPosition;

        const Vector3& lVert0 = lTriangleData.mVertex0;
        const Vector3& lVert1 = lTriangleData.mVertex1;
        const Vector3& lVert2 = lTriangleData.mVertex2;
        const Vector3& lOut0  = lTriangleData.mOuter01;
        const Vector3& lOut1  = lTriangleData.mOuter12;
        const Vector3& lOut2  = lTriangleData.mOuter20;

        const f32 lfU = (lPosition.x - lVert0.x) * lOut0.x + (lPosition.y - lVert0.y) * lOut0.y + (lPosition.z - lVert0.z) * lOut0.z;
        const f32 lfV = (lPosition.x - lVert1.x) * lOut1.x + (lPosition.y - lVert1.y) * lOut1.y + (lPosition.z - lVert1.z) * lOut1.z;
        const f32 lfW = (lPosition.x - lVert2.x) * lOut2.x + (lPosition.y - lVert2.y) * lOut2.y + (lPosition.z - lVert2.z) * lOut2.z;

        const bool lbU = (0.0f >= lfU);
        const bool lbV = (0.0f >= lfV);
        const bool lbW = (0.0f >= lfW);

        const bool lbAllPositive        = lbU && lbV && lbW;
        const bool lbAllNegative        = !lbU && !lbV && !lbW;
        const bool lbRayIntersects      = lbAllPositive || lbAllNegative;
        const bool lbNoneParallel       = !(lfPlaneLength == 0.0f);
        const bool lbLinePlaneIntersects = (lfParam >= 0.0f) && !(lfParam > 1.0f);

        const bool lbResult = lbRayIntersects && lbNoneParallel && lbLinePlaneIntersects;

        rw::math::vpu::MaskScalar lResult;
        const u32 luLane = lbResult ? 0xFFFFFFFFu : 0u;
        std::memcpy(&lResult.x, &luLane, sizeof(u32));
        std::memcpy(&lResult.y, &luLane, sizeof(u32));
        std::memcpy(&lResult.z, &luLane, sizeof(u32));
        std::memcpy(&lResult.w, &luLane, sizeof(u32));
        return lResult;
    }
}
