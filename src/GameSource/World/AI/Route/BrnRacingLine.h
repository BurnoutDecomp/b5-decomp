#ifndef BRN_RACING_LINE_H
#define BRN_RACING_LINE_H

// BrnAI::RacingLine -- the per-driver racing-line working state embedded BY VALUE in
// BrnAI::AIDriver (guest @0xF20 .. @0x1B30, 0xC10 bytes). ClearSectionCache is called from
// AIDriver::Prepare / AIDriver::InitialiseRacingLine; the RacingLineGenerator fills the section
// cache; the AIDriver bodies read the trailing scalars (centre-line samples, traffic-impact
// proximity, spread distance, initialised flags).
//
// SHAPE: DecFIGS DWARF GameSource/World/AI/RacingLine/BrnRacingLine.h:149 (SectionData) and
// :246 (RacingLine), every member gated on the X360 asm:
//   * ClearSectionCache @0x8276E090 walks r11 = this+0xA8 with stride 0xB0 storing
//       stw 9999 @r11-0x08   -> maSectionCache[i].mHardNoGoMap.miSectionIndex (entry+0x90)
//       stb 0    @r11+0x00   -> maSectionCache[i].mHardNoGoMap.mbReady        (entry+0x98)
//       sth 999  @r11+0x10   -> maSectionCache[i].mCachedSectionIndex         (entry+0xA8)
//       stb 0    @r11+0x12   -> maSectionCache[i].mbTargetUpToDate            (entry+0xAA)
//     i.e. maSectionCache[0] sits at this+0x10 and each entry is 0xB0 bytes on the console; then
//     stw 0/-1/0 @0xBC0/0xBC4/0xBC8 -> miSectionToSpread / miBackwardsStep / miHNGLineStart.
//   * AIDriver::Update @0x8279AF0C..AF50 reads mbIsInitialised (this+0xBD0 == driver+0x1AF0) and
//     stores mbCentreLineHereKnown (this+0xBD1); passes &mCentreHere (+0xBE0) / &mCentreAhead
//     (+0xBF0) to RacingLineGenerator::GetCentreCentreLineHere.
//   * AIDriver::ProximitySpeed @0x82770800 reads mfImmediateDistanceToTrafficImpact (+0xB1C) and
//     mfImmmediateApproachSpeedOfTrafficAhead (+0xB20); GenerateRacingLine writes mCarPos (+0xB30),
//     mfDefaultPerpendicularOffset (+0xB10), mbDefiniteDestination (+0xB14), mfRoadPlacement
//     (+0xB24); InitialiseRacingLine writes mfSpreadDistance (+0xBCC) and miLastKnownSectionID
//     (+0xC00); Prepare writes mfCentreLineAhead (+0xC04) / mfCentreLineAheadRecip (+0xC08).
//
// HOST LAYOUT NOTE: SectionData carries two pointers (mpLineSection / mpTargetPortal), so its
// host size is NOT the console 0xB0 and this class is NOT offset-pinned on the host. AIDriver
// therefore keeps the guest span as an opaque pad and holds the usable RacingLine as a host-side
// member (see BrnAIDriver.h). Never carry the console offsets above onto the host.
//
// VISIBILITY: members are public so the AIDriver / RacingLineGenerator bodies reach them by
// name (the DWARF class keeps them private behind inlined accessors).

#include "types.hpp"
#include "BrnCommonTypes.h"                                  // Vector2 / Vector3 / Vector4
#include "GameSource/World/AI/RacingLine/BrnHardNoGoMap.h"   // HardNoGoMap (embedded per section)
#include "SharedClasses/AI/AISectionsResourceType.h"         // AISection (SetFastSectionCorners needs the layout)

namespace BrnAI
{
    struct AISection;   // SharedClasses/AI/AISectionsResourceType.h
    struct Portal;      // BrnAIPortal.h
    class  RacingLineGenerator;

    // DWARF BrnRacingLine.h:4 / :7
    const s32 KI_RACING_LINE_MAX_AVAILABLE_SECTIONS = 16;
    const s32 KI_RACING_LINE_MAX_NODES              = 10;

    // DWARF BrnRacingLine.h:10 / :13 -- the centre-line-ahead lerp bounds AIDriver::Prepare draws
    // between with a per-car random (rodata flt_820C3DDC / 0x820C3DE0, read from the image).
    const f32 KF_CENTRE_LINE_AHEAD_CLOSE = 0.9975f;
    const f32 KF_CENTRE_LINE_AHEAD_FAR   = 0.985f;

    // DWARF BrnRacingLine.h:149 -- one cached AI section of the racing line.
    struct SectionData
    {
        Vector4          mA4XCoords;               // :20  (console +0x00)
        Vector4          mA4YCoords;               // :23  (+0x10)
        Vector4          mEdge4X;                  // :26  (+0x20)
        Vector4          mEdge4Y;                  // :29  (+0x30)
        Vector4          mPortalEntranceAndExit;   // :32  (+0x40)  entrance in x/y, exit in z/w
        HardNoGoMap      mHardNoGoMap;             // :35  (+0x50)
        const AISection* mpLineSection;            // :38  (+0xA0 guest)
        const Portal*    mpTargetPortal;           // :41  (+0xA4 guest)
        s16              mCachedSectionIndex;      // :44  (+0xA8 guest)
        bool             mbTargetUpToDate;         // :47  (+0xAA guest)

        // Inlined by RacingLineGenerator::SetupSectionExit @0x8278F548 (vrlimi128 SetZ<X>/SetW<Y>):
        // drop the exit point's x/y into the z/w lanes of the portal vector, x/y untouched.
        void SetSectionExit(const Vector2& lExit)
        {
            mPortalEntranceAndExit.z = lExit.x;
            mPortalEntranceAndExit.w = lExit.y;
        }

        // ---- ADDITIVE (aiwave2 lane R1, 2026-09-05) ------------------------------------
        // DWARF BrnRacingLine.h:170 / :167 / :174. The section portal pair lives packed in
        // mPortalEntranceAndExit -- entrance in the x/y lanes, exit in z/w -- and every
        // console reader is inlined. The exit read is attested by
        // RacingLineGenerator::GenerateInOutVectors @0x82780688
        // (`vpermwi128 v123, v124, 0xBF` == word-select {2,3,3,3}, i.e. the z/w pair pulled
        // down into the x/y lanes) and again at @0x8277099C; the entrance read is the raw
        // x/y lanes of the same load (@0x82780684). The z/w lanes of the RESULT are console
        // garbage there (the permute duplicates lane 3); they are zeroed here because no
        // consumer reads them.
        Vector2 GetSectionEntrance() const
        {
            Vector2 lEntrance;
            lEntrance.x = mPortalEntranceAndExit.x;
            lEntrance.y = mPortalEntranceAndExit.y;
            lEntrance.z = 0.0f;
            lEntrance.w = 0.0f;
            return lEntrance;
        }

        Vector2 GetSectionExit() const
        {
            Vector2 lExit;
            lExit.x = mPortalEntranceAndExit.z;
            lExit.y = mPortalEntranceAndExit.w;
            lExit.z = 0.0f;
            lExit.w = 0.0f;
            return lExit;
        }

        void SetSectionEntrance(const Vector2& lEntrance)
        {
            mPortalEntranceAndExit.x = lEntrance.x;
            mPortalEntranceAndExit.y = lEntrance.y;
        }

        // DWARF BrnRacingLine.h:182. Recovered from the byte-identical inlined block in
        // RacingLineGenerator::CacheLocalSections @0x8278EE0C..0x8278EE80 (and again at
        // @0x8278F0D8..0x8278F158): the section's four PACKED corners are gathered into two
        // SoA vectors (all four X lanes, all four Z lanes), each is rotated one word right
        // (`vpermwi128 v12, v0, 0xC6` == word-select {3,0,1,2} == "the PREVIOUS corner per
        // edge lane"), the ROTATED sets are stored as mA4XCoords / mA4YCoords, and the
        // difference (corner[i] - corner[i-1]) is stored as mEdge4X / mEdge4Y.
        // That is exactly the SoA half-plane form BrnAI::IsInsideSectionFast walks:
        // per lane i, cross = mEdge4X[i]*(y - mA4YCoords[i]) - mEdge4Y[i]*(x - mA4XCoords[i]).
        // AISection::Vector2 is the PACKED 2-float corner type; its .y lane is the world Z
        // (DWARF AISectionsData.h GetCornerX/GetCornerZ), which is the racing line's 2D y.
        void SetFastSectionCorners(const AISection* lpSection)
        {
            f32 lafX[KI_AI_SECTION_EDGES];
            f32 lafY[KI_AI_SECTION_EDGES];
            for (s32 liCorner = 0; liCorner < KI_AI_SECTION_EDGES; ++liCorner)
            {
                lafX[liCorner] = lpSection->mpaCorners[liCorner].x;
                lafY[liCorner] = lpSection->mpaCorners[liCorner].y;
            }

            f32* lapPrevX[KI_AI_SECTION_EDGES] = { &mA4XCoords.x, &mA4XCoords.y,
                                                   &mA4XCoords.z, &mA4XCoords.w };
            f32* lapPrevY[KI_AI_SECTION_EDGES] = { &mA4YCoords.x, &mA4YCoords.y,
                                                   &mA4YCoords.z, &mA4YCoords.w };
            f32* lapEdgeX[KI_AI_SECTION_EDGES] = { &mEdge4X.x, &mEdge4X.y,
                                                   &mEdge4X.z, &mEdge4X.w };
            f32* lapEdgeY[KI_AI_SECTION_EDGES] = { &mEdge4Y.x, &mEdge4Y.y,
                                                   &mEdge4Y.z, &mEdge4Y.w };

            for (s32 liEdge = 0; liEdge < KI_AI_SECTION_EDGES; ++liEdge)
            {
                const s32 liPrevious = (liEdge + KI_AI_SECTION_EDGES - 1) % KI_AI_SECTION_EDGES;
                *lapPrevX[liEdge] = lafX[liPrevious];
                *lapPrevY[liEdge] = lafY[liPrevious];
                *lapEdgeX[liEdge] = lafX[liEdge] - lafX[liPrevious];
                *lapEdgeY[liEdge] = lafY[liEdge] - lafY[liPrevious];
            }
        }
    };

    // DWARF BrnRacingLine.h:246
    class RacingLine
    {
    public:
        static const s32 KI_SECTION_CACHE_COUNT = KI_RACING_LINE_MAX_AVAILABLE_SECTIONS;

        // @0x8276E090 -- reset every cached section to its empty sentinels and clear the
        // spread cursor triple. Returns this (the X360 threads r3 back out).
        RacingLine* ClearSectionCache();

        // DWARF hint (BrnAIDriver.cpp:876 InitialiseRacingLine calls RacingLine::SetInitialised);
        // the X360 inlines it to the `stb` at driver+0x1AF0.
        void SetInitialised(bool lbInitialised) { mbIsInitialised = lbInitialised; }
        bool IsInitialised() const              { return mbIsInitialised; }

        // ---- storage (DWARF declaration order) ----------------------------------------------
        s32          mFirstSectionInCache;                                   // :70  (+0x00)
        s32          mLastSectionInCache;                                    // :73  (+0x04)
        SectionData  maSectionCache[KI_SECTION_CACHE_COUNT];                 // :76  (+0x10, 16 x 0xB0 guest)
        f32          mfDefaultPerpendicularOffset;                           // :79  (+0xB10)
        bool         mbDefiniteDestination;                                  // :82  (+0xB14)
        f32          mfImmediateTimeToTrafficImpact;                         // :85  (+0xB18)
        f32          mfImmediateDistanceToTrafficImpact;                     // :88  (+0xB1C)
        f32          mfImmmediateApproachSpeedOfTrafficAhead;                // :91  (+0xB20) (sic, DWARF spelling)
        f32          mfRoadPlacement;                                        // :94  (+0xB24)
        Vector3      mCarPos;                                                // :97  (+0xB30)
        f32          maStretchDistanceForHNG[32];                            // :100 (+0xB40)
        s32          miSectionToSpread;                                      // :103 (+0xBC0) ClearSectionCache -> 0
        s32          miBackwardsStep;                                        // :106 (+0xBC4) ClearSectionCache -> -1
        s32          miHNGLineStart;                                         // :109 (+0xBC8) ClearSectionCache -> 0
        f32          mfSpreadDistance;                                       // :112 (+0xBCC)
        bool         mbIsInitialised;                                        // :115 (+0xBD0)
        bool         mbCentreLineHereKnown;                                  // :118 (+0xBD1)
        Vector2      mCentreHere;                                            // :121 (+0xBE0)
        Vector2      mCentreAhead;                                           // :124 (+0xBF0)
        s32          miLastKnownSectionID;                                   // :127 (+0xC00)
        f32          mfCentreLineAhead;                                      // :130 (+0xC04)
        f32          mfCentreLineAheadRecip;                                 // :133 (+0xC08)

        static const u16 KU_COST_EMPTY     = 999;    // ClearSectionCache sth 0x3E7 -> mCachedSectionIndex
        static const s32 KI_DISTANCE_EMPTY = 9999;   // ClearSectionCache stw 0x270F -> mHardNoGoMap.miSectionIndex
    };
}

#endif
