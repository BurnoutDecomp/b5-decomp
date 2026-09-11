#include "GameSource/Director/MomentController/Moments/BrnMomentHardStop.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"        // Camera::Behaviour (the candidates' base accessors)
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.h"  // Camera::BehaviourIceAnim::SetUseCollisionPolicy
#include "GameShared/GameClasses/Core/CgsStringUtils.h"               // CgsCore::SPrintf
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                 // CgsNumeric::Random
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugPrinter.h"   // DebugLog
#include "GameSource/Director/Utils/BrnShotSelector.h"                // ShotSelector::GetCrashShot

// BrnDirector::MomentHardStop -- reconstructed from the console executable
// (home file BrnMomentHardStop.cpp; member/parameter names verbatim
// from the declarations).
//
// Bodied here (5 ledger functions):
//   Construct   Prepare   Update
//   SetParameters                     GetName

namespace BrnDirector
{

namespace
{
    // Read-only-data constants (values read from the console image):
    //   3        every 3rd high-energy world crash forces ultra slo-mo
    //   0.005f   ultra slo-mo timestep scale range MIN
    //   0.01f    ultra slo-mo timestep scale range MAX
    const u32 KU_CRASHES_BEFORE_FORCE_ULTRA_SLOMO = 3;
    const f32 KF_ULTRA_SLOMO_TIMESTEP_MIN_SCALE   = 0.005f;
    const f32 KF_ULTRA_SLOMO_TIMESTEP_MAX_SCALE   = 0.01f;

    // Camera-state head bits the moment raises (the moment family's shared per-bit
    // vocabulary -- BrnCameraState.h; bit roles not yet recovered):
    const u32 KU_HEAD_FLAG_SEARCHING = 18;   // searching, condition absent
    const u32 KU_HEAD_FLAG_ALLOCATED = 19;   // entering FOUND_PREPARING
    const u32 KU_HEAD_FLAG_PREPARING = 20;   // waiting on the candidates
    const u32 KU_HEAD_FLAG_INHIBITED = 23;   // searching inhibited
    const u32 KU_HEAD_FLAG_VALID     = 28;   // valid, mirroring the winner

    // The camera-state CURRENT flags the VALID body touches (roles not recovered):
    // SetFlag(8) is raised each valid frame; IsFlagSet(1) gates staying valid.
    const u32 KU_STATE_FLAG_KEEP_GATE = 1;
    const u32 KU_STATE_FLAG_HARD_STOP = 8;

    // The debug-log colours (packed RGBA words the console build passes literally).
    const u32 KU_LOG_COLOUR_RED    = 0xFFFF0000u;   // candidate-failed lines
    const u32 KU_LOG_COLOUR_ORANGE = 0xFF80FF00u;   // selection lines

    // The speed-diff eligibility threshold the console build reads from its data segment
    // (initialised 0.0f in the image). FLAG: plausibly the parameter bank's loaded
    // MomentHardStop::Parameters::mfSpeedDiffThreshold instance (the compiler
    // folding the global bank member's address); modelled as the file-scope global
    // the console code reads until the bank's data flow is attested.
    f32 gfHardStopSpeedDiffThreshold = 0.0f;   // console data segment
}

namespace detail
{
    // ---- MomentSharedInfo reaches (the record is un-homed; the Moment base
    // type-erases it to const void*). DECLARATION-ONLY named helpers per the
    // moment-family precedent (BrnMomentHitTraffic.cpp's detail reach); console
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
    f32  MomentSharedInfo_GetCrashSpeedDiff(const void* lpSharedInfo);      // +1292 vec +512 lane 1 (the console build compares that one lane)
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

    // ⛔ THE TWO BEHAVIOUR-HANDLE RESOLVER SHIMS ARE GONE. They stood here declared and never
    // defined -- the same defect BrnArbStateDriveThru.cpp retired, and for the same reason: they
    // are the de-inlined per-instantiation copies of two accessors this tree already owns, and
    // this class's handles are ALREADY the canonical Camera::BehaviourHandle<Camera::Behaviour>,
    // so the accessors were reachable by name the whole time. Read against that precedent, the
    // live-behaviour resolver is BehaviourHandle::GetBehaviour() and the produced-camera resolver
    // is BehaviourHandle::GetProducedCamera(); the mounted sibling states (ArbStateDriveThru /
    // ArbStateCrashing / ArbStateTakedown) already call both by name. Every call site below now
    // does the same, which cost the link nothing and closed two unresolved externals.

    // ⛔ THE DEBUG-LOG DUMP SHIM IS GONE. It stood declared and never defined because the
    // reason-name table the dump walks had not been recovered. It has been, so the operation is
    // the account's own ValidityAccount::Print(DebugLog&), bodied in BrnCameraValidityAccount.cpp
    // -- and the thing the console hands it is the produced camera's validity account, which the
    // camera publishes by name as GetValidityAccount().

    // The iceanim class-key tag check the console makes on each selected shot, before
    // raising the behaviour's collision-policy flag. Bodied in BrnMomentSharedInfo.cpp
    // beside the family's other ICE reaches; DECLARATION-ONLY here.
    //
    // ⛔ ITS PARTNER SHIM Behaviour_SetUseCollisionPolicy IS GONE. It stood declared and never
    // defined because this TU was believed unable to see BrnBehaviourIceAnim.h alongside
    // BehaviourRig.h's Behaviour slice. MEASURED 2026-09-11: the two headers compile together
    // cleanly, so the flag is reachable by name. The tag check above is exactly what licenses
    // the downcast -- it proves the shot the behaviour was built from IS an iceanim block --
    // and every mounted arbitrator state that raises the same flag calls
    // BehaviourIceAnim::SetUseCollisionPolicy(true) by name too.
    bool ShotReference_IsIceAnimClassKeyTagged(const Camera::Camera::ShotReference* lpShot);
}
using namespace detail;

// The inlined base Moment::Construct, both handle clears,
// both shot infos invalidated ({-1,-1}), and the counter/parameters/ready resets.
void MomentHardStop::Construct()
{
    Moment::Construct();         // inlined in the console build (state/type/inhibit/camera)
    mRigCameraHandleA.Clear();   // the console build zeroes the five handle fields inline
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

// Reset the per-run state and enter SEARCHING.
bool MomentHardStop::Prepare(void* /*lrBehaviourController*/)
{
    mfRunningTime       = 0.0f;
    mbReady             = false;
    mbCrashingLastFrame = false;
    mbFirstFrame        = true;
    SetState(E_STATE_INVALID_SEARCHING);
    return true;
}

// Adopt the tuning record (no type assert in the console build).
void MomentHardStop::SetParameters(const Moment::Parameters* lpParameters)
{
    mpParameters = static_cast<const Parameters*>(lpParameters);
}

const char* MomentHardStop::GetName() const
{
    return "MomentHardStop";
}

// The per-frame hard-stop state machine:
//   SEARCHING        evaluate crash eligibility; on a hit, snapshot the crash
//                    analysis, select the preferred (A) and fallback (B) crash
//                    shots, allocate both rig behaviours, roll the ultra-slo-mo,
//                    and enter FOUND_PREPARING.
//   FOUND_PREPARING  log per-candidate failures; both failed -> Release (the
//                    virtual) and back to SEARCHING; a candidate switchable
//                    pick it (A preferred), release the loser, enter VALID and
//                    RUN THE VALID BODY THE SAME FRAME (the console build's goto into the
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
    char lacLine[64];   // the console build's three 64-byte stack print buffers

    CGS_ASSERT(mpParameters != 0, "mpParameters != NULL");   //  (non-gating)

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
            CGS_ASSERT(false, "Unhandled crash type");   //  (non-gating)
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
            static_cast<Camera::BehaviourIceAnim*>(mRigCameraHandleA.GetBehaviour())
                ->SetUseCollisionPolicy(true);
        if (ShotReference_IsIceAnimClassKeyTagged(lpShotB))
            static_cast<Camera::BehaviourIceAnim*>(mRigCameraHandleB.GetBehaviour())
                ->SetUseCollisionPolicy(true);

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
                // The console inlines Random's LCG draw (the seed@+0x20 / ring@+0x28
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
        if (mRigCameraHandleA.GetBehaviour()->HasFailed())
        {
            Camera::Behaviour* lpBehaviourA = mRigCameraHandleA.GetBehaviour();
            lpDebugLog->Append("Hardstop: Camera A failed", KU_LOG_COLOUR_RED);
            lpDebugLog->Append(lpBehaviourA->GetDebugParametersName(), 0xFFFFFFFFu);
            mRigCameraHandleA.GetProducedCamera().GetValidityAccount().Print(*lpDebugLog);
        }
        if (mRigCameraHandleB.GetBehaviour()->HasFailed())
        {
            Camera::Behaviour* lpBehaviourB = mRigCameraHandleB.GetBehaviour();
            lpDebugLog->Append("Hardstop: Camera B failed", KU_LOG_COLOUR_RED);
            lpDebugLog->Append(lpBehaviourB->GetDebugParametersName(), 0xFFFFFFFFu);
            mRigCameraHandleB.GetProducedCamera().GetValidityAccount().Print(*lpDebugLog);
        }

        if (mRigCameraHandleA.GetBehaviour()->HasFailed()
            && mRigCameraHandleB.GetBehaviour()->HasFailed())
        {
            Release();   // the live-vtable call (slot 4)
            SetState(E_STATE_INVALID_SEARCHING);
            break;
        }

        if (mRigCameraHandleA.GetBehaviour()->CanSwitchToMeNow())
        {
            mbUsingA = true;
            SetState(E_STATE_VALID);
            CgsCore::SPrintf(lacLine, 64, "HardstopA: %s",
                             mRigCameraHandleA.GetBehaviour()->GetDebugParametersName());
            lpDebugLog->Append(lacLine, KU_LOG_COLOUR_ORANGE);
            CgsCore::SPrintf(lacLine, 64, "HardstopB: %s",
                             mRigCameraHandleB.GetBehaviour()->GetDebugParametersName());
            lpDebugLog->Append(lacLine, KU_LOG_COLOUR_ORANGE);
            mRigCameraHandleB.Release();
        }
        else if (mRigCameraHandleB.GetBehaviour()->CanSwitchToMeNow())
        {
            mbUsingA = false;
            SetState(E_STATE_VALID);
            CgsCore::SPrintf(lacLine, 64, "HardstopA: %s",
                             mRigCameraHandleA.GetBehaviour()->GetDebugParametersName());
            lpDebugLog->Append(lacLine, KU_LOG_COLOUR_ORANGE);
            CgsCore::SPrintf(lacLine, 64, "HardstopB: %s",
                             mRigCameraHandleB.GetBehaviour()->GetDebugParametersName());
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
        // fall through -- the console build runs the VALID body the same frame it picks a winner

    case E_STATE_VALID:
    {
        SetCanSwitchFromMeNow(false);
        GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_VALID);

        // Mirror the winner (camera + its shot-selection info), attach the crash
        // analysis, raise the per-frame hard-stop state flag, and (when rolled)
        // force the ultra-slo-mo sim-time scale.
        if (mbUsingA)
        {
            SetCamera(mRigCameraHandleA.GetProducedCamera());
            GetNonConstCamera().mShotSelectionInfo = mShotASelectionInfo;
        }
        else
        {
            SetCamera(mRigCameraHandleB.GetProducedCamera());
            GetNonConstCamera().mShotSelectionInfo = mShotBSelectionInfo;
        }
        GetNonConstCamera().mState.SetFlag(KU_STATE_FLAG_HARD_STOP, true);
        GetNonConstCamera().mpCrashAnalysis = &mCrashAnalysisUsedAtSelection;
        if (mbForceUltraSloMo)
            GetNonConstCamera().GetEffects().mfSimTimeScale = mfUltraSloMoTimestepScale;

        // The active candidate's own switch-from gate.
        const bool lbActiveCanSwitchFrom =
            mbUsingA ? mRigCameraHandleA.GetBehaviour()->CanSwitchFromMeNow()
                     : mRigCameraHandleB.GetBehaviour()->CanSwitchFromMeNow();

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
        CGS_ASSERT(false, "unhandled case in switch");   //  (non-gating)
        break;
    }

    mbFirstFrame        = false;
    mbCrashingLastFrame = MomentSharedInfo_IsPlayerCrashing(lSharedInfo);
}

}
