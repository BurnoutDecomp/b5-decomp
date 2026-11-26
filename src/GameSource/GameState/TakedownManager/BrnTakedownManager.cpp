#include <float.h>
#include "GameSource/GameState/TakedownManager/BrnTakedownManager.h"
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/TakedownManager/BrnTakedownManager.cpp"
#include <cmath>                                                        // std::asin (the de-SIMD'd XMVectorASin)

#include "rw/math/vpu/vector3_operation.h"                              // rw::math::vpu::Cross / Magnitude

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"                      // CGS_ASSERT
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
            lAction.meFocusOnRaceCarIndex = leFocusOnRaceCarIndex;
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
            GameStateModuleIO::SetPlayerCarDriverAction lAction = GameStateModuleIO::SetPlayerCarDriverAction();
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
        CGS_ASSERT_AT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
            "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0", CGS_ARTIST_FILE, 132 /* ARTIST 0x82359564 */);

        CGS_ASSERT_AT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
            "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT", CGS_ARTIST_FILE, 133 /* ARTIST 0x82359588 */);

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
        if (mpModeManager->GetCurrentGameModeType() == GameStateModuleIO::E_MODE_OFFLINE_RACE)
        {
            return (mfTakedownCameraTimer > KF_MIN_NORMAL_RESET_TIME) &&
                   (mfTakedownCameraTimer < KF_MAX_NORMAL_RESET_TIME);
        }
        return !mbDoneResetThisTakedown;
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
            if (lpRaceCarData->mfTimeSinceLastTakenDown < FLT_MAX - lfDeltaTime)   // FLT_MAX guard (flt_82029B70)
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
        const Vector3 lWorldUp(0.0f, 1.0f, 0.0f);
        const Vector3 lCrossProduct = rw::math::vpu::Cross(lpPlayerState->mTransform.yAxis, lWorldUp);

        f32 lfSign = 0.0f;
        if (lCrossProduct.mV.y > 0.0f)
        {
            lfSign = 1.0f;
        }
        else if (lCrossProduct.mV.y < 0.0f)
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
            lShutdown.meActiveRaceCarIndex = meCurrentVictimActiveRaceCarIndex;
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
        CGS_ASSERT_AT(IsInTakedownCamera(), "IsInTakedownCamera()", CGS_ARTIST_FILE, 1037);

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

// FOLDED FROM BrnTakedownManager_Detect.cpp
#include "GameSource/GameState/TakedownManager/BrnTakedownManager.h"
#include "GameSource/GameState/BrnGameActions.h"                                   // OnPlayerTakedownAction / ShutdownAction / SetPlayerCarDriverAction
#include "GameSource/GameState/ModeManager/BrnModeManager.h"                       // ModeManager (mode type, IsOnlineGameMode, the mode-params flags)
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"          // GameModeParams::KU_FLAG_*
#include "GameSource/GameState/Progression/BrnProgressionManager.h"                // ProgressionManager::OnPursuitWon (the free-burn rival shutdown)
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"           // RaceCarCrashEvent / RaceCarState
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h" // MarkedManInterface
#include "GameSource/World/BrnEntityTypes.h"                                       // BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"                      // CgsSceneManager::EntityId (owner / entity-index decode of the packed crasher word)
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                           // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstring>                                                                  // std::memset (the residue-zeroed action records)
#include <cstdlib>                                                                  // std::getenv ([td-detect] PC witness switch)

// ============================================================================================
// BrnGameState::TakedownManager -- Update and the detect / process chain (BrnTakedownManager.cpp
// on the console; split out of the lifecycle / camera / reset half that lives in
// BrnTakedownManager.cpp). Reconstructed from BURNOUT_X360_ARTIST.XEX, 2026-09-02 (takedown wave,
// agent T2):
//
//   Update                                  @0x8239FAC0
//   DetectTakedowns                         @0x8239D808
//   DetectInstantTakedown                   @0x82399638
//   DetectStandardTakedown                  @0x8237A3C0   (the core classifier)
//   DetectNetworkTakedowns                  @0x823997A0
//   ProcessQueuedTakedowns                  @0x82399A98
//   ProcessTakedownEvent                    @0x82393D40
//   GetTakedownTypeFromTrafficVehicleIndex  @0x82366288
//
// The ids the console packs into a crash event: mRaceCarVolumeInstanceID's entity word carries the
// crashed car's ACTIVE race-car index in the 14-bit entity field (`ld; srdi 32; extrwi 14,8`), and
// mCrasherEntityID is an EntityId whose owner byte says who hit it (2 == a traffic vehicle) and whose
// entity field is that aggressor's active index (race car) or traffic vehicle index (traffic).
//
// Mode-manager reads: mm+3476 is meCurrentGameModeType, mm+3480 mpCurrentGameMode (its +172 byte is
// mbIsOnline -- IsOnlineGameMode()), and the `ldx mm+0x8BE0` reads are mCurrentGameModeParams.muFlags
// (GetCurrentGameModeParams()->GetFlag); the three bits tested are named at their sites.
//
// Speeds: RaceCarState::mfSpeedMPH is in mph; every takedown-side compare first scales it by the
// console's 0.44704 (flt_82F31928) into m/s, the unit the KF_ speed tunables are in.
// ============================================================================================

namespace BrnGameState
{
    using BrnWorld::RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_RUNNING;
    using BrnPhysics::Vehicle::RaceCarCrashEvent;
    using BrnPhysics::Vehicle::RaceCarState;

    namespace
    {
        // DWARF BrnTakedownManager.cpp:38 (file-scope const of the console TU). X360 flt_82001DA0 ==
        // 0.5: Update posts the "player drives again" action once mfPlayerControlTimer passes it.
        const f32 KF_PLAYER_CONTROL_RETURN_DELAY_TIME = 0.5f;

        // flt_82F31928 -- the engine-wide mph -> m/s scale (0.44704), applied to mfSpeedMPH before the
        // m/s tunables are compared against it.


        // The (inlined) TakedownEvent::Construct the three event builders run first (DWARF
        // BrnTakedownManagerTypes.h:83; no out-of-line X360 symbol). Attested field set, from
        // DetectStandardTakedown @0x8237A4E8..0x8237A504: both car ids 0, type NONE, both counts 0,
        // the three flags false. The two indices are written by every builder straight after, so the
        // console shows no Construct-time store for them.
        void ConstructTakedownEvent(TakedownEvent& lrEvent)
        {
            lrEvent.mAggressorCarID         = 0;
            lrEvent.mVictimCarID            = 0;
            lrEvent.meType                  = E_TAKEDOWN_NONE;
            lrEvent.miMultipleTakedownCount = 0;
            lrEvent.miTakedownChainCount    = 0;
            lrEvent.mbMarkedManTakeDown     = false;
            lrEvent.mbRemote                = false;
            lrEvent.mbSettledScore          = false;
        }

        // The active-race-car index the physics side packed into a crash event's volume-instance id.
        EActiveRaceCarIndex GetCrashedActiveRaceCarIndex(const RaceCarCrashEvent* lpCrashEvent)
        {
            return static_cast<EActiveRaceCarIndex>(lpCrashEvent->mRaceCarVolumeInstanceID.GetEntityIDEntityIndex());
        }

        // The crasher's packed EntityId word, as the real CgsSceneManager::EntityId so the owner byte
        // and the 14-bit entity index come out through the named accessors (the BrnCommonTypes.h
        // EntityId is storage only). Same idiom as the challenge manager's crash reader.
        //
        // SETTLED this wave (the "attacker 0 on every rival crash" scare): there is NO "no attacker"
        // SENTINEL in this seat and none is owed. The crash sink publishes a real causing entity on
        // every path -- the world entity for a wall hit, the traffic slot's global id for a traffic
        // hit, the other car for a car-on-car pair, the victim's own id for the self-inflicted arm --
        // so "nobody took this car down" is spelled by the OWNER BYTE, never by the index. Both
        // readers below are already owner-correct: the instant path reads the index only after the
        // takedown TYPE has proven non-NONE (a classified takedown always has a race-car crasher),
        // and the standard path tests GetOwner() against the traffic type and otherwise names the
        // aggressor from the victim's shunt state rather than from this seat at all. A reader that
        // took the bare entity index as an attacker slot would credit the player for every rival's
        // self-inflicted crash; do not add one.
        CgsSceneManager::EntityId GetCrasherEntityId(const RaceCarCrashEvent* lpCrashEvent)
        {
            return CgsSceneManager::EntityId(lpCrashEvent->mCrasherEntityID.muValue);
        }

        // The active-race-car output interface speaks the GLOBAL ::EActiveRaceCarIndex
        // (BurnoutConstants.h); this manager's DWARF shape speaks the same-valued BrnGameState::
        // EActiveRaceCarIndex (BrnTakedownManagerTypes.h, which already notes the split). Bridge at
        // the interface boundary only.
        inline ::EActiveRaceCarIndex GlobalIndex(EActiveRaceCarIndex leIndex)   { return static_cast<::EActiveRaceCarIndex>(leIndex); }
        inline EActiveRaceCarIndex   LocalIndex(::EActiveRaceCarIndex leIndex)  { return static_cast<EActiveRaceCarIndex>(leIndex); }

    }

    // ----------------------------------------------------------------------------------------
    // Update @0x8239FAC0 -- BrnTakedownManager.cpp:171
    // ----------------------------------------------------------------------------------------
    void TakedownManager::Update(RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface,
                                 f32 lfDeltaTime,
                                 RaceCarCrashEventQueue* lpRaceCarCrashEventQueue,
                                 CrashingRaceCarInterface* lpCrashingVehicleInterface,
                                 const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                 GameStateModuleIO::OutputBuffer* lpOutput,
                                 TrafficTypeResponseQueue* lpLastTrafficTypeResponseQueue)
    {
        CGS_ASSERT_AT(lpActiveCarInterface != 0,           "lpActiveCarInterface != NULL", CGS_ARTIST_FILE, 180);            // :180
        CGS_ASSERT_AT(lpRaceCarCrashEventQueue != 0,       "lpRaceCarCrashEventQueue != NULL", CGS_ARTIST_FILE, 181);        // :181
        CGS_ASSERT_AT(lpCrashingVehicleInterface != 0,     "lpCrashingVehicleInterface != NULL", CGS_ARTIST_FILE, 182);      // :182
        CGS_ASSERT_AT(lpInput != 0,                        "lpInput != NULL", CGS_ARTIST_FILE, 183);                         // :183
        CGS_ASSERT_AT(lpOutput != 0,                       "lpOutput != NULL", CGS_ARTIST_FILE, 184);                        // :184
        CGS_ASSERT_AT(lpLastTrafficTypeResponseQueue != 0, "lpLastTrafficTypeResponseQueue != NULL", CGS_ARTIST_FILE, 185);  // :185
        // (lpCrashingVehicleInterface is asserted and then never read -- as on the console.)

        UpdateTakedownTimes(lpActiveCarInterface, lfDeltaTime);

        // Showtime never runs the takedown camera / reset: kill a live camera, else tick the reset logic.
        const GameStateModuleIO::EGameModeType leGameModeType = mpModeManager->GetCurrentGameModeType();
        if (leGameModeType == GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME ||
            leGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME)
        {
            if (IsInTakedownCamera())
            {
                EndTakedownCamera(lpOutput->GetGameActionQueue(), LocalIndex(lpActiveCarInterface->GetPlayerActiveRaceCarIndex()));
            }
        }
        else
        {
            UpdatePlayerResetStatus(lpActiveCarInterface, lpOutput->GetGameActionQueue(), lfDeltaTime);
        }

        // Local crash classification runs only for an active player car whose engine is running
        // (offline; online skips the engine test) in a mode that has not disabled takedowns
        // (`rlwinm r11,r11,0,9,9` @0x8239FD38 == muFlags bit 22 == KU_FLAG_DISABLE_ALL_TDS).
        bool lbDetectTakedowns = lpActiveCarInterface->IsPlayerCarActive();
        if (lbDetectTakedowns && !mpModeManager->IsOnlineGameMode())
        {
            lbDetectTakedowns = lpActiveCarInterface->GetPlayerEngineState() == E_ACTIVE_RACE_CAR_ENGINE_STATE_RUNNING;
        }
        if (lbDetectTakedowns && mpModeManager->GetCurrentGameMode() != 0)
        {
            lbDetectTakedowns = !mpModeManager->GetCurrentGameModeParams()->GetFlag(GameModeParams::KU_FLAG_DISABLE_ALL_TDS);
        }
        if (lbDetectTakedowns)
        {
            DetectTakedowns(lpInput, lpOutput, lpRaceCarCrashEventQueue, lpActiveCarInterface, lpLastTrafficTypeResponseQueue);
        }

        DetectNetworkTakedowns(lpInput, lpActiveCarInterface, lpOutput);

        // =====================================================================================
        // [PC HARNESS, NOT X360] BRN_TD_LADDER=<firstSeconds>[:<periodSeconds>] -- the TAKEDOWN
        // TYPE DISPLAY LADDER.
        //
        // ⭐ WHY. The takedown HUD banner is only ever produced INSIDE A GAME MODE: the console's
        // ProcessTakedownEvent pushes the TakedownEvent onto the output queue only in its
        // `mpCurrentGameMode != 0` arm (@0x82393E9C; the free-burn else-arm posts the rival
        // SHUTDOWN instead and queues nothing). So "does a VERTICAL show as TDGdVert" cannot be
        // answered by driving around free burn at all, and a scripted drive cannot be made to
        // produce a vertical, a T-bone AND a traffic check inside one event on demand.
        // THIS INVENTS NOTHING. It arms exactly the record the console's OWN debug callback
        // "Force takedown" arms (TakedownManagerDebugComponent::ForceTakedownCallback
        // @0x823597F8 -- Clear, mbWaitingOnTakedown, aggressor 0, victim 1) and changes ONE field:
        // the takedown TYPE, walked through the enum. Everything downstream -- ProcessQueuedTakedowns,
        // ProcessTakedownEvent, the output queue, TranslateTakedownsToGuiEvents, HandleTakedown,
        // KAPC_TAKEDOWN_TYPES -- is the console's own code, unmodified.
        // ⛔ A CAPABILITY, not an instrument: it awards takedowns nobody earned. Off unless set,
        // and it must be in the harness wipe list.
        // DELETE-WHEN every type is proven on film from organic play.
        // =====================================================================================
        {
        }

        ProcessQueuedTakedowns(lpInput, lpActiveCarInterface, lpOutput, lfDeltaTime);

        if (IsInTakedownCamera())
        {
            UpdateTakedownCamera(lfDeltaTime, lpOutput, lpActiveCarInterface);
        }

        // Hand the car back to the player KF_PLAYER_CONTROL_RETURN_DELAY_TIME after the camera released
        // it: action 7 with only the driver word (1 == entity module) and the drive-thru flag (0)
        // written (@0x8239FE1C..0x8239FE3C); the rest of the record is stack residue on the console,
        // zeroed here for reproducibility.
        if (mbPlayerWaitingForControl)
        {
            mfPlayerControlTimer += lfDeltaTime;
            if (mfPlayerControlTimer > KF_PLAYER_CONTROL_RETURN_DELAY_TIME)
            {
                GameStateModuleIO::SetPlayerCarDriverAction lSetPlayerCarDriverAction;
                std::memset(&lSetPlayerCarDriverAction, 0, sizeof(lSetPlayerCarDriverAction));
                lSetPlayerCarDriverAction.meCarControl  = BrnWorld::E_CAR_CONTROL_ENTITY_MODULE;
                lSetPlayerCarDriverAction.mbIsDriveThru = false;
                lpOutput->GetGameActionQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lSetPlayerCarDriverAction),
                    GameStateModuleIO::E_ACTION_SET_PLAYER_CAR_DRIVER,
                    static_cast<s32>(sizeof(lSetPlayerCarDriverAction)));
                mbPlayerWaitingForControl = false;
            }
        }
    }

    // ----------------------------------------------------------------------------------------
    // DetectTakedowns @0x8239D808 -- BrnTakedownManager.cpp:395
    // Walk this frame's primary crashes; offline every crash is a candidate, online only the
    // local player's own crash is (the rest arrive through DetectNetworkTakedowns).
    // ----------------------------------------------------------------------------------------
    void TakedownManager::DetectTakedowns(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                          GameStateModuleIO::OutputBuffer* lpOutput,
                                          const RaceCarCrashEventQueue* lpCrashQueue,
                                          RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface,
                                          TrafficTypeResponseQueue* lpLastTrafficTypeResponseQueue)
    {
        if (!lpActiveCarInterface->IsPlayerCarActive())
        {
            return;
        }

        const EActiveRaceCarIndex lePlayerActiveRaceCarIndex = LocalIndex(lpActiveCarInterface->GetPlayerActiveRaceCarIndex());

        // DWARF BrnTakedownManager.cpp:404 / :410 / :411 locals.
        for (s32 liCrashIndex = 0; liCrashIndex < lpCrashQueue->GetLength(); ++liCrashIndex)
        {
            const RaceCarCrashEvent* lpCrashEvent = &lpCrashQueue->GetEvent(liCrashIndex);
            if (lpCrashEvent->mbIsPrimaryCrash)
            {
                const CgsSceneManager::VolumeInstanceId lCrashedCarInstanceID = lpCrashEvent->mRaceCarVolumeInstanceID;
                if (!mpModeManager->IsOnlineGameMode() ||
                    static_cast<EActiveRaceCarIndex>(lCrashedCarInstanceID.GetEntityIDEntityIndex()) == lePlayerActiveRaceCarIndex)
                {
                    const EActiveRaceCarIndex leCrashedActiveRaceCarIndex =
                        static_cast<EActiveRaceCarIndex>(lCrashedCarInstanceID.GetEntityIDEntityIndex());
                    // The crashed car is the victim of whatever follows: its own pending takedown
                    // (if it was the aggressor of an unconfirmed one) is dropped.
                    RaceCarData* lpVictimCarData = GetRaceCarData(leCrashedActiveRaceCarIndex);
                    lpVictimCarData->mfTimeSinceVictimCrashed = 0.0f;
                    lpVictimCarData->mbWaitingOnTakedown      = false;

                    if (!DetectInstantTakedown(lpInput, lpOutput, lpActiveCarInterface, lpCrashEvent))
                    {
                        DetectStandardTakedown(lpInput, lpOutput, lpActiveCarInterface, lpCrashEvent, lpLastTrafficTypeResponseQueue);
                    }
                }
            }
        }
    }

    // ----------------------------------------------------------------------------------------
    // DetectInstantTakedown @0x82399638 -- BrnTakedownManager.cpp:451
    // The physics side already classified this crash (meInstantTakedownType != NONE): the crasher
    // is the aggressor and the takedown is processed immediately, no confirmation wait.
    // ----------------------------------------------------------------------------------------
    bool TakedownManager::DetectInstantTakedown(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                                GameStateModuleIO::OutputBuffer* lpOutput,
                                                RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface,
                                                const RaceCarCrashEvent* lpCrashEvent)
    {
        const EActiveRaceCarIndex leCrashedActiveRaceCarIndex = GetCrashedActiveRaceCarIndex(lpCrashEvent);
        RaceCarData* lpCrashedCarData = GetRaceCarData(leCrashedActiveRaceCarIndex);

        if (lpCrashEvent->meInstantTakedownType == E_TAKEDOWN_NONE)
        {
            return false;
        }
        // Online, a car taken down within the last KF_ONLINE_TAKEDOWN_IGNORE_TIME (flt_82CDBDBC == 5.05 s)
        // is ignored.
        if (mpModeManager->IsOnlineGameMode() &&
            lpCrashedCarData->mfTimeSinceLastTakenDown <= KF_ONLINE_TAKEDOWN_IGNORE_TIME)
        {
            return false;
        }

        const EActiveRaceCarIndex leInstantAggressorActiveRaceCarIndex =
            static_cast<EActiveRaceCarIndex>(GetCrasherEntityId(lpCrashEvent).GetEntityIndex());
        RaceCarData* lpInstantAggresorCarData = GetRaceCarData(leInstantAggressorActiveRaceCarIndex);

        if (!lpActiveCarInterface->IsRaceCarActive(GlobalIndex(leInstantAggressorActiveRaceCarIndex)))
        {
            return false;
        }
        if (lpActiveCarInterface->GetRaceCarState(GlobalIndex(leInstantAggressorActiveRaceCarIndex))->mbCrashing)
        {
            return false;
        }

        TakedownEvent* lpAggressorTakedownEvent = &lpInstantAggresorCarData->mPendingTakedownEvent;
        ConstructTakedownEvent(*lpAggressorTakedownEvent);
        lpAggressorTakedownEvent->meAggressorIndex = leInstantAggressorActiveRaceCarIndex;
        lpAggressorTakedownEvent->meVictimIndex    = leCrashedActiveRaceCarIndex;
        lpAggressorTakedownEvent->mAggressorCarID  = lpActiveCarInterface->GetCarModelId(GlobalIndex(leInstantAggressorActiveRaceCarIndex));
        lpAggressorTakedownEvent->mVictimCarID     = lpActiveCarInterface->GetCarModelId(GlobalIndex(leCrashedActiveRaceCarIndex));
        lpAggressorTakedownEvent->meType           = lpCrashEvent->meInstantTakedownType;

        ProcessTakedownEvent(lpInput, lpActiveCarInterface, lpOutput, lpAggressorTakedownEvent);

        lpInstantAggresorCarData->mbWaitingOnTakedown      = false;
        lpInstantAggresorCarData->mfTimeSinceVictimCrashed = 0.0f;
        return true;
    }

    // ----------------------------------------------------------------------------------------
    // DetectStandardTakedown @0x8237A3C0 -- BrnTakedownManager.cpp:520
    // The core classifier: the crashed car is the victim; IsBeingAttacked names the aggressor from
    // the victim's shunt state; the takedown is ARMED on the aggressor (mbWaitingOnTakedown) and
    // confirmed later by ProcessQueuedTakedowns. Type: DOUBLE when the aggressor already has a
    // multiple in flight, REVENGE when the victim owed one (and the mode allows it), INTO_CAR /
    // INTO_VAN / INTO_BUS when the victim was checked into traffic, else STANDARD.
    // ----------------------------------------------------------------------------------------
    void TakedownManager::DetectStandardTakedown(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                                 GameStateModuleIO::OutputBuffer* lpOutput,
                                                 RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface,
                                                 const RaceCarCrashEvent* lpCrashEvent,
                                                 TrafficTypeResponseQueue* lpLastTrafficTypeResponseQueue)
    {
        (void)lpInput;
        (void)lpOutput;

        const EActiveRaceCarIndex leVictimActiveRaceCarIndex = GetCrashedActiveRaceCarIndex(lpCrashEvent);

        if (!lpActiveCarInterface->IsRaceCarActive(GlobalIndex(leVictimActiveRaceCarIndex)))
        {
            return;
        }

        const RaceCarState* lpVictimRaceCarState = lpActiveCarInterface->GetRaceCarState(GlobalIndex(leVictimActiveRaceCarIndex));
        EActiveRaceCarIndex leAggressorActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
        if (!IsBeingAttacked(lpVictimRaceCarState, &leAggressorActiveRaceCarIndex))
        {
            return;
        }

        CGS_ASSERT_AT(leAggressorActiveRaceCarIndex != leVictimActiveRaceCarIndex,
                   "leAggressorActiveRaceCarIndex != leVictimActiveRaceCarIndex", CGS_ARTIST_FILE, 533);                 // :533
        CGS_ASSERT_AT(leAggressorActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID,
                   "leAggressorActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID", CGS_ARTIST_FILE, 534);            // :534

        if (!lpActiveCarInterface->IsRaceCarActive(GlobalIndex(leAggressorActiveRaceCarIndex)))
        {
            return;
        }
        if (lpActiveCarInterface->GetRaceCarState(GlobalIndex(leAggressorActiveRaceCarIndex))->mbCrashing)
        {
            return;
        }

        RaceCarData* lpAggressorCarData = GetRaceCarData(leAggressorActiveRaceCarIndex);
        lpAggressorCarData->mfTimeSinceVictimCrashed = 0.0f;
        lpAggressorCarData->mbWaitingOnTakedown      = true;

        TakedownEvent* lpAggressorTakedownEvent = &lpAggressorCarData->mPendingTakedownEvent;
        ConstructTakedownEvent(*lpAggressorTakedownEvent);
        lpAggressorTakedownEvent->meAggressorIndex = leAggressorActiveRaceCarIndex;
        lpAggressorTakedownEvent->meVictimIndex    = leVictimActiveRaceCarIndex;
        lpAggressorTakedownEvent->mAggressorCarID  = lpActiveCarInterface->GetCarModelId(GlobalIndex(leAggressorActiveRaceCarIndex));
        lpAggressorTakedownEvent->mVictimCarID     = lpActiveCarInterface->GetCarModelId(GlobalIndex(leVictimActiveRaceCarIndex));

        if (lpAggressorCarData->miMultipleTakedownLength <= 1)
        {
            if (lpAggressorCarData->mabRevengeRelationships[leVictimActiveRaceCarIndex] &&
                mpModeManager->CurrentGameModeAllowsRevengeTakedowns())
            {
                lpAggressorCarData->mabRevengeRelationships[leVictimActiveRaceCarIndex] = false;
                lpAggressorTakedownEvent->meType = E_TAKEDOWN_REVENGE;
            }
            else if (static_cast<BrnWorld::EEntityTypeID>(GetCrasherEntityId(lpCrashEvent).GetOwner()) ==
                     BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE)
            {
                lpAggressorTakedownEvent->meType = GetTakedownTypeFromTrafficVehicleIndex(
                    lpLastTrafficTypeResponseQueue, GetCrasherEntityId(lpCrashEvent).GetEntityIndex());
            }
            else
            {
                lpAggressorTakedownEvent->meType = E_TAKEDOWN_STANDARD;
            }
        }
        else
        {
            lpAggressorTakedownEvent->meType = E_TAKEDOWN_DOUBLE;
        }

        mTakedownManagerDebugComponent.RecordTakedown(leAggressorActiveRaceCarIndex, leVictimActiveRaceCarIndex);
    }

    // ----------------------------------------------------------------------------------------
    // DetectNetworkTakedowns @0x823997A0 -- BrnTakedownManager.cpp:603
    // Takedowns the network module queued onto the pre-world input (remote players' events) are
    // rebuilt as remote TakedownEvents and processed like a confirmed local one, unless the victim
    // was taken down within the last KF_ONLINE_TAKEDOWN_IGNORE_TIME (flt_82CDBDBC == 5.05 s).
    // ----------------------------------------------------------------------------------------
    void TakedownManager::DetectNetworkTakedowns(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                                 RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface,
                                                 GameStateModuleIO::OutputBuffer* lpOutput)
    {
        const GameStateModuleIO::TakedownEventInputQueueType* lpOnlineTakedownQueue =
            lpInput->GetTakedownEventInputQueue();

        for (s32 liTakedown = 0; liTakedown < lpOnlineTakedownQueue->GetLength(); ++liTakedown)
        {
            const TakedownEvent& lrQueuedEvent = lpOnlineTakedownQueue->GetEvent(liTakedown);
            const EActiveRaceCarIndex leVictimActiveRaceCarIndex = lrQueuedEvent.meVictimIndex;

            if (GetRaceCarData(leVictimActiveRaceCarIndex)->mfTimeSinceLastTakenDown <= KF_ONLINE_TAKEDOWN_IGNORE_TIME)
            {
                continue;
            }

            TakedownEvent lTakedownEvent;
            ConstructTakedownEvent(lTakedownEvent);
            lTakedownEvent.meAggressorIndex    = lrQueuedEvent.meAggressorIndex;
            lTakedownEvent.meVictimIndex       = lrQueuedEvent.meVictimIndex;
            lTakedownEvent.mbMarkedManTakeDown = lrQueuedEvent.mbMarkedManTakeDown;
            lTakedownEvent.mbRemote            = true;
            lTakedownEvent.mbSettledScore      = lrQueuedEvent.mbSettledScore;
            lTakedownEvent.meType              = lrQueuedEvent.meType;

            CGS_ASSERT_AT(lTakedownEvent.meAggressorIndex != lTakedownEvent.meVictimIndex,
                       "lTakedownEvent.meAggressorIndex != lTakedownEvent.meVictimIndex", CGS_ARTIST_FILE, 633);          // :633
            CGS_ASSERT_AT(lTakedownEvent.meAggressorIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                       "lTakedownEvent.meAggressorIndex >= E_ACTIVE_RACE_CAR_INDEX_0", CGS_ARTIST_FILE, 634);             // :634
            CGS_ASSERT_AT(lTakedownEvent.meAggressorIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                       "lTakedownEvent.meAggressorIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT", CGS_ARTIST_FILE, 635);          // :635
            CGS_ASSERT_AT(lTakedownEvent.meVictimIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                       "lTakedownEvent.meVictimIndex >= E_ACTIVE_RACE_CAR_INDEX_0", CGS_ARTIST_FILE, 636);                // :636
            CGS_ASSERT_AT(lTakedownEvent.meVictimIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                       "lTakedownEvent.meVictimIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT", CGS_ARTIST_FILE, 637);             // :637

            ProcessTakedownEvent(lpInput, lpActiveCarInterface, lpOutput, &lTakedownEvent);

            RaceCarData* lpAggressorCarData = GetRaceCarData(lTakedownEvent.meAggressorIndex);
            lpAggressorCarData->mbWaitingOnTakedown      = false;
            lpAggressorCarData->mfTimeSinceVictimCrashed = 0.0f;
        }
    }

    // ----------------------------------------------------------------------------------------
    // ProcessQueuedTakedowns @0x82399A98 -- BrnTakedownManager.cpp:661
    // Confirm the takedowns DetectStandardTakedown armed: once the aggressor has waited out the
    // confirmation time without crashing itself (and, offline, is still moving) the pending event
    // is processed; either way the arm is cleared.
    // ----------------------------------------------------------------------------------------
    void TakedownManager::ProcessQueuedTakedowns(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                                 RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface,
                                                 GameStateModuleIO::OutputBuffer* lpOutput, f32 lfSimTimestep)
    {
        for (s32 liRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0; liRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liRaceCarIndex)
        {
            const EActiveRaceCarIndex leRaceCarIndex = static_cast<EActiveRaceCarIndex>(liRaceCarIndex);
            RaceCarData* lpRaceCarData = GetRaceCarData(leRaceCarIndex);
            if (!lpRaceCarData->mbWaitingOnTakedown)
            {
                continue;
            }

            lpRaceCarData->mfTimeSinceVictimCrashed += lfSimTimestep;
            // 1.0 s online, 0.1 s offline on the console.
            const f32 lfTakedownConfirmationTime = mpModeManager->IsOnlineGameMode()
                                                       ? KF_ONLINE_TAKEDOWN_CONFIRMATION_TIME
                                                       : KF_TAKEDOWN_CONFIRMATION_TIME;
            if (lpRaceCarData->mfTimeSinceVictimCrashed <= lfTakedownConfirmationTime)
            {
                continue;
            }

            if (lpActiveCarInterface->IsRaceCarActive(GlobalIndex(leRaceCarIndex)))
            {
                const RaceCarState* lpRaceCarState = lpActiveCarInterface->GetRaceCarState(GlobalIndex(leRaceCarIndex));
                if (!lpRaceCarState->mbCrashing)
                {
                    if (lpRaceCarState->mfSpeedMPH * KF_MPH_TO_METRES_PER_SECOND > KF_MIN_TAKEDOWN_SPEED ||
                        mpModeManager->IsOnlineGameMode())
                    {
                        ProcessTakedownEvent(lpInput, lpActiveCarInterface, lpOutput, &lpRaceCarData->mPendingTakedownEvent);
                    }
                }
            }

            lpRaceCarData->mfTimeSinceVictimCrashed = 0.0f;
            lpRaceCarData->mbWaitingOnTakedown      = false;
        }
    }

    // ----------------------------------------------------------------------------------------
    // ProcessTakedownEvent @0x82393D40 -- BrnTakedownManager.cpp:718
    // A confirmed takedown: marked-man tagging, the takedown camera (player aggressor, offline, mode
    // not enforcing soft takedowns), the OnPlayerTakedown action, the aggressor's chain / multiple
    // bookkeeping, then either the scoring queue (inside a mode) or the free-burn rival shutdown.
    // ----------------------------------------------------------------------------------------
    void TakedownManager::ProcessTakedownEvent(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                               RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface,
                                               GameStateModuleIO::OutputBuffer* lpOutput, TakedownEvent* lpTakedownEvent)
    {
        CGS_ASSERT_AT(lpTakedownEvent->meType != E_TAKEDOWN_NONE, "lpTakedownEvent->meType != E_TAKEDOWN_NONE", CGS_ARTIST_FILE, 721);  // :721

        const EActiveRaceCarIndex leAggressorActiveRaceCarIndex = lpTakedownEvent->meAggressorIndex;
        const EActiveRaceCarIndex leVictimActiveRaceCarIndex    = lpTakedownEvent->meVictimIndex;
        RaceCarData* lpAggressorRaceCarData = GetRaceCarData(leAggressorActiveRaceCarIndex);
        RaceCarData* lpVictimRaceCarData    = GetRaceCarData(leVictimActiveRaceCarIndex);

        CGS_ASSERT_AT(leAggressorActiveRaceCarIndex != leVictimActiveRaceCarIndex,
                   "leAggressorActiveRaceCarIndex != leVictimActiveRaceCarIndex", CGS_ARTIST_FILE, 728);                     // :728

        // Was the victim the aggressor's marked man? (Not in the online lobby / online showtime.)
        BrnNetwork::BrnNetworkModuleIO::MarkedManInterface lMarkedManInterface;
        lMarkedManInterface.Construct();
        lMarkedManInterface.SetFromPlayerStatusInterface(*lpInput->GetPlayerStatusInterface());

        const GameStateModuleIO::EGameModeType leGameModeType = mpModeManager->GetCurrentGameModeType();
        if (leGameModeType != GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY &&
            leGameModeType != GameStateModuleIO::E_MODE_ONLINE_SHOWTIME)
        {
            if (lMarkedManInterface.CheckForMarkedManTakedown(static_cast<::EActiveRaceCarIndex>(leAggressorActiveRaceCarIndex),
                                                              static_cast<::EActiveRaceCarIndex>(leVictimActiveRaceCarIndex)))
            {
                lpTakedownEvent->mbMarkedManTakeDown = true;
            }
        }

        CGS_ASSERT(mpModeManager != 0, "mpModeManager != NULL");                                   // :745

        // The takedown camera: player aggressor, offline, and the mode is not enforcing soft takedowns
        // (`extldi r12,r12,64,33` @0x82393EB8 == muFlags bit 33 == KU_FLAG_ENFORCE_SOFT_TAKEDOWNS).
        const bool lbEnforceSoftTakedowns =
            mpModeManager->GetCurrentGameMode() != 0 &&
            mpModeManager->GetCurrentGameModeParams()->GetFlag(GameModeParams::KU_FLAG_ENFORCE_SOFT_TAKEDOWNS);
        if (!lbEnforceSoftTakedowns && !mpModeManager->IsOnlineGameMode())
        {
            if (leAggressorActiveRaceCarIndex == LocalIndex(lpActiveCarInterface->GetPlayerActiveRaceCarIndex()))
            {
                StartTakedownCamera(lpOutput->GetGameActionQueue(), leVictimActiveRaceCarIndex, lpTakedownEvent->meType);
                mfPlayerSpeedAtTakedown = lpActiveCarInterface->GetPlayerRaceCarState()->mfSpeedMPH * KF_MPH_TO_METRES_PER_SECOND;
            }
        }

        if (leAggressorActiveRaceCarIndex == LocalIndex(lpActiveCarInterface->GetPlayerActiveRaceCarIndex()))
        {
            GameStateModuleIO::OnPlayerTakedownAction lPlayerTakedownAction;
            lPlayerTakedownAction.meVictimGlobalRaceCarIndex = lpActiveCarInterface->GetGlobalRaceCarIndex(GlobalIndex(leVictimActiveRaceCarIndex));
            lpOutput->GetGameActionQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lPlayerTakedownAction),
                GameStateModuleIO::E_ACTION_ON_PLAYER_TAKEDOWN,
                static_cast<s32>(sizeof(lPlayerTakedownAction)));
        }

        // The aggressor's chain / multiple counters, mirrored into the event.
        lpAggressorRaceCarData->mfTimeSinceLastTakedown = 0.0f;
        lpAggressorRaceCarData->miTakedownChainLength++;
        lpAggressorRaceCarData->miMultipleTakedownLength++;
        lpTakedownEvent->miTakedownChainCount    = lpAggressorRaceCarData->miTakedownChainLength;
        lpTakedownEvent->miMultipleTakedownCount = lpAggressorRaceCarData->miMultipleTakedownLength;

        CGS_ASSERT(mpModeManager != 0, "mpModeManager != NULL");                                   // :780

        if (mpModeManager->GetCurrentGameMode() != 0)
        {
            // Inside a mode: a second takedown inside the multiple window is a DOUBLE; the event goes
            // to the scoring side through the output buffer's takedown queue.
            if (lpAggressorRaceCarData->miMultipleTakedownLength > 1)
            {
                lpTakedownEvent->meType = E_TAKEDOWN_DOUBLE;
            }
            // FLAG cross-home cast (see DetectNetworkTakedowns): TakedownEventOutputQueueType is the
            // forward-declared name of EventQueue<TakedownEvent,8> -- the same cast GameStateModule_gUI_00.cpp
            // and BrnGameModule.cpp carry for this member.
            CgsModule::EventQueue<TakedownEvent, 8>* lpTakedownEventQueue =
                reinterpret_cast<CgsModule::EventQueue<TakedownEvent, 8>*>(lpOutput->GetTakedownEventOutputQueue());
            lpTakedownEventQueue->AddEvent(*lpTakedownEvent);
        }
        else
        {
            // Free burn: the player taking down a rival shuts that rival down (and wins the pursuit).
            if (leVictimActiveRaceCarIndex != LocalIndex(lpActiveCarInterface->GetPlayerActiveRaceCarIndex()) &&
                leAggressorActiveRaceCarIndex == LocalIndex(lpActiveCarInterface->GetPlayerActiveRaceCarIndex()))
            {
                // The console writes only the victim car id and index (mRivalID is stack residue on the
                // wire); zeroed here for reproducibility.
                GameStateModuleIO::ShutdownAction lShutdown;
                std::memset(&lShutdown, 0, sizeof(lShutdown));
                lShutdown.mVictimCarID  = lpTakedownEvent->mVictimCarID;
                lShutdown.meVictimIndex = leVictimActiveRaceCarIndex;
                lpOutput->GetGameActionQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lShutdown),
                    GameStateModuleIO::E_ACTION_SHUTDOWN,
                    static_cast<s32>(sizeof(lShutdown)));

                const CgsID lRivalId = lpActiveCarInterface->GetRivalId(GlobalIndex(leVictimActiveRaceCarIndex));
                // 0x823940E4..0x823940F8: r5 = lpOutput->GetGameActionQueue() (the write-locked queue
                // accessor), r4 = the rival id just read, r3 = `lwz 0x290(r30)` == mpProgressionManager,
                // bl ProgressionManager::OnPursuitWon @0x82389F40 -- the pursuit win: FindRivalIndexFromId
                // on the progression data, DefeatRivalAndUnlockCar, the 197 rival-state-change record and
                // the forced autosave. Body: BrnProgressionManager_Rivals.cpp. (mpProgressionManager is
                // wired by TakedownManager::Construct from GameStateModule_gTD_00.cpp, as on the console.)
                mpProgressionManager->OnPursuitWon(lRivalId, lpOutput->GetGameActionQueue());
            }
        }

        // Either way the victim is freshly taken down and now owes the aggressor a revenge takedown.
        lpVictimRaceCarData->mfTimeSinceLastTakenDown = 0.0f;
        lpVictimRaceCarData->mabRevengeRelationships[leAggressorActiveRaceCarIndex] = true;
    }

    // ----------------------------------------------------------------------------------------
    // GetTakedownTypeFromTrafficVehicleIndex @0x82366288 -- BrnTakedownManager.cpp:823
    // Look the traffic vehicle up in the last traffic-type response batch and map its class to the
    // INTO_* takedown type (bigrig / anything else falls back to INTO_CAR).
    // ----------------------------------------------------------------------------------------
    ETakedownType TakedownManager::GetTakedownTypeFromTrafficVehicleIndex(const TrafficTypeResponseQueue* lpResponseQueue,
                                                                          u16 luTrafficVehicleIndex)
    {
        BrnTraffic::VehicleClass lVehicleClass = BrnTraffic::E_VEHICLECLASS_CAR;
        s32 liResponseIndex = 0;
        for (; liResponseIndex < lpResponseQueue->GetLength(); ++liResponseIndex)
        {
            const BrnTraffic::BrnTrafficIO::TrafficTypeResponse& lResponse = lpResponseQueue->GetEvent(liResponseIndex);
            if (lResponse.muVehicleIndex == luTrafficVehicleIndex)
            {
                lVehicleClass = lResponse.meType;   // `lwz r28, 4(r3)` @0x823662DC -- the class word, not IDA's +2
                break;
            }
        }

        CGS_ASSERT_AT(liResponseIndex < lpResponseQueue->GetLength(), "Missing traffic vehicle check!", CGS_ARTIST_FILE, 842);   // :842
        CGS_ASSERT_AT(lVehicleClass >= 0 && lVehicleClass < BrnTraffic::E_VEHICLECLASS_COUNT,
                   "lVehicleClass >= 0 && lVehicleClass < BrnTraffic::E_VEHICLECLASS_COUNT", CGS_ARTIST_FILE, 845);          // :845

        ETakedownType leTakedownType;
        switch (lVehicleClass)
        {
        case BrnTraffic::E_VEHICLECLASS_VAN: leTakedownType = E_TAKEDOWN_INTO_VAN; break;
        case BrnTraffic::E_VEHICLECLASS_BUS: leTakedownType = E_TAKEDOWN_INTO_BUS; break;
        case BrnTraffic::E_VEHICLECLASS_CAR:
        default:                             leTakedownType = E_TAKEDOWN_INTO_CAR; break;
        }
        return leTakedownType;
    }
}

#undef CGS_ARTIST_FILE
