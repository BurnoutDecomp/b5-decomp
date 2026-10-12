#include "GameShared/GameClasses/Geometric/Intersection/CgsTriangleBox.h"

#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsBox.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle.h"
#include "rw/math/vpu/matrix44affine_operation.h"

#include <array>
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

// ============================================================================
// CgsGeometric::ClipTriEdgeAgainstBox @ 0x8283FF90.
//
// The X360 body computes the six segment/box-face candidates with vrefp plus
// three reciprocal refinements, checks each candidate against the segment and
// the inclusive box slabs, then places the first two distinct valid boundary
// points in its two Vector3 slots. Candidate order is max-X/Y/Z followed by
// min-X/Y/Z; a segment wholly inside the box produces no points. This scalar
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
        const f32 lafStart[3] = { lLineStart.x, lLineStart.y, lLineStart.z };
        const f32 lafEnd[3]   = { lLineEnd.x, lLineEnd.y, lLineEnd.z };
        const f32 lafMin[3]   = { lBox.mMin.x, lBox.mMin.y, lBox.mMin.z };
        const f32 lafMax[3]   = { lBox.mMax.x, lBox.mMax.y, lBox.mMax.z };

        // vrefp followed by the same three Newton-Raphson rounds used by the
        // other recovered X360 reciprocal paths. In particular, a zero input
        // refines to NaN; the parallel-axis branch below uses the raw slab test.
        const auto Reciprocal3 = [](f32 lfValue)
        {
            f32 lfReciprocal = 1.0f / lfValue;
            for (s32 liStep = 0; liStep < 3; ++liStep)
            {
                const f32 lfError = std::fma(-lfValue, lfReciprocal, 1.0f);
                lfReciprocal      = std::fma(lfReciprocal, lfError, lfReciprocal);
            }
            return lfReciprocal;
        };

        const auto MakeMask = [](bool lbValid)
        {
            rw::math::vpu::MaskScalar lMask;
            const u32 luLane = lbValid ? 0xFFFFFFFFu : 0u;
            std::memcpy(&lMask.x, &luLane, sizeof(luLane));
            std::memcpy(&lMask.y, &luLane, sizeof(luLane));
            std::memcpy(&lMask.z, &luLane, sizeof(luLane));
            std::memcpy(&lMask.w, &luLane, sizeof(luLane));
            return lMask;
        };

        const auto MakeZero = []()
        {
            Vector3 lZero;
            lZero.x = 0.0f;
            lZero.y = 0.0f;
            lZero.z = 0.0f;
            lZero.w = 0.0f;
            return lZero;
        };
        const f32 lafDirection[4] = {
            lLineEnd.x - lLineStart.x,
            lLineEnd.y - lLineStart.y,
            lLineEnd.z - lLineStart.z,
            lLineEnd.w - lLineStart.w
        };

        const auto PointAt = [&](f32 lfParam)
        {
            Vector3 lPoint;
            lPoint.x = std::fma(lafDirection[0], lfParam, lLineStart.x);
            lPoint.y = std::fma(lafDirection[1], lfParam, lLineStart.y);
            lPoint.z = std::fma(lafDirection[2], lfParam, lLineStart.z);
            lPoint.w = std::fma(lafDirection[3], lfParam, lLineStart.w);
            return lPoint;
        };

        const f32 lafReciprocal[3] = {
            Reciprocal3(lafDirection[0]),
            Reciprocal3(lafDirection[1]),
            Reciprocal3(lafDirection[2])
        };
        Vector3 laCandidates[6];
        bool lbCandidateValid[6] = { false, false, false, false, false, false };

        // The raw vector stream builds the three maximum-face candidates
        // before the three minimum-face candidates. For a zero direction lane
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

                const s32 liCandidate = (liSide == 0) ? liAxis : liAxis + 3;
                const Vector3 lPoint = PointAt(lfParam);
                const bool lbInside =
                    (lPoint.x >= lBox.mMin.x) && !(lPoint.x > lBox.mMax.x)
                 && (lPoint.y >= lBox.mMin.y) && !(lPoint.y > lBox.mMax.y)
                 && (lPoint.z >= lBox.mMin.z) && !(lPoint.z > lBox.mMax.z);
                if (lbInside)
                {
                    laCandidates[liCandidate] = lPoint;
                    lbCandidateValid[liCandidate] = true;
                }
            }
        }

        lpaDest[0] = MakeZero();
        lpaDest[1] = MakeZero();
        lpaDestValid[0] = MakeMask(false);
        lpaDestValid[1] = MakeMask(false);
        s32 liStored = 0;
        for (s32 liCandidate = 0; liCandidate < 6 && liStored < 2; ++liCandidate)
        {
            if (!lbCandidateValid[liCandidate])
            {
                continue;
            }

            const Vector3& lrCandidate = laCandidates[liCandidate];
            bool lbDuplicate = false;
            for (s32 liPrevious = 0; liPrevious < liStored; ++liPrevious)
            {
                const Vector3& lrPrevious = lpaDest[liPrevious];
                if (lrCandidate.x == lrPrevious.x
                    && lrCandidate.y == lrPrevious.y
                    && lrCandidate.z == lrPrevious.z)
                {
                    lbDuplicate = true;
                    break;
                }
            }
            if (!lbDuplicate)
            {
                lpaDest[liStored] = lrCandidate;
                lpaDestValid[liStored] = MakeMask(true);
                ++liStored;
            }
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

        const f32 lfMinX = lAABox.mMin.x;
        const f32 lfMinY = lAABox.mMin.y;
        const f32 lfMinZ = lAABox.mMin.z;
        const f32 lfMaxX = lAABox.mMax.x;
        const f32 lfMaxY = lAABox.mMax.y;
        const f32 lfMaxZ = lAABox.mMax.z;

        const std::array<Vector3, 8> laCorners = {{
            { lfMinX, lfMinY, lfMaxZ, lfMinX }, // var_400, slot 0
            { lfMaxX, lfMinY, lfMinZ, lfMaxX }, // var_3D0, slot 1
            { lfMinX, lfMaxY, lfMaxZ, lfMinX }, // var_3A0, slot 2
            { lfMaxX, lfMaxY, lfMinZ, lfMaxX }, // var_370, slot 3
            { lfMaxX, lfMinY, lfMaxZ, lfMaxX }, // var_340, slot 4
            { lfMinX, lfMaxY, lfMinZ, lfMinX }, // var_310, slot 5
            { lfMinX, lfMinY, lfMinZ, lfMinX }, // var_2E0, slot 6
            { lfMaxX, lfMaxY, lfMaxZ, lfMaxX }  // var_2B0, slot 7
        }};

        const auto MakeSplat = [](f32 lfValue)
        {
            VecFloat lValue;
            lValue.x = lfValue;
            lValue.y = lfValue;
            lValue.z = lfValue;
            lValue.w = lfValue;
            return lValue;
        };

        std::array<BT::BoxVertexData, 8> laBoxVertices;
        for (s32 liCorner = 0; liCorner < static_cast<s32>(laCorners.size()); ++liCorner)
        {
            const Vector3& lrPosition = laCorners[static_cast<std::size_t>(liCorner)];
            const f32 lfPlaneOffset = lrPosition.x * lTriangleData.mNormal.x
                                    + lrPosition.y * lTriangleData.mNormal.y
                                    + lrPosition.z * lTriangleData.mNormal.z;

            laBoxVertices[static_cast<std::size_t>(liCorner)].mPosition = lrPosition;
            laBoxVertices[static_cast<std::size_t>(liCorner)].mPlaneOffset = MakeSplat(lfPlaneOffset);
            laBoxVertices[static_cast<std::size_t>(liCorner)].mPlaneOffsetFromTriangle =
                MakeSplat(lfPlaneOffset - lTriangleData.mPlaneOffset.x);
        }

        // BoxVertexData stack slots from the X360 caller, in call order.
        static const s32 laBoxEdgePairs[12][2] = {
            { 6, 5 }, { 1, 3 }, { 0, 2 }, { 4, 7 },
            { 6, 0 }, { 1, 4 }, { 6, 1 }, { 0, 4 },
            { 5, 2 }, { 3, 7 }, { 5, 3 }, { 2, 7 }
        };

        for (s32 liEdge = 0; liEdge < 12; ++liEdge)
        {
            const BT::BoxVertexData& lrStart =
                laBoxVertices[static_cast<std::size_t>(laBoxEdgePairs[liEdge][0])];
            const BT::BoxVertexData& lrEnd =
                laBoxVertices[static_cast<std::size_t>(laBoxEdgePairs[liEdge][1])];
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
        static const char* const klpOriginalFile =
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\geometric\\Intersection/CgsTriangleBox.cpp";
        const auto ReportAssert = [&](const char* lpcExpression, s32 liLine)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lpcExpression, klpOriginalFile, liLine);
            CgsDev::Assert::EndAssert();
        };
        const auto IsValid3 = [](const Vector3& lrVector)
        {
            // X360's vcmpeqfp x,x test rejects NaNs but permits infinities.
            return (lrVector.x == lrVector.x)
                && (lrVector.y == lrVector.y)
                && (lrVector.z == lrVector.z);
        };
        const auto MakeMask = [](bool lbValue)
        {
            rw::math::vpu::MaskScalar lMask;
            const u32 luLane = lbValue ? 0xFFFFFFFFu : 0u;
            std::memcpy(&lMask.x, &luLane, sizeof(luLane));
            std::memcpy(&lMask.y, &luLane, sizeof(luLane));
            std::memcpy(&lMask.z, &luLane, sizeof(luLane));
            std::memcpy(&lMask.w, &luLane, sizeof(luLane));
            return lMask;
        };

        if (!(lPadding.x >= 0.0f))
        {
            ReportAssert("lPadding >= 0.0f", 955);
        }

        const Matrix44Affine lBoxTransform = lBox.GetTransform();
        const Matrix44Affine lBoxInverseTransform =
            rw::math::vpu::InverseOfMatrixWithOrthonormal3x3(lBoxTransform);

        const Vector3 lBoxDimensions = lBox.GetDimensions();
        const VecFloat lBoxFatness = lBox.GetFatness();
        AxisAlignedBox lAABox;
        lAABox.mMin.x = -lBoxDimensions.x;
        lAABox.mMin.y = -lBoxDimensions.y;
        lAABox.mMin.z = -lBoxDimensions.z;
        lAABox.mMin.w = -lBoxFatness.x;
        lAABox.mMax.x =  lBoxDimensions.x;
        lAABox.mMax.y =  lBoxDimensions.y;
        lAABox.mMax.z =  lBoxDimensions.z;
        lAABox.mMax.w =  lBoxFatness.x;

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
        lTriangleNormal.x = lTriangleNormalRaw.x * lfTriangleNormalInverseLength;
        lTriangleNormal.y = lTriangleNormalRaw.y * lfTriangleNormalInverseLength;
        lTriangleNormal.z = lTriangleNormalRaw.z * lfTriangleNormalInverseLength;
        lTriangleNormal.w = lTriangleNormalRaw.w * lfTriangleNormalInverseLength;
        if (!IsValid3(lTriangleNormal))
        {
            ReportAssert("rw::math::IsValid(lTriangleNormal)", 986);
        }

        Vector3 lPaddingOffset;
        lPaddingOffset.x = lTriangleNormal.x * lPadding.x;
        lPaddingOffset.y = lTriangleNormal.y * lPadding.y;
        lPaddingOffset.z = lTriangleNormal.z * lPadding.z;
        lPaddingOffset.w = lTriangleNormal.w * lPadding.w;
        lTriangleV0.x += lPaddingOffset.x;
        lTriangleV0.y += lPaddingOffset.y;
        lTriangleV0.z += lPaddingOffset.z;
        lTriangleV0.w += lPaddingOffset.w;
        lTriangleV1.x += lPaddingOffset.x;
        lTriangleV1.y += lPaddingOffset.y;
        lTriangleV1.z += lPaddingOffset.z;
        lTriangleV1.w += lPaddingOffset.w;
        lTriangleV2.x += lPaddingOffset.x;
        lTriangleV2.y += lPaddingOffset.y;
        lTriangleV2.z += lPaddingOffset.z;
        lTriangleV2.w += lPaddingOffset.w;

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
            std::memcpy(&luPointValid, &laClippedPointsValid[liPoint].x,
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
                CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), klpOriginalFile, 1004);
                CgsDev::Assert::EndAssert();
            }

            const Vector3 lClippedPoint = laClippedPoints[liPoint];
            Vector3 lTrianglePointLocal;
            lTrianglePointLocal.x = lClippedPoint.x - lPaddingOffset.x;
            lTrianglePointLocal.y = lClippedPoint.y - lPaddingOffset.y;
            lTrianglePointLocal.z = lClippedPoint.z - lPaddingOffset.z;
            lTrianglePointLocal.w = lClippedPoint.w - lPaddingOffset.w;

            Vector3 lNormalDirection;
            lNormalDirection.x = (lTriangleNormal.x >= 0.0f) ? -1.0f : 1.0f;
            lNormalDirection.y = (lTriangleNormal.y >= 0.0f) ? -1.0f : 1.0f;
            lNormalDirection.z = (lTriangleNormal.z >= 0.0f) ? -1.0f : 1.0f;
            lNormalDirection.w = (lTriangleNormal.w >= 0.0f) ? -1.0f : 1.0f;

            Vector3 lBoxExtent;
            lBoxExtent.x = lBoxDimensions.x * lNormalDirection.x;
            lBoxExtent.y = lBoxDimensions.y * lNormalDirection.y;
            lBoxExtent.z = lBoxDimensions.z * lNormalDirection.z;
            lBoxExtent.w = lBoxFatness.x * lNormalDirection.w;

            const f32 lfPlaneOffset =
                rw::math::vpu::Dot(lTrianglePointLocal, lTriangleNormal)
              - rw::math::vpu::Dot(lBoxExtent, lTriangleNormal);
            const f32 lfSelectedOffset = lbClampOffset
                ? ((lfPlaneOffset < lfPlaneOffsetLimit) ? lfPlaneOffset : lfPlaneOffsetLimit)
                : lfPlaneOffset;

            Vector3 lBoxPointLocal;
            lBoxPointLocal.x = lTrianglePointLocal.x - lTriangleNormal.x * lfSelectedOffset;
            lBoxPointLocal.y = lTrianglePointLocal.y - lTriangleNormal.y * lfSelectedOffset;
            lBoxPointLocal.z = lTrianglePointLocal.z - lTriangleNormal.z * lfSelectedOffset;
            lBoxPointLocal.w = lTrianglePointLocal.w - lTriangleNormal.w * lfSelectedOffset;

            const Vector3 lBoxPointWorld =
                rw::math::vpu::TransformPoint(lBoxTransform, lBoxPointLocal);
            const Vector3 lTrianglePointWorld =
                rw::math::vpu::TransformPoint(lBoxTransform, lTrianglePointLocal);

            if (!IsValid3(lBoxPointWorld))
            {
                ReportAssert("Box point is invalid", 1031);
            }
            if (!IsValid3(lTrianglePointWorld))
            {
                ReportAssert("Triangle point is invalid", 1032);
            }
            if (!IsValid3(lNormalWorldSpace))
            {
                ReportAssert("Box normal is invalid", 1033);
            }
            if (!IsValid3(lNormalWorldSpace))
            {
                ReportAssert("Triangle normal is invalid", 1034);
            }

            lpOutTriangleNormals[liNumContacts] = lNormalWorldSpace;
            lpOutContactNormals[liNumContacts] = lNormalWorldSpace;
            lpOutTrianglePoints[liNumContacts] = lTrianglePointWorld;
            lpOutContactPoints[liNumContacts] = lBoxPointWorld;
            ++liNumContacts;
        }

        *lpOutNumContacts = liNumContacts;
        return MakeMask(liNumContacts > 0);
    }
}
