// ============================================================================
// BrnTrafficEntityModule_wT1_06.cpp  --  wave T1 (parked traffic cars) ROUND 4, item 1:
// THE STEADY-STATE LOOP.
//
// THREE FUNCTIONS LIVE HERE:
//   TrafficEntityModule::UpdateTimers           @0x82715858   (DWARF :1287)
//   TrafficEntityModule::UpdateDecisionFrame    @0x8274E508   (DWARF :1476)
//   TrafficEntityModule::UpdateNonDecisionFrame @0x8274C1A8   (DWARF :1479)
//
// ============================================================================
// ⭐⭐ WHAT THE ASM SAYS, AND WHY THE ROUND-4 BRIEF'S SHAPE HAD TO CHANGE
//
// The brief asked for "the PostPhysicsUpdate E_STATE_RUNNING-arm legs the console runs every
// decision frame -- RecalculateActiveHulls, SpawnNewTraffic, the StaticVehicles_* updates".
// Derived from the asm @0x8274E6D0, THOSE THREE ARE NOT IN PostPhysicsUpdate AT ALL. Its
// RUNNING arm (0x8274E6D0's `v14 == 1` branch) contains no call to any of them. What it has
// is a two-way dispatch:
//
//     if (IsPaused() || lbSimPaused) { ...nothing but the perfmon bracket... }
//     else if (IsDecisionFrame())  UpdateDecisionFrame(lpInput, lpOutput);
//     else                         UpdateNonDecisionFrame(lpInput, lpOutput);
//
// and UpdateDecisionFrame @0x8274E508 is where the spawn ladder actually lives:
//     assert(IsDecisionFrame());                                     ; .cpp 7150
//     ++muUpdateCount;
//     RecalculateActiveHulls(lpInput, lpOutput, &lNew, &lOld);
//     KillOutOfAreaTraffic(&lOld);
//     KillTrafficOnStartGridWholeSale(GetPlayerRaceCarState(...)->pos);
//     SpawnNewTraffic(lNew);
//     if (mbPlayingShowtimeMode) SpawnShowtimeTraffic();
//     UpdateJunctions();
//     UpdateParams(lpInput);
//     StaticVehicles_UpdateStaticParams();
//     UpdateVehicles(lpInput, lpOutput);
//     StaticVehicles_UpdateVehicles(lpInput);
//     UpdateTrailers(lpInput, lpOutput);
//     muLastParamCalculated = 0;  mbNeedToRunTrafficJamNuker = true;
//     ...the online start-protect countdown...
//
// ⚠️⚠️ AND THE LOOP HAS A GATE THE BRIEF DID NOT NAME: IsDecisionFrame(). Outside
// E_STATE_STARTING_UP it returns mbDecisionFrame, and mbDecisionFrame HAS EXACTLY ONE WRITER
// IN THE ENTIRE IMAGE -- UpdateTimers @0x82715858, whose only caller is PreSceneUpdate's
// E_STATE_RUNNING arm (verified by grepping the whole export set for the +0x713F5 store and
// by UpdateTimers' own `xrefs_to`). Reset() seeds it FALSE. So on this build, the instant the
// module left E_STATE_STARTING_UP, IsDecisionFrame() became permanently false and
// UpdateDecisionFrame could never have run even with a perfect RUNNING-arm dispatch: every
// frame would have taken the NON-decision branch for ever.
//
// That is why UpdateTimers lands here alongside the two frame functions, and why
// BrnTrafficEntityModule_wT1_02.cpp un-gates it in the same change. Landing the dispatch
// without the flag's producer would have produced a build that looks wired and still never
// recalculates a hull -- the "gate-green != closeable" shape this campaign keeps recording.
//
// ============================================================================
// WHAT IS REAL / WHAT IS GATED
//
// REAL: all of UpdateTimers except one store (below); both frame functions' skeletons,
// asserts and bookkeeping; and, inside them, exactly the four parked-path legs whose bodies
// exist -- RecalculateActiveHulls, SpawnNewTraffic, StaticVehicles_UpdateStaticParams,
// StaticVehicles_UpdateVehicles.
//
// GATED, every one because it has NO BODY ANYWHERE IN THE TREE (grep-verified against the
// post-round-3 tree, not assumed): KillOutOfAreaTraffic, KillTrafficOnStartGridWholeSale,
// SpawnShowtimeTraffic, UpdateJunctions, UpdateParams, UpdateVehicles, UpdateTrailers,
// UpdateLerpedParamTransforms, UpdateParams_DoTimeSlicedLogic, NukeTrafficJams. All ten are
// driving-traffic work; a PARKED car is created by RecalculateActiveHulls -> SpawnNewTraffic
// -> FillNewHull -> StaticVehicles_Generate and made alive by StaticVehicles_CreateNewVehicles
// (reached through StaticVehicles_UpdateVehicles), and none of the ten is on that path.
//
// ⚠️ THE ONE GATE WITH A REAL BEHAVIOURAL COST, STATED PLAINLY: KillOutOfAreaTraffic is the
// function that retires params for hulls that just went OUT of range. With it gated, a static
// param is never handed back to mFreeStaticParamStack, so over a long drive the 199-slot pool
// drains and new hulls stop producing parked cars. THAT IS BOUNDED AND SILENT-SAFE, not a
// crash: StaticVehicles_Generate's first statement is `if (mFreeStaticParamStack.GetLength()
// == 0) return;` (wT1_01), so an empty pool is a no-op, not an assert. The visible symptom is
// "parked cars stop appearing after driving far enough", which is the right failure direction
// for a bring-up wave and is reported by the [T1-static] probe's aliveParams count.
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
    // ------------------------------------------------------------------------------------
    // NAMED LEG GATE + diag plumbing -- the same shape (and the same "[FLAG PC partial gate]"
    // tail) as the sibling partfiles, so one boot log reads as a single stream. File-local by
    // design; this directory's convention. [DIAG] NOT IN THE X360 BINARY.
    // ------------------------------------------------------------------------------------
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

    // ---- the console's own literals, all plain .rdata floats read by the asm ------------
    // flt_820BA570 -- the scale on dt^2 for the VecFloat at X360 +0x72700 (see the gate in
    // UpdateTimers). flt_82004014 == 0.1f is the online sim-time-since-decision floor.
    // The 50 Hz test itself is NOT open-coded here: it is
    // CgsSystem::TimerStatusInterface::IsSimTimerFrequency50Hz(), whose committed body is
    // literally `mSimTimerStatus.GetCurrentTimeStep() == 0.02f` -- the same compare against
    // flt_82005574 the console inlines at 0x82715988.
    const f32 KF_SIM_TIMESTEP_SQ_SCALE            = 360.0f;   // flt_820BA570
    const f32 KF_ONLINE_SIM_TIME_SINCE_DECISION   = 0.1f;     // flt_82004014

    // `cmplwi r11, 5` / `cmplwi r11, 6` at 0x827159A4 / 0x827159AC -- a decision frame every
    // 5 frames at 50 Hz, every 6 at 60 Hz (both == 0.1 s).
    const u32 KU_FRAMES_PER_DECISION_50HZ = 5;
    const u32 KU_FRAMES_PER_DECISION_60HZ = 6;

    // (0x8274E694 `cmplwi r11, 0x64` is BrnTraffic::KU_START_PROTECT_UPDATE_FRAME_ONLINE ==
    // 100, which BrnTrafficConstants.h:153 already carries from DWARF :108 -- the canonical
    // one is used below rather than a second file-local copy.)
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateTimers  @ 0x82715858   (.cpp 1922)
//
// The frame clock for the whole traffic sim. Store map, every offset resolved to a named
// member (nothing below is reached by arithmetic):
//   +0x713F4  muFramesSinceDecision   +0x713F5  mbDecisionFrame
//   +0x713F8  mfSimTimeSinceLastDecision
//   +0x713FC  mfSimTimeStep           +0x71400  mfSimTimeStepMultiplier
//   +0x71410  mfSimTimeStepVec        +0x717E7  mbAllowDivergentBehaviour
//
// Console, statement for statement:
//   if (IsDecisionFrame())  mfSimTimeSinceLastDecision = 0.0f;      ; flt_82001CC0
//   mfSimTimeStep           = lpInput->GetTimerStatusInterface()
//                                     ->GetSimTimerStatus()->GetCurrentTimeStep();
//   mfSimTimeStepMultiplier = ...->GetSimTimerStatus()->GetTimeStepMultiplier();
//   mfSimTimeStepVec        = splat(mfSimTimeStep);
//   <the +0x72700 splat -- GATED, see below>
//   muFramesSinceDecision  += 1;   mbDecisionFrame = false;
//   if (muFramesSinceDecision >= (IsSimTimerFrequency50Hz() ? 5 : 6))
//   { mbDecisionFrame = true; muFramesSinceDecision = 0; }
//   mfSimTimeSinceLastDecision += <current sim step>;
//   if (!mbAllowDivergentBehaviour && IsDecisionFrame())
//       mfSimTimeSinceLastDecision = 0.1f;
//
// ⚠️ ORDER IS LOAD-BEARING AND EASY TO "TIDY" WRONG. mbDecisionFrame is cleared BEFORE the
// counter test and set again only if the counter reached its limit -- so a frame is a
// decision frame for exactly one visit. Clearing it after the test would make every frame a
// decision frame; clearing it in the else arm would leave it latched high.
//
// ⚠️ WHY THE FIRST FRAME IS A DECISION FRAME. Reset() seeds muFramesSinceDecision = 100, so
// the first increment gives 101, which is >= 5 and >= 6 either way -- the module does not
// wait a tenth of a second before its first RUNNING recalculation. That is the console's
// behaviour, and it is what makes item 1's "self-heal" claim true: even if the one
// POPULATING-time RecalculateActiveHulls produced zero hulls, the first RUNNING decision
// frame recomputes them.
//
// ⚠️ THE CONSOLE CALLS GetTimerStatusInterface FOUR TIMES (0x82715894 / 0x827158B8 /
// 0x82715968 / 0x827159C0) and recomputes the same product from it each time. That is the
// compiler re-materialising an inlined accessor across a call-free stretch, not four
// different buffers; de-inlined to one local here, exactly as round 2 did for
// PostPhysicsUpdate's triple GetActiveRaceCarOutputInterface. The read-lock assert inside the
// getter is idempotent.
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

    // 0x8272158F0..0x82715924: build a 4-lane splat of mfSimTimeStep on the stack (three zero
    // words then the float, `lvx128` + `vspltw ,0`) and store it. Expressed as the broadcast
    // it is; the zero-word staging is the console's way of getting a scalar into a lane.
    mfSimTimeStepVec.x = mfSimTimeStep;
    mfSimTimeStepVec.y = mfSimTimeStep;
    mfSimTimeStepVec.z = mfSimTimeStep;
    mfSimTimeStepVec.w = mfSimTimeStep;

    {
        // ---- THE ONE GATED STORE IN THIS FUNCTION ----
        // 0x82715928..0x82715954 computes `splat(mfSimTimeStep * mfSimTimeStep *
        // flt_820BA570)` with flt_820BA570 == 360.0f and stores it to a VecFloat member at
        // X360 +0x72700.
        //
        // THAT MEMBER HAS NO ATTESTED NAME. +0x72700 falls between
        // kfParamAvoidCrashCone_CosAngle_Length_RecipYScale_W (DWARF :821) and
        // mpDebugComponent (DWARF :834, X360 +0x727B0) -- i.e. inside the DecFIGS
        // UN-EMITTED :822..:833 window, the same class of hole as [MEMBER HOLE 6]
        // mCameraLastFrame. Inventing a member there would fake a layout on a keystone
        // header, which is this tree's #1 recurring defect.
        //
        // ⭐ AND IT IS WRITE-ONLY ON THIS BUILD, which is why gating it costs nothing:
        // scanning EVERY per-function export for `468736` / `0x72700` returns exactly ONE
        // file -- this function. There is no reader anywhere in the image. (Caveat recorded
        // honestly: a reader that reached it as base+displacement off some other anchor would
        // not match that scan. None is known.)
        //
        // UNBLOCKED BY: naming DWARF :822..:833. The expression is written above so whoever
        // names it can land the store in one line.
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
            // [T1-loop] one-shot: the first time the steady-state clock declares a decision
            // frame. Its absence is the single most diagnostic fact about a dead loop.
            // DELETE-WHEN-STABLE.
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
        // ONLINE-ONLY: offline mbAllowDivergentBehaviour is always true (EnterStartingUpState
        // sets it from !mbIsOnlineGameMode), so this arm is dead on this build. Written
        // anyway -- it is two instructions and a literal, not a blocker.
        if (IsDecisionFrame())
        {
            mfSimTimeSinceLastDecision = KF_ONLINE_SIM_TIME_SINCE_DECISION;
        }
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateDecisionFrame  @ 0x8274E508   *** PARTIAL ***  (.cpp 7046)
//
// ⭐⭐ THIS IS THE STEADY-STATE SPAWN LOOP. Everything the POPULATING arm does once, this
// does every decision frame (5 or 6 render frames, i.e. 10 Hz) for as long as the module is
// RUNNING -- which is what makes a one-frame miss at POPULATING self-healing rather than
// terminal, and is exactly why the round-4 boot evidence said not to reorder anything.
//
// ⚠️ WHICH SET GOES WHERE. RecalculateActiveHulls fills lNewActiveHulls (r6, the 4th arg) and
// lOldActiveHulls (r7, the 5th). KillOutOfAreaTraffic then takes the OLD set
// (`0x8274E590 addi r4, r1, var_170` == the 5th arg's buffer) and SpawnNewTraffic takes the
// NEW one (`0x8274E5C0 addi r4, r1, var_D0`). Crossing them would kill the hulls that just
// arrived and try to fill the ones that just left.
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdateDecisionFrame(
    const BrnTrafficIO::InputBuffer_PostPhysics* lpInput,
    BrnTrafficIO::OutputBuffer_PostPhysics* lpOutput)
{
    CGS_ASSERT(IsDecisionFrame(), "IsDecisionFrame()");   // baked .cpp 7150

    // 0x8274E564..0x8274E570: `lhz / addi 1 / sth` on the u16 at +0x71B30. It is read back
    // by this function's own start-protect arm below.
    ++muUpdateCount;

    {
        // The console brackets the spawn half in PerfMonCpu Start/StopMonitor(+0x72A40) and
        // the update half in +0x72A44. Same reason as every other traffic perfmon bracket in
        // this cluster: Construct's twenty AddMonitor registrations are themselves a gate, so
        // the handles are never issued and passing one would index the monitor registry with
        // a handle nothing allocated.
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
        // ---- THE ONE GATE IN THIS FUNCTION WITH A BEHAVIOURAL COST ----
        // KillOutOfAreaTraffic @? (DWARF :1539 `void KillOutOfAreaTraffic(Set<uint16_t,72u>
        // *)`) has no body in this tree -- it is the retire half of the spawn ladder, and it
        // retires DRIVING params as well as parked ones (it walks the param pools by hull).
        // CONSEQUENCE, stated in full at the file banner: parked params for hulls that go out
        // of range are never handed back to mFreeStaticParamStack, so the 199-slot pool drains
        // monotonically and new hulls eventually stop producing parked cars. Bounded and
        // assert-free (StaticVehicles_Generate no-ops on an empty stack), and visible in the
        // [T1-static] aliveParams count. NOT worked around with a host-side recycler -- that
        // would be inventing a retirement policy the binary does not have.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "UpdateDecisionFrame leg KillOutOfAreaTraffic (DWARF :1539) -- no body in this "
            "tree; it is the RETIRE half of the spawn ladder and covers driving params too. "
            "COST: static params for out-of-range hulls are never freed, so the 199-slot pool "
            "drains over a long drive and parked cars stop appearing. Bounded, assert-free, "
            "visible in [T1-static] aliveParams");
    }

    {
        // KillTrafficOnStartGridWholeSale (DWARF :1794, `void ...(Vector3)`): no body, and its
        // argument comes through sub_823102F0 off the post-physics active-race-car interface
        // (the player's grid position). Event-start-only surface; on a free drive there is no
        // start grid to clear.
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

    // ---- REAL: the parked param lifecycle (purgatory tick + kill/remove sweep) -----------
    StaticVehicles_UpdateStaticParams();

    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "UpdateDecisionFrame leg UpdateVehicles (DWARF :1713) -- no body; the DRIVING "
            "vehicles' per-decision-frame update (steering/avoidance/effects), wave 2. Note "
            "its PARKED counterpart StaticVehicles_UpdateVehicles IS called below");
    }

    // ---- REAL: the parked vehicle update, and with it StaticVehicles_CreateNewVehicles ----
    // ⭐ THIS IS THE CALL THAT MAKES PARKED CARS EXIST IN STEADY STATE. Its first statement
    // is StaticVehicles_CreateNewVehicles(lpInput), the function that turns an alive static
    // PARAM into an alive static VEHICLE via Vehicle::InitialiseAsStatic.
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
        // 0x8274E67C..0x8274E6BC. ONLINE-only start-line protection: after
        // KU_START_PROTECT_UPDATE_FRAME_ONLINE decision frames the protection is dropped, and
        // the console asserts that it lands on the frame EXACTLY (a `>` would mean a frame was
        // missed). Written rather than gated -- it is a counter compare and a flag clear, both
        // on members this header already has.
        if (muUpdateCount >= KU_START_PROTECT_UPDATE_FRAME_ONLINE)
        {
            CGS_ASSERT(muUpdateCount == KU_START_PROTECT_UPDATE_FRAME_ONLINE,
                       "KU_START_PROTECT_UPDATE_FRAME_ONLINE == muUpdateCount");   // .cpp 7209
            mbAtStartLineSoProtectRaceCarsFromTraffic = false;
        }
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateNonDecisionFrame  @ 0x8274C1A8   *** PARTIAL ***  (.cpp 7124)
//
// The other four frames in five: no hull recalculation and no spawning, just the per-frame
// movement of what already exists plus a slice of the param logic.
//   assert(!IsDecisionFrame());                                    ; .cpp 7228
//   assert(lpInput);                                               ; .cpp 7229
//   UpdateLerpedParamTransforms();
//   UpdateVehicles(lpInput, lpOutput);
//   StaticVehicles_UpdateVehicles(lpInput);
//   UpdateTrailers(lpInput, lpOutput);
//   if (muLastParamCalculated < KU_MAX_PARAMS)
//       UpdateParams_DoTimeSlicedLogic(muLastParamCalculated, muLastParamCalculated + 100,
//                                      lpInput->GetActiveRaceCarOutputInterface());
//   if (mbNeedToRunTrafficJamNuker && !mbNeedToKillAllZombies)
//   { NukeTrafficJams(); mbNeedToRunTrafficJamNuker = false; }
//
// ⚠️ THE PARKED LEG RUNS ON EVERY FRAME, NOT ONLY DECISION FRAMES. StaticVehicles_UpdateVehicles
// appears in BOTH functions, which means StaticVehicles_CreateNewVehicles is attempted every
// frame -- so a param generated on a decision frame becomes a live vehicle on the very next
// frame rather than 100 ms later. That is the console's shape and it is why this function is
// landed at all instead of being left as one gate.
//
// ⚠️ THE TIME-SLICE CURSOR IS NOT ADVANCED HERE. The console reads muLastParamCalculated and
// passes [cursor, cursor+100) to UpdateParams_DoTimeSlicedLogic, which is what advances it.
// With that callee gated the cursor stays 0 and the window is re-requested every frame -- and
// since the callee does nothing, so does the whole leg. Advancing it locally would be
// inventing progress through a pass that never ran.
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

    // ---- REAL: parked vehicles are created/updated on every frame, not just decision ones --
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
        // NukeTrafficJams (DWARF :1551) has no body. The flag CLEAR is gated with it on
        // purpose: clearing a request whose work never ran would make the request look
        // serviced. Nothing else in the tree reads mbNeedToRunTrafficJamNuker, so leaving it
        // latched high is inert.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "UpdateNonDecisionFrame leg NukeTrafficJams (DWARF :1551) -- no body; the "
            "mbNeedToRunTrafficJamNuker clear is gated WITH it so a request whose work never "
            "ran is not marked serviced. Jams are a DRIVING-traffic condition (wave 2)");
    }
}

}
