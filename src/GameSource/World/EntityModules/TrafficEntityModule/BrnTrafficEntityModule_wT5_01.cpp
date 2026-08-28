// ============================================================================
// BrnTrafficEntityModule_wT5_01.cpp -- THE TWO PRODUCERS THE CRASH SURFACE WAS WAITING ON.
//
//   TrafficEntityModule::UpdateCrashSlider                      @0x82715A18  (133 insns)
//   TrafficEntityModule::JunctionFUP_StopOffscreenTraffic       @0x82719868  (83 insns)
//   TrafficEntityModule::JunctionFUP_TryClearupNonMovingPhysical@0x8273F2E8  (75 insns)
//   TrafficEntityModule::UpdateJunctionFUP                      @0x82745218  (1365 insns)
//
// WHY THIS FILE EXISTS. Wave T2/T6 landed the whole crash surface -- the crashing-things
// producer and both reactions (UpdateParams_TryAvoidCrashing / _TryStartSympatheticCrashing,
// _wT2_06.cpp) -- and measured them working when FORCED. On the shipped path both arms were
// still dead, because their two GATE INPUTS had no producer anywhere in the tree:
//
//   NeedToTakeActionAgainstJunctionFUP()  reads mfJunctionFUP           <- UpdateJunctionFUP
//   ShouldBeHollywoodAction()             reads mfCrashSliderFinalValue <- UpdateCrashSlider
//
// Both predicates were bodied and correct and both were CONSTANT FALSE. These are the writers.
//
// ⚠️ TWO STALE PARK NOTES, both false, both checked against the image before being deleted:
//   * _wQ7_01.cpp logged `UpdateJunctionFUP (no export dumped)`. The per-function export
//     EXISTS (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82745218.json, 1365 asm lines).
//   * BrnTrafficEntityModule.h declared
//         void JunctionFUP_TryClearupNonMovingPhysical();
//     with NO parameters and a void return. The console is
//         bool JunctionFUP_TryClearupNonMovingPhysical(const FastBitArray<601>::Iterator&, bool)
//     -- r4 is the live iterator (`lwz r11, 0(r28)` == miIndex), r5 the bool (`clrlwi r11,r5,24`),
//     and r3 returns 0/1 at 0x8273F308 / 0x8273F408. Both DWARF dumps agree with the asm.
//     Its sibling was declared `void f(void*, bool)`; the first parameter is the iterator too.
//     Corrected in the header. This is the third wrong declaration found in this cluster.
//
// ⛔⛔ THE CONSTANT TRAP THIS FILE WALKED THROUGH. Every score/radius UpdateJunctionFUP uses is
// a `VecFloat` in the DYNAMICALLY-INITIALISED .data page at 0x8300Cxxx. Reading that page out
// of the image gives 0.0 for ALL of them -- the whole 0x8300C000..0x8300D000 page is zero in
// the loaded image (verified word by word). Taking those zeros would have made every vehicle
// score 0, left mfJunctionFUP identically 0, and produced a perfectly plausible "the avoid arm
// still never fires" -- the project's placeholder-identity failure class, silent and total.
// The values below come from the DYN-INIT THUNKS, each `lfs f0,<src> ; stfs -0x10(r1) ;
// lvx v0,r0,r10 ; vspltw v0,v0,0 ; stvx128 v0,r0,r11` i.e. dst_quad = splat(src_scalar):
//
//   thunk @0x82C66CF0  0x8300CC50 <- splat(flt_8200426C) ==     5.0f
//   thunk @0x82C66D18  0x8300CEC0 <- splat(flt_820BA5E8) ==    30.0f
//   thunk @0x82C66D40  0x8300C9D0 <- splat(flt_820BA7E4) ==    20.0f
//   thunk @0x82C66D68  0x8300CB00 <- splat(flt_820C07FC) == 14400.0f
//   thunk @0x82C65D08  0x8300CAD0 <- splat(flt_8200D4E4) ==  3600.0f
//
// ⭐ THE MAPPING IS CONFIRMED TWICE, INDEPENDENTLY. (a) By behaviour: the 30.0f arm is the one
// guarded by mbIsFatallyCrashing, the 20.0f arm by mfTimeNotDriving >= 6.0f, the 5.0f arm is
// the fall-through. (b) By ORDER: C++ static initialisers run in declaration order within a
// TU, and the thunks in the 0x82C66Cxx run fire 0.001 -> 5.0 -> 30.0 -> 20.0 -> 14400.0, which
// is exactly the DecFIGS declaration order of
//   KF_IS_SIMILAR_TOLLERANCE (.cpp:339), KF_JUNCTION_FUP_PHYSICAL_SCORE (:345),
//   KF_JUNCTION_FUP_FATAL_SCORE (:346), KF_JUNCTION_FUP_NOT_DRIVING_SCORE (:347),
//   KF_JUNCTION_FUP_FAR_FROM_BEHAVIOUR_CENTRE_SQ (:351).
// The two derivations agree constant for constant, and 3600.0f reproduces the value an earlier
// wave recovered for KF_JUNCTION_FUP_MAX_RADIUS_SQ from a different call site.
// (The .rdata reader used is scratchpad/x360rd.py, re-verified against the
// CrashedStuntHudState::GetResourcesToLoad @0x82508510 listing before any value was taken.)
//
// PLAIN .rdata constants (static, read directly, no thunk):
//   flt_820BA290 ==  65.0f  KF_JUNCTION_FUP_SCORE_NEEDS_ACTION
//   flt_820BA294 == 200.0f  KF_JUNCTION_FUP_ONLINE_SCORE_NEEDS_ACTION
//   flt_820BA8F8 ==   6.0f  KF_JUNCTION_FUP_VEHICLE_NOT_DRIVING_TIME
//   flt_82001C98 ==   1.0f  KF_JUNCTION_FUP_TIME_TILL_NEXT_PHYSICAL_KILL
//   flt_820BA62C ==   0.5f  KF_JUNCTION_FUP_TIME_TILL_NEXT_ONLINE_PHYSICAL_KILL
//   flt_820BA5C8/5E4/5F4/5E8/5DC/8200426C/82004014/820BC59C -- the crash-slider set, below.
//
// MOUNT REQUIRED (conductor-owned): add
//   echo "%SRC%\GameSource\World\EntityModules\TrafficEntityModule\BrnTrafficEntityModule_wT5_01.cpp"
// to tools/build/build_game_exe.bat in the SAME change that retires the gates in _wQ7_01.cpp
// and _wT1_02.cpp, or the exe link fails with LNK2019.
//
// Layout is host-native: every member is reached by name. The console displacements in the
// comments only attest which member a line resolves to.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficParam.h"
#include "GameSource/World/Traffic/BrnVehicleSoaData.h"

#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"

#include "rw/math/vpu/vector3_operation.h"          // rw::math::vpu::Dot, operator-
#include "rw/math/vpu/matrix44affine_operation.h"

#include <cstdlib>   // getenv (the BRN_TRAFFIC_DIAG probe)


namespace BrnTraffic
{
namespace
{
    // ---- recovered constants (provenance in the file banner) -----------------------------

    // BrnTrafficConstants.h:129 (DecFIGS). unk_8300CAD0, dyn-init @0x82C65D08.
    // 3600 m^2 == a 60 m radius around the average physical centre.
    const f32 KF_JUNCTION_FUP_MAX_RADIUS_SQ = 3600.0f;

    // BrnTrafficEntityModule.cpp:345/:346/:347 (DecFIGS). The three per-vehicle scores.
    const f32 KF_JUNCTION_FUP_PHYSICAL_SCORE    =  5.0f;   // unk_8300CC50 @0x82C66CF0
    const f32 KF_JUNCTION_FUP_FATAL_SCORE       = 30.0f;   // unk_8300CEC0 @0x82C66D18
    const f32 KF_JUNCTION_FUP_NOT_DRIVING_SCORE = 20.0f;   // unk_8300C9D0 @0x82C66D40

    // BrnTrafficEntityModule.cpp:349 (DecFIGS). flt_820BA8F8, plain .rdata.
    const f32 KF_JUNCTION_FUP_VEHICLE_NOT_DRIVING_TIME = 6.0f;

    // BrnTrafficEntityModule.cpp:351 (DecFIGS). unk_8300CB00, dyn-init @0x82C66D68.
    // 14400 m^2 == 120 m from the behaviour centre (the DWARF locals in
    // JunctionFUP_StopOffscreenTraffic call it lBCToVehicle -- "BC" is that centre, which the
    // module keeps as mCameraLastFrame's Pos row, X360 +0x728C0).
    const f32 KF_JUNCTION_FUP_FAR_FROM_BEHAVIOUR_CENTRE_SQ = 14400.0f;

    // BrnTrafficEntityModule.cpp:353/:354 (DecFIGS). flt_82001C98 / flt_820BA62C.
    const f32 KF_JUNCTION_FUP_TIME_TILL_NEXT_PHYSICAL_KILL        = 1.0f;
    const f32 KF_JUNCTION_FUP_TIME_TILL_NEXT_ONLINE_PHYSICAL_KILL = 0.5f;

    // BrnTrafficConstants.h:130 (DecFIGS). flt_820BA290. The threshold
    // NeedToTakeActionAgainstJunctionFUP() (BrnTrafficEntityModule.cpp) already tests against;
    // repeated here only so the diagnostic can name it.
    const f32 KF_JUNCTION_FUP_SCORE_NEEDS_ACTION = 65.0f;

    // BrnTrafficConstants.h:131 (DecFIGS). flt_820BA294. The SECOND, higher threshold: the
    // physical-kill arm below runs on `NeedToTakeActionAgainstJunctionFUP() || score >= 200`.
    // ⚠️ The console spells the second test INLINE rather than calling
    // NeedToTakeActionAgainstOnlineJunctionFUP() (which exists, DWARF :1881, and is a
    // different function); the inline test honours mbDEBUGOverrideJunctionFUP exactly as the
    // offline predicate does (0x82745B3C..0x82745B70). Transcribed as the console wrote it.
    const f32 KF_JUNCTION_FUP_ONLINE_SCORE_NEEDS_ACTION = 200.0f;

    // ---- the crash-slider set, all plain .rdata ------------------------------------------
    const f32 KF_CRASH_SLIDER_SPIKE_SCORE          = 100.0f;  // flt_820BA5C8
    const f32 KF_CRASH_SLIDER_SPIKE_DECAY          =   0.0f;  // flt_82001CC0
    const f32 KF_CRASH_SLIDER_SPIKE_FACTOR         =  10.0f;  // flt_820BA5E4
    const f32 KF_CRASH_SLIDER_MISBOUNCE_SPIKE_TIME =   3.0f;  // flt_820BA5F4
    const f32 KF_CRASH_SLIDER_SPIKE_GAP            =  30.0f;  // flt_820BA5E8
    const f32 KF_CRASH_SLIDER_SPIKE_GAP_VARIATION  =  15.0f;  // flt_820BA2A8
    const f32 KF_CRASH_SLIDER_SPIKE_HOLD_TIME      =   5.0f;  // flt_8200426C
    const f32 KF_CRASH_SLIDER_DECAY_AFTER_SPIKE    =   0.1f;  // flt_82004014
    const f32 KF_CRASH_SLIDER_FACTOR_AFTER_SPIKE   =   1.5f;  // flt_820BA5DC
    const f32 KF_CRASH_SLIDER_MAX_SCORE            = 100.0f;  // flt_820BA5C8 again
    const f32 KF_CRASH_SLIDER_MIN_INTERESTING      =   1.0f;  // flt_82001C98
    const f32 KF_CRASH_SLIDER_ZERO_POINT           =  10.0f;  // flt_820BA5E4 again
    // flt_820BC59C == 0.011111111f == 1/90, the reciprocal of (MAX_SCORE - ZERO_POINT).
    const f32 KF_CRASH_SLIDER_RECIP_RANGE          = 0.011111111f;

    // The sentinel UpdateJunctionFUP seeds luNextKillVehicle with. It is a 32-bit -1
    // (`li r11, -1` @0x82745268, `cmpwi r11, -1` @0x82745BAC), NOT BrnTraffic's 16-bit
    // KU_INVALID_VEHICLE (0xFFFF) -- the two are different values and this one is the
    // console's.
    const u32 KU_JUNCTION_FUP_NO_KILL_VEHICLE = 0xFFFFFFFFu;

    typedef CgsContainers::FastBitArray<VehicleSoaData::KU_MAX_VEHICLES> TrafficBitArray;

    // The tree's standard "this leg is not reconstructed" probe: one line, once.
    inline void LogMissingLeg(bool& lrbAlreadyLogged, const char* lpcLegNameAndReason)
    {
        if (!lrbAlreadyLogged && CgsDev::Log::gpDebugPrint != 0)
        {
            lrbAlreadyLogged = true;
            *CgsDev::Log::gpDebugPrint << "[TRAF-GATE] " << lpcLegNameAndReason << "\n";
        }
    }

    // ---- [DIAG] NOT IN THE X360 BINARY, OFF BY DEFAULT ------------------------------------
    // BRN_TRAFFIC_DIAG=1 turns on the junction-FUP trace, the same switch _wT2_06.cpp's crash
    // probes use. This is the instrument the wave measures with; it invents no behaviour and
    // reads nothing the console does not. DELETE-WHEN-STABLE.
    CgsDev::Log::DebugPrint* TrafficDiagStream()
    {
        static const bool skbOn = (std::getenv("BRN_TRAFFIC_DIAG") != 0);
        if (!skbOn || CgsDev::Log::gpDebugPrint == 0)
        {
            return 0;
        }
        return CgsDev::Log::gpDebugPrint;
    }
}

// --------------------------------------------------------------------------------------------
// TrafficEntityModule::UpdateCrashSlider  @0x82715A18  (DWARF BrnTrafficEntityModule.h:1290)
//
// THE PRODUCER OF mfCrashSliderFinalValue -- the one input ShouldBeHollywoodAction() reads,
// and therefore the gate on UpdateParams_TryStartSympatheticCrashing (_wT2_02.cpp).
//
// Called from PreSceneUpdate's E_STATE_RUNNING arm, immediately after UpdateTimers and inside
// the `!IsPaused() && !lbSimPaused` guard. PreSceneUpdate is an ARTIST export hole, so the
// position is read straight out of the image:
//     0x8274ABC4  bl  IsPaused                 ; guard
//     0x8274ABD0  bne -> 0x8274AC28            ; skip if paused
//     0x8274ABDC  bne -> 0x8274AC28            ; skip if lUpdateSet & SIM_PAUSED
//     0x8274ABE8  bl  UpdateTimers             (0x82715858)
//     0x8274ABF0  bl  UpdateCrashSlider        (0x82715A18)   <-- here
//     0x8274ABF8  bl  KillDyingVehicleEntities (0x82741E40)
//     0x8274AC04  bl  CreateNewVehicleEntities (0x8272FA30)
//     0x8274AC14  bl  UpdateCollidableVehicles (0x827302C8)
//     0x8274AC20  bl  GenerateCrashedVehicleEvents (0x82720030)
//
// ⚠️ WHAT THIS DOES *NOT* DO, and the previous brief had it the other way round: the showtime
// block only SCHEDULES spikes. The score itself is raised elsewhere -- HandleExternalResponses
// @0x827330B0 does `mfCrashSliderCrashScore += mfCrashSliderCrashScoreFactor * <event value>`
// per incoming crash event, and HandlePrepareForModeAction @0x827480D8 seeds the three
// members. Neither is bodied in this tree, so OUTSIDE showtime the score has no producer yet
// and mfCrashSliderFinalValue stays 0 -- i.e. landing this function alone does NOT make the
// sympathetic-crash arm fire in ordinary driving. It makes it correct, and it makes showtime
// drive it. Say so plainly rather than reporting a swerve that is not there.
// --------------------------------------------------------------------------------------------
void TrafficEntityModule::UpdateCrashSlider()
{
    // 0x82715A28..0x82715A40. The debug switch forces showtime scheduling on.
    if (mbDEBUGFakeShowtime)                       // +0x72876 (:868)
    {
        mbPlayingShowtimeMode = true;              // +0x717DD (:716)
    }

    if (mbPlayingShowtimeMode)
    {
        // 0x82715A8C..0x82715AB8.
        mfShowtimeTimer += mfSimTimeStep;

        // 0x82715AC4..0x82715AD8. Either the scheduled gap elapsed, or the player has been
        // mis-bouncing for 3 s -- both mint a fresh crash spike.
        if (mfShowtimeTimer >= mfShowtimeTimeNextCrashSpike ||
            mfShowtimeMisBounceTimer >= KF_CRASH_SLIDER_MISBOUNCE_SPIKE_TIME)
        {
            // 0x82715AE0..0x82715AF0 -- score pinned at 100 with the decay switched OFF, so
            // it HOLDS until the five-second block below turns the decay back on.
            mfCrashSliderCrashScore      = KF_CRASH_SLIDER_SPIKE_SCORE;
            mfCrashSliderCrashScoreDecay = KF_CRASH_SLIDER_SPIKE_DECAY;
            mfCrashSliderCrashScoreFactor= KF_CRASH_SLIDER_SPIKE_FACTOR;

            // 0x82715B08..0x82715B4C -- the ring draw is CgsNumeric::Random::RandomFloat()
            // inlined on mEffectRand (this + 0x1360 == mRand + sizeof(Random)): read the
            // oldest slot, refill THAT slot from the old seed's high word, step the LCG, then
            // advance the cursor. The returned value is the one primed eight draws ago.
            const f32 lfSpikeGapJitter = mEffectRand.RandomFloat();

            // 0x82715B50..0x82715B68. The console contracts `jitter * 15 + timer` into one
            // fmadds and adds the 30 after, so the gap is 30..45 s from now.
            mfShowtimeMisBounceTimer     = 0.0f;
            mfShowtimeTimeLastCrashSpike = mfShowtimeTimer;
            mfShowtimeTimeNextCrashSpike = mfShowtimeTimer
                                         + lfSpikeGapJitter * KF_CRASH_SLIDER_SPIKE_GAP_VARIATION
                                         + KF_CRASH_SLIDER_SPIKE_GAP;
        }

        // 0x82715B6C..0x82715BA0. Five seconds after a spike, let it start decaying.
        // ⚠️ The console parks mfShowtimeTimeLastCrashSpike at the NEXT spike time, not at
        // "now" -- that is what makes this block fire exactly once per spike instead of every
        // frame afterwards (`lfs f12, 0(r30)` reads +0x724B4, stores to +0x724B8).
        if (mfShowtimeTimer - mfShowtimeTimeLastCrashSpike >= KF_CRASH_SLIDER_SPIKE_HOLD_TIME)
        {
            mfShowtimeTimeLastCrashSpike  = mfShowtimeTimeNextCrashSpike;
            mfCrashSliderCrashScoreFactor = KF_CRASH_SLIDER_FACTOR_AFTER_SPIKE;
            mfCrashSliderCrashScoreDecay  = KF_CRASH_SLIDER_DECAY_AFTER_SPIKE;
        }
    }

    // 0x82715BA4..0x82715BDC. Below "1 point" the score is not interesting; otherwise it
    // decays proportionally (`fnmsubs f0, f12, f0, f0` == score - decay*dt*score) and is
    // clamped to [0, 100] with two fsels.
    if (mfCrashSliderCrashScore <= KF_CRASH_SLIDER_MIN_INTERESTING)
    {
        mfCrashSliderCrashScore = 0.0f;
    }
    else
    {
        const f32 lfDecayThisFrame = mfCrashSliderCrashScoreDecay * mfSimTimeStep;
        f32 lfScore = mfCrashSliderCrashScore - lfDecayThisFrame * mfCrashSliderCrashScore;

        if (lfScore < 0.0f)                       { lfScore = 0.0f; }
        if (lfScore > KF_CRASH_SLIDER_MAX_SCORE)  { lfScore = KF_CRASH_SLIDER_MAX_SCORE; }
        mfCrashSliderCrashScore = lfScore;
    }

    // 0x82715BE0..0x82715BF4. A showtime player who has come to rest on the ground kills the
    // slider outright.
    if (mbShowtimePlayerOnGround)                  // +0x717E6 (:725)
    {
        mfCrashSliderCrashScore = 0.0f;
    }

    // 0x82715BF8..0x82715C24. The 0..1 normalisation ShouldBeHollywoodAction() tests against
    // 0.01: everything below 10 points reads as zero, 100 points reads as one.
    f32 lfFinal = (mfCrashSliderCrashScore - KF_CRASH_SLIDER_ZERO_POINT)
                * KF_CRASH_SLIDER_RECIP_RANGE;
    if (lfFinal < 0.0f) { lfFinal = 0.0f; }
    if (lfFinal > 1.0f) { lfFinal = 1.0f; }
    mfCrashSliderFinalValue = lfFinal;
}

// --------------------------------------------------------------------------------------------
// TrafficEntityModule::JunctionFUP_StopOffscreenTraffic  @0x82719868
//   DWARF BrnTrafficEntityModule.h:1884 --
//     void JunctionFUP_StopOffscreenTraffic(const FastBitArray<601>::Iterator&, bool)
//
// A car nobody can see, a long way from the behaviour centre, is parked: its param's speed is
// zeroed so it stops feeding the jam. Nothing is removed here.
//
// SIGNATURE, from the asm (the committed declaration was `void f(void*, bool)`):
//   r3 this, r4 the LIVE iterator (`lwz r11, 0(r26)` reads miIndex twice, so it is the
//   iterator object and not a bare index), r5 the bool (`clrlwi r11, r5, 24`).
// --------------------------------------------------------------------------------------------
void TrafficEntityModule::JunctionFUP_StopOffscreenTraffic(
        const CgsContainers::FastBitArray<VehicleSoaData::KU_MAX_VEHICLES>::Iterator& lrItVehicle,
        bool lbRenderedLastFrame)
{
    // 0x82719874..0x82719884 -- on screen last frame, leave it alone.
    if (lbRenderedLastFrame)
    {
        return;
    }

    // 0x82719888..0x82719914 -- the iterator's own GetIndex() range assert
    // (CgsFastBitArray.h:235, streamed on the console).
    const u32 luVehicle = static_cast<u32>(lrItVehicle.GetIndex());
    CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "Attempt to get index when out of range");

    // 0x82719918..0x82719954. GetVehicleTransform returns by value (sret at var_80); the
    // console reads its Pos row at +0x30 and the behaviour centre out of the +0x728C0 lane,
    // which is mCameraLastFrame's Pos row.
    const Vector3 lVehiclePos = GetVehicleTransform(luVehicle).Pos();
    const Vector3 lBCToVehicle = lVehiclePos - mCameraLastFrame.GetPosition();
    const f32 lfDistanceFromBCSq = rw::math::vpu::Dot(lBCToVehicle, lBCToVehicle);

    // 0x82719954..0x82719964 `vcmpgefp. v0, v0, v13` -- distSq >= 14400 (120 m).
    if (lfDistanceFromBCSq >= KF_JUNCTION_FUP_FAR_FROM_BEHAVIOUR_CENTRE_SQ)
    {
        // 0x82719968..0x827199A8.
        Param* const lpParam = GetParam(luVehicle);
        CGS_ASSERT(lpParam != 0, "lpParam");     // baked .cpp 17569
        lpParam->mfSpeed = 0.0f;                 // Param +0x14
    }
}

// --------------------------------------------------------------------------------------------
// TrafficEntityModule::JunctionFUP_TryClearupNonMovingPhysical  @0x8273F2E8
//   DWARF BrnTrafficEntityModule.h:1887 --
//     bool JunctionFUP_TryClearupNonMovingPhysical(const FastBitArray<601>::Iterator&, bool)
//
// The harder half: an offscreen physical car that is either fatally crashing or has not driven
// for six seconds is DELETED, not parked. Returns whether it removed one.
// --------------------------------------------------------------------------------------------
bool TrafficEntityModule::JunctionFUP_TryClearupNonMovingPhysical(
        const CgsContainers::FastBitArray<VehicleSoaData::KU_MAX_VEHICLES>::Iterator& lrItVehicle,
        bool lbRenderedLastFrame)
{
    // 0x8273F2F4..0x8273F310.
    if (lbRenderedLastFrame)
    {
        return false;
    }

    // 0x8273F314..0x8273F3A0 -- the same CgsFastBitArray.h:235 assert.
    const u32 luVehicle = static_cast<u32>(lrItVehicle.GetIndex());
    CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "Attempt to get index when out of range");

    // 0x8273F3A4..0x8273F3D8.
    const TrafficPhysicsInfo* const lpPhysInfo = GetTrafficPhysicsInfoForVehicl(luVehicle);
    CGS_ASSERT(lpPhysInfo != 0, "lpPhysInfo");   // baked .cpp 17597

    // 0x8273F3DC..0x8273F3F8 -- still driving and not crashing, so it is not the problem.
    if (!lpPhysInfo->mbIsFatallyCrashing &&                          // +0xFE6
        lpPhysInfo->mfTimeNotDriving < KF_JUNCTION_FUP_VEHICLE_NOT_DRIVING_TIME)  // +0xFD8
    {
        return false;
    }

    // 0x8273F3FC..0x8273F408.
    {
        // GATE: RemoveVehicle @0x8272E370 (499 insns) is UNRECONSTRUCTED tree-wide -- its own
        // blockers (GetVehicleSpecies / Vehicle::DetachArticulation /
        // StaticTrafficParam::SetShouldBeRemoved) have no bodies either, and
        // ReturnPhysicalVehicleToTraffic (_wT3_02.cpp) already logs the same hole.
        // COST, and it is a REAL behaviour delta, not a cosmetic one: the junction-FUP RELIEF
        // VALVE never runs, so a jam that pushes the score over 65 is never cleared by this
        // path. The score itself is still correct -- UpdateJunctionFUP recomputes it from
        // scratch every frame -- so the gate the wave exists to open is unaffected; what is
        // missing is the console's way of ending the jam.
        // DELETE-WHEN RemoveVehicle lands.
        static bool sbLoggedRemoveVehicle = false;
        LogMissingLeg(sbLoggedRemoveVehicle,
                      "JunctionFUP_TryClearupNonMovingPhysical's RemoveVehicle @0x8272E370 -- "
                      "unreconstructed; the stuck offscreen car is not deleted");
    }
    return true;
}

// --------------------------------------------------------------------------------------------
// TrafficEntityModule::UpdateJunctionFUP  @0x82745218  (DWARF BrnTrafficEntityModule.h:1875)
//
// THE PRODUCER OF mfJunctionFUP -- the one input NeedToTakeActionAgainstJunctionFUP() reads,
// and therefore the gate on UpdateParams_TryAvoidCrashing (_wT2_02.cpp) AND on SpawnNewTraffic
// @0x82748A40.
//
// "FUP" is the module's own congestion score. It is rebuilt from zero every frame: every alive
// PHYSICAL traffic car within 60 m of the average physical centre contributes 5 / 20 / 30
// points depending on whether it is driving, stuck, or fatally crashing. At 65 points the
// module decides the junction is jammed and switches traffic into avoidance; at 200 it also
// starts deleting the furthest offender.
//
// Call site (PrePhysicsUpdate @0x8274C690, r3 == this only, no arguments):
//     0x8274C7A8  bl  BuildPotentialCollisionList (0x8274B378)
//     0x8274C7B0  bl  UpdateJunctionFUP           (0x82745218)   <-- here
//     0x8274C7BC  bl  GenerateDriverInputs        (0x82748E78)
//
// DWARF locals (BrnTrafficUnity.cpp:21390): lfFurthestDistance, luNextKillVehicle, lItVehicle,
// lVehicles_Alive_And_Physical, and per-iteration luVehicle / lpPhysInfo / lVehicleTransform /
// lVehiclePosition / lVehicleToCentre / lfDistanceFromCentreSq / lbRenderedLastFrame.
//
// ⚠️ THE PREDICATE IS INLINED THREE TIMES, not called. The console expands
// NeedToTakeActionAgainstJunctionFUP() at 0x82745AD0, 0x82745B3C and 0x82745BF0; the middle
// one is the 200-point variant. All three honour mbDEBUGOverrideJunctionFUP first, which is
// why the existing BRN_TRAFFIC_FORCE_AVOID switch keeps working unchanged.
// --------------------------------------------------------------------------------------------
void TrafficEntityModule::UpdateJunctionFUP()
{
    // 0x82745238..0x82745264. The score is a per-frame quantity, cleared before anything else
    // -- including before the showtime early-out, so showtime leaves it at zero.
    mfJunctionFUP = 0.0f;

    if (mbPlayingShowtimeMode)
    {
        return;
    }

    // 0x82745268..0x8274526C.
    f32 lfFurthestDistance   = 0.0f;
    u32 luNextKillVehicle    = KU_JUNCTION_FUP_NO_KILL_VEHICLE;

    // 0x82745278..0x827452B0 -- ten ld/and/std over the two SoA sets
    // (this+0x282D0 == mAliveVehicles, this+0x283C0 == mPhysicalVehicles).
    TrafficBitArray lVehicles_Alive_And_Physical;
    lVehicles_Alive_And_Physical.SetAnd(mVehicleSoaData.mAliveVehicles,
                                        mVehicleSoaData.mPhysicalVehicles);

    u32 luDiagScored = 0;

    // ============================================================================
    // PASS 1 -- score the jam, and remember the furthest offender.
    // 0x827454B4..0x82745ACC.
    // ============================================================================
    for (TrafficBitArray::Iterator lItVehicle = lVehicles_Alive_And_Physical.Begin();
         lItVehicle != lVehicles_Alive_And_Physical.End();
         ++lItVehicle)
    {
        const u32 luVehicle = static_cast<u32>(lItVehicle.GetIndex());
        CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "Attempt to get index when out of range");

        // 0x82745518..0x82745548. DWARF names this local `TrafficPhysicsInfo* lpPhysInfo`
        // (non-const, unlike TryClearupNonMovingPhysical's).
        TrafficPhysicsInfo* const lpPhysInfo = GetTrafficPhysicsInfoForVehicl(luVehicle);
        CGS_ASSERT(lpPhysInfo != 0, "lpPhysInfo");    // baked .cpp 17457
        if (lpPhysInfo == 0)
        {
            continue;    // PC-safety guard, as in the sibling partfiles
        }

        // 0x8274556C..0x8274558C. GetVehicleTransform carries the
        // `luIndex < KU_MAX_TOTAL_TRAFFIC` assert baked at BrnTrafficEntityModule.h:2483.
        const Vector3 lVehiclePosition = GetVehicleTransform(luVehicle).Pos();
        const Vector3 lVehicleToCentre = mAveragePhysicalCentre - lVehiclePosition;
        const f32 lfDistanceFromCentreSq = rw::math::vpu::Dot(lVehicleToCentre, lVehicleToCentre);

        // 0x82745590..0x827456BC `vcmpgefp128. v0, v13, v127` -- i.e. distSq <= 3600.
        // ⚠️ THE THREE ADDENDS ARE THE dyn-init QUADS. Reading them off the image gives 0.0
        // for all three and the whole score collapses to a plausible, silent zero. See the
        // banner: 30 / 5 / 20, recovered from the thunks and cross-checked by declaration
        // order.
        if (lfDistanceFromCentreSq <= KF_JUNCTION_FUP_MAX_RADIUS_SQ)
        {
            if (lpPhysInfo->mbIsFatallyCrashing)                                      // +0xFE6
            {
                mfJunctionFUP += KF_JUNCTION_FUP_FATAL_SCORE;
            }
            else if (lpPhysInfo->mfTimeNotDriving < KF_JUNCTION_FUP_VEHICLE_NOT_DRIVING_TIME)
            {
                mfJunctionFUP += KF_JUNCTION_FUP_PHYSICAL_SCORE;
            }
            else
            {
                mfJunctionFUP += KF_JUNCTION_FUP_NOT_DRIVING_SCORE;
            }
            ++luDiagScored;
        }

        // 0x827456C4..0x827457DC. The kill candidate is the FURTHEST car from the centre --
        // and, when divergent behaviour is allowed, one the player is not looking at.
        // ⚠️ `lfFurthestDistance` starts at zero and is compared with `>`, so the first
        // candidate must be strictly further than the centre itself; that is the console's
        // own seeding (`vspltisw128 v126, 0` @0x8274526C).
        if (lfDistanceFromCentreSq > lfFurthestDistance)
        {
            if (!mbAllowDivergentBehaviour ||
                !mVehicleSoaData.mVehiclesRenderedLastFrame.IsBitSet(luVehicle))
            {
                lfFurthestDistance = lfDistanceFromCentreSq;
                luNextKillVehicle  = luVehicle;
            }
        }
    }

    // ============================================================================
    // 0x82745AD0..0x82745BEC -- the physical-kill arm.
    // ============================================================================
    if (NeedToTakeActionAgainstJunctionFUP() ||
        (mbDEBUGOverrideJunctionFUP ||
         mfJunctionFUP >= KF_JUNCTION_FUP_ONLINE_SCORE_NEEDS_ACTION))
    {
        // 0x82745B80..0x82745BA4.
        mfJunctionFUP_TimeTillNextPhysicalKill -= mfSimTimeStep;

        if (mfJunctionFUP_TimeTillNextPhysicalKill <= 0.0f &&
            luNextKillVehicle != KU_JUNCTION_FUP_NO_KILL_VEHICLE)
        {
            {
                // GATE: RemoveVehicle @0x8272E370 -- see the note in
                // JunctionFUP_TryClearupNonMovingPhysical above. Same hole, same cost.
                static bool sbLoggedRemoveVehicle = false;
                LogMissingLeg(sbLoggedRemoveVehicle,
                              "UpdateJunctionFUP's RemoveVehicle @0x8272E370 -- "
                              "unreconstructed; the furthest jammed car is not deleted");
            }

            // 0x82745BC4..0x82745BEC. Offline (divergent behaviour allowed) waits a full
            // second between kills; online only half.
            mfJunctionFUP_TimeTillNextPhysicalKill =
                mbAllowDivergentBehaviour ? KF_JUNCTION_FUP_TIME_TILL_NEXT_PHYSICAL_KILL
                                          : KF_JUNCTION_FUP_TIME_TILL_NEXT_ONLINE_PHYSICAL_KILL;
        }
    }

    // 0x82745BF0..0x82745C50 -- the third inline expansion of the predicate; below the
    // threshold there is no jam and pass 2 does not run.
    if (!NeedToTakeActionAgainstJunctionFUP())
    {
        if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
        {
            static u32 suDiagFrame = 0;
            if ((suDiagFrame++ % 300u) == 0u)
            {
                *lpDiag << "[T5-fup] score=" << mfJunctionFUP
                        << " scored=" << luDiagScored
                        << " (below " << KF_JUNCTION_FUP_SCORE_NEEDS_ACTION << ", no action)\n";
            }
        }
        return;
    }

    // ============================================================================
    // PASS 2 -- the jam is real. Park or delete the offscreen cars.
    // 0x82745C54..0x8274674C. NOTE this walks mAliveVehicles DIRECTLY (the console takes
    // Begin() off this + 0x282D0, not off a local intersection).
    // ============================================================================
    u32 luDiagOffscreen = 0;

    for (TrafficBitArray::Iterator lItVehicle = mVehicleSoaData.mAliveVehicles.Begin();
         lItVehicle != mVehicleSoaData.mAliveVehicles.End();
         ++lItVehicle)
    {
        const u32 luVehicle = static_cast<u32>(lItVehicle.GetIndex());
        CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "Attempt to get index when out of range");

        // 0x82745DEC `cmplwi r19, 0x190 ; bge` -- driving traffic only. The 400..599 band is
        // the parked/static pool and is not this function's business.
        if (luVehicle >= KU_MAX_PARAMS)
        {
            continue;
        }

        // 0x8274613C..0x8274615C. The console reads the bit with the ITERATOR'S cached mask
        // rather than re-deriving one, and reuses the result for both calls.
        const bool lbRenderedLastFrame =
            mVehicleSoaData.mVehiclesRenderedLastFrame.IsBitSet(luVehicle);

        // 0x8274616C.
        JunctionFUP_StopOffscreenTraffic(lItVehicle, lbRenderedLastFrame);
        if (!lbRenderedLastFrame)
        {
            ++luDiagOffscreen;
        }

        // 0x8274641C..0x82746454 -- the delete pass is physical-only.
        if (mVehicleSoaData.mPhysicalVehicles.IsBitSet(luVehicle))
        {
            JunctionFUP_TryClearupNonMovingPhysical(lItVehicle, lbRenderedLastFrame);
        }
    }

    if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
    {
        static u32 suDiagActionFrame = 0;
        if ((suDiagActionFrame++ % 60u) == 0u)
        {
            *lpDiag << "[T5-fup] ACTION score=" << mfJunctionFUP
                    << " scored=" << luDiagScored
                    << " offscreen=" << luDiagOffscreen
                    << " killCand=" << static_cast<s32>(luNextKillVehicle) << "\n";
        }
    }
}

}
