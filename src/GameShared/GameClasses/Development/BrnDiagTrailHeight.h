#ifndef BRN_DIAG_TRAIL_HEIGHT_H
#define BRN_DIAG_TRAIL_HEIGHT_H

#include "types.hpp"

// ============================================================================================
// [DIAG] NOT IN THE X360 BINARY. Inert unless BRN_TRAIL_HEIGHT_DIAG names a non-zero value.
// DELETE-WHEN-STABLE.
//
// WHY THIS EXISTS -- issue #21 says the tyre marks "float above the ground". That is a claim
// about ONE number: the height of a laid mark above the road surface under it, in metres.
// Nothing in the tree could produce that number, because the two halves live in different
// subsystems and neither can see the other:
//
//   * the ROAD is the traction line test's hit point. It is read once per physics step in
//     BrnPhysics::Vehicle::VehicleManager::ReadRaceCarTractionLineTestResults, handed straight
//     to AddTractionPoint, and never published in that raw form again -- by the time the
//     effects module sees it, it has been re-expressed in the car's body frame
//     (VehiclePhysics::StoreLocalWheelPositions) and rigidly carried
//     (VehicleOutputInterface::UpdateRaceCarState @0x825EC808:
//      WheelLite.mRoadContact.mPosition = mTransform * GetLocalTractionPoint(i)), and road
//     noise (VehiclePhysics::UpdateRoadNoise) has displaced it along the contact normal;
//   * the MARK is laid from that carried point by BrnEffects::EffectsModule::HandleWheels
//     @0x82296C80, lifted by kTrailHeightAdjustment.
//
// So "mark y minus road y" is unobservable without carrying the raw hit across. This latch does
// exactly that and nothing else: the line-test reader stamps the hit it just consumed, and
// HandleWheels prints the difference beside the mark it just laid. It is a MEASURING STICK --
// no game-visible value is read from it, ever.
//
// ⚠ THE INDEX SPACES ARE NOT THE SAME, AND THAT LIED ONCE (run i21c_B/i21c_C). The writer
// iterates the VehicleManager's `mUsedRaceCars` SLOTS; the reader only has
// RaceCarState::miRaceCarID, which is an ATTRIBUTE field
// (`mpAttribs->mBaseAttribs.miRaceCarID`) copied in by UpdateRaceCarState @0x825EC808. A free
// burn carries FOUR live race-car slots, so keying either side on the other's index silently
// differenced the player's mark against another car's road hit -- dy read 3.9 m, then 25 m, and
// neither number was about tyre marks at all. [[diagnostics-that-lie]]
// So: the array is indexed BY SLOT, every entry also records the id of the car that wrote it,
// and the reader SEARCHES for the entry whose id matches and prints the slot it chose plus the
// stamp age. A line that found no matching entry says `slot=-1` and must be discarded, and a
// `dxz` far larger than one frame's travel says the match was wrong even so.
// ============================================================================================

namespace BrnDiag
{
    const s32 KI_TRAIL_HEIGHT_MAX_CARS   = 8;
    const s32 KI_TRAIL_HEIGHT_NUM_WHEELS = 4;

    struct TrailHeightHit
    {
        f32 mfX;
        f32 mfY;
        f32 mfZ;
        f32 mfNormalY;
        u32 muStamp;     // 0 == never written
        u32 muHit;       // the line test's own hit flag for this wheel this step
        s32 miCarId;     // the writing car's mpAttribs->mBaseAttribs.miRaceCarID (-1 == none)
    };

    // Defined in GameSource/Effects/Particles/Native/BrnTrailSystem.cpp.
    extern TrailHeightHit gaTrailHeightHits[KI_TRAIL_HEIGHT_MAX_CARS][KI_TRAIL_HEIGHT_NUM_WHEELS];
    extern u32            guTrailHeightStamp;

    // BRN_TRAIL_HEIGHT_DIAG -- the single gate both sides read. Defined beside the array.
    bool TrailHeightDiagEnabled();
}

#endif
