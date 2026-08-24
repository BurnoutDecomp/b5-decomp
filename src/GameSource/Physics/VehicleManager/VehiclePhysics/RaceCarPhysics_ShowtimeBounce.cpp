// ============================================================================
// b5-decomp/src/GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics_ShowtimeBounce.cpp
//
// SLICE TU, split out of RaceCarPhysics.cpp 2026-08-06 (PhysicsModule::Update leaves wave) in the
// RaceCarPhysics_Construct.cpp precedent: VehicleManager::ProcessDeformationStates @0x825EA580
// calls UpdateShowtimeBounceModifiers. Carries exactly what the call needs:
//   * the module-static showtime singleton msPlayerParams (extern-declared in RaceCarPhysics.h);
//   * RaceCarPhysics::UpdateShowtimeBounceModifiers @0x825D7940.
// (The old banner's "RaceCarPhysics.cpp must stay unmounted" rationale aged out -- both TUs are
// mounted in build_game_exe.bat; the split stands because it links today and folding is churn.)
//
// REWRITTEN 2026-08-24 (showtime wave). The previous body was a SILENT-DROP STUB (audit F6): it
// compiled, linked, ran, copied nothing, and published numSensors = 0 / deformationScale = 0.0
// every frame -- the exact defect class the ledger warns about. Every excuse it carried was
// stale: the CarState type is fully modelled (BrnDeformationState.h, deform-land wave), the
// clamp band flt_82F2A2B8/flt_82F2A2BC is image-read ([0.4, 1.0]), and the per-sensor scratch
// is now the NAMED PlayerParameters::maBounceSensors array (+0x120, 32-byte stride).
// ============================================================================

#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationState.h"  // CarState
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"           // vpu::MagnitudeSquared
#include <cmath>                                     // std::sqrt

namespace BrnPhysics
{
namespace Vehicle
{
    // The module-static showtime singleton (X360 msPlayerParams; base symbol lbBounceBoosting). One
    // car at a time. Defined here (its tentative storage); the per-process instance.
    PlayerParameters msPlayerParams = {};
    namespace { PlayerParameters& MS = msPlayerParams; }   // short alias, as in the home TU

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::UpdateShowtimeBounceModifiers  @0x825D7940
    //   Publish the live deformation state into the showtime singleton, per sensor
    //   (asm 0x825D79F8..0x825D7AE0, CarState record stride 80, scratch stride 32):
    //     maBounceSensors[i].mWorldScalarVector = sensor +0x10 (mWorldScalarVector);
    //     maBounceSensors[i].mfSpecScalar       = sensor +0x40 (mfSpecScalar);
    //     maBounceSensors[i].mfCrushFactor      = clamp(1 - 2*|sensor.mDisplacement|^2, 0.4, 1.0)
    //       -- the fnmsubs(magsq, 2.0, 1.0) + two fsels @0x825D7AB8-D0; the band is
    //       flt_82F2A2B8 (0.4) / flt_82F2A2BC (1.0), both image-read.
    //   Then the global scale: mfDeformationScale = sqrt(summedDisplacementSquared) / numSensors
    //   (fsqrts + s64->f64 fcfid convert of the count @0x825D7AE4-0x825D7B04).
    //   The per-iteration bounds assert is the console's own (CarState::GetSensor's tripwire,
    //   BrnDeformationState.h:62, fired three times per record on X360 because the accessor is
    //   inlined thrice; once per iteration here).
    // ---------------------------------------------------------------------------------------
    void RaceCarPhysics::UpdateShowtimeBounceModifiers(const Deformation::CarState* lpDeformationState)
    {
        namespace vpu = rw::math::vpu;
        static const f32 KF_CRUSH_BAND_LO = 0.4f;   // flt_82F2A2B8
        static const f32 KF_CRUSH_BAND_HI = 1.0f;   // flt_82F2A2BC

        CGS_ASSERT(mbPlayerCarInShowtime, "mbPlayerCarInShowtime");   // :1157
        CGS_ASSERT(lpDeformationState != nullptr, "lpDeformationState");   // :1158

        const u8 luNumSensors = lpDeformationState->mu8NumSensors;    // lbz +0x6A4
        MS.mu8NumBounceSensors = luNumSensors;                        // byte_82FB8590

        for (u8 luSensor = 0; luSensor < luNumSensors; ++luSensor)
        {
            CGS_ASSERT(luSensor < lpDeformationState->mu8NumSensors,
                       "luSensorIndex < mu8NumSensors");              // BrnDeformationState.h:62
            const Deformation::CarSensorState& lrSensor = lpDeformationState->maSensors[luSensor];
            PlayerParameters::BounceSensor& lrOut = MS.maBounceSensors[luSensor];

            lrOut.mWorldScalarVector = lrSensor.mWorldScalarVector;   // lvx rec+0x10 -> scratch+0
            lrOut.mfSpecScalar       = lrSensor.mfSpecScalar;         // lfs rec+0x40 -> scratch+0x10

            // crush factor: 1 - 2*|displacement|^2, clamped to [0.4, 1.0].
            const f32 lfDispSq = vpu::MagnitudeSquared(lrSensor.mDisplacement);   // vmsum3fp rec+0x20
            f32 lfCrush = 1.0f - 2.0f * lfDispSq;                     // fnmsubs(magsq, 2.0, 1.0)
            if (lfCrush < KF_CRUSH_BAND_LO) lfCrush = KF_CRUSH_BAND_LO;   // fsel vs flt_82F2A2B8
            if (lfCrush > KF_CRUSH_BAND_HI) lfCrush = KF_CRUSH_BAND_HI;   // fsel vs flt_82F2A2BC
            lrOut.mfCrushFactor = lfCrush;                            // stfsx scratch+0x14
        }

        // global deformation scale = sqrt(summed displacement^2) / numSensors. The console does
        // the integer->double convert unguarded (fcfid @0x825D7AF8); a zero-sensor record yields
        // the same IEEE 0/0 result on both machines, so no guard is invented.
        MS.mfDeformationScale =
            std::sqrt(lpDeformationState->mfSummedDisplacementSquared)
            / static_cast<f32>(luNumSensors);                         // flt_82FB84B4
    }
}
}
