// =================================================================================================
// BrnResetOnTrackManager_Strategies.cpp -- the SEVEN placement strategies ComputeResetOnTrack
// dispatches to, plus the four helpers they share (aiwave A11, 2026-09-03).
//
//   BrnAI::ResetOnTrackManager::ScanBackwardsAlongExtrapolatedRoute      @0x827847D0
//   BrnAI::ResetOnTrackManager::ScanForwardsAlongExtrapolatedRoute       @0x82784C40
//   BrnAI::ResetOnTrackManager::GetRoadSideForStartingLine               @0x82784378
//   BrnAI::ResetOnTrackManager::DeterminePositionBetweenNodes            @0x82785DC8
//   BrnAI::ResetOnTrackManager::ConvertNodesToPositionAndDirection       @0x82790300
//   BrnAI::ResetOnTrackManager::EnsureAIIsDrivingSameDirectionAsPlayer   @0x82778088
//   BrnAI::ResetOnTrackManager::ResetNearRoutelessPlayer                 @0x827844D8
//   BrnAI::ResetOnTrackManager::ResetAwayFromPlayer                      @0x82784148
//   BrnAI::ResetOnTrackManager::ResetFixedDistanceBehindPlayerAtStartOfRace @0x827908F0  (type 6)
//   BrnAI::ResetOnTrackManager::ResetFixedDistanceBehindPlayer           @0x82790628     (types 2/3)
//   BrnAI::ResetOnTrackManager::ResetFixedDistanceAheadOfPlayer          @0x827907D8     (type 4)
//   BrnAI::ResetOnTrackManager::ResetAheadFromSideTurnings               @0x827909F0     (type 5)
//   BrnAI::ResetOnTrackManager::ScanForwardsAndAlongJunction             @0x827852C0     (export hole)
//   BrnAI::ResetOnTrackManager::InterpolatePositionFromAngle             @0x82784FD8
//   BrnAI::ResetOnTrackManager::PlayerIsLookingBackwards                 @0x82778000
//
// WHY THIS TU EXISTS. Before it, ComputeResetOnTrack answered every non-STANDARD reset type with a
// once-only "PARKED strategy" log line and `false`, so every request that was not type 1 resolved
// to ResetOnTrackResult::E_STATE_FAILURE and the consumer fell back to the car's own reset-coords
// ring. That is fine for a crashed player (its ring holds real on-road poses) and useless for a
// rival being placed on the STARTING GRID, which has no ring yet: RaceCarEntityModule::
// PlaceRaceCarOnLoad @0x822CE588 sends every opponent a type-6
// (E_RESET_TYPE_BEHIND_PLAYER_RACE_START) request at -45 / -64.6 / -84.2 / -103.8 / -123.4 m and
// the ANSWER is where the rival's car ends up.
//
// WHAT MADE THEM RECONSTRUCTIBLE NOW, and what the 2026-08-26 park banner said was missing:
// every one of the "FOUR functions none of which exists in this tree" has since landed --
// AISectionsData::GetAISection / AISection::GetPortal / AISection::PassesThrough (SharedClasses/AI),
// Portal::GetBoundaryLine + BoundaryLine::GetInterp/GetLength (World/AI), and the three
// RacingLineGenerator::ExtrapolateRoute* (RacingLine/BrnRacingLineGenerator_Extrapolate.cpp, aiwave
// A6) -- and BrnAICar.h now NAMES muBestSectionIndex / muDefaultSectionIndex /
// muResetOnTrackSectionIndex / muResetOnTrackStartPortal / muResetOnTrackEndPortal. Nothing here is
// reached by raw offset.
//
// GEOMETRY CONVENTION. "2D" is the XZ ground plane throughout: BrnMath::Flatten(Vector3) -> Vector2
// with .x == world X and .y == world Z, which is exactly what the X360's
// `vrlimi128 v13, v0, 8, 0 ; vrlimi128 v13, v0, 4, 1` lane pair and the
// `vperm ... unk_82CDA450` mask build. The console's VMX (lvx128 / vspltw / vmsum3fp128 /
// vrsqrtefp + two Newton-Raphson steps) is de-optimised to portable scalar float math at SEMANTIC
// PARITY, the established subsystem convention (BrnRouteRequestManager.cpp, BrnAIBoundaryLine.cpp);
// the rsqrt pipeline lowers to sqrtf, whose exact-zero result satisfies the asm's own vsel guard.
//
// THE OPERAND-ORDER TRAP THIS TU HAD TO SOLVE. IDA prints the PowerPC A-form multiply-add family in
// ENCODING field order (vD, vA, vB, vC), not in mnemonic order (vD, vA, vC, vB). Read
// `vnmsubfp v11, v8, v11, v7` @0x827942BC as "v11 - v8*v7" and AvoidObstacles' lateral vector comes
// out as garbage; read it as the encoding order (vB - vA*vC == v11 - v8*v7 with vB==v11) and the
// whole block is the single-shuffle cross form `(a.yzx*b - a*b.yzx).yzx` with a == the reset
// direction and b == the world Y axis (the {0,1,0,0} literal at unk_82181510). That form evaluates
// to Cross(b, a), so the result is Cross(worldUp, dir) == (dir.z, 0, -dir.x) -- corrected 2026-09-22
// (G08-D1); it used to be labelled a "right vector" and carried the opposite sign in AvoidObstacles.
// The same reading is what makes DeterminePositionBetweenNodes' lerp come out as a lerp.
// =================================================================================================

#include "GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.h"

#include "GameSource/World/AI/BrnAICar.h"                              // AICar (section/portal members)
#include "GameSource/World/AI/BrnAIPortal.h"                           // BrnAI::Portal / BoundaryLine
#include "GameSource/World/AI/RacingLine/BrnRacingLineGenerator.h"     // ExtrapolateRoute* + ExtrapolatedIndexArray
#include "GameSource/World/AI/Route/BrnRouteMapModule.h"               // BrnAI::SectionAndPortalIndices
#include "GameSource/Math/BrnMathUtils.h"                              // BrnMath::Flatten (XZ)
#include "SharedClasses/AI/AISectionsResourceType.h"                   // AISection / AISectionsData
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"             // gpDebugPrint

#include <math.h>   // sqrtf

namespace BrnAI
{
namespace
{
    // ---- the console's baked literals, READ OUT OF THE DECRYPTED ARTIST IMAGE ------------------
    // (file offset == VA - 0x82000000; every value below is the big-endian f32 at that VA.)

    // unk_820C3B40 lane 0 == 2^-16. The rw "IsSimilar" tolerance every
    // `vandc(diff) ; vcmpgtfp. ; extract CR6 bit 26` block in this family compares against.
    const f32 KF_IS_SIMILAR_EPSILON = 1.52587890625e-05f;

    // flt_820C3B70 == FLT_EPSILON. ResetNearRoutelessPlayer's "is this offset non-zero enough to
    // normalise" guard uses this one, NOT the IsSimilar tolerance above -- two different constants
    // three instructions apart in the same function (0x82784690 vs 0x827845D8).
    const f32 KF_NORMALISE_EPSILON = 1.1920928955078125e-07f;

    // flt_82013A7C, read at ScanBackwardsAlongExtrapolatedRoute 0x82784C0C. The trailing
    // "we walked off the end of the extrapolated road" arm accepts the best pair found only when it
    // is already more than this far BEHIND the car.
    const f32 KF_LOOK_BACK_DISTANCE = -10.0f;

    // GetRoadSideForStartingLine @0x82784444..0x827844B4: 8.0 / 2.0 / +-0.25 / 0.5.
    const f32 KF_STARTING_LINE_CAR_WIDTH      = 8.0f;    // flt_820C41E0
    const f32 KF_STARTING_LINE_SPREAD         = 2.0f;    // flt_820C41F4
    const f32 KF_STARTING_LINE_MAX_OFFSET     = 0.25f;   // flt_82003F40 / flt_8200D56C (negated)
    const f32 KF_ROAD_CENTRE                  = 0.5f;    // flt_820C4168

    // ResetNearRoutelessPlayer @0x82784780 (flt_820C4890). DWARF local KF_DISTANCE_BEHIND_PLAYER.
    const f32 KF_DISTANCE_BEHIND_PLAYER = 20.0f;

    // ResetAwayFromPlayer @0x827842B4 (flt_820C4864) and its retry budget (cmpwi r23, 0xA).
    const f32 KF_AWAY_FROM_PLAYER_MIN_DISTANCE = 2000.0f;
    const s32 KI_AWAY_FROM_PLAYER_MAX_REPEATS  = 10;

    // The value AICar::Reset writes into every section-index member (BrnWorld::
    // KI_INVALID_SECTION_INDEX; AICar.h spells it AICar::KI_INVALID_SECTION_INDEX).
    const u16 KU_INVALID_SECTION_INDEX = 0x7FFFu;

    // The scan loops compare their running best against 0.0 (flt_82001CC0) and seed the node
    // distance-to-checkpoint slot with the same zero.
    const f32 KF_ZERO = 0.0f;

    // ---- de-optimised VMX helpers --------------------------------------------------------------

    // `vsubfp ; vandc(sign) ; vcmpgtfp. ; mfocrf ; extrwi ..,1,26` over lanes {x,y}: CR6 bit 26 is
    // "none true", so the console's `bne` arm is taken when EVERY lane is within tolerance.
    bool IsSimilar2D(Vector2 lA, Vector2 lB)
    {
        const f32 lfDX = (lB.x - lA.x) < 0.0f ? -(lB.x - lA.x) : (lB.x - lA.x);
        const f32 lfDY = (lB.y - lA.y) < 0.0f ? -(lB.y - lA.y) : (lB.y - lA.y);
        return !(lfDX > KF_IS_SIMILAR_EPSILON || lfDY > KF_IS_SIMILAR_EPSILON);
    }

    bool IsSimilar3D(Vector3 lA, Vector3 lB)
    {
        const f32 lfDX = (lB.x - lA.x) < 0.0f ? -(lB.x - lA.x) : (lB.x - lA.x);
        const f32 lfDY = (lB.y - lA.y) < 0.0f ? -(lB.y - lA.y) : (lB.y - lA.y);
        const f32 lfDZ = (lB.z - lA.z) < 0.0f ? -(lB.z - lA.z) : (lB.z - lA.z);
        return !(lfDX > KF_IS_SIMILAR_EPSILON || lfDY > KF_IS_SIMILAR_EPSILON ||
                 lfDZ > KF_IS_SIMILAR_EPSILON);
    }

    // `vmsum3fp128 ; vrsqrtefp ; two Newton-Raphson steps ; vmulfp`. The console's
    // `vcmpeqfp128 ; vsel` zero-guard (ResetAwayFromPlayer 0x827842C8/0x827842F4) is the
    // sqrtf(0)==0 case here.
    Vector3 Normalise3D(Vector3 lVector)
    {
        const f32 lfLengthSquared =
            lVector.x * lVector.x + lVector.y * lVector.y + lVector.z * lVector.z;
        if (lfLengthSquared == 0.0f)
        {
            return Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
        }
        const f32 lfInverse = 1.0f / sqrtf(lfLengthSquared);
        return Vector3{ lVector.x * lfInverse, lVector.y * lfInverse, lVector.z * lfInverse, 0.0f };
    }

    f32 Dot3D(Vector3 lA, Vector3 lB)
    {
        return lA.x * lB.x + lA.y * lB.y + lA.z * lB.z;
    }

    // `vsubfp ; vmulfp ; vspltw lane1 ; vaddfp` -- the 2-lane horizontal dot the two Scan* loops
    // use for "how far ahead of the car is this portal, along the car's heading".
    f32 Dot2D(Vector2 lA, Vector2 lB)
    {
        return lA.x * lB.x + lA.y * lB.y;
    }

    // The console writes the two scratch nodes field-for-field; the portal index goes in the
    // RouteNode byte at +14 (see the [FLAG header_request] on mHelperNode* in the header).
    void WriteHelperNode(RouteNode& lrNode, f32 lfX, f32 lfY, u16 luSectionIndex, u8 lu8PortalIndex)
    {
        lrNode.mfX                    = lfX;
        lrNode.mfY                    = lfY;
        lrNode.mfDistanceToCheckpoint = KF_ZERO;     // stfs f30 (0.0), 8(node)
        lrNode.muSectionIndex         = luSectionIndex;
        lrNode.muPad0x0E              = lu8PortalIndex;   // [FLAG header_request] the +14 byte
    }

    u8 GetHelperNodePortalIndex(const RouteNode& lrNode)
    {
        return static_cast<u8>(lrNode.muPad0x0E);        // [FLAG header_request]
    }

    // ---- reset type 5: ResetAheadFromSideTurnings @0x827909F0 + ScanForwardsAndAlongJunction
    //      @0x827852C0 + InterpolatePositionFromAngle @0x82784FD8 (crash parity G07-D2/D4) -------
    // The ADDRESS on each line is what the console loads. The NAMES are the DecFIGS DWARF's
    // file-scope list (BrnResetOnTrackManager.h:309/:314, BrnResetOnTrackManager.cpp:58/:60/
    // :61/:62/:67, local :1988) matched to the reader by ROLE -- the two 100.0 readers share one
    // rodata literal, so which DWARF name owns which read is not provable from the image.
    const f32 KF_NEAR_PLAYER_RESET_AHEAD_DISTANCE = 160.0f;   // flt_820C482C @0x82790AE4
    const f32 KF_RESET_AHEAD_VARIABILITY          = 40.0f;    // flt_82004D0C @0x82790A70
    const f32 KF_MIN_RESET_DISTANCE_BEHIND        = -25.0f;   // flt_820C4860 @0x82790B1C
    const f32 KF_TOO_CLOSE_TO_JOIN_FROM_SIDE      = 100.0f;   // flt_820C3FAC @0x82785434 (junction search)
    const f32 KF_TOO_CLOSE_DISTANCE               = 50.0f;    // flt_820C4244 @0x82785B68
    const f32 KF_TOO_CLOSE_TO_SPAWN_IN_VIEW       = 100.0f;   // flt_820C3FAC @0x82785CE8 (pop-up test)
    // .bss, written by the CRT initialisers (findinit): XMVectorCos of the rodata radians.
    const f32 KF_ANGLE_TO_JOIN_FROM_SIDE_ROAD     = 0.9396926f;   // flt_8300D7A8 = cos(flt_8201ED84 0.3490658 rad, 20 deg), init 0x82C69210
    const f32 KF_ASSUMED_FOV_FOR_RESET_AHEAD      = 0.76604444f;  // flt_8300D780 = cos(flt_820C8C30 0.6981317 rad, 40 deg), init 0x82C69260
    const s32 KI_SIDE_SCAN_BAIL_OUT               = 10;           // `cmpwi r20, 0xA` @0x82785B00

    // `vspltw ; vandc(sign) ; vcmpgtfp ; vperm 0x0004080C` on lane x, then lane y
    // (ScanForwardsAndAlongJunction 0x82785530..0x827855D8, InterpolatePositionFromAngle
    // 0x82785014..0x827850C0): a 2D vector is ZERO when neither lane's magnitude exceeds
    // FLT_EPSILON (flt_820C3B70). A NaN lane counts as zero, as vcmpgtfp does.
    bool IsZero2D(Vector2 lVector)
    {
        return !(fabsf(lVector.x) > KF_NORMALISE_EPSILON) &&
               !(fabsf(lVector.y) > KF_NORMALISE_EPSILON);
    }

    // The 3-lane form: `vandc ; vrlimi128 (w <- x) ; vcmpgtfp.` and the CR6 "none true" bit
    // (0x82785C20..0x82785C7C, 0x82785D3C..0x82785D64).
    bool IsZero3D(Vector3 lVector)
    {
        return !(fabsf(lVector.x) > KF_NORMALISE_EPSILON) &&
               !(fabsf(lVector.y) > KF_NORMALISE_EPSILON) &&
               !(fabsf(lVector.z) > KF_NORMALISE_EPSILON);
    }

    // `vmulfp ; vspltw x/y ; vaddfp ; vrsqrtefp + two Newton-Raphson steps ; vmulfp` with NO zero
    // guard -- every caller has already refused a zero vector through IsZero2D.
    Vector2 Normalise2D(Vector2 lVector)
    {
        const f32 lfInverse = 1.0f / sqrtf(lVector.x * lVector.x + lVector.y * lVector.y);
        return Vector2{ lVector.x * lfInverse, lVector.y * lfInverse, 0.0f, 0.0f };
    }

    // |v| through the same rsqrt pipeline (d2 * rsqrt(d2)) with its `vcmpeqfp ; vsel` zero guard
    // (0x82785904..0x8278595C for the 2-lane sum, 0x82785B88..0x82785BCC for vmsum3fp128).
    f32 Length2D(Vector2 lVector)
    {
        const f32 lfLengthSquared = lVector.x * lVector.x + lVector.y * lVector.y;
        return (lfLengthSquared == 0.0f) ? 0.0f : sqrtf(lfLengthSquared);
    }

    f32 Length3D(Vector3 lVector)
    {
        const f32 lfLengthSquared = Dot3D(lVector, lVector);
        return (lfLengthSquared == 0.0f) ? 0.0f : sqrtf(lfLengthSquared);
    }
}

// =================================================================================================
// ScanBackwardsAlongExtrapolatedRoute @0x827847D0 (DWARF :263)
//
//   0x8278481C  lpPlayerAICar = GetAICar(mePlayerGlobalRaceCarIndex)
//   0x82784838  lPlayerDirection = Flatten(lpPlayerAICar->GetDirection())      (v127)
//   0x82784860  lPlayerPosition  = Flatten(lpPlayerAICar->GetPosition())       (v126)
//   0x8278486C  section = muBestSectionIndex, falling back to muDefaultSectionIndex
//   0x827848B8  liForwardSectionsGenerated = ExtrapolateRouteForwards(1, section, dir, pos, ...)
//   0x827848BC  liExtrapolationLength = (leExtrapolateType == RaceStart) ? 16 : 8
//               (`subfic r10,r14,0 ; subfe r10,r10,r10 ; clrrwi r10,r10,3 ; addi r29,r10,0x10`
//                -- r14 == 0 leaves 0 and adds 16; r14 == 1 leaves -8 and adds 16)
//   0x8278490C  liSectionsGenerated = ExtrapolateRouteBackwards(len, section, dir, pos, ...)
//   0x82784914  if (backwards + forwards <= 1) return false
//   0x82784920  seed {luPrevSection, luPrevPortal} from lGeneratedForwardIndices[0] when the
//               forward walk produced anything, else from lGeneratedIndices[0] and start at 1
//   0x827849AC  for each backwards entry up to (total - 1):
//                 lfAheadness = Dot2D(dir, nextPortalPos2D - carPos2D)
//                 if (lfAheadness > lfBestSeparation) continue          (the scan runs backwards,
//                                                                        so these are negative)
//                 write mHelperNodeNext (the PREV portal) / mHelperNodePrev (the NEXT portal)
//                 hand both back; lfBestSeparation = lfAheadness
//                 if (lfAheadness >= lfResetDistance) { advance the pair; continue }
//                 return !IsSimilar2D(the two node positions)   -- true == a usable straddle
//   0x82784BE4  fell off the end: accept only for eExtrapolateType_RoadRage, with both nodes set
//               and the best separation already past KF_LOOK_BACK_DISTANCE
//
// THE TWO OUT-REFERENCES ARE CROSSED, AND THE ASM IS THE ONLY WITNESS. 0x82784B40/0x82784B44 store
// `this+0x520` (mHelperNodeNext) through r20 -- the SECOND reference -- and `this+0x530`
// (mHelperNodePrev) through r15 -- the FIRST. Every caller then passes its first out-slot as
// ConvertNodesToPositionAndDirection's lpPrevNode and its second as lpNextNode, so
// lrpPrevNode == &mHelperNodePrev and lrpNextNode == &mHelperNodeNext. Swap them and
// ConvertNodes' direction (next - prev) points the reset car the wrong way down the road.
// =================================================================================================
bool ResetOnTrackManager::ScanBackwardsAlongExtrapolatedRoute(const RouteNode*& lrpPrevNode,
                                                              const RouteNode*& lrpNextNode,
                                                              f32 lfResetDistance,
                                                              EExtrapolateType leExtrapolateType)
{
    const AISectionsData* lpAISectionsData = mpAISectionData.GetMemoryResource();
    if (lpAISectionsData == 0)
    {
        // [GUARD] Construct binds the network; no network is a Prepare-order error, not a frame
        // condition. The console dereferences it unconditionally.
        return false;
    }

    const AICar* lpPlayerAICar = GetAICar(mePlayerGlobalRaceCarIndex);

    const Vector2 lPlayerDirection = BrnMath::Flatten(lpPlayerAICar->GetDirection());
    const Vector2 lPlayerPosition  = BrnMath::Flatten(lpPlayerAICar->GetPosition());

    u16 luStartSection = lpPlayerAICar->muBestSectionIndex;
    if (luStartSection == KU_INVALID_SECTION_INDEX)
    {
        luStartSection = lpPlayerAICar->muDefaultSectionIndex;
    }

    ExtrapolatedIndexArray lGeneratedForwardIndices;
    ExtrapolatedIndexArray lGeneratedIndices;
    lGeneratedForwardIndices.SetFullCount();   // stw 16, +0x80 (both arrays, 0x82784830/0x82784834)
    lGeneratedIndices.SetFullCount();

    const s32 liForwardSectionsGenerated = RacingLineGenerator::ExtrapolateRouteForwards(
        1, static_cast<s32>(luStartSection), lPlayerDirection, lPlayerPosition,
        lpAISectionsData, lGeneratedForwardIndices);

    // The console re-reads the section indices here rather than reusing luStartSection; kept.
    u16 luBackwardsStartSection = lpPlayerAICar->muBestSectionIndex;
    if (luBackwardsStartSection == KU_INVALID_SECTION_INDEX)
    {
        luBackwardsStartSection = lpPlayerAICar->muDefaultSectionIndex;
    }

    const s32 liExtrapolationLength = (leExtrapolateType == eExtrapolateType_RaceStart) ? 16 : 8;

    const s32 liSectionsGenerated = RacingLineGenerator::ExtrapolateRouteBackwards(
        liExtrapolationLength, luBackwardsStartSection, lPlayerDirection, lPlayerPosition,
        lpAISectionsData, lGeneratedIndices);

    const s32 liTotalGenerated = liSectionsGenerated + liForwardSectionsGenerated;
    if (liTotalGenerated <= 1)
    {
        return false;
    }

    u32 luPrevSectionIndex;
    u32 luPrevPortalIndex;
    s32 liResetNodeIndex;

    if (liForwardSectionsGenerated != 0)
    {
        luPrevSectionIndex = lGeneratedForwardIndices[0].muSection;
        luPrevPortalIndex  = lGeneratedForwardIndices[0].muPortal;
        liResetNodeIndex   = 0;
    }
    else
    {
        luPrevSectionIndex = lGeneratedIndices[0].muSection;
        luPrevPortalIndex  = lGeneratedIndices[0].muPortal;
        liResetNodeIndex   = 1;
    }

    const s32 liLastNodeIndex = liTotalGenerated - 1;

    lrpPrevNode = 0;    // stw r27, 0(r15) / 0(r20) -- both cleared before the scan
    lrpNextNode = 0;

    f32 lfBestSeparation = KF_ZERO;

    for (; liResetNodeIndex < liLastNodeIndex; ++liResetNodeIndex)
    {
        const u32 luNextAISectionIndex = lGeneratedIndices[static_cast<u32>(liResetNodeIndex)].muSection;
        const u32 luNextAIPortalIndex  = lGeneratedIndices[static_cast<u32>(liResetNodeIndex)].muPortal;

        // Both bounds asserts are the console's own (AISectionsData.h:1201, inlined GetAISection).
        const AISection* lpPrevSection = lpAISectionsData->GetAISection(luPrevSectionIndex);
        const AISection* lpNextSection = lpAISectionsData->GetAISection(luNextAISectionIndex);

        const u8 lu8PrevPortal = static_cast<u8>(luPrevPortalIndex);
        const u8 lu8NextPortal = static_cast<u8>(luNextAIPortalIndex);

        const Portal* lpPrevPortal = lpPrevSection->GetPortal(lu8PrevPortal);
        const Portal* lpNextPortal = lpNextSection->GetPortal(lu8NextPortal);

        // `lfs 0(portal) ; lfs 8(portal)` -- the portal's X and Z, i.e. its ground-plane position.
        const Vector2 lPrevPortalPosition2D =
            { lpPrevPortal->GetPositionX(), lpPrevPortal->GetPositionZ(), 0.0f, 0.0f };
        const Vector2 lNextPortalPosition2D =
            { lpNextPortal->GetPositionX(), lpNextPortal->GetPositionZ(), 0.0f, 0.0f };

        const Vector2 lRelative = { lNextPortalPosition2D.x - lPlayerPosition.x,
                                    lNextPortalPosition2D.y - lPlayerPosition.y, 0.0f, 0.0f };
        const f32 lfAheadness = Dot2D(lPlayerDirection, lRelative);

        if (lfAheadness > lfBestSeparation)
        {
            continue;
        }

        lfBestSeparation = lfAheadness;

        WriteHelperNode(mHelperNodeNext, lPrevPortalPosition2D.x, lPrevPortalPosition2D.y,
                        static_cast<u16>(luPrevSectionIndex), lu8PrevPortal);
        WriteHelperNode(mHelperNodePrev, lNextPortalPosition2D.x, lNextPortalPosition2D.y,
                        static_cast<u16>(luNextAISectionIndex), lu8NextPortal);

        lrpNextNode = &mHelperNodeNext;
        lrpPrevNode = &mHelperNodePrev;

        // NaN polarity (FX-AINAN2): `fcmpu aheadness, lfResetDistance ; bge` @0x82784B14/
        // 0x82784B48 is taken on an unordered compare -- a NaN aheadness (or distance) keeps
        // walking instead of accepting the pair; `>=` accepted it.
        if (!(lfAheadness < lfResetDistance))
        {
            // Not far enough behind yet: this portal becomes the next pair's "previous".
            luPrevSectionIndex = luNextAISectionIndex;
            luPrevPortalIndex  = lu8NextPortal;   // `LOBYTE(v30) = v35` -- only the byte survives
            continue;
        }

        const Vector2 lNextNodePosition = { lrpNextNode->mfX, lrpNextNode->mfY, 0.0f, 0.0f };
        const Vector2 lPrevNodePosition = { lrpPrevNode->mfX, lrpPrevNode->mfY, 0.0f, 0.0f };
        if (!IsSimilar2D(lNextNodePosition, lPrevNodePosition))
        {
            return true;
        }
        // Degenerate pair (both portals in the same place): keep scanning.
    }

    if (leExtrapolateType == eExtrapolateType_RoadRage && lrpNextNode != 0 && lrpPrevNode != 0 &&
        lfBestSeparation < KF_LOOK_BACK_DISTANCE)
    {
        return true;
    }

    return false;
}

// =================================================================================================
// ScanForwardsAlongExtrapolatedRoute @0x82784C40 (DWARF :266)
//
// The mirror of the backwards scan, with three differences the asm pins:
//   * ONE extrapolation only -- ExtrapolateRouteForwards(16, ...) @0x82784D14 (`li r3, 0x10`).
//   * the scan STARTS at index 1 and stops at (generated - 1), and refuses outright when that
//     span is empty (`cmpwi r15, 1 ; ble` @0x82784D58).
//   * it SKIPS any previous section flagged as a shortcut: `lbz r11, 0x17(prev)` then
//     `rlwinm r8, r11, 0,25,25` (bit 0x40 == IsAIShortcut) and `clrlwi r11,r11,31` (bit 0x01 ==
//     IsShortcut) @0x82784E24..0x82784E4C. A car must not be reset onto a shortcut.
//   * the accept test is `lfAheadness > lfResetDistance` (ahead, not behind), and there is no
//     trailing "ran out of road" arm -- it just returns false.
// =================================================================================================
bool ResetOnTrackManager::ScanForwardsAlongExtrapolatedRoute(const RouteNode*& lrpPrevNode,
                                                             const RouteNode*& lrpNextNode,
                                                             f32 lfResetDistance)
{
    const AISectionsData* lpAISectionsData = mpAISectionData.GetMemoryResource();
    if (lpAISectionsData == 0)
    {
        return false;   // [GUARD] see ScanBackwardsAlongExtrapolatedRoute
    }

    const AICar* lpPlayerAICar = GetAICar(mePlayerGlobalRaceCarIndex);

    const Vector2 lPlayerDirection = BrnMath::Flatten(lpPlayerAICar->GetDirection());
    const Vector2 lPlayerPosition  = BrnMath::Flatten(lpPlayerAICar->GetPosition());

    u16 luStartSection = lpPlayerAICar->muBestSectionIndex;
    if (luStartSection == KU_INVALID_SECTION_INDEX)
    {
        luStartSection = lpPlayerAICar->muDefaultSectionIndex;
    }

    ExtrapolatedIndexArray lGeneratedIndices;
    lGeneratedIndices.SetFullCount();

    const s32 liSectionsGenerated = RacingLineGenerator::ExtrapolateRouteForwards(
        16, static_cast<s32>(luStartSection), lPlayerDirection, lPlayerPosition,
        lpAISectionsData, lGeneratedIndices);

    if (liSectionsGenerated <= 1)
    {
        return false;
    }

    u32 luPrevSectionIndex = lGeneratedIndices[0].muSection;
    u32 luPrevPortalIndex  = lGeneratedIndices[0].muPortal;

    const s32 liLastNodeIndex = liSectionsGenerated - 1;
    if (liLastNodeIndex <= 1)
    {
        return false;
    }

    for (s32 liResetNodeIndex = 1; liResetNodeIndex < liLastNodeIndex; ++liResetNodeIndex)
    {
        const u32 luNextAISectionIndex = lGeneratedIndices[static_cast<u32>(liResetNodeIndex)].muSection;
        const u32 luNextAIPortalIndex  = lGeneratedIndices[static_cast<u32>(liResetNodeIndex)].muPortal;

        const AISection* lpPrevSection = lpAISectionsData->GetAISection(luPrevSectionIndex);
        const AISection* lpNextSection = lpAISectionsData->GetAISection(luNextAISectionIndex);

        if (lpPrevSection->IsAIShortcut() || lpPrevSection->IsShortcut())
        {
            continue;   // 0x82784E40 / 0x82784E4C -- both jump straight to the loop increment
        }

        const u8 lu8PrevPortal = static_cast<u8>(luPrevPortalIndex);
        const u8 lu8NextPortal = static_cast<u8>(luNextAIPortalIndex);

        const Portal* lpPrevPortal = lpPrevSection->GetPortal(lu8PrevPortal);
        const Portal* lpNextPortal = lpNextSection->GetPortal(lu8NextPortal);

        const Vector2 lPrevPortalPosition2D =
            { lpPrevPortal->GetPositionX(), lpPrevPortal->GetPositionZ(), 0.0f, 0.0f };
        const Vector2 lNextPortalPosition2D =
            { lpNextPortal->GetPositionX(), lpNextPortal->GetPositionZ(), 0.0f, 0.0f };

        WriteHelperNode(mHelperNodeNext, lPrevPortalPosition2D.x, lPrevPortalPosition2D.y,
                        static_cast<u16>(luPrevSectionIndex), lu8PrevPortal);
        WriteHelperNode(mHelperNodePrev, lNextPortalPosition2D.x, lNextPortalPosition2D.y,
                        static_cast<u16>(luNextAISectionIndex), lu8NextPortal);

        lrpNextNode = &mHelperNodeNext;
        lrpPrevNode = &mHelperNodePrev;

        const Vector2 lRelative = { lNextPortalPosition2D.x - lPlayerPosition.x,
                                    lNextPortalPosition2D.y - lPlayerPosition.y, 0.0f, 0.0f };
        const f32 lfAheadness = Dot2D(lPlayerDirection, lRelative);

        // NaN polarity (FX-AINAN2): `fcmpu aheadness, lfResetDistance ; ble` @0x82784F14/
        // 0x82784F18 is taken on an unordered compare -- a NaN keeps walking; `<=` accepted it.
        if (!(lfAheadness > lfResetDistance))
        {
            luPrevSectionIndex = luNextAISectionIndex;
            luPrevPortalIndex  = lu8NextPortal;
            continue;
        }

        const Vector2 lNextNodePosition = { lrpNextNode->mfX, lrpNextNode->mfY, 0.0f, 0.0f };
        const Vector2 lPrevNodePosition = { lrpPrevNode->mfX, lrpPrevNode->mfY, 0.0f, 0.0f };
        if (!IsSimilar2D(lNextNodePosition, lPrevNodePosition))
        {
            return true;
        }
    }

    return false;
}

// =================================================================================================
// GetRoadSideForStartingLine @0x82784378 (DWARF :242)
//
//   0x827843A8  lNextPortal = GetAISection(lpNextNode->section)->GetPortal(lpNextNode->portal)
//   0x827843B0  assert(lu8BoundryIndex < mu8NumBoundaryLines)   (AISectionsData.h:811, the
//               inlined GetBoundaryLine(0) bounds check)
//   0x827843E0  lfRoadWidth = lpNextBoundaryLine->GetLength()
//   0x827843F4  if (lfRoadWidth == 0.0) return 0.5              (dead-centre)
//   0x82784418  lfInterp = mRandom.RandomFloat() * 2.0 + 8.0/lfRoadWidth - 1.0
//               (the unrolled draw at this+0x4F0 IS CgsNumeric::Random::RandomFloat -- read the
//                oldest ring slot, refill it from the OLD seed's high word, step the LCG, advance
//                the cursor. The committed body already returns the [1,2) value MINUS ONE, which
//                is the console's `fsubs f12, f12, 1.0`.)
//   0x82784490  lfInterp = Clamp(lfInterp, -0.25, 0.25)         (the two fsel pairs)
//   0x827844B8  return (liRaceCarIndex & 1) ? 0.5 - lfInterp : lfInterp + 0.5
//
// THE ODD/EVEN MIRROR IS WHAT MAKES A GRID. Cars 1,3,5 sit on one side of the road centre and
// 2,4 on the other; the random term only jitters how far. Drop it and every rival spawns on the
// same racing line, inside each other.
// =================================================================================================
f32 ResetOnTrackManager::GetRoadSideForStartingLine(const RouteNode* lpNextNode, s32 liRaceCarIndex)
{
    CGS_ASSERT(lpNextNode != 0, "lpNextNode != NULL");
    if (lpNextNode == 0 || !mpAISectionData.HasMemoryResource())
    {
        return KF_ROAD_CENTRE;   // [GUARD] see ScanBackwardsAlongExtrapolatedRoute
    }

    const AISection* lpNextSection =
        mpAISectionData.operator->()->GetAISection(lpNextNode->muSectionIndex);
    const Portal* lNextPortal = lpNextSection->GetPortal(GetHelperNodePortalIndex(*lpNextNode));

    const BoundaryLine* lpNextBoundaryLine = lNextPortal->GetBoundaryLine(0);

    const f32 lfRoadWidth = lpNextBoundaryLine->GetLength();
    if (lfRoadWidth == 0.0f)
    {
        return KF_ROAD_CENTRE;
    }

    f32 lfInterp = mRandom.RandomFloat() * KF_STARTING_LINE_SPREAD
                 + (KF_STARTING_LINE_CAR_WIDTH / lfRoadWidth)
                 - 1.0f;

    // rw::math::vpu::Clamp, lowered by the console to two `fsubs ; fsel` pairs in this order
    // (0x82784498..0x827844B0): t = (-0.25 - x >= 0) ? -0.25 : x, then (0.25 - t >= 0) ? t : 0.25.
    // fsel takes its THIRD operand on an unordered test, so a NaN comes back as +0.25
    // (FX-AINAN2); the old `if (x < lo) x = lo; if (x > hi) x = hi;` kept the NaN.
    lfInterp = ((-KF_STARTING_LINE_MAX_OFFSET - lfInterp) >= 0.0f) ? -KF_STARTING_LINE_MAX_OFFSET
                                                                  : lfInterp;
    lfInterp = ((KF_STARTING_LINE_MAX_OFFSET - lfInterp) >= 0.0f) ? lfInterp
                                                                 : KF_STARTING_LINE_MAX_OFFSET;

    if ((liRaceCarIndex & 1) != 0)
    {
        return KF_ROAD_CENTRE - lfInterp;
    }
    return lfInterp + KF_ROAD_CENTRE;
}

// =================================================================================================
// DeterminePositionBetweenNodes @0x82785DC8 (DWARF :330)
//
//   0x82785E00  lPlayerPosition2D  = Flatten(lpPlayerAICar->GetPosition())     (v126)
//   0x82785E28  lPlayerDirection2D = Flatten(lpPlayerAICar->GetDirection())    (v127)
//   0x82785E3C  lfPrevAheadness = Dot2D(dir, Flatten(prev) - playerPos)
//   0x82785E64  lfNextAheadness = Dot2D(dir, Flatten(next) - playerPos)
//   0x82785E94  if (lfNextAheadness - lfPrevAheadness == 0.0)
//                   log "<AI> Nodes in same place\n" ; return lPrevPortalPosition
//   0x82785ED0  lfInterp = (lfResetDistance - lfPrevAheadness) / (next - prev)
//               log "Lerping <t> from <prev> to <next>\n"
//   0x82785F3C  if (lfInterp <  0.0) return lPrevPortalPosition
//   0x82785F4C  if (lfInterp >  1.0) return lNextPortalPosition
//   0x82785F5C  return prev + (next - prev) * lfInterp
//
// The two Flatten calls are BrnMath::Flatten, not a lane shuffle: `bl BrnMath__Flatten` twice, with
// the vector argument passed in v1 and returned in v1.
// =================================================================================================
Vector3 ResetOnTrackManager::DeterminePositionBetweenNodes(Vector3 lPrevPortalPosition,
                                                           Vector3 lNextPortalPosition,
                                                           const AICar* lpPlayerAICar,
                                                           f32 lfResetDistance)
{
    const Vector2 lPlayerPosition2D  = BrnMath::Flatten(lpPlayerAICar->GetPosition());
    const Vector2 lPlayerDirection2D = BrnMath::Flatten(lpPlayerAICar->GetDirection());

    const Vector2 lPrev2D = BrnMath::Flatten(lPrevPortalPosition);
    const Vector2 lNext2D = BrnMath::Flatten(lNextPortalPosition);

    const Vector2 lPrevRelative = { lPrev2D.x - lPlayerPosition2D.x,
                                    lPrev2D.y - lPlayerPosition2D.y, 0.0f, 0.0f };
    const Vector2 lNextRelative = { lNext2D.x - lPlayerPosition2D.x,
                                    lNext2D.y - lPlayerPosition2D.y, 0.0f, 0.0f };

    const f32 lfPrevAheadness = Dot2D(lPrevRelative, lPlayerDirection2D);
    const f32 lfNextAheadness = Dot2D(lNextRelative, lPlayerDirection2D);

    if ((lfNextAheadness - lfPrevAheadness) == 0.0f)
    {
        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint << "<AI> Nodes in same place\n";
        }
        return lPrevPortalPosition;
    }

    const f32 lfInterp = (lfResetDistance - lfPrevAheadness) / (lfNextAheadness - lfPrevAheadness);

    if (lfInterp < 0.0f)
    {
        return lPrevPortalPosition;
    }
    if (lfInterp > 1.0f)
    {
        return lNextPortalPosition;
    }

    return Vector3{ (lNextPortalPosition.x - lPrevPortalPosition.x) * lfInterp + lPrevPortalPosition.x,
                    (lNextPortalPosition.y - lPrevPortalPosition.y) * lfInterp + lPrevPortalPosition.y,
                    (lNextPortalPosition.z - lPrevPortalPosition.z) * lfInterp + lPrevPortalPosition.z,
                    0.0f };
}

// =================================================================================================
// ConvertNodesToPositionAndDirection @0x82790300 (DWARF :294)
//
//   0x82790344  if (!lpPrevNode || !lpNextNode) return false
//   0x82790360  assert(lpPrevNode != lpNextNode)                        (:708)
//   0x82790394  log "Converting nodes to position & direction\n"
//   0x827903B0  lpPrevSection / lpNextSection = GetAISection(node->section)
//   0x827903E0  lpResetData->mpAISection = lpNextSection                ⭐ the NEXT section
//   0x82790408  the FIRST boundary line of each node's portal, interpolated at lfRoadSide
//   0x82790480  if (IsSimilar2D(prevInterp, nextInterp)) return false
//   0x82790494  the two 3D portal poses: the boundary-line interpolant supplies X and Z, the
//               PORTAL's own mPositionY supplies Y (`lfs f0, 4(portal)` for each)
//   0x8279052C  lpResetData->mPosition = DeterminePositionBetweenNodes(prev, next, playerCar,
//                                                                      lfResetDistance)
//   0x82790588  assert(!IsSimilar(lNextPortalPosition, lPrevPortalPosition))   (:758)
//   0x827905A8  lpResetData->mDirection = Normalise(next - prev)
//
// lfRoadSide IS THE LATERAL PARAMETER AND lfResetDistance THE LONGITUDINAL ONE -- f1 and f2, two
// floats in a row, trivially swappable. f1 reaches BoundaryLine::GetInterp (the across-the-road
// lerp) and f2 reaches DeterminePositionBetweenNodes (the along-the-road lerp).
// =================================================================================================
bool ResetOnTrackManager::ConvertNodesToPositionAndDirection(const RouteNode* lpPrevNode,
                                                             const RouteNode* lpNextNode,
                                                             f32 lfRoadSide, f32 lfResetDistance,
                                                             ResetOnTrackCoords* lpResetData)
{
    if (lpPrevNode == 0 || lpNextNode == 0)
    {
        return false;
    }

    CGS_ASSERT(lpPrevNode != lpNextNode, "lpPrevNode != lpNextNode");   // :708

    if (lpResetData == 0 || !mpAISectionData.HasMemoryResource())
    {
        return false;   // [GUARD] see ScanBackwardsAlongExtrapolatedRoute
    }

    if (CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint << "Converting nodes to position & direction\n";
    }

    const AISectionsData* lpAISectionsData = mpAISectionData.operator->();

    const AISection* lpPrevSection = lpAISectionsData->GetAISection(lpPrevNode->muSectionIndex);
    const AISection* lpNextSection = lpAISectionsData->GetAISection(lpNextNode->muSectionIndex);

    lpResetData->mpAISection = lpNextSection;

    const Portal* lpPrevPortal = lpPrevSection->GetPortal(GetHelperNodePortalIndex(*lpPrevNode));
    const Portal* lpNextPortal = lpNextSection->GetPortal(GetHelperNodePortalIndex(*lpNextNode));

    Vector2 lPrevInterp;
    Vector2 lNextInterp;
    lpPrevPortal->GetBoundaryLine(0)->GetInterp(&lPrevInterp, lfRoadSide);
    lpNextPortal->GetBoundaryLine(0)->GetInterp(&lNextInterp, lfRoadSide);

    if (IsSimilar2D(lPrevInterp, lNextInterp))
    {
        return false;
    }

    const Vector3 lPrevPortalPosition = { lPrevInterp.x, lpPrevPortal->GetPositionY(),
                                          lPrevInterp.y, 0.0f };
    const Vector3 lNextPortalPosition = { lNextInterp.x, lpNextPortal->GetPositionY(),
                                          lNextInterp.y, 0.0f };

    const AICar* lpPlayerAICar = GetAICar(mePlayerGlobalRaceCarIndex);

    lpResetData->mPosition = DeterminePositionBetweenNodes(lPrevPortalPosition, lNextPortalPosition,
                                                           lpPlayerAICar, lfResetDistance);

    const Vector3 lDelta = { lNextPortalPosition.x - lPrevPortalPosition.x,
                             lNextPortalPosition.y - lPrevPortalPosition.y,
                             lNextPortalPosition.z - lPrevPortalPosition.z, 0.0f };

    CGS_ASSERT(!IsSimilar3D(lNextPortalPosition, lPrevPortalPosition),
               "!RwMath::IsSimilar( lNextPortalPosition, lPrevPortalPosition )");   // :758

    lpResetData->mDirection = Normalise3D(lDelta);
    return true;
}

// =================================================================================================
// EnsureAIIsDrivingSameDirectionAsPlayer @0x82778088 (DWARF :230)
//
//   0x827780BC  v127 = lpResetData->mDirection                          (+0x20)
//   0x827780D0  lfDot = Dot(lpPlayerAICar->GetDirection(), v127)
//   0x827780E8  if (0.0 > lfDot)  lpResetData->mDirection = -mDirection
//               (`vspltisw -1 ; vslw ; vxor` == flip every lane's sign bit)
// =================================================================================================
void ResetOnTrackManager::EnsureAIIsDrivingSameDirectionAsPlayer(const AICar* lpPlayerAICar,
                                                                 ResetOnTrackCoords* lpResetData)
{
    const f32 lfDot = Dot3D(lpPlayerAICar->GetDirection(), lpResetData->mDirection);

    if (0.0f > lfDot)
    {
        lpResetData->mDirection = Vector3{ -lpResetData->mDirection.x,
                                           -lpResetData->mDirection.y,
                                           -lpResetData->mDirection.z, 0.0f };
    }
}

// =================================================================================================
// ResetNearRoutelessPlayer @0x827844D8 (DWARF :306) -- the RACE-START strategy's fallback.
//
//   0x82784500  liResetOnTrackIndex = lpPlayerAICar->muResetOnTrackSectionIndex   (lhz 0x1530)
//   0x82784504  if (invalid) { log "<AI> routeless fail\n" ; return false }
//   0x82784554  lpResetData->mpAISection = GetAISection(liResetOnTrackIndex)
//   0x8278455C  lPrevPortal = section->GetPortal(car->muResetOnTrackStartPortal)  (lbz 0x1538)
//   0x82784594  lNextPortal = section->GetPortal(car->muResetOnTrackEndPortal)    (lbz 0x1539)
//   0x8278460C  assert(!RwMath::IsSimilar( lNextPortal, lPrevPortal ))            (:1199)
//   0x82784644  lpResetData->mPosition  = lPrevPortal                             (+0x10)
//   0x82784688  lpResetData->mDirection = Normalise(lNextPortal - lPrevPortal)    (+0x20)
//   0x8278468C  lPlayerRelativePosition = mPosition - lpPlayerAICar->GetPosition()
//               (normalised, unless it is within FLT_EPSILON of zero)
//   0x82784718  lfAheadness = Dot(lPlayerRelativePosition, lpPlayerAICar->GetDirection())
//   0x82784734  if (lfAheadness > 0.0)
//                   log "<AI> Car tried to start ahead of player. Forced behind instead.\n"
//                   mPosition = playerPos - playerDir * KF_DISTANCE_BEHIND_PLAYER(20.0)
//
// NOTE the portal positions here are the portals' OWN 3D positions (x,y,z at +0/+4/+8), not a
// boundary-line interpolation: this arm has no road-side parameter to spend.
// =================================================================================================
bool ResetOnTrackManager::ResetNearRoutelessPlayer(ResetOnTrackCoords* lpResetData)
{
    const AICar* lpPlayerAICar = GetAICar(mePlayerGlobalRaceCarIndex);

    const u16 liResetOnTrackIndex = lpPlayerAICar->muResetOnTrackSectionIndex;
    if (liResetOnTrackIndex == KU_INVALID_SECTION_INDEX)
    {
        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint << "<AI> routeless fail\n";
        }
        return false;
    }

    if (lpResetData == 0 || !mpAISectionData.HasMemoryResource())
    {
        return false;   // [GUARD] see ScanBackwardsAlongExtrapolatedRoute
    }

    const AISection* lpAISection =
        mpAISectionData.operator->()->GetAISection(liResetOnTrackIndex);
    lpResetData->mpAISection = lpAISection;

    const Portal* lpStartPortal = lpAISection->GetPortal(lpPlayerAICar->muResetOnTrackStartPortal);
    const Vector3 lPrevPortal = { lpStartPortal->GetPositionX(), lpStartPortal->GetPositionY(),
                                  lpStartPortal->GetPositionZ(), 0.0f };

    const Portal* lpEndPortal = lpAISection->GetPortal(lpPlayerAICar->muResetOnTrackEndPortal);
    const Vector3 lNextPortal = { lpEndPortal->GetPositionX(), lpEndPortal->GetPositionY(),
                                  lpEndPortal->GetPositionZ(), 0.0f };

    CGS_ASSERT(!IsSimilar3D(lNextPortal, lPrevPortal),
               "!RwMath::IsSimilar( lNextPortal, lPrevPortal )");   // :1199

    lpResetData->mPosition  = lPrevPortal;
    lpResetData->mDirection = Normalise3D(Vector3{ lNextPortal.x - lPrevPortal.x,
                                                   lNextPortal.y - lPrevPortal.y,
                                                   lNextPortal.z - lPrevPortal.z, 0.0f });

    const Vector3 lPlayerPosition = lpPlayerAICar->GetPosition();
    Vector3 lPlayerRelativePosition = { lpResetData->mPosition.x - lPlayerPosition.x,
                                        lpResetData->mPosition.y - lPlayerPosition.y,
                                        lpResetData->mPosition.z - lPlayerPosition.z, 0.0f };

    // 0x827846AC: normalise only when the offset is not (near) zero -- the guard uses FLT_EPSILON,
    // NOT the IsSimilar tolerance the assert above used.
    {
        const f32 lfAbsX = lPlayerRelativePosition.x < 0.0f ? -lPlayerRelativePosition.x
                                                            :  lPlayerRelativePosition.x;
        const f32 lfAbsY = lPlayerRelativePosition.y < 0.0f ? -lPlayerRelativePosition.y
                                                            :  lPlayerRelativePosition.y;
        const f32 lfAbsZ = lPlayerRelativePosition.z < 0.0f ? -lPlayerRelativePosition.z
                                                            :  lPlayerRelativePosition.z;
        if (lfAbsX > KF_NORMALISE_EPSILON || lfAbsY > KF_NORMALISE_EPSILON ||
            lfAbsZ > KF_NORMALISE_EPSILON)
        {
            lPlayerRelativePosition = Normalise3D(lPlayerRelativePosition);
        }
    }

    const Vector3 lPlayerDirection = lpPlayerAICar->GetDirection();
    const f32 lfAheadness = Dot3D(lPlayerRelativePosition, lPlayerDirection);

    if (lfAheadness > 0.0f)
    {
        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "<AI> Car tried to start ahead of player. Forced behind instead.\n";
        }

        const Vector3 lDirection = lpPlayerAICar->GetDirection();
        const Vector3 lPosition  = lpPlayerAICar->GetPosition();
        lpResetData->mPosition = Vector3{ lPosition.x - lDirection.x * KF_DISTANCE_BEHIND_PLAYER,
                                          lPosition.y - lDirection.y * KF_DISTANCE_BEHIND_PLAYER,
                                          lPosition.z - lDirection.z * KF_DISTANCE_BEHIND_PLAYER,
                                          0.0f };
    }

    return true;
}

// =================================================================================================
// ResetAwayFromPlayer @0x82784148 (DWARF :326) -- reset type 7, and the fallback of both
// fixed-distance strategies.
//
//   0x82784178  lPlayerPosition = lpPlayerAICar->GetPosition()
//   0x827841C0  for (liRepeats = 0; liRepeats < 10; ++liRepeats)
//                 luSection = (u32)(RandomFloat() * (f32)(u16)muNumSections)
//                 lpAISection = GetAISection(luSection)
//                 lOffset = lpAISection->GetMiddle() - lPlayerPosition
//                 if (Magnitude(lOffset) > 2000.0) -> accept
//   0x82784330  accept: mpAISection = lpAISection ; mPosition = GetMiddle()
//                       mDirection  = {1, 0, 0}                       ⭐ a LITERAL +X facing
//   0x82784314  ten misses -> return false
//
// ⭐ THE DRAW COMES FROM THE **GLOBAL** CgsNumeric::Random AT .data 0x8300D5D0 (0x827841A8/0x827841BC
// `lis/addi r31`; the inline RandomFloat at 0x827841C0..0x82784220), NOT from this object's mRandom
// (@this+0x4F0, which GetRoadSideForStartingLine uses). DWARF: the draw is AICar::GetRandomNumber()
// (BrnAICar.h:476; PS3 0xA122A8 calls Aggressiveness::GetRandomNumber on lpPlayerAICar+5132), and the
// stream is the one AICar::Construct (0x82792754..) and AICar::Reset (0x82792900..) re-seed -- so
// after any car's attach (HandleManagementEvents case 0 -> Reset, before this frame's
// UpdateResetOnTrackManager) the next draws are exactly 0.0, 0.783, 0.193 ... again.
// ⛔ CORRECTED 2026-09-22 (crash parity G07-D6): the draw came from a function-local copy
// (`lsGlobalResetRandom_8300D5D0`) Constructed once per process and never re-seeded, so every
// Marked Man load placement / fixed-distance fallback after the first probed a different section
// than the console. It now draws lpPlayerAICar->GetRandomNumber() (FX-AIDRV 1a611271 homed the
// stream beside AICar::Construct/Reset).
//
// ⚠️ THE SECTION INDEX IS DRAWN FROM A **u16-TRUNCATED** COUNT. 0x82784230 is
// `lwz r11, 0x30(data) ; clrlwi r11, r11, 16` -- muNumSections is narrowed to 16 bits BEFORE the
// float convert. With 7,639 sections that changes nothing today, but it is the console's arithmetic
// and a 70k-section map would wrap; reproduced rather than "fixed".
// =================================================================================================
bool ResetOnTrackManager::ResetAwayFromPlayer(ResetOnTrackCoords* lpResetData)
{
    if (lpResetData == 0 || !mpAISectionData.HasMemoryResource())
    {
        return false;   // [GUARD] see ScanBackwardsAlongExtrapolatedRoute
    }

    const AICar* lpPlayerAICar = GetAICar(mePlayerGlobalRaceCarIndex);
    const Vector3 lPlayerPosition = lpPlayerAICar->GetPosition();

    const AISectionsData* lpAISectionsData = mpAISectionData.operator->();

    for (s32 liRepeats = 0; liRepeats < KI_AWAY_FROM_PLAYER_MAX_REPEATS; ++liRepeats)
    {
        const f32 lfRandom = lpPlayerAICar->GetRandomNumber();   // [0, 1) -- 0x8300D5D0, 0x827841C0..0x82784220

        const u32 luSectionIndex = static_cast<u32>(
            static_cast<f32>(static_cast<u16>(lpAISectionsData->muNumSections)) * lfRandom);

        const AISection* lpAISection = lpAISectionsData->GetAISection(luSectionIndex);

        const Vector3 lMiddle = lpAISection->GetMiddle();
        const Vector3 lOffset = { lMiddle.x - lPlayerPosition.x,
                                  lMiddle.y - lPlayerPosition.y,
                                  lMiddle.z - lPlayerPosition.z, 0.0f };

        const f32 lfMagnitudeSquared = Dot3D(lOffset, lOffset);
        const f32 lfMagnitude = (lfMagnitudeSquared == 0.0f) ? 0.0f : sqrtf(lfMagnitudeSquared);

        if (lfMagnitude > KF_AWAY_FROM_PLAYER_MIN_DISTANCE)
        {
            lpResetData->mpAISection = lpAISection;
            lpResetData->mPosition   = lMiddle;
            // 0x82784334..0x82784368: {1.0, 0.0, 0.0, 0.0} built on the stack and stored to +0x20.
            lpResetData->mDirection  = Vector3{ 1.0f, 0.0f, 0.0f, 0.0f };
            return true;
        }
    }

    return false;
}

// =================================================================================================
// ResetFixedDistanceBehindPlayerAtStartOfRace @0x827908F0 (DWARF :239) -- ⭐ RESET TYPE 6, the one
// RaceCarEntityModule::PlaceRaceCarOnLoad sends every rival on the starting grid.
//
//   0x82790910  section = GetAICar(mePlayerGlobalRaceCarIndex)->best/default section
//               if invalid -> return false
//   0x82790964  ScanBackwardsAlongExtrapolatedRoute(prev, next, lfResetDistance,
//                                                   eExtrapolateType_RaceStart)   (`li r7, 0`)
//   0x82790978  on failure -> ResetNearRoutelessPlayer(lpResetData)
//   0x82790998  lfRoadSide = GetRoadSideForStartingLine(lpNextNode, leCarIndex)
//   0x827909B0  ConvertNodesToPositionAndDirection(prev, next, lfRoadSide, lfResetDistance, out)
//   0x827909C0  on success: lpResetData->mpAISection = GetAISection(lpNextNode->section)
//
// ⭐ THE THIRD ARGUMENT IS THE **REQUESTING CAR'S** GLOBAL INDEX AND IT IS THE ONLY REASON THE GRID
// IS A GRID. It reaches GetRoadSideForStartingLine's odd/even mirror; the rest of the function only
// ever looks at the PLAYER's AI car. Pass the player's index by mistake and all five rivals land on
// the same side of the road at 20 m spacing, i.e. in a line inside the barrier.
//
// ⚠️ mpAISection IS WRITTEN TWICE and the second write is not redundant: ConvertNodesToPosition-
// AndDirection already stored the NEXT section, and the console stores it again from
// lpNextNode->muSectionIndex here (0x827909C4 `lhz r31, 0xC(r30)`). Same value; kept because the
// asm keeps it.
// =================================================================================================
bool ResetOnTrackManager::ResetFixedDistanceBehindPlayerAtStartOfRace(ResetOnTrackCoords* lpResetData,
                                                                      f32 lfResetDistance,
                                                                      EGlobalRaceCarIndex leCarIndex)
{
    const AICar* lpPlayerAICar = GetAICar(mePlayerGlobalRaceCarIndex);

    u16 luSection = lpPlayerAICar->muBestSectionIndex;
    if (luSection == KU_INVALID_SECTION_INDEX)
    {
        luSection = lpPlayerAICar->muDefaultSectionIndex;
    }
    if (luSection == KU_INVALID_SECTION_INDEX)
    {
        return false;
    }

    const RouteNode* lpPrevNode = 0;
    const RouteNode* lpNextNode = 0;

    if (!ScanBackwardsAlongExtrapolatedRoute(lpPrevNode, lpNextNode, lfResetDistance,
                                             eExtrapolateType_RaceStart))
    {
        return ResetNearRoutelessPlayer(lpResetData);
    }

    const f32 lfRoadSide = GetRoadSideForStartingLine(lpNextNode, static_cast<s32>(leCarIndex));

    if (!ConvertNodesToPositionAndDirection(lpPrevNode, lpNextNode, lfRoadSide, lfResetDistance,
                                            lpResetData))
    {
        return false;
    }

    lpResetData->mpAISection =
        mpAISectionData.operator->()->GetAISection(lpNextNode->muSectionIndex);
    return true;
}

// =================================================================================================
// ResetFixedDistanceBehindPlayer @0x82790628 (DWARF :246) -- reset types 2 and 3.
//
// The same backwards walk as the race-start strategy, with three differences:
//   * the extrapolate type is eExtrapolateType_RoadRage (`li r7, 1` @0x82790690), which halves the
//     backwards budget to 8 sections AND enables the trailing "ran out of road" acceptance arm;
//   * the road-side interpolant is the fixed road CENTRE (flt_820C4168 == 0.5), not a per-car side;
//   * the failure fallback is ResetAwayFromPlayer, not ResetNearRoutelessPlayer;
//   * and it ends with EnsureAIIsDrivingSameDirectionAsPlayer, so a road-rage rival never spawns
//     head-on. (@0x8279075C; the two dev-log lines around it print the pose.)
// =================================================================================================
bool ResetOnTrackManager::ResetFixedDistanceBehindPlayer(ResetOnTrackCoords* lpResetData,
                                                         f32 lfResetDistance)
{
    const AICar* lpPlayerAICar = GetAICar(mePlayerGlobalRaceCarIndex);

    u16 luSection = lpPlayerAICar->muBestSectionIndex;
    if (luSection == KU_INVALID_SECTION_INDEX)
    {
        luSection = lpPlayerAICar->muDefaultSectionIndex;
    }
    if (luSection == KU_INVALID_SECTION_INDEX)
    {
        return false;
    }

    const RouteNode* lpPrevNode = 0;
    const RouteNode* lpNextNode = 0;

    if (!ScanBackwardsAlongExtrapolatedRoute(lpPrevNode, lpNextNode, lfResetDistance,
                                             eExtrapolateType_RoadRage))
    {
        return ResetAwayFromPlayer(lpResetData);
    }

    if (!ConvertNodesToPositionAndDirection(lpPrevNode, lpNextNode, KF_ROAD_CENTRE, lfResetDistance,
                                            lpResetData))
    {
        return false;
    }

    lpResetData->mpAISection =
        mpAISectionData.operator->()->GetAISection(lpNextNode->muSectionIndex);

    EnsureAIIsDrivingSameDirectionAsPlayer(lpPlayerAICar, lpResetData);
    return true;
}

// =================================================================================================
// ResetFixedDistanceAheadOfPlayer @0x827907D8 (DWARF :320) -- reset type 4
// (E_RESET_TYPE_AHEAD_PLAYER_ON_COMING).
//
// ScanForwards + the road centre + EnsureAIIsDrivingSameDirectionAsPlayer, and then the direction
// is NEGATED (0x827908C8 `vspltisw -1 ; vslw ; vxor` on lpResetData+0x20). The pair is not
// self-cancelling: Ensure* aligns the pose with the player's heading FIRST, and only then is it
// flipped, so the car reliably faces the player head-on instead of inheriting the road's own
// arbitrary portal ordering.
// =================================================================================================
bool ResetOnTrackManager::ResetFixedDistanceAheadOfPlayer(ResetOnTrackCoords* lpResetData,
                                                          f32 lfResetDistance)
{
    const AICar* lpPlayerAICar = GetAICar(mePlayerGlobalRaceCarIndex);

    u16 luSection = lpPlayerAICar->muBestSectionIndex;
    if (luSection == KU_INVALID_SECTION_INDEX)
    {
        luSection = lpPlayerAICar->muDefaultSectionIndex;
    }
    if (luSection == KU_INVALID_SECTION_INDEX)
    {
        return false;
    }

    const RouteNode* lpPrevNode = 0;
    const RouteNode* lpNextNode = 0;

    if (!ScanForwardsAlongExtrapolatedRoute(lpPrevNode, lpNextNode, lfResetDistance))
    {
        return ResetAwayFromPlayer(lpResetData);
    }

    if (!ConvertNodesToPositionAndDirection(lpPrevNode, lpNextNode, KF_ROAD_CENTRE, lfResetDistance,
                                            lpResetData))
    {
        return false;
    }

    lpResetData->mpAISection =
        mpAISectionData.operator->()->GetAISection(lpNextNode->muSectionIndex);

    EnsureAIIsDrivingSameDirectionAsPlayer(lpPlayerAICar, lpResetData);

    lpResetData->mDirection = Vector3{ -lpResetData->mDirection.x,
                                       -lpResetData->mDirection.y,
                                       -lpResetData->mDirection.z, 0.0f };
    return true;
}

// =================================================================================================
// InterpolatePositionFromAngle @0x82784FD8 (DWARF :365, source :1737; PS3 witness 0x9CA490)
//
//   r3 is the hidden return slot; v1 = lPlayerPosition, v2 = lPlayerDirection, v3 = the entrance
//   node, v4 = the exit node, f1 = lfAngleToJoin (a COSINE); `this` is unused.
//   0x82785008  lVectorFromPlayer = Flatten(entrance) - player           (0x82785010 vsubfp128)
//   0x82785014  zero (both lanes <= FLT_EPSILON)          -> return the EXIT node (0x827850C4)
//   0x827850CC  lfEntranceDot = |Dot2D(Normalise(lVectorFromPlayer), dir)|   (vandc sign)
//   0x82785144  the same for the exit node; zero           -> return the EXIT node
//   0x8278525C  lfRange = lfExitDot - lfEntranceDot ; == 0.0 (flt_82001CC0) -> the EXIT node
//   0x8278526C  lfInterp = (lfAngleToJoin - lfEntranceDot) / lfRange
//   0x8278527C  lfInterp < 0.0 -> the ENTRANCE node ; 0x8278528C  > 1.0 (flt_82001C98) -> EXIT
//   0x827852A0  vmaddfp128 v121 = (exit - entrance) * lfInterp + entrance   (all four lanes)
// =================================================================================================
Vector3 ResetOnTrackManager::InterpolatePositionFromAngle(Vector2 lPlayerPosition,
                                                          Vector2 lPlayerDirection,
                                                          Vector3 lEntranceNodePosition,
                                                          Vector3 lExitNodePosition,
                                                          f32 lfAngleToJoin)
{
    const Vector2 lFlatEntrance = BrnMath::Flatten(lEntranceNodePosition);
    Vector2 lVectorFromPlayer = { lFlatEntrance.x - lPlayerPosition.x,
                                  lFlatEntrance.y - lPlayerPosition.y, 0.0f, 0.0f };
    if (IsZero2D(lVectorFromPlayer))
    {
        return lExitNodePosition;
    }
    const f32 lfEntranceDot = fabsf(Dot2D(Normalise2D(lVectorFromPlayer), lPlayerDirection));

    const Vector2 lFlatExit = BrnMath::Flatten(lExitNodePosition);
    lVectorFromPlayer = Vector2{ lFlatExit.x - lPlayerPosition.x,
                                 lFlatExit.y - lPlayerPosition.y, 0.0f, 0.0f };
    if (IsZero2D(lVectorFromPlayer))
    {
        return lExitNodePosition;
    }
    const f32 lfExitDot = fabsf(Dot2D(Normalise2D(lVectorFromPlayer), lPlayerDirection));

    const f32 lfRange = lfExitDot - lfEntranceDot;
    if (lfRange == KF_ZERO)
    {
        return lExitNodePosition;
    }

    const f32 lfUse    = lfAngleToJoin - lfEntranceDot;
    const f32 lfInterp = lfUse / lfRange;
    if (lfInterp < KF_ZERO)
    {
        return lEntranceNodePosition;
    }
    if (lfInterp > 1.0f)
    {
        return lExitNodePosition;
    }

    return Vector3{ (lExitNodePosition.x - lEntranceNodePosition.x) * lfInterp + lEntranceNodePosition.x,
                    (lExitNodePosition.y - lEntranceNodePosition.y) * lfInterp + lEntranceNodePosition.y,
                    (lExitNodePosition.z - lEntranceNodePosition.z) * lfInterp + lEntranceNodePosition.z,
                    (lExitNodePosition.w - lEntranceNodePosition.w) * lfInterp + lEntranceNodePosition.w };
}

// =================================================================================================
// ScanForwardsAndAlongJunction @0x827852C0 (DWARF :282, source :1803) -- reset type 5's FIRST try.
// X360 EXPORT HOLE: read with tools/re/ppcdis.py 0x827852C0 706 (to 0x82785DC8); PS3 DecFIGS
// export 0xA164E4 is the second witness and carries the DWARF local names used below.
//
//   0x827852E4  lpPlayerAICar = GetAICar(mePlayerGlobalRaceCarIndex)      (lpRoute == the car +0)
//   0x827852F0  liPlayerNextNodeIndex = miNextRouteNodeIndex (+0x1524), minus one when > 0
//   0x82785300  liLastNodeIndex = lpRoute->GetNodeCount() - 1              (lwz +0x1400)
//   0x82785304..0x82785374  ONE RandomFloat on the 0x8300D5D0 stream -- the ring slot is
//               refilled, the seed stepped and the cursor advanced, but the drawn value is never
//               loaded (no lfsx): the draw is DEAD on the console and still CONSUMES a number.
//               PS3 0xA164E4 calls Aggressiveness::GetRandomNumber and drops f1 the same way.
//   0x827853A8  for each node from liPlayerNextNodeIndex while < liLastNodeIndex:
//                 section = GetAISection(node->muSectionIndex)
//                 if (|playerPos - section->GetMiddle()| >= 100.0 && section->IsJunction())
//                     lbJunctionFound, stop                                  (flag 0x10 @+0x17)
//                 lpEntrySection = section                                   (r24)
//   0x827854B8  no junction, or the junction is the FIRST node scanned (no entry) -> false
//   0x827854CC  lJunctionMiddle = Flatten(junction->GetMiddle())                     (v121)
//   0x827854E4  node + 1 must exist; lSectionAfterJunctionMiddle from ITS section
//   0x8278552C  lfRouteDirection = Normalise(afterMiddle - junctionMiddle); zero -> false  (v123)
//   0x82785658  for each junction portal: the linked section, skipping the entry section and
//               any SHORTCUT (bit 0x01); lfAheadness = Dot2D(Normalise(junctionMiddle - side),
//               lfRouteDirection); keep the largest POSITIVE one (`blt f30` then `ble f31`), and
//               the junction portal's own position as lEntranceNodePosition            (v120)
//               -- ble 0x827857F8 (bc 4,gt) is taken on unordered, so a NaN |aheadness| is
//               SKIPPED; only an ordered |a| > best is kept
//               (blt 0x827857EC lets a NaN through to it)
//   0x82785848  no side road -> false
//   0x82785850  lPlayerDirection = Flatten(player->GetDirection())         (v123 reused)
//   0x82785874  up to KI_SIDE_SCAN_BAIL_OUT (10) steps OUT along the side road: from the current
//               section take the linked section (never the junction itself) whose middle is
//               FARTHEST from lJunctionMiddle, remembering that portal as lExitNodePosition
//               (v122) -- ble 0x8278596C skips a NaN distance as it skips a nearer one;
//               none -> false; a section with no portals -> false. Then, unless the exit
//               portal sits on the player (both lanes <= FLT_EPSILON), stop as soon as its
//               bearing |Dot2D(Normalise(Flatten(exit - playerPos)), lPlayerDirection)| drops
//               below cos 20 deg; otherwise the exit becomes the next entrance (0x82785AF8).
//   0x82785B34  lpResetData->mPosition = InterpolatePositionFromAngle(Flatten(playerPos),
//               lPlayerDirection, entrance, exit, cos 20 deg)
//   0x82785B50  lfSeparation = |mPosition - playerPos|; 50.0 > it -> false (vcmpgtfp. all)
//   0x82785BE4  lfAheadness = Dot(Normalise-unless-zero(mPosition - playerPos), player direction);
//               `bge` 0x82785CE4 (aheadness vs cos 40 deg) and `bge` 0x82785CF4 (lfSeparation vs
//               100.0) both jump past the test, and bge is taken on unordered too: only an
//               ORDERED aheadness < cos 40 AND an ORDERED separation < 100 -> "Pop up
//               prevented\n" (printed when gxMessageFilterFlags bit 0 is set, 0x82785CF8) -> false
//   0x82785D28  lDirection3D = entrance - exit, STORED to mDirection before the zero test;
//               zero -> false; else mpAISection = the last side section, mDirection normalised
// =================================================================================================
bool ResetOnTrackManager::ScanForwardsAndAlongJunction(ResetOnTrackCoords* lpResetData)
{
    const AICar* lpPlayerAICar = GetAICar(mePlayerGlobalRaceCarIndex);
    const Route* lpRoute = lpPlayerAICar->GetRoute();

    s32 liPlayerNextNodeIndex = lpPlayerAICar->GetNextRouteNodeIndex();
    if (liPlayerNextNodeIndex > 0)
    {
        liPlayerNextNodeIndex = liPlayerNextNodeIndex - 1;
    }
    const s32 liLastNodeIndex = lpRoute->GetNodeCount() - 1;

    // DWARF local lfResetAheadDistance (:1825). Dead on the console; the draw itself is not.
    (void)lpPlayerAICar->GetRandomNumber();

    s32 liResetNodeIndex = liPlayerNextNodeIndex;
    bool lbJunctionFound = false;
    const AISection* lpJunctionSection = 0;
    const AISection* lpEntrySection = 0;

    for (; liResetNodeIndex < liLastNodeIndex; ++liResetNodeIndex)
    {
        const RouteNode* lpNextNode = lpRoute->GetNode(liResetNodeIndex);
        lpJunctionSection = mpAISectionData.operator->()->GetAISection(lpNextNode->muSectionIndex);

        const Vector3 lMiddle   = lpJunctionSection->GetMiddle();
        const Vector3 lPosition = lpPlayerAICar->GetPosition();
        const Vector3 lOffset   = { lPosition.x - lMiddle.x, lPosition.y - lMiddle.y,
                                    lPosition.z - lMiddle.z, 0.0f };

        // `fcmpu ; blt` @0x82785484: only a section NOT closer than 100 m is tested for the flag.
        if (!(Length3D(lOffset) < KF_TOO_CLOSE_TO_JOIN_FROM_SIDE) && lpJunctionSection->IsJunction())
        {
            lbJunctionFound = true;
            break;
        }
        lpEntrySection = lpJunctionSection;
    }

    if (!lbJunctionFound || lpEntrySection == 0)
    {
        return false;
    }

    const Vector2 lJunctionMiddle = BrnMath::Flatten(lpJunctionSection->GetMiddle());

    const s32 liNextSectionNodeIndex = liResetNodeIndex + 1;
    if (liNextSectionNodeIndex >= lpRoute->GetNodeCount())
    {
        return false;
    }
    const s32 liNextSectionIndex = lpRoute->GetNode(liNextSectionNodeIndex)->muSectionIndex;
    const AISection* lpSectionAfterJunction =
        mpAISectionData.operator->()->GetAISection(static_cast<u32>(liNextSectionIndex));
    const Vector2 lSectionAfterJunctionMiddle =
        BrnMath::Flatten(lpSectionAfterJunction->GetMiddle());

    const Vector2 lRouteDelta = { lSectionAfterJunctionMiddle.x - lJunctionMiddle.x,
                                  lSectionAfterJunctionMiddle.y - lJunctionMiddle.y, 0.0f, 0.0f };
    if (IsZero2D(lRouteDelta))
    {
        return false;
    }
    const Vector2 lfRouteDirection = Normalise2D(lRouteDelta);

    // ---- the side road that runs back into the junction -----------------------------------------
    f32 lfBestAheadness = KF_ZERO;
    const AISection* lpSideRouteSection = 0;
    Vector3 lEntranceNodePosition = { 0.0f, 0.0f, 0.0f, 0.0f };   // vmr128 v120, v124 (zero)

    for (s32 liPortal = 0; liPortal < lpJunctionSection->mu8NumPortals; ++liPortal)
    {
        const Portal* lpPortal = lpJunctionSection->GetPortal(static_cast<u8>(liPortal));
        const AISection* lpCandidate =
            mpAISectionData.operator->()->GetAISection(lpPortal->GetLinkSectionIndex());

        if (lpCandidate == lpEntrySection || lpCandidate->IsShortcut())
        {
            continue;
        }

        const Vector2 lCandidateMiddle = BrnMath::Flatten(lpCandidate->GetMiddle());
        const Vector2 lToJunction = { lJunctionMiddle.x - lCandidateMiddle.x,
                                      lJunctionMiddle.y - lCandidateMiddle.y, 0.0f, 0.0f };
        if (IsZero2D(lToJunction))
        {
            continue;
        }

        const f32 lfAheadness = Dot2D(Normalise2D(lToJunction), lfRouteDirection);
        if (lfAheadness < KF_ZERO)                          // blt 0x827857EC: a NaN falls through
        {
            continue;
        }
        const f32 lfAbsAheadness = fabsf(lfAheadness);
        if (!(lfAbsAheadness > lfBestAheadness))            // ble 0x827857F8: a NaN is skipped
        {
            continue;
        }

        lfBestAheadness = lfAbsAheadness;
        lpSideRouteSection = lpCandidate;
        lEntranceNodePosition = lpJunctionSection->GetPortal(static_cast<u8>(liPortal))->GetPosition();
    }

    if (lpSideRouteSection == 0)
    {
        return false;
    }

    // ---- walk out along it until the join point leaves the player's 20-degree cone ------------
    const Vector2 lPlayerDirection = BrnMath::Flatten(lpPlayerAICar->GetDirection());
    Vector3 lExitNodePosition = { 0.0f, 0.0f, 0.0f, 0.0f };        // vmr128 v122, v124 (zero)
    const AISection* lpNextSideRouteSection = 0;

    for (s32 liSideScan = 0; ; )
    {
        if (lpSideRouteSection->mu8NumPortals == 0)
        {
            return false;
        }

        f32 lfFurthest = KF_ZERO;
        lpNextSideRouteSection = 0;

        for (s32 liPortal = 0; liPortal < lpSideRouteSection->mu8NumPortals; ++liPortal)
        {
            const Portal* lpPortal = lpSideRouteSection->GetPortal(static_cast<u8>(liPortal));
            const AISection* lpCandidate =
                mpAISectionData.operator->()->GetAISection(lpPortal->GetLinkSectionIndex());

            if (lpCandidate == lpJunctionSection)
            {
                continue;
            }

            const Vector2 lCandidateMiddle = BrnMath::Flatten(lpCandidate->GetMiddle());
            const Vector2 lFromJunction = { lCandidateMiddle.x - lJunctionMiddle.x,
                                            lCandidateMiddle.y - lJunctionMiddle.y, 0.0f, 0.0f };
            const f32 lfDistance = Length2D(lFromJunction);
            if (!(lfDistance > lfFurthest))                 // ble 0x8278596C: a NaN is skipped
            {
                continue;
            }

            lfFurthest = lfDistance;
            lpNextSideRouteSection = lpCandidate;
            lExitNodePosition =
                lpSideRouteSection->GetPortal(static_cast<u8>(liPortal))->GetPosition();
        }

        if (lpNextSideRouteSection == 0)
        {
            return false;
        }
        lpSideRouteSection = lpNextSideRouteSection;

        const Vector3 lPlayerPosition3D = lpPlayerAICar->GetPosition();
        const Vector2 lExitFromPlayer = BrnMath::Flatten(Vector3{
            lExitNodePosition.x - lPlayerPosition3D.x, lExitNodePosition.y - lPlayerPosition3D.y,
            lExitNodePosition.z - lPlayerPosition3D.z, lExitNodePosition.w - lPlayerPosition3D.w });

        if (!IsZero2D(lExitFromPlayer))
        {
            const f32 lfBearing = fabsf(Dot2D(Normalise2D(lExitFromPlayer), lPlayerDirection));
            if (lfBearing < KF_ANGLE_TO_JOIN_FROM_SIDE_ROAD)
            {
                break;
            }
            lEntranceNodePosition = lExitNodePosition;
        }

        ++liSideScan;
        if (liSideScan >= KI_SIDE_SCAN_BAIL_OUT)
        {
            break;
        }
    }

    // ---- the join point, and the two "would the player see it pop in" tests -------------------
    lpResetData->mPosition = InterpolatePositionFromAngle(
        BrnMath::Flatten(lpPlayerAICar->GetPosition()), lPlayerDirection, lEntranceNodePosition,
        lExitNodePosition, KF_ANGLE_TO_JOIN_FROM_SIDE_ROAD);

    const Vector3 lPlayerPosition = lpPlayerAICar->GetPosition();
    Vector3 lRelativePosition = { lpResetData->mPosition.x - lPlayerPosition.x,
                                  lpResetData->mPosition.y - lPlayerPosition.y,
                                  lpResetData->mPosition.z - lPlayerPosition.z,
                                  lpResetData->mPosition.w - lPlayerPosition.w };

    if (KF_TOO_CLOSE_DISTANCE > Length3D(lRelativePosition))
    {
        return false;
    }

    const f32 lfSeparation = Length3D(lRelativePosition);
    if (!IsZero3D(lRelativePosition))
    {
        const f32 lfInverse = 1.0f / sqrtf(Dot3D(lRelativePosition, lRelativePosition));
        lRelativePosition = Vector3{ lRelativePosition.x * lfInverse, lRelativePosition.y * lfInverse,
                                     lRelativePosition.z * lfInverse, lRelativePosition.w * lfInverse };
    }
    const f32 lfAheadness = Dot3D(lRelativePosition, lpPlayerAICar->GetDirection());

    // bge 0x82785CE4 / 0x82785CF4 skip this block on >= AND on unordered (FX-NANPOL 2026-09-24:
    // the `!(a >= K)` spelling prevented the pop-up for a NaN aheadness).
    if (lfAheadness < KF_ASSUMED_FOV_FOR_RESET_AHEAD &&
        lfSeparation < KF_TOO_CLOSE_TO_SPAWN_IN_VIEW)
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint << "Pop up prevented\n";
        }
        return false;
    }

    const Vector3 lDirection3D = { lEntranceNodePosition.x - lExitNodePosition.x,
                                   lEntranceNodePosition.y - lExitNodePosition.y,
                                   lEntranceNodePosition.z - lExitNodePosition.z,
                                   lEntranceNodePosition.w - lExitNodePosition.w };
    lpResetData->mDirection = lDirection3D;   // 0x82785D40: stored BEFORE the zero test
    if (IsZero3D(lDirection3D))
    {
        return false;
    }

    lpResetData->mpAISection = lpNextSideRouteSection;
    const f32 lfInverse = 1.0f / sqrtf(Dot3D(lDirection3D, lDirection3D));
    lpResetData->mDirection = Vector3{ lDirection3D.x * lfInverse, lDirection3D.y * lfInverse,
                                       lDirection3D.z * lfInverse, lDirection3D.w * lfInverse };
    return true;
}

// =================================================================================================
// ResetAheadFromSideTurnings @0x827909F0 (DWARF :236) -- reset type 5, and reset type 3 when the
// player is looking backwards (ComputeResetOnTrack 0x82797E34 / 0x82797E78).
//
//   0x82790A14  section = best, else default; still 0x7FFF -> false
//   0x82790A48  ScanForwardsAndAlongJunction(lpResetData) -> true, as is (no direction fix-up)
//   0x82790A58  lfResetDistance = RandomFloat(0x8300D5D0) * 40.0 + 160.0   (fmadds, 0x82790AE8)
//   0x82790AF0  ScanForwardsAlongExtrapolatedRoute(prev, next, lfResetDistance); on failure:
//   0x82790B0C      PlayerIsLookingBackwards() -> false
//   0x82790B1C      lfResetDistance = -25.0 (flt_820C4860) -- ALSO the Convert distance below
//   0x82790B34      ScanBackwardsAlongExtrapolatedRoute(prev, next, -25.0, `li r7,1` RoadRage);
//                   failure -> ResetAwayFromPlayer(lpResetData)                     (0x82790B4C)
//   0x82790B78  ConvertNodesToPositionAndDirection(prev, next, 0.5, lfResetDistance, out);
//               failure -> ResetAwayFromPlayer(lpResetData)                         (0x82790B90)
//   0x82790BA8  EnsureAIIsDrivingSameDirectionAsPlayer(player, out)
//   0x82790BC4  out->mpAISection = GetAISection(next->muSectionIndex) ; return true
//
// ⛔ CORRECTED 2026-09-22 (crash parity G07-D2/G07-D4): this was a parked stub answering false,
// so every type-5 wrap (Road Rage: half of the out-of-range wraps; Marked Man: one in five)
// resolved to FAILURE and the rival was put back at its own last pose, still out of range.
// The draw comes from the stream AICar::Construct/Reset re-seed (AICar::GetRandomNumber,
// FX-AIDRV 1a611271), which ScanForwardsAndAlongJunction has already stepped once.
// =================================================================================================
bool ResetOnTrackManager::ResetAheadFromSideTurnings(ResetOnTrackCoords* lpResetData)
{
    const AICar* lpPlayerAICar = GetAICar(mePlayerGlobalRaceCarIndex);

    u16 luSection = lpPlayerAICar->muBestSectionIndex;
    if (luSection == KU_INVALID_SECTION_INDEX)
    {
        luSection = lpPlayerAICar->muDefaultSectionIndex;
    }
    if (luSection == KU_INVALID_SECTION_INDEX)
    {
        return false;
    }

    if (ScanForwardsAndAlongJunction(lpResetData))
    {
        return true;
    }

    f32 lfResetDistance = lpPlayerAICar->GetRandomNumber() * KF_RESET_AHEAD_VARIABILITY +
                          KF_NEAR_PLAYER_RESET_AHEAD_DISTANCE;

    const RouteNode* lpPrevNode = 0;
    const RouteNode* lpNextNode = 0;

    if (!ScanForwardsAlongExtrapolatedRoute(lpPrevNode, lpNextNode, lfResetDistance))
    {
        if (PlayerIsLookingBackwards())
        {
            return false;
        }

        lfResetDistance = KF_MIN_RESET_DISTANCE_BEHIND;
        if (!ScanBackwardsAlongExtrapolatedRoute(lpPrevNode, lpNextNode, lfResetDistance,
                                                 eExtrapolateType_RoadRage))
        {
            return ResetAwayFromPlayer(lpResetData);
        }
    }

    if (!ConvertNodesToPositionAndDirection(lpPrevNode, lpNextNode, KF_ROAD_CENTRE, lfResetDistance,
                                            lpResetData))
    {
        return ResetAwayFromPlayer(lpResetData);
    }

    EnsureAIIsDrivingSameDirectionAsPlayer(lpPlayerAICar, lpResetData);

    lpResetData->mpAISection =
        mpAISectionData.operator->()->GetAISection(lpNextNode->muSectionIndex);
    return true;
}

// =================================================================================================
// PlayerIsLookingBackwards @0x82778000
//
//   0x82778020  lpPlayerAICar = GetAICar(mePlayerGlobalRaceCarIndex)
//   0x82778030  lvx128 v127, this, 0x3B0     -- mCamera (+0x390) + 0x20: mTransform's At row, the
//                                                camera's view direction (Camera::GetDirection)
//   0x82778034  AICar::GetDirection ; 0x82778040 vmsum3fp128 (3-lane dot)
//   0x82778058  fcmpu vs 0.0 (flt_82001CC0) ; blt -> true
//
// ⛔ CORRECTED 2026-09-22 (crash parity G07-D3): this was parked to `false` because mCamera was an
// unwritten u8[0x164]. ResetOnTrackManager::Update now copies the camera AIModule hands it
// (G08-D2 / G05-D3), and ResetAheadFromSideTurnings -- the arm a `true` routes reset type 3 into --
// is real (G07-D2), so a look-back reset lands ahead of the player instead of in view behind.
// =================================================================================================
bool ResetOnTrackManager::PlayerIsLookingBackwards()
{
    const AICar* lpPlayerAICar = GetAICar(mePlayerGlobalRaceCarIndex);
    const Vector3 lCameraDirection = mCamera.GetDirection();

    return Dot3D(lpPlayerAICar->GetDirection(), lCameraDirection) < KF_ZERO;
}

}   // namespace BrnAI
