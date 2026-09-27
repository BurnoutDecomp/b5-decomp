// ============================================================================
// GameSource/Game/BrnHarnessWinTeleport.cpp
//
// [HARNESS -- NOT A CONSOLE FUNCTION] The guaranteed-win teleport, BRN_WIN_TELEPORT. Read the
// header banner first; this file holds the producer (which landmark, which point, when) and the
// mailbox. The consumer is PlaceOnTrackManager::ArmWinTeleportBringUp.
//
// PERMANENT harness capability. Inert unless BRN_WIN_TELEPORT is set: ModeManager tests
// IsEnabled() (one parse, then one bool) before it gathers anything, and the consumer reads one
// atomic sequence number per pre-physics update.
//
// ----------------------------------------------------------------------------
// WHERE THE CAR IS PUT, AND WHY THAT POINT
// ----------------------------------------------------------------------------
// The target is the landmark's BoxRegion centre (BoxRegion::ComputeTransform().wAxis). Measured
// on the three harness events (TRIGGERS.DAT, 2026-09-27): every one has a single checkpoint, its
// finish, and the boxes are wide, tall and thin --
//     race 557182 (junction 480852)            region 4677  dim 105.4 x 45.8 x  6.3
//     burning route 558944 (junction 480860)   region 4675  dim 131.9 x 29.6 x 13.4
//     marked man 560148 (junction 480856)      region 4673  dim  40.0 x 61.8 x  6.6
// i.e. a finish LINE: tens of metres across the road (local X), tens of metres tall (local Y),
// 6-13 m along the road (local Z). The place-on-track chain moves the request only
//   (a) vertically: a 50 m-up / 50 m-down line test through the point, and ComputeBestPlaceOnT
//       takes the nearest surface below the request height + 1 m. The box is 30-62 m tall, so a
//       road that runs through the box lies within its half-height of the centre and is the
//       surface the test finds (an overpass inside the box above the road but below the centre
//       would win instead -- the seat line below says so if it happens);
//   (b) sideways onto the lane, which is along the box's WIDE axis.
// Neither moves the car along the thin axis, so a centre request seats inside the box. This is
// checked, not assumed: every hop is followed by a `[win-teleport] seat` line that transforms the
// seated car into the box frame exactly as the landmark test does and prints inside=0|1.
//
// THE TRIGGER NEEDS ONE MORE FRAME. The landmark test (PostWorldUpdateLandmarksBringUp) skips any
// car whose position moved 10 m or more in one frame -- a placement jump -- so the frame the car
// lands is ignored and the NEXT frame's segment (seat -> seat + a few cm) is tested. A segment
// with either end inside the box is a hit, so a car at rest inside the box triggers.
//
// ----------------------------------------------------------------------------
// WHY AN OFFLINE RACE IS STAGED FIRST (BRN_WIN_TELEPORT_LEAD)
// ----------------------------------------------------------------------------
// Burning Route and Marked Man finish types do not depend on anyone else: Burning Route is 1st
// unless timed out (and its results position is the medal target), Marked Man passes unless the
// car was totalled. An offline RACE is different: FinishCurrentMode reads the player's LIVE race
// position (ScoringSystem::GetCarRacePosition), which UpdateRacePositions sorts by each car's
// distance to the finish, and the player's distance comes from its AI route
// (AICar::ComputeDistanceToCheckpoint: next route node's distance + the car's projection). After a
// long jump that route is stale -- the player-race route style never re-asks by itself; only
// CheckForSectionChange drops it after a second off-route -- so a car dropped straight onto the
// line would finish while its own tracker still ranks it behind rivals that are kilometres away.
// The finish ORDER (GetCarRaceFinishPosition, which feeds the results record and the medal) would
// still say 1, but the GUI finish type would not.
// So for the LAST checkpoint of an offline race the car is first STAGED lead metres outside the box
// on the approach side, FACING AWAY from it (the case holds the throttle, and a car facing the line
// would cross it on its own before the tracker catches up), and the finish hop is issued once the
// game's own live position reads 1 -- or after KI_LEADER_WAIT_CAP_FRAMES, whichever is first. Both
// outcomes are printed (why=leader / why=wait-cap).
//
// ----------------------------------------------------------------------------
// LOG LINES (unbudgeted: the variable itself is the opt-in and the harness is the only user)
//   [win-teleport] armed ...                          once, when the variable parses
//   [win-teleport] FAIL ...                           a refusal; the capability is then off
//   [win-teleport] hop <n> landmark=<k> region=<r> id=<CgsID> -> (x, y, z) dir=(..) remaining=<m> ...
//   [win-teleport] seat hop <n> ... inside=0|1        where the car actually landed
//   [win-teleport] credited landmark=<k> ...          the game cleared the checkpoint bit
//   [win-teleport] done state=<E_GMS_...> ...         the mode left IN_PROGRESS
// ============================================================================

#include "GameSource/Game/BrnHarnessWinTeleport.h"

#include "GameSource/GameState/ModeManager/BrnModeManager.h"            // ModeManager public accessors
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"     // GameMode::GetCurrentState
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"  // ScoringSystem::GetCarRacePosition
#include "GameSource/GameState/BrnGameStateSharedIO.h"                  // EGameModeType / EGameModeState
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // player RaceCarState
#include "SharedClasses/Trigger/BrnTriggerData.h"                       // TriggerData::GetLandmarkFromRegionIndex
#include "SharedClasses/Trigger/BrnLandmark.h"                          // Landmark (GetBoxRegion / GetId)
#include "SharedClasses/Trigger/BrnRegion.h"                            // BoxRegion::ComputeTransform / GetDimensions
#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // CgsDev::Log::gpDebugPrint

#include <atomic>
#include <cmath>
#include <cstdlib>

namespace BrnGame
{
namespace HarnessWinTeleport
{
namespace
{
    // Defaults. Harness numbers, not console ones.
    const s32 KI_DEFAULT_GAP_FRAMES             = 90;      // 1.5 s at the 60 Hz sim step
    const f32 KF_DEFAULT_LEAD_METRES            = 30.0f;   // outside a 6-13 m deep finish box
    const s32 KI_LEADER_WAIT_CAP_FRAMES         = 600;     // 10 s: the staged car stops waiting for position 1
    const s32 KI_MAX_FINISH_HOPS_PER_CHECKPOINT = 3;       // then the capability gives up, loudly
    const f32 KF_PLACEMENT_JUMP_METRES          = 10.0f;   // the landmark test's own placement-jump gate
    const f32 KF_MIN_AXIS_LENGTH                = 0.001f;

    // ------------------------------------------------------------------------
    // The mailbox. The payload is written before the sequence number is published, and the
    // consumer reads the sequence number before the payload, so a reader on another thread never
    // pairs a new number with an old payload. Requests are GAP frames apart, so a payload is never
    // rewritten while a reader holds its number.
    // ------------------------------------------------------------------------
    Request          gRequest = { { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, 0.0f, 0u };
    std::atomic<u32> gauRequestSeq(0u);

    // ------------------------------------------------------------------------
    // Settings, parsed once on the first IsEnabled() call.
    // ------------------------------------------------------------------------
    bool gbParsed         = false;
    bool gbEnabled        = false;
    f32  gfDelaySeconds   = 0.0f;
    s32  giGapFrames      = KI_DEFAULT_GAP_FRAMES;
    f32  gfSpeed          = 0.0f;
    f32  gfLeadMetres     = KF_DEFAULT_LEAD_METRES;

    // ------------------------------------------------------------------------
    // Producer state. One event per process: once DONE, the capability stays quiet.
    // ------------------------------------------------------------------------
    enum EStage
    {
        E_STAGE_WAITING_DELAY = 0,   // IN_PROGRESS time has not reached the delay yet
        E_STAGE_READY,               // the next landmark is issued once GAP frames have passed
        E_STAGE_STAGED,              // offline race: parked outside the finish, waiting for position 1
        E_STAGE_HOPPED,              // a finish hop is out; waiting for the game to credit it
        E_STAGE_DONE
    };

    struct BoxFrame
    {
        Vector3 mCentre;
        Vector3 mAxisX;
        Vector3 mAxisY;
        Vector3 mAxisZ;
        Vector3 mHalf;               // GetDimensions() * 0.5, unsigned exactly as the landmark test uses it
    };

    struct ProducerState
    {
        EStage   meStage;
        bool     mbSeenInProgress;
        f32      mfInProgressSeconds;
        s32      miFrame;
        s32      miLastIssueFrame;
        s32      miHopCount;
        s32      miTargetCheckpoint;
        u32      muTargetRemaining;
        s32      miFinishHops;
        // seat watch
        bool     mbSeatWatch;
        bool     mbSeatWatchIsFinish;
        s32      miSeatHop;
        s32      miSeatDeadline;
        BoxFrame mSeatBox;
        bool     mbHavePreviousPosition;
        Vector3  mPreviousPosition;
    };

    ProducerState gState = { E_STAGE_WAITING_DELAY, false, 0.0f, 0, 0, 0, -1, 0u, 0,
                             false, false, 0, 0, {}, false, { 0.0f, 0.0f, 0.0f, 0.0f } };

    // ------------------------------------------------------------------------
    // Small helpers.
    // ------------------------------------------------------------------------
    CgsDev::Log::DebugPrint* DebugLog()
    {
        return CgsDev::Log::gpDebugPrint;
    }

    f32 Dot3(const Vector3& lrA, const Vector3& lrB)
    {
        return lrA.x * lrB.x + lrA.y * lrB.y + lrA.z * lrB.z;
    }

    Vector3 Sub3(const Vector3& lrA, const Vector3& lrB)
    {
        return Vector3{ lrA.x - lrB.x, lrA.y - lrB.y, lrA.z - lrB.z, 0.0f };
    }

    // The horizontal part of a vector, normalised; false when it has (almost) none.
    bool FlattenNormalise(const Vector3& lrIn, Vector3* lpOut)
    {
        const f32 lfLength = std::sqrt(lrIn.x * lrIn.x + lrIn.z * lrIn.z);
        if (!(lfLength > KF_MIN_AXIS_LENGTH))
        {
            return false;
        }
        *lpOut = Vector3{ lrIn.x / lfLength, 0.0f, lrIn.z / lfLength, 0.0f };
        return true;
    }

    void PrintVector(CgsDev::StrStreamBase& lrLog, const Vector3& lrV)
    {
        lrLog << "(" << lrV.x << ", " << lrV.y << ", " << lrV.z << ")";
    }

    const char* StateName(s32 liState)
    {
        static const char* const KAPC_STATE_NAMES[BrnGameState::GameStateModuleIO::E_GMS_COUNT] =
        {
            "E_GMS_COUNTDOWN", "E_GMS_INTRO", "E_GMS_IN_PROGRESS", "E_GMS_OUTRO",
            "E_GMS_RESULTS",   "E_GMS_QUIT",  "E_GMS_ONLINE_LOADING", "E_GMS_ONLINE_SPLASH"
        };
        if (liState >= 0 && liState < BrnGameState::GameStateModuleIO::E_GMS_COUNT)
        {
            return KAPC_STATE_NAMES[liState];
        }
        return "(no mode)";
    }

    // Parse a float setting. Unset -> the default and true. Set but empty, not a number, or below
    // the minimum -> false (the caller refuses the whole capability).
    bool ParseFloat(const char* lpcName, f32 lfDefault, f32 lfMinimum, bool lbRequired, f32* lpfOut)
    {
        const char* lpcValue = std::getenv(lpcName);
        if (lpcValue == 0)
        {
            *lpfOut = lfDefault;
            return !lbRequired;
        }
        char* lpcEnd = 0;
        const double ldValue = std::strtod(lpcValue, &lpcEnd);
        if (lpcEnd == lpcValue)
        {
            return false;
        }
        while (*lpcEnd == ' ' || *lpcEnd == '\t')
        {
            ++lpcEnd;
        }
        if (*lpcEnd != '\0' || !(ldValue >= static_cast<double>(lfMinimum)))
        {
            return false;
        }
        *lpfOut = static_cast<f32>(ldValue);
        return true;
    }

    void Parse()
    {
        gbParsed  = true;
        gbEnabled = false;

        const char* lpcDelay = std::getenv("BRN_WIN_TELEPORT");
        if (lpcDelay == 0)
        {
            return;   // not requested: silent, a default run
        }

        f32 lfGap = static_cast<f32>(KI_DEFAULT_GAP_FRAMES);
        const char* lpcBad = 0;
        if (!ParseFloat("BRN_WIN_TELEPORT", 0.0f, 0.0f, true, &gfDelaySeconds))
        {
            lpcBad = "BRN_WIN_TELEPORT";
        }
        else if (!ParseFloat("BRN_WIN_TELEPORT_GAP", static_cast<f32>(KI_DEFAULT_GAP_FRAMES), 1.0f, false, &lfGap))
        {
            lpcBad = "BRN_WIN_TELEPORT_GAP";
        }
        else if (!ParseFloat("BRN_WIN_TELEPORT_SPEED", 0.0f, 0.0f, false, &gfSpeed))
        {
            lpcBad = "BRN_WIN_TELEPORT_SPEED";
        }
        else if (!ParseFloat("BRN_WIN_TELEPORT_LEAD", KF_DEFAULT_LEAD_METRES, 0.0f, false, &gfLeadMetres))
        {
            lpcBad = "BRN_WIN_TELEPORT_LEAD";
        }

        if (lpcBad != 0)
        {
            // [FLAG PC witness] the refusal. A run that asked for the teleport and silently did
            // not get it would be scored as "the finish never fired".
            if (DebugLog() != 0)
            {
                const char* lpcShown = std::getenv(lpcBad);
                *DebugLog() << "[win-teleport] FAIL: " << lpcBad << "=\"" << (lpcShown != 0 ? lpcShown : "")
                            << "\" is not a valid number (BRN_WIN_TELEPORT: seconds >= 0; _GAP: frames >= 1;"
                            << " _SPEED: m/s >= 0; _LEAD: metres >= 0) -- the win teleport is OFF\n";
            }
            return;
        }

        giGapFrames = static_cast<s32>(lfGap);
        gbEnabled   = true;

        // [FLAG PC witness] the arming line.
        if (DebugLog() != 0)
        {
            *DebugLog() << "[win-teleport] armed delay=" << gfDelaySeconds << "s gap=" << giGapFrames
                        << " frames speed=" << gfSpeed << " m/s lead=" << gfLeadMetres
                        << " m -- after that much IN_PROGRESS time the player's car is placed INTO each"
                        << " remaining landmark box through ActiveRaceCar::RequestPlaceOnTrack; the"
                        << " checkpoint and finish credit is the game's own landmark chain\n";
        }
    }

    // ------------------------------------------------------------------------
    // Reads.
    // ------------------------------------------------------------------------
    bool ReadPlayerPosition(const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActive,
                            Vector3* lpOut)
    {
        if (lpActive == 0 || !lpActive->IsPlayerCarActive())
        {
            return false;
        }
        *lpOut = lpActive->GetPlayerRaceCarState()->mTransform.wAxis;
        return true;
    }

    s32 ReadPlayerRacePosition(const BrnGameState::ModeManager& lrModeManager)
    {
        const EActiveRaceCarIndex lePlayer = lrModeManager.GetPlayerActiveRaceCarIndex();
        if (!(lePlayer > E_ACTIVE_RACE_CAR_INDEX_INVALID && lePlayer < E_ACTIVE_RACE_CAR_INDEX_COUNT))
        {
            return -1;
        }
        return static_cast<s32>(lrModeManager.GetScoringSystem()->GetCarRacePosition(lePlayer));
    }

    const BrnTrigger::Landmark* FindLandmark(const BrnGameState::ModeManager& lrModeManager, s32 liRegionIndex)
    {
        if (liRegionIndex < 0)
        {
            return 0;
        }
        const BrnTrigger::TriggerData* lpData = lrModeManager.GetCheckpointTriggerData();
        if (lpData == 0)
        {
            return 0;
        }
        return lpData->GetLandmarkFromRegionIndex(liRegionIndex);
    }

    BoxFrame MakeBoxFrame(const BrnTrigger::BoxRegion& lrBox)
    {
        const Matrix44Affine lTransform  = lrBox.ComputeTransform();
        const Vector3        lDimensions = lrBox.GetDimensions();
        BoxFrame lFrame;
        lFrame.mCentre = lTransform.wAxis;
        lFrame.mAxisX  = lTransform.xAxis;
        lFrame.mAxisY  = lTransform.yAxis;
        lFrame.mAxisZ  = lTransform.zAxis;
        lFrame.mHalf   = Vector3{ lDimensions.x * 0.5f, lDimensions.y * 0.5f, lDimensions.z * 0.5f, 0.0f };
        return lFrame;
    }

    // The landmark test's own inside predicate (min = -half, max = +half, per local axis).
    bool IsInsideBox(const BoxFrame& lrBox, const Vector3& lrPoint, Vector3* lpLocal)
    {
        const Vector3 lRelative = Sub3(lrPoint, lrBox.mCentre);
        *lpLocal = Vector3{ Dot3(lRelative, lrBox.mAxisX), Dot3(lRelative, lrBox.mAxisY),
                            Dot3(lRelative, lrBox.mAxisZ), 0.0f };
        const f32 lafLocal[3] = { lpLocal->x, lpLocal->y, lpLocal->z };
        const f32 lafHalf[3]  = { lrBox.mHalf.x, lrBox.mHalf.y, lrBox.mHalf.z };
        for (s32 liAxis = 0; liAxis < 3; ++liAxis)
        {
            if (!((lafLocal[liAxis] >= -lafHalf[liAxis]) && !(lafLocal[liAxis] > lafHalf[liAxis])))
            {
                return false;
            }
        }
        return true;
    }

    // The direction the car should travel through the box: the box's THIN horizontal axis (the
    // finish line's depth), signed so that it points from the reference (the previous landmark,
    // else the car) toward the box.
    Vector3 ApproachDirection(const BoxFrame& lrBox, bool lbHaveReference, const Vector3& lrReference)
    {
        Vector3 lAxisX;
        Vector3 lAxisZ;
        const bool lbHaveX = FlattenNormalise(lrBox.mAxisX, &lAxisX);
        const bool lbHaveZ = FlattenNormalise(lrBox.mAxisZ, &lAxisZ);

        Vector3 lToBox = Vector3{ 0.0f, 0.0f, 1.0f, 0.0f };
        const bool lbHaveToBox =
            lbHaveReference && FlattenNormalise(Sub3(lrBox.mCentre, lrReference), &lToBox);

        Vector3 lAxis;
        if (lbHaveX && lbHaveZ)
        {
            lAxis = (std::fabs(lrBox.mHalf.z) <= std::fabs(lrBox.mHalf.x)) ? lAxisZ : lAxisX;
        }
        else if (lbHaveZ)
        {
            lAxis = lAxisZ;
        }
        else if (lbHaveX)
        {
            lAxis = lAxisX;
        }
        else
        {
            return lToBox;   // a box with no horizontal extent: head straight at it
        }

        if (lbHaveToBox && Dot3(lAxis, lToBox) < 0.0f)
        {
            lAxis = Vector3{ -lAxis.x, 0.0f, -lAxis.z, 0.0f };
        }
        return lAxis;
    }

    // ------------------------------------------------------------------------
    // Issue one request and print its hop line.
    // ------------------------------------------------------------------------
    void Post(const Vector3& lrPosition, const Vector3& lrDirection, f32 lfSpeed)
    {
        gRequest.mPosition  = lrPosition;
        gRequest.mDirection = lrDirection;
        gRequest.mfSpeed    = lfSpeed;
        const u32 luSeq = gauRequestSeq.load(std::memory_order_relaxed) + 1u;
        gRequest.muSeq = luSeq;
        gauRequestSeq.store(luSeq, std::memory_order_release);
    }

    // kind: "stage" (offline race, outside the finish box, facing away) or "finish" (into the box).
    void IssueHop(ProducerState& lrState, const BrnGameState::ModeManager& lrModeManager,
                  const BrnTrigger::Landmark& lrLandmark, s32 liCheckpoint, s32 liRegionIndex,
                  u32 luRemaining, bool lbStage, const char* lpcWhy,
                  bool lbHaveReference, const Vector3& lrReference, s32 liRacePosition)
    {
        const BoxFrame lBox       = MakeBoxFrame(*lrLandmark.GetBoxRegion());
        const Vector3  lApproach  = ApproachDirection(lBox, lbHaveReference, lrReference);

        Vector3 lTarget    = lBox.mCentre;
        Vector3 lDirection = lApproach;
        if (lbStage)
        {
            lTarget    = Vector3{ lBox.mCentre.x - lApproach.x * gfLeadMetres, lBox.mCentre.y,
                                  lBox.mCentre.z - lApproach.z * gfLeadMetres, 0.0f };
            lDirection = Vector3{ -lApproach.x, 0.0f, -lApproach.z, 0.0f };
        }

        Post(lTarget, lDirection, gfSpeed);

        ++lrState.miHopCount;
        lrState.miLastIssueFrame = lrState.miFrame;

        lrState.mbSeatWatch          = true;
        lrState.mbSeatWatchIsFinish  = !lbStage;
        lrState.miSeatHop            = lrState.miHopCount;
        lrState.miSeatDeadline       = lrState.miFrame + giGapFrames;
        lrState.mSeatBox             = lBox;

        // [FLAG PC witness] the hop line the harness cases gate on.
        if (DebugLog() != 0)
        {
            CgsDev::StrStreamBase& lrLog = *DebugLog();
            lrLog << "[win-teleport] hop " << lrState.miHopCount << " landmark=" << liCheckpoint
                  << " region=" << liRegionIndex << " id=" << static_cast<u64>(lrLandmark.GetId()) << " -> ";
            PrintVector(lrLog, lTarget);
            lrLog << " dir=";
            PrintVector(lrLog, lDirection);
            lrLog << " remaining=" << luRemaining << " kind=" << (lbStage ? "stage" : "finish")
                  << " speed=" << gfSpeed << " racePos=" << liRacePosition << " why=" << lpcWhy
                  << " mode=" << static_cast<s32>(lrModeManager.GetCurrentGameModeType()) << " box=";
            PrintVector(lrLog, lBox.mCentre);
            lrLog << " half=";
            PrintVector(lrLog, lBox.mHalf);
            if (lbStage)
            {
                lrLog << " (" << gfLeadMetres << " m outside the box on the approach side, facing away:"
                      << " the finish hop waits for racePos 1)";
            }
            lrLog << "\n";
        }
    }

    void PrintSeat(ProducerState& lrState, const Vector3& lrPosition, bool lbSeen)
    {
        lrState.mbSeatWatch = false;
        if (DebugLog() == 0)
        {
            return;
        }
        Vector3 lLocal;
        const bool lbInside = IsInsideBox(lrState.mSeatBox, lrPosition, &lLocal);
        CgsDev::StrStreamBase& lrLog = *DebugLog();
        lrLog << "[win-teleport] seat hop " << lrState.miSeatHop
              << " kind=" << (lrState.mbSeatWatchIsFinish ? "finish" : "stage")
              << (lbSeen ? " landed" : " NOT SEEN (no placement jump before the deadline; current position)")
              << " car=";
        PrintVector(lrLog, lrPosition);
        lrLog << " local=";
        PrintVector(lrLog, lLocal);
        lrLog << " half=";
        PrintVector(lrLog, lrState.mSeatBox.mHalf);
        lrLog << " inside=" << (lbInside ? 1 : 0) << "\n";
    }

    void UpdateSeatWatch(ProducerState& lrState, bool lbHavePosition, const Vector3& lrPosition)
    {
        if (lrState.mbSeatWatch && lbHavePosition)
        {
            bool lbJumped = false;
            if (lrState.mbHavePreviousPosition)
            {
                const Vector3 lStep = Sub3(lrPosition, lrState.mPreviousPosition);
                lbJumped = !(Dot3(lStep, lStep) < KF_PLACEMENT_JUMP_METRES * KF_PLACEMENT_JUMP_METRES);
            }
            if (lbJumped)
            {
                PrintSeat(lrState, lrPosition, true);
            }
            else if (lrState.miFrame >= lrState.miSeatDeadline)
            {
                PrintSeat(lrState, lrPosition, false);
            }
        }
        lrState.mbHavePreviousPosition = lbHavePosition;
        if (lbHavePosition)
        {
            lrState.mPreviousPosition = lrPosition;
        }
    }

    void Finish(ProducerState& lrState, const BrnGameState::ModeManager& lrModeManager, s32 liState,
                bool lbHavePosition, const Vector3& lrPosition, u32 luRemaining)
    {
        if (lrState.mbSeatWatch && lbHavePosition)
        {
            PrintSeat(lrState, lrPosition, false);
        }
        lrState.meStage = E_STAGE_DONE;
        // [FLAG PC witness] the closing line.
        if (DebugLog() != 0)
        {
            *DebugLog() << "[win-teleport] done state=" << StateName(liState)
                        << " mode=" << static_cast<s32>(lrModeManager.GetCurrentGameModeType())
                        << " hops=" << lrState.miHopCount << " racePos=" << ReadPlayerRacePosition(lrModeManager)
                        << " remaining=" << luRemaining << "\n";
        }
    }

    void Fail(ProducerState& lrState, const char* lpcWhy, s32 liCheckpoint, s32 liRegionIndex)
    {
        lrState.meStage = E_STAGE_DONE;
        // [FLAG PC witness] a give-up, never silent.
        if (DebugLog() != 0)
        {
            *DebugLog() << "[win-teleport] FAIL: " << lpcWhy << " (landmark=" << liCheckpoint
                        << " region=" << liRegionIndex << " hops=" << lrState.miHopCount
                        << ") -- no further hops\n";
        }
    }
}

bool IsEnabled()
{
    if (!gbParsed)
    {
        Parse();
    }
    return gbEnabled;
}

void PreWorldUpdate(const BrnGameState::ModeManager& lrModeManager,
                    EGlobalRaceCarIndex lePlayerGlobalRaceCarIndex,
                    s32 liNextRegionIndex,
                    s32 liPreviousRegionIndex,
                    f32 lfSimTimeStep,
                    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutput)
{
    ProducerState& lrState = gState;
    if (!IsEnabled() || lrState.meStage == E_STAGE_DONE)
    {
        return;
    }
    ++lrState.miFrame;

    // UpdateCurrentMode can tear the mode down earlier in the same tick.
    const BrnGameState::GameMode* lpMode = lrModeManager.GetCurrentGameMode();
    const s32 liState = (lpMode != 0) ? lpMode->GetCurrentState() : -1;

    Vector3 lPlayerPosition = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
    const bool lbHavePosition = ReadPlayerPosition(lpActiveRaceCarOutput, &lPlayerPosition);

    // Before any state gate, so the seat of the hop that ends IN_PROGRESS is still reported.
    UpdateSeatWatch(lrState, lbHavePosition, lPlayerPosition);

    const u32 luRemaining = lrModeManager.CountCheckpointsRemaining(lePlayerGlobalRaceCarIndex);

    if (liState != BrnGameState::GameStateModuleIO::E_GMS_IN_PROGRESS)
    {
        if (lrState.mbSeenInProgress)
        {
            Finish(lrState, lrModeManager, liState, lbHavePosition, lPlayerPosition, luRemaining);
        }
        return;
    }
    lrState.mbSeenInProgress = true;
    lrState.mfInProgressSeconds += lfSimTimeStep;

    if (lrState.meStage == E_STAGE_WAITING_DELAY)
    {
        if (lrState.mfInProgressSeconds < gfDelaySeconds)
        {
            return;
        }
        lrState.meStage          = E_STAGE_READY;
        lrState.miLastIssueFrame = lrState.miFrame - giGapFrames;   // the first hop goes now
    }

    // GetNextLandmarkIndex asserts when nothing remains, so it is only asked while something does.
    const s32 liNextCheckpoint =
        (luRemaining > 0u) ? lrModeManager.GetNextLandmarkIndex(lePlayerGlobalRaceCarIndex) : -1;

    // Did the game credit the checkpoint the last request was aimed at?
    if ((lrState.meStage == E_STAGE_STAGED || lrState.meStage == E_STAGE_HOPPED) &&
        (liNextCheckpoint != lrState.miTargetCheckpoint || luRemaining != lrState.muTargetRemaining))
    {
        // [FLAG PC witness]
        if (DebugLog() != 0)
        {
            *DebugLog() << "[win-teleport] credited landmark=" << lrState.miTargetCheckpoint
                        << " remaining=" << luRemaining << " racePos=" << ReadPlayerRacePosition(lrModeManager)
                        << " after hop " << lrState.miHopCount << "\n";
        }
        lrState.meStage = E_STAGE_READY;
    }

    if (luRemaining == 0u)
    {
        return;   // the finish is credited; the mode leaves IN_PROGRESS on its own
    }

    const s32 liFramesSinceIssue = lrState.miFrame - lrState.miLastIssueFrame;
    if (liFramesSinceIssue < giGapFrames)
    {
        return;
    }

    const BrnTrigger::Landmark* lpLandmark = FindLandmark(lrModeManager, liNextRegionIndex);
    if (lpLandmark == 0 || lpLandmark->GetBoxRegion() == 0)
    {
        Fail(lrState, "the next checkpoint's landmark region does not resolve", liNextCheckpoint, liNextRegionIndex);
        return;
    }

    // Approach reference: the previous landmark's centre, else the car.
    Vector3 lReference = lPlayerPosition;
    bool    lbHaveReference = lbHavePosition;
    const BrnTrigger::Landmark* lpPrevious = FindLandmark(lrModeManager, liPreviousRegionIndex);
    if (lpPrevious != 0 && lpPrevious->GetBoxRegion() != 0)
    {
        lReference      = lpPrevious->GetBoxRegion()->GetPosition();
        lbHaveReference = true;
    }

    const s32  liRacePosition = ReadPlayerRacePosition(lrModeManager);
    const bool lbStageFirst =
        (lrModeManager.GetCurrentGameModeType() == BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_RACE) &&
        (luRemaining == 1u) && (gfLeadMetres > 0.0f);

    switch (lrState.meStage)
    {
        case E_STAGE_READY:
            lrState.miTargetCheckpoint = liNextCheckpoint;
            lrState.muTargetRemaining  = luRemaining;
            if (lbStageFirst)
            {
                lrState.miFinishHops = 0;
                lrState.meStage      = E_STAGE_STAGED;
                IssueHop(lrState, lrModeManager, *lpLandmark, liNextCheckpoint, liNextRegionIndex,
                         luRemaining, true, "stage", lbHaveReference, lReference, liRacePosition);
            }
            else
            {
                lrState.miFinishHops = 1;
                lrState.meStage      = E_STAGE_HOPPED;
                IssueHop(lrState, lrModeManager, *lpLandmark, liNextCheckpoint, liNextRegionIndex,
                         luRemaining, false, "direct", lbHaveReference, lReference, liRacePosition);
            }
            break;

        case E_STAGE_STAGED:
            if (liRacePosition == 1 || liFramesSinceIssue >= KI_LEADER_WAIT_CAP_FRAMES)
            {
                lrState.miFinishHops = 1;
                lrState.meStage      = E_STAGE_HOPPED;
                IssueHop(lrState, lrModeManager, *lpLandmark, liNextCheckpoint, liNextRegionIndex,
                         luRemaining, false, (liRacePosition == 1) ? "leader" : "wait-cap",
                         lbHaveReference, lReference, liRacePosition);
            }
            break;

        case E_STAGE_HOPPED:
            if (lrState.miFinishHops >= KI_MAX_FINISH_HOPS_PER_CHECKPOINT)
            {
                Fail(lrState, "the landmark was not credited after the maximum number of finish hops",
                     liNextCheckpoint, liNextRegionIndex);
                break;
            }
            ++lrState.miFinishHops;
            IssueHop(lrState, lrModeManager, *lpLandmark, liNextCheckpoint, liNextRegionIndex,
                     luRemaining, false, "retry", lbHaveReference, lReference, liRacePosition);
            break;

        default:
            break;
    }
}

bool PeekRequest(u32 luLastSeq, Request* lpRequest)
{
    const u32 luSeq = gauRequestSeq.load(std::memory_order_acquire);
    if (luSeq == 0u || luSeq == luLastSeq)
    {
        return false;
    }
    *lpRequest       = gRequest;
    lpRequest->muSeq = luSeq;
    return true;
}
}
}
