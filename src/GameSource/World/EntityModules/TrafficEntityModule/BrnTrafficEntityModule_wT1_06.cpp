// ============================================================================
// BrnTrafficEntityModule_wT1_06.cpp -- the traffic steady-state loop.
//
//   TrafficEntityModule::UpdateTimers           @0x82715858  (DWARF :1287)
//   TrafficEntityModule::UpdateDecisionFrame    @0x8274E508  (DWARF :1476)  PARTIAL
//   TrafficEntityModule::UpdateNonDecisionFrame @0x8274C1A8  (DWARF :1479)  PARTIAL
//
// PostPhysicsUpdate's E_STATE_RUNNING arm @0x8274E6D0 calls none of the spawn ladder itself.
// It is a two-way dispatch: paused does nothing, IsDecisionFrame() picks UpdateDecisionFrame,
// otherwise UpdateNonDecisionFrame. The spawn ladder lives in UpdateDecisionFrame.
//
// Outside E_STATE_STARTING_UP, IsDecisionFrame() returns mbDecisionFrame, whose only writer in
// the whole image is UpdateTimers here (the +0x713F5 store), itself called only from
// PreSceneUpdate's E_STATE_RUNNING arm. Reset() seeds it false, so without UpdateTimers every
// frame takes the non-decision branch for ever. That is why the three land together and why
// _wT1_02.cpp un-gates the UpdateTimers call in the same change.
//
// The ten gated legs below all lack a body anywhere in the tree and are all driving-traffic
// work. A parked car comes from RecalculateActiveHulls -> SpawnNewTraffic -> FillNewHull ->
// StaticVehicles_Generate, then StaticVehicles_CreateNewVehicles via
// StaticVehicles_UpdateVehicles; none of the ten is on that path. The one with a real cost is
// KillOutOfAreaTraffic; see its gate in UpdateDecisionFrame.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"

#include <cstdlib>   // getenv

namespace BrnTraffic
{
namespace
{
    // NAMED LEG GATE + diag plumbing, same shape as the sibling partfiles', file-local by
    // convention. [DIAG] NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
    inline void LogMissingLeg(bool& lrbAlreadyLogged, const char* lpcLegNameAndReason)
    {
        if (lrbAlreadyLogged)
        {
            return;
        }
        lrbAlreadyLogged = true;

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[T1-traffic-leg] TrafficEntityModule leg NOT RECONSTRUCTED, skipped: "
                << lpcLegNameAndReason << " [FLAG PC partial gate]\n";
        }
    }

    bool TrafficDiagEnabled()
    {
        static const bool sbEnabled = (getenv("BRN_TRAFFIC_DIAG") != 0);
        return sbEnabled;
    }

    CgsDev::Log::DebugPrint* TrafficDiagStream()
    {
        if (!TrafficDiagEnabled() || CgsDev::Log::gpDebugPrint == 0)
        {
            return 0;
        }
        return CgsDev::Log::gpDebugPrint;
    }

    // The console's own .rdata literals. IsSimTimerFrequency50Hz() is the committed
    // `GetCurrentTimeStep() == 0.02f`, the same compare the console inlines at 0x82715988.
    const f32 KF_SIM_TIMESTEP_SQ_SCALE            = 360.0f;   // flt_820BA570
    const f32 KF_ONLINE_SIM_TIME_SINCE_DECISION   = 0.1f;     // flt_82004014

    // `cmplwi r11, 5` / `cmplwi r11, 6` at 0x827159A4 / 0x827159AC -- a decision frame every
    // 5 frames at 50 Hz, every 6 at 60 Hz (both == 0.1 s).
    const u32 KU_FRAMES_PER_DECISION_50HZ = 5;
    const u32 KU_FRAMES_PER_DECISION_60HZ = 6;

    // 0x8274E694 `cmplwi r11, 0x64` is KU_START_PROTECT_UPDATE_FRAME_ONLINE == 100, already in
    // BrnTrafficConstants.h:153; the canonical one is used below.
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateTimers  @ 0x82715858   (.cpp 1922)
//
// The frame clock for the whole traffic sim. Offsets, since the C++ reaches these by name:
//   +0x713F4  muFramesSinceDecision   +0x713F5  mbDecisionFrame
//   +0x713F8  mfSimTimeSinceLastDecision
//   +0x713FC  mfSimTimeStep           +0x71400  mfSimTimeStepMultiplier
//   +0x71410  mfSimTimeStepVec        +0x717E7  mbAllowDivergentBehaviour
//
// ORDER IS LOAD-BEARING. mbDecisionFrame is cleared BEFORE the counter test and set again only
// when the counter reaches its limit, so a frame is a decision frame for exactly one visit.
// Clearing it after the test makes every frame a decision frame; clearing it in the else arm
// leaves it latched high.
//
// The first frame is a decision frame: Reset() seeds muFramesSinceDecision = 100, so the first
// increment gives 101, over both the 5- and 6-frame limits. The module does not wait 0.1 s for
// its first RUNNING recalculation, so a POPULATING-time hull miss self-heals.
//
// The console calls GetTimerStatusInterface four times (0x82715894, 0x827158B8, 0x82715968,
// 0x827159C0) and recomputes the same product each time. That is the compiler rematerialising
// an inlined accessor, not four buffers; de-inlined to one local, and the getter's read-lock
// assert is idempotent.
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdateTimers(const BrnTrafficIO::InputBuffer_PreScene* lpInput)
{
    if (IsDecisionFrame())
    {
        mfSimTimeSinceLastDecision = 0.0f;   // flt_82001CC0 == 0.0f
    }

    const CgsSystem::TimerStatusInterface* lpTimerStatus = lpInput->GetTimerStatusInterface();
    const CgsSystem::TimerStatus* lpSimTimer = lpTimerStatus->GetSimTimerStatus();

    // `lfs 0x20(r11)` * `lfs 0x1C(r11)` == the SIM block's multiplier * base step: sim block
    // at interface+24, mfTimeStepMultiplier at +8 (== +32) and mfBaseTimeStep at +4 (== +28).
    mfSimTimeStep           = lpSimTimer->GetCurrentTimeStep();
    mfSimTimeStepMultiplier = lpSimTimer->GetTimeStepMultiplier();

    // 0x8272158F0..0x82715924 builds a 4-lane splat of mfSimTimeStep through the stack
    // (`lvx128` + `vspltw ,0`); the zero-word staging is just how the console gets a scalar
    // into a lane.
    mfSimTimeStepVec.x = mfSimTimeStep;
    mfSimTimeStepVec.y = mfSimTimeStep;
    mfSimTimeStepVec.z = mfSimTimeStep;
    mfSimTimeStepVec.w = mfSimTimeStep;

    {
        // GATE -- the store at 0x82715928..0x82715954, `splat(mfSimTimeStep * mfSimTimeStep *
        // 360.0f)` into a VecFloat member at X360 +0x72700. That offset falls in the DecFIGS
        // un-emitted :822..:833 window (between :821 and mpDebugComponent :834 @+0x727B0), so
        // the member has no attested name and inventing one would fake a keystone layout.
        // Write-only on this build: an export-wide scan for 468736/0x72700 finds only this
        // function, so no consumer is starved.
        // DELETE WHEN: DWARF :822..:833 is named. The expression above lands in one line then.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "UpdateTimers' splat(mfSimTimeStep^2 * 360.0f) store to the VecFloat at X360 "
            "+0x72700 -- that offset lies in the DecFIGS UN-EMITTED :822..:833 window (between "
            "kfParamAvoidCrashCone... :821 and mpDebugComponent :834 @+0x727B0), so the member "
            "has no attested name. WRITE-ONLY on this build: an export-wide scan for "
            "468736/0x72700 finds this function and nothing else, so no consumer is starved");
        (void)KF_SIM_TIMESTEP_SQ_SCALE;
    }

    // 0x82715958..0x82715964: read the counter, clear the flag, store counter+1. The clear
    // sits BETWEEN the read and the store in the console too.
    muFramesSinceDecision = static_cast<u8>(muFramesSinceDecision + 1);
    mbDecisionFrame       = false;

    const u32 luFramesPerDecision = lpTimerStatus->IsSimTimerFrequency50Hz()
                                        ? KU_FRAMES_PER_DECISION_50HZ
                                        : KU_FRAMES_PER_DECISION_60HZ;

    if (muFramesSinceDecision >= luFramesPerDecision)
    {
        mbDecisionFrame       = true;
        muFramesSinceDecision = 0;

        if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
        {
            // [T1-loop] one-shot: the first time the clock declares a decision frame. If this
            // never prints, the loop is dead. DELETE-WHEN-STABLE.
            static bool sbFirst = true;
            if (sbFirst)
            {
                sbFirst = false;
                *lpDiag << "[T1-loop] FIRST decision frame (dt=" << mfSimTimeStep
                        << ", period=" << static_cast<s32>(luFramesPerDecision)
                        << " frames, meState=" << static_cast<s32>(meState) << ")\n";
            }
        }
    }

    // `fmadds f0, f0, f13, f12` -- the accumulate uses the timer's current step again, not
    // the member just stored. Same value; transcribed as the console spells it.
    mfSimTimeSinceLastDecision += lpSimTimer->GetCurrentTimeStep();

    if (!mbAllowDivergentBehaviour)
    {
        // Online-only: offline mbAllowDivergentBehaviour is always true (EnterStartingUpState
        // sets it from !mbIsOnlineGameMode), so this arm is dead on this build.
        if (IsDecisionFrame())
        {
            mfSimTimeSinceLastDecision = KF_ONLINE_SIM_TIME_SINCE_DECISION;
        }
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateDecisionFrame  @ 0x8274E508   PARTIAL   (.cpp 7046)
//
// The steady-state spawn loop: everything the POPULATING arm does once, repeated every
// decision frame (5 or 6 render frames, 10 Hz) while the module is RUNNING.
//
// ARGUMENT ORDER. RecalculateActiveHulls fills lNewActiveHulls (r6, 4th arg) and
// lOldActiveHulls (r7, 5th). KillOutOfAreaTraffic takes the OLD set (0x8274E590 `addi r4, r1,
// var_170`) and SpawnNewTraffic the NEW one (0x8274E5C0 `addi r4, r1, var_D0`). Crossing them
// kills the hulls that just arrived and fills the ones that just left.
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdateDecisionFrame(
    const BrnTrafficIO::InputBuffer_PostPhysics* lpInput,
    BrnTrafficIO::OutputBuffer_PostPhysics* lpOutput)
{
    CGS_ASSERT(IsDecisionFrame(), "IsDecisionFrame()");   // baked .cpp 7150

    // 0x8274E564: `lhz / addi 1 / sth` on the u16 at +0x71B30, read back by the start-protect
    // arm below.
    ++muUpdateCount;

    {
        // GATE: the PerfMonCpu Start/StopMonitor brackets (+0x72A40 spawn half, +0x72A44
        // update half). The handles are never issued because Construct's twenty AddMonitor
        // registrations are gated. DELETE WHEN those registrations land.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "UpdateDecisionFrame PerfMonCpu Start/StopMonitor brackets (+0x72A40 spawn half, "
            "+0x72A44 update half) -- the handles are never issued because Construct's twenty "
            "AddMonitor registrations are gated; same disposition as PreSceneUpdate's");
    }

    ActiveHullSet lNewActiveHulls;
    ActiveHullSet lOldActiveHulls;
    lNewActiveHulls.Construct();
    lOldActiveHulls.Construct();

    RecalculateActiveHulls(lpInput, lpOutput, &lNewActiveHulls, &lOldActiveHulls);

    if (lOldActiveHulls.GetLength() != 0)
    {
        // GATE with a real behavioural cost: KillOutOfAreaTraffic (DWARF :1539
        // `void KillOutOfAreaTraffic(Set<uint16_t,72u>*)`) has no body. It is the retire half
        // of the spawn ladder, so static params for out-of-range hulls are never handed back
        // to mFreeStaticParamStack: the 199-slot pool drains monotonically and parked cars
        // stop appearing after a long drive. Bounded and assert-free, since
        // StaticVehicles_Generate no-ops on an empty stack, and visible in [T1-static]
        // aliveParams. Do not paper over it with a host-side recycler; that would invent a
        // retirement policy the binary does not have. DELETE WHEN the body lands.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "UpdateDecisionFrame leg KillOutOfAreaTraffic (DWARF :1539) -- no body in this "
            "tree; it is the RETIRE half of the spawn ladder and covers driving params too. "
            "COST: static params for out-of-range hulls are never freed, so the 199-slot pool "
            "drains over a long drive and parked cars stop appearing. Bounded, assert-free, "
            "visible in [T1-static] aliveParams");
    }

    {
        // GATE: KillTrafficOnStartGridWholeSale (DWARF :1794, takes a Vector3), no body. Its
        // argument is the player position via sub_823102F0 on the post-physics active-race-car
        // interface. Event-start-only; a free drive has no start grid to clear.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "UpdateDecisionFrame leg KillTrafficOnStartGridWholeSale (DWARF :1794) -- no body; "
            "event-start-only (it clears traffic off the race start grid). Its Vector3 "
            "argument is the player position via sub_823102F0 on the post-physics active "
            "race-car interface");
    }

    SpawnNewTraffic(lNewActiveHulls);

    if (mbPlayingShowtimeMode)
    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "UpdateDecisionFrame leg SpawnShowtimeTraffic (DWARF :1566) -- no body. "
            "SHOWTIME-only; mbPlayingShowtimeMode is false on a normal boot (ResetEventData)");
    }

    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "UpdateDecisionFrame leg UpdateJunctions (DWARF :1617) -- no body; junction "
            "give-way/priority logic for DRIVING traffic (wave 2)");
    }

    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "UpdateDecisionFrame leg UpdateParams (DWARF :1626) -- no body; it is the whole "
            "lane-param simulation for DRIVING traffic and needs [MEMBER HOLE 1] "
            "ParamNeedToSlowData / [MEMBER HOLE 2] ParamListNode (wave 2)");
    }

    // The parked param lifecycle: purgatory tick plus the kill/remove sweep.
    StaticVehicles_UpdateStaticParams();

    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "UpdateDecisionFrame leg UpdateVehicles (DWARF :1713) -- no body; the DRIVING "
            "vehicles' per-decision-frame update (steering/avoidance/effects), wave 2. Note "
            "its PARKED counterpart StaticVehicles_UpdateVehicles IS called below");
    }

    // This is what makes parked cars exist in steady state: its first statement is
    // StaticVehicles_CreateNewVehicles(lpInput), which turns an alive static PARAM into an
    // alive static VEHICLE via Vehicle::InitialiseAsStatic.
    StaticVehicles_UpdateVehicles(lpInput);

    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "UpdateDecisionFrame leg UpdateTrailers (DWARF :1725) -- no body; the single "
            "trailer slot follows its cab, which only exists once driving traffic does "
            "(wave 2)");
    }

    // 0x8274E648..0x8274E674, in the console's order: clear the time-slice cursor, then raise
    // the jam-nuker request that UpdateNonDecisionFrame consumes on a later frame.
    muLastParamCalculated       = 0;      // stwx 0 -> +0x71830
    mbNeedToRunTrafficJamNuker  = true;   // stbx 1 -> +0x71404

    if (mbAtStartLineSoProtectRaceCarsFromTraffic && mbIsOnlineGameMode)
    {
        // 0x8274E67C..0x8274E6BC. Online-only start-line protection: it is dropped after
        // KU_START_PROTECT_UPDATE_FRAME_ONLINE decision frames, and the console asserts the
        // count lands on that frame exactly, since `>` would mean a frame was missed.
        if (muUpdateCount >= KU_START_PROTECT_UPDATE_FRAME_ONLINE)
        {
            CGS_ASSERT(muUpdateCount == KU_START_PROTECT_UPDATE_FRAME_ONLINE,
                       "KU_START_PROTECT_UPDATE_FRAME_ONLINE == muUpdateCount");   // .cpp 7209
            mbAtStartLineSoProtectRaceCarsFromTraffic = false;
        }
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateNonDecisionFrame  @ 0x8274C1A8   PARTIAL   (.cpp 7124)
//
// The other four frames in five: no hull recalculation and no spawning, just per-frame
// movement of what exists plus a slice of the param logic.
//
// The parked leg runs on every frame, not only decision frames. StaticVehicles_UpdateVehicles
// appears in both functions, so StaticVehicles_CreateNewVehicles is attempted every frame and
// a param generated on a decision frame becomes a live vehicle on the next frame rather than
// 100 ms later.
//
// The time-slice cursor is not advanced here. The console reads muLastParamCalculated and
// passes [cursor, cursor+100) to UpdateParams_DoTimeSlicedLogic, which is what advances it.
// With that callee gated the cursor stays 0; advancing it locally would fake progress through
// a pass that never ran.
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdateNonDecisionFrame(
    const BrnTrafficIO::InputBuffer_PostPhysics* lpInput,
    BrnTrafficIO::OutputBuffer_PostPhysics* lpOutput)
{
    (void)lpOutput;   // only read by the two GATED driving legs below

    CGS_ASSERT(!IsDecisionFrame(), "!IsDecisionFrame()");   // baked .cpp 7228
    CGS_ASSERT(lpInput != 0, "lpInput");                    // baked .cpp 7229

    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "UpdateNonDecisionFrame leg UpdateLerpedParamTransforms (DWARF :1710) -- no body; "
            "it interpolates DRIVING params between decision frames. A parked car's transform "
            "never changes, so nothing on the parked path reads its output");
    }

    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "UpdateNonDecisionFrame leg UpdateVehicles (DWARF :1713) -- no body; same "
            "driving-traffic function UpdateDecisionFrame gates (wave 2)");
    }

    StaticVehicles_UpdateVehicles(lpInput);

    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "UpdateNonDecisionFrame leg UpdateTrailers (DWARF :1725) -- no body (wave 2)");
    }

    if (muLastParamCalculated < KU_MAX_PARAMS)
    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "UpdateNonDecisionFrame leg UpdateParams_DoTimeSlicedLogic (DWARF :1635, X360 "
            "@0x82743FE8 -- an ARTIST EXPORT HOLE) -- no body. It is the DRIVING-param "
            "time-slicer; the cursor muLastParamCalculated is deliberately NOT advanced here "
            "because the callee is what advances it");
    }

    if (mbNeedToRunTrafficJamNuker && !mbNeedToKillAllZombies)
    {
        // GATE: NukeTrafficJams (DWARF :1551), no body. The flag clear is gated with it, since
        // clearing a request whose work never ran would mark it serviced. Nothing else reads
        // mbNeedToRunTrafficJamNuker, so leaving it latched high is inert.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "UpdateNonDecisionFrame leg NukeTrafficJams (DWARF :1551) -- no body; the "
            "mbNeedToRunTrafficJamNuker clear is gated WITH it so a request whose work never "
            "ran is not marked serviced. Jams are a DRIVING-traffic condition (wave 2)");
    }
}

}
