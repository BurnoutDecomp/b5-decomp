#include "SharedClasses/AI/AISectionsResourceType.h"   // BrnAI::AISection (mx8Flags @+23, mpaCorners @+8, KI_AI_SECTION_EDGES)

#include "GameSource/World/AI/BrnAIPortal.h"                 // BrnAI::Portal (20-byte stride; GetPortal return)
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT + Begin/Fire/End + KI_MESSAGEBUFFERSIZE
#include "GameShared/GameClasses/Development/CgsStrStream.h"  // CgsDev::StrStream (GetPortal miss message)
#include "GameSource/World/AI/BrnAIBoundaryLine.h"           // BrnAI::BoundaryLine (mpaNoGoLines target)
#include <cstdint>                                           // uintptr_t (relocation arithmetic)

// BrnAI::AISection member bodies.
//
// BrnAI::AISection::IsUnsuitableForResetOnTrackLink @0x8276AC18
//   (BrnAI::ResetOnTrackManager::UpdateResetOnTrackSectionUsingCurrentSection):
//   reads mx8Flags @+23 (lbz r11, 0x17(r3)) and returns true if bit 0x01 is set
//   (clrlwi r10,r11,31 -> &1) OR bit 0x40 is set (rlwinm r11,r11,0,25,25 -> &0x40),
//   otherwise false. The result is masked to a byte before return (clrlwi r3,r11,24).
//
// BrnAI::AISection::GetPortal     @0x8230F5D0
// BrnAI::AISection::IsInside      @0x82677058
// BrnAI::AISection::PassesThrough @0x826772A8
//
// IsInside / PassesThrough operate purely on the section's KI_AI_SECTION_EDGES-corner
// convex polygon (mpaCorners @+8, 4 edges) in a single 2D plane. The X360 bodies are
// VMX128 but every vector op is a splat of lane 0 -- i.e. scalar f32 maths done in
// vector registers -- so they lower to the plain 2D predicates below. The polygon's two
// coordinates are the Vector2 .x / .y lanes; the section uses the (x, z) ground plane
// (corroborated by AISectionsData.cpp's grid, which buckets sections in world (X, Z)),
// with the corners storing the two relevant axes packed as .x / .y.

namespace BrnAI
{
    // PassesThrough parallel-edge cull threshold. In the X360 asm this is a single
    // rodata float, splatted from &unk_820A36A0, compared against abs(cross) via
    // `vcmpgtfp`. Its literal value is NOT present in the supplied IDA exports (the
    // rodata word at 0x820A36A0 is referenced only by this function and was not dumped),
    // so it is declared here as a FLAGGED PLACEHOLDER rather than fabricated -- see
    // open_questions. The placeholder uses the smallest positive normal f32, the
    // conventional "treat as parallel only when the cross product is effectively zero"
    // epsilon; the real constant must be read from rodata @0x820A36A0 to be byte-faithful.
    static const f32 KF_INTERSECTION_EPSILON = 1.17549435e-38f;   // PLACEHOLDER (rodata @0x820A36A0 unread)

    // GetMiddle averages the four corners: 1 / KI_AI_SECTION_EDGES.
    static const f32 KF_CORNER_MEAN_SCALE = 0.25f;

    // ------------------------------------------------------------------------------
    // AISection::FixUp @0x8267D8C8 / AISection::FixDown @0x8267D978 -- load-time
    // pointer relocation of the section's three slots.
    //
    // The asm shape (FixUp; FixDown is the mirror with -= and the pointer write AFTER the
    // inner loop):
    //
    //     if (mpaPortals) {
    //         mpaPortals += base;
    //         for (j = 0; j < mu8NumPortals; ++j) mpaPortals[j].FixUp(base);   // inlined
    //     }
    //     if (mpaNoGoLines) mpaNoGoLines += base;
    //     mpaCorners += base;                       // UNCONDITIONAL -- no null guard
    //
    // The asymmetry is the console's, not a transcription slip: only mpaCorners is rebased
    // without a guard. Every AI section in the shipped AI.DAT has a corner block, so the
    // guardless slot is safe; keeping the difference preserves parity with the console
    // (and with FixDown, which is guardless on the same slot).
    // ------------------------------------------------------------------------------
    void AISection::FixUp(const void* lpBaseData)
    {
        const uintptr_t luBase = reinterpret_cast<uintptr_t>(lpBaseData);

        if (mpaPortals != nullptr)
        {
            mpaPortals = reinterpret_cast<Portal*>(
                reinterpret_cast<uintptr_t>(mpaPortals) + luBase);

            for (u32 luPortal = 0; luPortal < mu8NumPortals; ++luPortal)
            {
                mpaPortals[luPortal].FixUp(lpBaseData);
            }
        }

        if (mpaNoGoLines != nullptr)
        {
            mpaNoGoLines = reinterpret_cast<BoundaryLine*>(
                reinterpret_cast<uintptr_t>(mpaNoGoLines) + luBase);
        }

        mpaCorners = reinterpret_cast<Vector2*>(
            reinterpret_cast<uintptr_t>(mpaCorners) + luBase);
    }

    void AISection::FixDown(const void* lpBaseData)
    {
        const uintptr_t luBase = reinterpret_cast<uintptr_t>(lpBaseData);

        if (mpaPortals != nullptr)
        {
            for (u32 luPortal = 0; luPortal < mu8NumPortals; ++luPortal)
            {
                mpaPortals[luPortal].FixDown(lpBaseData);
            }

            mpaPortals = reinterpret_cast<Portal*>(
                reinterpret_cast<uintptr_t>(mpaPortals) - luBase);
        }

        if (mpaNoGoLines != nullptr)
        {
            mpaNoGoLines = reinterpret_cast<BoundaryLine*>(
                reinterpret_cast<uintptr_t>(mpaNoGoLines) - luBase);
        }

        mpaCorners = reinterpret_cast<Vector2*>(
            reinterpret_cast<uintptr_t>(mpaCorners) - luBase);
    }

    // 0x826771D0 -- the section's representative point: the mean of the four corner
    // positions in the ground plane, lifted to the height of portal 0.
    //
    // asm: the four corner X lanes are read at mpaCorners +0x00/+0x08/+0x10/+0x18 and the
    // four Z lanes at +0x04/+0x0C/+0x14/+0x1C -- i.e. an 8-BYTE corner stride, which is
    // what pins BrnAI::Vector2 as the packed 2-float type rather than the 16-byte SIMD
    // alias. The two sums are multiplied by 0.25 (the rodata quarter splat) and the Y lane
    // is taken verbatim from GetPortal(0)->GetPositionY(), so a section with no portals
    // fires GetPortal's own bounds assert -- reproduced by calling through GetPortal.
    Vector3 AISection::GetMiddle() const
    {
        const f32 lfSumX = mpaCorners[0].x + mpaCorners[1].x
                         + mpaCorners[2].x + mpaCorners[3].x;
        const f32 lfSumZ = mpaCorners[0].y + mpaCorners[1].y
                         + mpaCorners[2].y + mpaCorners[3].y;

        Vector3 lMiddle;
        lMiddle.x = lfSumX * KF_CORNER_MEAN_SCALE;
        lMiddle.y = GetPortal(0)->GetPositionY();
        lMiddle.z = lfSumZ * KF_CORNER_MEAN_SCALE;
        lMiddle.w = 0.0f;
        return lMiddle;
    }

    bool AISection::IsUnsuitableForResetOnTrackLink() const
    {
        if ((mx8Flags & 0x01) != 0)
        {
            return true;
        }
        if ((mx8Flags & 0x40) != 0)
        {
            return true;
        }
        return false;
    }

    // 0x8230F5D0 -- return the luPortalIndex'th portal of this section.
    //
    // asm: lbz r11, 0x14(r27) reads mu8NumPortals (@+20); if luPortalIndex >= that it
    // fires the dev-assert, building the diagnostic with the StrStream operator<<(int)
    // overloads ("Trying to access portal " << index << " when only " << count <<
    // " available."). The hit path computes 20*index + *this == &mpaPortals[index]
    // (Portal is a 20-byte struct: slwi r11,r28,2 + add r28 + slwi 2 == *20). The
    // X360-baked d:\p4 path/line (AISectionsData.h:880, li r5,0x370) is kept on the
    // FireAssert call, matching the committed AISectionsResourceType::GetMiddle precedent.
    const Portal* AISection::GetPortal(u8 luPortalIndex) const
    {
        if (luPortalIndex >= mu8NumPortals)
        {
            // X360 streams the diagnostic into the global CgsDev::Assert message buffer;
            // matching the committed AISectionsResourceType::GetMiddle precedent, build it
            // into a local buffer and fire the assert.
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStream << "Trying to access portal " << static_cast<s32>(luPortalIndex)
                    << " when only " << static_cast<s32>(mu8NumPortals) << " available.";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessageBuffer,
                                       "..\\..\\..\\SharedClasses\\AI/AISectionsData.h", 880);
            CgsDev::Assert::EndAssert();
        }

        return &mpaPortals[luPortalIndex];
    }

    // 0x82677058 -- convex point-in-polygon test over the section's KI_AI_SECTION_EDGES
    // corners (mpaCorners @+8). The test point is (lfX, lfY) (the two scalar float args;
    // the second is the world-Z lane). The loop walks every edge prev->cur, starting with
    // prev == corners[3] (asm seeds v127/v126 from mpaCorners+0x18 == corner[3], so the
    // first edge is corner[3]->corner[0], the wrap-around edge), and forms the 2D cross
    // product of the edge vector with the (point - prev) vector:
    //
    //   cross = (cur.x - prev.x) * (P.y - prev.y) - (cur.y - prev.y) * (P.x - prev.x)
    //
    // asm: v12 = (cur.x-prev.x)*(P.y-prev.y); v13 = (cur.y-prev.y)*(P.x-prev.x);
    //      vcmpgtfp v12 > v13 -> break (return false) the moment any edge's cross is > 0.
    // So the point is inside iff cross <= 0 for ALL four edges (consistent winding;
    // strictly-positive cross means the point is on the outside half-plane of an edge).
    // The per-iteration "liCornerIndex >= 0 && liCornerIndex < KI_AI_SECTION_EDGES" guard
    // is the inlined GetCorner bounds-assert (fired twice per edge in the asm).
    bool AISection::IsInside(f32 lfX, f32 lfY) const
    {
        f32 lfPrevX = mpaCorners[KI_AI_SECTION_EDGES - 1].x;
        f32 lfPrevY = mpaCorners[KI_AI_SECTION_EDGES - 1].y;

        for (s32 liCornerIndex = 0; liCornerIndex < KI_AI_SECTION_EDGES; ++liCornerIndex)
        {
            CGS_ASSERT(liCornerIndex >= 0 && liCornerIndex < KI_AI_SECTION_EDGES,
                       "liCornerIndex >= 0 && liCornerIndex < KI_AI_SECTION_EDGES");
            const f32 lfCurX = mpaCorners[liCornerIndex].x;

            CGS_ASSERT(liCornerIndex >= 0 && liCornerIndex < KI_AI_SECTION_EDGES,
                       "liCornerIndex >= 0 && liCornerIndex < KI_AI_SECTION_EDGES");
            const f32 lfCurY = mpaCorners[liCornerIndex].y;

            const f32 lfEdgeX  = lfCurX - lfPrevX;
            const f32 lfEdgeY  = lfCurY - lfPrevY;
            const f32 lfRelX   = lfX - lfPrevX;
            const f32 lfRelY   = lfY - lfPrevY;

            // cross = edge x (point - prev); > 0 => point on the outside half-plane.
            if ((lfEdgeX * lfRelY) > (lfEdgeY * lfRelX))
            {
                return false;
            }

            lfPrevX = lfCurX;
            lfPrevY = lfCurY;
        }

        return true;
    }

    // 0x826772A8 -- does the segment lStart->lEnd cross into this section?
    //
    // First: if either endpoint is already inside the section (IsInside on each), the
    // segment trivially passes through, so return true immediately (asm calls IsInside
    // twice up front and returns 1 on either hit).
    //
    // Otherwise walk the four polygon edges (prev->cur, seeded prev == corners[3]) and do a
    // parametric segment-vs-edge intersection, the inlined form of
    // BrnAI::Calc2DIntersectionEquationData (BrnAIUtils.cpp @0x82771800). With
    //   dSx = lEnd.x - lStart.x   dSy = lEnd.y - lStart.y   (the query segment)
    //   dEx = cur.x  - prev.x     dEy = cur.y  - prev.y     (the polygon edge)
    //   cross = dEx*dSy - dEy*dSx                            (denominator)
    // the segments are parallel when |cross| <= KF_INTERSECTION_EPSILON (skip that edge).
    // Otherwise, with rPx = prev.x - lStart.x, rPy = prev.y - lStart.y:
    //   t = (rPy*dSx - rPx*dSy) / cross   (parameter along the query segment)
    //   u = (rPy*dEx - rPx*dEy) / cross   (parameter along the polygon edge)
    // The asm requires t in [0,1] AND u >= 0 (u is only lower-bounded -- a ray test on the
    // edge side: the two t-checks are 0>t and t>1, the u-check is only 0>u). The first edge
    // that satisfies this means the segment crosses the section boundary -> return true. If
    // no edge intersects, return false.
    //
    // The X360 computes 1/cross with vrefp + one Newton-Raphson step (vnmsubfp/vmaddfp);
    // that is the hardware reciprocal-estimate refinement and lowers to an exact divide.
    bool AISection::PassesThrough(Vector2 lStart, Vector2 lEnd) const
    {
        if (IsInside(lStart.x, lStart.y))
        {
            return true;
        }
        if (IsInside(lEnd.x, lEnd.y))
        {
            return true;
        }

        const f32 lfDSx = lEnd.x - lStart.x;
        const f32 lfDSy = lEnd.y - lStart.y;

        f32 lfPrevX = mpaCorners[KI_AI_SECTION_EDGES - 1].x;
        f32 lfPrevY = mpaCorners[KI_AI_SECTION_EDGES - 1].y;

        for (s32 liCornerIndex = 0; liCornerIndex < KI_AI_SECTION_EDGES; ++liCornerIndex)
        {
            CGS_ASSERT(liCornerIndex >= 0 && liCornerIndex < KI_AI_SECTION_EDGES,
                       "liCornerIndex >= 0 && liCornerIndex < KI_AI_SECTION_EDGES");
            const f32 lfCurX = mpaCorners[liCornerIndex].x;

            CGS_ASSERT(liCornerIndex >= 0 && liCornerIndex < KI_AI_SECTION_EDGES,
                       "liCornerIndex >= 0 && liCornerIndex < KI_AI_SECTION_EDGES");
            const f32 lfCurY = mpaCorners[liCornerIndex].y;

            const f32 lfEdgeX = lfCurX - lfPrevX;
            const f32 lfEdgeY = lfCurY - lfPrevY;

            // Denominator: edge x segment. Parallel (|cross| <= epsilon) edges are skipped.
            const f32 lfCross    = (lfEdgeX * lfDSy) - (lfEdgeY * lfDSx);
            const f32 lfAbsCross = (lfCross < 0.0f) ? -lfCross : lfCross;

            if (lfAbsCross > KF_INTERSECTION_EPSILON)
            {
                const f32 lfInv = 1.0f / lfCross;

                const f32 lfRPx = lfPrevX - lStart.x;
                const f32 lfRPy = lfPrevY - lStart.y;

                // Parameter along the query segment; must lie within the segment [0,1].
                const f32 lfT = ((lfRPy * lfDSx) - (lfRPx * lfDSy)) * lfInv;
                if (lfT >= 0.0f && lfT <= 1.0f)
                {
                    // Parameter along the polygon edge; only the >= 0 side is required.
                    const f32 lfU = ((lfRPy * lfEdgeX) - (lfRPx * lfEdgeY)) * lfInv;
                    if (lfU >= 0.0f)
                    {
                        return true;
                    }
                }
            }

            lfPrevX = lfCurX;
            lfPrevY = lfCurY;
        }

        return false;
    }
}
