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
// Live legs: KillOutOfAreaTraffic, SpawnNewTraffic, SpawnShowtimeTraffic (_wT1_07.cpp),
// UpdateParams, UpdateVehicles, UpdateLerpedParamTransforms and
// UpdateParams_DoTimeSlicedLogic. Still gated: UpdateJunctions, UpdateTrailers,
// KillTrafficOnStartGridWholeSale, NukeTrafficJams; each gate names its own blocker and cost.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"


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
        // The retire half of the spawn ladder (DWARF :1539, @0x82734C78): it takes the OLD
        // hull set, not the new one.
        KillOutOfAreaTraffic(&lOldActiveHulls);
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
        // The showtime top-up (DWARF :1566, @0x82743038) -- LANDED, body in _wT1_07.cpp.
        // It is a SECOND spawn source layered on SpawnNewTraffic above, refilling every
        // active section at a fixed 20 vpm scaled by mfShowtimeTrafficDensityScale instead
        // of the section's authored rate. Still inert on a normal boot: mbPlayingShowtimeMode
        // is seeded false by ResetEventData and the console's only writers of it are
        // HandlePrepareForModeAction @0x827480D8 (unreconstructed) and UpdateCrashSlider
        // @0x82715A18's mbDEBUGFakeShowtime arm (_wT5_01.cpp, LIVE).
        SpawnShowtimeTraffic();
    }

    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "UpdateDecisionFrame leg UpdateJunctions (DWARF :1617) -- no body; junction "
            "give-way/priority logic for DRIVING traffic (wave 2)");
    }

    // The whole lane-param simulation for DRIVING traffic (DWARF :1626, @0x82744A80). It
    // forwards lpInput to UpdateParams_BuildListOfCrashingThings, which asserts on it.
    UpdateParams(lpInput);

    // The parked param lifecycle: purgatory tick plus the kill/remove sweep.
    StaticVehicles_UpdateStaticParams();

    // The DRIVING vehicles' update (DWARF :1713, @0x82744F58). Its first statement is
    // UpdateVehicles_CreateNewVehicles, which turns an alive lane PARAM into an alive standard
    // VEHICLE via Vehicle::InitialiseAsStandard. Its PARKED counterpart
    // StaticVehicles_UpdateVehicles is called below.
    UpdateVehicles(lpInput, lpOutput);

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
    CGS_ASSERT(!IsDecisionFrame(), "!IsDecisionFrame()");   // baked .cpp 7228
    CGS_ASSERT(lpInput != 0, "lpInput");                    // baked .cpp 7229

    // Interpolates DRIVING params between decision frames (DWARF :1710, @0x82739CD8).
    UpdateLerpedParamTransforms();

    UpdateVehicles(lpInput, lpOutput);

    StaticVehicles_UpdateVehicles(lpInput);

    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "UpdateNonDecisionFrame leg UpdateTrailers (DWARF :1725) -- no body (wave 2)");
    }

    // 0x8274C2A4: [cursor, cursor + KU_MAX_PARAMS_UPDATE_ON_NON_DECISION_FRAME); the callee
    // advances the cursor. Body in _wT2_05.cpp.
    if (muLastParamCalculated < KU_MAX_PARAMS)
    {
        UpdateParams_DoTimeSlicedLogic(
            muLastParamCalculated,
            muLastParamCalculated + KU_MAX_PARAMS_UPDATE_ON_NON_DECISION_FRAME,
            lpInput->GetActiveRaceCarOutputInterface());
    }

    if (mbNeedToRunTrafficJamNuker && !mbNeedToKillAllZombies)
    {
        // GATE: NukeTrafficJams (DWARF :1551, @0x827353E8), no body. The flag clear is gated
        // with it, since clearing a request whose work never ran would mark it serviced.
        // Nothing else reads mbNeedToRunTrafficJamNuker, so leaving it latched high is inert.
        //
        // ⭐⭐ WHAT IT IS, read out of the asm 2026-08-29 so the next wave does not re-derive it.
        // This IS the console's jam relief valve, and it is the missing half of a chain whose
        // other half is ALREADY LIVE in this tree:
        //   * it walks runs of consecutive params whose Param::mfSpeed (+0x14) is below
        //     5.0f m/s (flt_8200426C, compared at 0x82735A5C and 0x82735CE4), collecting them
        //     into a stack Set<u16,64> and stopping when that set is full;
        //   * for each collected param it reads mbAllowDivergentBehaviour (+0x717E7,
        //     0x82735F5C). OFFLINE (divergent) it projects the param through
        //     ParamTransform::GetLerpedPos and SKIPS any car within 40.0f m of mCameraLastFrame
        //     (flt_820BA590, the `vcmpgtfp.` at 0x82736188) -- i.e. it never pops a car in the
        //     player's face. ONLINE it skips that distance test entirely;
        //   * the action is `mxFlags |= 0x10` at 0x827361F4 -- Param::SetShouldBeRemoved(),
        //     after asserting IsAlive() (BrnTrafficParam.h:813).
        // And UpdateParams (_wT2_02.cpp) ALREADY tests E_FLAG_SHOULD_BE_REMOVED and calls the
        // bodied KillParam. ⇒ landing this one function closes the whole valve.
        // ⭐ It does NOT consult the junction FUP score, so it is NOT subject to the 65
        // threshold that leaves two permanently stuck cars pinned at a score of 40.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "UpdateNonDecisionFrame leg NukeTrafficJams (DWARF :1551) -- no body; the "
            "mbNeedToRunTrafficJamNuker clear is gated WITH it so a request whose work never "
            "ran is not marked serviced. It marks jammed params (<5 m/s, >40 m from camera) "
            "SetShouldBeRemoved, and UpdateParams' KillParam consumer is already live");
    }
}

}
