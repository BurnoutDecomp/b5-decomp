#include "GameSource/World/AI/BrnAIDebugComponent.h"

#include "GameSource/World/AI/BrnAIModule.h"                                      // BrnAI::AIModule (member-pointer completeness)
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAIDriver.h"
#include "GameSource/World/AI/BrnAIAggression.h"
#include "GameSource/World/AI/BrnAIBuzzBy.h"
#include "GameSource/BurnoutConstants.h"                                           // EActiveRaceCarIndex operator++
#include "GameShared/GameClasses/Development/CgsStrStream.h"                       // CgsDev::SimpleStrStream
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"
#include "GameSource/World/AI/Route/BrnRoute.h"                                    // Route / RouteNode (state table)
#include "rw/math/vpu/vector3_operation.h"                                         // Magnitude / operator-

int MaybeDrawText(CgsDev::Debug2DImmediateRender* lpDisplay, const char* lpcText,
                  f32 lfX, f32 lfY, f32 lfScale, CgsDev::RGBA lColour, bool lbCentred);

// BrnAI::AIDebugComponent -- the in-game "Main AI" debug overlay. Reconstructed from the X360
// ARTIST build (function addresses cited per method) cross-checked against the DecFIGS DWARF
// (member names / virtual shape) and the recovered base CgsDev::DebugComponent.
//
// Bodied in this TU: GetName / GetPath, the HUD path (RenderHUD with the inlined
// DrawDriftingDebug, DrawAIStatesTable and its StateTable* cursor helpers) and DrawAIStatesOnCar.
// Their member reads are on the homed AICar / AIDriver / AIAggression records, and the toggle each
// one reads is pinned by its byte offset in this object (+0x18 mbDrawCarUnderSection .. +0x39
// mbDrawDriftingDebug, +0x40/+0x44 the state-table cursor). RenderWorld, OnActivate and the 3D
// per-car draw helpers (DrawAICarRoute / DrawAICarSection / DrawCarControlData /
// DrawAggressiveTargetPoints / DrawAggressiveWarning / DrawCarIndices / DrawChevrons /
// DrawPortalTargets / DrawDirectDestinationVector / DrawResetOnTrackAISection / DrawAISectionEdge /
// DeactiveateDriversCallback) are not bodied here.

namespace BrnAI
{

// @0x827671B8  The component's display name in the debug menu.
const char* AIDebugComponent::GetName() const
{
    return "Main AI";
}

// @0x82767678  The component's debug-menu group path.
const char* AIDebugComponent::GetPath() const
{
    return "AI";
}

namespace
{
    // The debug read-out names for the AI enums, in enumerator order.
    const char* const KAPC_AI_BEHAVIOUR_NAMES[E_AI_BEHAVIOUR_COUNT] =
    {
        "Stop", "RollingStart", "DriveThru", "Cruising", "Fighting", "QuickTurn", "SlowTurn",
        "Crashing", "Donut", "PostRaceWin", "PostRaceLose",
    };

    const char* const KAPC_BIAS_MODE_NAMES[eBiasMode_Count] =
    {
        "Race", "RaceDangerous", "Slam", "RoadRage", "CloseToPlayer", "SlamRivals", "HitOncoming",
        "SlamDangerous", "NoHNGCentering", "VeerAwayFromPlayer",
    };

    const char* const KAPC_AGGRESSION_STATE_NAMES[E_AI_AGGRESSION_STATE_COUNT] =
    {
        "OutOfRange", "OvertakeToSlam", "DropBackToSlam", "AttackSlam", "Wait", "Veer", "Passive",
        "FallPast", "BeFodder", "ClipOffBehind", "OvertakeFast", "OvertakeSlowly", "SpurtForward",
        "VeerExtreme", "HangAroundAhead",
    };

    const char* const KAPC_SPEED_MATCH_NAMES[ESpeedMatch_Count] =
    {
        "Disabled", "Enabled", "Slower", "SlowToClip", "OvertakeFast", "OvertakeSlowly",
    };

    const char* const KAPC_PERSONALITY_TYPE_NAMES[E_PERSONALITY_TYPE_COUNT] =
    {
        "Racing", "Aggression",
    };

    const char KAAC_DRIFT_STATE_NAMES[E_DRIFT_STATE_COUNT][64] =
    {
        "Normal", "Start", "Drift", "Exit",
    };

    // m/s to mph: 1 / 0.44704.
    const f32 KF_MPS_TO_MPH = 1.0f / 0.44704f;

    // The state-table speed-method and route-finding-style names, in enumerator order.
    const char KAAC_SPEED_SELECTION_METHOD_NAMES[E_AI_SPEED_SELECTION_METHOD_COUNT][64] =
    {
        "FreeRoam", "Race", "MatchPlayer", "Personality", "Top",
    };

    const char KAAC_ROUTE_FINDING_STYLE_NAMES[E_ROUTE_FINDING_MARKED_MAN + 1][64] =
    {
        "FreeRoam", "Race", "RoadRage", "Pursuit", "AvoidPlayer", "AlwaysStraight", "MarkedMan",
    };

    // State table layout: the label column is wider than the per-car columns.
    const f32 KF_STATE_TABLE_ORIGIN       = 50.0f;
    const f32 KF_STATE_TABLE_LABEL_WIDTH  = 180.0f;
    const f32 KF_STATE_TABLE_COLUMN_WIDTH = 130.0f;
    const f32 KF_STATE_TABLE_ROW_HEIGHT   = 20.0f;
    const s32 KI_STATE_TABLE_MAX_CARS     = 8;
    const u32 KU_STATE_TABLE_LABEL_COLOUR = 0xFF4062FFu;

    // Route status words of the state table.
    const char* const KPC_ROUTE_STATUS_COMPLETE = "Complete";
    const char* const KPC_ROUTE_STATUS_PARTIAL  = "Partial";
    const char* const KPC_ROUTE_STATUS_BLOCK    = "Blocked";
    const char* const KPC_ROUTE_STATUS_UNKNOWN  = "Unknown";
    const char* const KPC_ROUTE_INVALID_BLOCK   = "Blocked ";

    // A car's checkpoint distance holds this until it has a checkpoint to measure to.
    const f32 KF_NO_CHECKPOINT_DISTANCE = 3.4028235e+38f;

    // The route section after the car's next route node, 0x7FFF when there is none.
    u16 GetNextNextRouteSectionIndex(const AICar* lpCar)
    {
        if (lpCar->HasValidRoute())
        {
            const s32 liNode = lpCar->GetNextRouteNodeIndex() + 1;
            if (liNode >= 0 && liNode < lpCar->GetRoute()->GetNodeCount())
            {
                return lpCar->GetRoute()->GetNode(liNode)->GetSectionIndex();
            }
        }
        return 0x7FFF;
    }

    // Where the section-under-car and drift read-outs are drawn.
    const Vector2 KV2_HUD_TEXT_POSITION = { 400.0f, 500.0f, 0.0f, 0.0f };

    const f32 KF_DRIFT_DEBUG_TEXT_SCALE = 20.0f;
}

// The index of the AI section under the player car (or "Invalid"), the drift state of the current
// AI car, the AI state table and the buzz-by timer.
void AIDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay)
{
    if (mbDrawCarUnderSection)
    {
        const AICar* lpPlayerCar = mpAIModule->GetAICar(mpAIModule->mePlayerGlobalRaceCarIndex);
        if (lpPlayerCar->muBestSectionIndex == 0x7FFF)
        {
            lpDisplay->DrawText("Invalid", KV2_HUD_TEXT_POSITION, 40.0f, 0xFFFFFFFFu, false);
        }
        else
        {
            CgsDev::SimpleStrStream lStream;
            lStream.Reset();
            lStream << static_cast<s32>(lpPlayerCar->muBestSectionIndex);
            lpDisplay->DrawText(lStream.GetBuffer(), KV2_HUD_TEXT_POSITION, 40.0f, 0xFFFFFFFFu, false);
        }
    }

    if (mbDrawDriftingDebug)
    {
        DrawDriftingDebug(lpDisplay);
    }

    if (mbDrawAICarStatesTable)
    {
        DrawAIStatesTable(lpDisplay);
    }

    if (mbDrawBuzzTime)
    {
        mpAIModule->mBuzzBy.DrawBuzzTimer();
    }
}

// The drift state of the current AI car while it is in range.
void AIDebugComponent::DrawDriftingDebug(CgsDev::Debug2DImmediateRender* lpDisplay)
{
    const AICar& lrCar = mpAIModule->maAICars[miCurrentAICar];
    if (lrCar.meCarState == E_AI_CAR_STATE_IN_RANGE)
    {
        lpDisplay->DrawText(KAAC_DRIFT_STATE_NAMES[lrCar.GetDriver()->GetDriftState()], KV2_HUD_TEXT_POSITION,
                            KF_DRIFT_DEBUG_TEXT_SCALE, 0xFFFFFFFFu, false);
    }
}

// Over each active AI-driven car, one line with whichever per-car read-outs are toggled on,
// shrinking with distance from the player. A player car in the junkyard just says so.
void AIDebugComponent::DrawAIStatesOnCar(CgsDev::Debug3DImmediateRender* lpDisplay)
{
    const rw::RGBA KCOLOUR_TEXT(255, 64, 64, 255);

    for (EActiveRaceCarIndex leActiveCar = E_ACTIVE_RACE_CAR_INDEX_0;
         leActiveCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; leActiveCar++)
    {
        AIDriver* lpDriver = mpAIModule->GetAIDriver(leActiveCar);
        if (!lpDriver->IsActive())
        {
            continue;
        }

        const AICar* lpCar = lpDriver->GetCar();
        if (lpCar->mbIsPlayer)
        {
            if (lpCar->mbIsInJunkYard)
            {
                CgsDev::SimpleStrStream lStream;
                lStream << "In Junk Yard ";
                lpDisplay->DrawText(lpCar->GetPosition(), lStream.GetBuffer(), 32.0f, KCOLOUR_TEXT);
            }
            continue;
        }

        CgsDev::SimpleStrStream lStream;
        CgsDev::StrStreamBase&  lrStream     = lStream;
        const AIAggression&     lrAggression = *lpDriver->GetAggression();

        if (mbDrawProximityOnCar)
        {
            lStream << "prox " << lpCar->miProximityIndex << " ";
        }
        if (mbDrawBehaviourOnCar)
        {
            lStream << KAPC_AI_BEHAVIOUR_NAMES[lpCar->meBehaviour];
            lStream << " ";
        }
        if (mbDrawSpeedOnCar)
        {
            lStream << lpCar->GetSpeed() * KF_MPS_TO_MPH;
            lStream << "mph ";
        }
        if (mbDrawFanModeOnCar)
        {
            lStream << KAPC_BIAS_MODE_NAMES[lpDriver->mSteeringFan.meBiasMode];
            lStream << " ";
        }
        if (mbDrawAggressionStateOnCar)
        {
            lStream << KAPC_AGGRESSION_STATE_NAMES[lrAggression.GetAggressionState()];
            lStream << " ";
            if (lrAggression.GetAggressionState() == E_AI_AGGRESSION_STATE_DROP_BACK_TO_SLAM)
            {
                lStream << lrAggression.GetLeadingSeparation(lrAggression.mpCar, lrAggression.mpTargetCar);
                lStream << " ";
            }
        }
        if (mbDrawAggressionLevelOnCar)
        {
            lStream << lpCar->mAggressiveness.GetAggressionLevel();
            lStream << " ";
        }
        if (mbDrawPersonalityTypeOnCar)
        {
            lStream << KAPC_PERSONALITY_TYPE_NAMES[lpCar->mePersonalityType];
            lStream << " ";
        }
        if (mbDrawScheduleOffsetOnCar)
        {
            lStream << lpCar->mfScheduleOffset0;
            lStream << "/" << lpCar->mfScheduleOffset1;
        }
        if (mbDrawInvulnerabilityOnCar)
        {
            lrStream << lpDriver->IsInvulnerable();
        }
        if (mbDrawPlayerSlowSpeedTimeOnCar)
        {
            lStream << lpDriver->mfPlayerSlowSpeedTime;
        }
        if (mbDrawSpeedMatchingOnCar)
        {
            lStream << KAPC_SPEED_MATCH_NAMES[lrAggression.meSpeedMatchType];
            lStream << " ";
        }

        f32 lfDistanceShrink = lpCar->GetDistanceToPlayer() * 0.1f;
        lfDistanceShrink = (-lfDistanceShrink >= 0.0f) ? 0.0f : lfDistanceShrink;
        lfDistanceShrink = (10.0f - lfDistanceShrink >= 0.0f) ? lfDistanceShrink : 10.0f;

        lpDisplay->DrawText(lpCar->GetPosition(), lStream.GetBuffer(), 18.0f - lfDistanceShrink, KCOLOUR_TEXT);
    }
}

// One cell of the state table at the running cursor, every other row dimmed to two thirds (and
// made opaque), then the cursor moves down a row.
void AIDebugComponent::StateTableDrawEntry(CgsDev::Debug2DImmediateRender* lpDisplay, const char* lpcText, RGBA lColour)
{
    const s32 liRow = static_cast<s32>(mfStateTableY * 0.05f);
    if (liRow % 2 == 0)
    {
        const u32 luRed   = ((lColour & 0xFFu) * 2) / 3;
        const u32 luGreen = (((lColour >> 8) & 0xFFu) * 2) / 3;
        const u32 luBlue  = (((lColour >> 16) & 0xFFu) * 2) / 3;
        lColour = 0xFF000000u | ((luBlue & 0xFFu) << 16) | ((luGreen & 0xFFu) << 8) | (luRed & 0xFFu);
    }

    MaybeDrawText(lpDisplay, lpcText, mfStateTableX, mfStateTableY, 16.0f, lColour, false);
    mfStateTableY += KF_STATE_TABLE_ROW_HEIGHT;
}

void AIDebugComponent::StateTableBegin()
{
    mfStateTableX = KF_STATE_TABLE_ORIGIN;
    mfStateTableY = KF_STATE_TABLE_ORIGIN;
}

void AIDebugComponent::StateTableNextColumn()
{
    const f32 lfColumnWidth = (mfStateTableX == KF_STATE_TABLE_ORIGIN) ? KF_STATE_TABLE_LABEL_WIDTH
                                                                        : KF_STATE_TABLE_COLUMN_WIDTH;
    mfStateTableY  = KF_STATE_TABLE_ORIGIN;
    mfStateTableX += lfColumnWidth;
}

// The label column, then one column per active AI car (up to eight): indices, range state, speeds,
// route state, behaviour, aggression, position, sections, timers and, behind their toggles, the
// speed-calculation and personality values. The player car's column is green, the in-range car
// nearest the player white, other in-range cars red and out-of-range cars cyan.
void AIDebugComponent::DrawAIStatesTable(CgsDev::Debug2DImmediateRender* lpDisplay)
{
    CgsDev::SimpleStrStream lStream;
    CgsDev::StrStreamBase&  lrStream = lStream;

    f32                 lfNearestDistance = 3.4028235e+38f;
    EGlobalRaceCarIndex leNearestCar      = E_GLOBAL_RACE_CAR_INDEX_COUNT;
    s32                 liCarColumns      = 0;

    for (EGlobalRaceCarIndex leCar = E_GLOBAL_RACE_CAR_INDEX_0; leCar < E_GLOBAL_RACE_CAR_INDEX_COUNT; leCar++)
    {
        const AICar* lpCar = mpAIModule->GetAICar(leCar);
        if (lpCar->IsActive() && lpCar->meCarState == E_AI_CAR_STATE_IN_RANGE && !lpCar->mbIsPlayer)
        {
            const AICar* lpPlayerCar = mpAIModule->GetAICar(mpAIModule->mePlayerGlobalRaceCarIndex);
            const f32 lfDistance = rw::math::vpu::Magnitude(
                rw::math::vpu::operator-(lpPlayerCar->GetPosition(), lpCar->GetPosition()));
            if (lfDistance < lfNearestDistance)
            {
                lfNearestDistance = lfDistance;
                leNearestCar      = leCar;
            }
        }
    }

    StateTableBegin();
    StateTableDrawEntry(lpDisplay, "Global Index", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Active Index", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "State", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Opponent index", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Speed", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Desired speed", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Racing line", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Route status", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Current checkpoint", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Behaviour", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Route finding style", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Route finding modifiers", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Speed matching", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Distance to player", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Misc state", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Aggression level", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Aggression", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Position", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Current section", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Next route section", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Destination section", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Distance to checkpoint", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Buzz time", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Wrong way time", KU_STATE_TABLE_LABEL_COLOUR);
    if (mbDrawSpeedCalculationStateOnTable)
    {
        StateTableDrawEntry(lpDisplay, "Hard shoulder speed", KU_STATE_TABLE_LABEL_COLOUR);
        StateTableDrawEntry(lpDisplay, "Proximity speed", KU_STATE_TABLE_LABEL_COLOUR);
        StateTableDrawEntry(lpDisplay, "Cornering speed", KU_STATE_TABLE_LABEL_COLOUR);
        StateTableDrawEntry(lpDisplay, "Fan Lerp", KU_STATE_TABLE_LABEL_COLOUR);
        StateTableDrawEntry(lpDisplay, "Top Speed", KU_STATE_TABLE_LABEL_COLOUR);
        StateTableDrawEntry(lpDisplay, "Accelerator", KU_STATE_TABLE_LABEL_COLOUR);
    }
    if (mbDrawrPersonalityOnTable)
    {
        StateTableDrawEntry(lpDisplay, "Personality aggression", KU_STATE_TABLE_LABEL_COLOUR);
        StateTableDrawEntry(lpDisplay, "Personality speed", KU_STATE_TABLE_LABEL_COLOUR);
        StateTableDrawEntry(lpDisplay, "Personality skill", KU_STATE_TABLE_LABEL_COLOUR);
        StateTableDrawEntry(lpDisplay, "Personality overtake", KU_STATE_TABLE_LABEL_COLOUR);
    }

    for (EGlobalRaceCarIndex leCar = E_GLOBAL_RACE_CAR_INDEX_0; leCar < E_GLOBAL_RACE_CAR_INDEX_COUNT; leCar++)
    {
        if (liCarColumns >= KI_STATE_TABLE_MAX_CARS)
        {
            break;
        }

        const AICar* lpCar = mpAIModule->GetAICar(leCar);
        if (!lpCar->IsActive())
        {
            continue;
        }

        const AIDriver* lpDriver = (lpCar->meCarState == E_AI_CAR_STATE_IN_RANGE) ? lpCar->GetDriver() : nullptr;

        ++liCarColumns;
        StateTableNextColumn();

        RGBA lColour;
        if (lpCar->mbIsPlayer)
        {
            lColour = 0xFF40C440u;
        }
        else if (leNearestCar == leCar)
        {
            lColour = 0xFFDCDCDCu;
        }
        else if (lpCar->meCarState == E_AI_CAR_STATE_IN_RANGE)
        {
            lColour = 0xFF4060FFu;
        }
        else
        {
            lColour = 0xFF40C4C4u;
        }

        // Global index
        lStream.Reset();
        lrStream << static_cast<s32>(leCar);
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Active index
        lStream.Reset();
        if (lpDriver != nullptr)
        {
            lrStream << lpDriver->GetRelatedActiveCarIndex();
        }
        else
        {
            lStream << "Invalid";
        }
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // State
        lStream.Reset();
        lStream << ((lpCar->meCarState == E_AI_CAR_STATE_IN_RANGE) ? "InRange " : "OutOfRange ");
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Opponent index
        lStream.Reset();
        lrStream << static_cast<s32>(lpCar->GetOpponentIndex());
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Speed
        lStream.Reset();
        if (lpCar->IsCrashing())
        {
            lStream << "Crashing";
        }
        else
        {
            lStream << lpCar->GetSpeed() * KF_MPS_TO_MPH;
            lStream << "mph";
        }
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Desired speed
        lStream.Reset();
        lStream << lpCar->GetDesiredSpeed() * KF_MPS_TO_MPH;
        lStream << "mph";
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Racing line
        lStream.Reset();
        if (lpDriver != nullptr)
        {
            lStream << (lpDriver->mbIsRacingLineInitialised ? "Valid" : "Invalid");
        }
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Route status
        lStream.Reset();
        const Route* lpRoute = lpCar->GetRoute();
        if (lpCar->HasValidRoute())
        {
            switch (lpRoute->GetStatus())
            {
            case Route::E_STATUS_COMPLETE: lStream << KPC_ROUTE_STATUS_COMPLETE; break;
            case Route::E_STATUS_PARTIAL:  lStream << KPC_ROUTE_STATUS_PARTIAL;  break;
            case Route::E_STATUS_BLOCKED:  lStream << KPC_ROUTE_STATUS_BLOCK;    break;
            default:                       lStream << KPC_ROUTE_STATUS_UNKNOWN;  break;
            }
            lStream << " (";
            lrStream << lpRoute->GetNodeCount();
            lStream << ")";
        }
        else
        {
            lStream << "Invalid ";
            if (lpRoute->GetStatus() == Route::E_STATUS_BLOCKED)
            {
                lStream << KPC_ROUTE_INVALID_BLOCK;
            }
        }
        if (lpCar->NeedsNewRoute())
        {
            lStream << "NeedsNew ";
        }
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Current checkpoint
        lStream.Reset();
        lrStream << lpCar->GetCurrentCheckpoint();
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Behaviour
        StateTableDrawEntry(lpDisplay, KAPC_AI_BEHAVIOUR_NAMES[lpCar->meBehaviour], lColour);

        // Route finding style
        lStream.Reset();
        lStream << KAAC_ROUTE_FINDING_STYLE_NAMES[lpCar->GetRouteFindingStyle()];
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Route finding modifiers
        lStream.Reset();
        if (lpCar->HasBlockCheckpoints())
        {
            lStream << "Block ";
        }
        else
        {
            if (lpCar->WantsAlternativeRoute())
            {
                lStream << "Deviate ";
            }
            if (lpCar->UseAIShortcuts())
            {
                lStream << "Shortcuts ";
            }
        }
        if (lpCar->GetRouteFindingStyle() == E_ROUTE_FINDING_FREE_ROAM)
        {
            lStream << "FreeRoam";
        }
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Speed matching (the speed selection method)
        StateTableDrawEntry(lpDisplay, KAAC_SPEED_SELECTION_METHOD_NAMES[lpCar->meSpeedSelectionMethod], lColour);

        // Distance to player
        lStream.Reset();
        lStream << lpCar->GetDistanceToPlayer();
        lStream << "m ";
        lStream << (lpCar->IsAheadOfPlayer() ? "Ahead" : "Behind");
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Misc state
        lStream.Reset();
        if (lpDriver != nullptr)
        {
            if (lpDriver->mfBrake > 0.1f)
            {
                lStream << "Brake ";
            }
            if (lpDriver->mbBoosting)
            {
                lStream << "Boost ";
            }
            if (lpCar->mbIsDrifting)
            {
                lStream << (lpDriver->mbWantToExitDrift ? "Exit drift " : "Drift ");
            }
            if (lpCar->IsCrashing())
            {
                lStream << "Crash ";
            }
        }
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Aggression level
        lStream.Reset();
        lStream << lpCar->mAggressiveness.GetAggressionLevel();
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Aggression
        lStream.Reset();
        if (lpDriver != nullptr)
        {
            lStream << KAPC_AGGRESSION_STATE_NAMES[lpDriver->GetAggression()->GetAggressionState()];
        }
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Position
        const Vector3 lv3Position = lpCar->GetPosition();
        lStream.Reset();
        lrStream << static_cast<s32>(lv3Position.x);
        lStream << ",";
        lrStream << static_cast<s32>(lv3Position.y);
        lStream << ",";
        lrStream << static_cast<s32>(lv3Position.z);
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Current section
        if (lpCar->muBestSectionIndex == 0x7FFF)
        {
            StateTableDrawEntry(lpDisplay, "Invalid", lColour);
        }
        else
        {
            lStream.Reset();
            lrStream << static_cast<s32>(lpCar->muBestSectionIndex);
            StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);
        }

        // Next route section
        if (GetNextNextRouteSectionIndex(lpCar) == 0x7FFF)
        {
            StateTableDrawEntry(lpDisplay, "Invalid", lColour);
        }
        else
        {
            lStream.Reset();
            lrStream << static_cast<s32>(GetNextNextRouteSectionIndex(lpCar));
            StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);
        }

        // Destination section
        if (lpCar->GetDestinationSectionIndex() == 0x7FFF)
        {
            StateTableDrawEntry(lpDisplay, "Invalid", lColour);
        }
        else
        {
            lStream.Reset();
            lrStream << static_cast<s32>(lpCar->GetDestinationSectionIndex());
            StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);
        }

        // Distance to checkpoint
        lStream.Reset();
        if (lpCar->mfDistanceToCheckpoint != KF_NO_CHECKPOINT_DISTANCE)
        {
            lStream << lpCar->mfDistanceToCheckpoint;
            lStream << "m";
        }
        else
        {
            lStream << "Invalid";
        }
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Buzz time
        lStream.Reset();
        lStream << mpAIModule->mBuzzBy.GetBuzzFrequency(lpCar);
        lStream << "s";
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Wrong way time
        lStream.Reset();
        lStream << lpCar->mfWrongWayTime;
        lStream << "s";
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        if (mbDrawSpeedCalculationStateOnTable && lpDriver != nullptr)
        {
            lStream.Reset();
            lStream << lpDriver->mfTopSpeed * KF_MPS_TO_MPH;
            lStream << "mph";
            StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

            lStream.Reset();
            lStream << lpDriver->mfAccelerator;
            StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);
        }

        if (mbDrawrPersonalityOnTable)
        {
            lStream.Reset();
            lStream << lpCar->mAggressiveness.GetAggressionLevel();
            StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

            lStream.Reset();
            lStream << lpCar->mAggressiveness.GetProximityToSpeedMatch();
            StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);
        }
    }
}

}
