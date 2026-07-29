#pragma once

// =============================================================================
// BrnTrafficSection.h  (OWNING HEADER for the BrnTraffic traffic-graph value types)
//
// DWARF home (references/DecFIGS/dwarfdump/SharedClasses/Traffic/BrnTrafficSection.h)
// of the BrnTraffic traffic-graph value types. This slice owns:
//
//   BrnTraffic::LaneRung   (struct @ BrnTrafficSection.h:50)  -- GetRightVector @ 0x821F4A88
//   BrnTraffic::Section    (struct @ BrnTrafficSection.h:106) -- the section accessor family:
//       GetNumSegments                  @ 0x821F4B78
//       CalcPositionAtParameter         @ 0x821F4BD8
//       GetGlobalRungForSegment         @ 0x821F5068
//       CalcSignedDistanceAlongSection  @ 0x82705BC0
//
// The X360 bakes this file's path into every assert in the accessor family
// ("...\\SharedClasses\\traffic\\BrnTrafficSection.h", the original line numbers
// shown per-method), confirming the home. Sibling value types (Neighbour / Side /
// SectionSpan / SectionFlow) and the not-yet-bodied Section methods are still owned
// elsewhere; only the members LaneRung and Section need for this batch are defined.
// GROW this header with those siblings/methods when they land -- never redefine
// LaneRung or Section.
//
// Section LAYOUT (DWARF-authoritative, BrnTrafficSection.h:106..130): the exact
// member list below (names/order verified against the DWARF dump). The X360 sizes
// the record to 48 bytes (proven by BrnTrafficHull::GetSection's index*48 stride),
// so a 4-byte tail pad follows mfLength (natural member sum reaches +44). The
// accessor asm reads *(this+0)=muRungOffset (lwz 0) and *(this+4)=muNumRungs (lbz 4),
// exactly the +0 / +4 offsets this layout produces.
//
// GUARD: this real definition sets BRNTRAFFIC_SECTION_DEFINED, so the 48-byte
// placeholder `struct Section` in BrnTrafficHull.h yields to it whenever this header
// is included first.
// =============================================================================

#include "BrnCommonTypes.h"               // Vector3, VecFloat (= rw::math::vpu::Vector3 / Vector4)
#include "BrnTrafficSharedConstants.h"    // BrnTraffic::Side (FindNeighbourForRung arg)
#include <cstddef>                        // offsetof (host layout static_asserts)

namespace BrnTraffic
{
    struct Hull;   // fwd: FindNeighbourForRung takes a (const Hull*); real home BrnTrafficHull.h

    // BrnTrafficSection.h:81 -- a cross-hull lane join. Two adjacent hulls that share a
    // stretch of lane record, per neighbour, the target section plus how our rungs map onto
    // theirs (WorldMap::WalkLaneLeft converts our rung/parameter into the neighbour's frame:
    // theirStart + (ours - ourStart)). 4-byte record (X360 GetNeighbour stride == 4).
    struct Neighbour
    {
        u8 muSection;        // +0  (:83)  neighbour section index in the target hull
        u8 muSharedLength;   // +1  (:84)
        u8 muOurStartRung;   // +2  (:85)  first shared rung on our section
        u8 muTheirStartRung; // +3  (:86)  first shared rung on the neighbour section

        // BrnTrafficSection.h:92 -- own TU (not this slice); declaration only.
        f32 ConvertOurParameterToTheirs(f32 lfParameter) const;
    };

    // BrnTrafficSection.h:50 -- a lane "rung": the two cross-lane endpoints that span
    // one lane at one point along a section. maPoints[0]/[1] are the left/right (or
    // start/end) endpoints; their difference is the lateral "right" direction.
    struct LaneRung
    {
        // BrnTrafficSection.h:52 -- the two rung endpoints (16-byte SIMD lanes).
        Vector3 maPoints[2];

        // BrnTrafficSection.h:57 / :61 / :65 -- attested members.
        // GetCentrePos and EndianSwap live in their own (not-yet-reconstructed) TUs;
        // declared here so the home is faithful, bodied elsewhere. NOT owned by this
        // slice. GetRightVector @ 0x821F4A88 is owned and bodied in
        // BrnTrafficSection.cpp.
        Vector3 GetCentrePos() const;    // :57
        Vector3 GetRightVector() const;  // :61  (@ 0x821F4A88)
        void    EndianSwap();            // :65
    };

    // BrnTrafficSection.h:106 -- one directed lane section of the traffic graph.
    // 48-byte record (DWARF members below; X360 GetSection stride == 48).
#ifndef BRNTRAFFIC_SECTION_DEFINED
#define BRNTRAFFIC_SECTION_DEFINED
    struct Section
    {
        u32 muRungOffset;            // +0x00  (:108)  first global rung of this section
        u8  muNumRungs;              // +0x04  (:109)  rung count (GetNumSegments = this-1)
        u8  muStopLineOffset;        // +0x05  (:111)
        u8  muNumStopLines;          // +0x06  (:112)
        u8  muSpanIndex;             // +0x07  (:113)
        u16 mauForwardHulls[3];      // +0x08  (:115)
        u16 mauBackwardHulls[3];     // +0x0E  (:116)
        u8  mauForwardSections[3];   // +0x14  (:117)
        u8  mauBackwardSections[3];  // +0x17  (:118)
        u8  muTurnLeftProb;          // +0x1A  (:120)
        u8  muTurnRightProb;         // +0x1B  (:121)
        u16 muNeighbourOffset;       // +0x1C  (:123)
        u8  muLeftNeighbourCount;    // +0x1E  (:124)
        u8  muRightNeighbourCount;   // +0x1F  (:125)
        u8  muChangeLeftProb;        // +0x20  (:126)
        u8  muChangeRightProb;       // +0x21  (:127)
        u8  maPad22[2];              // +0x22  (alignment hole before mfSpeed)
        f32 mfSpeed;                 // +0x24  (:129)
        f32 mfLength;                // +0x28  (:130)
        u8  maPad44[4];              // +0x2C  (tail pad to the X360's 48-byte footprint)

        // -- BrnTrafficSection.h:138 accessor family (this batch) --------------------
        // A "segment" is the span between two adjacent rungs; a section with muNumRungs
        // rungs therefore has muNumRungs-1 segments.
        u32   GetNumSegments() const;                                                     // :138  @ 0x821F4B78

        // Position along the lane at fractional parameter lfParam within luSegment,
        // interpolated from the global rung table lpaGlobalRungs.
        void  CalcPositionAtParameter(const LaneRung* lpaGlobalRungs, VecFloat lfParam,   // :146  @ 0x821F4BD8
                                      u32 luSegment, Vector3& lrResult) const;

        // Lane tangent (forward direction) at the parameter; same rung-table sampling as
        // CalcPositionAtParameter. Consumed by BrnDirector::WorldMap::GetSafePositionNearest.
        void  CalcDirectionAtParameter(const LaneRung* lpaGlobalRungs, VecFloat lfParam,  // :150 (DWARF)
                                       u32 luSegment, Vector3& lrDirection) const;

        // Full lane frame (position + forward + up) at the parameter. WorldMap uses the
        // position and the up vector (drops the safe camera point 2.25 below the lane).
        void  CalcTransformAtParameter(const LaneRung* lpaGlobalRungs, VecFloat lfParam,  // :156 (DWARF)
                                       u32 luSegment, Vector3& lrPosition,
                                       Vector3& lrDirection, Vector3& lrUp) const;

        // First neighbouring section reachable from luRung on the given side, or 0xFFFF if
        // none. lpHull owns the neighbour table. Consumed by WorldMap::WalkLaneLeft.
        u16   FindNeighbourForRung(u32 luRung, Side leSide, const Hull* lpHull) const;    // :129 (DWARF)

        // Global (whole-graph) rung id of the given local segment index.
        s32   GetGlobalRungForSegment(VecFloat lfParam, u32 luSegment) const;             // :160  @ 0x821F5068

        // Signed arc-length from (lfParamA, luSegmentA) to (lfParamB, luSegmentB) along
        // this section, using the shared cumulative rung-length table.
        f32   CalcSignedDistanceAlongSection(f32 lfParamA, u32 luSegmentA,                // :204  @ 0x82705BC0
                                             f32 lfParamB, u32 luSegmentB,
                                             const f32* lpafRungLengths) const;

        // -- BrnTrafficSection.h siblings this batch depends on (bodied elsewhere) ----
        // Arc-length from the section start to (lfParam, luSegment). Declared here so
        // CalcSignedDistanceAlongSection can call it; bodied in its own slice.
        f32   CalcDistanceAlongSection(f32 lfParam, u32 luSegment,                        // :188
                                       const f32* lpafRungLengths) const;
    };

    // Host layout contract with tools/assets/bundles/lane_transcode.py's emitter. Section
    // holds NO pointers (muRungOffset / muNeighbourOffset / muStopLineOffset are indices
    // into the owning Hull's tables), so its stride is identical on target and host -- and
    // the 48-byte footprint is load-bearing for Hull::GetSection's `&mpaSections[i]`.
    static_assert(offsetof(Section, mfSpeed)  == 0x24, "Section::mfSpeed");
    static_assert(offsetof(Section, mfLength) == 0x28, "Section::mfLength");
    static_assert(sizeof(Section) == 48, "Section stride (Hull::GetSection @0x821F52E0 uses 48)");
    static_assert(sizeof(LaneRung) == 32, "LaneRung stride");
    static_assert(sizeof(Neighbour) == 4, "Neighbour stride (Hull::GetNeighbour @0x821F5358)");
#endif
}
