// ============================================================================================
// b5-decomp/src/GameSource/GameState/Progression/BrnProgressionManager_PreWorldUpdate.cpp
// ============================================================================================
// [issue #10 "progression: miles driven not recorded when driving", 2026-09-06]
// The per-frame tick of BrnProgression::ProgressionManager and the distance integrator it drives.
//
//   PreWorldUpdate    @0x823A4F68  (DWARF BrnProgressionManager.cpp:304; asserts :343 / :382)
//   AddDistanceDriven @0x823668F0  (DWARF BrnProgressionManager.cpp:3523)
//
// WHY THE ODOMETER READ 0.0 km. AddDistanceDriven is the ONLY writer in the image of every
// "distance driven" number the game shows -- Profile::mfDistanceDrivenOnline / Offline, the
// per-car-type tally, and the current LiveryData::mfDistanceDriven that GameStateModule::
// CopyScoringDataToOutput @0x8236CDC0 publishes as ScoringOutputInterface::
// mfDistanceDrivenInCurrentCar -> GuiEventCurrentStatus (492) -> GuiCache::mfDistanceDriven ->
// OdometerComponent::Update. Its sole caller is PreWorldUpdate, which GameStateModule::
// PreWorldUpdate @0x823A5328 runs every in-game frame (the `(lUpdateSet & 8)` leg, right after
// TriggerQueryManager::PreWorldUpdate). Neither function existed in the tree and the pump's own
// banner recorded the call as "NOT STAGED ... nothing in the junction/start chain reads what it
// writes". The whole publish chain downstream was already live; the source was silent.
//
// A partfile, not BrnProgressionManager.cpp, so this lane stays file-disjoint from the other
// ProgressionManager partfiles (the _Completion / _EventFinish / _Unlocks / _Rivals precedent).
// Every member is reached BY NAME; the console offsets are quoted to show which member each
// load lands on. The embedded Profile is this+0x170 on the console (`addi r30, r31, 0x170`).
//
// WHAT IS PARKED (named, not faked -- each parks ONCE per run in the log):
//   UpdatePlayerMedals @0x8239FE50 / UnlockRivals / CheckForAllModeTypeCompletion /
//   UpdateRivals / SendTrophyUnlockUpdate @0x823892B8 -- none has a body in the tree. Their
//   GATES (the request bytes and the two 2 s holds) run exactly as the console runs them, so
//   the state machine around them is real and the day a body lands it is a one-line un-park.
// ============================================================================================
#include "GameSource/GameState/Progression/BrnProgressionManager.h"

#include <stdlib.h>                                                       // getenv (the [odometer] witness gate)

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CgsDev::Assert::{Begin,Fire,End}Assert
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // CgsDev::Log::gpDebugPrint
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // CgsDev::PerfMonCpu::Start/StopMonitor
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // GameActionQueue (VariableEventQueue<13312,16>) ::AddEvent
#include "GameSource/GameState/Progression/BrnProfile.h"                  // Profile::Add* / SetCar* (inline)
#include "GameSource/GameState/Progression/BrnProgressionLiveryData.h"    // LiveryData::mfDistanceDriven
#include "GameSource/GameState/BrnGameStateModuleIO.h"                    // OutputBuffer::GetGameActionQueue
#include "GameSource/GameState/BrnGameActions.h"                          // E_ACTION_REQUEST_AUTOSAVE (55) / E_ACTION_ALL_RIVALS_SHUTDOWN (210)
#include "GameSource/GameState/ModeManager/BrnModeManager.h"              // ModeManager::IsOnlineGameMode
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntityActiveRaceCarOutputInterface + RaceCarState

namespace BrnProgression
{

namespace GsmIO = BrnGameState::GameStateModuleIO;

namespace
{
    // The console's own baked assert location (BeginAssert/FireAssert/EndAssert is called
    // directly rather than through CGS_ASSERT so the file/line stay the binary's, not this
    // TU's -- the _GameStats / _EventFinish partfiles' treatment).
    const char* const KAC_PROGRESSION_MANAGER_CPP =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Progression/BrnProgressionManager.cpp";

    // IMAGE-CITED CONSTANTS (big-endian dump of image.bin, offset == VA - 0x82000000):
    //   flt_82F31928 = 0x3EE4E9C0 == 0.447039992f -- MPH -> m/s. The same rodata word the
    //                  TakedownManager / BystanderCam / SimpleVehiclePhysics reconstructions
    //                  cite; RaceCarState::mfSpeedMPH * this * simStep == metres this step.
    //   flt_82001D9C = 0x40000000 == 2.0f  -- the two HUD-message holds (seconds of sim time).
    //   flt_82001CC0 = 0x00000000 == 0.0f  -- the hold reset.
    const f32 KF_MPH_TO_METRES_PER_SECOND     = 0.447039992f;   // flt_82F31928
    const f32 KF_HUD_MESSAGE_HOLD_SECONDS     = 2.0f;           // flt_82001D9C

    // One-shot park reporters (campaign house rule: a park must be visible, not silent). The
    // same shape as _EventFinish.cpp's; that one lives in its own anonymous namespace.
    void ParkOnce(bool& lrbAlreadySaid, const char* lpcMessage)
    {
        if (lrbAlreadySaid)
        {
            return;
        }
        lrbAlreadySaid = true;
        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint << lpcMessage;
        }
    }
    bool gbSaidMedals       = false;
    bool gbSaidAllModeTypes = false;
    bool gbSaidUpdateRivals = false;
    bool gbSaidTrophyUpdate = false;

    // [FLAG PC witness] the `[odometer] car=<m> offline=<m> online=<m> incar=<s> real=<s>`
    // line -- NOT IN THE X360 BINARY. Opt-in (BRN_ODOMETER_DIAG), once per second of sim time,
    // capped at 400 lines a run; tools/tests/cases/progression_odometer.ps1 reads it against
    // the HUD-side `[odometer] hud=` line and the harness' own [motion] path length.
    bool OdometerDiagEnabled()
    {
        static const bool sbDiag = (getenv("BRN_ODOMETER_DIAG") != 0);
        return sbDiag && CgsDev::Log::gpDebugPrint != 0;
    }
}

// --------------------------------------------------------------------------------------------
// AddDistanceDriven (X360 0x823668F0).
//   fabs f0, f1                                  -- the metres, magnitude only
//   if (lbOnline)  Profile::AddDistanceDrivenOnline(f0)     `lfs/fadds/stfs 0x1D4(this)`
//   else           Profile::AddDistanceDrivenOffline(f0)    `0x68(profile)` + the per-car-type
//                                                             tally at profile+0x1CCB0+4*type
//   if (mpCurrentLiveryData)  mpCurrentLiveryData->mfDistanceDriven += f0
//                                                `lwz r10, 0x208D4(this) ; lfs/fadds/stfs 0x10`
// The DWARF hint list for :3523 is exactly {fpu::Abs, AddDistanceDrivenOffline,
// AddDistanceDrivenOnline}; the livery tail is the inlined `(*this+133332)->+0x10` the header
// already documents on GetCurrentLiveryData.
// --------------------------------------------------------------------------------------------
void ProgressionManager::AddDistanceDriven(f32 lfDistance, bool lbOnline)
{
    const f32 lfMetres = (lfDistance < 0.0f) ? -lfDistance : lfDistance;   // fabs

    if (lbOnline)
    {
        mProfile.AddDistanceDrivenOnline(lfMetres);
    }
    else
    {
        mProfile.AddDistanceDrivenOffline(lfMetres);
    }

    LiveryData* lpLiveryData = GetCurrentLiveryData();          // this+133332 (0x208D4)
    if (lpLiveryData != 0)
    {
        lpLiveryData->mfDistanceDriven += lfMetres;             // +0x10
    }
}

// --------------------------------------------------------------------------------------------
// PreWorldUpdate (X360 0x823A4F68). Register map from the prologue: r31 = this, f31 = f1 =
// lfSimTimeStep, f30 = f2 = lfGameTimeStep, r30 = r6 = lpOutputBuffer (then re-used as the
// Profile base `addi r30, r31, 0x170`), r29 = r7 = lpActiveRaceCarInterface, r28 = r8 =
// lUpdateSet, r27 = r9 = lbIsInJunkyard, r23 = &miPreWorldUpdate, r24 = the action queue.
// --------------------------------------------------------------------------------------------
void ProgressionManager::PreWorldUpdate(
    f32 lfSimTimeStep, f32 lfGameTimeStep,
    GsmIO::OutputBuffer* lpOutputBuffer,
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
    BrnUpdateSet lUpdateSet, bool lbIsInJunkyard)
{
    CgsDev::PerfMonCpu::StartMonitor(miPreWorldUpdate);                       // lwz r3, 0(r23)

    GsmIO::GameActionQueue* lpGameActionQueue = lpOutputBuffer->GetGameActionQueue();   // r24

    // `lfs/fadds/stfs 0x1CD28(profile)` on f30 -- the real-time counter ticks on the GAME step,
    // paused or not, in the junkyard or not.
    mProfile.AddRealTimePlayed(lfGameTimeStep);

    // ---- the player-car arm ---------------------------------------------------------------
    // IsPlayerCarActive() is inlined on the console (the :967 "mePlayerActiveRaceCarIndex <
    // E_ACTIVE_RACE_CAR_INDEX_COUNT" assert then `idx != -1 ? +0x2860 : 0`); the accessor body
    // carries that assert. `clrlwi r11, r28, 31` == lUpdateSet bit 0, the network catch-up step
    // Physics/AI test the same way: no time or distance is booked on a catch-up step.
    if (lpActiveRaceCarInterface->IsPlayerCarActive() && (lUpdateSet & 1) == 0)
    {
        const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState =
            lpActiveRaceCarInterface->GetRaceCarState(
                lpActiveRaceCarInterface->GetPlayerActiveRaceCarIndex());
        if (lpRaceCarState == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpRaceCarState", KAC_PROGRESSION_MANAGER_CPP, 343);
            CgsDev::Assert::EndAssert();
        }

        // IsPlayerEngineOn() inlined: `idx != -1 && +0x285C == 2` (RUNNING). In-car time ticks on
        // the GAME step, `lfs/fadds/stfs 0x6C(profile)`.
        if (lpActiveRaceCarInterface->IsPlayerEngineOn())
        {
            mProfile.AddInCarTimePlayed(lfGameTimeStep);
        }

        // ModeManager::IsOnlineGameMode() inlined: `lwzx r11, this, 0x2093C ; lwz r11, 0xD98(r11)`
        // (mpModeManager->mpCurrentGameMode) then `lbz 0xAC` (GameMode::mbIsOnline) when non-null.
        // [PC GUARD] the console dereferences mpModeManager unconditionally; Prepare2 installs
        // it here, and a null one on a PC boot path is a silent "offline" rather than an AV.
        const bool lbOnlineGameMode = (mpModeManager != 0) && mpModeManager->IsOnlineGameMode();

        if (lbIsInJunkyard)
        {
            // The spawn pose: `lvx128 v0, rcs, 0x220 ; stvx128 v0, this, 0x1A0` (mTransform.wAxis
            // -> Profile::mCarPosition) and `0x210 -> 0x1B0` (mTransform.zAxis -> mCarDirection),
            // only when no online mode is running.
            if (!lbOnlineGameMode)
            {
                mProfile.SetCarPosition(lpRaceCarState->mTransform.wAxis);
                mProfile.SetCarDirection(lpRaceCarState->mTransform.zAxis);
            }
        }
        else
        {
            // `lfs f13, 0x3CC(rcs)` (RaceCarState::mfSpeedMPH) * flt_82F31928 * f31 (the SIM
            // step) -> metres this step; the online flag rides r5.
            AddDistanceDriven(lpRaceCarState->mfSpeedMPH * KF_MPH_TO_METRES_PER_SECOND * lfSimTimeStep,
                              lbOnlineGameMode);
        }
    }

    // ---- the deferred legs (each gated exactly as the console gates it) --------------------

    // `lbzx +0x20973` -- mbPlayerMedalsUpdateRequired -> UpdatePlayerMedals + UnlockRivals.
    if (mbPlayerMedalsUpdateRequired)
    {
        // ⛔ PARK -- console: `UpdatePlayerMedals(lpGameActionQueue); UnlockRivals(lpGameActionQueue);`
        // (@0x823A5154 / @0x823A5160). Neither has a body in the tree (the _EventFinish partfile
        // parks the same pair at its own seat). The byte stays set, as it would until the
        // callee cleared it. DELETE-WHEN 0x8239FE50 has a body.
        ParkOnce(gbSaidMedals,
                 "[FLAG PC bring-up] ProgressionManager::PreWorldUpdate: the medals arm "
                 "(UpdatePlayerMedals @0x8239FE50 + UnlockRivals) is NOT reconstructed; "
                 "mbPlayerMedalsUpdateRequired stays set.\n");
    }

    // `lbz +0x20975` then `lbz +0x20976` -- the all-win-types check runs a 2 s hold on the SIM step.
    if (mbCheckAllWinTypesPending)
    {
        if (mbCheckAllWinTypesArmed)
        {
            mfTimeTillAllEventTypeCompleteHudMessage += lfSimTimeStep;          // +0x2097C
            if (mfTimeTillAllEventTypeCompleteHudMessage > KF_HUD_MESSAGE_HOLD_SECONDS)
            {
                if (meModeToCheckForAllWinTypes == -1)                          // E_MODE_INVALID
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        "meModeToCheckForAllWinTypes != RaceEventData::E_MODE_INVALID",
                        KAC_PROGRESSION_MANAGER_CPP, 382);
                    CgsDev::Assert::EndAssert();
                }
                // ⛔ PARK -- console: `CheckForAllModeTypeCompletion(lpGameActionQueue,
                // meModeToCheckForAllWinTypes);` @0x823A51EC. No body in the tree.
                ParkOnce(gbSaidAllModeTypes,
                         "[FLAG PC bring-up] ProgressionManager::PreWorldUpdate: "
                         "CheckForAllModeTypeCompletion() is NOT reconstructed; the "
                         "all-win-types HUD message for this mode was not evaluated.\n");
                mbCheckAllWinTypesPending                = false;               // stb 0 -> +0x20975
                mbCheckAllWinTypesArmed                  = false;               // stb 0 -> +0x20976
                mfTimeTillAllEventTypeCompleteHudMessage = 0.0f;                // flt_82001CC0
                meModeToCheckForAllWinTypes              = -1;                  // li r11, -1
            }
        }
    }

    // `lbz +0x20981` then `lbz +0x20980` -- the all-rivals-beaten message, 2 s hold on the SIM step,
    // then AllRivalsShutDownAction (id 210, size 1). The console posts var_70 WITHOUT storing to
    // it first on this arm (only the autosave arm below writes the byte); a zero is the one
    // value that cannot be "the wrong stack garbage".
    if (mbShowShutDownAllIfNeeded)
    {
        if (mbNeedToShowAllRivalsBeatenMessage)
        {
            mfTimeTillShowAllRivalsBeatenMessage += lfSimTimeStep;              // +0x20984
            if (mfTimeTillShowAllRivalsBeatenMessage > KF_HUD_MESSAGE_HOLD_SECONDS)
            {
                u8 lacAllRivalsShutDown[1] = { 0 };
                lpGameActionQueue->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(lacAllRivalsShutDown),
                    GsmIO::E_ACTION_ALL_RIVALS_SHUTDOWN, 1);                    // li r5,0xD2 ; li r6,1
                mbShowShutDownAllIfNeeded          = false;                     // stb 0 -> +0x20981
                mbNeedToShowAllRivalsBeatenMessage = false;                     // stb 0 -> +0x20980
            }
        }
    }

    // `lbz +0x2096F` -- the FORCED autosave: RequestAutoSaveAction (id 55, size 1) with the
    // payload byte set to 1 (`li r11,1 ; stb r11, var_70`), then the latch clears.
    if (mbAutosaveRequested)
    {
        u8 lacRequestAutosave[1] = { 1 };
        lpGameActionQueue->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(lacRequestAutosave),
            GsmIO::E_ACTION_REQUEST_AUTOSAVE, 1);                               // li r5,0x37 ; li r6,1
        mbAutosaveRequested = false;                                            // stb 0 -> +0x2096F
    }

    // `lbzx +0x20971` -- mbUpdateRivals -> UpdateRivals(lpGameActionQueue).
    if (mbUpdateRivalsRequested)
    {
        // ⛔ PARK -- console: `UpdateRivals(lpGameActionQueue);` @0x823A52BC. No body in the
        // tree; the byte stays set as it would until the callee cleared it.
        ParkOnce(gbSaidUpdateRivals,
                 "[FLAG PC bring-up] ProgressionManager::PreWorldUpdate: UpdateRivals() is "
                 "NOT reconstructed; mbUpdateRivalsRequested stays set.\n");
    }

    // `lwz r11, 0xC0(this+0x20808)` -- mQueueOfTrophyCarUnLocks' count word; the CgsArray.h:336
    // "Array used before Construct/Clear was called" assert is GetLength's own (the tree's
    // Array::GetLength fires the identical string on the -1 sentinel), then `> 0`.
    if (mQueueOfTrophyCarUnLocks.GetLength() > 0)
    {
        // ⛔ PARK -- console: `SendTrophyUnlockUpdate(lpGameActionQueue);` @0x823892B8 (posts the
        // tail element as game action 204 and Erases it). No body in the tree, so the queue keeps
        // its entries; the _Unlocks partfile that Appends them already documents the same park.
        ParkOnce(gbSaidTrophyUpdate,
                 "[FLAG PC bring-up] ProgressionManager::PreWorldUpdate: SendTrophyUnlockUpdate() "
                 "is NOT reconstructed; queued trophy-car unlocks are not posted.\n");
    }

    // [FLAG PC witness] see OdometerDiagEnabled.
    if (OdometerDiagEnabled())
    {
        static f32 sfSinceLastLine  = 1.0e9f;   // print on the first frame
        static s32 siPrintedLines   = 0;
        sfSinceLastLine += lfSimTimeStep;
        if (sfSinceLastLine >= 1.0f && siPrintedLines < 400)
        {
            sfSinceLastLine = 0.0f;
            ++siPrintedLines;
            const LiveryData* lpLiveryData = GetCurrentLiveryData();
            *CgsDev::Log::gpDebugPrint
                << "[odometer] car="   << ((lpLiveryData != 0) ? lpLiveryData->mfDistanceDriven : -1.0f)
                << " offline="         << mProfile.GetDistanceDrivenOffline()
                << " online="          << mProfile.GetDistanceDrivenOnline()
                << " incar="           << mProfile.GetInCarTimePlayed()
                << " real="            << mProfile.GetRealTimePlayed()
                << " active="          << (lpActiveRaceCarInterface->IsPlayerCarActive() ? 1 : 0)
                << "\n";
        }
    }

    CgsDev::PerfMonCpu::StopMonitor(miPreWorldUpdate);                        // lwz r3, 0(r23)
}

}   // namespace BrnProgression
