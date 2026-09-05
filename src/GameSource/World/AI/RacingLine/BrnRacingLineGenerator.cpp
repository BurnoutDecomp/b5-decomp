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
#include "GameShared/GameClasses/Core/CgsAssert.h"       // CGS_ASSERT + Begin/Fire/End
#include "GameShared/GameClasses/Development/CgsStrStream.h"          // CgsDev::StrStream
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // Start/StopMonitor

namespace BrnAI
{
// ---- the class's static state (DWARF BrnRacingLineGenerator.h:380..:385, :393) ------------
// DEFINED ONCE, here. No other RacingLineGenerator partfile may define them (LNK2005).
// The console registers all six handles in InitialiseRacingLine @0x8278FB20; until that body
// lands they stay 0, which CgsDev::PerfMonCpu treats as monitor handle 0.
s32  RacingLineGenerator::miGenerateRacingLinePM = 0;
s32  RacingLineGenerator::miGenerateInSectionPM  = 0;
s32  RacingLineGenerator::miHNGMapGenerationPM1  = 0;
s32  RacingLineGenerator::miHNGMapGenerationPM2  = 0;
s32  RacingLineGenerator::miHNGMapGenerationPM3  = 0;
s32  RacingLineGenerator::miFarAheadPM           = 0;
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

// AISection::mx8Flags bit 0x10 (`lbz r11,0x17(section) ; rlwinm r11,r11,0,27,27`
// @0x82775CF4 / @0x82775E14). LookForStraightSection{Ahead,Behind} walk WHILE the bit is set,
// i.e. the bit marks "this section is NOT straight" -- semantically a junction.
// ⚠️ [FLAG PC bring-up] the DWARF accessor NAME for this bit is still not pinned; see the
// identical park in BrnRacingLineGenerator_Extrapolate.cpp (KX8_SECTION_FLAG_TRACKBACK_STOP),
// which reads the same bit for its walk-back stop. The BIT is attested, the name is not.
// DELETE-WHEN an AISection accessor for 0x10 is tied to a DWARF name.
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
    return (lrVector.x == lrVector.x) && (lrVector.y == lrVector.y);
}

inline Vector2 Make2D(f32 lfX, f32 lfY)
{
    Vector2 lResult;
    lResult.x = lfX;
    lResult.y = lfY;
    lResult.z = 0.0f;
    lResult.w = 0.0f;
    return lResult;
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
    CgsDev::Assert::FireAssert(lacMessageBuffer, KPC_SOURCE_FILE_H, 418);
    CgsDev::Assert::EndAssert();
}

void FireStreamedTextAssert(const char* lpcText, s32 liLine)
{
    char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
    CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
    lStream << lpcText;
    CgsDev::Assert::BeginAssert();
    CgsDev::Assert::FireAssert(lacMessageBuffer, KPC_SOURCE_FILE_CPP, liLine);
    CgsDev::Assert::EndAssert();
}

void FireSectionIndexAssert(const char* lpcPrefix, s32 liValue, const char* lpcSuffix, s32 liLine)
{
    char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
    CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
    lStream << lpcPrefix << liValue << lpcSuffix;
    CgsDev::Assert::BeginAssert();
    CgsDev::Assert::FireAssert(lacMessageBuffer, KPC_SOURCE_FILE_CPP, liLine);
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
                                        lPosition.x, lPosition.y))
                {
                    CgsDev::PerfMonCpu::StopMonitor(miGenerateInSectionPM);
                    return liSection;
                }
            }
        }
        else
        {
            if (IsInsideSectionFast(GetSectionPointer(lpRacingLine, liSectionID),
                                    lPosition.x, lPosition.y))
            {
                CgsDev::PerfMonCpu::StopMonitor(miGenerateInSectionPM);
                return liSectionID;
            }

            if (CacheUpToDate(lpRacingLine, liSectionID + 1)
                && IsInsideSectionFast(GetSectionPointer(lpRacingLine, liSectionID + 1),
                                       lPosition.x, lPosition.y))
            {
                CgsDev::PerfMonCpu::StopMonitor(miGenerateInSectionPM);
                return liSectionID + 1;
            }

            if (CacheUpToDate(lpRacingLine, liSectionID - 1)
                && IsInsideSectionFast(GetSectionPointer(lpRacingLine, liSectionID - 1),
                                       lPosition.x, lPosition.y))
            {
                CgsDev::PerfMonCpu::StopMonitor(miGenerateInSectionPM);
                return liSectionID - 1;
            }

            for (s32 liSection = liSectionID + 2;
                 liSection <= lpRacingLine->mLastSectionInCache;
                 ++liSection)
            {
                if (IsInsideSectionFast(GetSectionPointer(lpRacingLine, liSection),
                                        lPosition.x, lPosition.y))
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
                                        lPosition.x, lPosition.y))
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
                                lPosition.x, lPosition.y))
        {
            CgsDev::PerfMonCpu::StopMonitor(miGenerateInSectionPM);
            return liSectionID;
        }

        if (CacheUpToDate(lpRacingLine, liSectionID + 1)
            && IsInsideSectionFast(GetSectionPointer(lpRacingLine, liSectionID + 1),
                                   lPosition.x, lPosition.y))
        {
            CgsDev::PerfMonCpu::StopMonitor(miGenerateInSectionPM);
            return liSectionID + 1;
        }

        if (CacheUpToDate(lpRacingLine, liSectionID - 1)
            && IsInsideSectionFast(GetSectionPointer(lpRacingLine, liSectionID - 1),
                                   lPosition.x, lPosition.y))
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

        if (IsSimilar2D(lPreviousCorner.x, lPreviousCorner.y, lrLeft.x, lrLeft.y)
            && IsSimilar2D(lCurrentCorner.x, lCurrentCorner.y, lrRight.x, lrRight.y))
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
    if (!(lInterp.x == lInterp.x && lInterp.y == lInterp.y
          && lInterp.z == lInterp.z && lInterp.w == lInterp.w))
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

    const f32 lfT  = lInterp.x;
    const f32 lfT2 = lfT * lfT;
    const f32 lfT3 = lfT2 * lfT;

    const f32 lfH1 = (2.0f * lfT3) - (3.0f * lfT2) + 1.0f;
    const f32 lfH2 = 1.0f - lfH1;
    const f32 lfH3 = lfT3 - (2.0f * lfT2) + lfT;
    const f32 lfH4 = lfT3 - lfT2;

    lrResult.x = (lInVectorA.x * lfH1) + (lInVectorB.x * lfH2)
               + (lInVector.x  * lfH3) + (lOutVector.x * lfH4);
    lrResult.y = (lInVectorA.y * lfH1) + (lInVectorB.y * lfH2)
               + (lInVector.y  * lfH3) + (lOutVector.y * lfH4);
    lrResult.z = (lInVectorA.z * lfH1) + (lInVectorB.z * lfH2)
               + (lInVector.z  * lfH3) + (lOutVector.z * lfH4);
    lrResult.w = (lInVectorA.w * lfH1) + (lInVectorB.w * lfH2)
               + (lInVector.w  * lfH3) + (lOutVector.w * lfH4);

    if (!IsValid2D(lrResult))
    {
        // @0x82776BFC..0x82776D5C: "NAN rsult in simple hermite : h1=" << h1 << " h2=" << h2
        // << " h3=" << h3 << " In=" << (%f, %f) << " Out=" << (%f, %f) << "\n"  (.cpp:1826).
        // ("rsult" is the console's own spelling.)
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStream << "NAN rsult in simple hermite : h1=" << lfH1
                << " h2=" << lfH2 << " h3=" << lfH3 << " In=";
        lStream.AppendFormat("(%f, %f)", lInVector.x, lInVector.y);
        lStream << " Out=";
        lStream.AppendFormat("(%f, %f)", lOutVector.x, lOutVector.y);
        lStream << "\n";
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lacMessageBuffer, KPC_SOURCE_FILE_CPP, 1826);
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

    const f32 lfDeltaX  = lInVectorB.x - lInVectorA.x;
    const f32 lfDeltaY  = lInVectorB.y - lInVectorA.y;
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
        const VecFloat lGuessVector = { lfBestGuess, lfBestGuess, lfBestGuess, lfBestGuess };
        GetSimpleHermite(lInVectorA, lInVectorB, lInVector, lOutVector, lGuessVector, lrResult);

        const Vector2 lPointLine = Make2D(lrResult.x - lInVectorA.x, lrResult.y - lInVectorA.y);
        const f32 lfNewInterp = (lVectorLine.x * lPointLine.x) + (lVectorLine.y * lPointLine.y);
        const f32 lfError     = lfNewInterp - lInterp;

        if (!(lfError == lfError))
        {
            // @0x82777338..0x8277749C (.cpp:1899).
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStream << "'Error' is a NAN in iterative hermite lfBestGuess=" << lfBestGuess
                    << " liIterate=" << liIterate << " lVectorLine=";
            lStream.AppendFormat("(%f, %f)", lVectorLine.x, lVectorLine.y);
            lStream << " lPointLine=";
            lStream.AppendFormat("(%f, %f)", lPointLine.x, lPointLine.y);
            lStream << " lNewInterp=" << lfNewInterp << " lInterp=" << lInterp << "\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessageBuffer, KPC_SOURCE_FILE_CPP, 1899);
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
            lStream.AppendFormat("(%f, %f)", lInVectorA.x, lInVectorA.y);
            lStream << " vertB=";
            lStream.AppendFormat("(%f, %f)", lInVectorB.x, lInVectorB.y);
            lStream << " In=";
            lStream.AppendFormat("(%f, %f)", lInVector.x, lInVector.y);
            lStream << " Out=";
            lStream.AppendFormat("(%f, %f)", lOutVector.x, lOutVector.y);
            lStream << " Result=";
            lStream.AppendFormat("(%f, %f)", lrResult.x, lrResult.y);
            lStream << "\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessageBuffer, KPC_SOURCE_FILE_CPP, 1906);
            CgsDev::Assert::EndAssert();
        }
    }

    if (!(lfBestGuess == lfBestGuess))
    {
        lrResult = lInVectorA;
        return 0.0f;
    }

    const VecFloat lFinalVector = { lfBestGuess, lfBestGuess, lfBestGuess, lfBestGuess };
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

    const f32 lfAxisX = lExit.x - lEntrance.x;
    const f32 lfAxisY = lExit.y - lEntrance.y;

    if (IsZero2D(lfAxisX, lfAxisY))
    {
        return 0.0f;
    }

    const f32 lfFromEntranceX = lPosition.x - lEntrance.x;
    const f32 lfFromEntranceY = lPosition.y - lEntrance.y;

    const f32 lfAxisLengthSq = (lfAxisX * lfAxisX) + (lfAxisY * lfAxisY);
    const f32 lfProjection   = (lfAxisX * lfFromEntranceX) + (lfAxisY * lfFromEntranceY);
    const f32 lfInterp       = lfProjection / lfAxisLengthSq;

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

    lrCentre.x = (lLeftEdge.x + lRightEdge.x) * 0.5f;
    lrCentre.y = (lLeftEdge.y + lRightEdge.y) * 0.5f;
    lrCentre.z = (lLeftEdge.z + lRightEdge.z) * 0.5f;
    lrCentre.w = (lLeftEdge.w + lRightEdge.w) * 0.5f;
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
        lIntersection.x = (lLeftEdge.x + lRightEdge.x) * 0.5f;
        lIntersection.y = (lLeftEdge.y + lRightEdge.y) * 0.5f;
        lIntersection.z = (lLeftEdge.z + lRightEdge.z) * 0.5f;
        lIntersection.w = (lLeftEdge.w + lRightEdge.w) * 0.5f;
    }
    else if (lfEdgeParam < 0.0f)
    {
        lIntersection.x = lLeftEdge.x + ((lRightEdge.x - lLeftEdge.x) * KF_ROUTE_EDGE_INSET);
        lIntersection.y = lLeftEdge.y + ((lRightEdge.y - lLeftEdge.y) * KF_ROUTE_EDGE_INSET);
        lIntersection.z = lLeftEdge.z + ((lRightEdge.z - lLeftEdge.z) * KF_ROUTE_EDGE_INSET);
        lIntersection.w = lLeftEdge.w + ((lRightEdge.w - lLeftEdge.w) * KF_ROUTE_EDGE_INSET);
    }
    else if (lfEdgeParam > 1.0f)
    {
        lIntersection.x = lRightEdge.x + ((lLeftEdge.x - lRightEdge.x) * KF_ROUTE_EDGE_INSET);
        lIntersection.y = lRightEdge.y + ((lLeftEdge.y - lRightEdge.y) * KF_ROUTE_EDGE_INSET);
        lIntersection.z = lRightEdge.z + ((lLeftEdge.z - lRightEdge.z) * KF_ROUTE_EDGE_INSET);
        lIntersection.w = lRightEdge.w + ((lLeftEdge.w - lRightEdge.w) * KF_ROUTE_EDGE_INSET);
    }
    else
    {
        lIntersection.x = lFrom.x + ((lTo.x - lFrom.x) * lfRouteParam);
        lIntersection.y = lFrom.y + ((lTo.y - lFrom.y) * lfRouteParam);
        lIntersection.z = lFrom.z + ((lTo.z - lFrom.z) * lfRouteParam);
        lIntersection.w = lFrom.w + ((lTo.w - lFrom.w) * lfRouteParam);
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

    if (IsSimilar2D(lEntrance.x, lEntrance.y, lExit.x, lExit.y))
    {
        lrInVector.SetZero();
        lrOutVector.SetZero();
        return;
    }

    const f32 lfChordLength = Magnitude2D(lEntrance.x - lExit.x, lEntrance.y - lExit.y);
    const f32 lfScale = (lfChordLength > KF_IN_OUT_VECTOR_MAX_LENGTH)
                            ? KF_IN_OUT_VECTOR_MAX_LENGTH
                            : lfChordLength;

    // ---- the incoming tangent -----------------------------------------------------------
    if (CacheUpToDate(lpRacingLine, liSectionIndex - 1))
    {
        const SectionData* lpPrevious = GetSectionPointer(lpRacingLine, liSectionIndex - 1);
        const Vector2 lPreviousEntrance = lpPrevious->GetSectionEntrance();

        if (IsSimilar2D(lEntrance.x, lEntrance.y, lPreviousEntrance.x, lPreviousEntrance.y))
        {
            lrInVector = Make2D(lExit.x - lEntrance.x, lExit.y - lEntrance.y);
        }
        else
        {
            lrInVector = Make2D(lEntrance.x - lPreviousEntrance.x,
                                lEntrance.y - lPreviousEntrance.y);
        }
    }
    else
    {
        lrInVector = Make2D(lExit.x - lEntrance.x, lExit.y - lEntrance.y);
    }

    if (IsZero2D(lrInVector.x, lrInVector.y))
    {
        FireStreamedTextAssert("!RwMath::IsZero(lInVector)", 1984);
    }

    {
        const Vector2 lNormalised = Normalise2D(lrInVector.x, lrInVector.y);
        lrInVector = Make2D(lNormalised.x * lfScale, lNormalised.y * lfScale);
    }

    // ---- the outgoing tangent ------------------------------------------------------------
    if (CacheUpToDate(lpRacingLine, liSectionIndex + 1))
    {
        const SectionData* lpNext = GetSectionPointer(lpRacingLine, liSectionIndex + 1);
        const Vector2 lNextExit = lpNext->GetSectionExit();

        if (IsSimilar2D(lExit.x, lExit.y, lNextExit.x, lNextExit.y))
        {
            lrOutVector = Make2D(lExit.x - lEntrance.x, lExit.y - lEntrance.y);
        }
        else
        {
            lrOutVector = Make2D(lNextExit.x - lExit.x, lNextExit.y - lExit.y);
        }
    }
    else
    {
        lrOutVector = Make2D(lExit.x - lEntrance.x, lExit.y - lEntrance.y);
    }

    if (IsZero2D(lrOutVector.x, lrOutVector.y))
    {
        FireStreamedTextAssert("!RwMath::IsZero(lOutVector)", 2010);
    }

    {
        const Vector2 lNormalised = Normalise2D(lrOutVector.x, lrOutVector.y);
        lrOutVector = Make2D(lNormalised.x * lfScale, lNormalised.y * lfScale);
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
    CGS_ASSERT(lpRacingLine->IsInitialised(), "lpRacingLine->IsInitialised()");   // .cpp:126

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
        f32 lfInterp = GetSectionInterpPosition(lpRacingLine, liSection, lTargetPos2D);
        if (lfInterp <= 0.0f)
        {
            lfInterp = 0.0f;        // fsel f31, -f1, 0.0, f1
        }

        const SectionData* lpSectionData = GetSectionPointer(lpRacingLine, liSection);
        const Vector2 lEntrance = lpSectionData->GetSectionEntrance();
        const Vector2 lExit     = lpSectionData->GetSectionExit();

        if (IsSimilar2D(lEntrance.x, lEntrance.y, lExit.x, lExit.y))
        {
            continue;
        }

        Vector2 lInVector;
        Vector2 lOutVector;
        GenerateInOutVectors(lpRacingLine, liSection, lInVector, lOutVector);

        const f32 lfClampedInterp = (lfInterp > 1.0f) ? 1.0f : lfInterp;   // fsel on 1.0 - interp

        Vector2 lPointOnCurve;
        const f32 lfCurveInterp = GetIterativeHermite(lEntrance, lExit, lInVector, lOutVector,
                                                      lfClampedInterp, lPointOnCurve);

        const f32 lfDistance = Magnitude2D(lPointOnCurve.x - lTargetPos2D.x,
                                           lPointOnCurve.y - lTargetPos2D.y);
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

        const VecFloat lAheadVector = { lfAheadInterp, lfAheadInterp,
                                        lfAheadInterp, lfAheadInterp };
        GetSimpleHermite(lEntrance, lExit, lInVector, lOutVector, lAheadVector, lrCentreAhead);

        if (lbMirrorAhead)
        {
            // 0x8278EBB4: ahead = here - (ahead - here) -- reflect the backwards sample
            // through the "here" point so the direction still leads forwards.
            lrCentreAhead.x = lrCentreHere.x - (lrCentreAhead.x - lrCentreHere.x);
            lrCentreAhead.y = lrCentreHere.y - (lrCentreAhead.y - lrCentreHere.y);
            lrCentreAhead.z = lrCentreHere.z - (lrCentreAhead.z - lrCentreHere.z);
            lrCentreAhead.w = lrCentreHere.w - (lrCentreAhead.w - lrCentreHere.w);
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
        CGS_ASSERT(liSection < lpRoute->GetNodeCount(), "Route node count out of range");  // :341

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
        // [FLAG header_request] RouteNode::muPortalIndex (DWARF BrnRoute.h:89, u8 @+0x0E) is
        // modelled in the host BrnRoute.h -- which this lane does not own -- as the second half
        // of a u16 `muPad0x0E`. The console reads the byte directly (`lbz r4, 0xE(node)`
        // @0x8278EDFC); the low byte of the host u16 IS that byte on this little-endian target.
        // DELETE-WHEN BrnRoute.h splits it into `u8 mu8PortalIndex; u8 mu8Pad0x0F;`.
        lrEntry.mpTargetPortal = lpSection->GetPortal(static_cast<u8>(lpNode->muPad0x0E));
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
    lMidPoint.x = (lLeftEdge.x + lRightEdge.x) * 0.5f;
    lMidPoint.y = (lLeftEdge.y + lRightEdge.y) * 0.5f;
    lMidPoint.z = (lLeftEdge.z + lRightEdge.z) * 0.5f;
    lMidPoint.w = (lLeftEdge.w + lRightEdge.w) * 0.5f;

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

// =================================================================================================
// [FLAG PC bring-up] NOT BODIED BY THIS LANE -- declared in BrnRacingLineGenerator.h, no
// definition anywhere yet. Each is a real console function; none is reached by any body in this
// file, so nothing here links against them.
//   GetSectionPointerForWrite (DWARF :176)  -- inlined on the console; no recovered call site in
//                                              this lane's bodies, so its (assert-free?) shape is
//                                              not attested. DELETE-WHEN a caller is recovered.
//   GetBestSectionIndex       (DWARF :181)  -- inlined; same.
//   GetWidthHalfWidthOfKnownSection (:337)  -- inlined; same.
//   DoSlowTurn                (DWARF :231)  -- inlined; no recovered call site.
//   (IsTargetUpToDate / IsAJunction / GetPointAndNormalOnCurve / SetUpIncomingPortalTarget were
//    bodied by aiwave2 lane R3 in BrnRacingLineGenerator_Curve.cpp, 2026-09-05.)
//   RenderHardNoGoMap (:190) / DrawCentreLine (:203) / RenderSectionDetails (:208)
//                                           -- debug render; presentation-only on this host, so
//                                              parked per the wave brief.
// =================================================================================================
}
