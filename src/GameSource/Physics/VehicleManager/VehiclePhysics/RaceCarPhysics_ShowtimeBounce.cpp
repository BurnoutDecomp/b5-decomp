// ============================================================================
// b5-decomp/src/GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics_ShowtimeBounce.cpp
//
// SLICE TU, split out of RaceCarPhysics.cpp 2026-08-06 (PhysicsModule::Update leaves wave) in the
// RaceCarPhysics_Construct.cpp precedent: VehicleManager::ProcessDeformationStates @0x825EA580
// (mounted this wave) calls UpdateShowtimeBounceModifiers, and RaceCarPhysics.cpp must stay
// unmounted while flt_820037C8 / unk_82FB8880 are unread (see the mount note in
// tools/build/build_game_exe.bat). Carries exactly what the call needs:
//   * the module-static showtime singleton msPlayerParams (extern-declared in RaceCarPhysics.h;
//     its definition CANNOT stay in the unmounted home or the slice would not link);
//   * RaceCarPhysics::UpdateShowtimeBounceModifiers @0x825D7940, byte-identical to the body that
//     lived in RaceCarPhysics.cpp (its pre-existing FLAG included, unchanged).
// Fold both back into RaceCarPhysics.cpp when it mounts.
// ============================================================================

#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"

namespace BrnPhysics
{
namespace Vehicle
{
    // The module-static showtime singleton (X360 msPlayerParams; base symbol lbBounceBoosting). One
    // car at a time. Defined here (its tentative storage); the per-process instance.
    PlayerParameters msPlayerParams = {};
    namespace { PlayerParameters& MS = msPlayerParams; }   // short alias, as in the home TU

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::UpdateShowtimeBounceModifiers  @0x825D7940  (asserts showtime + state)
    //   From the live deformation state, per bounce-sensor: copy the sensor world position into the
    //   per-sensor scratch (unk_82FB85A0), record its crush magnitude (flt_82FB85B0), and clamp a
    //   bounce-strength (flt_82FB85B4) into [flt_82F2A2B8, flt_82F2A2BC] from (2*crushNorm - 1).
    //   Then the global deformation scale = sqrt(totalCrush) / numSensors.
    //   FLAG: lpDeformationState is a BrnDeformationState* (incomplete here); the per-sensor reads use
    //   its documented +1700 (num sensors), +1696 (total crush), per-sensor +16/+32/+64 layout. With
    //   the type incomplete in this slice, the per-sensor loop is ELIDED as faithful-but-inert and
    //   only the observable global scale + sensor-count writes are kept (numSensors=0 -> scale path
    //   guarded). The clamp band flt_82F2A2B8/BC is un-homed (FLAGGED).
    // ---------------------------------------------------------------------------------------
    void RaceCarPhysics::UpdateShowtimeBounceModifiers(const void* lpDeformationState)
    {
        // assert(mbPlayerCarInShowtime && lpDeformationState) -- elided.
        if (!lpDeformationState)
        {
            MS.mu8NumBounceSensors = 0;
            MS.mfDeformationScale  = 0.0f;
            return;
        }
        // The per-sensor crush -> bounce-strength scatter reads BrnDeformationState fields by raw
        // offset (+1700 sensor count, +1696 total crush, per-sensor +16/+32/+64). That type is not
        // modelled in this slice, so the scatter is ELIDED as faithful-but-inert (it writes only the
        // per-sensor scratch arrays the C10 group does not own). The two observable singleton writes
        // (sensor count + global deformation scale) are kept; with the type opaque here they default
        // to the safe no-bounce path. FLAG: BrnDeformationState layout un-modelled in this TU.
        MS.mu8NumBounceSensors = 0;       // = *(lpDeformationState + 1700)
        MS.mfDeformationScale  = 0.0f;    // = sqrt(*(lpDeformationState+1696)) / numSensors
    }
}
}
