// =================================================================================================
// BrnAI::RacingLineGenerator -- THE SECTION-CACHE HALF (aiwave2 lane R1, 2026-09-05)
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX. Every body below is the console's;
// the DecFIGS DWARF (references/DecFIGS/dwarfdump/.../BrnRacingLineGenerator.h) supplies the
// declaration shape and the local-variable names the assert strings confirm.
//
//   @0x827655D0  GetSectionPointer                        (no IDA export -- image-disassembled)
//   @0x82776280  GetLocalSectionID
//   @0x827765A8  GetNearSectionID
//   @0x827685B8  GetSectionPortal
//   @0x82775E50  GuessInwardsSectionPortal
//   @0x82768710  SetTargetUpToDate
//   @0x82775C10  LookForStraightSectionAhead
//   @0x82775D30  LookForStraightSectionBehind
//   @0x827767F0  GetSimpleHermite
//   @0x82776D70  GetIterativeHermite                      (no IDA export -- image-disassembled)
//   @0x827815F8  GetSectionInterpPosition
//   @0x8278EC08  GetHalfRoadWidthHere
//   @0x8278F408  GetRouteCentre
//   @0x8278F188  CalculateIntersectionWithProjectedRoute
//   @0x82780638  GenerateInOutVectors
//   @0x82780518  SetUpHardNoGoMap
//   @0x8278E930  GetCentreCentreLineHere
//   @0x8278ECE8  CacheLocalSections
//   @0x8278F548  SetupSectionExit                         (pre-existing; re-verified this wave)
//   @0x8278F600  DropHardNoGoLinesIntoMap                 (pre-existing; re-verified this wave)
//   plus CacheUpToDate, which has no export because it is inlined at all eight of its call sites.
//
// TWO STANDING CONVENTIONS APPLY THROUGHOUT.
//  1. Vector2 here is the 16-byte rw::math::vpu register (four lanes). The console does most of
//     this arithmetic four-lane; where the z/w lanes carry nothing a consumer reads (they are
//     duplicated garbage out of the `vpermwi128 ..., 0xBF` portal unpack) they are published as
//     zero. Each such spot says so.
//  2. Flattening is world (x, Z) -> 2D (x, y) -- `BrnMath::Flatten`, and AISection's PACKED
//     corner type stores (GetCornerX, GetCornerZ) in its .x / .y.
//
// CONSTANTS RECOVERED FROM THE IMAGE (big-endian, file offset = VA - 0x82000000):
//   flt_82001C98 = 1.0                  flt_82001CC0 = 0.0
//   flt_820C4168 = 0.5                  flt_820C4150 = 10.0   flt_820C4154 = 3.0
//   flt_82004D04 = 1.5                  flt_82004000 = 16.0   flt_820C424C = 0.1
//   flt_820C47FC = 0.5                  flt_82002138 = 0.01
//   unk_820C7D34 = unk_820C7D38 = 0.05  flt_820C3B70 = FLT_EPSILON
//   unk_820C3B40 = 2^-16 (the rw::math::vpu::IsSimilar epsilon)
//
// ⭐⭐ FINDING FOR THE CONDUCTOR -- BrnAI::IsInsideSectionFast (GameSource/World/AI/BrnAIUtils.cpp,
// NOT this lane's file) HAS ITS HALF-PLANE TEST INVERTED, and it is the single predicate this
// whole file's section lookup runs on. The X360 body @0x82768680 computes, per lane,
//   cross = mEdge4X*(y - mA4XCoords) - mEdge4Y*(x - mA4YCoords)      [SoA, four edges at once]
// then `vcmpgefp ; vnot ; vperm(<AND-reduce>) ; vcmpequw ~0 ; vcmpeqfp. 0` and returns
// "not all lanes zero" -- i.e. INSIDE == all four crosses are NEGATIVE. The committed host body
// returns true when every cross is `>= 0.0f`, the exact opposite. Three independent corroborations:
//   (a) the AND-reduce permute constant is spelled out as an immediate in
//       GetSectionInterpPosition @0x8278162C..0x82781658 (`lis r11,4 ; ori r11,r11,0x80C` ->
//       0x0004080C x4 = gather byte 0 of each of the four mask words), so the reduction really is
//       "all four lanes", not "any";
//   (b) AISection::IsInside @0x82677058 -- the scalar, non-SoA twin, already committed in
//       SharedClasses/AI/AISection.cpp -- returns FALSE when `edgeX*relY > edgeY*relX`, i.e.
//       inside is `cross <= 0`; and
//   (c) SectionData::SetFastSectionCorners (recovered this wave from CacheLocalSections
//       @0x8278EE0C) stores the PREVIOUS corner in mA4XCoords/mA4YCoords and (corner - previous)
//       in mEdge4X/mEdge4Y -- the identical winding IsInside walks.
// Left untouched because BrnAIUtils.cpp is outside this lane. With it inverted, GetLocalSectionID
// answers KI_INVALID_SECTION_INDEX for every position and the whole racing line is inert.
// =================================================================================================

#include "GameSource/World/AI/RacingLine/BrnRacingLineGenerator.h"

#include <cmath>
#include <cfloat>

#include "GameSource/World/AI/Route/BrnRacingLine.h"     // RacingLine + SectionData
#include "GameSource/World/AI/Route/BrnRoute.h"          // Route / RouteNode
#include "GameSource/World/AI/Route/BrnRouteMapModule.h" // SectionAndPortalIndices
#include "GameSource/World/AI/BrnAICar.h"                // AICar (GetPosition/GetDirection/GetBestSectionIndex)
#include "GameSource/World/AI/BrnAIPortal.h"             // Portal / BoundaryLine
#include "GameSource/World/AI/BrnAIUtils.h"              // IsInsideSectionFast / Calc2DIntersectionEquationData
#include "GameSource/Math/BrnMathUtils.h"                // BrnMath::Flatten
#include "SharedClasses/AI/AISectionsResourceType.h"     // AISection / AISectionsData
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"       // CGS_ASSERT + Begin/Fire/End
#include "GameShared/GameClasses/Development/CgsStrStream.h"          // CgsDev::StrStream
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // Start/StopMonitor

namespace BrnAI
{
// ---- the class's static state (DWARF BrnRacingLineGenerator.h:380..:385, :393) ------------
// DEFINED ONCE, here. No other RacingLineGenerator partfile may define them (LNK2005).
// ARTIST data at 0x82F30254..0x82F30268 contains six 0xFFFFFFFF words.
// InitialiseRacingLine @0x8278FBF4 registers them only when the first handle is -1.
s32  RacingLineGenerator::miGenerateRacingLinePM = -1;
s32  RacingLineGenerator::miGenerateInSectionPM  = -1;
s32  RacingLineGenerator::miHNGMapGenerationPM1  = -1;
s32  RacingLineGenerator::miHNGMapGenerationPM2  = -1;
s32  RacingLineGenerator::miHNGMapGenerationPM3  = -1;
s32  RacingLineGenerator::miFarAheadPM           = -1;
bool RacingLineGenerator::mbDrawAvoidanceDebug   = false;

namespace
{
// ---- baked constants -------------------------------------------------------------------
// Per-frame budget of hard-no-go lines the map is asked to place (the +0x3C step in
// DropHardNoGoLinesIntoMap @0x8278F600).
const s32 KI_MAX_HNG_LINES_TO_PLACE_PER_FRAME = 60;

// BrnWorld::KI_INVALID_SECTION_INDEX. Asm literal `cmpwi cr6, r29, 0x7FFF` @0x827762D4 and
// `li r3, 0x7FFF` @0x82776584.
const s32 KI_INVALID_SECTION_INDEX = 0x7FFF;

// The cache is 16 slots addressed by `sectionIndex & 15` (`clrlwi r11, r5, 28` everywhere).
const s32 KI_SECTION_CACHE_MASK = KI_RACING_LINE_MAX_AVAILABLE_SECTIONS - 1;

// The four-corner footprint wraps with `clrlwi r31, r11, 30` (== & 3) in
// GuessInwardsSectionPortal @0x82776154 / @0x827761C8.
const s32 KI_AI_SECTION_EDGES_MASK = KI_AI_SECTION_EDGES - 1;

// flt_820C3B70 lane 0 == FLT_EPSILON: the RwMath::IsZero epsilon.
const f32 KF_ZERO_EPSILON = 1.1920929e-07f;

// unk_820C3B40 lane 0 == 0x37800000 == 2^-16: the rw::math::vpu::IsSimilar epsilon
// GenerateInOutVectors / GuessInwardsSectionPortal match portal points with.
const f32 KF_PORTAL_MATCH_EPSILON = 1.52587890625e-05f;

// flt_820C4150 / flt_820C4154 / flt_82004D04 -- GetHalfRoadWidthHere's fallback, floor and
// inset (`lfs` @0x8278ECCC / @0x8278ECA0 / @0x8278EC94).
const f32 KF_HALF_ROAD_WIDTH_FALLBACK = 10.0f;
const f32 KF_HALF_ROAD_WIDTH_MIN      = 3.0f;
const f32 KF_HALF_ROAD_WIDTH_INSET    = 1.5f;

// unk_820C7D34 == unk_820C7D38 == 0.05f -- how far in from a maximal edge
// CalculateIntersectionWithProjectedRoute clamps a route that leaves the section sideways.
const f32 KF_ROUTE_EDGE_INSET = 0.05f;

// flt_82004000 == 16.0f -- GenerateInOutVectors' cap on the tangent length
// (`fsel f31, f13 - 16.0, 16.0, f13` @0x827807A4..0x827807A8 == min(length, 16)).
const f32 KF_IN_OUT_VECTOR_MAX_LENGTH = 16.0f;

// flt_820C424C == 0.1f -- how much further along the curve GetCentreCentreLineHere samples
// its "ahead" point (mirrored back past the "here" point when it would leave the section).
const f32 KF_CENTRE_LINE_AHEAD_STEP = 0.1f;

// flt_820C47FC == 0.5f -- GetIterativeHermite's relaxation factor, and flt_82002138 == 0.01f
// its minimum squared chord length. It runs a fixed five iterations
// (`addi r21,r21,1 ; cmpwi r21,5 ; blt` @0x827776D0).
const f32 KF_ITERATIVE_HERMITE_STEP           = 0.5f;
const f32 KF_ITERATIVE_HERMITE_MIN_CHORD_SQ   = 0.01f;
const s32 KI_ITERATIVE_HERMITE_ITERATIONS     = 5;

// RouteMapModuleIO::KI_MAX_PLAYER_ROUTE_EXTRAPOLATION_GENERATED_SECTIONS -- the assert text at
// BrnRacingLineGenerator.cpp:396 names it; the asm literal is `cmpwi cr6, r31, 0x10`
// @0x8278EEE4 and the array's live count is stored as 16 @0x8278EEE8.
const s32 KI_MAX_PLAYER_ROUTE_EXTRAPOLATION_GENERATED_SECTIONS = 16;

const u8 KX8_SECTION_FLAG_JUNCTION = 0x10;

// The unity-build file string every streamed assert in this TU cites.
const char* const KPC_SOURCE_FILE_CPP =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../World/AI/RacingLine/BrnRacingLineGenerator.cpp";
// GetSectionPointer's assert cites the HEADER (it is an inline there).
const char* const KPC_SOURCE_FILE_H =
    "..\\..\\..\\GameSource\\World/AI/RacingLine/BrnRacingLineGenerator.h";
// The AISection accessor asserts GuessInwardsSectionPortal / CacheLocalSections inline.
const char* const KPC_SOURCE_FILE_AISECTIONS = "..\\..\\..\\SharedClasses\\AI/AISectionsData.h";

// ---- small shared helpers ---------------------------------------------------------------
// RwMath::IsZero on a 2D vector: |x| <= FLT_EPSILON && |y| <= FLT_EPSILON. The console runs it
// lane-wise with `vandc` (clear the sign bit) + `vcmpgtfp` + the 0x0004080C AND-reduce permute.
inline bool IsZero2D(f32 lfX, f32 lfY)
{
    return !(fabsf(lfX) > KF_ZERO_EPSILON) && !(fabsf(lfY) > KF_ZERO_EPSILON);
}

// rw::math::vpu::IsSimilar on the x/y lanes: no lane differs by more than 2^-16. The console
// duplicates x/y into z/w with `vrlimi128 v, v, 3, 2` so all four compared lanes are the pair.
inline bool IsSimilar2D(f32 lfAX, f32 lfAY, f32 lfBX, f32 lfBY)
{
    return !(fabsf(lfAX - lfBX) > KF_PORTAL_MATCH_EPSILON)
        && !(fabsf(lfAY - lfBY) > KF_PORTAL_MATCH_EPSILON);
}

// The lane-wise `vcmpeqfp.` self-compare the console uses as "is this finite/not-NaN".
inline bool IsValid2D(const Vector2& lrVector)
{
    return (lrVector.mV.x == lrVector.mV.x) && (lrVector.mV.y == lrVector.mV.y);
}

inline Vector2 Make2D(f32 lfX, f32 lfY)
{
    return Vector2(lfX, lfY);
}

// One 2D normalise. The console spells it vrsqrtefp + two Newton-Raphson steps with a
// `vsel`-guarded zero; std::sqrt is the exact form of the same expression.
inline Vector2 Normalise2D(f32 lfX, f32 lfY)
{
    const f32 lfLengthSq = (lfX * lfX) + (lfY * lfY);
    if (lfLengthSq == 0.0f)
    {
        return Make2D(0.0f, 0.0f);
    }
    const f32 lfInvLength = 1.0f / std::sqrt(lfLengthSq);
    return Make2D(lfX * lfInvLength, lfY * lfInvLength);
}

// |v|, with the console's exact zero (the `vsel` on `vcmpeqfp(0, lengthSq)`).
inline f32 Magnitude2D(f32 lfX, f32 lfY)
{
    const f32 lfLengthSq = (lfX * lfX) + (lfY * lfY);
    return (lfLengthSq == 0.0f) ? 0.0f : std::sqrt(lfLengthSq);
}

// [FLAG header_request] AISection::GetCorner / GetCornerX / GetCornerZ (DWARF AISectionsData.h
// :308 / :311 / :314) have no declaration in the host SharedClasses/AI/AISectionsResourceType.h,
// which this lane does not own. GuessInwardsSectionPortal @0x82775F4C..0x82775FA8 inlines the
// pair (bounds assert, then the packed corner's x, then the same assert and its y -- the
// arguments evaluate right-to-left, which is why the Z accessor's line 1125 is emitted before
// the X accessor's 1109). Reproduced here as a file-local helper so the asserts and the access
// pattern stay faithful and no second, guessed declaration appears in another TU.
// DELETE-WHEN AISectionsResourceType.h declares the three accessors; then call GetCorner.
Vector2 GetSectionCorner(const AISection* lpSection, s32 liCornerIndex)
{
    CGS_ASSERT(liCornerIndex >= 0 && liCornerIndex < KI_AI_SECTION_EDGES,
               "liCornerIndex >= 0 && liCornerIndex < KI_AI_SECTION_EDGES");   // :1125 (GetCornerZ)
    const f32 lfZ = lpSection->mpaCorners[liCornerIndex].y;

    CGS_ASSERT(liCornerIndex >= 0 && liCornerIndex < KI_AI_SECTION_EDGES,
               "liCornerIndex >= 0 && liCornerIndex < KI_AI_SECTION_EDGES");   // :1109 (GetCornerX)
    const f32 lfX = lpSection->mpaCorners[liCornerIndex].x;

    return Make2D(lfX, lfZ);
}

// ---- the streamed asserts (BeginAssert ; StrStream ; FireAssert ; EndAssert) --------------
// The retail X360 build DOES emit these -- each is a full StrStream sequence in the asm, not a
// compiled-out CGS_ASSERT -- so they are reproduced with the console's own file/line.
void FireSectionNotCachedAssert(s32 liSectionIndex, s32 liCacheIndex)
{
    // @0x827655FC..0x827656BC: "Cached Sections wrapped [Section" << index << " not cached in "
    // << (index & 15) << "]", BrnRacingLineGenerator.h:418.
    char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
    CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
    lStream << "Cached Sections wrapped [Section" << liSectionIndex
            << " not cached in " << liCacheIndex << "]";
    CgsDev::Assert::BeginAssert();
    CgsDev::Assert::FireAssert(lacMessageBuffer, CGS_ASSERT_LOCATION(KPC_SOURCE_FILE_H, 418));
    CgsDev::Assert::EndAssert();
}

void FireStreamedTextAssert(const char* lpcText, s32 liLine)
{
    char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
    CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
    lStream << lpcText;
    CgsDev::Assert::BeginAssert();
    CgsDev::Assert::FireAssert(lacMessageBuffer, CGS_ASSERT_LOCATION(KPC_SOURCE_FILE_CPP, liLine));
    CgsDev::Assert::EndAssert();
}

void FireSectionIndexAssert(const char* lpcPrefix, s32 liValue, const char* lpcSuffix, s32 liLine)
{
    char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
    CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
    lStream << lpcPrefix << liValue << lpcSuffix;
    CgsDev::Assert::BeginAssert();
    CgsDev::Assert::FireAssert(lacMessageBuffer, CGS_ASSERT_LOCATION(KPC_SOURCE_FILE_CPP, liLine));
    CgsDev::Assert::EndAssert();
}
}   // anonymous namespace

// =================================================================================================
// GetSectionPointer @0x827655D0  (r3 = this, r4 = lpRacingLine, r5 = liIndex)
//
// No IDA export exists for this address -- IDA knows the name (it appears in the xrefs_from of
// every caller) but wrote no body file, so it was disassembled straight out of the image.
//   0x827655DC  r28 = liIndex ; r26 = liIndex & 15 ; r27 = lpRacingLine + r26*0xB0
//   0x827655EC  lhz/extsh r27+0xB8 (== maSectionCache[r26].mCachedSectionIndex) ; cmpw r28
//   0x827655FC  on mismatch: the streamed "Cached Sections wrapped [...]" assert (header line 418)
//   0x827656C0  UNCONDITIONALLY returns r27 + 0x10 == &maSectionCache[liIndex & 15]
// The assert is a report, not a guard: the console returns the (stale) slot either way.
// =================================================================================================
// ARTIST82790278: render each cached, ready hard-no-go map at the car position.
void RacingLineGenerator::RenderHardNoGoMap(RacingLine* lpRacingLine)
{
    for (s32 i = lpRacingLine->mFirstSectionInCache; i <= lpRacingLine->mLastSectionInCache; ++i)
    {
        if (lpRacingLine->maSectionCache[i & 15].mCachedSectionIndex == i)
        {
            SectionData* lpSection = GetSectionPointer(lpRacingLine, i);
            if (lpSection->mHardNoGoMap.IsReady())
                lpSection->mHardNoGoMap.Render(lpRacingLine->mCarPos);
        }
    }
}

SectionData* RacingLineGenerator::GetSectionPointer(RacingLine* lpRacingLine, s32 liIndex)
{
    const s32 liCacheIndex = liIndex & KI_SECTION_CACHE_MASK;

    if (lpRacingLine->maSectionCache[liCacheIndex].mCachedSectionIndex != liIndex)
    {
        FireSectionNotCachedAssert(liIndex, liCacheIndex);
    }

    return &lpRacingLine->maSectionCache[liCacheIndex];
}

// =================================================================================================
// CacheUpToDate (DWARF BrnRacingLineGenerator.h:226)
//
// No export: the console inlines this three-instruction test at every call site --
// GetSectionPointer @0x827655EC, GetLocalSectionID @0x827763E0/@0x82776438,
// GetNearSectionID @0x8277672C/@0x82776790, SetTargetUpToDate @0x82768728,
// CacheLocalSections @0x8278ED7C, SetUpHardNoGoMap @0x8278053C/@0x82780588,
// CalculateIntersectionWithProjectedRoute @0x8278F1C8, GenerateInOutVectors @0x82780748/@0x82780964.
// Written once here, called by name, per the de-inlining rule.
// =================================================================================================
bool RacingLineGenerator::CacheUpToDate(RacingLine* lpRacingLine, s32 liSectionIndex)
{
    const s32 liCacheIndex = liSectionIndex & KI_SECTION_CACHE_MASK;
    return lpRacingLine->maSectionCache[liCacheIndex].mCachedSectionIndex == liSectionIndex;
}

// =================================================================================================
// GetLocalSectionID @0x82776280  (r3 = this, r4 = lpRacingLine, v1 = lPosition, r5 = liSectionID)
//
// Bracketed by PerfMonCpu Start/StopMonitor(miGenerateInSectionPM == dword_82F30258) on EVERY
// exit. Four probes, in order:
//   0x827762C4  if (!lpRacingLine->mbIsInitialised)                       -> invalid
//   0x827762E4  liSectionID == KI_INVALID_SECTION_INDEX: scan the whole cache window
//               [mFirstSectionInCache .. mLastSectionInCache] and return the first hit
//   0x827763A4  else test liSectionID itself, then +1 and -1 (each guarded by CacheUpToDate)
//   0x8277647C  then walk forwards from liSectionID+2 to mLastSectionInCache
//   0x827764C4  then walk backwards from liSectionID-2 to mFirstSectionInCache
// The inside test is BrnAI::IsInsideSectionFast, called out of line for the single probes and
// inlined for the two walks (identical instruction sequence, @0x82776328 / @0x82776504).
// =================================================================================================
s32 RacingLineGenerator::GetLocalSectionID(RacingLine* lpRacingLine, Vector2 lPosition,
                                           s32 liSectionID)
{
    CgsDev::PerfMonCpu::StartMonitor(miGenerateInSectionPM);

    if (lpRacingLine->mbIsInitialised)
    {
        if (liSectionID == KI_INVALID_SECTION_INDEX)
        {
            for (s32 liSection = lpRacingLine->mFirstSectionInCache;
                 liSection <= lpRacingLine->mLastSectionInCache;
                 ++liSection)
            {
                if (IsInsideSectionFast(GetSectionPointer(lpRacingLine, liSection),
                                        lPosition.mV.x, lPosition.mV.y))
                {
                    CgsDev::PerfMonCpu::StopMonitor(miGenerateInSectionPM);
                    return liSection;
                }
            }
        }
        else
        {
            if (IsInsideSectionFast(GetSectionPointer(lpRacingLine, liSectionID),
                                    lPosition.mV.x, lPosition.mV.y))
            {
                CgsDev::PerfMonCpu::StopMonitor(miGenerateInSectionPM);
                return liSectionID;
            }

            if (CacheUpToDate(lpRacingLine, liSectionID + 1)
                && IsInsideSectionFast(GetSectionPointer(lpRacingLine, liSectionID + 1),
                                       lPosition.mV.x, lPosition.mV.y))
            {
                CgsDev::PerfMonCpu::StopMonitor(miGenerateInSectionPM);
                return liSectionID + 1;
            }

            if (CacheUpToDate(lpRacingLine, liSectionID - 1)
                && IsInsideSectionFast(GetSectionPointer(lpRacingLine, liSectionID - 1),
                                       lPosition.mV.x, lPosition.mV.y))
            {
                CgsDev::PerfMonCpu::StopMonitor(miGenerateInSectionPM);
                return liSectionID - 1;
            }

            for (s32 liSection = liSectionID + 2;
                 liSection <= lpRacingLine->mLastSectionInCache;
                 ++liSection)
            {
                if (IsInsideSectionFast(GetSectionPointer(lpRacingLine, liSection),
                                        lPosition.mV.x, lPosition.mV.y))
                {
                    CgsDev::PerfMonCpu::StopMonitor(miGenerateInSectionPM);
                    return liSection;
                }
            }

            for (s32 liSection = liSectionID - 2;
                 liSection >= lpRacingLine->mFirstSectionInCache;
                 --liSection)
            {
                if (IsInsideSectionFast(GetSectionPointer(lpRacingLine, liSection),
                                        lPosition.mV.x, lPosition.mV.y))
                {
                    CgsDev::PerfMonCpu::StopMonitor(miGenerateInSectionPM);
                    return liSection;
                }
            }
        }
    }

    CgsDev::PerfMonCpu::StopMonitor(miGenerateInSectionPM);
    return KI_INVALID_SECTION_INDEX;
}

// =================================================================================================
// GetNearSectionID @0x827765A8  (r3 = this, r4 = lpRacingLine, v1 = lPosition, r5 = liSectionID)
//
// The neighbour-only twin: the seed section, then +1, then -1, each guarded by CacheUpToDate;
// no walk. Same PerfMon bracket. The leading streamed assert fires when the caller hands it
// KI_INVALID_SECTION_INDEX (@0x827765E0..0x82776658, .cpp:1516) and is a REPORT -- the console
// then runs the whole body anyway, with GetSectionPointer(0x7FFF) resolving to slot 15.
// =================================================================================================
s32 RacingLineGenerator::GetNearSectionID(RacingLine* lpRacingLine, Vector2 lPosition,
                                          s32 liSectionID)
{
    CgsDev::PerfMonCpu::StartMonitor(miGenerateInSectionPM);

    if (liSectionID == KI_INVALID_SECTION_INDEX)
    {
        FireStreamedTextAssert("Invalid section used in guess\n", 1516);
    }

    if (lpRacingLine->mbIsInitialised)
    {
        if (IsInsideSectionFast(GetSectionPointer(lpRacingLine, liSectionID),
                                lPosition.mV.x, lPosition.mV.y))
        {
            CgsDev::PerfMonCpu::StopMonitor(miGenerateInSectionPM);
            return liSectionID;
        }

        if (CacheUpToDate(lpRacingLine, liSectionID + 1)
            && IsInsideSectionFast(GetSectionPointer(lpRacingLine, liSectionID + 1),
                                   lPosition.mV.x, lPosition.mV.y))
        {
            CgsDev::PerfMonCpu::StopMonitor(miGenerateInSectionPM);
            return liSectionID + 1;
        }

        if (CacheUpToDate(lpRacingLine, liSectionID - 1)
            && IsInsideSectionFast(GetSectionPointer(lpRacingLine, liSectionID - 1),
                                   lPosition.mV.x, lPosition.mV.y))
        {
            CgsDev::PerfMonCpu::StopMonitor(miGenerateInSectionPM);
            return liSectionID - 1;
        }
    }

    CgsDev::PerfMonCpu::StopMonitor(miGenerateInSectionPM);
    return KI_INVALID_SECTION_INDEX;
}

// =================================================================================================
// GetSectionPortal @0x827685B8  (r3 = this, r4 = lpSectionData, r5 = &lrLeft, r6 = &lrRight)
//
//   0x827685C4  r31 = lpSectionData->mpTargetPortal  (SectionData +0xA4)
//   0x827685D0  the inlined Portal::GetBoundaryLine(0) bounds assert
//               ("lu8BoundryIndex < mu8NumBoundaryLines", AISectionsData.h:811)
//   0x827685FC  loads the whole 16-byte BoundaryLine and publishes
//                 *r5 = (mfStartX, mfStartY, 0, 0)     (`std r8` zeroes the z/w pair)
//                 *r6 = (mfEndX,   mfEndY,   0, 0)
// The two out params are the portal's two ends, in the boundary line's own order.
// =================================================================================================
void RacingLineGenerator::GetSectionPortal(SectionData* lpSectionData, Vector2& lrLeft,
                                           Vector2& lrRight)
{
    const BoundaryLine* lpBoundaryLine = lpSectionData->mpTargetPortal->GetBoundaryLine(0);

    lrLeft  = Make2D(lpBoundaryLine->mfStartX, lpBoundaryLine->mfStartY);
    lrRight = Make2D(lpBoundaryLine->mfEndX,   lpBoundaryLine->mfEndY);
}

// =================================================================================================
// GuessInwardsSectionPortal @0x82775E50  (r3 = this, r4 = lpSectionData, r5 = &lrLeft, r6 = &lrRight)
//
// Used when the NEXT section is not in the cache, so its portal cannot be read: the console
// instead assumes the section is a quad and returns the edge OPPOSITE the one its own target
// portal sits on.
//   0x82775EA8  publishes the target portal's two ends exactly as GetSectionPortal does
//   0x82775F4C  walks the four footprint edges (liEdge 0..3, previous = (liEdge + 3) & 3),
//               matching corner[previous] against lrLeft and corner[liEdge] against lrRight
//               with rw::math::vpu::IsSimilar (2^-16, unk_820C3B40)
//   0x827760DC  no match -> the streamed "Failed to match portal with section edge\n"
//               (.cpp:766) and liMatchedEdge stays -1
//   0x82776150  publishes lrRight = corner[(liMatchedEdge + 1) & 3]
//                         lrLeft  = corner[(liMatchedEdge + 2) & 3]
//               -- the far edge, traversed the other way round so left/right stay consistent.
// =================================================================================================
void RacingLineGenerator::GuessInwardsSectionPortal(SectionData* lpSectionData, Vector2& lrLeft,
                                                    Vector2& lrRight)
{
    const BoundaryLine* lpBoundaryLine = lpSectionData->mpTargetPortal->GetBoundaryLine(0);

    lrLeft  = Make2D(lpBoundaryLine->mfStartX, lpBoundaryLine->mfStartY);
    lrRight = Make2D(lpBoundaryLine->mfEndX,   lpBoundaryLine->mfEndY);

    const AISection* lpLineSection = lpSectionData->mpLineSection;

    s32 liMatchedEdge = -1;
    for (s32 liEdge = 0; liEdge < KI_AI_SECTION_EDGES; ++liEdge)
    {
        const s32 liPrevious = (liEdge + KI_AI_SECTION_EDGES - 1) % KI_AI_SECTION_EDGES;

        const Vector2 lCurrentCorner  = GetSectionCorner(lpLineSection, liEdge);
        const Vector2 lPreviousCorner = GetSectionCorner(lpLineSection, liPrevious);

        if (IsSimilar2D(lPreviousCorner.mV.x, lPreviousCorner.mV.y, lrLeft.mV.x, lrLeft.mV.y)
            && IsSimilar2D(lCurrentCorner.mV.x, lCurrentCorner.mV.y, lrRight.mV.x, lrRight.mV.y))
        {
            liMatchedEdge = liEdge;
            break;
        }
    }

    if (liMatchedEdge == -1)
    {
        FireStreamedTextAssert("Failed to match portal with section edge\n", 766);
    }

    lrRight = GetSectionCorner(lpLineSection, (liMatchedEdge + 1) & KI_AI_SECTION_EDGES_MASK);
    lrLeft  = GetSectionCorner(lpLineSection, (liMatchedEdge + 2) & KI_AI_SECTION_EDGES_MASK);
}

// =================================================================================================
// SetTargetUpToDate @0x82768710  (r3 = this, r4 = lpRacingLine, r5 = liSectionIndex)
//
//   0x8276871C  the inlined CacheUpToDate; on a miss the streamed
//               "Can't set target up to date in stale cache " << (index & 15)  (.cpp:2098)
//   0x827687BC  UNCONDITIONALLY `stb 1, 0xBA(entry)` == maSectionCache[i & 15].mbTargetUpToDate
// =================================================================================================
void RacingLineGenerator::SetTargetUpToDate(RacingLine* lpRacingLine, s32 liSectionIndex)
{
    const s32 liCacheIndex = liSectionIndex & KI_SECTION_CACHE_MASK;

    if (!CacheUpToDate(lpRacingLine, liSectionIndex))
    {
        FireSectionIndexAssert("Can't set target up to date in stale cache ", liCacheIndex, "", 2098);
    }

    lpRacingLine->maSectionCache[liCacheIndex].mbTargetUpToDate = true;
}

// =================================================================================================
// LookForStraightSectionAhead @0x82775C10   (r3 = this, r4 = lpRacingLine, r5 = liSectionIndex)
//
//   0x82775C28  assert liSectionIndex >= mFirstSectionInCache, streamed
//               "Section " << index << " not cached"  (.cpp:618)
//   0x82775CD0  walk li = liSectionIndex+1 while li < mLastSectionInCache (the bound is RE-READ
//               each iteration); the first section WITHOUT the junction bit ends the walk and
//               the function returns li - 1 (the section BEFORE it)
//   0x82775D14  ran out -> mLastSectionInCache
// =================================================================================================
s32 RacingLineGenerator::LookForStraightSectionAhead(RacingLine* lpRacingLine, s32 liSectionIndex)
{
    if (liSectionIndex < lpRacingLine->mFirstSectionInCache)
    {
        FireSectionIndexAssert("Section ", liSectionIndex, " not cached", 618);
    }

    s32 liSection = liSectionIndex + 1;
    while (liSection < lpRacingLine->mLastSectionInCache)
    {
        const SectionData* lpSectionData = GetSectionPointer(lpRacingLine, liSection);
        if ((lpSectionData->mpLineSection->mx8Flags & KX8_SECTION_FLAG_JUNCTION) == 0)
        {
            return liSection - 1;
        }
        ++liSection;
    }

    return lpRacingLine->mLastSectionInCache;
}

// =================================================================================================
// LookForStraightSectionBehind @0x82775D30  (r3 = this, r4 = lpRacingLine, r5 = liSectionIndex)
//
// The mirror, with ONE deliberate asymmetry the asm insists on: it returns liSection itself
// (@0x82775E40 `mr r3, r31`), not liSection + 1, where Ahead returns liSection - 1.
//   0x82775D48  assert liSectionIndex <= mLastSectionInCache, streamed
//               "Section " << index << " not cached"  (.cpp:658)
// =================================================================================================
s32 RacingLineGenerator::LookForStraightSectionBehind(RacingLine* lpRacingLine, s32 liSectionIndex)
{
    if (liSectionIndex > lpRacingLine->mLastSectionInCache)
    {
        FireSectionIndexAssert("Section ", liSectionIndex, " not cached", 658);
    }

    s32 liSection = liSectionIndex - 1;
    while (liSection > lpRacingLine->mFirstSectionInCache)
    {
        const SectionData* lpSectionData = GetSectionPointer(lpRacingLine, liSection);
        if ((lpSectionData->mpLineSection->mx8Flags & KX8_SECTION_FLAG_JUNCTION) == 0)
        {
            return liSection;
        }
        --liSection;
    }

    return lpRacingLine->mFirstSectionInCache;
}

// =================================================================================================
// GetSimpleHermite @0x827767F0
//   (r3 = this, r4 = &lrResult, v1 = A, v2 = B, v3 = lInVector, v4 = lOutVector, v5 = lInterp)
//
// The plain cubic hermite. The basis is built from `vspltisw`+`vcfsx` 1.0 / 2.0 constants at
// 0x82776B40..0x82776B90, and the four terms are accumulated with `vmaddfp128` (D = A*B + D):
//   h1 = 2t^3 - 3t^2 + 1        [sp+var_120]   -- weight of A
//   h2 = 1 - h1 = -2t^3 + 3t^2  [sp+var_C0]    -- weight of B
//   h3 = t^3 - 2t^2 + t         [sp+var_B0]    -- weight of lInVector
//   h4 = t^3 - t^2                             -- weight of lOutVector
// Five streamed NaN asserts on the inputs (.cpp:1806..1810) and one on the result (.cpp:1826);
// all six are emitted in the retail asm. lInterp is a VecFloat because the console broadcasts
// the parameter and evaluates all four lanes at once -- and the console STORES all four lanes
// of the result, so all four are computed here.
// =================================================================================================
void RacingLineGenerator::GetSimpleHermite(Vector2 lInVectorA, Vector2 lInVectorB,
                                           Vector2 lInVector, Vector2 lOutVector,
                                           VecFloat lInterp, Vector2& lrResult)
{
    if (!IsValid2D(lInVectorA))
    {
        FireStreamedTextAssert("Bad in vector A in simple\n", 1806);
    }
    if (!IsValid2D(lInVectorB))
    {
        FireStreamedTextAssert("Bad in vector B in simple\n", 1807);
    }
    if (!(lInterp.mV.x == lInterp.mV.x && lInterp.mV.y == lInterp.mV.y
          && lInterp.mV.z == lInterp.mV.z && lInterp.mV.w == lInterp.mV.w))
    {
        FireStreamedTextAssert("Bad in interp in simple\n", 1808);
    }
    if (!IsValid2D(lInVector))
    {
        FireStreamedTextAssert("Bad in vector in simple\n", 1809);
    }
    if (!IsValid2D(lOutVector))
    {
        FireStreamedTextAssert("Bad out vector in simple\n", 1810);
    }

    // ARTIST 82776B40..BAC: preserve the packed operations and their rounding.
    const __vector4 lOne = __vcfsx(__vspltisw(1), 0);
    const __vector4 lTwo = __vcfsx(__vspltisw(2), 0);
    const __vector4 lT2 = __vmulfp(lInterp.mV, lInterp.mV);
    const __vector4 lT3 = __vmulfp(lT2, lInterp.mV);
    const __vector4 lH3 = __vaddfp(__vsubfp(lT3, __vmulfp(lTwo, lT2)), lInterp.mV);
    const __vector4 lH4 = __vsubfp(lT3, lT2);
    const __vector4 lH1 = __vaddfp(__vsubfp(__vmulfp(lTwo, lT3),
                                             __vmulfp(__vaddfp(lOne, lTwo), lT2)), lOne);
    const __vector4 lH2 = __vsubfp(lOne, lH1);
    const f32 lfH1 = lH1.x;
    const f32 lfH2 = lH2.x;
    const f32 lfH3 = lH3.x;
    __vector4 lSum = __vmulfp(lInVectorB.mV, lH2);
    lSum = __vmaddfp(lInVectorA.mV, lH1, lSum);
    lSum = __vmaddfp(lInVector.mV, lH3, lSum);
    lrResult.mV = __vmaddfp(lOutVector.mV, lH4, lSum);

    if (!IsValid2D(lrResult))
    {
        // @0x82776BFC..0x82776D5C: "NAN rsult in simple hermite : h1=" << h1 << " h2=" << h2
        // << " h3=" << h3 << " In=" << (%f, %f) << " Out=" << (%f, %f) << "\n"  (.cpp:1826).
        // ("rsult" is the console's own spelling.)
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStream << "NAN rsult in simple hermite : h1=" << lfH1
                << " h2=" << lfH2 << " h3=" << lfH3 << " In=";
        lStream.AppendFormat("(%f, %f)", lInVector.mV.x, lInVector.mV.y);
        lStream << " Out=";
        lStream.AppendFormat("(%f, %f)", lOutVector.mV.x, lOutVector.mV.y);
        lStream << "\n";
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lacMessageBuffer, CGS_ASSERT_LOCATION(KPC_SOURCE_FILE_CPP, 1826));
        CgsDev::Assert::EndAssert();
    }
}

// =================================================================================================
// GetIterativeHermite @0x82776D70
//   (r3 = this, r4 = &lrResult, v1 = A, v2 = B, v3 = lInVector, v4 = lOutVector, f1 = lInterp)
//
// No IDA export; disassembled out of the image (the register fields decoded with the VMX128
// layout, since IDA swaps the two high bits of vA). Register map:
//   v124 = A, v122 = B, v120 = lInVector, v119 = lOutVector, f31 = lInterp, r26 = &lrResult.
//
// WHAT IT DOES. GetSimpleHermite's parameter is a curve parameter, not an arc-length or a chord
// fraction. This function inverts that: it finds the hermite parameter whose POINT projects onto
// `lInterp` along the straight chord A->B, by relaxation.
//   0x827770E0  lDelta   = B - A ; lfLengthSq = |lDelta.xy|^2
//   0x82777144  if (0.01 > lfLengthSq)            -> lrResult = A, return 0.0   (flt_82002138)
//   0x827771C8  if (!(|lfLengthSq| > FLT_EPSILON)) -> lrResult = A, return 0.0  (RwMath::IsZero)
//   0x827772B0  lVectorLine = lDelta / lfLengthSq   (vrefp + two Newton-Raphson steps)
//   0x827772B4  five iterations (`cmpwi r21, 5` @0x827776D4):
//                 GetSimpleHermite(A, B, in, out, lfBestGuess, lrResult)
//                 lPointLine  = lrResult - A
//                 lNewInterp  = Dot(lVectorLine, lPointLine)
//                 lError      = lNewInterp - lInterp        <- the ORIGINAL parameter, reloaded
//                                                              from [sp+0x394] every iteration
//                 lfBestGuess = clamp(lfBestGuess - lError*0.5, 0, 1)   (flt_820C47FC == 0.5)
//   0x827776DC  if (lfBestGuess is NaN)           -> lrResult = A, return 0.0
//   0x827776F4  one last GetSimpleHermite at lfBestGuess, return lfBestGuess
// Two streamed asserts inside the loop (.cpp:1899 / :1906) name every local; five input NaN
// asserts precede the loop (.cpp:1847..1851).
// =================================================================================================
f32 RacingLineGenerator::GetIterativeHermite(Vector2 lInVectorA, Vector2 lInVectorB,
                                             Vector2 lInVector, Vector2 lOutVector,
                                             f32 lInterp, Vector2& lrResult)
{
    if (!IsValid2D(lInVectorA))
    {
        FireStreamedTextAssert("Bad in vector A\n", 1847);
    }
    if (!IsValid2D(lInVectorB))
    {
        FireStreamedTextAssert("Bad in vector B\n", 1848);
    }
    if (!(lInterp == lInterp))
    {
        FireStreamedTextAssert("Bad in interp\n", 1849);
    }
    if (!IsValid2D(lInVector))
    {
        FireStreamedTextAssert("Bad in vector\n", 1850);
    }
    if (!IsValid2D(lOutVector))
    {
        FireStreamedTextAssert("Bad out vector\n", 1851);
    }

    const f32 lfDeltaX  = lInVectorB.mV.x - lInVectorA.mV.x;
    const f32 lfDeltaY  = lInVectorB.mV.y - lInVectorA.mV.y;
    const f32 lfLengthSq = (lfDeltaX * lfDeltaX) + (lfDeltaY * lfDeltaY);

    if (KF_ITERATIVE_HERMITE_MIN_CHORD_SQ > lfLengthSq)
    {
        lrResult = lInVectorA;
        return 0.0f;
    }
    if (!(fabsf(lfLengthSq) > KF_ZERO_EPSILON))
    {
        lrResult = lInVectorA;
        return 0.0f;
    }

    const Vector2 lVectorLine = Make2D(lfDeltaX / lfLengthSq, lfDeltaY / lfLengthSq);

    f32 lfBestGuess = lInterp;
    for (s32 liIterate = 0; liIterate < KI_ITERATIVE_HERMITE_ITERATIONS; ++liIterate)
    {
        const VecFloat lGuessVector(lfBestGuess);
        GetSimpleHermite(lInVectorA, lInVectorB, lInVector, lOutVector, lGuessVector, lrResult);

        const Vector2 lPointLine = Make2D(lrResult.mV.x - lInVectorA.mV.x, lrResult.mV.y - lInVectorA.mV.y);
        const f32 lfNewInterp = (lVectorLine.mV.x * lPointLine.mV.x) + (lVectorLine.mV.y * lPointLine.mV.y);
        const f32 lfError     = lfNewInterp - lInterp;

        if (!(lfError == lfError))
        {
            // @0x82777338..0x8277749C (.cpp:1899).
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStream << "'Error' is a NAN in iterative hermite lfBestGuess=" << lfBestGuess
                    << " liIterate=" << liIterate << " lVectorLine=";
            lStream.AppendFormat("(%f, %f)", lVectorLine.mV.x, lVectorLine.mV.y);
            lStream << " lPointLine=";
            lStream.AppendFormat("(%f, %f)", lPointLine.mV.x, lPointLine.mV.y);
            lStream << " lNewInterp=" << lfNewInterp << " lInterp=" << lInterp << "\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessageBuffer, CGS_ASSERT_LOCATION(KPC_SOURCE_FILE_CPP, 1899));
            CgsDev::Assert::EndAssert();
        }

        lfBestGuess -= lfError * KF_ITERATIVE_HERMITE_STEP;
        if (lfBestGuess < 0.0f)
        {
            lfBestGuess = 0.0f;      // vmaxfp128 against the vspltisw128 zero
        }
        if (lfBestGuess > 1.0f)
        {
            lfBestGuess = 1.0f;      // vminfp128 against the vcsxwfp128 one
        }

        if (!(lfBestGuess == lfBestGuess))
        {
            // @0x827774D4..0x827776C8 (.cpp:1906).
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStream << "NAN in iterative hermite :Iteration" << liIterate
                    << " lError " << lfError << " vertA=";
            lStream.AppendFormat("(%f, %f)", lInVectorA.mV.x, lInVectorA.mV.y);
            lStream << " vertB=";
            lStream.AppendFormat("(%f, %f)", lInVectorB.mV.x, lInVectorB.mV.y);
            lStream << " In=";
            lStream.AppendFormat("(%f, %f)", lInVector.mV.x, lInVector.mV.y);
            lStream << " Out=";
            lStream.AppendFormat("(%f, %f)", lOutVector.mV.x, lOutVector.mV.y);
            lStream << " Result=";
            lStream.AppendFormat("(%f, %f)", lrResult.mV.x, lrResult.mV.y);
            lStream << "\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessageBuffer, CGS_ASSERT_LOCATION(KPC_SOURCE_FILE_CPP, 1906));
            CgsDev::Assert::EndAssert();
        }
    }

    if (!(lfBestGuess == lfBestGuess))
    {
        lrResult = lInVectorA;
        return 0.0f;
    }

    const VecFloat lFinalVector(lfBestGuess);
    GetSimpleHermite(lInVectorA, lInVectorB, lInVector, lOutVector, lFinalVector, lrResult);
    return lfBestGuess;
}

// =================================================================================================
// GetSectionInterpPosition @0x827815F8  (r3 = this, r4 = lpRacingLine, r5 = liSectionIndex,
//                                        v1 = lPosition)
//
//   0x82781638  v12 = section->mPortalEntranceAndExit ; v123-equivalent = `vpermwi128 .., 0xBF`
//               (word select {2,3,3,3}) == the EXIT point in the low lanes
//   0x82781658  lAxis = exit - entrance
//   0x82781688  RwMath::IsZero(lAxis) -> return 0.0f  (flt_82001CC0)
//   0x827816FC  Dot(lAxis, lPosition - entrance) / Dot(lAxis, lAxis)   (vrefp + two Newton steps)
//   0x82781774  streamed "Bad interp\n" if the result is NaN (.cpp:2616), then returns it anyway
// =================================================================================================
f32 RacingLineGenerator::GetSectionInterpPosition(RacingLine* lpRacingLine, s32 liSectionIndex,
                                                  Vector2 lPosition)
{
    const SectionData* lpSectionData = GetSectionPointer(lpRacingLine, liSectionIndex);

    const Vector2 lEntrance = lpSectionData->GetSectionEntrance();
    const Vector2 lExit     = lpSectionData->GetSectionExit();

    // ARTIST 82781640..8174C: packed deltas, dot products and reciprocal refinement.
    const __vector4 lAxis = __vsubfp(lExit.mV, lEntrance.mV);
    if (IsZero2D(lAxis.x, lAxis.y))
        return 0.0f;
    const __vector4 lFromEntrance = __vsubfp(lPosition.mV, lEntrance.mV);
    const __vector4 lAxisSquare = __vmulfp(lAxis, lAxis);
    const __vector4 lAxisLength = __vaddfp(__vspltw(lAxisSquare, 0), __vspltw(lAxisSquare, 1));
    const __vector4 lProduct = __vmulfp(lAxis, lFromEntrance);
    const __vector4 lProjection = __vaddfp(__vspltw(lProduct, 0), __vspltw(lProduct, 1));
    const __vector4 lOne = __vcfsx(__vspltisw(1), 0);
    __vector4 lReciprocal = __vrefp(lAxisLength);
    lReciprocal = __vmaddfp(lReciprocal, __vnmsubfp(lReciprocal, lAxisLength, lOne), lReciprocal);
    lReciprocal = __vmaddfp(lReciprocal, __vnmsubfp(lReciprocal, lAxisLength, lOne), lReciprocal);
    const f32 lfInterp = __vmulfp(lReciprocal, lProjection).x;

    if (!(lfInterp == lfInterp))
    {
        FireStreamedTextAssert("Bad interp\n", 2616);
    }

    return lfInterp;
}

// =================================================================================================
// GetHalfRoadWidthHere @0x8278EC08  (r3 = this, r4 = lpRacingLine, v1 = lPosition)
//
//   0x8278EC2C  GetLocalSectionID(lpRacingLine, lPosition, lpRacingLine->miLastKnownSectionID)
//   0x8278EC38  not found                              -> 10.0f  (flt_820C4150)
//   0x8278EC40  liSection - 1 < mFirstSectionInCache   -> 10.0f  (the previous section must be
//                                                        cached, because the map spans both)
//   0x8278EC5C  lpRacingLine->miLastKnownSectionID = liSection
//   0x8278EC64  lfInterp = GetSectionInterpPosition(...)
//   0x8278EC84  lfWidth  = section->mHardNoGoMap.GetEstimatedRoadWidth(lfInterp)
//   0x8278EC9C  lfHalf   = lfWidth * 0.5 - 1.5         (fmsubs; flt_820C4168 / flt_82004D04)
//   0x8278ECA4  return max(lfHalf, 3.0f)               (fcmpu + blt; flt_820C4154)
// =================================================================================================
f32 RacingLineGenerator::GetHalfRoadWidthHere(RacingLine* lpRacingLine, Vector2 lPosition)
{
    const s32 liSection = GetLocalSectionID(lpRacingLine, lPosition,
                                            lpRacingLine->miLastKnownSectionID);

    if (liSection == KI_INVALID_SECTION_INDEX)
    {
        return KF_HALF_ROAD_WIDTH_FALLBACK;
    }
    if ((liSection - 1) < lpRacingLine->mFirstSectionInCache)
    {
        return KF_HALF_ROAD_WIDTH_FALLBACK;
    }

    lpRacingLine->miLastKnownSectionID = liSection;

    const f32 lfInterp = GetSectionInterpPosition(lpRacingLine, liSection, lPosition);
    SectionData* lpSectionData = GetSectionPointer(lpRacingLine, liSection);
    const f32 lfRoadWidth = lpSectionData->mHardNoGoMap.GetEstimatedRoadWidth(lfInterp);

    const f32 lfHalfWidth = (lfRoadWidth * 0.5f) - KF_HALF_ROAD_WIDTH_INSET;
    if (lfHalfWidth < KF_HALF_ROAD_WIDTH_MIN)
    {
        return KF_HALF_ROAD_WIDTH_MIN;
    }
    return lfHalfWidth;
}

// =================================================================================================
// GetRouteCentre @0x8278F408  (r3 = this, r4 = lpRacingLine, r5 = liSectionIndex, r6 = &lrCentre)
//
//   0x8278F41C  lpSectionData = GetSectionPointer(...)   (r3/r4/r5 pass straight through)
//   0x8278F420  both edge vectors are zeroed first
//   0x8278F430  `lbz 0x98(section)` == mHardNoGoMap.mbReady -- on false the streamed
//               "HNG Not set up in " << index << "\n"  (.cpp:808), then it CONTINUES
//   0x8278F4F4  FindMaximalEdges(lLeftEdge, lRightEdge, 1.0f)     (flt_82001C98)
//   0x8278F510  *lrCentre = (lLeftEdge + lRightEdge) * 0.5        (flt_820C4168), all four lanes
// =================================================================================================
void RacingLineGenerator::GetRouteCentre(RacingLine* lpRacingLine, s32 liSectionIndex,
                                         Vector2& lrCentre)
{
    SectionData* lpSectionData = GetSectionPointer(lpRacingLine, liSectionIndex);

    Vector2 lLeftEdge;
    Vector2 lRightEdge;
    lLeftEdge.SetZero();
    lRightEdge.SetZero();

    if (!lpSectionData->mHardNoGoMap.IsReady())
    {
        FireSectionIndexAssert("HNG Not set up in ", liSectionIndex, "\n", 808);
    }

    lpSectionData->mHardNoGoMap.FindMaximalEdges(lLeftEdge, lRightEdge, 1.0f);

    // ARTIST 8278F510/534: packed add followed by packed multiply.
    const __vector4 lHalf = __vcfsx(__vspltisw(1), 1);
    lrCentre.mV = __vmulfp(__vaddfp(lLeftEdge.mV, lRightEdge.mV), lHalf);
}

// =================================================================================================
// CalculateIntersectionWithProjectedRoute @0x8278F188
//   (r3 = this, r4 = lpRacingLine, r5 = liNodeIndex, v1 = lFrom, v2 = lTo, f1 = UNUSED)
//
// The DWARF's fifth parameter is a float32_t the body never reads: f1 is overwritten with
// flt_82001C98 (1.0) at 0x8278F28C before the first use. Kept in the signature, marked unused.
//
//   0x8278F1C0  the inlined CacheUpToDate; on a miss the streamed
//               "Node Index " << index << " isn't cached!"  (.cpp:474) -- then it CONTINUES
//   0x8278F2A4  entry.mCachedSectionIndex = liNodeIndex ; entry.mbTargetUpToDate = false
//   0x8278F2B0  entry.mHardNoGoMap.FindMaximalEdges(lLeftEdge, lRightEdge, 1.0f)
//   0x8278F2D4  Calc2DIntersectionEquationData(lFrom, lTo, lLeftEdge, lRightEdge, &a, &b)
//               (the two out params take r3/r4 because the four Vector2s take v1..v4)
//   0x8278F2E4  no intersection, or a outside [0,1]  -> the edge MIDPOINT
//   0x8278F30C  b < 0                                -> 5% in from the left edge  (unk_820C7D38)
//   0x8278F334  b > 1                                -> 5% in from the right edge (unk_820C7D34)
//   0x8278F350  otherwise                            -> lFrom + (lTo - lFrom) * a
//   0x8278F3D4  the result's x/y are written into the section's EXIT lanes (`vrlimi128 .., 2, 2`
//               then `.., 1, 2`) == SectionData::SetSectionExit.
// =================================================================================================
void RacingLineGenerator::CalculateIntersectionWithProjectedRoute(RacingLine* lpRacingLine,
                                                                  s32 liNodeIndex,
                                                                  Vector2 lFrom, Vector2 lTo,
                                                                  f32 lfUnused)
{
    (void)lfUnused;   // f1 is overwritten with 1.0 at 0x8278F28C before any read

    const s32 liCacheIndex = liNodeIndex & KI_SECTION_CACHE_MASK;

    if (!CacheUpToDate(lpRacingLine, liNodeIndex))
    {
        FireSectionIndexAssert("Node Index ", liNodeIndex, " isn't cached!", 474);
    }

    SectionData& lrEntry = lpRacingLine->maSectionCache[liCacheIndex];

    Vector2 lLeftEdge;
    Vector2 lRightEdge;
    lLeftEdge.SetZero();
    lRightEdge.SetZero();

    lrEntry.mCachedSectionIndex = static_cast<s16>(liNodeIndex);
    lrEntry.mbTargetUpToDate    = false;

    lrEntry.mHardNoGoMap.FindMaximalEdges(lLeftEdge, lRightEdge, 1.0f);

    f32 lfRouteParam = 0.0f;
    f32 lfEdgeParam  = 0.0f;
    const bool lbIntersects = Calc2DIntersectionEquationData(lFrom, lTo, lLeftEdge, lRightEdge,
                                                             &lfRouteParam, &lfEdgeParam);

    Vector2 lIntersection;
    if (!lbIntersects || lfRouteParam < 0.0f || lfRouteParam > 1.0f)
    {
        const __vector4 lHalf = __vcfsx(__vspltisw(1), 1);
        lIntersection.mV = __vmulfp(__vaddfp(lLeftEdge.mV, lRightEdge.mV), lHalf);
    }
    else if (lfEdgeParam < 0.0f)
    {
        __vector4 lInset;
        lInset.x = KF_ROUTE_EDGE_INSET;
        lIntersection.mV = __vmaddfp(__vsubfp(lRightEdge.mV, lLeftEdge.mV),
                                      __vspltw(lInset, 0), lLeftEdge.mV);
    }
    else if (lfEdgeParam > 1.0f)
    {
        __vector4 lInset;
        lInset.x = KF_ROUTE_EDGE_INSET;
        lIntersection.mV = __vmaddfp(__vsubfp(lLeftEdge.mV, lRightEdge.mV),
                                      __vspltw(lInset, 0), lRightEdge.mV);
    }
    else
    {
        __vector4 lParam;
        lParam.x = lfRouteParam;
        // VMX128 vmaddcfp at 8278F374; XDK xboxmath.h maps it to __vmaddfp.
        lIntersection.mV = __vmaddfp(__vsubfp(lTo.mV, lFrom.mV),
                                      __vspltw(lParam, 0), lFrom.mV);
    }

    lrEntry.SetSectionExit(lIntersection);
}

// =================================================================================================
// GenerateInOutVectors @0x82780638
//   (r3 = this, r4 = lpRacingLine, r5 = liSectionIndex, r6 = &lrInVector, r7 = &lrOutVector)
//
// The two hermite tangents of the section's centre line. Both are the step to the NEIGHBOUR's
// portal (falling back to this section's own entrance->exit chord), normalised and scaled by
// min(|entrance - exit|, 16.0).
//   0x82780688  lEntrance / lExit out of mPortalEntranceAndExit (`vpermwi128 .., 0xBF`)
//   0x827806A8  IsSimilar(entrance, exit) (2^-16) -> BOTH outputs zeroed, return
//   0x82780710  lfScale = min(|entrance - exit|, 16.0)          (flt_82004000)
//   0x827807AC  IN : previous section cached AND its entrance differs -> entrance - prevEntrance
//                    otherwise                                        -> exit - entrance
//   0x82780814  the streamed "!RwMath::IsZero(lInVector)" assert (.cpp:1984), then normalise*scale
//   0x82780964  OUT: next section cached AND its exit differs         -> nextExit - exit
//                    otherwise                                        -> exit - entrance
//   0x82780A6C  the streamed "!RwMath::IsZero(lOutVector)" assert (.cpp:2010), then normalise*scale
// The console does the subtractions four-lane, so its z/w lanes carry the exit pair's own
// difference; nothing reads them, so they are published as zero here.
// =================================================================================================
void RacingLineGenerator::GenerateInOutVectors(RacingLine* lpRacingLine, s32 liSectionIndex,
                                               Vector2& lrInVector, Vector2& lrOutVector)
{
    const SectionData* lpSectionData = GetSectionPointer(lpRacingLine, liSectionIndex);

    const Vector2 lEntrance = lpSectionData->GetSectionEntrance();
    const Vector2 lExit     = lpSectionData->GetSectionExit();

    if (IsSimilar2D(lEntrance.mV.x, lEntrance.mV.y, lExit.mV.x, lExit.mV.y))
    {
        lrInVector.SetZero();
        lrOutVector.SetZero();
        return;
    }

    const f32 lfChordLength = Magnitude2D(lEntrance.mV.x - lExit.mV.x, lEntrance.mV.y - lExit.mV.y);
    const f32 lfScale = (lfChordLength > KF_IN_OUT_VECTOR_MAX_LENGTH)
                            ? KF_IN_OUT_VECTOR_MAX_LENGTH
                            : lfChordLength;

    // ---- the incoming tangent -----------------------------------------------------------
    if (CacheUpToDate(lpRacingLine, liSectionIndex - 1))
    {
        const SectionData* lpPrevious = GetSectionPointer(lpRacingLine, liSectionIndex - 1);
        const Vector2 lPreviousEntrance = lpPrevious->GetSectionEntrance();

        if (IsSimilar2D(lEntrance.mV.x, lEntrance.mV.y, lPreviousEntrance.mV.x, lPreviousEntrance.mV.y))
        {
            lrInVector = Make2D(lExit.mV.x - lEntrance.mV.x, lExit.mV.y - lEntrance.mV.y);
        }
        else
        {
            lrInVector = Make2D(lEntrance.mV.x - lPreviousEntrance.mV.x,
                                lEntrance.mV.y - lPreviousEntrance.mV.y);
        }
    }
    else
    {
        lrInVector = Make2D(lExit.mV.x - lEntrance.mV.x, lExit.mV.y - lEntrance.mV.y);
    }

    if (IsZero2D(lrInVector.mV.x, lrInVector.mV.y))
    {
        FireStreamedTextAssert("!RwMath::IsZero(lInVector)", 1984);
    }

    {
        const Vector2 lNormalised = Normalise2D(lrInVector.mV.x, lrInVector.mV.y);
        lrInVector = Make2D(lNormalised.mV.x * lfScale, lNormalised.mV.y * lfScale);
    }

    // ---- the outgoing tangent ------------------------------------------------------------
    if (CacheUpToDate(lpRacingLine, liSectionIndex + 1))
    {
        const SectionData* lpNext = GetSectionPointer(lpRacingLine, liSectionIndex + 1);
        const Vector2 lNextExit = lpNext->GetSectionExit();

        if (IsSimilar2D(lExit.mV.x, lExit.mV.y, lNextExit.mV.x, lNextExit.mV.y))
        {
            lrOutVector = Make2D(lExit.mV.x - lEntrance.mV.x, lExit.mV.y - lEntrance.mV.y);
        }
        else
        {
            lrOutVector = Make2D(lNextExit.mV.x - lExit.mV.x, lNextExit.mV.y - lExit.mV.y);
        }
    }
    else
    {
        lrOutVector = Make2D(lExit.mV.x - lEntrance.mV.x, lExit.mV.y - lEntrance.mV.y);
    }

    if (IsZero2D(lrOutVector.mV.x, lrOutVector.mV.y))
    {
        FireStreamedTextAssert("!RwMath::IsZero(lOutVector)", 2010);
    }

    {
        const Vector2 lNormalised = Normalise2D(lrOutVector.mV.x, lrOutVector.mV.y);
        lrOutVector = Make2D(lNormalised.mV.x * lfScale, lNormalised.mV.y * lfScale);
    }
}

// =================================================================================================
// SetUpHardNoGoMap @0x82780518  (r3 = this, r4 = lpRacingLine, r5 = liSectionIndex)
//
//   0x8278052C  the inlined CacheUpToDate -- a stale slot returns immediately (a real GUARD here,
//               not a report: the asm's `bne cr6, loc_82780630` skips the whole body)
//   0x82780554  the map already built for this section (`lwz 0x90(section)` ==
//               mHardNoGoMap.miSectionIndex) -> return
//   0x82780574  HardNoGoMap::Prepare(liSectionIndex)   (inlined: `stw index, 0x40` ; `stb 0, 0x48`)
//   0x8278057C  GetSectionPortal(this section)                        -> the CURRENT pair
//   0x82780588  the PREVIOUS section, when cached      -> GetSectionPortal(previous)
//               otherwise                              -> GuessInwardsSectionPortal(this section)
//   0x82780604  HardNoGoMap::SetCorners(current left, current right, previous left, previous right)
//   0x82780608  HardNoGoMap::ClearMap() (inlined: eight rows of 0x80000001, then `stb 1, 0x48`)
// =================================================================================================
void RacingLineGenerator::SetUpHardNoGoMap(RacingLine* lpRacingLine, s32 liSectionIndex)
{
    if (!CacheUpToDate(lpRacingLine, liSectionIndex))
    {
        return;
    }

    SectionData* lpSectionData = GetSectionPointer(lpRacingLine, liSectionIndex);
    if (lpSectionData->mHardNoGoMap.GetSectionIndex() == liSectionIndex)
    {
        return;
    }

    lpSectionData->mHardNoGoMap.Prepare(liSectionIndex);

    Vector2 lCurrentLeft;
    Vector2 lCurrentRight;
    GetSectionPortal(lpSectionData, lCurrentLeft, lCurrentRight);

    Vector2 lPreviousLeft;
    Vector2 lPreviousRight;
    if (CacheUpToDate(lpRacingLine, liSectionIndex - 1))
    {
        SectionData* lpPrevious = GetSectionPointer(lpRacingLine, liSectionIndex - 1);
        GetSectionPortal(lpPrevious, lPreviousLeft, lPreviousRight);
    }
    else
    {
        SectionData* lpThisSection = GetSectionPointer(lpRacingLine, liSectionIndex);
        GuessInwardsSectionPortal(lpThisSection, lPreviousLeft, lPreviousRight);
    }

    lpSectionData->mHardNoGoMap.SetCorners(lCurrentLeft, lCurrentRight,
                                           lPreviousLeft, lPreviousRight);
    lpSectionData->mHardNoGoMap.ClearMap();
}

// =================================================================================================
// GetCentreCentreLineHere @0x8278E930
//   (r3 = this, r4 = lpRacingLine, v1 = lCarPos2D, v2 = lTargetPos2D,
//    r5 = &lrCentreHere, r6 = &lrCentreAhead)
//
//   0x8278E964  CGS_ASSERT(lpRacingLine->IsInitialised())  (.cpp:126, a PLAIN FireAssert)
//   0x8278E990  liSection = GetLocalSectionID(rl, lCarPos2D, rl->miLastKnownSectionID)
//   0x8278E9AC  not found -> search the WHOLE cache window; otherwise clamp the window to
//               [liSection-1, liSection+1] and latch miLastKnownSectionID
//   0x8278EA28  for each section in the window:
//                 lfInterp = max(GetSectionInterpPosition(rl, section, lTargetPos2D), 0)
//                 skip the section when IsSimilar(entrance, exit)
//                 GenerateInOutVectors -> lIn / lOut
//                 lfCurveInterp = GetIterativeHermite(entrance, exit, lIn, lOut,
//                                                     min(lfInterp, 1.0), lPointOnCurve)
//                 lfDistance = |lPointOnCurve - lTargetPos2D|
//                 keep the closest: lrCentreHere = lPointOnCurve, then
//                   lfAhead = lfCurveInterp + 0.1        (flt_820C424C)
//                   if (lfAhead > 1.0) { lfAhead = lfCurveInterp - 0.1 ; mirror the result }
//                   GetSimpleHermite(..., lfAhead, lrCentreAhead)
//                   mirrored: lrCentreAhead = here - (ahead - here)
//   0x8278EBD0  nothing found (the best distance is still +FLT_MAX, flt_8204F664) ->
//               both outputs collapse to lTargetPos2D and it returns false.
// =================================================================================================
bool RacingLineGenerator::GetCentreCentreLineHere(RacingLine* lpRacingLine, Vector2 lCarPos2D,
                                                  Vector2 lTargetPos2D, Vector2& lrCentreHere,
                                                  Vector2& lrCentreAhead)
{
    CGS_ASSERT(lpRacingLine->IsInitialised(), CGS_ASSERT_MESSAGE_AT("lpRacingLine->IsInitialised()", __FILE__, 126));   // .cpp:126

    const s32 liLocalSection = GetLocalSectionID(lpRacingLine, lCarPos2D,
                                                 lpRacingLine->miLastKnownSectionID);

    s32 liFirstSection;
    s32 liLastSection;
    if (liLocalSection == KI_INVALID_SECTION_INDEX)
    {
        liFirstSection = lpRacingLine->mFirstSectionInCache;
        liLastSection  = lpRacingLine->mLastSectionInCache;
    }
    else
    {
        liFirstSection = liLocalSection - 1;
        if (liFirstSection <= lpRacingLine->mFirstSectionInCache)
        {
            liFirstSection = lpRacingLine->mFirstSectionInCache;
        }
        liLastSection = liLocalSection + 1;
        if (liLastSection >= lpRacingLine->mLastSectionInCache)
        {
            liLastSection = lpRacingLine->mLastSectionInCache;
        }
        lpRacingLine->miLastKnownSectionID = liLocalSection;
    }

    f32 lfBestDistance = FLT_MAX;   // flt_8204F664

    for (s32 liSection = liFirstSection; liSection <= liLastSection; ++liSection)
    {
        // ⭐⭐⭐ THE TWO CLAMPS ROUND THIS CALL ARE AN `fsel` PAIR AND THE SECOND ONE'S NaN ARM
        // IS THE CONSOLE'S NaN CONTAINMENT FOR THE WHOLE HERMITE STACK (fixed 2026-09-15,
        // issue #24). ARTIST 0x8278EA38..0x8278EAD8:
        //     bl    GetSectionInterpPosition
        //     fneg  f0, f1                 ; -interp
        //     fsel  f31, f0, f28(0.0), f1  ; (-interp >= 0) ? 0.0 : interp
        //     ... GenerateInOutVectors ...
        //     fsubs f0, f30(1.0), f31      ; 1.0 - interp
        //     fsel  f1, f0, f31, f30(1.0)  ; (1.0 - interp >= 0) ? interp : 1.0
        //     bl    GetIterativeHermite
        // `fsel FRT,FRA,FRC,FRB` is "if (FRA) >= 0.0 then FRC else FRB" and that compare is FALSE
        // for a NaN FRA, so a NaN interp SURVIVES the first fsel (its FRB is interp) and is turned
        // into **1.0** by the second (its FRB is the 1.0 literal).
        // ⛔ The previous `if (x <= 0) x = 0;` / `(x > 1.0f) ? 1.0f : x` form is not that program:
        // in C++ both comparisons are false for a NaN, so the NaN went straight into
        // GetIterativeHermite. THIS is the site the measured storm comes through -- issue #24 runs
        // 1-4, byte-identical every time: 2,416 x "Bad interp" at GetSectionInterpPosition
        // (.cpp:2616) feeding 2,416 x "Bad in interp" inside GetIterativeHermite (.cpp:1849), then
        // 12,080 each of :1808 / :1826 / :1899 / :1906 and 302 x "NAN error in
        // AIDriver::FindSignedAngleBetween2DVectors" from SteeringFan::UpdateWeightings -- 53,454
        // asserts in one 275 s run, which starves the sim being measured.
        // Written as explicit `>= 0.0f` tests so each line IS its fsel (a NaN comparison is false
        // in C++ too, so the ternary's own else-arm reproduces FRB).
        const f32 lfRawInterp = GetSectionInterpPosition(lpRacingLine, liSection, lTargetPos2D);
        f32 lfInterp = (-lfRawInterp >= 0.0f) ? 0.0f : lfRawInterp;   // fsel f31, f0, f28, f1

        const SectionData* lpSectionData = GetSectionPointer(lpRacingLine, liSection);
        const Vector2 lEntrance = lpSectionData->GetSectionEntrance();
        const Vector2 lExit     = lpSectionData->GetSectionExit();

        if (IsSimilar2D(lEntrance.mV.x, lEntrance.mV.y, lExit.mV.x, lExit.mV.y))
        {
            continue;
        }

        Vector2 lInVector;
        Vector2 lOutVector;
        GenerateInOutVectors(lpRacingLine, liSection, lInVector, lOutVector);

        // fsel f1, f0, f31, f30 @0x8278EAD4 -- FRA is (1.0 - interp), so a NaN lands on FRB == 1.0.
        const f32 lfClampedInterp = ((1.0f - lfInterp) >= 0.0f) ? lfInterp : 1.0f;

        Vector2 lPointOnCurve;
        const f32 lfCurveInterp = GetIterativeHermite(lEntrance, lExit, lInVector, lOutVector,
                                                      lfClampedInterp, lPointOnCurve);

        const f32 lfDistance = Magnitude2D(lPointOnCurve.mV.x - lTargetPos2D.mV.x,
                                           lPointOnCurve.mV.y - lTargetPos2D.mV.y);
        if (lfDistance > lfBestDistance)
        {
            continue;
        }

        lfBestDistance = lfDistance;
        lrCentreHere   = lPointOnCurve;

        f32  lfAheadInterp = lfCurveInterp + KF_CENTRE_LINE_AHEAD_STEP;
        bool lbMirrorAhead = false;
        if (lfAheadInterp > 1.0f)
        {
            lfAheadInterp = lfCurveInterp - KF_CENTRE_LINE_AHEAD_STEP;
            lbMirrorAhead = true;
        }

        const VecFloat lAheadVector(lfAheadInterp);
        GetSimpleHermite(lEntrance, lExit, lInVector, lOutVector, lAheadVector, lrCentreAhead);

        if (lbMirrorAhead)
        {
            // 0x8278EBB4: ahead = here - (ahead - here) -- reflect the backwards sample
            // through the "here" point so the direction still leads forwards.
            lrCentreAhead.mV = __vsubfp(lrCentreHere.mV,
                                         __vsubfp(lrCentreAhead.mV, lrCentreHere.mV));
        }
    }

    if (lfBestDistance != FLT_MAX)
    {
        return true;
    }

    lrCentreHere  = lTargetPos2D;
    lrCentreAhead = lTargetPos2D;
    return false;
}

// =================================================================================================
// CacheLocalSections @0x8278ECE8
//   (r3 = this, r4 = lpRacingLine, r5 = lpRoute, r6 = lpAISectionsData, r7 = lpCar)
//
//   0x8278ED4C  for every slot in [mFirstSectionInCache, mLastSectionInCache]:
//                 CGS_ASSERT(index < lpRoute->GetNodeCount())  "Route node count out of range"
//                                                              (.cpp:341, a plain FireAssert)
//                 already cached      -> nothing
//                 index < 0           -> remember that extrapolation is needed
//                 otherwise           -> refill the slot from the route node:
//                     entry.mCachedSectionIndex = index ; entry.mbTargetUpToDate = false
//                     entry.mpLineSection  = AISectionsData::GetAISection(node.muSectionIndex)
//                     entry.mpTargetPortal = section->GetPortal(node.muPortalIndex)
//                     entry.SetFastSectionCorners(section)
//   0x8278EE9C  no negative slot was stale -> return mFirstSectionInCache
//   0x8278EEA8  otherwise: the car must know a section (AICar::GetBestSectionIndex, the
//               muBestSectionIndex / muDefaultSectionIndex fallback pair) or the whole call
//               returns 0
//   0x8278EEDC  liNumSectionsToGenerate = -mFirstSectionInCache, asserted <= 16 (.cpp:396)
//   0x8278EF0C  the extrapolation direction: with >1 route node it is
//               Normalise(node[1] - node[0]); with 0 or 1 it is Flatten(car direction)
//   0x8278EFEC  ExtrapolateRouteBackwards(count, bestSection, direction, Flatten(car position),
//                                         sections, generated)
//   0x8278F048  the generated (section, portal) pairs fill slots -1, -2, ... the same way
//   0x8278F160  returns the last filled slot + 1 == the new first valid section index
// =================================================================================================
s32 RacingLineGenerator::CacheLocalSections(RacingLine* lpRacingLine, const Route* lpRoute,
                                            AISectionsData* lpAISectionsData, AICar* lpCar)
{
    bool lbNeedsExtrapolation = false;

    for (s32 liSection = lpRacingLine->mFirstSectionInCache;
         liSection <= lpRacingLine->mLastSectionInCache;
         ++liSection)
    {
        CGS_ASSERT(liSection < lpRoute->GetNodeCount(), CGS_ASSERT_MESSAGE_AT("Route node count out of range", __FILE__, 341));  // :341

        if (CacheUpToDate(lpRacingLine, liSection))
        {
            continue;
        }
        if (liSection < 0)
        {
            lbNeedsExtrapolation = true;
            continue;
        }

        const RouteNode* lpNode = lpRoute->GetNode(liSection);
        SectionData& lrEntry = lpRacingLine->maSectionCache[liSection & KI_SECTION_CACHE_MASK];

        lrEntry.mCachedSectionIndex = static_cast<s16>(liSection);
        lrEntry.mbTargetUpToDate    = false;

        const AISection* lpSection = lpAISectionsData->GetAISection(lpNode->muSectionIndex);
        lrEntry.mpLineSection  = lpSection;
        // ARTIST @0x8278EDFC reads the original portal byte directly.
        lrEntry.mpTargetPortal = lpSection->GetPortal(lpNode->muPortalIndex);
        lrEntry.SetFastSectionCorners(lpSection);
    }

    if (!lbNeedsExtrapolation)
    {
        return lpRacingLine->mFirstSectionInCache;
    }

    if (lpCar->GetBestSectionIndex() == KI_INVALID_SECTION_INDEX)
    {
        return 0;
    }

    const s32 liNumSectionsToGenerate = -lpRacingLine->mFirstSectionInCache;

    ExtrapolatedIndexArray lauGeneratedIndices;
    lauGeneratedIndices.SetFullCount();   // `li r10, 0x10 ; stw r10, +0x80` @0x8278EEE8

    CGS_ASSERT(liNumSectionsToGenerate <= KI_MAX_PLAYER_ROUTE_EXTRAPOLATION_GENERATED_SECTIONS,
               "liNumSectionsToGenerate <= RouteMapModuleIO::"
               "KI_MAX_PLAYER_ROUTE_EXTRAPOLATION_GENERATED_SECTIONS");            // :396

    Vector2 lCarDirection;
    if (lpRoute->GetNodeCount() > 1)
    {
        const RouteNode* lpFirstNode  = lpRoute->GetNode(0);
        const f32 lfFirstX = lpFirstNode->GetX();
        const f32 lfFirstY = lpFirstNode->GetY();

        const RouteNode* lpSecondNode = lpRoute->GetNode(1);
        lCarDirection = Normalise2D(lpSecondNode->GetX() - lfFirstX,
                                    lpSecondNode->GetY() - lfFirstY);
    }
    else
    {
        lCarDirection = BrnMath::Flatten(lpCar->GetDirection());
    }

    const Vector2 lCarPosition = BrnMath::Flatten(lpCar->GetPosition());

    const s32 liGeneratedCount =
        ExtrapolateRouteBackwards(liNumSectionsToGenerate, lpCar->GetBestSectionIndex(),
                                  lCarDirection, lCarPosition, lpAISectionsData,
                                  lauGeneratedIndices);

    s32 liCacheSlot = -1;
    for (s32 liGenerated = 0; liGenerated < liGeneratedCount; ++liGenerated)
    {
        const u32 luSectionIndex = lauGeneratedIndices[static_cast<u32>(liGenerated)].muSection;
        const u8  lu8PortalIndex =
            static_cast<u8>(lauGeneratedIndices[static_cast<u32>(liGenerated)].muPortal);

        SectionData& lrEntry = lpRacingLine->maSectionCache[liCacheSlot & KI_SECTION_CACHE_MASK];
        lrEntry.mCachedSectionIndex = static_cast<s16>(liCacheSlot);
        lrEntry.mbTargetUpToDate    = false;

        const AISection* lpSection = lpAISectionsData->GetAISection(luSectionIndex);
        lrEntry.mpLineSection  = lpSection;
        lrEntry.mpTargetPortal = lpSection->GetPortal(lu8PortalIndex);
        lrEntry.SetFastSectionCorners(lpSection);

        --liCacheSlot;
    }

    return liCacheSlot + 1;
}

// =================================================================================================
// SetupSectionExit @0x8278F548  (re-verified against the asm this wave -- unchanged)
//
// GetSectionPointer -> zero two edge vectors -> FindMaximalEdges(left, right, 1.0)
// -> exit = (left + right) * 0.5 -> SectionData::SetSectionExit (inlined `vrlimi128` pair).
// =================================================================================================
void RacingLineGenerator::SetupSectionExit(RacingLine* lpRacingLine, s32 liNodeIndex)
{
    SectionData* lpCurrentSectionData = GetSectionPointer(lpRacingLine, liNodeIndex);

    Vector2 lLeftEdge;
    Vector2 lRightEdge;
    lLeftEdge.SetZero();
    lRightEdge.SetZero();

    lpCurrentSectionData->mHardNoGoMap.FindMaximalEdges(lLeftEdge, lRightEdge, 1.0f);

    // Midpoint of the two maximal edges (flt_820C4168 == 0.5f). The vaddfp/vmulfp combine all
    // four SIMD lanes; SetSectionExit only consumes x and y.
    Vector2 lMidPoint;
    lMidPoint.mV = __vmulfp(__vaddfp(lLeftEdge.mV, lRightEdge.mV),
                              __vcfsx(__vspltisw(1), 1));

    lpCurrentSectionData->SetSectionExit(lMidPoint);
}

// =================================================================================================
// DropHardNoGoLinesIntoMap @0x8278F600  (re-verified against the asm this wave -- unchanged)
//
// GetSectionPointer(miSectionToSpread) -> HardNoGoMap::MakeMap over the next line budget window
// starting at miHNGLineStart; on success step miBackwardsStep, otherwise advance miHNGLineStart
// by one budget. (Member names: DWARF BrnRacingLine.h:103/:106/:109, pinned to the
// 0xBC0/0xBC4/0xBC8 stores of ClearSectionCache @0x8276E090.)
// =================================================================================================
void RacingLineGenerator::DropHardNoGoLinesIntoMap(RacingLine* lpRacingLine)
{
    SectionData* lpCurrentSectionData =
        GetSectionPointer(lpRacingLine, lpRacingLine->miSectionToSpread);

    const s32 liStart = lpRacingLine->miHNGLineStart;
    const s32 liEnd   = liStart + KI_MAX_HNG_LINES_TO_PLACE_PER_FRAME;

    if (lpCurrentSectionData->mHardNoGoMap.MakeMap(lpCurrentSectionData->mpLineSection,
                                                   liStart, liEnd))
    {
        ++lpRacingLine->miBackwardsStep;
    }
    else
    {
        lpRacingLine->miHNGLineStart += KI_MAX_HNG_LINES_TO_PLACE_PER_FRAME;
    }
}

}

// FOLDED FROM BrnRacingLineGenerator_Curve.cpp
// =================================================================================================
// RacingLine/BrnRacingLineGenerator_Curve.cpp  (aiwave2 lane R3, 2026-09-05)
//
// Partfile of RacingLine/BrnRacingLineGenerator.cpp. The two REAL out-of-line console bodies that
// carry no IDA export at all -- lanes R1 and R2 both parked them, and the already-landed
// BrnRacingLineGenerator_Query.cpp calls BOTH, so the game cannot link without this file.
//
//   BODIED (no IDA export -- disassembled straight out of the image with tools/re/ppcdis.py)
//     GetPointAndNormalOnCurve  @0x82780B18 .. 0x827815F4  (DWARF BrnRacingLineGenerator.h:331)
//                               -- called by GetPointFarAhead @0x82790204
//     SetUpIncomingPortalTarget @0x8278F878 .. 0x8278FB1C  (DWARF :359)
//                               -- called TWICE by InitialiseRacingLine (@0x8278FF50/@0x8278FF78)
//   BODIED (no export -- the console inlines them; recovered from the blocks inside the two above
//           and inside InitialiseRacingLine @0x8278FB20, written ONCE, here)
//     IsTargetUpToDate          (DWARF :323) -- @0x8278F8A0..0x8278F8D0 and @0x8278FE44..0x8278FE74
//     IsAJunction               (DWARF :292) -- @0x8278FE88..0x8278FEA4
//
// PARAMETER ORDER PROVED (the open question lane R2 raised). GetPointAndNormalOnCurve's prologue
// @0x82780B48..0x82780B64 keeps r3=this, r4=lpRacingLine, r5=liSectionIndex, f1=lfInterp and then
// `mr r14, r7` / `mr r27, r8` -- r6 is SKIPPED because the f32 consumes its GPR slot (COMMON_BRIEF
// rule 4). r14 is handed to GetIterativeHermite as its `&lrResult` (@0x827810A4 `mr r5, r14`;
// GetIterativeHermite's own prologue @0x82776D8C is `mr r26, r5` -> the result pointer), and r27 is
// what the closing normalise stores into (@0x827815C4). So the FOURTH parameter (r7) IS lrPoint and
// the FIFTH (r8) IS lrNormal -- lane R2's inference from `vmaddfp v12, v0, v12, v10`
// (normal * mfRoadPlacement + point) is CONFIRMED, and the DWARF prototype needs no change.
//
// SOURCE FILE / LINE NUMBERS: every assert cites the same unity-build path the sibling partfiles
// use; the console line is quoted at each site. The X360 retail build DOES emit these (each is a
// full BeginAssert / StrStream / FireAssert / EndAssert sequence in the listing).
//
// CONSTANTS RECOVERED FROM THE IMAGE (big-endian, file offset = VA - 0x82000000):
//   flt_82001C98 = 1.0f     flt_82001CC0 = 0.0f     flt_820C4168 = 0.5f
//   flt_820C424C = 0.1f     (the curve-normal sampling step)
//   unk_820C3B40 = 0x37800000 = 2^-16       (rw::math::vpu::IsSimilar epsilon)
//   flt_820C3B70 = 0x34000000 = FLT_EPSILON (RwMath::IsZero epsilon)
// =================================================================================================

#include "GameSource/World/AI/RacingLine/BrnRacingLineGenerator.h"

#include <cmath>

#include "GameSource/World/AI/Route/BrnRacingLine.h"      // RacingLine + SectionData
#include "GameSource/World/AI/BrnAIPortal.h"              // Portal (SectionData::mpTargetPortal)
#include "SharedClasses/AI/AISectionsResourceType.h"      // AISection::mx8Flags
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CgsDev::Assert::Begin/Fire/End
#include "GameShared/GameClasses/Development/CgsStrStream.h"   // CgsDev::StrStream

namespace BrnAI
{
namespace
{
    // ARTIST section flag test uses bit 0x10.


// ---- baked constants ---------------------------------------------------------------------
// The cache is 16 slots addressed by `sectionIndex & 15` (`clrlwi r11, r5, 28` everywhere; here
// @0x8278F8A0 and @0x8278FE44).


// flt_820C3B70 lane 0 == FLT_EPSILON: the RwMath::IsZero epsilon (`vandc` sign-clear + `vcmpgtfp`
// + the 0x0004080C AND-reduce permute @0x82781388 and @0x82781474).


// unk_820C3B40 lane 0 == 2^-16: the rw::math::vpu::IsSimilar epsilon the entrance/exit degeneracy
// tests @0x82780C78 / @0x82780CF8 / @0x82781030 run on.


// flt_820C424C == 0.1f -- how much further along the hermite GetPointAndNormalOnCurve samples its
// second point to get the tangent (`lfs f0, 0(r15)` @0x827811BC, with r15 == 0x820C424C).
const f32 KF_CURVE_NORMAL_STEP = 0.1f;

// The unity-build file string every streamed assert in this TU cites.



// ---- small shared helpers ----------------------------------------------------------------
// These mirror the file-local helpers of BrnRacingLineGenerator.cpp; both copies live in an
// anonymous namespace, so there is no LNK2005 and no second external symbol.

// RwMath::IsZero on a 2D vector: |x| <= FLT_EPSILON && |y| <= FLT_EPSILON.
inline bool IsZeroCurveVector(f32 lfX, f32 lfY)
{
    return !(std::fabs(lfX) > KF_ZERO_EPSILON) && !(std::fabs(lfY) > KF_ZERO_EPSILON);
}

// rw::math::vpu::IsSimilar on the x/y lanes: no lane differs by more than 2^-16. The console
// duplicates x/y into z/w with `vrlimi128 v, v, 3, 2` so all four compared lanes are the pair.
inline bool IsSimilar2D(const Vector2& lrA, const Vector2& lrB)
{
    return !(std::fabs(lrA.mV.x - lrB.mV.x) > KF_PORTAL_MATCH_EPSILON)
        && !(std::fabs(lrA.mV.y - lrB.mV.y) > KF_PORTAL_MATCH_EPSILON);
}

// The lane-wise `vcmpeqfp.` self-compare the console uses as "is this finite / not-NaN".





// ---- the streamed asserts (BeginAssert ; StrStream ; FireAssert ; EndAssert) ---------------










// "<prefix>" << index << ", interp " << interp << "\n"   (str_820C7304 == ", interp ").
void FireBadCentreAssert(const char* lpcPrefix, s32 liSectionIndex, f32 lfInterp, s32 liLine)
{
    char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
    CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
    lStream << lpcPrefix << liSectionIndex << ", interp " << lfInterp << "\n";
    CgsDev::Assert::BeginAssert();
    CgsDev::Assert::FireAssert(lacMessageBuffer, CGS_ASSERT_LOCATION(KPC_SOURCE_FILE_CPP, liLine));
    CgsDev::Assert::EndAssert();
}

// "Zero vector being normalize, Interp = " << interp << "\n"   (str_820C72A8, .cpp:2267).
void FireZeroNormalAssert(f32 lfInterp, s32 liLine)
{
    char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
    CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
    lStream << "Zero vector being normalize, Interp = " << lfInterp << "\n";
    CgsDev::Assert::BeginAssert();
    CgsDev::Assert::FireAssert(lacMessageBuffer, CGS_ASSERT_LOCATION(KPC_SOURCE_FILE_CPP, liLine));
    CgsDev::Assert::EndAssert();
}
}   // anonymous namespace

// =================================================================================================
// IsAJunction -- DWARF BrnRacingLineGenerator.h:292. NO IDA export; the console inlines it.
// Recovered from InitialiseRacingLine @0x8278FE88..0x8278FEA4, the one site that calls it through
// the wrapper shape (GetSectionPointer, then the flag test):
//
//   0x8278FE84  bl  GetSectionPointer(lpRacingLine, liSection)
//   0x8278FE88  lwz    r11, 0xA0(r3)          -- SectionData::mpLineSection
//   0x8278FE98  lbz    r11, 0x17(r11)         -- AISection::mx8Flags (host offsetof == 0x23)
//   0x8278FE9C  rlwinm r11, r11, 0, 27, 27    -- & 0x10
//   0x8278FEA4  beq -> the "not a junction" arm
//
// LookForStraightSectionAhead / Behind open-code the identical `& 0x10` test on the same member;
// this is the wrapper the DWARF names.
// =================================================================================================
bool RacingLineGenerator::IsAJunction(RacingLine* lpRacingLine, s32 liSectionIndex)
{
    const SectionData* lpSectionData = GetSectionPointer(lpRacingLine, liSectionIndex);
    return (lpSectionData->mpLineSection->mx8Flags & KX8_SECTION_FLAG_JUNCTION) != 0;
}

// =================================================================================================
// IsTargetUpToDate -- DWARF BrnRacingLineGenerator.h:323. NO IDA export; the console inlines it at
// both of its call sites, and the two copies agree:
//
//   InitialiseRacingLine @0x8278FE44..0x8278FE74 (the fully open-coded form)
//     r11 = lpRacingLine + (liSection & 15) * 0xB0
//     lhz / extsh 0xB8(r11) == maSectionCache[i].mCachedSectionIndex  (0x10 + 0xA8)
//     != liSection -> the value is 0 (false)
//     otherwise    -> lbz 0xBA(r11) == maSectionCache[i].mbTargetUpToDate (0x10 + 0xAA)
//
//   SetUpIncomingPortalTarget @0x8278F8A0..0x8278F8D0 (the same test, but the second half goes
//     through a real `bl GetSectionPointer` and reads +0xAA off its result -- which is the same
//     &maSectionCache[i & 15], so the two shapes mean the same thing).
//
// i.e. the READ twin of SetTargetUpToDate @0x82768710, whose write is this same byte. The console
// emits NO assert on the stale-cache arm here (SetTargetUpToDate's does) -- it simply answers
// false, which is why InitialiseRacingLine may call it on an index it has not cached yet.
// =================================================================================================
bool RacingLineGenerator::IsTargetUpToDate(RacingLine* lpRacingLine, s32 liSectionIndex)
{
    if (!CacheUpToDate(lpRacingLine, liSectionIndex))
    {
        return false;
    }
    return lpRacingLine->maSectionCache[liSectionIndex & KI_SECTION_CACHE_MASK].mbTargetUpToDate;
}

// =================================================================================================
// SetUpIncomingPortalTarget @0x8278F878 .. 0x8278FB1C (170 instructions; no IDA export)
//   (r3/r29 = this, r4/r30 = lpRacingLine, r5 = liSectionIndex)
//
// Give the section its ENTRANCE point -- the incoming half of the target pair whose outgoing half
// SetupSectionExit @0x8278F548 writes. InitialiseRacingLine calls it right after SetTargetUpToDate
// on every section it processes, in both the junction and the ordinary arm.
//
//   0x8278F894  lpSectionData = GetSectionPointer(lpRacingLine, liSectionIndex)   (kept in r31)
//   0x8278F898  addi r5, r31, -1 -- everything below is about the PREVIOUS section
//   0x8278F8A0  the inlined IsTargetUpToDate(lpRacingLine, liSectionIndex - 1)
//   0x8278F8E4  arm A: lvx 0x40(prev) + `vpermwi128 .., 0xBF` == prev->GetSectionExit(), then the
//               `vrlimi128` mask-8 / mask-4 pair == SectionData::SetSectionEntrance
//   0x8278F9E0  arm B: FindMaximalEdges(left, right, 0.0f) on THIS section's own map -- height 0.0
//               is the ENTRANCE end of the map (SetupSectionExit uses 1.0, the exit end)
//   0x8278FA44  arm B: entrance = (left + right) * 0.5   (flt_820C4168)
//   0x8278F938 / 0x8278FA74  each arm then asserts its OWN copy of "Zero EXit created.\n"
//               (str_820C7D6C) when both lanes of the new entrance are exactly 0.0 -- .cpp:1159 in
//               arm A, .cpp:1178 in arm B. (The console's own spelling: this member writes the
//               ENTRANCE, but the literal says "EXit".) Two distinct line numbers is what proves
//               the source really carries the assert twice, once per arm.
// Each assert is a `vcmpeqfp.` against a 0.0 splat, lane 0 then lane 1, and it fires only when both
// are equal -- the console then RETURNS anyway; there is no recovery arm.
// =================================================================================================
void RacingLineGenerator::SetUpIncomingPortalTarget(RacingLine* lpRacingLine, s32 liSectionIndex)
{
    SectionData* lpSectionData = GetSectionPointer(lpRacingLine, liSectionIndex);

    if (IsTargetUpToDate(lpRacingLine, liSectionIndex - 1))
    {
        // The previous section already has a target, so this section's entrance IS that exit.
        SectionData* lpPreviousSectionData = GetSectionPointer(lpRacingLine, liSectionIndex - 1);
        lpSectionData->SetSectionEntrance(lpPreviousSectionData->GetSectionExit());

        const Vector2 lEntrance = lpSectionData->GetSectionEntrance();
        if (lEntrance.mV.x == 0.0f && lEntrance.mV.y == 0.0f)
        {
            FireStreamedTextAssert("Zero EXit created.\n", 1159);
        }
    }
    else
    {
        // Nothing to inherit: take the midpoint of this section's own maximal hard-no-go edges at
        // the entrance end of the map.
        Vector2 lLeftEdge;
        Vector2 lRightEdge;
        lLeftEdge.SetZero();
        lRightEdge.SetZero();

        lpSectionData->mHardNoGoMap.FindMaximalEdges(lLeftEdge, lRightEdge, 0.0f);

        // The vaddfp / vmulfp pair combines all four SIMD lanes; SetSectionEntrance consumes only
        // x and y.
        Vector2 lMidPoint;
        const VecFloat lHalf(0.5f);
        lMidPoint.mV = __vmulfp(__vaddfp(lLeftEdge.mV, lRightEdge.mV), lHalf.mV);

        lpSectionData->SetSectionEntrance(lMidPoint);

        const Vector2 lEntrance = lpSectionData->GetSectionEntrance();
        if (lEntrance.mV.x == 0.0f && lEntrance.mV.y == 0.0f)
        {
            FireStreamedTextAssert("Zero EXit created.\n", 1178);
        }
    }
}

// =================================================================================================
// GetPointAndNormalOnCurve @0x82780B18 .. 0x827815F4 (438 instructions; no IDA export)
//   (r3/r20 = this, r4/r29 = lpRacingLine, r5/r22 = liSectionIndex, f1/f31 = lfInterp,
//    r7/r14 = &lrPoint, r8/r27 = &lrNormal -- r6 skipped, the f32 takes its GPR slot)
//
// Evaluate the section's hermite centre line at lfInterp and hand back the point plus the UNIT
// NORMAL there. GetPointFarAhead @0x82790204 is the only caller: it offsets the point along the
// normal by mfRoadPlacement, and turns the normal 90 degrees back into the travel direction.
//
//   0x82780B68  lpFirstTrySectionData = GetSectionPointer(lpRacingLine, liSectionIndex)
//   0x82780B84  `vcmpeqfp.` self-compare on lfInterp -> "Bad interp\n"  (.cpp:2148)
//   0x82780C24  GenerateInOutVectors(lpRacingLine, liSectionIndex, lInVector (r6 = sp+0xA0),
//                                    lOutVector (r7 = sp+0x120))
//   0x82780C44  A = lpFirstTrySectionData->GetSectionEntrance()   -> [sp+0x100]
//   0x82780C60  B = lpFirstTrySectionData->GetSectionExit()       -> [sp+0xF0]
//               (`vpermwi128 .., 0xBF` == word-select {2,3,3,3}, the z/w pair pulled down)
//   0x82780C78  IsSimilar(A, B) -- the DEGENERATE-section arm below
//   0x8278109C  the main arm: GetIterativeHermite(A, B, in, out, lfInterp, lrPoint)
//   0x827810D4  validity of lrPoint -> "Bad centre at index " << i << ", interp " << t << "\n"
//               (.cpp:2220)
//   0x827811BC  lfAheadParam = t + 0.1 (flt_820C424C); when that passes 1.0 (flt_82001C98) it
//               steps BACKWARDS instead (t - 0.1) and remembers that in r28
//   0x82781218  GetSimpleHermite(A, B, in, out, splat(lfAheadParam), lPointAhead (sp+0x110))
//   0x8278122C  validity of lPointAhead -> "Bad centre ahead at index " ...  (.cpp:2244)
//   0x82781314  THE NORMAL. The two `vrlimi128` write masks decode as mask 4 == the Y lane
//               (@0x82781338 / @0x82781354, vA field 68) and mask 8 == the X lane (@0x82781370,
//               vA field 72) -- the same encodings, in the same order, that
//               SectionData::SetSectionExit (masks 2/1) and SetSectionEntrance (masks 8/4) use.
//               So the Y lane takes the tangent's X difference and the X lane the NEGATED Y
//               difference:
//                   normal = (-tangent.y, tangent.x)      -- a clean +90 degree rotation
//               and the r28 (stepped-backwards) arm negates BOTH differences, keeping the same
//               orientation. GetPointFarAhead then reads back (normal.y, -normal.x) == the
//               tangent, which is exactly self-consistent -- the two bodies corroborate each other.
//   0x82781388  IsZero(normal) -> fall back to the raw chord (see the [FLAG] on that arm)
//   0x82781458  IsZero again -> "Zero vector being normalize, Interp = " << lfAheadParam (:2267)
//   0x82781574  `vrsqrtefp` + two Newton-Raphson steps + `vmulfp128` == normalise. There is NO
//               `vsel` zero guard here (unlike the shared Normalise2D idiom elsewhere in this
//               class): the console divides by zero and publishes the NaN. Reproduced as-is -- the
//               assert above is the console's only warning, and an assert is not a guard.
// =================================================================================================
void RacingLineGenerator::GetPointAndNormalOnCurve(RacingLine* lpRacingLine, s32 liSectionIndex,
                                                   f32 lfInterp, Vector2& lrPoint,
                                                   Vector2& lrNormal)
{
    SectionData* lpFirstTrySectionData = GetSectionPointer(lpRacingLine, liSectionIndex);

    if (!(lfInterp == lfInterp))
    {
        FireStreamedTextAssert("Bad interp\n", 2148);
    }

    Vector2 lInVector;
    Vector2 lOutVector;
    GenerateInOutVectors(lpRacingLine, liSectionIndex, lInVector, lOutVector);

    Vector2 lLineStart = lpFirstTrySectionData->GetSectionEntrance();
    Vector2 lLineEnd   = lpFirstTrySectionData->GetSectionExit();

    if (IsSimilar2D(lLineStart, lLineEnd))
    {
        // A degenerate section -- its portal entrance and exit are the same point, so there is no
        // chord to run a hermite along. Answer the entrance, and try to borrow a chord from the
        // exit of the NEXT cached section.
        lrPoint = lLineStart;

        if (liSectionIndex >= lpRacingLine->mLastSectionInCache)
        {
            // 0x82781070: the default normal is the literal pair (flt_82001C98, flt_82001CC0) with
            // z/w zeroed, and the console branches STRAIGHT to the store at 0x827815C4 -- it is
            // published UNNORMALISED (it is already unit length).
            lrNormal.mV.x = 1.0f;
            lrNormal.mV.y = 0.0f;
            lrNormal.mV.z = 0.0f;
            lrNormal.mV.w = 0.0f;
            return;
        }

        SectionData* lpSecondTrySectionData = GetSectionPointer(lpRacingLine, liSectionIndex + 1);
        lLineEnd = lpSecondTrySectionData->GetSectionExit();

        if (IsSimilar2D(lLineStart, lLineEnd))
        {
            lrNormal.mV.y = 0.0f;
            lrNormal.mV.z = 0.0f;
            lrNormal.mV.w = 0.0f;
            return;
        }
    }

    // GetIterativeHermite returns the CURVE PARAMETER it settled on, which is not lfInterp (see its
    // banner in BrnRacingLineGenerator.cpp) -- both asserts below print that parameter, not the
    // argument.
    const f32 lfCurveParam =
        GetIterativeHermite(lLineStart, lLineEnd, lInVector, lOutVector, lfInterp, lrPoint);

    if (!IsValid2D(lrPoint))
    {
        FireBadCentreAssert("Bad centre at index ", liSectionIndex, lfCurveParam, 2220);
    }

    // Sample a second point a little further along the curve; when that would run off the end,
    // sample BEHIND instead and flip the difference so the tangent still points forwards.
    bool lbSteppedBackwards = false;
    f32  lfAheadParam       = lfCurveParam + KF_CURVE_NORMAL_STEP;
    if (lfAheadParam > 1.0f)
    {
        lfAheadParam       = lfCurveParam - KF_CURVE_NORMAL_STEP;
        lbSteppedBackwards = true;
    }

    const VecFloat lAheadVector(lfAheadParam);
    Vector2 lPointAhead;
    GetSimpleHermite(lLineStart, lLineEnd, lInVector, lOutVector, lAheadVector, lPointAhead);

    if (!IsValid2D(lPointAhead))
    {
        FireBadCentreAssert("Bad centre ahead at index ", liSectionIndex, lfAheadParam, 2244);
    }

    // The Y lane is written first (mask 4) and the X lane second (mask 8); the two arms are exact
    // negations of one another, i.e. the same forward tangent turned 90 degrees.
    if (lbSteppedBackwards)
    {
        lrNormal.mV.y = lrPoint.mV.x     - lPointAhead.mV.x;
        lrNormal.mV.x = lPointAhead.mV.y - lrPoint.mV.y;
    }
    else
    {
        lrNormal.mV.y = lPointAhead.mV.x - lrPoint.mV.x;
        lrNormal.mV.x = lrPoint.mV.y     - lPointAhead.mV.y;
    }

    // The console retains the existing z/w lanes until the full-register normalization.

    if (IsZeroCurveVector(lrNormal.mV.x, lrNormal.mV.y))
    {
        lrNormal.mV.y = lLineStart.mV.x - lLineEnd.mV.x;
        lrNormal.mV.x = lLineEnd.mV.y   - lLineStart.mV.y;
    }

    if (IsZeroCurveVector(lrNormal.mV.x, lrNormal.mV.y))
    {
        FireZeroNormalAssert(lfAheadParam, 2267);
    }

    // vrsqrtefp + two Newton-Raphson refinements + vmulfp128, with no zero guard.
    const f32 lfLengthSq  = (lrNormal.mV.x * lrNormal.mV.x) + (lrNormal.mV.y * lrNormal.mV.y);
    const f32 lfInvLength = 1.0f / std::sqrt(lfLengthSq);
    const VecFloat lInvLength(lfInvLength);
    lrNormal.mV = __vmulfp(lrNormal.mV, lInvLength.mV);
}
}

// FOLDED FROM BrnRacingLineGenerator_Extrapolate.cpp
// =================================================================================================
// RacingLine/BrnRacingLineGenerator_Extrapolate.cpp  (aiwave A6, 2026-09-03)
//
//   BrnAI::RacingLineGenerator::ExtrapolateRouteBackwards @0x82782580  (398 insns, VMX128)
//   BrnAI::RacingLineGenerator::ExtrapolateRouteForwards  @0x82781AE8  (330 insns, VMX128)
//   BrnAI::RacingLineGenerator::ExtrapolateTwistyRoute    @0x82782018  (344 insns, VMX128)
//   DWARF BrnRacingLineGenerator.h:139 / :148 / :158; bodies at .cpp :3085 / :2699 / :2877.
//
// PROTOTYPE (DWARF, parameter names verbatim -- dumpfile BrnRacingLineGenerator.cpp:528/825/689):
//   (int32_t liNumSectionsToGenerate, uint16_t|int32_t lStartSectionIndex,
//    const Vector2 lCarDirection, const Vector2 lCarPosition,
//    const AISectionsData* lpAISectionsData, const ExtrapolatedIndexArray& lpauGeneratedIndices)
// The DIRECTION is the third parameter, not the position -- the DWARF names it, ProcessExtrapolatedRoute
// feeds v1 from request+0x20 (mCarDirection) and v2 from +0x10 (mCarPosition) @0x8278C534/0x8278C53C,
// and Backwards negates v1. The DWARF's `const` on the array reference is drift: the console writes
// every generated pair through it (`stw` into the Array<>::operator[] result), so it is taken
// non-const here. All three are STATIC: the console passes the generate-count in r3 and never a
// `this` (ProcessExtrapolatedRoute @0x8278C528 `li r3, 6` / @0x8278C66C `li r3, 8`).
//
// Partfile of RacingLine/BrnRacingLineGenerator.cpp. The three are the producers behind
// RouteMapModule::ProcessExtrapolatedRoute @0x8278C4C8 (6 behind + 8 ahead) and the
// ResetOnTrackManager's Scan{Backwards,Forwards}AlongExtrapolatedRoute; an "extrapolated route"
// is the road portal-to-portal through the AI sections, and every non-A* AI driver (Road Rage,
// pursuit, marked man, avoid-player, free roam) drives on one.
//
// Shared register map (see the header banner): r3 = liNumSectionsToGenerate (spilled to
// arg_14), r4 = start section, v1 = lCarDirection (`vmr128 v12x, v1` is the ONLY vector argument
// read; v2 = lCarPosition is never touched), r5 = lpAISectionsData, r6 = &lpauGeneratedIndices.
//
// Shared inner geometry (identical instruction sequence in all three; Backwards
// 0x82782844..0x82782A1C, Forwards 0x82781DAC..0x82781F84, Twisty 0x827822D4..0x827824A8):
//   lPossiblePortalCentre3D = portal->(x, y, z, 0)      (three lfs/stfs + stw 0)
//   lPossiblePortalCentre2D = (x, z)                    (vrlimi128 lane packs 8,0 / 4,1)
//   lCentreOfNextSection3D  = next->GetMiddle()         (bl 0x826771D0)
//   lCentreOfNextSection2D  = (x, z)
//   lPortalLeadsToDirection = centre2D - portal2D       (vsubfp128)
//   IsZero(lPortalLeadsToDirection): |x| <= eps && |y| <= eps, eps = flt_820C3B70 lane 0
//     (`lvlx ; vspltw ..,0` ; `vandc` abs ; `vcmpgtfp` per lane) -> the "Strangely shaped
//     section index <n>\n" streamed assert, then the portal is skipped
//   Normalize: vrsqrtefp + two Newton-Raphson steps (an exact 1/sqrt here)
//   lfAheadness = Dot(normalised, v12x)                 (vmulfp128 ; vspltw 0/1 ; vaddfp)
//
// Baked constants (image.bin, big-endian, 2026-09-03):
//   flt_820C3B70 = 0x34000000 = FLT_EPSILON        (IsZero epsilon)
//   unk_820C3B40 = 0x37800000 = 1.52587890625e-05  (= 2^-16; the IsSimilar epsilon of the
//                                                  Backwards exit-portal match)
//   flt_82035570 = 0xFF7FFFFF = -FLT_MAX           (best-aheadness seed, Backwards/Forwards)
//   flt_8204F664 = 0x7F7FFFFF = +FLT_MAX           (best-aheadness seed, Twisty, liGen >= 0)
//   flt_82001CC0 = 0x00000000 = 0.0f               (Twisty's "still points forward" floor)
//
// Section flag bits read through mx8Flags (+0x17): 0x01 IsShortcut, 0x40 IsAIShortcut (DWARF
// :377/:380, now inline in AISectionsResourceType.h) and 0x10 (`rlwinm r11,r11,0,27,27`) -- the
// track-back stop; see KX8_SECTION_FLAG_TRACKBACK_STOP below.
// =================================================================================================

#include <cmath>
#include <cfloat>

#include "GameSource/World/AI/RacingLine/BrnRacingLineGenerator.h"
#include "GameSource/World/AI/Route/BrnRouteMapModule.h"          // BrnAI::SectionAndPortalIndices
#include "SharedClasses/AI/AISectionsResourceType.h"              // AISection / AISectionsData
#include "GameSource/World/AI/BrnAIPortal.h"                      // BrnAI::Portal
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"                // CGS_ASSERT + Begin/Fire/End
#include "GameShared/GameClasses/Development/CgsStrStream.h"      // CgsDev::StrStream

namespace BrnAI
{
namespace
{
    // ARTIST section flag test uses bit 0x10.
    const u8 KX8_SECTION_FLAG_TRACKBACK_STOP = 0x10;

    // flt_820C3B70 lane 0 == FLT_EPSILON: the RwMath::IsZero epsilon.


    // unk_820C3B40 lane 0 == 0x37800000 == 2^-16: the rw::math::vpu::IsSimilar epsilon used by
    // ExtrapolateRouteBackwards to match the exit section's portal against the chosen one
    // (`lvlx v0, r0, r18 ; vspltw v13, v0, 0` at 0x82782B00/0x82782B08, r18 = 0x820C3B40).


    // flt_82035570 == -FLT_MAX / flt_8204F664 == +FLT_MAX: the lfBestAheadnesss seeds.
    const f32 KF_AHEADNESS_SEED_MIN = -FLT_MAX;
    const f32 KF_AHEADNESS_SEED_MAX =  FLT_MAX;

    // flt_82001CC0 == 0.0f: Twisty's "aheadness must not point backwards" floor.
    const f32 KF_AHEADNESS_FLOOR = 0.0f;

    // The "no portal chosen" marker written to SectionAndPortalIndices::muPortal (li r11,0xFF).
    const u32 KU_NO_PORTAL = 0xFF;

    // BrnWorld::KI_INVALID_SECTION_INDEX (the assert text at :3087); the asm literal is
    // `cmplwi cr6, r31, 0x7FFF` @0x827825BC. AICar.h carries the same value as
    // AICar::KI_INVALID_SECTION_INDEX; repeated here so this leaf does not pull BrnAICar.h.
    const u16 KU_INVALID_SECTION_INDEX = 0x7FFF;

    // Stuck budget: the third single-portal dead end ends the walk (`cmpwi cr6, r11, 2 ; bgt`).
    const s32 KI_MAX_STUCK_ITERATIONS = 2;

    // The unity build's file string every "Strangely shaped section index" assert cites.
    const char* const KPC_SOURCE_FILE =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../World/AI/RacingLine/BrnRacingLineGenerator.cpp";






    // The streamed assert at BrnRacingLineGenerator.cpp:<liLine>: BeginAssert ; StrStream over
    // gpcMessageBuffer << "Strangely shaped section index " << <index> << "\n" ; FireAssert ;
    // EndAssert (0x82782930..0x827829A4 / 0x82781E9C..0x82781F0C / 0x827823C4..0x82782434).
    void FireStrangelyShapedSectionAssert(u32 luSectionIndex, s32 liLine)
    {
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStream << "Strangely shaped section index " << static_cast<s32>(luSectionIndex) << "\n";
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lacMessageBuffer, CGS_ASSERT_LOCATION(KPC_SOURCE_FILE, liLine));
        CgsDev::Assert::EndAssert();
    }

    // The shared inner geometry (banner): how far "ahead" of lReferenceDirection the road
    // through lpPossibleNextPortal into lpNextSection leads. Returns false (and fires the
    // streamed assert) when the portal sits on the next section's middle.
    bool ComputeAheadness(const Portal* lpPossibleNextPortal, const AISection* lpNextSection,
                          u32 luLinkedSectionIndex, Vector2 lReferenceDirection, s32 liAssertLine,
                          f32& lrfAheadness)
    {
        // lPossiblePortalCentre2D = (x, z) of the portal; lCentreOfNextSection2D = (x, z) of
        // the section middle; lPortalLeadsToDirection = centre - portal.
        const Vector3 lCentreOfNextSection3D = lpNextSection->GetMiddle();
        const f32 lfLeadsToX = lCentreOfNextSection3D.mV.x - lpPossibleNextPortal->GetPositionX();
        const f32 lfLeadsToY = lCentreOfNextSection3D.mV.z - lpPossibleNextPortal->GetPositionZ();

        if (IsZero2D(lfLeadsToX, lfLeadsToY))
        {
            FireStrangelyShapedSectionAssert(luLinkedSectionIndex, liAssertLine);
            return false;
        }

        // Normalize (vrsqrtefp + 2 x Newton-Raphson == exact) then Dot with the reference.
        const f32 lfInvLength =
            1.0f / sqrtf((lfLeadsToX * lfLeadsToX) + (lfLeadsToY * lfLeadsToY));
        lrfAheadness = (lfLeadsToX * lfInvLength * lReferenceDirection.mV.x)
                     + (lfLeadsToY * lfInvLength * lReferenceDirection.mV.y);
        return true;
    }

    // The stuck walk-back shared by the three bodies (Backwards 0x82782718..0x82782794,
    // Forwards 0x82781C80..0x82781CFC, Twisty 0x827821C4..0x82782240): from the entry below
    // the current one down to 0, find the last generated section carrying the track-back
    // stop bit. On success the walk resumes AT that entry (liGeneratedNodeIndex = liTrackBack),
    // with the section it had continued into blocked; liPreviousSection is NOT touched.
    bool WalkBackToTrackBackStop(const AISectionsData* lpAISectionsData,
                                 const ExtrapolatedIndexArray& lpauGeneratedIndices,
                                 s32& lriGeneratedNodeIndex, s32& lriCurrentSectionIndex,
                                 const AISection*& lrpCurrentSection, s32& lriBlockedSection)
    {
        for (s32 liTrackBack = lriGeneratedNodeIndex - 1; liTrackBack >= 0; --liTrackBack)
        {
            lriCurrentSectionIndex = static_cast<s32>(lpauGeneratedIndices[liTrackBack].muSection);
            lrpCurrentSection      = lpAISectionsData->GetAISection(lriCurrentSectionIndex);
            if ((lrpCurrentSection->mx8Flags & KX8_SECTION_FLAG_TRACKBACK_STOP) != 0)
            {
                lriBlockedSection      = static_cast<s32>(lpauGeneratedIndices[liTrackBack + 1].muSection);
                lriGeneratedNodeIndex  = liTrackBack;
                return true;
            }
        }
        return false;
    }
}

// =================================================================================================
// ExtrapolateRouteBackwards @0x82782580 (DWARF BrnRacingLineGenerator.cpp:3085)
//
//   r30 = liNumSectionsToGenerate (-> arg_14), r31 = luStartSectionIndex (clrlwi 16),
//   r24 = lpAISectionsData, r17 = &lpauGeneratedIndices, v126 = lCarDirection
//   0x827825BC  assert(luStartSectionIndex != 0x7FFF)                             (:3087)
//   0x827825F0  lpCurrentSection = GetAISection(start) ; lbUseShortcuts (var_1E0) = flags & 1 ;
//               lbUseAIShortcuts (var_1DF) = flags & 0x40
//   0x82782628  v123 = lCarBackwardsDirection = -lCarDirection   (vxor with the sign splat)
//   r16 = liPreviousSection = -1, var_1D8 = liBlockedSection = -1, var_1D4 = liStuckIterations = 0,
//   r20 = liGeneratedNodeIndex = 0 ; r22 = liCurrentSectionIndex
//   0x82782618  if (liNum <= 0) return 0
//   loop (0x827826A8):
//     r19 = liExitToSectionIndex = -1 ; f31 = lfBestAheadnesss = -FLT_MAX ; r28 = GetAISection(r22)
//     0x827826E8  if (r28->mu8NumPortals == 1) {           -- a dead end
//       0x82782704    indices[liGen].muPortal = 0             (muSection is NOT written here)
//       0x82782714    if (++liStuckIterations > 2) return liGen
//       0x82782718    walk back (helper above); not found -> return liGen
//     }
//     0x82782798  r26 = lpBestNextPortal = NULL ; for (lPortalIndex < mu8NumPortals):
//       0x827827C0    liLinkedSectionIndex = portal->mu16LinkSection (+0x10)
//       0x827827C4    if (linked == liBlockedSection || linked == liPreviousSection) continue
//       0x82782814    if (!lbUseShortcuts && next->IsShortcut()) continue
//       0x82782828    if (!lbUseAIShortcuts && next->IsAIShortcut()) continue
//       0x82782844    lfAheadness = Dot(Normalize(middle2D - portal2D), lCarBackwardsDirection)
//       0x82782A18    if (lfAheadness > best) { best = it ; liExitTo = linked ; lpBestNextPortal = portal }
//     0x82782A40  if (liExitTo == -1) return liGen
//     0x82782A54  indices[liGen].muSection = liExitTo ; r30 = lpExitSection = GetAISection(liExitTo)
//     0x82782AA4  indices[liGen].muPortal = 0xFF
//     0x82782AB8  for (each lpExitPortal of the exit section):
//                   IsSimilar(lpExitPortal->GetPosition(), lpBestNextPortal->GetPosition(), 2^-16)
//                   -- 3-D: |dx|,|dy|,|dz| all <= eps (vsubfp ; vandc128 abs ; vrlimi128 W<-X ;
//                   vcmpgtfp. ; CR6[all-false]) -> indices[liGen].muPortal = that index ; break
//     0x82782B78  if (indices[liGen].muPortal == 0xFF) return liGen - 1      (0x82782BB4)
//     0x82782B84  ++liGen ; liPreviousSection = liCurrent ; liCurrent = liExitTo
//     0x82782B90  if (liGen < liNum) loop ; else return liGen
//
// The pseudocode's argument list is empty and it labels r4 `v1`; the asm above is the map.
// =================================================================================================
s32 RacingLineGenerator::ExtrapolateRouteBackwards(s32 liNumSectionsToGenerate,
                                                   u16 luStartSectionIndex,
                                                   Vector2 lCarDirection, Vector2 lCarPosition,
                                                   const AISectionsData* lpAISectionsData,
                                                   ExtrapolatedIndexArray& lpauGeneratedIndices)
{
    (void)lCarPosition;   // v2: never read (0x82782580..0x82782BB8 copy only v1)

    CGS_ASSERT(luStartSectionIndex != KU_INVALID_SECTION_INDEX,
               CGS_ASSERT_MESSAGE_AT("luStartSectionIndex != BrnWorld::KI_INVALID_SECTION_INDEX", __FILE__, 3087));   // :3087

    s32 liCurrentSectionIndex = luStartSectionIndex;

    const AISection* lpCurrentSection = lpAISectionsData->GetAISection(luStartSectionIndex);
    const bool lbUseShortcuts   = lpCurrentSection->IsShortcut();     // :3092
    const bool lbUseAIShortcuts = lpCurrentSection->IsAIShortcut();   // :3093

    Vector2 lCarBackwardsDirection;
    __vector4 lSignMask;
    lSignMask.u[0] = lSignMask.u[1] = lSignMask.u[2] = lSignMask.u[3] = 0x80000000u;
    lCarBackwardsDirection.mV = __vxor(lCarDirection.mV, lSignMask);   // :3097

    s32 liBlockedSection     = -1;
    s32 liPreviousSection    = -1;
    s32 liStuckIterations    = 0;
    s32 liGeneratedNodeIndex = 0;

    if (liNumSectionsToGenerate <= 0)
    {
        return liGeneratedNodeIndex;
    }

    for (;;)
    {
        s16           liExitToSectionIndex = -1;
        f32           lfBestAheadnesss     = KF_AHEADNESS_SEED_MIN;
        const Portal* lpBestNextPortal     = 0;

        lpCurrentSection = lpAISectionsData->GetAISection(static_cast<u32>(liCurrentSectionIndex));

        if (lpCurrentSection->mu8NumPortals == 1)
        {
            lpauGeneratedIndices[liGeneratedNodeIndex].muPortal = 0;

            if (++liStuckIterations > KI_MAX_STUCK_ITERATIONS)
            {
                return liGeneratedNodeIndex;
            }
            if (!WalkBackToTrackBackStop(lpAISectionsData, lpauGeneratedIndices,
                                         liGeneratedNodeIndex, liCurrentSectionIndex,
                                         lpCurrentSection, liBlockedSection))
            {
                return liGeneratedNodeIndex;
            }
        }

        const u8 luNumPortals = lpCurrentSection->mu8NumPortals;
        for (u8 lPortalIndex = 0; lPortalIndex < luNumPortals; ++lPortalIndex)
        {
            const Portal* lpPossibleNextPortal = lpCurrentSection->GetPortal(lPortalIndex);
            const u32     luLinkedSectionIndex = lpPossibleNextPortal->GetLinkSectionIndex();

            if (static_cast<s32>(luLinkedSectionIndex) == liBlockedSection ||
                static_cast<s32>(luLinkedSectionIndex) == liPreviousSection)
            {
                continue;
            }

            const AISection* lpNextSection = lpAISectionsData->GetAISection(luLinkedSectionIndex);
            if (!lbUseShortcuts && lpNextSection->IsShortcut())
            {
                continue;
            }
            if (!lbUseAIShortcuts && lpNextSection->IsAIShortcut())
            {
                continue;
            }

            f32 lfAheadness;
            if (!ComputeAheadness(lpPossibleNextPortal, lpNextSection, luLinkedSectionIndex,
                                  lCarBackwardsDirection, 3211, lfAheadness))
            {
                continue;
            }

            if (lfAheadness > lfBestAheadnesss)
            {
                liExitToSectionIndex = static_cast<s16>(luLinkedSectionIndex);
                lfBestAheadnesss     = lfAheadness;
                lpBestNextPortal     = lpPossibleNextPortal;
            }
        }

        if (liExitToSectionIndex == -1)
        {
            return liGeneratedNodeIndex;
        }

        lpauGeneratedIndices[liGeneratedNodeIndex].muSection = static_cast<u32>(liExitToSectionIndex);
        const AISection* lpExitSection =
            lpAISectionsData->GetAISection(static_cast<u32>(liExitToSectionIndex));   // :3248
        lpauGeneratedIndices[liGeneratedNodeIndex].muPortal = KU_NO_PORTAL;

        // The portal of the exit section that IS the chosen portal (same 3-D position).
        const u8 luNumExitPortals = lpExitSection->mu8NumPortals;
        for (u8 luExitPortalIndex = 0; luExitPortalIndex < luNumExitPortals; ++luExitPortalIndex)
        {
            const Portal* lpExitPortal = lpExitSection->GetPortal(luExitPortalIndex);   // :3249
            const f32 lfDX = lpExitPortal->GetPositionX() - lpBestNextPortal->GetPositionX();
            const f32 lfDY = lpExitPortal->GetPositionY() - lpBestNextPortal->GetPositionY();
            const f32 lfDZ = lpExitPortal->GetPositionZ() - lpBestNextPortal->GetPositionZ();
            const bool lbSimilar = !(fabsf(lfDX) > KF_PORTAL_MATCH_EPSILON) &&
                                   !(fabsf(lfDY) > KF_PORTAL_MATCH_EPSILON) &&
                                   !(fabsf(lfDZ) > KF_PORTAL_MATCH_EPSILON);
            if (lbSimilar)
            {
                lpauGeneratedIndices[liGeneratedNodeIndex].muPortal = luExitPortalIndex;
                break;
            }
        }

        if (lpauGeneratedIndices[liGeneratedNodeIndex].muPortal == KU_NO_PORTAL)
        {
            return liGeneratedNodeIndex - 1;   // 0x82782BB4
        }

        ++liGeneratedNodeIndex;
        liPreviousSection     = liCurrentSectionIndex;
        liCurrentSectionIndex = liExitToSectionIndex;

        if (liGeneratedNodeIndex >= liNumSectionsToGenerate)
        {
            return liGeneratedNodeIndex;
        }
    }
}

// =================================================================================================
// ExtrapolateRouteForwards @0x82781AE8 (DWARF BrnRacingLineGenerator.cpp:2699)
//
//   r31 = liNumSectionsToGenerate (-> arg_14), r28 = liCurrentSectionIndex (= r4),
//   r24 = lpAISectionsData, r20 = &lpauGeneratedIndices, v125 = lCarDirection
//   var_174 = liBlockedSection = -1, var_170 = liPreviousSection = -1, var_17C = liStuck = 0,
//   r22 = liGeneratedNodeIndex = 0
//   0x82781B44  lbUseShortcuts (var_17F) / lbUseAIShortcuts (var_180) from the START section
//   0x82781B50  if (liNum <= 0) return 0
//   loop (0x82781BE8):
//     f31 = -FLT_MAX ; var_17E = liExitToSectionIndex = -1
//     0x82781C0C  indices[liGen].muPortal = 0xFF ; 0x82781C14 indices[liGen].muSection = liCurrent
//     0x82781C50  if (cur->mu8NumPortals == 1) { indices[liGen].muPortal = 0 ;
//                   if (++stuck > 2) return liGen + 1 (0x8278200C) ; walk back, not found ->
//                   return liGen + 1 }
//     0x82781D10  for (lPortalIndex < mu8NumPortals): blocked/previous skip (0x82781D28/D34),
//                 shortcut gates (0x82781D7C/D90), aheadness vs lCarDirection (0x82781F68),
//       0x82781F80  if (aheadness > best) { best ; liExitTo = linked ; indices[liGen].muPortal = i }
//     0x82781FC0  if (liExitTo == -1) return liGen
//     0x82781FC8  liPreviousSection = liCurrent ; liCurrent = liExitTo
//     0x82781FD8  if (++liGen < liNum) loop ; else return liGen
//
// Hex-Rays' 12-argument prototype is register noise; the asm map above is the shape.
// =================================================================================================
s32 RacingLineGenerator::ExtrapolateRouteForwards(s32 liNumSectionsToGenerate,
                                                  s32 liStartSectionIndex,
                                                  Vector2 lCarDirection, Vector2 lCarPosition,
                                                  const AISectionsData* lpAISectionsData,
                                                  ExtrapolatedIndexArray& lpauGeneratedIndices)
{
    (void)lCarPosition;   // v2: never read

    s32 liCurrentSectionIndex = liStartSectionIndex;
    s32 liBlockedSection      = -1;
    s32 liPreviousSection     = -1;
    s32 liStuckIterations     = 0;
    s32 liGeneratedNodeIndex  = 0;

    const AISection* lpCurrentSection =
        lpAISectionsData->GetAISection(static_cast<u32>(liStartSectionIndex));
    const bool lbUseShortcuts   = lpCurrentSection->IsShortcut();     // :2709
    const bool lbUseAIShortcuts = lpCurrentSection->IsAIShortcut();   // :2710

    if (liNumSectionsToGenerate <= 0)
    {
        return liGeneratedNodeIndex;
    }

    for (;;)
    {
        f32 lfBestAheadnesss     = KF_AHEADNESS_SEED_MIN;
        s16 liExitToSectionIndex = -1;

        lpauGeneratedIndices[liGeneratedNodeIndex].muPortal  = KU_NO_PORTAL;
        lpauGeneratedIndices[liGeneratedNodeIndex].muSection = static_cast<u32>(liCurrentSectionIndex);

        lpCurrentSection = lpAISectionsData->GetAISection(static_cast<u32>(liCurrentSectionIndex));

        if (lpCurrentSection->mu8NumPortals == 1)
        {
            lpauGeneratedIndices[liGeneratedNodeIndex].muPortal = 0;

            if (++liStuckIterations > KI_MAX_STUCK_ITERATIONS)
            {
                return liGeneratedNodeIndex + 1;
            }
            if (!WalkBackToTrackBackStop(lpAISectionsData, lpauGeneratedIndices,
                                         liGeneratedNodeIndex, liCurrentSectionIndex,
                                         lpCurrentSection, liBlockedSection))
            {
                return liGeneratedNodeIndex + 1;
            }
        }

        const u8 luNumPortals = lpCurrentSection->mu8NumPortals;
        for (u8 lPortalIndex = 0; lPortalIndex < luNumPortals; ++lPortalIndex)
        {
            const Portal* lpPossibleNextPortal = lpCurrentSection->GetPortal(lPortalIndex);
            const u32     luLinkedSectionIndex = lpPossibleNextPortal->GetLinkSectionIndex();

            if (static_cast<s32>(luLinkedSectionIndex) == liBlockedSection ||
                static_cast<s32>(luLinkedSectionIndex) == liPreviousSection)
            {
                continue;
            }

            const AISection* lpNextSection = lpAISectionsData->GetAISection(luLinkedSectionIndex);
            if (!lbUseShortcuts && lpNextSection->IsShortcut())
            {
                continue;
            }
            if (!lbUseAIShortcuts && lpNextSection->IsAIShortcut())
            {
                continue;
            }

            f32 lfAheadness;
            if (!ComputeAheadness(lpPossibleNextPortal, lpNextSection, luLinkedSectionIndex,
                                  lCarDirection, 2819, lfAheadness))
            {
                continue;
            }

            if (lfAheadness > lfBestAheadnesss)
            {
                lfBestAheadnesss     = lfAheadness;
                liExitToSectionIndex = static_cast<s16>(luLinkedSectionIndex);
                lpauGeneratedIndices[liGeneratedNodeIndex].muPortal = lPortalIndex;
            }
        }

        if (liExitToSectionIndex == -1)
        {
            return liGeneratedNodeIndex;
        }

        liPreviousSection     = liCurrentSectionIndex;
        liCurrentSectionIndex = liExitToSectionIndex;

        if (++liGeneratedNodeIndex >= liNumSectionsToGenerate)
        {
            return liGeneratedNodeIndex;
        }
    }
}

// =================================================================================================
// ExtrapolateTwistyRoute @0x82782018 (DWARF BrnRacingLineGenerator.cpp:2877)
//
// Same skeleton as Forwards (r25 = liCurrent, r21 = liGen, var_184/var_180/var_18C the blocked /
// previous / stuck slots, var_18E = liExitToSectionIndex, v125 = lCarDirection) with THREE
// differences:
//   0x82782074  only lbUseShortcuts (var_190 = flags & 1) is computed -- there is no AI-shortcut
//               gate in the portal loop (0x827822A4..0x827822D0 tests bit 0x01 only)
//   0x82782120  lfBestAheadnesss = (liGen >= 0) ? +FLT_MAX (f28 = flt_8204F664)
//                                               : -FLT_MAX (f29 = flt_82035570)
//   0x827824AC  lbKeepThisSection (:3014):
//                 liGen >= 0 : (aheadness >= 0.0f (f30 = flt_82001CC0)) && (aheadness < best)
//                 liGen <  0 : (aheadness > best)
//               i.e. the twisty walk takes the portal that turns the MOST while still pointing
//               forward. liGeneratedNodeIndex starts at 0 and only grows, so the `< 0` arm is
//               dead on the console too; it is kept because it is in the binary.
//   0x82782574  the stuck / walk-back-failed exits return liGen + 1, as Forwards.
// =================================================================================================
s32 RacingLineGenerator::ExtrapolateTwistyRoute(s32 liNumSectionsToGenerate,
                                                s32 liStartSectionIndex,
                                                Vector2 lCarDirection, Vector2 lCarPosition,
                                                const AISectionsData* lpAISectionsData,
                                                ExtrapolatedIndexArray& lpauGeneratedIndices)
{
    (void)lCarPosition;   // v2: never read

    s32 liCurrentSectionIndex = liStartSectionIndex;
    s32 liBlockedSection      = -1;
    s32 liPreviousSection     = -1;
    s32 liStuckIterations     = 0;
    s32 liGeneratedNodeIndex  = 0;

    const AISection* lpCurrentSection =
        lpAISectionsData->GetAISection(static_cast<u32>(liStartSectionIndex));
    const bool lbUseShortcuts = lpCurrentSection->IsShortcut();   // :2887

    if (liNumSectionsToGenerate <= 0)
    {
        return liGeneratedNodeIndex;
    }

    for (;;)
    {
        s16 liExitToSectionIndex = -1;
        f32 lfBestAheadnesss     = (liGeneratedNodeIndex >= 0) ? KF_AHEADNESS_SEED_MAX
                                                               : KF_AHEADNESS_SEED_MIN;

        lpauGeneratedIndices[liGeneratedNodeIndex].muPortal  = KU_NO_PORTAL;
        lpauGeneratedIndices[liGeneratedNodeIndex].muSection = static_cast<u32>(liCurrentSectionIndex);

        lpCurrentSection = lpAISectionsData->GetAISection(static_cast<u32>(liCurrentSectionIndex));

        if (lpCurrentSection->mu8NumPortals == 1)
        {
            lpauGeneratedIndices[liGeneratedNodeIndex].muPortal = 0;

            if (++liStuckIterations > KI_MAX_STUCK_ITERATIONS)
            {
                return liGeneratedNodeIndex + 1;
            }
            if (!WalkBackToTrackBackStop(lpAISectionsData, lpauGeneratedIndices,
                                         liGeneratedNodeIndex, liCurrentSectionIndex,
                                         lpCurrentSection, liBlockedSection))
            {
                return liGeneratedNodeIndex + 1;
            }
        }

        const u8 luNumPortals = lpCurrentSection->mu8NumPortals;
        for (u8 lPortalIndex = 0; lPortalIndex < luNumPortals; ++lPortalIndex)
        {
            const Portal* lpPossibleNextPortal = lpCurrentSection->GetPortal(lPortalIndex);
            const u32     luLinkedSectionIndex = lpPossibleNextPortal->GetLinkSectionIndex();

            if (static_cast<s32>(luLinkedSectionIndex) == liBlockedSection ||
                static_cast<s32>(luLinkedSectionIndex) == liPreviousSection)
            {
                continue;
            }

            const AISection* lpNextSection = lpAISectionsData->GetAISection(luLinkedSectionIndex);
            if (!lbUseShortcuts && lpNextSection->IsShortcut())
            {
                continue;
            }

            f32 lfAheadness;
            if (!ComputeAheadness(lpPossibleNextPortal, lpNextSection, luLinkedSectionIndex,
                                  lCarDirection, 3002, lfAheadness))
            {
                continue;
            }

            bool lbKeepThisSection;   // :3014
            if (liGeneratedNodeIndex >= 0)
            {
                lbKeepThisSection = (lfAheadness >= KF_AHEADNESS_FLOOR) &&
                                    (lfAheadness < lfBestAheadnesss);
            }
            else
            {
                lbKeepThisSection = (lfAheadness > lfBestAheadnesss);
            }

            if (lbKeepThisSection)
            {
                lfBestAheadnesss     = lfAheadness;
                liExitToSectionIndex = static_cast<s16>(luLinkedSectionIndex);
                lpauGeneratedIndices[liGeneratedNodeIndex].muPortal = lPortalIndex;
            }
        }

        if (liExitToSectionIndex == -1)
        {
            return liGeneratedNodeIndex;
        }

        liPreviousSection     = liCurrentSectionIndex;
        liCurrentSectionIndex = liExitToSectionIndex;

        if (++liGeneratedNodeIndex >= liNumSectionsToGenerate)
        {
            return liGeneratedNodeIndex;
        }
    }
}
}

// FOLDED FROM BrnRacingLineGenerator_GetForwardPortalIndex.cpp
// =================================================================================================
// RacingLine/BrnRacingLineGenerator_GetForwardPortalIndex.cpp  (aiwave A5, 2026-09-03)
//
//   BrnAI::RacingLineGenerator::GetForwardPortalIndex @0x827817F8  (186 insns, VMX128)
//   DWARF BrnRacingLineGenerator.h:79:
//     static uint8_t GetForwardPortalIndex(const AISectionsData*, const AISection*, Vector2, Vector2)
//
// Partfile of RacingLine/BrnRacingLineGenerator.cpp (which bodies SetupSectionExit and
// DropHardNoGoLinesIntoMap only). Split out because the committed BrnRacingLineGenerator.h
// declares neither this member nor the three Extrapolate* siblings, and that header is not this
// lane's file -- see the header_request below.
//
// ⚠️ [FLAG header_request] The class is declared TU-LOCALLY here with ONLY this static member,
// exactly the idiom BrnRouteRequestManager.cpp already uses for the same function (its sole
// caller: ComputeSectionBehind @0x827892C0). The real declaration belongs in
// RacingLine/BrnRacingLineGenerator.h:
//     static u8 GetForwardPortalIndex(const AISectionsData* lpAISectionData,
//                                     const AISection* lpAISection,
//                                     Vector2 lCarPosition, Vector2 lCarDirection);
// DELETE-WHEN that lands: replace the local declaration with `#include
// "GameSource/World/AI/RacingLine/BrnRacingLineGenerator.h"` here and in the manager TU.
//
// Register map (X360): r3 = lpAISectionData (never read), r28 = r4 = lpAISection,
// v122 = v1 = lCarPosition, v126 = v2 = lCarDirection (the DWARF order; ComputeSectionBehind
// @0x82789318..0x82789334 builds v1 = Flatten(GetPosition()), v2 = Flatten(-GetDirection())).
//
//   0x8278182C..0x827818F8  assert(!RwMath::IsZero(lCarDirection))  BrnRacingLineGenerator.cpp:2640
//                           -- the `vandc` (abs) / `vcmpgtfp` vs flt_820C3B70 lane 0 (FLT_EPSILON,
//                           read from the image) pair per lane: zero <=> |x| <= eps && |y| <= eps
//   0x827818FC..0x82781958  v123 = lCarDirection * rsqrt(x*x + y*y)   (vrsqrtefp + two
//                           Newton-Raphson refinements == an exact normalise here)
//   0x82781854              f31 = flt_820C4358 == -2.0f   (best-dot seed)
//   0x82781904/0x82781ABC   for (i = 0; i < lpAISection->mu8NumPortals (+0x14); ++i)
//   0x82781984                portal = lpAISection->GetPortal(i)
//   0x827819B8..0x827819C4    v126 = (portal.x, portal.z)   (vrlimi lane packs of the Vector3)
//   0x827819D0                d = v126 - lCarPosition
//   0x827819D4..0x82781A44    if (IsZero(d)) continue      (same eps idiom, both lanes)
//   0x82781A48..0x82781A94    dN = d * rsqrt(d.x*d.x + d.y*d.y) ; dot = dN . v123
//   0x82781AA8..0x82781AB8    if (dot > best) { best = dot ; result = i }
//   0x82781ACC              return result   (0 when no portal beat the seed)
// =================================================================================================

#include <cmath>

#include "types.hpp"
#include "BrnCommonTypes.h"                                  // Vector2 (rw vpu; .x/.y)
#include "SharedClasses/AI/AISectionsResourceType.h"        // BrnAI::AISection / AISectionsData
#include "GameSource/World/AI/BrnAIPortal.h"                // BrnAI::Portal (GetPositionX/Z)
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT

namespace BrnAI
{
namespace
{
    // flt_820C3B70 lane 0 == 0x34000000 == FLT_EPSILON (image bytes, 2026-09-03). The
    // RwMath::IsZero epsilon every VMX user of that address splats (`lvlx ; vspltw ..,0`).


    // flt_820C4358 == -2.0f: the best-dot seed (below any unit dot product).
    const f32 KF_BEST_DOT_SEED = -2.0f;





}

u8 RacingLineGenerator::GetForwardPortalIndex(const AISectionsData* lpAISectionData,
                                              const AISection* lpAISection,
                                              Vector2 lCarPosition, Vector2 lCarDirection)
{
    (void)lpAISectionData;   // r3: passed by every caller, read by nothing in the body

    CGS_ASSERT(!IsZero2D(lCarDirection.mV.x, lCarDirection.mV.y),
               "!RwMath::IsZero(lCarDirection)");   // BrnRacingLineGenerator.cpp:2640

    // The console normalises unconditionally after the (non-gating) assert; a zero direction
    // yields NaN lanes, every dot compare then fails and portal 0 is returned. Same here.
    const f32 lfDirectionInvLength =
        1.0f / sqrtf((lCarDirection.mV.x * lCarDirection.mV.x) + (lCarDirection.mV.y * lCarDirection.mV.y));
    const f32 lfDirectionX = lCarDirection.mV.x * lfDirectionInvLength;
    const f32 lfDirectionY = lCarDirection.mV.y * lfDirectionInvLength;

    u8  luBestPortalIndex = 0;
    f32 lfBestDot         = KF_BEST_DOT_SEED;

    const u8 luNumPortals = lpAISection->mu8NumPortals;
    for (u8 luPortalIndex = 0; luPortalIndex < luNumPortals; ++luPortalIndex)
    {
        const Portal* lpPortal = lpAISection->GetPortal(luPortalIndex);

        // Flattened portal position (x, z) minus the car's flattened position.
        const f32 lfDeltaX = lpPortal->GetPositionX() - lCarPosition.mV.x;
        const f32 lfDeltaY = lpPortal->GetPositionZ() - lCarPosition.mV.y;

        if (IsZero2D(lfDeltaX, lfDeltaY))
        {
            continue;
        }

        const f32 lfDeltaInvLength = 1.0f / sqrtf((lfDeltaX * lfDeltaX) + (lfDeltaY * lfDeltaY));
        const f32 lfDot = (lfDeltaX * lfDeltaInvLength * lfDirectionX)
                        + (lfDeltaY * lfDeltaInvLength * lfDirectionY);

        if (lfDot > lfBestDot)
        {
            lfBestDot         = lfDot;
            luBestPortalIndex = luPortalIndex;
        }
    }

    return luBestPortalIndex;
}
}

// FOLDED FROM BrnRacingLineGenerator_Query.cpp
// =================================================================================================
// RacingLine/BrnRacingLineGenerator_Query.cpp  (aiwave2 lane R2, 2026-09-05)
//
// Partfile of RacingLine/BrnRacingLineGenerator.cpp: the QUERY half of BrnAI::RacingLineGenerator
// -- the five members the AIDriver / SteeringFan call once the section cache exists, plus the four
// DWARF-declared helpers the console inlined into them.
//
//   BODIED (exported)
//     InitialiseRacingLine         @0x8278FB20   (AIDriver::InitialiseRacingLine @0x82792DF0)
//     GetPointFarAhead             @0x827900A0   (SteeringFan::CachePointAhead, AIDriver::
//                                                 FindPositionInFuture / FindFinalDriftDirection)
//     GetRoadPositionAsPercentage  @0x8278FFB0   (AIDriver::HardShoulderSpeed @0x827930B8)
//     SpreadHNGBackOneStep         @0x8278F680   (AIDriver::DoRoundRobinWork  @0x82796340)
//     RaceLineDefaultsToSlamPlayer @0x82777730   (AIDriver::GenerateRacingLine @0x8277C4E8)
//   BODIED (no export -- recovered from the inlined blocks inside the five above, written ONCE)
//     GetPerpendicularDistanceToCentreLine     (DWARF BrnRacingLineGenerator.h:102) -- inlined
//                                  twice, at 0x8278FFDC..0x8279004C and 0x827777D8..0x82777814
//     FinishedSpreadingBack                    (:363) -- inlined THREE times inside
//                                  SpreadHNGBackOneStep (0x8278F768, 0x8278F7A8, 0x8278F82C)
//     HasSpreadHardNoGoLinesFinished           (:368) -- the 32-entry scan @0x8278F7F4..0x8278F824
//     SpreadHardNoGoLinesThroughCurrentSection (:372) -- the miBackwardsStep == 0 arm
//                                  @0x8278F70C..0x8278F740
//   PARKED (see the park banner at the foot of this file)
//     RenderHardNoGoMap @0x82790278, DrawCentreLine (:203), RenderSectionDetails (:208),
//     FindTargetFixedDistanceAhead (:96), FindAnyTargetOnLineAhead (:248),
//     RaceDefaultsLineToAutoCentre (:107), RaceLineDefaultsToHoldingRoadPosition (:117),
//     MoveRaceLineToSlamPlayer (:122).
//
// (RESOLVED 2026-09-05: the class is now taken from BrnRacingLineGenerator.h; the note below is historical) THE CLASS WAS DECLARED TU-LOCALLY, exactly the idiom the sibling
// partfile RacingLine/BrnRacingLineGenerator_GetForwardPortalIndex.cpp already uses. The host
// RacingLine/BrnRacingLineGenerator.h (lane R1's file this wave) declares this lane's five entry
// points but NOT the section-cache helpers they call, and this lane owns no header -- so including
// it would not compile. Every declaration below is the DecFIGS DWARF prototype verbatim
// (references/DecFIGS/dwarfdump/.../BrnRacingLineGenerator.h) at the DWARF's OWN access, because
// MSVC mangles member access into the symbol name. DELETE-WHEN lane R1's header grow lands:
// replace this block with `#include "GameSource/World/AI/RacingLine/BrnRacingLineGenerator.h"`.
//
// SOURCE FILE / LINE NUMBERS: every assert this TU reproduces cites
// "d:\p4\b5_main\burnout\main\code\gamesource\unity\../World/AI/RacingLine/BrnRacingLineGenerator.cpp"
// -- the console line number is quoted at each site. The X360 retail build DOES emit these
// (BeginAssert / FireAssert / EndAssert are in the listing), so they are reproduced; the
// StrStream-formatted asserts elsewhere in this class are compiled out and are not.
// =================================================================================================

#include <cmath>

#include "types.hpp"
#include "BrnCommonTypes.h"                                               // Vector2 / Vector3 (vpu)
#include "rw/math/vpu/vector3_operation.h"                            // rw::math::vpu::IsValid
#include "GameSource/World/AI/Route/BrnRacingLine.h"                      // RacingLine + SectionData
#include "GameSource/World/AI/RacingLine/BrnRacingLineGenerator.h"     // RacingLineGenerator (lane R1 header)
#include "GameSource/World/AI/BrnAIUtils.h"                                // BrnAI::DistancePointToLine
#include "GameSource/World/AI/Route/BrnRoute.h"                           // Route::GetNodeCount/GetStatus
#include "GameSource/World/AI/BrnAICar.h"                                 // AICar::GetPosition / GetRoute
#include "SharedClasses/AI/AISectionsResourceType.h"                      // AISectionsData / AISection
#include "GameSource/Math/BrnMathUtils.h"                                 // BrnMath::Flatten
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // AddMonitor/Start/Stop

namespace BrnAI
{
// (The TU-local RacingLineGenerator declaration that stood here was replaced by the real header
//  once lane R1 landed it -- conductor, 2026-09-05.)

// (RESOLVED 2026-09-05: declared in BrnAIUtils.h, bodied in BrnAIUtils.cpp) BrnAI::DistancePointToLine -- DWARF GameSource/World/AI/BrnAIUtils.h:7,
// X360 @0x8276DDB8 (its own assert cites "..\..\..\GameSource\World/AI/BrnAIUtils.cpp":117,
// "Bad maths! Point = ..., Start = ..., End = ..."). The host BrnAIUtils.h deliberately declares
// only the helpers already homed, and this lane owns no header, so it is declared here.
// It has NO BODY anywhere in the tree yet -- see this lane's report (`## risks`), which carries
// the fully decoded body for BrnAIUtils.cpp. DELETE-WHEN it is declared in BrnAIUtils.h.

namespace
{
    // AddMonitor page argument: `li r4, 7` at every one of the six call sites in
    // InitialiseRacingLine. CgsDev::PerfMonCpuPage only spells out the pages earlier waves
    // needed; 7 is inside the enum's range (E_PMP_MAX == 24), so the cast is well defined.
    // [FLAG header_request] add `E_PMP_7 = 7` to CgsPerfMonCpu.h and use it here instead.
    const CgsDev::PerfMonCpuPage KE_RACING_LINE_PERFMON_PAGE =
        static_cast<CgsDev::PerfMonCpuPage>(7);

    // AddMonitor CPU budgets, read from the image: flt_820C4150 == 10.0f for the top-level
    // "Generate Racing Line" monitor, flt_820C41F4 == 2.0f for the other five.
    const f32 KF_PERFMON_BUDGET_GENERATE = 10.0f;
    const f32 KF_PERFMON_BUDGET_STEP     = 2.0f;

    // `cmpwi r11, 0x10` in InitialiseRacingLine @0x8278FDC8: at most sixteen route nodes are ever
    // cached, i.e. KI_RACING_LINE_MAX_AVAILABLE_SECTIONS.
    const s32 KI_MAX_SECTIONS_TO_CACHE = KI_RACING_LINE_MAX_AVAILABLE_SECTIONS;

    // `cmpwi r11, 7` / `bgt` @0x8278FF84: InitialiseRacingLine stops setting up targets once it
    // has done eight non-junction sections in one call (later frames finish the rest).
    const s32 KI_MAX_NON_JUNCTION_SETUPS = 7;

    // `cmpwi r11, 0x20` @0x8278F814 -- one stretch-distance slot per hard-no-go map COLUMN
    // (DWARF BrnHardNoGoMap.h:30, KI_HNG_MAP_WIDTH == 32), matching
    // RacingLine::maStretchDistanceForHNG[32].
    const s32 KI_HNG_STRETCH_COUNT = 32;

    // `lfs f1, flt_820C4168` == 0.5f -- GetPointFarAhead measures every section's length across
    // the middle of the map (height interpolant 0.5).
    const f32 KF_SECTION_LENGTH_HEIGHT = 0.5f;
}

// The six PerfMon handles: ONE definition, here (see the header_request above).

// =================================================================================================
// GetPerpendicularDistanceToCentreLine -- DWARF BrnRacingLineGenerator.h:102. NO IDA export; the
// console inlines it whole into the two bodies below, and the two copies are identical:
//
//   0x8278FFDC / 0x827777DC  lbz 0xBD1(lpRacingLine)      -- mbCentreLineHereKnown
//   0x82790034 / 0x82777800  lvx 0xBE0 -> v2, 0xBF0 -> v3 -- mCentreHere, mCentreAhead
//   0x82790048 / 0x82777810  bl  sub_8276DDB8              -- DistancePointToLine(v1, v2, v3)
//   0x8279002C / 0x827777F8  the "not known" arm supplies the CALLER's own default: 0.0f in
//                            GetRoadPositionAsPercentage, mfRoadPlacement (lfs 0xB24) in
//                            RaceLineDefaultsToSlamPlayer -- which is exactly what the DWARF's
//                            third (float32_t) parameter is for.
// =================================================================================================
f32 RacingLineGenerator::GetPerpendicularDistanceToCentreLine(RacingLine* lpRacingLine,
                                                              Vector2 lPos2D,
                                                              f32 lfDefault)
{
    if (!lpRacingLine->mbCentreLineHereKnown)
        return lfDefault;

    return DistancePointToLine(lPos2D, lpRacingLine->mCentreHere, lpRacingLine->mCentreAhead);
}

// =================================================================================================
// InitialiseRacingLine @0x8278FB20 (168 instructions)
//
// Register map: r3/r25 = this, r4/r30 = lpRacingLine, r5/r23 = lpCar, r6/r22 = liSectionIndex,
// r7/r21 = lpSectionsData -- i.e. the DWARF (:75) order (RacingLine*, AICar*, int32_t,
// AISectionsData*). The asserts NAME the second argument `lpRoute` because AICar embeds its Route
// at offset 0 (BrnAICar.h: GetRoute() == reinterpret_cast<Route*>(this)), so `lwz 0x1408(r23)`
// (Route::meStatus) and `bl AICar::GetPosition(r4 = r23)` really are the same pointer. That is the
// one place the pseudocode misleads -- it renders a3 as a bare int serving both roles.
//
//   0x8278FB48..0x8278FBDC  the four asserts (:1196 / :1197 / :1201 / :1202)
//   0x8278FBE0              lCarPosition = lpCar->GetPosition() -- only .y is used, hoisted into
//                           f31 @0x8278FE40 BEFORE the second loop (the compiler then reuses that
//                           stack slot for a GetRouteCentre out-parameter)
//   0x8278FBEC..0x8278FDB4  the one-shot PerfMon registration and its five `>= 0` asserts
//   0x8278FDB8              stb 0, 0xBD0 -- SetInitialised(false)
//   0x8278FDC0..0x8278FDF4  first / last section in cache, from the route's node count
//   0x8278FDF8              CacheLocalSections(lpRacingLine, lpCar->GetRoute(), lpSectionsData,
//                                              lpCar)   (r5 and r7 are the SAME register, r23)
//   0x8278FE10..0x8278FE2C  SetUpHardNoGoMap over the whole cache
//   0x8278FE44..0x8278FF98  the target-setup pass
//   0x8278FF9C              stb 1, 0xBD0 -- SetInitialised(true)
// =================================================================================================
void RacingLineGenerator::InitialiseRacingLine(RacingLine* lpRacingLine, AICar* lpCar,
                                               s32 liSectionIndex, AISectionsData* lpSectionsData)
{
    CGS_ASSERT(lpRacingLine != 0, CGS_ASSERT_MESSAGE_AT("lpRacingLine", __FILE__, 1196));      // :1196
    CGS_ASSERT(lpSectionsData != 0, CGS_ASSERT_MESSAGE_AT("lpSectionsData", __FILE__, 1197));  // :1197

    const Route* lpRoute = lpCar->GetRoute();
    CGS_ASSERT(lpRoute != 0, CGS_ASSERT_MESSAGE_AT("lpRoute", __FILE__, 1201));                                                // :1201
    CGS_ASSERT(lpRoute->GetStatus() != Route::E_STATUS_UNINITIALISED,
               CGS_ASSERT_MESSAGE_AT("lpRoute->IsInitialised()", __FILE__, 1202));                                             // :1202

    const Vector3 lCarPosition = lpCar->GetPosition();

    if (miGenerateRacingLinePM == -1)
    {
        miGenerateRacingLinePM = CgsDev::PerfMonCpu::AddMonitor("Generate Racing Line",
                                                                KE_RACING_LINE_PERFMON_PAGE, false,
                                                                KF_PERFMON_BUDGET_GENERATE, true);
        miGenerateInSectionPM  = CgsDev::PerfMonCpu::AddMonitor("In Section",
                                                                KE_RACING_LINE_PERFMON_PAGE, false,
                                                                KF_PERFMON_BUDGET_STEP, true);
        miHNGMapGenerationPM1  = CgsDev::PerfMonCpu::AddMonitor("HNG Map dropping",
                                                                KE_RACING_LINE_PERFMON_PAGE, false,
                                                                KF_PERFMON_BUDGET_STEP, true);
        miHNGMapGenerationPM2  = CgsDev::PerfMonCpu::AddMonitor("HNG Map spread current",
                                                                KE_RACING_LINE_PERFMON_PAGE, false,
                                                                KF_PERFMON_BUDGET_STEP, true);
        miHNGMapGenerationPM3  = CgsDev::PerfMonCpu::AddMonitor("HNG Map spread previous",
                                                                KE_RACING_LINE_PERFMON_PAGE, false,
                                                                KF_PERFMON_BUDGET_STEP, true);
        miFarAheadPM           = CgsDev::PerfMonCpu::AddMonitor("Far ahead scan",
                                                                KE_RACING_LINE_PERFMON_PAGE, false,
                                                                KF_PERFMON_BUDGET_STEP, true);

        CGS_ASSERT(miGenerateRacingLinePM >= 0, CGS_ASSERT_MESSAGE_AT("miGenerateRacingLinePM >= 0", __FILE__, 1216));   // :1216
        CGS_ASSERT(miGenerateInSectionPM  >= 0, CGS_ASSERT_MESSAGE_AT("miGenerateInSectionPM >= 0", __FILE__, 1217));    // :1217
        CGS_ASSERT(miHNGMapGenerationPM1  >= 0, CGS_ASSERT_MESSAGE_AT("miHNGMapGenerationPM1 >= 0", __FILE__, 1218));    // :1218
        CGS_ASSERT(miHNGMapGenerationPM2  >= 0, CGS_ASSERT_MESSAGE_AT("miHNGMapGenerationPM2 >= 0", __FILE__, 1219));    // :1219
        CGS_ASSERT(miHNGMapGenerationPM3  >= 0, CGS_ASSERT_MESSAGE_AT("miHNGMapGenerationPM3 >= 0", __FILE__, 1220));    // :1220
        // (the console emits FIVE of these, not six -- miFarAheadPM is not asserted.)
    }

    lpRacingLine->SetInitialised(false);                                          // stb 0, 0xBD0

    // lwz 0x1400(route) - liSectionIndex, clamped to sixteen: the cache holds at most
    // KI_RACING_LINE_MAX_AVAILABLE_SECTIONS nodes and never runs past the end of the route.
    s32 liSectionsToCache = lpRoute->GetNodeCount() - liSectionIndex;
    if (liSectionsToCache >= KI_MAX_SECTIONS_TO_CACHE)
        liSectionsToCache = KI_MAX_SECTIONS_TO_CACHE;

    lpRacingLine->mFirstSectionInCache = liSectionIndex;
    lpRacingLine->mLastSectionInCache  = liSectionIndex + liSectionsToCache - 1;

    // CacheLocalSections returns the section the cache actually starts at.
    lpRacingLine->mFirstSectionInCache =
        CacheLocalSections(lpRacingLine, lpRoute, lpSectionsData, lpCar);

    for (s32 liSection = lpRacingLine->mFirstSectionInCache;
         liSection <= lpRacingLine->mLastSectionInCache;
         ++liSection)
    {
        SetUpHardNoGoMap(lpRacingLine, liSection);
    }

    // f31 is loaded ONCE, before the loop, from the car position's height lane.
    const f32 lfCarHeight = lCarPosition.mV.y;

    Vector2 lRouteCentreAhead;
    Vector2 lRouteCentreBehind;

    for (s32 liSection = lpRacingLine->mFirstSectionInCache;
         liSection <= lpRacingLine->mLastSectionInCache;
         ++liSection)
    {
        if (IsTargetUpToDate(lpRacingLine, liSection))
            continue;

        if (IsAJunction(lpRacingLine, liSection))
        {
            // A junction's exit is where the straight stretches either side of it cross.
            const s32 liStraightAhead  = LookForStraightSectionAhead(lpRacingLine, liSection);
            const s32 liStraightBehind = LookForStraightSectionBehind(lpRacingLine, liSection);

            GetRouteCentre(lpRacingLine, liStraightAhead,  lRouteCentreAhead);
            GetRouteCentre(lpRacingLine, liStraightBehind, lRouteCentreBehind);

            if (liSection == liStraightAhead || liSection == liStraightBehind)
            {
                // No straight to project from on one side -- fall back to the map's own exit.
                SetupSectionExit(lpRacingLine, liSection);
            }
            else
            {
                // v1 = the BEHIND centre (var_80), v2 = the AHEAD centre (var_90), f1 = the car's
                // height (0x8278FEFC..0x8278FF1C).
                CalculateIntersectionWithProjectedRoute(lpRacingLine, liSection,
                                                        lRouteCentreBehind, lRouteCentreAhead,
                                                        lfCarHeight);
            }

            SetTargetUpToDate(lpRacingLine, liSection);
            SetUpIncomingPortalTarget(lpRacingLine, liSection);
        }
        else
        {
            SetupSectionExit(lpRacingLine, liSection);
            SetTargetUpToDate(lpRacingLine, liSection);
            SetUpIncomingPortalTarget(lpRacingLine, liSection);

            // Budget: at most eight ordinary sections get their target set up in one call.
            if (liSection - lpRacingLine->mFirstSectionInCache > KI_MAX_NON_JUNCTION_SETUPS)
                break;
        }
    }

    lpRacingLine->SetInitialised(true);                                           // stb 1, 0xBD0
}

// =================================================================================================
// GetRoadPositionAsPercentage @0x8278FFB0 (59 instructions)
//
// r3/r30 = this, r4/r31 = lpRacingLine, r5 = lpCar. How far off the centre line the car is, as a
// fraction of the half road width -- AIDriver::HardShoulderSpeed ramps its 75 % speed limiter on
// it above 0.85.
//
//   0x8278FFDC  lbz 0xBD0 -- not initialised -> 0.0f (flt_82001CC0)
//   0x8278FFFC  AICar::GetPosition, then the two vrlimi128 lane packs (mask 8 shift 0 = .x,
//               mask 4 shift 1 = .z into .y) == BrnMath::Flatten: world (x, Z) -> 2D (x, y)
//   0x8279000C  the inlined GetPerpendicularDistanceToCentreLine, default 0.0f
//   0x8279005C  GetHalfRoadWidthHere(lpRacingLine, lCarPos2D)
//   0x82790060  fcmpu vs 0.0 -- an exactly-zero half width returns 0.0f, no divide
//   0x82790068  fdivs then fabs
// =================================================================================================
f32 RacingLineGenerator::GetRoadPositionAsPercentage(RacingLine* lpRacingLine, AICar* lpCar)
{
    if (!lpRacingLine->mbIsInitialised)
        return 0.0f;

    const Vector2 lCarPos2D = BrnMath::Flatten(lpCar->GetPosition());

    const f32 lfPerpendicularDistance =
        GetPerpendicularDistanceToCentreLine(lpRacingLine, lCarPos2D, 0.0f);

    const f32 lfHalfRoadWidth = GetHalfRoadWidthHere(lpRacingLine, lCarPos2D);
    if (lfHalfRoadWidth == 0.0f)
        return 0.0f;

    return std::fabs(lfPerpendicularDistance / lfHalfRoadWidth);
}

// =================================================================================================
// RaceLineDefaultsToSlamPlayer @0x82777730 (44 instructions)
//
// r3 = this, r4/r31 = lpRacingLine, v1/v127 = lSlamTarget3D. Point the racing line's default
// perpendicular offset at the slam target: AIDriver::GenerateRacingLine calls it while the
// aggression machine holds a valid target, then (in ATTACK_SLAM) copies the offset into
// mfRoadPlacement.
//
//   0x82777754..0x827777A8  the three-lane vspltw128 + vcmpeqfp. self-equality cascade ==
//                           rw::math::vpu::IsValid(Vector3); assert :3297 IS emitted in retail
//   0x827777E4..0x827777F0  the same two vrlimi128 lane packs as above == BrnMath::Flatten
//   0x827777DC..0x82777810  the inlined GetPerpendicularDistanceToCentreLine, whose "not known"
//                           default is mfRoadPlacement (lfs 0xB24)
//   0x82777818 / 0x8277781C stfs 0xB10 / stb 1, 0xB14
// =================================================================================================
void RacingLineGenerator::RaceLineDefaultsToSlamPlayer(RacingLine* lpRacingLine,
                                                       Vector3 lSlamTarget3D)
{
    CGS_ASSERT(rw::math::vpu::IsValid(lSlamTarget3D), CGS_ASSERT_MESSAGE_AT("RwMath::IsValid( lSlamTarget3D )", __FILE__, 3297));  // :3297

    const Vector2 lSlamTarget2D = BrnMath::Flatten(lSlamTarget3D);

    lpRacingLine->mfDefaultPerpendicularOffset =
        GetPerpendicularDistanceToCentreLine(lpRacingLine, lSlamTarget2D,
                                             lpRacingLine->mfRoadPlacement);
    lpRacingLine->mbDefiniteDestination = true;
}

// =================================================================================================
// FinishedSpreadingBack -- DWARF BrnRacingLineGenerator.h:363. No export; inlined three times in
// SpreadHNGBackOneStep (0x8278F768..0x8278F780, 0x8278F7A8..0x8278F7C4, 0x8278F82C..0x8278F848),
// each copy the identical store triple: miSectionToSpread + 1, miBackwardsStep = -1,
// miHNGLineStart = 0 -- move the spread cursor on to the next section, from scratch.
// =================================================================================================
void RacingLineGenerator::FinishedSpreadingBack(RacingLine* lpRacingLine)
{
    ++lpRacingLine->miSectionToSpread;
    lpRacingLine->miBackwardsStep = -1;
    lpRacingLine->miHNGLineStart  = 0;
}

// =================================================================================================
// HasSpreadHardNoGoLinesFinished -- DWARF :368. No export; the scan at 0x8278F7F4..0x8278F824:
// walk maStretchDistanceForHNG[0..31] and stop at the FIRST slot that is >= 0.0f (fcmpu / bge).
// The spread is finished only once every one of the 32 columns has gone negative, i.e. the
// hard-no-go lines have been carried back past the spread distance everywhere.
// =================================================================================================
bool RacingLineGenerator::HasSpreadHardNoGoLinesFinished(RacingLine* lpRacingLine)
{
    for (s32 liColumn = 0; liColumn < KI_HNG_STRETCH_COUNT; ++liColumn)
    {
        // NaN polarity (FX-AINAN2): `fcmpu stretch, 0.0 ; bge -> not finished` @0x8278F804/
        // 0x8278F808 is taken on an unordered compare -- a NaN column still counts as budget.
        if (!(lpRacingLine->maStretchDistanceForHNG[liColumn] < 0.0f))
            return false;
    }
    return true;
}

// =================================================================================================
// SpreadHardNoGoLinesThroughCurrentSection -- DWARF :372. No export; the miBackwardsStep == 0 arm
// at 0x8278F70C..0x8278F740: seed the stretch distances by spreading this section's OWN map along
// the track, then step the backwards cursor.
//   0x8278F724  GetSectionPointer(lpRacingLine, miSectionToSpread)
//   0x8278F72C  r3 = section + 0x50 == &SectionData::mHardNoGoMap
//   0x8278F728  r4 = lpRacingLine + 0xB40 == maStretchDistanceForHNG
//   0x8278F730  f1 = lfs 0xBCC == mfSpreadDistance
// =================================================================================================
void RacingLineGenerator::SpreadHardNoGoLinesThroughCurrentSection(RacingLine* lpRacingLine)
{
    SectionData* lpCurrentSectionData =
        GetSectionPointer(lpRacingLine, lpRacingLine->miSectionToSpread);

    lpCurrentSectionData->mHardNoGoMap.SpreadHNGAlongTrack(lpRacingLine->maStretchDistanceForHNG,
                                                           lpRacingLine->mfSpreadDistance);
    ++lpRacingLine->miBackwardsStep;
}

// =================================================================================================
// SpreadHNGBackOneStep @0x8278F680 (125 instructions)
//
// r3/r29 = this, r4/r31 = lpRacingLine. One slice of the round-robin hard-no-go work
// (AIDriver::DoRoundRobinWork runs up to four per frame). Returns TRUE when there is nothing left
// to do -- that is the caller's loop-exit answer, not an error.
//
// The cursor triple is (miSectionToSpread 0xBC0, miBackwardsStep 0xBC4, miHNGLineStart 0xBC8), all
// three reset by RacingLine::ClearSectionCache. miBackwardsStep sequences the work for one
// section: -1 = still building that section's own map, 0 = spread it along the track, n > 0 =
// carry it back into the n'th section behind.
//
//   0x8278F694  lbz 0xBD0 -- not initialised -> true
//   0x8278F6AC  miSectionToSpread > mLastSectionInCache -> true
//   0x8278F6BC  miSectionToSpread < mFirstSectionInCache -> snap it forward
//   0x8278F6CC  == -1 -> DropHardNoGoLinesIntoMap, bracketed by miHNGMapGenerationPM1
//   0x8278F704  ==  0 -> SpreadHardNoGoLinesThroughCurrentSection, bracketed by ...PM2
//   0x8278F758  (miSectionToSpread - miBackwardsStep) has walked off the front of the cache ->
//               FinishedSpreadingBack
//   0x8278F79C  lbz 0x98(section) == mHardNoGoMap.mbReady -- that section's map is not built yet
//               -> FinishedSpreadingBack
//   0x8278F7D0  SpreadHNGIntoPreviousSection, bracketed by ...PM3, then either
//               FinishedSpreadingBack (all 32 stretch columns exhausted) or one more step back
// =================================================================================================
bool RacingLineGenerator::SpreadHNGBackOneStep(RacingLine* lpRacingLine)
{
    if (!lpRacingLine->mbIsInitialised)
        return true;

    if (lpRacingLine->miSectionToSpread > lpRacingLine->mLastSectionInCache)
        return true;

    if (lpRacingLine->miSectionToSpread < lpRacingLine->mFirstSectionInCache)
        lpRacingLine->miSectionToSpread = lpRacingLine->mFirstSectionInCache;

    if (lpRacingLine->miBackwardsStep == -1)
    {
        CgsDev::PerfMonCpu::StartMonitor(miHNGMapGenerationPM1);
        DropHardNoGoLinesIntoMap(lpRacingLine);
        CgsDev::PerfMonCpu::StopMonitor(miHNGMapGenerationPM1);
        return false;
    }

    if (lpRacingLine->miBackwardsStep == 0)
    {
        CgsDev::PerfMonCpu::StartMonitor(miHNGMapGenerationPM2);
        SpreadHardNoGoLinesThroughCurrentSection(lpRacingLine);
        CgsDev::PerfMonCpu::StopMonitor(miHNGMapGenerationPM2);
        return false;
    }

    const s32 liPreviousSection = lpRacingLine->miSectionToSpread - lpRacingLine->miBackwardsStep;
    if (liPreviousSection < lpRacingLine->mFirstSectionInCache)
    {
        FinishedSpreadingBack(lpRacingLine);
        return false;
    }

    SectionData* lpPreviousSectionData = GetSectionPointer(lpRacingLine, liPreviousSection);
    if (!lpPreviousSectionData->mHardNoGoMap.IsReady())
    {
        FinishedSpreadingBack(lpRacingLine);
        return false;
    }

    CgsDev::PerfMonCpu::StartMonitor(miHNGMapGenerationPM3);
    lpPreviousSectionData->mHardNoGoMap.SpreadHNGIntoPreviousSection(
        lpRacingLine->maStretchDistanceForHNG);
    CgsDev::PerfMonCpu::StopMonitor(miHNGMapGenerationPM3);

    if (HasSpreadHardNoGoLinesFinished(lpRacingLine))
        FinishedSpreadingBack(lpRacingLine);
    else
        ++lpRacingLine->miBackwardsStep;

    return false;
}

// =================================================================================================
// GetPointFarAhead @0x827900A0 (118 instructions)
//
// r3/r29 = this, r4/r31 = lpRacingLine, f1/f29 = lfDistanceAhead, v1/v127 = lFrom2D,
// r6/r25 = &lrOutPosition, r7/r26 = &lrOutDirection. NOTE r5 IS SKIPPED: the float argument
// consumes its GPR slot, which is why the two reference outs land in r6/r7 (COMMON_BRIEF rule 4).
//
// Walk forward along the cached racing line until lfDistanceAhead metres have been covered, then
// return the point on that section's curve (pushed sideways by mfRoadPlacement) and the curve's
// tangent there. Returns false whenever the walk runs out of cache -- which is exactly what every
// caller (SteeringFan::CachePointAhead, AIDriver::FindPositionInFuture /
// FindFinalDriftDirection) reads as "no point ahead is known".
//
//   0x827900DC  StartMonitor(miFarAheadPM) -- the WHOLE body is bracketed, both exits included
//   0x827900F0  GetLocalSectionID(lpRacingLine, lFrom2D, miLastKnownSectionID (lwz 0xC00))
//   0x827900FC  == 0x7FFF (KI_INVALID_SECTION_INDEX) -> fail
//   0x82790118  lbz 0x98(section) == mHardNoGoMap.mbReady (SectionData +0x50 plus 0x48) -> fail
//   0x82790134  GetSectionInterpPosition(lpRacingLine, liSectionID, lFrom2D)
//   0x8279014C  SectionLength(0.5f)
//   0x82790154  fmadds: remaining = sectionLength * interp + lfDistanceAhead -- measured from the
//               START of the current section, not from the car
//   0x82790160..0x827901A0  step forward one section at a time, subtracting each section's length,
//               until one is longer than what is left (fcmpu / bgt -> found)
//   0x827901CC  fdivs then the fsel pair == saturate(remaining / sectionLength) into [0, 1]
//   0x82790204  GetPointAndNormalOnCurve(lpRacingLine, liSectionID, interp, lPoint (r7),
//                                        lNormal (r8))
//   0x82790250  vmaddfp, read in RAW FIELD ORDER vD = vA*vC + vB (tools/re/vmx128.py banner):
//               outPosition = lNormal * splat(mfRoadPlacement) + lPoint
//   0x8279025C  the two vrlimi128 writes touch ONLY lanes x and y of *lpOutDirection:
//               x = lNormal.y, y = -lNormal.x (v13 is lNormal.x XORed with the 0x80000000 sign
//               mask built by vspltisw -1 / vslw) -- the normal turned 90 degrees, i.e. the
//               tangent along the line.
// =================================================================================================
bool RacingLineGenerator::GetPointFarAhead(RacingLine* lpRacingLine, f32 lfDistanceAhead,
                                           Vector2 lFrom2D, Vector2& lrOutPosition,
                                           Vector2& lrOutDirection)
{
    CgsDev::PerfMonCpu::StartMonitor(miFarAheadPM);

    s32 liSectionID = GetLocalSectionID(lpRacingLine, lFrom2D, lpRacingLine->miLastKnownSectionID);
    if (liSectionID == AICar::KI_INVALID_SECTION_INDEX)
    {
        CgsDev::PerfMonCpu::StopMonitor(miFarAheadPM);
        return false;
    }

    SectionData* lpSectionData = GetSectionPointer(lpRacingLine, liSectionID);
    if (!lpSectionData->mHardNoGoMap.IsReady())
    {
        CgsDev::PerfMonCpu::StopMonitor(miFarAheadPM);
        return false;
    }

    const f32 lfInterpInSection = GetSectionInterpPosition(lpRacingLine, liSectionID, lFrom2D);
    f32 lfDistanceRemaining =
        lpSectionData->mHardNoGoMap.SectionLength(KF_SECTION_LENGTH_HEIGHT) * lfInterpInSection
        + lfDistanceAhead;

    f32  lfSectionLength = 0.0f;
    bool lbFoundSection  = false;

    while (liSectionID <= lpRacingLine->mLastSectionInCache)
    {
        SectionData* lpWalkSectionData = GetSectionPointer(lpRacingLine, liSectionID);
        if (!lpWalkSectionData->mHardNoGoMap.IsReady())
            break;

        lfSectionLength = lpWalkSectionData->mHardNoGoMap.SectionLength(KF_SECTION_LENGTH_HEIGHT);
        if (lfSectionLength > lfDistanceRemaining)
        {
            lbFoundSection = true;
            break;
        }

        lfDistanceRemaining -= lfSectionLength;
        ++liSectionID;
    }

    if (!lbFoundSection)
    {
        CgsDev::PerfMonCpu::StopMonitor(miFarAheadPM);
        return false;
    }

    // ⭐⭐⭐ THE CLAMP IS AN `fsel` PAIR AND ITS NaN ARM IS LOAD-BEARING (fixed 2026-09-15,
    // issue #24). ARTIST 0x827901CC..0x82790200, verbatim:
    //     fdivs f0, f31, f1              ; interp = remaining / sectionLength
    //     lfs   f13, flt_82001CC0        ; 0.0
    //     fneg  f12, f0                  ; -interp
    //     fsel  f0, f12, f13, f0         ; FRT,FRA,FRC,FRB -> (-interp >= 0) ? 0.0 : interp
    //     lfs   f13, flt_82001C98        ; 1.0
    //     fsubs f12, f13, f0             ; 1.0 - interp
    //     fsel  f1, f12, f0, f13         ; (1.0 - interp >= 0) ? interp : 1.0
    // PowerPC `fsel FRT,FRA,FRC,FRB` is "if (FRA) >= 0.0 then FRC else FRB", and the >= is FALSE
    // for a NaN FRA -- so a NaN interp survives the FIRST fsel (FRB is interp) and is turned into
    // **1.0** by the SECOND (FRB is 1.0). That second arm is the console's NaN CONTAINMENT for the
    // whole hermite stack.
    // ⛔ The previous `if (x <= 0) x = 0; if (x > 1) x = 1;` form is NOT that program: in C++ both
    // comparisons are false for a NaN, so a NaN interp was passed straight into
    // GetPointAndNormalOnCurve.
    // ⚠️ REPRODUCIBLE IS NOT ATTRIBUTABLE -- MEASURED, AND SAID OUT LOUD. Fixing THIS site alone
    // (issue #24 run 4) changed nothing: 53,522 asserts and a per-site histogram identical to the
    // runs before it, and GetPointAndNormalOnCurve's own "Bad interp" (.cpp:2148) never fires at
    // all. The storm comes through the OTHER caller of the same hermite pair,
    // GetCentreCentreLineHere @0x8278E930, which carried the identical mistranslation; see the
    // banner there. This site is corrected because the console's program says so, not because it
    // was the cause of anything observed.
    // Written as the two explicit `>= 0.0f` tests so the NaN behaviour is the ternary's own
    // (a NaN comparison is false in C++ too, so each line IS its fsel).
    const f32 lfRawInterp = lfDistanceRemaining / lfSectionLength;
    f32 lfInterp = (-lfRawInterp >= 0.0f) ? 0.0f : lfRawInterp;   // fsel f0, f12, f13, f0
    lfInterp    = ((1.0f - lfInterp) >= 0.0f) ? lfInterp : 1.0f;  // fsel f1, f12, f0, f13

    Vector2 lPoint;
    Vector2 lNormal;
    GetPointAndNormalOnCurve(lpRacingLine, liSectionID, lfInterp, lPoint, lNormal);

    const f32 lfRoadPlacement = lpRacingLine->mfRoadPlacement;
    lrOutPosition.mV.x = lNormal.mV.x * lfRoadPlacement + lPoint.mV.x;
    lrOutPosition.mV.y = lNormal.mV.y * lfRoadPlacement + lPoint.mV.y;
    lrOutPosition.mV.z = lNormal.mV.z * lfRoadPlacement + lPoint.mV.z;
    lrOutPosition.mV.w = lNormal.mV.w * lfRoadPlacement + lPoint.mV.w;

    // Only lanes x and y are written (vrlimi128 masks 8 and 4); z / w keep whatever the caller
    // had there, exactly as the console leaves them.
    lrOutDirection.mV.x =  lNormal.mV.y;
    lrOutDirection.mV.y = -lNormal.mV.x;

    CgsDev::PerfMonCpu::StopMonitor(miFarAheadPM);
    return true;
}

//
//   RenderHardNoGoMap @0x82790278 (DWARF :190), DrawCentreLine (:203), RenderSectionDetails (:208)
//     -- debug render. Presentation-only on this host (COMMON_BRIEF rule 11): they draw the map
//     grid / centre line through the AI debug component, which is not part of this bring-up.
//     DELETE-WHEN the AI debug-render component is brought up on the host.
//
//   FindTargetFixedDistanceAhead (:96), FindAnyTargetOnLineAhead (:248),
//   RaceDefaultsLineToAutoCentre (:107), RaceLineDefaultsToHoldingRoadPosition (:117),
//   MoveRaceLineToSlamPlayer (:122)
//     -- DWARF-declared, NO IDA export, and NO inlined copy of any of them appears anywhere in
//     this lane's five exported bodies (they are folded into GenerateRacingLine @0x8277C4E8 and
//     the AIDriver target chain, neither of which is in this wave). There is nothing to recover
//     them from, so no body is written -- a guessed one would be invention.
//     DELETE-WHEN the wave that reconstructs their inlining call sites lands.
// =================================================================================================
}
