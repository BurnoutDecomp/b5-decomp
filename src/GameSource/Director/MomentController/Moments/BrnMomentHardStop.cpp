#include "GameSource/Director/MomentController/Moments/BrnMomentHardStop.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"        // Camera::Behaviour (the candidates' base accessors)
#include "GameShared/GameClasses/Core/CgsStringUtils.h"               // CgsCore::SPrintf
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                 // CgsNumeric::Random
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugPrinter.h"   // DebugLog
#include "GameSource/Director/Utils/BrnShotSelector.h"                // ShotSelector::GetCrashShot

// BrnDirector::MomentHardStop -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (DWARF primary file BrnMomentHardStop.cpp; member/parameter names verbatim
// from the DecFIGS DWARF).
//
// Bodied here (5 ledger functions):
//   Construct @0x8225EC08   Prepare @0x821F7518   Update  @0x82271438
//   SetParameters @0x821F7548                     GetName @0x821F7550

namespace BrnDirector
{

namespace
{
    // XEX rodata (values read from the decrypted image):
    //   dword_82CDA53C == 3      every 3rd high-energy world crash forces ultra slo-mo
    //   flt_82CDA544  == 0.005f  ultra slo-mo timestep scale range MIN
    //   flt_82CDA540  == 0.01f   ultra slo-mo timestep scale range MAX
    const u32 KU_CRASHES_BEFORE_FORCE_ULTRA_SLOMO = 3;        // cpp:29 (@0x82CDA53C)
    const f32 KF_ULTRA_SLOMO_TIMESTEP_MIN_SCALE   = 0.005f;   // cpp:31 (@0x82CDA544)
    const f32 KF_ULTRA_SLOMO_TIMESTEP_MAX_SCALE   = 0.01f;    // cpp:30 (@0x82CDA540)

    // Camera-state head bits the moment raises (the moment family's shared per-bit
    // vocabulary -- BrnCameraState.h; bit roles not yet recovered):
    const u32 KU_HEAD_FLAG_SEARCHING = 18;   // oris 4      searching, condition absent
    const u32 KU_HEAD_FLAG_ALLOCATED = 19;   // oris 8      entering FOUND_PREPARING
    const u32 KU_HEAD_FLAG_PREPARING = 20;   // oris 0x10   waiting on the candidates
    const u32 KU_HEAD_FLAG_INHIBITED = 23;   // oris 0x80   searching inhibited
    const u32 KU_HEAD_FLAG_VALID     = 28;   // oris 0x1000 valid, mirroring the winner

    // The camera-state CURRENT flags the VALID body touches (roles not recovered):
    // SetFlag(8) is raised each valid frame; IsFlagSet(1) gates staying valid.
    const u32 KU_STATE_FLAG_KEEP_GATE = 1;
    const u32 KU_STATE_FLAG_HARD_STOP = 8;

    // The debug-log colours (packed RGBA words the X360 passes literally).
    const u32 KU_LOG_COLOUR_RED    = 0xFFFF0000u;   // candidate-failed lines
    const u32 KU_LOG_COLOUR_ORANGE = 0xFF80FF00u;   // selection lines (asm lis -0x80 / ori 0xFF00)

    // The speed-diff eligibility threshold the X360 reads from .data @0x82FAA848
    // (initialised 0.0f in the image). FLAG: plausibly the parameter bank's loaded
    // MomentHardStop::Parameters::mfSpeedDiffThreshold instance (the compiler
    // folding the global bank member's address); modelled as the file-scope global
    // the asm reads until the bank's data flow is attested.
    f32 gfHardStopSpeedDiffThreshold = 0.0f;   // X360 .data @0x82FAA848
}

namespace detail
{
    // ---- MomentSharedInfo reaches (the record is un-homed; the Moment base
    // type-erases it to const void*). DECLARATION-ONLY named helpers per the
    // moment-family precedent (BrnMomentHitTraffic.cpp's detail reach); X360
    // shared-info offsets in comments; role names FLAG-inferred from the uses.
    // Replace with the real accessors when the MomentSharedInfo home lands. ----
    CgsNumeric::Random* MomentSharedInfo_GetRandom(const void* lpSharedInfo);        // +1272
    DebugLog*           MomentSharedInfo_GetDebugLog(const void* lpSharedInfo);      // +1276
    ShotSelector*       MomentSharedInfo_GetShotSelector(const void* lpSharedInfo);  // +1344

    // The live crash analysis (+1348, a pointer to the CrashAnalysis record).
    const CrashAnalysis* MomentSharedInfo_GetCrashAnalysis(const void* lpSharedInfo); // +1348

    // Player-state block (+1284) fields:
    bool MomentSharedInfo_IsCrashCameraBlocked(const void* lpSharedInfo);   // +1284 byte 449 (blocks eligibility)
    bool MomentSharedInfo_IsCrashCameraActive(const void* lpSharedInfo);    // +1284 byte 256 (gates alloc AND stay-valid)
    bool MomentSharedInfo_IsCrashReplayDisabled(const void* lpSharedInfo);  // +1284 byte 284 (blocks eligibility)
    s32  MomentSharedInfo_GetCrashModeWord(const void* lpSharedInfo);       // +1284 word +288 (-1/0/3 significant)
    bool MomentSharedInfo_IsPlayerCrashing(const void* lpSharedInfo);       // +1284 byte 249 (-> mbCrashingLastFrame)

    // Vehicle-dynamics block (+1292) fields:
    f32  MomentSharedInfo_GetCrashSpeedDiff(const void* lpSharedInfo);      // +1292 vec +512 lane 1 (the X360 vspltw/vcmpgtfp single-lane compare)
    bool MomentSharedInfo_HasCrashDynamics(const void* lpSharedInfo);       // +1292 word +488
    f32  MomentSharedInfo_GetCrashElapsed(const void* lpSharedInfo);        // +1292 f32 +480 (eligible while < 4s)

    bool MomentSharedInfo_GetFlag1318(const void* lpSharedInfo);            // +1318 byte (eligibility gate)
    bool MomentSharedInfo_GetFlag1319(const void* lpSharedInfo);            // +1319 byte (biases the A-shot pick)
    bool MomentSharedInfo_GetForceFlag1320(const void* lpSharedInfo);       // +1320 byte (forces eligibility)

    s32  MomentSharedInfo_GetCurrentCrashType(const void* lpSharedInfo);    // +1332 word +664 (VehicleTracker::ECrashType)

    // Block +1356 (a byte pair; at least one set for eligibility, the first also
    // biases the A-shot pick):
    bool MomentSharedInfo_GetCrashFlag36(const void* lpSharedInfo);         // +1356 byte +36
    bool MomentSharedInfo_GetCrashFlag37(const void* lpSharedInfo);         // +1356 byte +37

    // ---- Behaviour-handle resolvers (the BrnArbStateDriveThru.cpp precedent):
    // the de-inlined per-type BehaviourHandle accessors the X360 bl's. ----
    Camera::Behaviour*    sub_821FCDA8(void* lpHandle);   // live-behaviour resolver @0x821FCDA8
    const Camera::Camera* sub_821FCE10(void* lpHandle);   // produced-camera resolver @0x821FCE10

    // Dump a camera state's validity/flag account (the produced camera's NAMED
    // mState member) to the debug log (X360 @0x8223E6E0 walks that u64 bit set).
    // DECLARATION-ONLY foreign reach (the pretty-printer's own TU).
    void CameraState_AppendToDebugLog(const Camera::CameraState& lrState, DebugLog* lpDebugLog);

    // The iceanim class-key tag check the X360 makes on each selected shot (the
    // shot's leading u64 == Attrib::Gen::iceanim::ClassKey() 0x4644E379A997C1EE)
    // before raising the behaviour's +0xE28 byte -- the SAME byte the committed
    // BehaviourIceAnim::SetUseCollisionPolicy(true) writes (the shot data IS an
    // iceanim block, so the allocated behaviour honours its collision policy).
    // Both expressed as named helpers (the RefSpec record and the type-erased
    // behaviour interior are un-homed here). DECLARATION-ONLY.
    bool ShotReference_IsIceAnimClassKeyTagged(const Camera::Camera::ShotReference* lpShot);
    void Behaviour_SetUseCollisionPolicy(Camera::Behaviour* lpBehaviour);   // behaviour byte +0xE28 = 1
}
using namespace detail;

// @ 0x8225EC08 -- cpp:49. The inlined base Moment::Construct, both handle clears,
// both shot infos invalidated ({-1,-1}), and the counter/parameters/ready resets.
void MomentHardStop::Construct()
{
    Moment::Construct();         // inlined on the X360 (state/type/inhibit/camera)
    mRigCameraHandleA.Clear();   // the X360 zeroes the five handle fields inline
    mRigCameraHandleB.Clear();
    mShotASelectionInfo.miType = -1;
    mShotASelectionInfo.miId   = -1;
    mShotBSelectionInfo.miType = -1;
    mShotBSelectionInfo.miId   = -1;
    muHighEnergyWorldCrashCount = 0;
    muNextShotIndex             = 0;
    mpParameters                = 0;
    mbReady                     = false;
}

// @ 0x821F7518 -- cpp:71. Reset the per-run state and enter SEARCHING.
bool MomentHardStop::Prepare(void* /*lrBehaviourController*/)
{
    mfRunningTime       = 0.0f;
    mbReady             = false;
    mbCrashingLastFrame = false;
    mbFirstFrame        = true;
    SetState(E_STATE_INVALID_SEARCHING);
    return true;
}

// @ 0x821F7548 -- cpp:421. Adopt the tuning record (no type assert on the X360).
void MomentHardStop::SetParameters(const Moment::Parameters* lpParameters)
{
    mpParameters = static_cast<const Parameters*>(lpParameters);
}

// @ 0x821F7550 -- cpp:457.
const char* MomentHardStop::GetName() const
{
    return "MomentHardStop";
}

// @ 0x82271438 -- cpp:90. The per-frame hard-stop state machine:
//   SEARCHING        evaluate crash eligibility; on a hit, snapshot the crash
//                    analysis, select the preferred (A) and fallback (B) crash
//                    shots, allocate both rig behaviours, roll the ultra-slo-mo,
//                    and enter FOUND_PREPARING.
//   FOUND_PREPARING  log per-candidate failures; both failed -> Release (the
//                    virtual) and back to SEARCHING; a candidate switchable ->
//                    pick it (A preferred), release the loser, enter VALID and
//                    RUN THE VALID BODY THE SAME FRAME (the X360's goto into the
//                    state-3 body -- the switch falls through).
//   VALID            mirror the winner's produced camera + its shot-selection
//                    info + the crash analysis + the optional ultra-slo-mo
//                    sim-time scale; integrate the running time; drop back to
//                    SEARCHING when the keep conditions lapse.
// The epilogue always clears the first-frame latch and re-latches
// mbCrashingLastFrame from the shared info.
void MomentHardStop::Update(f32 lfTimeStep, void* lrBehaviourController,
                            const void* lSharedInfo)
{
    Camera::BehaviourManager* lpBehaviourManager =
        static_cast<Camera::BehaviourManager*>(lrBehaviourController);
    DebugLog* lpDebugLog = MomentSharedInfo_GetDebugLog(lSharedInfo);
    char lacLine[64];   // the X360's three 64-byte stack print buffers

    CGS_ASSERT(mpParameters != 0, "mpParameters != NULL");   // :92 (non-gating)

    switch (GetState())
    {
    case E_STATE_INVALID_SEARCHING:
    {
        // ---- eligibility (every gate must hold, unless the force flag is up) ----
        const CrashAnalysis* lpLiveAnalysis = MomentSharedInfo_GetCrashAnalysis(lSharedInfo);
        bool lbEligible = false;
        if ((lpLiveAnalysis->mxFlags & 0x20) != 0
            && !MomentSharedInfo_IsCrashCameraBlocked(lSharedInfo)
            && MomentSharedInfo_IsCrashCameraActive(lSharedInfo)
            && MomentSharedInfo_GetFlag1318(lSharedInfo)
            && (MomentSharedInfo_GetCrashFlag36(lSharedInfo)
                || MomentSharedInfo_GetCrashFlag37(lSharedInfo))
            && MomentSharedInfo_GetCrashSpeedDiff(lSharedInfo) > gfHardStopSpeedDiffThreshold
            && MomentSharedInfo_HasCrashDynamics(lSharedInfo)
            && MomentSharedInfo_GetCrashElapsed(lSharedInfo) < 4.0f
            && !MomentSharedInfo_IsCrashReplayDisabled(lSharedInfo))
        {
            lbEligible = true;
        }

        if (!lbEligible && !MomentSharedInfo_GetForceFlag1320(lSharedInfo))
        {
            SetConditionsNotMet();
            SetCanSwitchToMeNow(false);
            GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_SEARCHING);
            break;
        }
        if (IsInhibited())
        {
            SetCanSwitchToMeNow(false);
            GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_INHIBITED);
            break;
        }

        // ---- snapshot the crash + map it onto a shot group ----
        meCrashType = MomentSharedInfo_GetCurrentCrashType(lSharedInfo);
        ShotSelector::EGroup leShotGroup;
        switch (meCrashType)
        {
        case 1:  leShotGroup = ShotSelector::EGroup(0); break;
        case 2:  leShotGroup = ShotSelector::EGroup(1); break;
        case 3:  leShotGroup = ShotSelector::EGroup(2); break;
        default:
            CGS_ASSERT(false, "Unhandled crash type");   // :146 (non-gating)
            leShotGroup = ShotSelector::EGroup(1);
            break;
        }
        {
            const s32 liCrashModeWord = MomentSharedInfo_GetCrashModeWord(lSharedInfo);
            if ((liCrashModeWord == 3 || liCrashModeWord == 0)
                && MomentSharedInfo_GetCurrentCrashType(lSharedInfo) == 3)
            {
                leShotGroup = ShotSelector::EGroup(1);
            }
        }
        mCrashAnalysisUsedAtSelection = *lpLiveAnalysis;

        // ---- pick the two candidate shots (A biased -- the aftertouch set when
        //      the 1319/36 pair holds, else the crash side; B the generic
        //      property-4 fallback) ----
        u32 luPreferredA, luRequiredA;
        if (MomentSharedInfo_GetFlag1319(lSharedInfo)
            && MomentSharedInfo_GetCrashFlag36(lSharedInfo))
        {
            luPreferredA = 0;
            luRequiredA  = 4;
        }
        else
        {
            luPreferredA = mCrashAnalysisUsedAtSelection.mbUseLeftSide ? 1u : 2u;
            luRequiredA  = 0;
        }

        ShotSelector* lpShotSelector = MomentSharedInfo_GetShotSelector(lSharedInfo);
        const u32 luCrashEventFlags = mCrashAnalysisUsedAtSelection.mxFlags;
        Camera::Camera::ShotReference* lpShotA = lpShotSelector->GetCrashShot(
            luCrashEventFlags, &mShotASelectionInfo, luPreferredA, 0, luRequiredA, leShotGroup);
        Camera::Camera::ShotReference* lpShotB = lpShotSelector->GetCrashShot(
            luCrashEventFlags, &mShotBSelectionInfo, 0, 0, 4, leShotGroup);

        lpBehaviourManager->NewBehaviour<Camera::Behaviour>(mRigCameraHandleA, lpShotA, 0, this, 1);
        lpBehaviourManager->NewBehaviour<Camera::Behaviour>(mRigCameraHandleB, lpShotB, 0, this, 1);

        if (ShotReference_IsIceAnimClassKeyTagged(lpShotA))
            Behaviour_SetUseCollisionPolicy(sub_821FCDA8(&mRigCameraHandleA));
        if (ShotReference_IsIceAnimClassKeyTagged(lpShotB))
            Behaviour_SetUseCollisionPolicy(sub_821FCDA8(&mRigCameraHandleB));

        mbUsingA = false;
        SetCanSwitchToMeNow(false);
        mfRunningTime = 0.0f;
        SetState(E_STATE_INVALID_FOUND_PREPARING);
        GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_ALLOCATED);

        // ---- every KU_CRASHES_BEFORE_FORCE_ULTRA_SLOMOth high-energy world crash
        //      (type 3, mode -1, analysis bit 3) rolls a random ultra-slo-mo scale ----
        mbForceUltraSloMo = false;
        if (meCrashType == 3
            && MomentSharedInfo_GetCrashModeWord(lSharedInfo) == -1
            && (mCrashAnalysisUsedAtSelection.mxFlags & 8) != 0)
        {
            if (muHighEnergyWorldCrashCount % KU_CRASHES_BEFORE_FORCE_ULTRA_SLOMO == 0)
            {
                // The X360 inlines Random's LCG draw (the seed@+0x20 / ring@+0x28
                // AddRandomFloatToBuffer step + the [1,2)->range map) == the
                // committed RandomFloat(min, max).
                mfUltraSloMoTimestepScale = MomentSharedInfo_GetRandom(lSharedInfo)->RandomFloat(
                    KF_ULTRA_SLOMO_TIMESTEP_MIN_SCALE, KF_ULTRA_SLOMO_TIMESTEP_MAX_SCALE);
                mbForceUltraSloMo = true;
            }
            ++muHighEnergyWorldCrashCount;
        }

        // ---- the selection log line: "Hardstop: Front Rear Left Right  Use A/B side" ----
        {
            const u32 luFlags = mCrashAnalysisUsedAtSelection.mxFlags;
            CgsCore::SPrintf(lacLine, 64, "Hardstop: %s%s%s%s %s",
                             (luFlags & 0x40) ? "Front " : "",
                             (luFlags & 0x80) ? "Rear "  : "",
                             (luFlags & 0x02) ? "Left "  : "",
                             (luFlags & 0x04) ? "Right " : "",
                             mCrashAnalysisUsedAtSelection.mbUseLeftSide ? "Use L" : "Use R");
            lpDebugLog->Append(lacLine, KU_LOG_COLOUR_ORANGE);
        }
        break;
    }

    case E_STATE_INVALID_FOUND_PREPARING:
    {
        // ---- per-candidate failure logging (name + validity dump) ----
        if (sub_821FCDA8(&mRigCameraHandleA)->HasFailed())
        {
            Camera::Behaviour* lpBehaviourA = sub_821FCDA8(&mRigCameraHandleA);
            lpDebugLog->Append("Hardstop: Camera A failed", KU_LOG_COLOUR_RED);
            lpDebugLog->Append(lpBehaviourA->GetDebugParametersName(), 0xFFFFFFFFu);
            CameraState_AppendToDebugLog(sub_821FCE10(&mRigCameraHandleA)->mState, lpDebugLog);
        }
        if (sub_821FCDA8(&mRigCameraHandleB)->HasFailed())
        {
            Camera::Behaviour* lpBehaviourB = sub_821FCDA8(&mRigCameraHandleB);
            lpDebugLog->Append("Hardstop: Camera B failed", KU_LOG_COLOUR_RED);
            lpDebugLog->Append(lpBehaviourB->GetDebugParametersName(), 0xFFFFFFFFu);
            CameraState_AppendToDebugLog(sub_821FCE10(&mRigCameraHandleB)->mState, lpDebugLog);
        }

        if (sub_821FCDA8(&mRigCameraHandleA)->HasFailed()
            && sub_821FCDA8(&mRigCameraHandleB)->HasFailed())
        {
            Release();   // the live-vtable call (slot 4)
            SetState(E_STATE_INVALID_SEARCHING);
            break;
        }

        if (sub_821FCDA8(&mRigCameraHandleA)->CanSwitchToMeNow())
        {
            mbUsingA = true;
            SetState(E_STATE_VALID);
            CgsCore::SPrintf(lacLine, 64, "HardstopA: %s",
                             sub_821FCDA8(&mRigCameraHandleA)->GetDebugParametersName());
            lpDebugLog->Append(lacLine, KU_LOG_COLOUR_ORANGE);
            CgsCore::SPrintf(lacLine, 64, "HardstopB: %s",
                             sub_821FCDA8(&mRigCameraHandleB)->GetDebugParametersName());
            lpDebugLog->Append(lacLine, KU_LOG_COLOUR_ORANGE);
            mRigCameraHandleB.Release();
        }
        else if (sub_821FCDA8(&mRigCameraHandleB)->CanSwitchToMeNow())
        {
            mbUsingA = false;
            SetState(E_STATE_VALID);
            CgsCore::SPrintf(lacLine, 64, "HardstopA: %s",
                             sub_821FCDA8(&mRigCameraHandleA)->GetDebugParametersName());
            lpDebugLog->Append(lacLine, KU_LOG_COLOUR_ORANGE);
            CgsCore::SPrintf(lacLine, 64, "HardstopB: %s",
                             sub_821FCDA8(&mRigCameraHandleB)->GetDebugParametersName());
            lpDebugLog->Append(lacLine, KU_LOG_COLOUR_ORANGE);
            mRigCameraHandleA.Release();
        }
        else
        {
            SetCanSwitchToMeNow(false);
            GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_PREPARING);
            break;
        }
    }
        // fall through -- the X360 runs the VALID body the same frame it picks a winner

    case E_STATE_VALID:
    {
        SetCanSwitchFromMeNow(false);
        GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_VALID);

        // Mirror the winner (camera + its shot-selection info), attach the crash
        // analysis, raise the per-frame hard-stop state flag, and (when rolled)
        // force the ultra-slo-mo sim-time scale.
        if (mbUsingA)
        {
            SetCamera(*sub_821FCE10(&mRigCameraHandleA));
            GetNonConstCamera().mShotSelectionInfo = mShotASelectionInfo;
        }
        else
        {
            SetCamera(*sub_821FCE10(&mRigCameraHandleB));
            GetNonConstCamera().mShotSelectionInfo = mShotBSelectionInfo;
        }
        GetNonConstCamera().mState.SetFlag(KU_STATE_FLAG_HARD_STOP, true);
        GetNonConstCamera().mpCrashAnalysis = &mCrashAnalysisUsedAtSelection;
        if (mbForceUltraSloMo)
            GetNonConstCamera().GetEffects().mfSimTimeScale = mfUltraSloMoTimestepScale;

        // The active candidate's own switch-from gate.
        const bool lbActiveCanSwitchFrom =
            mbUsingA ? sub_821FCDA8(&mRigCameraHandleA)->CanSwitchFromMeNow()
                     : sub_821FCDA8(&mRigCameraHandleB)->CanSwitchFromMeNow();

        mfRunningTime += lfTimeStep;

        // A "clean early out": not crashing last frame, the active candidate
        // releasable, no crash in progress now, and the selected crash was type 1.
        const bool lbCleanEarlyOut =
            !mbCrashingLastFrame
            && lbActiveCanSwitchFrom
            && MomentSharedInfo_GetCurrentCrashType(lSharedInfo) == 0
            && meCrashType == 1;

        if (!GetNonConstCamera().mState.IsFlagSet(KU_STATE_FLAG_KEEP_GATE)
            || !MomentSharedInfo_IsCrashCameraActive(lSharedInfo)
            || (lbActiveCanSwitchFrom && meCrashType != 1)
            || lbCleanEarlyOut)
        {
            Release();   // the live-vtable call (slot 4)
            SetState(E_STATE_INVALID_SEARCHING);
        }
        break;
    }

    default:
        CGS_ASSERT(false, "unhandled case in switch");   // :384 (non-gating)
        break;
    }

    mbFirstFrame        = false;
    mbCrashingLastFrame = MomentSharedInfo_IsPlayerCrashing(lSharedInfo);
}

}
