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
#include "GameShared/GameClasses/Core/CgsAssert.h"                // CGS_ASSERT + Begin/Fire/End
#include "GameShared/GameClasses/Development/CgsStrStream.h"      // CgsDev::StrStream

namespace BrnAI
{
namespace
{
    // flt_820C3B70 lane 0 == FLT_EPSILON: the RwMath::IsZero epsilon.
    const f32 KF_ZERO_EPSILON = 1.1920929e-07f;

    // unk_820C3B40 lane 0 == 0x37800000 == 2^-16: the rw::math::vpu::IsSimilar epsilon used by
    // ExtrapolateRouteBackwards to match the exit section's portal against the chosen one
    // (`lvlx v0, r0, r18 ; vspltw v13, v0, 0` at 0x82782B00/0x82782B08, r18 = 0x820C3B40).
    const f32 KF_PORTAL_MATCH_EPSILON = 1.52587890625e-05f;

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

    // mx8Flags bit 0x10: the section the stuck-walk-back stops at (`lbz 0x17 ; rlwinm 0,27,27`
    // at 0x82782770 / 0x82781CD8 / 0x8278221C). A single-portal section is a dead end; the
    // generators walk their own output back to the last section carrying this bit and block the
    // section they had taken out of it.
    // ⚠️ [FLAG PC bring-up] the DWARF accessor NAME for this bit is NOT attested. The
    // liTrackBack variable-hint blocks (:3130 / :2740 / :2926) list only GetAISection and
    // operator[], and the six remaining single-bit accessors the DWARF declares for AISection --
    // IsNoReset (AISectionsData.h:383), IsInAir (:386), IsSplit (:389), IsTerminator (:392),
    // IsInterstateExit (:395), IsJunction (:398) -- are ALL inlined: none has a named export in
    // the ARTIST index, so no body exists anywhere in the image to tie a bit to a name.
    // (IsShortcut = 0x01 / IsAIShortcut = 0x40 are pinned, and IsInterstateExit is 0x80 per the
    // street manager.) Semantically the walk-back stops at a junction, so 0x10 is very likely
    // IsJunction -- but the bit, not the name, is what this test needs, and the bit IS attested
    // (`lbz r11,0x17(r28) ; rlwinm r11,r11,0,27,27` at 0x82782774 / 0x82781CDC / 0x82782220).
    // DELETE-WHEN an AISection accessor for 0x10 is pinned to a name; replace the raw test.
    const u8 KX8_SECTION_FLAG_TRACKBACK_STOP = 0x10;

    // Stuck budget: the third single-portal dead end ends the walk (`cmpwi cr6, r11, 2 ; bgt`).
    const s32 KI_MAX_STUCK_ITERATIONS = 2;

    // The unity build's file string every "Strangely shaped section index" assert cites.
    const char* const KPC_SOURCE_FILE =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../World/AI/RacingLine/BrnRacingLineGenerator.cpp";

    inline bool IsZero2D(f32 lfX, f32 lfY)
    {
        return !(fabsf(lfX) > KF_ZERO_EPSILON) && !(fabsf(lfY) > KF_ZERO_EPSILON);
    }

    // The streamed assert at BrnRacingLineGenerator.cpp:<liLine>: BeginAssert ; StrStream over
    // gpcMessageBuffer << "Strangely shaped section index " << <index> << "\n" ; FireAssert ;
    // EndAssert (0x82782930..0x827829A4 / 0x82781E9C..0x82781F0C / 0x827823C4..0x82782434).
    void FireStrangelyShapedSectionAssert(u32 luSectionIndex, s32 liLine)
    {
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStream << "Strangely shaped section index " << static_cast<s32>(luSectionIndex) << "\n";
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lacMessageBuffer, KPC_SOURCE_FILE, liLine);
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
        const f32 lfLeadsToX = lCentreOfNextSection3D.x - lpPossibleNextPortal->GetPositionX();
        const f32 lfLeadsToY = lCentreOfNextSection3D.z - lpPossibleNextPortal->GetPositionZ();

        if (IsZero2D(lfLeadsToX, lfLeadsToY))
        {
            FireStrangelyShapedSectionAssert(luLinkedSectionIndex, liAssertLine);
            return false;
        }

        // Normalize (vrsqrtefp + 2 x Newton-Raphson == exact) then Dot with the reference.
        const f32 lfInvLength =
            1.0f / sqrtf((lfLeadsToX * lfLeadsToX) + (lfLeadsToY * lfLeadsToY));
        lrfAheadness = (lfLeadsToX * lfInvLength * lReferenceDirection.x)
                     + (lfLeadsToY * lfInvLength * lReferenceDirection.y);
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
               "luStartSectionIndex != BrnWorld::KI_INVALID_SECTION_INDEX");   // :3087

    s32 liCurrentSectionIndex = luStartSectionIndex;

    const AISection* lpCurrentSection = lpAISectionsData->GetAISection(luStartSectionIndex);
    const bool lbUseShortcuts   = lpCurrentSection->IsShortcut();     // :3092
    const bool lbUseAIShortcuts = lpCurrentSection->IsAIShortcut();   // :3093

    const Vector2 lCarBackwardsDirection = { -lCarDirection.x, -lCarDirection.y };   // :3097

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
