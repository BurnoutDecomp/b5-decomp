#include "GameSource/GameState/TakedownManager/BrnTakedownManager.h"

#include <cmath>                                                        // std::asin (the de-SIMD'd XMVectorASin)

#include "rw/math/vpu/vector3_operation.h"                              // rw::math::vpu::Cross / Magnitude

#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // GameActionQueue::AddEvent / CgsModule::Event
#include "GameSource/GameState/BrnGameStateModuleIO.h"                  // OutputBuffer::GetGameActionQueue
#include "GameSource/GameState/BrnGameActions.h"                        // the five takedown records + their ids
#include "GameSource/GameState/BrnGameStateSharedIO.h"                  // GameStateModuleIO::E_MODE_ROAD_RAGE
#include "GameSource/GameState/ModeManager/BrnModeManager.h"            // ModeManager::GetCurrentGameMode / GetCurrentGameModeType / IsOnlineGameMode
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h" // BrnPhysics::Vehicle::RaceCarState (+ the embedded Slam/ShuntEffect)
#include "GameSource/World/BrnWorldSharedConstants.h"                   // BrnWorld::E_CAR_CONTROL_*
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // the active race-car accessors

// ============================================================================================
// BrnGameState::TakedownManager -- lifecycle / timers / camera / player reset.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (addresses on each body). The other half of the
// TU -- Update and the detect / process chain -- lives in BrnTakedownManager_Detect.cpp against
// the same (frozen) header.
//
// Units: every speed the classifier compares is METRES PER SECOND -- the console multiplies
// RaceCarState::mfSpeedMPH by flt_82F31928 (0.44704, mph -> m/s) before every compare, and the
// four speed tunables are STATIC-INITIALISED from an mph literal times that same word (see the
// constants block). The X360 image holds zero in those .data words, which is why they cannot be
// read straight out of it; the initialiser bodies at 0x82C4D7A0..0x82C4D81C are the evidence.
// ============================================================================================

namespace BrnGameState
{
    // -----------------------------------------------------------------------------------------
    // The 19 class-static tunables (DWARF BrnTakedownManager.h:203-227). Each value is the pool
    // word (or static-initialiser product) its X360 READER loads; "no reader" ones are left
    // declared-only, deliberately -- an unattested value would be an invention.
    // -----------------------------------------------------------------------------------------
    namespace
    {
        // flt_82F31928 -- the console's mph -> m/s factor, loaded by every speed compare in this TU
        // (UpdatePlayerResetStatus @0x82388B90, ProcessQueuedTakedowns @0x82399BE0, the static
        // initialisers below). TrafficPhysics.h carries the same word as its own KF_MPH_TO_MPS.
        const f32 KF_MPH_TO_METRES_PER_SECOND = 0.447039992f;
    }

    // StartTakedownCamera @0x82388EB4..EB8: PlayerInvulnerable time = camera time + flt_8202AEB8 (2.5)
    const f32 TakedownManager::KF_POST_TAKEDOWN_INVULNERABLE_TIME       = 2.5f;      // 0x8202AEB8
    // UpdateTakedownTimes @0x823660F8/0x823661C4: miMultipleTakedownLength resets past flt_82001C98 (1.0)
    const f32 TakedownManager::KF_DOUBLE_TAKEDOWN_TIME_LIMIT            = 1.0f;      // 0x82001C98
    // IsBeingAttacked @0x8236F14C (offline) / @0x8236F174 (online): the contact window
    const f32 TakedownManager::KF_TAKEDOWN_TIME_CONTACT                 = 2.0f;      // 0x82001D9C
    const f32 TakedownManager::KF_TAKEDOWN_TIME_CONTACT_ONLINE          = 4.0f;      // 0x8202AEAC
    // KF_TAKEDOWN_IGNORE_TIME (DWARF :209): NO X360 READER -- every ignore-time compare in the image
    // (DetectInstantTakedown @0x82399638, DetectNetworkTakedowns @0x823997A0) is on the ONLINE arm.
    // Left declared-only; do not seed it.
    // DetectInstantTakedown @0x82399638 / DetectNetworkTakedowns @0x823997A0: mfTimeSinceLastTakenDown
    // vs flt_82CDBDBC (5.05) on the online arm
    const f32 TakedownManager::KF_ONLINE_TAKEDOWN_IGNORE_TIME           = 5.05f;     // 0x82CDBDBC
    // ProcessQueuedTakedowns @0x82399A98: the wait after the victim crashed, online ? 1.0 : 0.1
    const f32 TakedownManager::KF_TAKEDOWN_CONFIRMATION_TIME            = 0.1f;      // 0x82004014
    const f32 TakedownManager::KF_ONLINE_TAKEDOWN_CONFIRMATION_TIME     = 1.0f;      // 0x82001C98
    // The reset-speed trio: .data words zero in the image, written by the static initialisers
    //   0x82C4D7A0  flt_82FADF10 = flt_82F31928 * flt_82029F30 (30.0)   read @0x82388C90
    //   0x82C4D7C0  flt_82FADF14 = flt_82F31928 * flt_82004D0C (40.0)   read @0x82388C78
    //   0x82C4D7E0  flt_82FAE05C = flt_82F31928 * flt_82013FBC (140.0)  read @0x82388D90
    const f32 TakedownManager::KF_SPEED_DROP_FOR_PLAYER_RESET           = 30.0f  * KF_MPH_TO_METRES_PER_SECOND;   // 0x82FADF10
    const f32 TakedownManager::KF_MIN_SPEED_FOR_PLAYER_RESET            = 40.0f  * KF_MPH_TO_METRES_PER_SECOND;   // 0x82FADF14
    const f32 TakedownManager::KF_MAX_SPEED_FOR_PLAYER_RESET            = 140.0f * KF_MPH_TO_METRES_PER_SECOND;   // 0x82FAE05C
    // UpdatePlayerResetStatus @0x82388CD4: the tilt angle vs flt_82CDBDC0 (0x3F490FDB == pi/4)
    const f32 TakedownManager::KF_MIN_ANGLE_FOR_PLAYER_RESET            = 0.785398185f; // 0x82CDBDC0
    // UpdatePlayerResetStatus @0x82388D00: |angular velocity| vs flt_8202AEB8 (2.5)
    const f32 TakedownManager::KF_MAX_ANGULAR_VELOCITY_FOR_PLAYER_RESET = 2.5f;      // 0x8202AEB8
    // UpdatePlayerResetStatus @0x82388CC0: mfTimeWithWheelsOffGround vs flt_82001DA0 (0.5)
    const f32 TakedownManager::KF_MIN_TIME_IN_AIR_FOR_PLAYER_RESET      = 0.5f;      // 0x82001DA0
    // UpdatePlayerResetStatus @0x82388C4C `cmpwi r30, 2`
    const s32 TakedownManager::KI_WHEELS_OFF_GROUND_FOR_PLAYER_RESET    = 2;
    // UpdateTakedownTimes @0x823661A8: miTakedownChainLength resets past flt_8202AEC4 (15.0)
    const f32 TakedownManager::KF_TAKEDOWN_CHAIN_TIMEOUT_SECONDS        = 15.0f;     // 0x8202AEC4
    // flt_820037C8 -- the "never" timer value (RaceCarData::Clear, UpdateTakedownTimes, the camera timer)
    const f32 TakedownManager::KF_INVALID_TIME                          = -1.0f;     // 0x820037C8
    // ProcessQueuedTakedowns @0x82399BE0 reads flt_82FADECC; static initialiser @0x82C4D800:
    //   flt_82FADECC = flt_82F31928 * flt_820138DC (50.0)
    const f32 TakedownManager::KF_MIN_TAKEDOWN_SPEED                    = 50.0f  * KF_MPH_TO_METRES_PER_SECOND;   // 0x82FADECC
    // KF_FRONT_CONTACT_TOLERANCE (DWARF :227): NO X360 READER anywhere in the 22 bodies. Declared-only.

    // -----------------------------------------------------------------------------------------
    // The TU-local tunables (DWARF BrnTakedownManager.cpp:30-43). Internal linkage so the other
    // half of the TU can carry its own without a duplicate-definition clash.
    // -----------------------------------------------------------------------------------------
    namespace
    {
        // The mode-dependent camera duration (StartTakedownCamera @0x82388E60..E9C, UpdateTakedownCamera
        // @0x823890E4..8391C): road rage -> 2.0; any other current mode -> 1.425; no current mode -> 1.9.
        // ROAD_RAGE is pinned by the `== 3` test; NORMAL by the reset window below (0.35625 == 0.25 *
        // 1.425); PURSUIT is the remaining DWARF name for the remaining value (the no-mode arm).
        const f32 KF_NORMAL_TIME_IN_TAKEDOWN_CAM       = 1.425f;   // 0x8202AE84
        const f32 KF_ROAD_RAGE_TIME_IN_TAKEDOWN_CAM    = 2.0f;     // 0x82001D9C
        const f32 KF_PURSUIT_TIME_IN_TAKEDOWN_CAM      = 1.9f;     // 0x8202AE8C
        // IsAllowedToResetPlayer @0x8235968C reads flt_82CDBDB8 = 0.35625 (0x3EB66666) -- the folded
        // product 0.25 * 1.425 -- and @0x8235969C reads flt_82FAE054, which the static initialiser
        // @0x82C4D780 computes as flt_8202AE84 (1.425) - flt_82CDBDB8 (0.35625) = 1.06875.
        const f32 KF_RESET_PROTECTION_TIME_PROPORTION  = 0.25f;
        const f32 KF_MIN_NORMAL_RESET_TIME             = KF_RESET_PROTECTION_TIME_PROPORTION * KF_NORMAL_TIME_IN_TAKEDOWN_CAM;   // 0x82CDBDB8
        const f32 KF_MAX_NORMAL_RESET_TIME             = KF_NORMAL_TIME_IN_TAKEDOWN_CAM - KF_MIN_NORMAL_RESET_TIME;             // 0x82FAE054
        // KF_PLAYER_CONTROL_RETURN_DELAY_TIME (cpp:38) is read by Update @0x8239FAC0 (the other half).
        // UpdateTakedownCamera @0x823891A0/0x823891C8 (1.0), @0x823891D4 (0.2)
        const f32 KF_TAKEDOWN_CAMERA_EARLY_OUT_MIN_TIME = 1.0f;    // 0x82001C98
        const f32 KF_TAKEDOWN_CAMERA_EARLY_OUT_TIME     = 0.2f;    // 0x8202AC1C
        const f32 KR_MIN_TAKEDOWN_CAMERA_EARLY_OUT_SPEED = 1.0f;   // 0x82001C98 (m/s, |mLinearVelocity|)

        // The block StartTakedownCamera and UpdateTakedownCamera both inline (asm cited above).
        f32 TimeInTakedownCamera(const ModeManager* lpModeManager)
        {
            if (lpModeManager->GetCurrentGameMode() == 0)
            {
                return KF_PURSUIT_TIME_IN_TAKEDOWN_CAM;
            }
            if (lpModeManager->GetCurrentGameModeType() == GameStateModuleIO::E_MODE_ROAD_RAGE)
            {
                return KF_ROAD_RAGE_TIME_IN_TAKEDOWN_CAM;
            }
            return KF_NORMAL_TIME_IN_TAKEDOWN_CAM;
        }

        // The console's SetTakedownCameraAction / SetPlayerCarDriverAction builders. The X360 leaves
        // the unwritten bytes of both records as stack residue; they are zeroed here so the payload
        // is deterministic (no consumer reads them).
        void PostTakedownCameraState(GameStateModuleIO::GameActionQueue* lpGameActionQueue,
                                     EActiveRaceCarIndex leFocusOnRaceCarIndex, bool lbActive, bool lbIsRevengeTakedown)
        {
            GameStateModuleIO::SetTakedownCameraAction lAction;
            lAction.meFocusOnRaceCarIndex = static_cast<decltype(lAction.meFocusOnRaceCarIndex)>(leFocusOnRaceCarIndex);
            lAction.mbActive              = lbActive;
            lAction.mbIsSignature         = false;
            lAction.mbIsRevengeTakedown   = lbIsRevengeTakedown;
            lAction.muPad07               = 0;
            lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAction),
                                        GameStateModuleIO::E_ACTION_SET_TAKEDOWN_CAMERA_STATE,
                                        static_cast<s32>(sizeof(lAction)));
        }

        void PostPlayerCarDriver(GameStateModuleIO::GameActionQueue* lpGameActionQueue, BrnWorld::CarControl leCarControl)
        {
            GameStateModuleIO::SetPlayerCarDriverAction lAction = {};
            lAction.meCarControl  = leCarControl;
            lAction.mbIsDriveThru = false;
            lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAction),
                                        GameStateModuleIO::E_ACTION_SET_PLAYER_CAR_DRIVER,
                                        static_cast<s32>(sizeof(lAction)));
        }
    }

    // =========================================================================================
    // Construct -- no out-of-line X360 symbol: inlined into GameStateModule::Construct @0x82380388
    // (r23 = gsm+0x238 == this):
    //   0x82380410  stw  r26 (gsm+0x1020,  the embedded ModeManager),        +0x28C  mpModeManager
    //   0x82380414  stw  r28 (gsm+0xBB30, the embedded ProgressionManager),  +0x290  mpProgressionManager
    //   0x82380404..0x82380434  the debug component's own inlined Construct(this) + Register
    // Nothing else of this object is touched there -- the timers are seeded by Prepare.
    // =========================================================================================
    void TakedownManager::Construct(ModeManager* lpModeManager, BrnProgression::ProgressionManager* lpProgressionManager)
    {
        mpModeManager        = lpModeManager;
        mpProgressionManager = lpProgressionManager;

        mTakedownManagerDebugComponent.Construct(this);
    }

    // X360 0x823595B8 (caller GameStateModule::Prepare @0x8239EAD0). The eight stores, in the
    // console's order; returns true unconditionally (`li r3, 1`).
    bool TakedownManager::Prepare()
    {
        ClearRaceCarData();

        mfPlayerSpeedAtTakedown           = 0.0f;
        mbPlayerWaitingForControl         = false;
        mfTakedownCameraEarlyOutTimer     = 0.0f;
        meCurrentVictimActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
        mfTimeWithWheelsOffGround         = 0.0f;
        mbDoneResetThisTakedown           = false;
        mfPlayerControlTimer              = 0.0f;
        mfTakedownCameraTimer             = KF_INVALID_TIME;   // == not in a takedown camera
        return true;
    }

    // X360 0x823594E0. Clear() every slot (the console's u16 loop counter is an artifact).
    void TakedownManager::ClearRaceCarData()
    {
        for (s32 li = 0; li < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++li)
        {
            maRaceCarData[li].Clear();
        }
    }

    // X360 0x82388FA8 (callers GameStateModule::OnModeFinish @0x82390EE0, ProcessGameEvents @0x823A0A18).
    // If a takedown camera is running, switch it off and hand the car back to the player (the two
    // posts @0x82389008 / 0x82389028), then reset every piece of state Prepare seeds.
    void TakedownManager::ClearAllTakedowns(GameStateModuleIO::GameActionQueue* lpGameActionQueue)
    {
        if (IsInTakedownCamera())
        {
            PostTakedownCameraState(lpGameActionQueue, E_ACTIVE_RACE_CAR_INDEX_INVALID, false, false);
            PostPlayerCarDriver(lpGameActionQueue, BrnWorld::E_CAR_CONTROL_ENTITY_MODULE);
        }

        ClearRaceCarData();

        meCurrentVictimActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
        mfTakedownCameraTimer             = KF_INVALID_TIME;
        mbPlayerWaitingForControl         = false;
        mbDoneResetThisTakedown           = false;
        mfPlayerSpeedAtTakedown           = 0.0f;
        mfTakedownCameraEarlyOutTimer     = 0.0f;
        mfTimeWithWheelsOffGround         = 0.0f;
        mfPlayerControlTimer              = 0.0f;
    }

    // X360 0x82359538. Both range asserts (BrnTakedownManager.cpp:132/133) then the 80-byte stride.
    TakedownManager::RaceCarData* TakedownManager::GetRaceCarData(EActiveRaceCarIndex leActiveRaceCarIndex)
    {
        CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
        return &maRaceCarData[leActiveRaceCarIndex];
    }

    // X360 0x82359620 (caller ModeManager::UpdateCurrentMode @0x82350EC8): the timer is -1.0
    // (flt_820037C8, the same word KF_INVALID_TIME is) exactly while no takedown camera runs.
    bool TakedownManager::IsInTakedownCamera() const
    {
        return mfTakedownCameraTimer != KF_INVALID_TIME;
    }

    // X360 0x82359648 (caller UpdatePlayerResetStatus). Outside a takedown camera: never. Inside a
    // mode other than an offline race (incl. free burn): once per takedown (mbDoneResetThisTakedown). In an offline race: only inside the
    // [KF_MIN_NORMAL_RESET_TIME, KF_MAX_NORMAL_RESET_TIME] window of the camera timer.
    bool TakedownManager::IsAllowedToResetPlayer()
    {
        if (!IsInTakedownCamera())
        {
            return false;
        }

        // [verify V3 2026-09-02] CORRECTED: 0x82359678..84 is `lwz r11, 0x28C(r3); lwz r11, 0xD94(r11);
        // cmpwi cr6, r11, 0; bne` -- meCurrentGameModeType (mm+3476) compared with 0 ==
        // E_MODE_OFFLINE_RACE, NOT the mode pointer (+0xD98). Free burn (E_MODE_NONE == -1) takes
        // this arm; only an offline race uses the reset window below.
        if (mpModeManager->GetCurrentGameModeType() != GameStateModuleIO::E_MODE_OFFLINE_RACE)
        {
            return !mbDoneResetThisTakedown;
        }

        return (mfTakedownCameraTimer > KF_MIN_NORMAL_RESET_TIME) &&
               (mfTakedownCameraTimer < KF_MAX_NORMAL_RESET_TIME);
    }

    // No out-of-line X360 symbol (DWARF :120, cpp:883): inlined at IsBeingAttacked @0x8236F138..
    // 0x8236F174, where the contact window is chosen off ModeManager::IsOnlineGameMode() (the
    // `mpCurrentGameMode ? mpCurrentGameMode->mbIsOnline : false` read of mm+0xD98 / mode+0xAC).
    f32 TakedownManager::GetTakedownTime()
    {
        if (mpModeManager->IsOnlineGameMode())
        {
            return KF_TAKEDOWN_TIME_CONTACT_ONLINE;
        }
        return KF_TAKEDOWN_TIME_CONTACT;
    }

    // X360 0x8236F120 (caller DetectStandardTakedown @0x8237A420). A car that is being slammed
    // (mSlamEffect.mfSlamLife > 0, +908) or shunted (mShuntEffect.IsActive(), +928) names its last
    // attacker; otherwise its last race-car contact counts only while it is fresher than the
    // contact window. The DWARF's local is `lfContactTime`.
    bool TakedownManager::IsBeingAttacked(const RaceCarState* lpRaceCarState, EActiveRaceCarIndex* lpeAttackerIndex)
    {
        const f32 lfContactTime = GetTakedownTime();

        if (lpRaceCarState->mSlamEffect.mfSlamLife > 0.0f || lpRaceCarState->mShuntEffect.IsActive())
        {
            *lpeAttackerIndex = static_cast<EActiveRaceCarIndex>(lpRaceCarState->mi8LastAttackersRaceCarIndex);
            return true;
        }

        if (lpRaceCarState->mfTimeSinceLastRaceCarContact >= lfContactTime)
        {
            return false;
        }

        *lpeAttackerIndex = static_cast<EActiveRaceCarIndex>(lpRaceCarState->mi8LastContactedRaceCar);
        return true;
    }

    // No out-of-line X360 symbol (DWARF :107). The only predicate the X360 evaluates between
    // fetching the aggressor's RaceCarState and building the takedown record is
    // DetectStandardTakedown @0x8237A4A8..0x8237A4B0: `lbz r11, 0x44A(aggressor)` (mbCrashing) must
    // be zero. That is the whole inlined body this build kept; the victim argument is not read
    // there. FLAG: if a later export shows a second inlined site with more of the predicate
    // (KF_FRONT_CONTACT_TOLERANCE / KF_MIN_TAKEDOWN_SPEED have no reader in any of the 22 bodies),
    // extend it here -- do not widen it on a guess.
    bool TakedownManager::IsValidTakedownSituation(const RaceCarState* lpAggressor, const RaceCarState* lpVictim)
    {
        (void)lpVictim;
        return !lpAggressor->mbCrashing;
    }

    // X360 0x823660C0 (caller Update @0x8239FAC0). Per slot: advance both takedown clocks
    // (mfTimeSinceLastTakenDown saturates at FLT_MAX -- `if (t < FLT_MAX - dt) t += dt`
    // @0x8236618C), time the chain and the double out, and a crashing car loses its chain.
    void TakedownManager::UpdateTakedownTimes(RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface, f32 lfDeltaTime)
    {
        for (s32 li = 0; li < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++li)
        {
            const EActiveRaceCarIndex leIndex    = static_cast<EActiveRaceCarIndex>(li);
            RaceCarData*              lpRaceCarData = GetRaceCarData(leIndex);

            lpRaceCarData->mfTimeSinceLastTakedown += lfDeltaTime;
            if (lpRaceCarData->mfTimeSinceLastTakenDown < 3.4028235e38f - lfDeltaTime)   // FLT_MAX guard (flt_82029B70)
            {
                lpRaceCarData->mfTimeSinceLastTakenDown += lfDeltaTime;
            }

            const f32 lfTimeSinceLastTakedown = lpRaceCarData->mfTimeSinceLastTakedown;
            if (lfTimeSinceLastTakedown == KF_INVALID_TIME || lfTimeSinceLastTakedown > KF_TAKEDOWN_CHAIN_TIMEOUT_SECONDS)
            {
                lpRaceCarData->miTakedownChainLength = 0;
            }
            if (lfTimeSinceLastTakedown == KF_INVALID_TIME || lfTimeSinceLastTakedown > KF_DOUBLE_TAKEDOWN_TIME_LIMIT)
            {
                lpRaceCarData->miMultipleTakedownLength = 0;
            }

            // @0x82366210: the inlined IsRaceCarActive (`maxRaceCarFlags[i] & 1`) then GetRaceCarState(i)->mbCrashing
            if (lpActiveCarInterface->IsRaceCarActive(static_cast<::EActiveRaceCarIndex>(leIndex)) &&
                lpActiveCarInterface->GetRaceCarState(static_cast<::EActiveRaceCarIndex>(leIndex))->mbCrashing)
            {
                lpRaceCarData->miTakedownChainLength = 0;
            }
        }
    }

    // X360 0x82388AC0 (caller Update @0x8239FAC0). While a reset is allowed, decide whether the
    // player's car is in a state worth resetting -- too slow / dropped too much speed (game modes
    // only), airborne too long, tilted past KF_MIN_ANGLE_FOR_PLAYER_RESET, or spinning faster than
    // KF_MAX_ANGULAR_VELOCITY_FOR_PLAYER_RESET -- and if so post ResetPlayerCarOnTrack at the
    // takedown speed clamped to [KF_MIN_SPEED_FOR_PLAYER_RESET, KF_MAX_SPEED_FOR_PLAYER_RESET].
    void TakedownManager::UpdatePlayerResetStatus(RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface,
                                                  GameStateModuleIO::GameActionQueue* lpGameActionQueue, f32 lfDeltaTime)
    {
        if (!IsAllowedToResetPlayer())
        {
            return;
        }

        // sub_82310240 == GetPlayerRaceCarState (asserts IsPlayerCarActive(), header line 1266)
        const RaceCarState* lpPlayerState = lpActiveCarInterface->GetPlayerRaceCarState();

        // 0x82388B5C..0x82388C10: the car's up axis (mTransform row 1, +512) crossed with world up
        // (0,1,0), the cross product's magnitude signed by its Y lane (+1 / 0 / -1 via the two vsel),
        // through XMVectorASin -- the same two-permute cross + signed-magnitude + asin idiom
        // StuntAttackMode uses for its start-direction angle. NOTE (console behaviour, reproduced):
        // for up x (0,1,0) the Y lane is identically zero, so the sign -- and the angle -- is always
        // 0 here; the tilt arm can only fire if the up axis carries a non-zero product, which it
        // never does. Kept as the console computes it.
        const Vector3 lWorldUp      = { 0.0f, 1.0f, 0.0f, 0.0f };
        const Vector3 lCrossProduct = rw::math::vpu::Cross(lpPlayerState->mTransform.yAxis, lWorldUp);

        f32 lfSign = 0.0f;
        if (lCrossProduct.y > 0.0f)
        {
            lfSign = 1.0f;
        }
        else if (lCrossProduct.y < 0.0f)
        {
            lfSign = -1.0f;
        }
        const f32 lfTiltAngle = static_cast<f32>(std::asin(static_cast<double>(lfSign * rw::math::vpu::Magnitude(lCrossProduct))));

        // 0x82388B64/0x82388B9C: mfSpeedMPH (+972) in m/s
        const f32 lfPlayerSpeed = lpPlayerState->mfSpeedMPH * KF_MPH_TO_METRES_PER_SECOND;

        // 0x82388C04..0x82388C64: count the wheels off the ground (WheelLite+40 == mRoadContact.mbIsOnGround)
        s32 liWheelsOffGround = 0;
        for (s32 liWheel = 0; liWheel < 4; ++liWheel)
        {
            if (!lpPlayerState->maWheels[liWheel].mRoadContact.mbIsOnGround)
            {
                ++liWheelsOffGround;
            }
        }
        if (liWheelsOffGround < KI_WHEELS_OFF_GROUND_FOR_PLAYER_RESET)
        {
            mfTimeWithWheelsOffGround = 0.0f;
        }
        else
        {
            mfTimeWithWheelsOffGround += lfDeltaTime;
        }

        // 0x82388C68..0x82388CA8: the speed tests only apply inside a game mode
        bool lbSpeedDropped = false;
        if (mpModeManager->GetCurrentGameMode() != 0)
        {
            lbSpeedDropped = (lfPlayerSpeed < KF_MIN_SPEED_FOR_PLAYER_RESET) ||
                             (lfPlayerSpeed < mfPlayerSpeedAtTakedown - KF_SPEED_DROP_FOR_PLAYER_RESET);
        }

        // 0x82388CB0..0x82388D6C: the four-way OR, in the console's order
        const bool lbNeedsReset =
            lbSpeedDropped ||
            (mfTimeWithWheelsOffGround >= KF_MIN_TIME_IN_AIR_FOR_PLAYER_RESET) ||
            (lfTiltAngle > KF_MIN_ANGLE_FOR_PLAYER_RESET) ||
            (rw::math::vpu::Magnitude(lpPlayerState->mAngularVelocity) > KF_MAX_ANGULAR_VELOCITY_FOR_PLAYER_RESET);

        if (lbNeedsReset)
        {
            // 0x82388D70..0x82388D9C: the two fsel = clamp(mfPlayerSpeedAtTakedown, MIN, MAX)
            f32 lfResetSpeed = mfPlayerSpeedAtTakedown;
            if (lfResetSpeed < KF_MIN_SPEED_FOR_PLAYER_RESET)
            {
                lfResetSpeed = KF_MIN_SPEED_FOR_PLAYER_RESET;
            }
            if (lfResetSpeed > KF_MAX_SPEED_FOR_PLAYER_RESET)
            {
                lfResetSpeed = KF_MAX_SPEED_FOR_PLAYER_RESET;
            }

            GameStateModuleIO::ResetPlayerCarOnTrackAction lAction;
            lAction.mfSpeed = lfResetSpeed;
            lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAction),
                                        GameStateModuleIO::E_ACTION_RESET_PLAYER_CAR_ON_TRACK,
                                        static_cast<s32>(sizeof(lAction)));

            mbDoneResetThisTakedown = true;
        }
    }

    // X360 0x82388DD8 (caller ProcessTakedownEvent @0x82393D40). Start the camera on the victim:
    // camera on (revenge flag == type is E_TAKEDOWN_REVENGE, the `cntlzw(type - 9)` @0x82388E0C),
    // hand the player's car to the AI module for the duration, make the player invulnerable for
    // the camera time plus KF_POST_TAKEDOWN_INVULNERABLE_TIME, and arm the reset bookkeeping.
    void TakedownManager::StartTakedownCamera(GameStateModuleIO::GameActionQueue* lpGameActionQueue,
                                              EActiveRaceCarIndex leVictimIndex, ETakedownType leType)
    {
        mfTakedownCameraTimer             = 0.0f;
        mfTakedownCameraEarlyOutTimer     = 0.0f;
        meCurrentVictimActiveRaceCarIndex = leVictimIndex;

        PostTakedownCameraState(lpGameActionQueue, leVictimIndex, true, leType == E_TAKEDOWN_REVENGE);
        PostPlayerCarDriver(lpGameActionQueue, BrnWorld::E_CAR_CONTROL_AI_MODULE);

        GameStateModuleIO::PlayerInvulnerableAction lInvulnerable;
        lInvulnerable.mfInvulnerableTime = TimeInTakedownCamera(mpModeManager) + KF_POST_TAKEDOWN_INVULNERABLE_TIME;
        lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lInvulnerable),
                                    GameStateModuleIO::E_ACTION_PLAYER_INVULNERABLE,
                                    static_cast<s32>(sizeof(lInvulnerable)));

        mfTimeWithWheelsOffGround = 0.0f;
        mbDoneResetThisTakedown   = false;
    }

    // X360 0x82388ED8 (callers UpdateTakedownCamera @0x82389234, Update @0x8239FAC0; both pass the
    // PLAYER's active index). Camera off, start waiting for control to come back; with no current
    // game mode, a victim other than that index is SHUT DOWN (car handed straight back to the
    // player, ShutdownFinished on the victim) instead. Then clear the camera state.
    void TakedownManager::EndTakedownCamera(GameStateModuleIO::GameActionQueue* lpGameActionQueue, EActiveRaceCarIndex leVictimIndex)
    {
        PostTakedownCameraState(lpGameActionQueue, E_ACTIVE_RACE_CAR_INDEX_INVALID, false, false);

        mfPlayerControlTimer      = 0.0f;
        mbPlayerWaitingForControl = true;

        if (mpModeManager->GetCurrentGameMode() == 0 && meCurrentVictimActiveRaceCarIndex != leVictimIndex)
        {
            PostPlayerCarDriver(lpGameActionQueue, BrnWorld::E_CAR_CONTROL_ENTITY_MODULE);
            mbPlayerWaitingForControl = false;

            GameStateModuleIO::ShutdownFinishedAction lShutdown;
            lShutdown.meActiveRaceCarIndex = static_cast<decltype(lShutdown.meActiveRaceCarIndex)>(meCurrentVictimActiveRaceCarIndex);
            lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lShutdown),
                                        GameStateModuleIO::E_ACTION_SHUTDOWN_FINISHED,
                                        static_cast<s32>(sizeof(lShutdown)));
        }

        mfTakedownCameraEarlyOutTimer     = 0.0f;
        meCurrentVictimActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
        mfTakedownCameraTimer             = KF_INVALID_TIME;
    }

    // X360 0x82389068 (caller Update @0x8239FAC0). Run the camera timer against the mode's camera
    // time; in road rage a victim that has (nearly) stopped for KF_TAKEDOWN_CAMERA_EARLY_OUT_TIME
    // after the first KF_TAKEDOWN_CAMERA_EARLY_OUT_MIN_TIME ends the camera early.
    void TakedownManager::UpdateTakedownCamera(f32 lfDeltaTime, GameStateModuleIO::OutputBuffer* lpOutput,
                                               RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface)
    {
        CGS_ASSERT(IsInTakedownCamera(), "IsInTakedownCamera()");

        mfTakedownCameraTimer += lfDeltaTime;

        bool lbEndCamera = mfTakedownCameraTimer > TimeInTakedownCamera(mpModeManager);

        // 0x82389128: the road-rage test reads the mode type directly (no current-mode guard)
        if (mpModeManager->GetCurrentGameModeType() == GameStateModuleIO::E_MODE_ROAD_RAGE)
        {
            // 0x82389138..0x823891C0: |victim mLinearVelocity| (+816) vs the early-out speed
            const RaceCarState* lpVictimState =
                lpActiveCarInterface->GetRaceCarState(static_cast<::EActiveRaceCarIndex>(meCurrentVictimActiveRaceCarIndex));

            if (rw::math::vpu::Magnitude(lpVictimState->mLinearVelocity) <= KR_MIN_TAKEDOWN_CAMERA_EARLY_OUT_SPEED)
            {
                mfTakedownCameraEarlyOutTimer += lfDeltaTime;
            }
            else
            {
                mfTakedownCameraEarlyOutTimer = 0.0f;
            }

            if (mfTakedownCameraTimer > KF_TAKEDOWN_CAMERA_EARLY_OUT_MIN_TIME &&
                mfTakedownCameraEarlyOutTimer > KF_TAKEDOWN_CAMERA_EARLY_OUT_TIME)
            {
                lbEndCamera = true;
            }
        }

        if (lbEndCamera)
        {
            // 0x823891F0..0x82389234: the inlined GetPlayerActiveRaceCarIndex ("Player car index
            // hasn't been set", header line 980) and OutputBuffer::GetGameActionQueue @0x8231D4B8.
            EndTakedownCamera(lpOutput->GetGameActionQueue(),
                              static_cast<EActiveRaceCarIndex>(lpActiveCarInterface->GetPlayerActiveRaceCarIndex()));
        }
    }
}
