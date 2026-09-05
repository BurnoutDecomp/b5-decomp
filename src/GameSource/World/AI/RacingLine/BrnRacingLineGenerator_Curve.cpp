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
// ---- baked constants ---------------------------------------------------------------------
// The cache is 16 slots addressed by `sectionIndex & 15` (`clrlwi r11, r5, 28` everywhere; here
// @0x8278F8A0 and @0x8278FE44).
const s32 KI_SECTION_CACHE_MASK = KI_RACING_LINE_MAX_AVAILABLE_SECTIONS - 1;

// AISection::mx8Flags bit 0x10 -- `lbz r11, 0x17(section) ; rlwinm r11, r11, 0, 27, 27`
// @0x8278FE98 (that pair IS IsAJunction), and the same test @0x82775CF4 / @0x82775E14 inside
// LookForStraightSection{Ahead,Behind}, which walk WHILE the bit is set.
// [FLAG PC bring-up] the DWARF accessor NAME for this bit is still not pinned -- the identical
// park stands in BrnRacingLineGenerator.cpp (KX8_SECTION_FLAG_JUNCTION) and in
// BrnRacingLineGenerator_Extrapolate.cpp (KX8_SECTION_FLAG_TRACKBACK_STOP). The BIT is attested by
// three independent bodies; the name is not.
// DELETE-WHEN an AISection accessor for 0x10 is tied to a DWARF name.
const u8 KX8_SECTION_FLAG_JUNCTION = 0x10;

// flt_820C3B70 lane 0 == FLT_EPSILON: the RwMath::IsZero epsilon (`vandc` sign-clear + `vcmpgtfp`
// + the 0x0004080C AND-reduce permute @0x82781388 and @0x82781474).
const f32 KF_ZERO_EPSILON = 1.1920929e-07f;

// unk_820C3B40 lane 0 == 2^-16: the rw::math::vpu::IsSimilar epsilon the entrance/exit degeneracy
// tests @0x82780C78 / @0x82780CF8 / @0x82781030 run on.
const f32 KF_PORTAL_MATCH_EPSILON = 1.52587890625e-05f;

// flt_820C424C == 0.1f -- how much further along the hermite GetPointAndNormalOnCurve samples its
// second point to get the tangent (`lfs f0, 0(r15)` @0x827811BC, with r15 == 0x820C424C).
const f32 KF_CURVE_NORMAL_STEP = 0.1f;

// The unity-build file string every streamed assert in this TU cites.
const char* const KPC_SOURCE_FILE_CPP =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../World/AI/RacingLine/BrnRacingLineGenerator.cpp";

// ---- small shared helpers ----------------------------------------------------------------
// These mirror the file-local helpers of BrnRacingLineGenerator.cpp; both copies live in an
// anonymous namespace, so there is no LNK2005 and no second external symbol.

// RwMath::IsZero on a 2D vector: |x| <= FLT_EPSILON && |y| <= FLT_EPSILON.
inline bool IsZero2D(f32 lfX, f32 lfY)
{
    return !(std::fabs(lfX) > KF_ZERO_EPSILON) && !(std::fabs(lfY) > KF_ZERO_EPSILON);
}

// rw::math::vpu::IsSimilar on the x/y lanes: no lane differs by more than 2^-16. The console
// duplicates x/y into z/w with `vrlimi128 v, v, 3, 2` so all four compared lanes are the pair.
inline bool IsSimilar2D(const Vector2& lrA, const Vector2& lrB)
{
    return !(std::fabs(lrA.x - lrB.x) > KF_PORTAL_MATCH_EPSILON)
        && !(std::fabs(lrA.y - lrB.y) > KF_PORTAL_MATCH_EPSILON);
}

// The lane-wise `vcmpeqfp.` self-compare the console uses as "is this finite / not-NaN".
inline bool IsValid2D(const Vector2& lrVector)
{
    return (lrVector.x == lrVector.x) && (lrVector.y == lrVector.y);
}

// ---- the streamed asserts (BeginAssert ; StrStream ; FireAssert ; EndAssert) ---------------
void FireStreamedTextAssert(const char* lpcText, s32 liLine)
{
    char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
    CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
    lStream << lpcText;
    CgsDev::Assert::BeginAssert();
    CgsDev::Assert::FireAssert(lacMessageBuffer, KPC_SOURCE_FILE_CPP, liLine);
    CgsDev::Assert::EndAssert();
}

// "<prefix>" << index << ", interp " << interp << "\n"   (str_820C7304 == ", interp ").
void FireBadCentreAssert(const char* lpcPrefix, s32 liSectionIndex, f32 lfInterp, s32 liLine)
{
    char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
    CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
    lStream << lpcPrefix << liSectionIndex << ", interp " << lfInterp << "\n";
    CgsDev::Assert::BeginAssert();
    CgsDev::Assert::FireAssert(lacMessageBuffer, KPC_SOURCE_FILE_CPP, liLine);
    CgsDev::Assert::EndAssert();
}

// "Zero vector being normalize, Interp = " << interp << "\n"   (str_820C72A8, .cpp:2267).
void FireZeroNormalAssert(f32 lfInterp, s32 liLine)
{
    char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
    CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
    lStream << "Zero vector being normalize, Interp = " << lfInterp << "\n";
    CgsDev::Assert::BeginAssert();
    CgsDev::Assert::FireAssert(lacMessageBuffer, KPC_SOURCE_FILE_CPP, liLine);
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
        if (lEntrance.x == 0.0f && lEntrance.y == 0.0f)
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
        lMidPoint.x = (lLeftEdge.x + lRightEdge.x) * 0.5f;
        lMidPoint.y = (lLeftEdge.y + lRightEdge.y) * 0.5f;
        lMidPoint.z = (lLeftEdge.z + lRightEdge.z) * 0.5f;
        lMidPoint.w = (lLeftEdge.w + lRightEdge.w) * 0.5f;

        lpSectionData->SetSectionEntrance(lMidPoint);

        const Vector2 lEntrance = lpSectionData->GetSectionEntrance();
        if (lEntrance.x == 0.0f && lEntrance.y == 0.0f)
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
            lrNormal.x = 1.0f;
            lrNormal.y = 0.0f;
            lrNormal.z = 0.0f;
            lrNormal.w = 0.0f;
            return;
        }

        SectionData* lpSecondTrySectionData = GetSectionPointer(lpRacingLine, liSectionIndex + 1);
        lLineEnd = lpSecondTrySectionData->GetSectionExit();

        if (IsSimilar2D(lLineStart, lLineEnd))
        {
            // [FLAG PC bring-up] 0x82780D0C..0x82780FF8 is a ten-line DEV LOG here, gated on bit 0
            // of the 64-bit global at 0x82F31908 and streamed into the object at 0x82F31904 (the
            // same unnamed AI debug-stream pair BrnAIAggression.cpp already parks). It re-fetches
            // both section pointers and prints liIndex, lpRacingLine->mLastSectionInCache, both
            // SectionData pointers, both mCachedSectionIndex values, and both
            // GetSectionEntrance() / GetSectionExit() pairs -- the literals at 0x820C7478,
            // 0x820C7454, 0x820C743C, 0x820C7424, 0x820C73F8, 0x820C73C8, 0x820C7398, 0x820C736C,
            // 0x820C733C, 0x820C7310. Neither global is named in the export set, so the log is not
            // wired; it cannot change the answer (the console re-tests this same condition at
            // 0x8278100C and takes the same arm either way).
            // DELETE-WHEN the AI debug-stream globals (0x82F31904 / 0x82F31908) are identified.
            lrNormal.x = 1.0f;
            lrNormal.y = 0.0f;
            lrNormal.z = 0.0f;
            lrNormal.w = 0.0f;
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

    const VecFloat lAheadVector = { lfAheadParam, lfAheadParam, lfAheadParam, lfAheadParam };
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
        lrNormal.y = lrPoint.x     - lPointAhead.x;
        lrNormal.x = lPointAhead.y - lrPoint.y;
    }
    else
    {
        lrNormal.y = lPointAhead.x - lrPoint.x;
        lrNormal.x = lrPoint.y     - lPointAhead.y;
    }

    // The console's vrlimi128 pair leaves the caller's own z/w bytes alone and the closing
    // normalise then scales them; on this host those bytes are an uninitialised local in every
    // caller, so they are published as zero (the standing z/w convention of this class).
    lrNormal.z = 0.0f;
    lrNormal.w = 0.0f;

    if (IsZero2D(lrNormal.x, lrNormal.y))
    {
        // 0x82781420..0x82781454 -- the curve gave no tangent at all, so fall back to the raw
        // chord A -> B.
        // [FLAG PC bring-up] THE CONSOLE'S SIGN HERE IS THE OPPOSITE of the main arm above: the Y
        // lane takes (A.x - B.x) and the X lane takes (B.y - A.y), i.e. -90 degrees where the
        // hermite arm rotates +90. Verified from the raw words (`vsubfp v11, v10, v11` @0x82781444
        // == A.x - B.x, `vsubfp v0, v0, v13` @0x82781448 == B.y - A.y) with the same
        // mask-4-then-mask-8 vrlimi128 pair. Reproduced exactly as the console has it: this is a
        // degenerate-only arm, and "correcting" it would make a rival steer to the wrong side of a
        // zero-length curve segment relative to the console.
        // DELETE-WHEN a console trace shows this arm being taken and its sign being wrong.
        lrNormal.y = lLineStart.x - lLineEnd.x;
        lrNormal.x = lLineEnd.y   - lLineStart.y;
    }

    if (IsZero2D(lrNormal.x, lrNormal.y))
    {
        FireZeroNormalAssert(lfAheadParam, 2267);
    }

    // vrsqrtefp + two Newton-Raphson refinements + vmulfp128, with no zero guard.
    const f32 lfLengthSq  = (lrNormal.x * lrNormal.x) + (lrNormal.y * lrNormal.y);
    const f32 lfInvLength = 1.0f / std::sqrt(lfLengthSq);
    lrNormal.x *= lfInvLength;
    lrNormal.y *= lfInvLength;
}
}
