#include "GameShared/GameClasses/Geometric/Intersection/CgsTriangleBox.h"

#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsBox.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "rw/math/vpu/matrix44affine_operation.h"

#include <cmath>
#include <cstring>   // std::memcpy (ClipBoxEdgeAgainstTriangle's mask bit pattern)
#include <limits>    // std::numeric_limits (ClipBoxEdgeAgainstTriangle's refined reciprocal)

namespace CgsGeometric
{
    namespace
    {
        const char* const klpTriangleBoxOriginalFile =
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\geometric\\Intersection/CgsTriangleBox.cpp";

        inline f32 Reciprocal3(f32 lfValue)
        {
            f32 lfReciprocal = 1.0f / lfValue;
            for (s32 liStep = 0; liStep < 3; ++liStep)
            {
                const f32 lfError = (-lfValue) * lfReciprocal + 1.0f;
                lfReciprocal = lfReciprocal * lfError + lfReciprocal;
            }
            return lfReciprocal;
        }

        inline bool IsFiniteFloat(f32 lfValue)
        {
            u32 luBits;
            std::memcpy(&luBits, &lfValue, sizeof(luBits));
            return (luBits & 0x7F800000u) != 0x7F800000u;
        }

        inline __vector4 SplatFloat(f32 lfValue)
        {
            return __vspltw(__lvlx(&lfValue, 0), 0);
        }

        inline Vector3 MakeZeroVector3()
        {
            return Vector3(__vspltisw(0));
        }

        inline Vector3 MakeVector3W(f32 lfX, f32 lfY, f32 lfZ, f32 lfW)
        {
            Vector3 lValue(lfX, lfY, lfZ);
            lValue.mV.v[3] = lfW;
            return lValue;
        }

        inline Vector3 PointAtLine(const Vector3& lrStart, const f32* lpDirection, f32 lfParam)
        {
            Vector3 lPoint;
            lPoint.mV.v[0] = lpDirection[0] * lfParam + lrStart.mV.v[0];
            lPoint.mV.v[1] = lpDirection[1] * lfParam + lrStart.mV.v[1];
            lPoint.mV.v[2] = lpDirection[2] * lfParam + lrStart.mV.v[2];
            lPoint.mV.v[3] = lpDirection[3] * lfParam + lrStart.mV.v[3];
            return lPoint;
        }

        inline rw::math::vpu::MaskScalar MakeTriangleBoxMask(bool lbValue)
        {
            return rw::math::vpu::MaskScalar(lbValue);
        }

        inline VecFloat MakeSplat(f32 lfValue)
        {
            VecFloat lResult;
            lResult.mV = SplatFloat(lfValue);
            return lResult;
        }

        inline bool IsValid3(const Vector3& lrVector)
        {
            // X360's vcmpeqfp x,x test rejects NaNs but permits infinities.
            return (lrVector.mV.v[0] == lrVector.mV.v[0])
                && (lrVector.mV.v[1] == lrVector.mV.v[1])
                && (lrVector.mV.v[2] == lrVector.mV.v[2]);
        }

        inline void ReportTriangleBoxAssert(const char* lpcExpression, s32 liLine)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lpcExpression, klpTriangleBoxOriginalFile, liLine);
            CgsDev::Assert::EndAssert();
        }
    }
}

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
                lResult.mV.v[0] = lrA.mV.v[0] - lrB.mV.v[0];
                lResult.mV.v[1] = lrA.mV.v[1] - lrB.mV.v[1];
                lResult.mV.v[2] = lrA.mV.v[2] - lrB.mV.v[2];
                lResult.mV.v[3] = 0.0f;
                return lResult;
            }

            // Cross product via the recovered edge-subtraction VMX idiom
            // (vpermwi128 yzx-rotate x2 / vmulfp128 / vnmsubfp), lowered to
            // the standard scalar cross product.
            inline Vector3 EdgeCross(const Vector3& lrA, const Vector3& lrB)
            {
                Vector3 lResult;
                lResult.mV.v[0] = lrA.mV.v[1] * lrB.mV.v[2] - lrA.mV.v[2] * lrB.mV.v[1];
                lResult.mV.v[1] = lrA.mV.v[2] * lrB.mV.v[0] - lrA.mV.v[0] * lrB.mV.v[2];
                lResult.mV.v[2] = lrA.mV.v[0] * lrB.mV.v[1] - lrA.mV.v[1] * lrB.mV.v[0];
                lResult.mV.v[3] = 0.0f;
                return lResult;
            }

            // Normalize via vmsum3fp128 (dot3) + vrsqrtefp with two
            // Newton-Raphson refinement steps -- converges to the exact
            // 1/sqrt(lenSq); guarded the same way as every other VMX-normalize
            // reconstruction in this project (CgsTriangle4/CgsPackedOobb).
            inline Vector3 EdgeNormalize(const Vector3& lrV)
            {
                const f32 lfLenSq = lrV.mV.v[0] * lrV.mV.v[0] + lrV.mV.v[1] * lrV.mV.v[1] + lrV.mV.v[2] * lrV.mV.v[2];
                const f32 lfInv = (lfLenSq > 0.0f) ? (1.0f / std::sqrt(lfLenSq)) : 0.0f;

                Vector3 lResult;
                lResult.mV.v[0] = lrV.mV.v[0] * lfInv;
                lResult.mV.v[1] = lrV.mV.v[1] * lfInv;
                lResult.mV.v[2] = lrV.mV.v[2] * lfInv;
                lResult.mV.v[3] = 0.0f;
                return lResult;
            }

            inline VecFloat EdgeDot(const Vector3& lrA, const Vector3& lrB)
            {
                // vmsum3fp128
                const f32 lfDot = lrA.mV.v[0] * lrB.mV.v[0] + lrA.mV.v[1] * lrB.mV.v[1] + lrA.mV.v[2] * lrB.mV.v[2];
                VecFloat lResult;
                lResult.SetFloat(lfDot);
                return lResult;
            }
        }

        void TriangleData::Construct(Vector3 lVert0, Vector3 lVert1, Vector3 lVert2)
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

        const f32 lfStartPlaneOffset = lLineStartVertex.mPlaneOffset.mV.v[0];
        const f32 lfEndPlaneOffset   = lLineEndVertex.mPlaneOffset.mV.v[0];
        const f32 lfStartTriOffset   = lLineStartVertex.mPlaneOffsetFromTriangle.mV.v[0];

        const f32 lfPlaneLength = lfEndPlaneOffset - lfStartPlaneOffset;
        const f32 lfReciprocal  = 1.0f / lfPlaneLength;
        const f32 lfParam       = lfStartTriOffset
                                * ((IsFiniteFloat(lfReciprocal) && lfReciprocal != 0.0f)
                                       ? lfReciprocal : std::numeric_limits<f32>::quiet_NaN());

        Vector3 lPosition;
        lPosition.mV.v[0] = (lLineEnd.mV.v[0] - lLineStart.mV.v[0]) * lfParam + lLineStart.mV.v[0];
        lPosition.mV.v[1] = (lLineEnd.mV.v[1] - lLineStart.mV.v[1]) * lfParam + lLineStart.mV.v[1];
        lPosition.mV.v[2] = (lLineEnd.mV.v[2] - lLineStart.mV.v[2]) * lfParam + lLineStart.mV.v[2];
        lPosition.mV.v[3] = (lLineEnd.mV.v[3] - lLineStart.mV.v[3]) * lfParam + lLineStart.mV.v[3];
        lPoint = lPosition;

        const Vector3& lVert0 = lTriangleData.mVertex0;
        const Vector3& lVert1 = lTriangleData.mVertex1;
        const Vector3& lVert2 = lTriangleData.mVertex2;
        const Vector3& lOut0  = lTriangleData.mOuter01;
        const Vector3& lOut1  = lTriangleData.mOuter12;
        const Vector3& lOut2  = lTriangleData.mOuter20;

        const f32 lfU = (lPosition.mV.v[0] - lVert0.mV.v[0]) * lOut0.mV.v[0] + (lPosition.mV.v[1] - lVert0.mV.v[1]) * lOut0.mV.v[1] + (lPosition.mV.v[2] - lVert0.mV.v[2]) * lOut0.mV.v[2];
        const f32 lfV = (lPosition.mV.v[0] - lVert1.mV.v[0]) * lOut1.mV.v[0] + (lPosition.mV.v[1] - lVert1.mV.v[1]) * lOut1.mV.v[1] + (lPosition.mV.v[2] - lVert1.mV.v[2]) * lOut1.mV.v[2];
        const f32 lfW = (lPosition.mV.v[0] - lVert2.mV.v[0]) * lOut2.mV.v[0] + (lPosition.mV.v[1] - lVert2.mV.v[1]) * lOut2.mV.v[1] + (lPosition.mV.v[2] - lVert2.mV.v[2]) * lOut2.mV.v[2];

        const bool lbU = (0.0f >= lfU);
        const bool lbV = (0.0f >= lfV);
        const bool lbW = (0.0f >= lfW);

        const bool lbAllPositive        = lbU && lbV && lbW;
        const bool lbAllNegative        = !lbU && !lbV && !lbW;
        const bool lbRayIntersects      = lbAllPositive || lbAllNegative;
        const bool lbNoneParallel       = !(lfPlaneLength == 0.0f);
        const bool lbLinePlaneIntersects = (lfParam >= 0.0f) && !(lfParam > 1.0f);

        const bool lbResult = lbRayIntersects && lbNoneParallel && lbLinePlaneIntersects;

        return MakeTriangleBoxMask(lbResult);
    }
}

// ============================================================================
// CgsGeometric::ClipTriEdgeAgainstBox @ 0x8283FF90.
//
// The X360 body computes the six segment/box-face candidates with vrefp plus
// three reciprocal refinements, checks each candidate against the segment and
// the other two inclusive box slabs, then places the first two valid face
// candidates in its two Vector3 slots. Candidate order is max-X, min-X, max-Y,
// min-Y, max-Z, min-Z; equal points on different faces remain separate. This scalar
// lowering retains that order, comparison inclusivity, and fused point eval.
// ============================================================================

namespace CgsGeometric
{
    void ClipTriEdgeAgainstBox(Vector3* const                  lpaDest,
                               rw::math::vpu::MaskScalar* const lpaDestValid,
                               const AxisAlignedBox&            lBox,
                               Vector3                          lLineStart,
                               Vector3                          lLineEnd)
    {
        const f32 lafStart[3] = { lLineStart.mV.v[0], lLineStart.mV.v[1], lLineStart.mV.v[2] };
        const f32 lafEnd[3]   = { lLineEnd.mV.v[0], lLineEnd.mV.v[1], lLineEnd.mV.v[2] };
        const f32 lafMin[3]   = { lBox.mMin.mV.v[0], lBox.mMin.mV.v[1], lBox.mMin.mV.v[2] };
        const f32 lafMax[3]   = { lBox.mMax.mV.v[0], lBox.mMax.mV.v[1], lBox.mMax.mV.v[2] };

        const f32 lafDirection[4] = {
            lLineEnd.mV.v[0] - lLineStart.mV.v[0],
            lLineEnd.mV.v[1] - lLineStart.mV.v[1],
            lLineEnd.mV.v[2] - lLineStart.mV.v[2],
            lLineEnd.mV.v[3] - lLineStart.mV.v[3]
        };

        const f32 lafReciprocal[3] = {
            Reciprocal3(lafDirection[0]),
            Reciprocal3(lafDirection[1]),
            Reciprocal3(lafDirection[2])
        };
        Vector3 laCandidates[6];
        bool lbCandidateValid[6] = { false, false, false, false, false, false };

        // Selection follows the original alternating maximum/minimum faces.
        // For a zero direction lane
        // the refined reciprocal becomes NaN and both segment comparisons fail.
        for (s32 liAxis = 0; liAxis < 3; ++liAxis)
        {
            for (s32 liSide = 0; liSide < 2; ++liSide)
            {
                const f32 lfBoundary = (liSide == 0) ? lafMax[liAxis] : lafMin[liAxis];
                const f32 lfParam = (lfBoundary - lafStart[liAxis]) * lafReciprocal[liAxis];
                const bool lbOnSegment = (1.0f >= lfParam) && !(0.0f > lfParam);
                if (!lbOnSegment)
                {
                    continue;
                }

                const s32 liCandidate = liAxis * 2 + liSide;
                const Vector3 lPoint = PointAtLine(lLineStart, lafDirection, lfParam);
                const f32 lafPoint[3] = { lPoint.mV.v[0], lPoint.mV.v[1], lPoint.mV.v[2] };
                bool lbInside = true;
                for (s32 liOtherAxis = 0; liOtherAxis < 3; ++liOtherAxis)
                {
                    if (liOtherAxis != liAxis)
                        lbInside = lbInside
                            && (lafPoint[liOtherAxis] >= lafMin[liOtherAxis])
                            && !(lafPoint[liOtherAxis] > lafMax[liOtherAxis]);
                }
                if (lbInside)
                {
                    laCandidates[liCandidate] = lPoint;
                    lbCandidateValid[liCandidate] = true;
                }
            }
        }

        lpaDest[0] = MakeZeroVector3();
        lpaDest[1] = MakeZeroVector3();
        lpaDestValid[0] = MakeTriangleBoxMask(false);
        lpaDestValid[1] = MakeTriangleBoxMask(false);
        s32 liStored = 0;
        for (s32 liCandidate = 0; liCandidate < 6 && liStored < 2; ++liCandidate)
        {
            if (!lbCandidateValid[liCandidate])
            {
                continue;
            }

            lpaDest[liStored] = laCandidates[liCandidate];
            lpaDestValid[liStored] = MakeTriangleBoxMask(true);
            ++liStored;
        }
    }

    // ========================================================================
    // CgsGeometric::ClipTriWithAABB @ 0x82840210.
    //
    // The stack slots and call sequence are taken from the X360 stream:
    //  - twelve box edges, in the captured edge-pair order below;
    //  - the three triangle edges V0->V1, V1->V2, V2->V0;
    //  - the resulting points occupy lpaDest[0..17].
    //
    // The corner W lane is x (vperm control 0x82CDA350), not zero. Plane
    // distances are the dot3 values consumed by ClipBoxEdgeAgainstTriangle.
    // ========================================================================
    void ClipTriWithAABB(Vector3* const                  lpaDest,
                         rw::math::vpu::MaskScalar* const lpaDestValid,
                         const AxisAlignedBox&            lAABox,
                         Vector3                          lTriangleV0,
                         Vector3                          lTriangleV1,
                         Vector3                          lTriangleV2)
    {
        BT::TriangleData lTriangleData;
        lTriangleData.Construct(lTriangleV0, lTriangleV1, lTriangleV2);

        const f32 lfMinX = lAABox.mMin.mV.v[0];
        const f32 lfMinY = lAABox.mMin.mV.v[1];
        const f32 lfMinZ = lAABox.mMin.mV.v[2];
        const f32 lfMaxX = lAABox.mMax.mV.v[0];
        const f32 lfMaxY = lAABox.mMax.mV.v[1];
        const f32 lfMaxZ = lAABox.mMax.mV.v[2];

        const Vector3 laCorners[8] = {
            MakeVector3W(lfMinX, lfMinY, lfMaxZ, lfMinX), // var_400, slot 0
            MakeVector3W(lfMaxX, lfMinY, lfMinZ, lfMaxX), // var_3D0, slot 1
            MakeVector3W(lfMinX, lfMaxY, lfMaxZ, lfMinX), // var_3A0, slot 2
            MakeVector3W(lfMaxX, lfMaxY, lfMinZ, lfMaxX), // var_370, slot 3
            MakeVector3W(lfMaxX, lfMinY, lfMaxZ, lfMaxX), // var_340, slot 4
            MakeVector3W(lfMinX, lfMaxY, lfMinZ, lfMinX), // var_310, slot 5
            MakeVector3W(lfMinX, lfMinY, lfMinZ, lfMinX), // var_2E0, slot 6
            MakeVector3W(lfMaxX, lfMaxY, lfMaxZ, lfMaxX)  // var_2B0, slot 7
        };

        BT::BoxVertexData laBoxVertices[8];
        for (s32 liCorner = 0; liCorner < 8; ++liCorner)
        {
            const Vector3& lrPosition = laCorners[liCorner];
            const f32 lfPlaneOffset = lrPosition.mV.v[0] * lTriangleData.mNormal.mV.v[0]
                                    + lrPosition.mV.v[1] * lTriangleData.mNormal.mV.v[1]
                                    + lrPosition.mV.v[2] * lTriangleData.mNormal.mV.v[2];

            laBoxVertices[liCorner].mPosition = lrPosition;
            laBoxVertices[liCorner].mPlaneOffset = MakeSplat(lfPlaneOffset);
            laBoxVertices[liCorner].mPlaneOffsetFromTriangle =
                MakeSplat(lTriangleData.mPlaneOffset.mV.v[0] - lfPlaneOffset);
        }

        // BoxVertexData stack slots from the X360 caller, in call order.
        static const s32 laBoxEdgePairs[12][2] = {
            { 6, 5 }, { 1, 3 }, { 0, 2 }, { 4, 7 },
            { 6, 0 }, { 1, 4 }, { 6, 1 }, { 0, 4 },
            { 5, 2 }, { 3, 7 }, { 5, 3 }, { 2, 7 }
        };

        for (s32 liEdge = 0; liEdge < 12; ++liEdge)
        {
            const BT::BoxVertexData& lrStart = laBoxVertices[laBoxEdgePairs[liEdge][0]];
            const BT::BoxVertexData& lrEnd = laBoxVertices[laBoxEdgePairs[liEdge][1]];
            lpaDestValid[liEdge] = ClipBoxEdgeAgainstTriangle(
                lTriangleData, lrStart, lrEnd, lpaDest[liEdge]);
        }

        ClipTriEdgeAgainstBox(lpaDest + 12, lpaDestValid + 12, lAABox,
                              lTriangleV0, lTriangleV1);
        ClipTriEdgeAgainstBox(lpaDest + 14, lpaDestValid + 14, lAABox,
                              lTriangleV1, lTriangleV2);
        ClipTriEdgeAgainstBox(lpaDest + 16, lpaDestValid + 16, lAABox,
                              lTriangleV2, lTriangleV0);
    }

    // ========================================================================
    // CgsGeometric::IntersectTriangleBox @ 0x828406D0.
    // ========================================================================
    rw::math::vpu::MaskScalar IntersectTriangleBox(
        const Box& lBox, const Triangle& lTriangle, VecFloat lPadding,
        rw::math::vpu::MaskScalar lbClampPenetration,
        Vector3* lpOutTriangleNormals, Vector3* lpOutContactNormals,
        Vector3* lpOutTrianglePoints, Vector3* lpOutContactPoints,
        s32* lpOutNumContacts, s32 liMaxContacts)
    {
        if (!(lPadding.mV.v[0] >= 0.0f))
        {
            ReportTriangleBoxAssert("lPadding >= 0.0f", 955);
        }

        const Matrix44Affine lBoxTransform = lBox.GetTransform();
        const Matrix44Affine lBoxInverseTransform =
            rw::math::vpu::InverseOfMatrixWithOrthonormal3x3(lBoxTransform);

        const Vector3 lBoxDimensions = lBox.GetDimensions();
        const VecFloat lBoxFatness = lBox.GetFatness();
        AxisAlignedBox lAABox;
        lAABox.mMin.mV.v[0] = -lBoxDimensions.mV.v[0];
        lAABox.mMin.mV.v[1] = -lBoxDimensions.mV.v[1];
        lAABox.mMin.mV.v[2] = -lBoxDimensions.mV.v[2];
        lAABox.mMin.mV.v[3] = -lBoxFatness.mV.v[0];
        lAABox.mMax.mV.v[0] =  lBoxDimensions.mV.v[0];
        lAABox.mMax.mV.v[1] =  lBoxDimensions.mV.v[1];
        lAABox.mMax.mV.v[2] =  lBoxDimensions.mV.v[2];
        lAABox.mMax.mV.v[3] =  lBoxFatness.mV.v[0];

        Vector3 lTriangleV0 = rw::math::vpu::TransformPoint(
            lBoxInverseTransform, lTriangle.Get(0));
        Vector3 lTriangleV1 = rw::math::vpu::TransformPoint(
            lBoxInverseTransform, lTriangle.Get(1));
        Vector3 lTriangleV2 = rw::math::vpu::TransformPoint(
            lBoxInverseTransform, lTriangle.Get(2));

        // ARTIST forms the same local normal from edge01 and edge12, then
        // applies vrsqrtefp with two Newton refinements. The console has no
        // zero-length guard here: retain its NaN/assert path for a degenerate
        // triangle instead of using the vendor Normalize zero-vector guard.
        const Vector3 lTriangleNormalRaw = rw::math::vpu::Cross(
            lTriangleV1 - lTriangleV0, lTriangleV2 - lTriangleV1);
        const f32 lfTriangleNormalLengthSq =
            rw::math::vpu::Dot(lTriangleNormalRaw, lTriangleNormalRaw);
        const f32 lfTriangleNormalInverseLength =
            1.0f / std::sqrt(lfTriangleNormalLengthSq);
        Vector3 lTriangleNormal;
        lTriangleNormal.mV.v[0] = lTriangleNormalRaw.mV.v[0] * lfTriangleNormalInverseLength;
        lTriangleNormal.mV.v[1] = lTriangleNormalRaw.mV.v[1] * lfTriangleNormalInverseLength;
        lTriangleNormal.mV.v[2] = lTriangleNormalRaw.mV.v[2] * lfTriangleNormalInverseLength;
        lTriangleNormal.mV.v[3] = lTriangleNormalRaw.mV.v[3] * lfTriangleNormalInverseLength;
        if (!IsValid3(lTriangleNormal))
        {
            ReportTriangleBoxAssert("rw::math::IsValid(lTriangleNormal)", 986);
        }

        Vector3 lPaddingOffset;
        lPaddingOffset.mV.v[0] = lTriangleNormal.mV.v[0] * lPadding.mV.v[0];
        lPaddingOffset.mV.v[1] = lTriangleNormal.mV.v[1] * lPadding.mV.v[1];
        lPaddingOffset.mV.v[2] = lTriangleNormal.mV.v[2] * lPadding.mV.v[2];
        lPaddingOffset.mV.v[3] = lTriangleNormal.mV.v[3] * lPadding.mV.v[3];
        lTriangleV0.mV.v[0] += lPaddingOffset.mV.v[0];
        lTriangleV0.mV.v[1] += lPaddingOffset.mV.v[1];
        lTriangleV0.mV.v[2] += lPaddingOffset.mV.v[2];
        lTriangleV0.mV.v[3] += lPaddingOffset.mV.v[3];
        lTriangleV1.mV.v[0] += lPaddingOffset.mV.v[0];
        lTriangleV1.mV.v[1] += lPaddingOffset.mV.v[1];
        lTriangleV1.mV.v[2] += lPaddingOffset.mV.v[2];
        lTriangleV1.mV.v[3] += lPaddingOffset.mV.v[3];
        lTriangleV2.mV.v[0] += lPaddingOffset.mV.v[0];
        lTriangleV2.mV.v[1] += lPaddingOffset.mV.v[1];
        lTriangleV2.mV.v[2] += lPaddingOffset.mV.v[2];
        lTriangleV2.mV.v[3] += lPaddingOffset.mV.v[3];

        Vector3 laClippedPoints[18];
        rw::math::vpu::MaskScalar laClippedPointsValid[18];
        ClipTriWithAABB(laClippedPoints, laClippedPointsValid, lAABox,
                        lTriangleV0, lTriangleV1, lTriangleV2);

        // The original 0x830390E0/0x830390D0 once-cache is referenced only by
        // this X360 function. Keep the same zero -> flt_82013F90 (0.001f)
        // transition as a function-local typed cache; no other ARTIST reader
        // or writer needs a shared global declaration.
        static bool lbPlaneOffsetLimitInitialized = false;
        static f32 lfPlaneOffsetLimit = 0.0f;
        if (!lbPlaneOffsetLimitInitialized)
        {
            lfPlaneOffsetLimit = 0.001f;
            lbPlaneOffsetLimitInitialized = true;
        }
        const bool lbClampOffset = lbClampPenetration.GetBool();

        const Vector3 lNormalWorldSpace =
            rw::math::vpu::TransformVector(lBoxTransform, lTriangleNormal);
        s32 liNumContacts = 0;

        for (s32 liPoint = 0; liPoint < 18; ++liPoint)
        {
            u32 luPointValid = 0;
            std::memcpy(&luPointValid, &laClippedPointsValid[liPoint].mV.u[0],
                        sizeof(luPointValid));
            if (luPointValid == 0u)
            {
                continue;
            }

            if (liNumContacts >= liMaxContacts)
            {
                // Keep the console's assert-and-continue contract. The message
                // stream's extra byte contains the original sink's terminator.
                CgsDev::Assert::BeginAssert();
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE + 1];
                CgsDev::StrStream lStrStream(
                    lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream.Reset();
                lStrStream << "Overrun contacts buffer - pass in a larger buffer\n";
                CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), klpTriangleBoxOriginalFile, 1004);
                CgsDev::Assert::EndAssert();
            }

            const Vector3 lClippedPoint = laClippedPoints[liPoint];
            Vector3 lTrianglePointLocal;
            lTrianglePointLocal.mV.v[0] = lClippedPoint.mV.v[0] - lPaddingOffset.mV.v[0];
            lTrianglePointLocal.mV.v[1] = lClippedPoint.mV.v[1] - lPaddingOffset.mV.v[1];
            lTrianglePointLocal.mV.v[2] = lClippedPoint.mV.v[2] - lPaddingOffset.mV.v[2];
            lTrianglePointLocal.mV.v[3] = lClippedPoint.mV.v[3] - lPaddingOffset.mV.v[3];

            Vector3 lNormalDirection;
            lNormalDirection.mV.v[0] = (lTriangleNormal.mV.v[0] >= 0.0f) ? -1.0f : 1.0f;
            lNormalDirection.mV.v[1] = (lTriangleNormal.mV.v[1] >= 0.0f) ? -1.0f : 1.0f;
            lNormalDirection.mV.v[2] = (lTriangleNormal.mV.v[2] >= 0.0f) ? -1.0f : 1.0f;
            lNormalDirection.mV.v[3] = (lTriangleNormal.mV.v[3] >= 0.0f) ? -1.0f : 1.0f;

            Vector3 lBoxExtent;
            lBoxExtent.mV.v[0] = lBoxDimensions.mV.v[0] * lNormalDirection.mV.v[0];
            lBoxExtent.mV.v[1] = lBoxDimensions.mV.v[1] * lNormalDirection.mV.v[1];
            lBoxExtent.mV.v[2] = lBoxDimensions.mV.v[2] * lNormalDirection.mV.v[2];
            lBoxExtent.mV.v[3] = lBoxFatness.mV.v[0] * lNormalDirection.mV.v[3];

            const f32 lfPlaneOffset =
                rw::math::vpu::Dot(lTrianglePointLocal, lTriangleNormal)
              - rw::math::vpu::Dot(lBoxExtent, lTriangleNormal);
            const f32 lfSelectedOffset = lbClampOffset
                ? ((lfPlaneOffset < lfPlaneOffsetLimit) ? lfPlaneOffset : lfPlaneOffsetLimit)
                : lfPlaneOffset;

            Vector3 lBoxPointLocal;
            lBoxPointLocal.mV.v[0] = lTrianglePointLocal.mV.v[0] - lTriangleNormal.mV.v[0] * lfSelectedOffset;
            lBoxPointLocal.mV.v[1] = lTrianglePointLocal.mV.v[1] - lTriangleNormal.mV.v[1] * lfSelectedOffset;
            lBoxPointLocal.mV.v[2] = lTrianglePointLocal.mV.v[2] - lTriangleNormal.mV.v[2] * lfSelectedOffset;
            lBoxPointLocal.mV.v[3] = lTrianglePointLocal.mV.v[3] - lTriangleNormal.mV.v[3] * lfSelectedOffset;

            const Vector3 lBoxPointWorld =
                rw::math::vpu::TransformPoint(lBoxTransform, lBoxPointLocal);
            const Vector3 lTrianglePointWorld =
                rw::math::vpu::TransformPoint(lBoxTransform, lTrianglePointLocal);

            if (!IsValid3(lBoxPointWorld))
            {
                ReportTriangleBoxAssert("Box point is invalid", 1031);
            }
            if (!IsValid3(lTrianglePointWorld))
            {
                ReportTriangleBoxAssert("Triangle point is invalid", 1032);
            }
            if (!IsValid3(lNormalWorldSpace))
            {
                ReportTriangleBoxAssert("Box normal is invalid", 1033);
            }
            if (!IsValid3(lNormalWorldSpace))
            {
                ReportTriangleBoxAssert("Triangle normal is invalid", 1034);
            }

            lpOutTriangleNormals[liNumContacts] = lNormalWorldSpace;
            lpOutContactNormals[liNumContacts] = lNormalWorldSpace;
            lpOutTrianglePoints[liNumContacts] = lTrianglePointWorld;
            lpOutContactPoints[liNumContacts] = lBoxPointWorld;
            ++liNumContacts;
        }

        *lpOutNumContacts = liNumContacts;
        return MakeTriangleBoxMask(liNumContacts > 0);
    }
}
