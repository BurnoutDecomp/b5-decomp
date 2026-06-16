#include "GameSource/GameState/ModeManager/GameModes/BrnOfflineGameMode.h"

#include "GameSource/GameState/ModeManager/BrnModeManager.h"
#include "GameSource/GameState/Progression/BrnProgressionManager.h"
#include "GameSource/GameState/BrnGameStateTypes.h"
#include "SharedClasses/Trigger/BrnTriggerData.h"
#include "SharedClasses/Trigger/BrnLandmark.h"
#include "SharedClasses/Maths/BrnVectorMaths.h"   // Vector3 operator-/Dot/Length/GetY (kept out of the generated vendor header)

namespace BrnGameState
{
// X360 0x82321E38, file BrnOfflineGameMode.cpp:27. The minimum world-Y a landmark may
// sit at relative to the search origin before it is rejected as a destination. The leading
// "HACK" matches the original symbol name -- it patches around landmarks authored below the
// drivable surface. -150.0 is the flt_820211E4 constant the X360 height test compares
// against (delta.y > KF_HACK_MIN_LANDMARK_HEIGHT).
const f32 KF_HACK_MIN_LANDMARK_HEIGHT = -150.0f;

// Above this squared distance the forced-debug landmark is accepted immediately; 100^2
// world units. X360: the flt_82005D9C compared against the squared delta in the debug
// "always race to one landmark" branch.
static const f32 KF_DEBUG_FORCE_MIN_DISTANCE_SQUARED = 10000.0f;

// X360 0x8232FE58, BrnOfflineGameMode.cpp:50. Forwards to the GameMode base (which stores
// the ModeManager* and zero-inits the shared mode flags), then clears this mode's own
// construction flag and seeds the debug "always race to a single landmark" overrides
// (disabled, design index 41). The Hex-Rays `int result`/`return result` are register-reuse
// artifacts of a void function and are dropped; the X360 body writes:
//   *(this+172) = 0   -> mbConstructed (GameMode base)              = false
//   *(this+180) = 0   -> mbDebugAlwaysRaceToSingleLocation          = false
//   *(this+184) = 41  -> miDebugDesignIndexOfLandmarkToAlwaysRaceTo = 41
void OfflineGameMode::Construct(ModeManager* lpModeManager)
{
    GameMode::Construct(lpModeManager);
    mbConstructed = false;
    mbDebugAlwaysRaceToSingleLocation = false;
    miDebugDesignIndexOfLandmarkToAlwaysRaceTo = 41;
}

// X360 0x82321E38, BrnOfflineGameMode.cpp:98. Walks every landmark in the supplied
// TriggerData and collects those usable as a race destination from lv3Origin:
//   - the debug override short-circuits: when mbDebugAlwaysRaceToSingleLocation is set and a
//     landmark's design index matches the forced one, if it is far enough away
//     (squared distance > 100^2) it is selected on its own and the scan stops, returning 1;
//   - otherwise, when lbApplyDirectionFilter is set, landmarks lying behind the reference
//     direction lv3Direction (negative dot with the origin->landmark vector) are skipped;
//   - a landmark is accepted when its straight-line distance from the origin lies strictly
//     inside (lfMinDistance, lfMaxDistance) and its height above the origin clears
//     KF_HACK_MIN_LANDMARK_HEIGHT.
// Accepted landmarks have their region index (wrapped as a LandmarkIndex) and AI-section
// index appended to the two parallel output arrays. The de-optimised body restores the
// SIMD reciprocal-sqrt refinement to a plain Vector3 length, the inlined TriggerData::
// GetLandmark accessor (whose own bounds assert lives in its TU) to a call, and the
// magic-multiply ModeManager->ProgressionManager indirection to GetProgressionManager().
u32 OfflineGameMode::SelectRandomDestinations(const BrnTrigger::TriggerData* lpTriggerData,
                                              Vector3 lv3Origin,
                                              Vector3 lv3Direction,
                                              f32 lfMinDistance,
                                              f32 lfMaxDistance,
                                              LandmarkIndex* lpaLandmarkIndicesOut,
                                              u16* lpaAISectionIndicesOut,
                                              bool lbApplyDirectionFilter)
{
    u32 luFoundCount = 0;

    const s32 liLandmarkCount = lpTriggerData->GetLandmarkCount();
    for (s32 liLandmarkIndex = 0; liLandmarkIndex < liLandmarkCount; ++liLandmarkIndex)
    {
        const BrnTrigger::Landmark* lpLandmark = lpTriggerData->GetLandmark(liLandmarkIndex);

        const Vector3 lv3LandmarkPosition = lpLandmark->GetBoxRegion()->GetPosition();
        const Vector3 lv3Delta = lv3LandmarkPosition - lv3Origin;

        // Debug override: force the single landmark whose design index matches, provided it
        // is not right on top of the origin. Selecting it terminates the whole scan.
        if (mbDebugAlwaysRaceToSingleLocation
            && lpLandmark->GetDesignIndex() == miDebugDesignIndexOfLandmarkToAlwaysRaceTo)
        {
            if (Dot(lv3Delta, lv3Delta) > KF_DEBUG_FORCE_MIN_DISTANCE_SQUARED)
            {
                *lpaLandmarkIndicesOut = LandmarkIndex(lpLandmark->GetRegionIndex());
                *lpaAISectionIndicesOut = mpModeManager->GetProgressionManager()
                                              ->FindLandmarkAISectionIndex(lpLandmark->GetId());
                return 1;
            }
        }

        // Reject landmarks lying behind the reference direction.
        if (lbApplyDirectionFilter && Dot(lv3Direction, lv3Delta) < 0.0f)
        {
            continue;
        }

        // Accept when within the distance band and above the minimum height.
        const f32 lfDistance = Length(lv3Delta);
        if (lfDistance > lfMinDistance
            && lfDistance < lfMaxDistance
            && GetY(lv3Delta) > KF_HACK_MIN_LANDMARK_HEIGHT)
        {
            lpaLandmarkIndicesOut[luFoundCount] = LandmarkIndex(lpLandmark->GetRegionIndex());
            lpaAISectionIndicesOut[luFoundCount] = mpModeManager->GetProgressionManager()
                                                       ->FindLandmarkAISectionIndex(lpLandmark->GetId());
            ++luFoundCount;
        }
    }

    return luFoundCount;
}
}
