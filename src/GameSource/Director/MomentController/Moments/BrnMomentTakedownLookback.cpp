#include "GameSource/Director/MomentController/Moments/BrnMomentTakedownLookback.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT
#include "BrnCommonTypes.h"                                           // Vector3
#include "GameSource/BurnoutConstants.h"                              // EActiveRaceCarIndex
#include "GameSource/Director/Camera/Camera.h"                        // Camera::Camera (SetCamera / operator=)
#include "GameSource/Director/Camera/Utils/BrnConsoleVpu.h"           // ConsoleVpu:: the SDK inlines, rounded as the console
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h"  // GameState::mbTakedownActive / meTakedownVictimID
#include "GameSource/Director/MomentController/BrnMomentSharedInfo.h" // MomentSharedInfo (read by name)
#include "rw/math/vpu/vector3_operation.h"                            // Vector3 operator-

// ============================================================================
// GameSource/Director/MomentController/Moments/BrnMomentTakedownLookback.cpp
//
// BrnDirector::MomentTakedownLookback -- the "takedown look-back" moment: after the player takes a rival down, a rig
// camera on the player's car looks back at the wreck while the victim is behind and within 60 m.
//
// Bodied here: Construct @0x8225EDB8, Update @0x822662F0, Release @0x8223AA08, GetName @0x821F75C0 and the four
// vtable one-liners at the end (vtable off_82008CD8).
//
// [FX-DIRECTOR2 2026-09-25] Update RE-DERIVED from the X360, with the PS3 build's own body (which names the preset):
//   * the record is read BY NAME -- the six detail:: reach shims it went through are retired (BrnMomentSharedInfo.cpp),
//     and the three that never had a body (the victim's two lanes and the rig's look-at helper) are gone;
//   * the look-back rig's block was a 64-byte ZERO placeholder copied over the block's rig preset; it is
//     CameraRig::ParamsBonnetLow (the 8-doubleword copy from 0x82CDAA10, slot 8 of the preset table) plus
//     mbUseShake = true (`stb 1, 0x2A9` == block +0x119);
//   * the rig is aimed with the header inline BehaviourRig::StartLookingAtRaceCar(1, true) -- race car ONE, a
//     literal in the console (`stw 1, 0x454`), not the victim's index;
//   * the geometry reads the DWARF locals (BrnMomentTakedownLookback.cpp:87..:99 / :130..:142) and rounds as the
//     console does (the campaign rounding rule): three vmsum3fp128 dots (rule 1), NormalizeFast's vrsqrtefp + one
//     Newton step and the VecFloat divide's vrefp + two (rule 5, with their fused vnmsubfp / vmaddfp, rule 3).
// ============================================================================

namespace BrnDirector
{

namespace
{
    const f32 KF_ZERO                     = 0.0f;    // flt_82001CC0 (the behind / band near-edge tests)
    const f32 KF_ONE                      = 1.0f;    // flt_82001C98 (the band far edge: one second)
    const f32 KF_MAX_DISTANCE_TO_VICTIM   = 60.0f;   // flt_82004C6C == 0x42700000 (the PS3 splats 0x42700000 too)

    // The moment camera's head bits (the family's shared vocabulary; `oris r11, r11, 4` / `oris r11, r11, 0x80` on
    // the camera state's head word at moment +0x148).
    const u32 KU_HEAD_FLAG_SEARCHING = 18;
    const u32 KU_HEAD_FLAG_INHIBITED = 23;
}

// ============================================================================
// Construct @0x8225EDB8 (DWARF BrnMomentTakedownLookback.cpp:34)
//   meState = 0, meType = GetInstanceType() (vtable +0x1C), mbIsInhibited = 0, Camera::Construct(+0x10) -- the
//   inlined Moment::Construct; the rig handle's five words zeroed (+0x2B0..+0x2C0); BehaviourRig::Parameters::
//   Construct(+0x190); the victim ref's set byte (+0x2D0) = 0; mpParameters (+0x180) = 0.
// ============================================================================
void MomentTakedownLookback::Construct()
{
    Moment::Construct();
    mRigCameraHandle.Clear();
    mLookbackRigParams.Construct();
    mVictim.mbSet = false;
    mpParameters  = 0;
}

// ============================================================================
// GetName @0x821F75C0 (DWARF :229)
// ============================================================================
const char* MomentTakedownLookback::GetName() const
{
    return "MomentTakedownLookback";
}

// ============================================================================
// Release @0x8223AA08 (DWARF :171)
//   if the rig handle is allocated: BehaviourManager::UnSetBehaviourUsedByHandle, then the handle cleared; then
//   mbConditionsMet (+0x17A) = 0, mbCanSwitchToMeNow (+0x178) = 0, head bit 18, meState = 0 (INACTIVE); return 1.
// ============================================================================
bool MomentTakedownLookback::Release()
{
    mRigCameraHandle.Release();
    SetConditionsNotMet();
    SetCanSwitchToMeNow(false);
    GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_SEARCHING);
    SetState(E_STATE_INVALID_INACTIVE);
    return true;
}

// ============================================================================
// Update @0x822662F0 (DWARF :72)
//
//   0x82266320  assert "mpParameters != NULL" (:74)
//   0x82266350  switch (meState): 1 -> SEARCHING, 3 -> VALID, else assert "unhandled case in switch" (:158)
//
//   SEARCHING (0x82266494)
//     no takedown this frame (GameState::mbTakedownActive, +0xDA): conditions not met, cannot switch to me, head
//     bit 18 (0x822664A8..0x822664B8)
//     else: mVictim.SetToRaceCar(GameState::meTakedownVictimID) (out of line, 0x821F29D8) and, from the player's
//     snapshot and the victim's record (resolved twice, 0x822664FC / 0x82266518):
//       lToVictim                  = victim pos - player pos                        vsubfp128
//       lDistanceToVictim          = Dot(NormalizeFast(lToVictim), lToVictim)       |lToVictim|
//       lZDistanceToVictim         = Dot(player z, lToVictim)                       > 0 when the victim is ahead
//       lNetVictimVel              = victim velocity - player velocity              vsubfp
//       lTimeToVictimOvertakingUs  = -lZDistanceToVictim / Dot(player z, lNetVictimVel)
//     the shot is on when the victim is NOT about to draw level within a second (t < 0 or t > 1), is behind
//     (lZDistanceToVictim < 0) and is within 60 m -- each a vcmpgtfp. (an ordered compare: a NaN fails it);
//     otherwise not met, as above.
//     On: inhibited (+0x17B) -> cannot switch to me + head bit 23 (0x82266720), and nothing else. Not inhibited ->
//       NewBehaviour<BehaviourRig>(the rig handle, 0, this, 1)                    0x822666AC
//       the block's rig = CameraRig::ParamsBonnetLow (8 doublewords from 0x82CDAA10); mbUseShake = 1
//       rig->SetParameters(&block) (out of line, 0x821F3B10)
//       rig->StartLookingAtRaceCar(E_ACTIVE_RACE_CAR_INDEX_1, true) -- the six inlined stores 0x82266700..0x82266714
//       meState = VALID (3)
//
//   VALID (0x82266384)
//     the same player / victim lanes (the victim resolved twice again, the second result unused, 0x822663B4);
//     held while the victim is behind and within 60 m: the moment's camera = the rig's produced camera
//     (Camera::operator=, 0x82266480); otherwise back to SEARCHING (`stw 1, 0x174`, nothing else).
// ============================================================================
void MomentTakedownLookback::Update(f32 /*lfTimeStep*/, void* lrBehaviourController, const void* lSharedInfo)
{
    namespace ConsoleVpu = Camera::Utils::ConsoleVpu;

    Camera::BehaviourManager& lrBehaviourManager = *static_cast<Camera::BehaviourManager*>(lrBehaviourController);
    const MomentSharedInfo&   lrSharedInfo       = *static_cast<const MomentSharedInfo*>(lSharedInfo);

    CGS_ASSERT(mpParameters != 0, "mpParameters != NULL");   // :74

    switch (GetState())
    {
    case E_STATE_INVALID_SEARCHING:
    {
        if (lrSharedInfo.mpGameState->mbTakedownActive)
        {
            mVictim.SetToRaceCar(lrSharedInfo.mpGameState->meTakedownVictimID);

            const Vector3 lPlayerZ   = lrSharedInfo.mPlayerInfo.mRaceCarState.mTransform.zAxis;             // :87
            const Vector3 lVictimPos = mVictim.GetVehicle(lrSharedInfo).mRaceCarState.mTransform.wAxis;     // :88
            const Vector3 lPlayerPos = lrSharedInfo.mPlayerInfo.mRaceCarState.mTransform.wAxis;             // :89
            const Vector3 lVictimVel = mVictim.GetVehicle(lrSharedInfo).mRaceCarState.mLinearVelocity;      // :90
            const Vector3 lPlayerVel = lrSharedInfo.mPlayerInfo.mRaceCarState.mLinearVelocity;              // :91

            const Vector3 lToVictim           = lVictimPos - lPlayerPos;                                    // :93
            const Vector3 lToVictimNormalized = ConsoleVpu::NormalizeFast(lToVictim);                       // :94
            const f32     lfDistanceToVictim  = ConsoleVpu::Dot3(lToVictimNormalized, lToVictim);           // :95
            const f32     lfZDistanceToVictim = ConsoleVpu::Dot3(lPlayerZ, lToVictim);                      // :96

            const Vector3 lNetVictimVel = lVictimVel - lPlayerVel;                                          // :98
            const f32     lfTimeToVictimOvertakingUs =                                                      // :99
                ConsoleVpu::Divide(-lfZDistanceToVictim, ConsoleVpu::Dot3(lPlayerZ, lNetVictimVel));

            if (((KF_ZERO > lfTimeToVictimOvertakingUs) || (lfTimeToVictimOvertakingUs > KF_ONE))
                && (KF_ZERO > lfZDistanceToVictim)
                && (KF_MAX_DISTANCE_TO_VICTIM > lfDistanceToVictim))
            {
                if (!IsInhibited())
                {
                    lrBehaviourManager.NewBehaviour<Camera::BehaviourRig>(mRigCameraHandle, 0, this, 1);

                    mLookbackRigParams.mRigParams  = Camera::Utils::CameraRig::ParamsBonnetLow;
                    mLookbackRigParams.mbUseShake  = true;
                    mRigCameraHandle.GetBehaviour()->SetParameters(&mLookbackRigParams);
                    mRigCameraHandle.GetBehaviour()->StartLookingAtRaceCar(E_ACTIVE_RACE_CAR_INDEX_1, true);

                    SetState(E_STATE_VALID);
                }
                else
                {
                    SetCanSwitchToMeNow(false);
                    GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_INHIBITED);
                }
                break;
            }
        }

        SetConditionsNotMet();
        SetCanSwitchToMeNow(false);
        GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_SEARCHING);
        break;
    }

    case E_STATE_VALID:
    {
        const Vector3 lPlayerZ   = lrSharedInfo.mPlayerInfo.mRaceCarState.mTransform.zAxis;                 // :130
        const Vector3 lVictimPos = mVictim.GetVehicle(lrSharedInfo).mRaceCarState.mTransform.wAxis;         // :131
        const Vector3 lPlayerPos = lrSharedInfo.mPlayerInfo.mRaceCarState.mTransform.wAxis;                 // :132
        // :133 lVictimVel -- the victim is resolved a second time (0x822663B4) and the lane is never read: the
        // console keeps the call (its tripwires) and drops the load.
        mVictim.GetVehicle(lrSharedInfo);

        const Vector3 lToVictim           = lVictimPos - lPlayerPos;                                        // :136
        const Vector3 lToVictimNormalized = ConsoleVpu::NormalizeFast(lToVictim);                           // :137
        const f32     lfDistanceToVictim  = ConsoleVpu::Dot3(lToVictimNormalized, lToVictim);               // :138
        const f32     lfZDistanceToVictim = ConsoleVpu::Dot3(lPlayerZ, lToVictim);                          // :139

        if ((KF_ZERO > lfZDistanceToVictim) && (KF_MAX_DISTANCE_TO_VICTIM > lfDistanceToVictim))
        {
            SetCamera(mRigCameraHandle.GetProducedCamera());
        }
        else
        {
            SetState(E_STATE_INVALID_SEARCHING);
        }
        break;
    }

    default:
        CGS_ASSERT(false, "unhandled case in switch");   // :158
        break;
    }
}

// ============================================================================
// The vtable one-liners (off_82008CD8; AllocateVoid<MomentTakedownLookback> @0x8224B5D0 stores it at +0), and the
// ICF-folded slot bodies they point at:
//   slot 1 Prepare          0x821F7560  meState = E_STATE_INVALID_SEARCHING, return true (the shared body the export
//                                       names MomentBystanderSeesAction::Prepare)
//   slot 5 Destruct         0x8284CB38  `blr` (the one empty body all twelve moments share)
//   slot 3 SetParameters    0x821F7670  `stw r4, 0x180(r3)` -- mpParameters
//   slot 7 GetInstanceType  0x826D7F68  `li r3, 3 ; blr` -- E_MOMENT_TAKEDOWN_LOOKBACK
// [FX-DIRECTOR 2026-09-24]
// ============================================================================
bool MomentTakedownLookback::Prepare(void* /*lrBehaviourController*/)
{
    SetState(E_STATE_INVALID_SEARCHING);   // li r10, 1 ; stw r10, 0x174(r3)
    return true;                           // li r3, 1
}

void MomentTakedownLookback::Destruct()
{
    // 0x8284CB38 is a lone `blr`: nothing to tear down.
}

void MomentTakedownLookback::SetParameters(const Moment::Parameters* lpParameters)
{
    mpParameters = static_cast<const Parameters*>(lpParameters);   // stw r4, 0x180(r3)
}

Moment::EType MomentTakedownLookback::GetInstanceType()
{
    return E_MOMENT_TAKEDOWN_LOOKBACK;   // li r3, 3
}

}
