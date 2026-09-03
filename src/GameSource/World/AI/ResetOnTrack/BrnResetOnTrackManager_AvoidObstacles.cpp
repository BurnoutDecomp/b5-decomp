// =================================================================================================
// BrnResetOnTrackManager_AvoidObstacles.cpp -- the post-placement obstacle sweep and its three
// tests (aiwave A11, 2026-09-03).
//
//   BrnAI::ResetOnTrackManager::AvoidObstacles    @0x827941E0   (DWARF :227)
//   BrnAI::ResetOnTrackManager::TestSectionHNG    @0x82785F90   (DWARF :344)
//   BrnAI::ResetOnTrackManager::TestRecentResets  @0x82778130   (DWARF :350)
//   BrnAI::ResetOnTrackManager::TestCarHNG        @0x82790BD8   (DWARF :341)  [PARKED]
//
// ⭐ AvoidObstacles IS NOT OPTIONAL POLISH -- IT IS ON THE RETURN PATH OF SIX OF THE SEVEN RESET
// TYPES. ComputeResetOnTrack @0x82797F70..0x82797F94 ends with
//     `if (!found) return false; return (type == 5) ? true : AvoidObstacles(request, out);`
// so whatever this function returns IS ComputeResetOnTrack's answer for reset types 1,2,3,4,6,7.
// It also carries a GATE that has nothing to do with obstacles: @0x82794300..0x82794324 it returns
// FALSE outright when the requesting car's AICar is neither IN_RANGE nor OUT_OF_RANGE, i.e. when
// the AI is not modelling that car at all. Reproducing the sweep but dropping that gate would let
// a pose through for a car the AI has never seen; dropping the sweep but keeping the gate is the
// honest partial, and that is what this file is while TestCarHNG stays parked.
//
// ⚠️ THE OPERAND-ORDER TRAP. IDA prints the PowerPC A-form multiply-add family in ENCODING order
// (vD, vA, vB, vC), not mnemonic order (vD, vA, vC, vB). The lateral vector this whole function is
// built around comes out of 0x8279428C..0x827942C0:
//     v10 = dir.yzx                          (vpermwi128 ..., 0x63)
//     v11 = {0,1,0,0}                        (unk_82181510, the world Y axis)
//     v8  = v11.yzx = {1,0,0,0}
//     v11 = v11 * v10                        (== up * dir.yzx)
//     vnmsubfp v11, v8, v11, v7              (== vB - vA*vC == v11 - v8*v7 in ENCODING order)
//     v124 = v11.yzx                         (vpermwi128 ..., 0x63)
// which is the standard single-shuffle cross product `(a.yzx*b - a*b.yzx).yzx` with a == the reset
// direction and b == world up -- i.e. the ROAD'S RIGHT VECTOR, (-dir.z, 0, dir.x). Read the operands
// in mnemonic order instead and the same block "computes" the direction itself, so the sweep would
// nudge the car FORWARD along the road instead of sideways off the obstacle. The value below is
// written as the cross product it is, not transcribed.
// =================================================================================================

#include "GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.h"

#include "GameSource/World/AI/BrnAICar.h"                    // AICar (state + driver)
#include "GameSource/World/AI/BrnAIPortal.h"                 // BrnAI::Portal (GetLinkSectionIndex)
#include "GameSource/World/AI/BrnHNGTest.h"                  // BrnAI::LineTestSectionHNG
#include "GameSource/World/AI/BrnAISharedConstants.h"        // EAICarState / EResetType
#include "GameSource/Math/BrnMathUtils.h"                    // BrnMath::Flatten (XZ)
#include "SharedClasses/AI/AISectionsResourceType.h"         // AISection / AISectionsData
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <math.h>   // sqrtf

namespace BrnAI
{
namespace
{
    // flt_82093AB4, read at TestRecentResets 0x8277817C. Compared against a MagnitudeSquared, so
    // it is a squared distance: 36 == (6 m)^2.
    const f32 KF_RECENT_RESET_SEPARATION_SQUARED = 36.0f;

    // AvoidObstacles' sweep budget and step. 0x82794504 `cmpwi r26, 3` (DWARF local
    // KI_SWEEP_MAX_ATTEMP_COUNT) and flt_820C41F4 == 2.0 (KF_SWEEP_DISTANCE).
    const s32 KI_SWEEP_MAX_ATTEMP_COUNT = 3;
    const f32 KF_SWEEP_DISTANCE         = 2.0f;

    // The "sweep found nothing" fallback @0x82794518..0x827945C4: the car is shoved sideways by
    // ((miResetCount % 8) - 4) * 3.0 * 2.0 metres -- a deterministic spread that walks across the
    // road as consecutive resets pile up. flt_820C4154 == 3.0, flt_820C41C0 == 4.0,
    // flt_820C41F4 == 2.0, and the modulus is the `srawi/addze/slwi/subf` signed %8 idiom.
    const f32 KF_FALLBACK_SPREAD_SCALE = 3.0f;
    const f32 KF_FALLBACK_SPREAD_BIAS  = 4.0f;
    const s32 KI_FALLBACK_SPREAD_WRAP  = 8;

    // The two sides the sweep tries, in the console's own order: index 0 is the NEGATED lateral
    // (var_E0, stored first at 0x827943C8) and index 1 the positive one (var_D0, 0x827943C0);
    // the inner loop walks r29 upwards from var_E0.
    const s32 KI_SWEEP_SIDE_COUNT = 2;
}

// =================================================================================================
// TestRecentResets @0x82778130 (DWARF :350)
//
//   0x82778158  if (mRecentResets.GetLength() <= 0) return false
//   0x82778180  for (luIndex = 0; luIndex < GetLength(); ++luIndex)
//                 assert(luIndex < miLength)   ("Attempt to access element outside range",
//                                               CgsRingBuffer.h:258 -- the inlined operator[])
//                 if (36.0 > MagnitudeSquared(lTestPosition - mRecentResets[luIndex].mPosition))
//                     return true
//   0x8277821C  return false
//
// The console's element address is `((miReadPos + luIndex) % miMaxLength) * 32 + mpData`, which is
// exactly FixedRingBuffer::operator[]; reached by name here.
// =================================================================================================
bool ResetOnTrackManager::TestRecentResets(Vector3 lTestPosition)
{
    const s32 liLength = mRecentResets.GetLength();
    if (liLength <= 0)
    {
        return false;
    }

    for (s32 liIndex = 0; liIndex < mRecentResets.GetLength(); ++liIndex)
    {
        const RecentResetEntry& lrEntry = mRecentResets[static_cast<u32>(liIndex)];

        const f32 lfDX = lTestPosition.x - lrEntry.mPosition.x;
        const f32 lfDY = lTestPosition.y - lrEntry.mPosition.y;
        const f32 lfDZ = lTestPosition.z - lrEntry.mPosition.z;

        const f32 lfSeparationSquared = lfDX * lfDX + lfDY * lfDY + lfDZ * lfDZ;

        if (KF_RECENT_RESET_SEPARATION_SQUARED > lfSeparationSquared)
        {
            return true;
        }
    }

    return false;
}

// =================================================================================================
// TestSectionHNG @0x82785F90 (DWARF :344)
//
//   0x82785FC4  StartMonitor(muTestLineHNGPM)                                    [PARKED]
//   0x82785FC8  assert(lpAISection != NULL)                                      (:2254)
//   0x82785FFC  lbHit = LineTestSectionHNG(lpAISection, lStartPos, lEndPos)
//   0x82786024  for (i = 0; i < lpAISection->mu8NumPortals; ++i)  {
//                 if (lbHit) break;                                              (the loop-top test)
//                 luLink = lpAISection->GetPortal(i)->GetLinkSectionIndex()       (lhz 0x10)
//                 assert(luLink < muNumSections)                                 (AISectionsData.h:1201)
//                 lpLinked = GetAISection(luLink)
//                 if (lpLinked->PassesThrough(lStartPos, lEndPos))
//                     lbHit = LineTestSectionHNG(lpLinked, lStartPos, lEndPos)    ⭐ ASSIGN, not OR
//               }
//   0x827860CC  StopMonitor / PushHngTestInfo                                    [PARKED]
//
// ⭐ THE INNER RESULT IS ASSIGNED, NOT OR'd (`mr r26, r3` @0x827860B4), so a linked section that
// the query segment crosses but that has no no-go line CLEARS a hit the owning section had already
// reported. That reads like a bug and it is the shipped behaviour; it is only reachable when the
// loop-top `if (lbHit) break` did not fire, i.e. never after a hit -- so the two agree. Reproduced
// as written rather than "corrected" to an OR.
//
// ⚠️ THE TWO Vector2 TYPES ARE NOT THE SAME TYPE. LineTestSectionHNG takes the 16-byte SIMD
// `Vector2` (BrnCommonTypes.h); AISection::PassesThrough takes the PACKED 8-byte
// `AISection::Vector2` (the section's own corner type -- see its banner). The conversion below is
// explicit for that reason; passing one where the other belongs re-strides the corner walk.
//
// [FLAG PC boot gate] the PerfMonCpu Start/StopMonitor pair (muTestLineHNGPM) and
// ResetOnTrackDebugComponent::PushHngTestInfo -- both parked exactly as ResetOnTrackManager::
// Construct's own banner parks the monitor registrations and the debug component.
// =================================================================================================
bool ResetOnTrackManager::TestSectionHNG(const AISection* lpAISection,
                                          Vector2 lStartPos, Vector2 lEndPos)
{
    CGS_ASSERT(lpAISection != 0, "lpAISection != NULL");   // :2254
    if (lpAISection == 0 || !mpAISectionData.HasMemoryResource())
    {
        return false;   // [GUARD] the console dereferences both unconditionally
    }

    bool lbHitHNG = LineTestSectionHNG(lpAISection, lStartPos, lEndPos);

    const AISectionsData* lpAISectionsData = mpAISectionData.operator->();

    const AISection::Vector2 lPackedStart = { lStartPos.x, lStartPos.y };
    const AISection::Vector2 lPackedEnd   = { lEndPos.x,   lEndPos.y   };

    for (s32 liPortal = 0; liPortal < static_cast<s32>(lpAISection->mu8NumPortals); ++liPortal)
    {
        if (lbHitHNG)
        {
            break;
        }

        const u16 luLinkSectionIndex =
            lpAISection->GetPortal(static_cast<u8>(liPortal))->GetLinkSectionIndex();

        const AISection* lpLinkedSection = lpAISectionsData->GetAISection(luLinkSectionIndex);

        if (lpLinkedSection->PassesThrough(lPackedStart, lPackedEnd))
        {
            lbHitHNG = LineTestSectionHNG(lpLinkedSection, lStartPos, lEndPos);
        }
    }

    // [FLAG PC boot gate] StopMonitor + ResetOnTrackDebugComponent::PushHngTestInfo -- see banner.
    return lbHitHNG;
}

// =================================================================================================
// TestCarHNG @0x82790BD8 (DWARF :341)
//
// ⛔ [FLAG PC bring-up] PARKED. It builds a five-point car footprint (the pose plus
// KF_CAR_LENGTH/KF_CAR_WIDTH-scaled corners: 2.5 and 4.5 half-extents plus a
// `lfSpeed + 6.0` look-ahead, @0x82790C74..0x82790D40) and tests it in four calls:
// TestSectionHNG twice (both reconstructed above) and **LineTestTrafficHNG @0x8277A878 twice**
// (0x82790DB8 / 0x82790DD4). That traffic-side overload is NOT in this tree -- BrnHNGTest.h's own
// banner records it as a separate, unreconstructed TU -- and it needs the AI driver's
// BrnAI::NearbyVehicles set, which has no home either.
//
// ⭐ FALSE IS THE CONSOLE'S "NOTHING IN THE WAY" ANSWER, NOT AN ERROR CODE, and it is the value
// that keeps AvoidObstacles' shape honest: with it, the sweep's first two questions resolve to
// "no car in the way", the pose is accepted unchanged, and the AICar-state gate above still runs.
// The cost is that a reset car can be placed overlapping traffic or another racer -- which is
// exactly what would be reported if it happens, rather than silently mis-attributed to the
// placement geometry.
// DELETE-WHEN LineTestTrafficHNG and BrnAI::NearbyVehicles land.
// =================================================================================================
bool ResetOnTrackManager::TestCarHNG(const AISection* lpAISection,
                                     const NearbyVehicles* lpaNearbyVehicles,
                                     Vector2 lPosition, Vector2 lDirection, f32 lfSpeed)
{
    CGS_ASSERT(lpAISection != 0, "lpAISection != NULL");   // :2215
    (void)lpAISection;
    (void)lpaNearbyVehicles;
    (void)lPosition;
    (void)lDirection;
    (void)lfSpeed;

    static bool sbReportedParked = false;
    if (!sbReportedParked)
    {
        sbReportedParked = true;
        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[rot] PARKED: ResetOnTrackManager::TestCarHNG (X360 0x82790BD8) needs "
                   "LineTestTrafficHNG (0x8277A878) and BrnAI::NearbyVehicles, neither of which "
                   "is reconstructed -- answering 'no car in the way', so a reset pose is never "
                   "swept aside for traffic.\n";
        }
    }
    return false;
}

// =================================================================================================
// AvoidObstacles @0x827941E0 (DWARF :227)
//
//   0x82794208  assert(lpRequest != NULL)                                        (:549)
//   0x82794238  lpAICar = GetAICar(lpRequest->GetGlobalRaceCarIndex())
//   0x8279427C  lPosition2D  = Flatten(lpResetData->mPosition)                    (v126)
//   0x827942FC  lDirection2D = Normalise(Flatten(lpResetData->mDirection))        (v125)
//   0x827942C0  lLateral     = Cross(lpResetData->mDirection, worldUp)            (v124) -- see banner
//   0x827942A8  lbIsNotRaceStart = (lpRequest->GetResetType() != 6)               (r19)
//   0x82794318  if (!lpAICar->IsActive()) return false                            ⭐ THE GATE
//   0x82794330  laNearbyVehicles = (carState == IN_RANGE) ? lpAICar->mpDriver : 0
//   0x82794334  ...and it is FORCED to 0 for reset type 5
//   0x82794368  if (TestCarHNG(section, nearby, pos2D, dir2D, resetSpeed))   goto SWEEP
//   0x82794380  if (!TestRecentResets(lpResetData->mPosition))               return true
//   0x82794394  if (!lbIsNotRaceStart)                                       return true
//   SWEEP (0x8279439C):
//     lafSideBlocked[2] = {0,0}; lateral[0] = -lLateral; lateral[1] = +lLateral
//     for (liAttempt = 1; liAttempt <= 3; ++liAttempt)
//       for (liSide = 0; liSide < 2; ++liSide)
//         if (lafSideBlocked[liSide] != 0.0) continue
//         lOffset = lateral[liSide] * (f32)liAttempt * 2.0
//         if (TestSectionHNG(section, pos2D, pos2D + Flatten(lOffset)))
//              lafSideBlocked[liSide] = 1.0 ; continue          -- that side is walled off
//         if (lbIsNotRaceStart && TestRecentResets(mPosition + lOffset)) continue
//         if (!TestCarHNG(section, nearby, pos2D + Flatten(lOffset), dir2D, resetSpeed))
//              mPosition += lOffset ; return true
//   0x8279450C  swept out: reset type 1 accepts the pose unchanged; every other type takes the
//               deterministic ((miResetCount % 8) - 4) * 3.0 * 2.0 lateral shove and accepts.
//
// ⭐ THE SIDE-BLOCKED LATCH IS A FLOAT ARRAY, NOT A BOOL ARRAY, and it is compared against 0.0
// (`lfs f0, 0(r24) ; fcmpu f0, f31`), which is why it is written as 1.0f rather than `true`.
// Once a side reports a hard-no-go line, no LONGER offset on that side is tried -- the sweep does
// not walk through a wall to find clear road on the far side.
//
// ⚠️ THE THREE ARGUMENTS THAT ARE NOT THE OBVIOUS ONES: the AICar is the REQUESTING car's (so the
// gate and the nearby-vehicle set belong to the car being placed), while every geometry input comes
// from lpResetData (the pose just computed) and lfSpeed from the REQUEST, not from the car.
// =================================================================================================
bool ResetOnTrackManager::AvoidObstacles(const AIModuleIO::ResetOnTrackRequest* lpRequest,
                                          ResetOnTrackCoords* lpResetData)
{
    CGS_ASSERT(lpRequest != 0, "lpRequest != NULL");   // :549
    if (lpRequest == 0 || lpResetData == 0)
    {
        return false;
    }

    const AICar* lpAICar = GetAICar(lpRequest->GetGlobalRaceCarIndex());

    const Vector2 lPosition2D = BrnMath::Flatten(lpResetData->mPosition);

    Vector2 lDirection2D = BrnMath::Flatten(lpResetData->mDirection);
    {
        const f32 lfLengthSquared = lDirection2D.x * lDirection2D.x + lDirection2D.y * lDirection2D.y;
        if (lfLengthSquared != 0.0f)
        {
            const f32 lfInverse = 1.0f / sqrtf(lfLengthSquared);
            lDirection2D.x *= lfInverse;
            lDirection2D.y *= lfInverse;
        }
    }

    // Cross(mDirection, worldUp) == the road's right vector -- see the file banner for why this is
    // written as a cross product rather than transcribed from the shuffle pair.
    const Vector3 lResetDirection = lpResetData->mDirection;
    const Vector3 lLateral = { -lResetDirection.z, 0.0f, lResetDirection.x, 0.0f };

    const bool lbIsNotRaceStart =
        (lpRequest->GetResetType() != E_RESET_TYPE_BEHIND_PLAYER_RACE_START);

    if (!lpAICar->IsActive())
    {
        // The console's own gate: an AICar the AI is not modelling gets no pose at all.
        static bool sbReportedInactive = false;
        if (!sbReportedInactive && CgsDev::Log::gpDebugPrint != 0)
        {
            sbReportedInactive = true;
            *CgsDev::Log::gpDebugPrint
                << "[rot] AvoidObstacles refused: AICar for global car "
                << static_cast<s32>(lpRequest->GetGlobalRaceCarIndex())
                << " is not IN_RANGE/OUT_OF_RANGE (state "
                << static_cast<s32>(lpAICar->meCarState)
                << ") -- this is the console's own gate at X360 0x82794318, not a park.\n";
        }
        return false;
    }

    // [FLAG PC bring-up] the console reads the AI driver slot at AICar+0x14B0 and hands it to
    // TestCarHNG as `const NearbyVehicles*`; on the host that word is the guest mpDriver whose
    // 8-byte replacement is mpDriverHost (BrnAICar.h's own note). TestCarHNG is parked below, so
    // this value is carried, not dereferenced. DELETE-WHEN BrnAI::NearbyVehicles lands and the
    // driver's nearby-vehicle set can be named.
    const NearbyVehicles* lpaNearbyVehicles = 0;
    if (lpAICar->meCarState == E_AI_CAR_STATE_IN_RANGE)
    {
        lpaNearbyVehicles = reinterpret_cast<const NearbyVehicles*>(lpAICar->GetDriver());
    }
    if (lpRequest->GetResetType() == E_RESET_TYPE_FROM_TURNINGS_ROAD_RAGE)
    {
        lpaNearbyVehicles = 0;   // 0x8279433C
    }

    const AISection* lpAISection = static_cast<const AISection*>(lpResetData->mpAISection);

    bool lbNeedsSweep = false;

    if (TestCarHNG(lpAISection, lpaNearbyVehicles, lPosition2D, lDirection2D,
                   lpRequest->GetResetSpeed()))
    {
        lbNeedsSweep = true;
    }
    else if (TestRecentResets(lpResetData->mPosition) && lbIsNotRaceStart)
    {
        lbNeedsSweep = true;
    }

    if (!lbNeedsSweep)
    {
        return true;
    }

    // ---- the sweep ---------------------------------------------------------------------------
    f32 lafSideBlocked[KI_SWEEP_SIDE_COUNT];
    lafSideBlocked[0] = 0.0f;
    lafSideBlocked[1] = 0.0f;

    Vector3 laLateral[KI_SWEEP_SIDE_COUNT];
    laLateral[0] = Vector3{ -lLateral.x, -lLateral.y, -lLateral.z, 0.0f };   // var_E0 (the xor)
    laLateral[1] = lLateral;                                                 // var_D0

    for (s32 liAttempt = 1; liAttempt <= KI_SWEEP_MAX_ATTEMP_COUNT; ++liAttempt)
    {
        for (s32 liSide = 0; liSide < KI_SWEEP_SIDE_COUNT; ++liSide)
        {
            if (lafSideBlocked[liSide] != 0.0f)
            {
                continue;
            }

            const f32 lfStep = static_cast<f32>(liAttempt) * KF_SWEEP_DISTANCE;
            const Vector3 lOffset = { laLateral[liSide].x * lfStep,
                                      laLateral[liSide].y * lfStep,
                                      laLateral[liSide].z * lfStep, 0.0f };

            const Vector2 lOffset2D  = BrnMath::Flatten(lOffset);
            const Vector2 lCandidate2D = { lPosition2D.x + lOffset2D.x,
                                           lPosition2D.y + lOffset2D.y, 0.0f, 0.0f };

            if (TestSectionHNG(lpAISection, lPosition2D, lCandidate2D))
            {
                lafSideBlocked[liSide] = 1.0f;
                continue;
            }

            if (lbIsNotRaceStart)
            {
                const Vector3 lCandidate = { lpResetData->mPosition.x + lOffset.x,
                                             lpResetData->mPosition.y + lOffset.y,
                                             lpResetData->mPosition.z + lOffset.z, 0.0f };
                if (TestRecentResets(lCandidate))
                {
                    continue;
                }
            }

            if (!TestCarHNG(lpAISection, lpaNearbyVehicles, lCandidate2D, lDirection2D,
                            lpRequest->GetResetSpeed()))
            {
                lpResetData->mPosition = Vector3{ lpResetData->mPosition.x + lOffset.x,
                                                  lpResetData->mPosition.y + lOffset.y,
                                                  lpResetData->mPosition.z + lOffset.z, 0.0f };
                return true;
            }
        }
    }

    if (lpRequest->GetResetType() == E_RESET_TYPE_STANDARD)
    {
        return true;   // 0x82794514 -- a crash recovery keeps the pose it computed
    }

    {
        const f32 lfSpread =
            (static_cast<f32>(miResetCount % KI_FALLBACK_SPREAD_WRAP) - KF_FALLBACK_SPREAD_BIAS)
            * KF_FALLBACK_SPREAD_SCALE * KF_SWEEP_DISTANCE;

        lpResetData->mPosition = Vector3{ lLateral.x * lfSpread + lpResetData->mPosition.x,
                                          lLateral.y * lfSpread + lpResetData->mPosition.y,
                                          lLateral.z * lfSpread + lpResetData->mPosition.z, 0.0f };
    }

    return true;
}

}   // namespace BrnAI
