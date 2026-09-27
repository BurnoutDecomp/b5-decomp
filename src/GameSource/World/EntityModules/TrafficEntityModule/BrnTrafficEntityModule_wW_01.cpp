// ============================================================================
// BrnTrafficEntityModule_wW_01.cpp -- the race start grid: keeping traffic off it.
//
//   TrafficEntityModule::KillTrafficOnStartGridWholeSale
//   TrafficEntityModule::CalcRaceCarOnStartGridFuzzyScores
//
// HandlePrepareForModeAction latches mbAtStartLineSoProtectRaceCarsFromTraffic from
// mbGameModeClearsTraffic and caches the event's grid in maEventGridStartPositions; the intro
// clears the flag again once the race cars are away. While it is set the traffic does two things:
//   * every decision frame, UpdateDecisionFrame sweeps every car out of a 200 m x 10 m cylinder
//     on the player (KillTrafficOnStartGridWholeSale), so nothing spawns into the grid;
//   * each param's fuzzy race-car inputs come from the nearest grid slot ahead of it instead of
//     the physical race cars (CalcRaceCarOnStartGridFuzzyScores), and the drive-around score is
//     zeroed (UpdateParams_PrecalcBehaviourParams), so traffic already on the road stops short
//     of the grid rather than swerving through it.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficMathsUtils.h"   // IsPointWithinSquishedCone
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficTrackWitness.h" // TrafficRemoveReasonTag

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include "rw/math/vpu/vector3_operation.h"   // Dot, Magnitude, operator-/*, operator!=
#include "rw/math/vpu/vector4_operation.h"   // Splat

#include <cfloat>    // FLT_MAX
#include <cmath>     // std::sqrt, fabsf
#include <cstdlib>   // getenv

namespace BrnTraffic
{
namespace
{
    // KillTrafficOnStartGridWholeSale's cylinder: the radius and height literals it loads, 200.0f
    // (0x43480000) and 10.0f (0x41200000). HandlePrepareForModeAction's inlined copy loads the same
    // two words.
    const f32 KF_START_GRID_WHOLESALE_CLEAR_RADIUS = 200.0f;
    const f32 KF_START_GRID_WHOLESALE_CLEAR_HEIGHT = 10.0f;

    // CalcRaceCarOnStartGridFuzzyScores' cone of interest, splatted from three literals on the
    // stack: cos half-angle 0.984f (0x3F7BE76D), length 100.0f (0x42C80000), vertical scale 1.0f
    // (0x3F800000). The running nearest distance starts at FLT_MAX (0x7F7FFFFF).
    const f32 KF_START_GRID_CONE_COS_ANGLE = 0.984f;
    const f32 KF_START_GRID_CONE_LENGTH    = 100.0f;
    const f32 KF_START_GRID_CONE_RECIP_Y   = 1.0f;

    // [FLAG PC witness] NOT console code. BRN_STARTGRID_DIAG, default off, read once. The wholesale
    // grid kill prints its first firing and then every firing that removed a car, capped.
    CgsDev::Log::DebugPrint* StartGridDiagStream_PC()
    {
        static const bool sbEnabled = (getenv("BRN_STARTGRID_DIAG") != 0);
        if (!sbEnabled || CgsDev::Log::gpDebugPrint == 0)
        {
            return 0;
        }
        return CgsDev::Log::gpDebugPrint;
    }
    s32 giStartGridDiagLinesLeft_PC = 24;
    u32 guStartGridDiagFires_PC     = 0;
    u32 guStartGridDiagRemoved_PC   = 0;
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::KillTrafficOnStartGridWholeSale   (35 insns)
//
// Three byte/word tests, then the cylinder kill with lbIncludeStatic = true (`li r6, 1`), so
// parked cars go too. The two float arguments ride f1/f2 and take the r4/r5 slots; the Vector3
// rides v1 and is preserved across the IsDecisionFrame call.
// ----------------------------------------------------------------------------
void TrafficEntityModule::KillTrafficOnStartGridWholeSale(Vector3 lPlayerPosition)
{
    if (mbAtStartLineSoProtectRaceCarsFromTraffic
        && meState == E_STATE_RUNNING
        && IsDecisionFrame())
    {
        // [FLAG PC witness] NOT console code. Counts, before the kill, the cars KillAllTrafficInCylinder
        // is about to hand to RemoveVehicle (its own predicate, read only). The count is taken first
        // because an offline standard car is only flagged should-be-removed there and stays alive
        // until the pool sweep, so an alive count after the call would miss it.
        CgsDev::Log::DebugPrint* const lpDiag_PC = StartGridDiagStream_PC();
        u32 luVictims_PC = 0;
        if (lpDiag_PC != 0 && giStartGridDiagLinesLeft_PC > 0)
        {
            const f32 lfRadiusSquared_PC = KF_START_GRID_WHOLESALE_CLEAR_RADIUS * KF_START_GRID_WHOLESALE_CLEAR_RADIUS;
            for (u32 luVehicle = 0; luVehicle < KU_MAX_TOTAL_TRAFFIC; ++luVehicle)
            {
                if (!GetVehicle(luVehicle)->IsAlive())
                {
                    continue;
                }
                const Vector3 lvPosition = GetVehicleTransform(luVehicle).Pos();
                const f32 lfDeltaX = lvPosition.x - lPlayerPosition.x;
                const f32 lfDeltaZ = lvPosition.z - lPlayerPosition.z;
                if (lfDeltaX * lfDeltaX + lfDeltaZ * lfDeltaZ > lfRadiusSquared_PC)
                {
                    continue;
                }
                if (fabsf(lvPosition.y - lPlayerPosition.y) >= KF_START_GRID_WHOLESALE_CLEAR_HEIGHT)
                {
                    continue;
                }
                ++luVictims_PC;
            }
        }

        {
            const TrafficRemoveReasonTag lTag("start-grid-wholesale");
            KillAllTrafficInCylinder(lPlayerPosition, KF_START_GRID_WHOLESALE_CLEAR_RADIUS,
                                     KF_START_GRID_WHOLESALE_CLEAR_HEIGHT, true);
        }

        // [FLAG PC witness] NOT console code.
        if (lpDiag_PC != 0)
        {
            ++guStartGridDiagFires_PC;
            guStartGridDiagRemoved_PC += luVictims_PC;
            if (giStartGridDiagLinesLeft_PC > 0 && (guStartGridDiagFires_PC == 1 || luVictims_PC != 0))
            {
                --giStartGridDiagLinesLeft_PC;
                *lpDiag_PC << "[startgrid] wholesale kill fired #" << guStartGridDiagFires_PC
                           << " removed=" << luVictims_PC
                           << " total=" << guStartGridDiagRemoved_PC
                           << " at=(" << lPlayerPosition.x << ", " << lPlayerPosition.y << ", "
                           << lPlayerPosition.z << ")"
                           << " radius=" << KF_START_GRID_WHOLESALE_CLEAR_RADIUS
                           << " height=" << KF_START_GRID_WHOLESALE_CLEAR_HEIGHT
                           << " [FLAG PC witness]\n";
            }
        }
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::CalcRaceCarOnStartGridFuzzyScores   (233 insns)
//
// ABI: r3 this, r4 luParam, r5..r9 the five outputs in declaration order. The one caller,
// UpdateParam_CheckIfNeedToSlow, seeds all five with KF_MAX_FLOAT before the call and reads them
// back after it, so an output this function does not write keeps that seed.
//
//   * mbGameModeClearsTraffic clear: nothing is written.
//   * The grid search: among the slots inside the param's cone (origin its LERPED position, its
//     direction, cos 0.984, 100 m, no vertical squish) take the nearest. Strictly nearer only, so
//     the first of two equal slots wins, and a NaN distance never wins.
//   * No slot found (the nearest is still the zero vector): nothing is written.
//   * Otherwise the slot is a stationary race car: distance and height along the param's own
//     direction and up, closing speed = the param's velocity along the unit separation, lane
//     position and speed-in-our-lane zero. Each result is a dot product broadcast to all four
//     lanes.
// ----------------------------------------------------------------------------
void TrafficEntityModule::CalcRaceCarOnStartGridFuzzyScores(u32 luParam,
                                                           VecFloat& lfRCDistance,
                                                           VecFloat& lfRCHeight,
                                                           VecFloat& lfRCClosingSpeed,
                                                           VecFloat& lfRCLanePos,
                                                           VecFloat& lfRCSpeedInOurLane) const
{
    CGS_ASSERT(mbAtStartLineSoProtectRaceCarsFromTraffic, "mbAtStartLineSoProtectRaceCarsFromTraffic");

    if (!mbGameModeClearsTraffic)
    {
        return;
    }

    // Streamed on the console into the assert buffer; the condition is the participant count's
    // byte test.
    CGS_ASSERT(muNumberOfParticipantsInCurrentEvent != 0,
               "There should have been at least one car starting this event!");

    const ParamTransform* const lpParamTransform = GetParamTransform(luParam);

    const Vector3 lZero = { 0.0f, 0.0f, 0.0f, 0.0f };

    f32     lfNearestDistance        = FLT_MAX;
    Vector3 lNearestStartingPosition = lZero;

    const VecFloat lfConeCosAngle = rw::math::vpu::Splat(KF_START_GRID_CONE_COS_ANGLE);
    const VecFloat lfConeLength   = rw::math::vpu::Splat(KF_START_GRID_CONE_LENGTH);
    const VecFloat lfConeRecipY   = rw::math::vpu::Splat(KF_START_GRID_CONE_RECIP_Y);

    for (u8 luiIndex = 0; luiIndex < muNumberOfParticipantsInCurrentEvent; ++luiIndex)
    {
        CGS_ASSERT(maEventGridStartPositions[luiIndex] != lZero,
                   "maEventGridStartPositions[luiIndex] != RwMath::GetVector3_Zero()");

        if (IsPointWithinSquishedCone(lpParamTransform->GetLerpedPos(),
                                      lpParamTransform->GetDirection(),
                                      lfConeCosAngle, lfConeLength, lfConeRecipY,
                                      maEventGridStartPositions[luiIndex]))
        {
            const f32 lfDistance =
                rw::math::vpu::Magnitude(maEventGridStartPositions[luiIndex] - lpParamTransform->GetLerpedPos());

            if (lfDistance < lfNearestDistance)
            {
                lfNearestDistance        = lfDistance;
                lNearestStartingPosition = maEventGridStartPositions[luiIndex];
            }
        }
    }

    if (lNearestStartingPosition != lZero)
    {
        const Vector3 lDiff = lNearestStartingPosition - lpParamTransform->GetLerpedPos();

        lfRCDistance = rw::math::vpu::Splat(rw::math::vpu::Dot(lDiff, lpParamTransform->GetDirection()));
        lfRCHeight   = rw::math::vpu::Splat(rw::math::vpu::Dot(lDiff, lpParamTransform->CalcUp()));
        lfRCLanePos  = rw::math::vpu::Splat(0.0f);

        // The grid slot does not move, so the closing velocity is the param's own. The console
        // normalises lDiff with a refined reciprocal square root and no zero guard, and so does this.
        // On this host lDiff is never zero: the tree's IsPointWithinSquishedCone rejects a slot at the
        // cone's origin (a host guard of its own; the console accepts it and gets a NaN here).
        const Vector3 lParamLinearVel = lpParamTransform->GetDirection() * lpParamTransform->GetSpeed();
        const Vector3 lClosingVel     = lParamLinearVel;
        const Vector3 lRCDirection    = lDiff * (1.0f / std::sqrt(rw::math::vpu::Dot(lDiff, lDiff)));

        lfRCClosingSpeed   = rw::math::vpu::Splat(rw::math::vpu::Dot(lRCDirection, lClosingVel));
        lfRCSpeedInOurLane = rw::math::vpu::Splat(0.0f);
    }
}

} // namespace BrnTraffic
