// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnScoringSystem_RoadRage.cpp
// ============================================================================
// Partfile of the BrnGameState::ScoringSystem keystone (BrnScoringSystem.h): the road-rage
// player-crash hook. It was the one BLOCKED body in BrnScoringSystem_Timer.cpp's banner
// ("dereferences the GameStateModuleIO::OutputBuffer param ... and
// CgsModule::VariableEventQueue<13312,16> (no committed home)"); both homes exist now --
// OutputBuffer::GetGameActionQueue() (BrnGameStateModuleIO.h, X360 0x8231D4B8, bodied in
// BrnGameStateModuleIO.cpp) and the GameActionQueue typedef (BrnGameStateSharedIO.h) -- so
// the body lands here, in its own partfile, next to the counters it drives
// (ResetRoadRageCrashesForPlayer / GetRoadRagePlayerCrashes / GetPlayerCrashesRemaining in
// BrnScoringSystem_Timer.cpp).
//
// X360 0x823444B0  ScoringSystem::OnRoadRagePlayerCrashed(OutputBuffer*, ERoadRageCrashType)
// Register map: r31 == this, r28 == lpOutput. r5 (leCrashType) is NEVER read -- the parameter
// is dead on the X360 and is left unnamed below.
//
//   0x823444C4  lwz r11, 0x4B58(this) ; miMaximumPlayerCrashedNumber == 0 -> assert (line 2550)
//   0x823444F0  lwz r11, 0x4B5C       ; r11 = miCurrentPlayerCrashedNumber + 1
//   0x82344518  stw r11, 0x4B5C       ; store it back
//   0x82344514  subf r10, r11, r10    ; remaining = miMaximumPlayerCrashedNumber - new current
//   0x82344524  extsb r30, r10        ; ... kept as an s8 (a DWARF-era int8_t local)
//   0x82344508  stb 0 -> record+4 ; stb 0 -> record+5    (both flag bytes cleared first)
//   0x82344528..0x82344574  ratio:
//       f13 = (f32)(s32)newCurrent ; f0 = (f32)max ; f13 = f13 / f0
//       f0  = f13 + flt_82020F70                       ; == 0.1f   (image: 3DCCCCCD)
//       fneg f12,f0 ; fsel f0, f12, flt_82001CC0, f0   ; x = (x <= 0.0f) ? 0.0f : x  (image: 0)
//       f12 = flt_82001C98 - f0 ; fsel f0, f12, f0, f13 ; x = (1.0f - x >= 0) ? x : 1.0f (image: 3F800000)
//       stfs f0 -> record+0                            ; mfHowCloseToTotalled
//   0x82344534/0x82344578  cmpwi r30, 1 ; bne -> skip:
//       0x8234457C  li r11, 0x39 (57)  -> training record        [E_TRAINING_TYPE_DAMAGE_CRITICAL]
//       0x82344580  stb 1 -> record+4                            [mbOneMoreCrashToTotalled]
//       0x8234458C  bl OutputBuffer::GetGameActionQueue(lpOutput)
//       0x8234459C  AddEvent(queue, &training, 0x95 == 149, 4)   [RequestGameTrainingAction]
//   0x823445A0  cmpwi r30, 0 ; bne -> skip:
//       0x823445A8  stb 1 -> record+5                            [mbPlayerTotalled]
//       0x823445AC  stb 1, 0x4B70(this)                          [this->mbPlayerTotalled]
//   0x823445B4  bl OutputBuffer::GetGameActionQueue(lpOutput)
//   0x823445C4  AddEvent(queue, &record, 0xCD == 205, 8)         [RoadRagePlayerDamageAction]
//
// The record layout {+0 f32, +4 bool, +5 bool} is the DWARF's RoadRagePlayerDamageAction
// (BrnGameActions.h:2455-2457), now homed in BrnGameActions.h with the 8-byte size pinned.
// ============================================================================

#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"

#include "GameSource/GameState/BrnGameStateModuleIO.h"           // GameStateModuleIO::OutputBuffer::GetGameActionQueue
#include "GameSource/GameState/BrnGameActions.h"                 // RoadRagePlayerDamageAction / RequestGameTrainingAction
#include "SharedClasses/Progression/BrnTrainingTypes.h"          // BrnProgression::E_TRAINING_TYPE_DAMAGE_CRITICAL (57)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h" // CgsModule::VariableEventQueue<13312,16>::AddEvent
#include "GameShared/GameClasses/Core/CgsAssert.h"               // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"      // CgsDev::Log::gpDebugPrint (the [rr] witness)

namespace BrnGameState
{

// X360 0x823444B0. Bank one more player crash against the road-rage crash allowance, tell the
// HUD how close the car is to being totalled (action 205), request the DAMAGE_CRITICAL training
// tip on the last-but-one crash (action 149) and latch mbPlayerTotalled on the last one.
void ScoringSystem::OnRoadRagePlayerCrashed(GameStateModuleIO::OutputBuffer* lpOutput,
                                            GameStateModuleIO::ERoadRageCrashType /* leCrashType -- unread on the X360 */)
{
    // [rr] PC witness (NOT X360), one line per crash: proves the crash arm reaches the scorer.
    if (CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint << "[rr] OnRoadRagePlayerCrashed: crashes " << miCurrentPlayerCrashedNumber + 1
                                   << " of " << miMaximumPlayerCrashedNumber << " [FLAG PC witness]\n";
    }
    CGS_ASSERT(miMaximumPlayerCrashedNumber != 0, "miMaximumPlayerCrashedNumber != 0");   // BrnScoringSystem.cpp:2550

    ++miCurrentPlayerCrashedNumber;

    // The console narrows the remaining count to a signed byte (`extsb r30, r10`) before the
    // two compares below; reproduced so the compare domain is the console's.
    const s8 liCrashesRemaining = static_cast<s8>(miMaximumPlayerCrashedNumber - miCurrentPlayerCrashedNumber);

    GameStateModuleIO::RoadRagePlayerDamageAction lDamageAction;
    lDamageAction.mbOneMoreCrashToTotalled = false;
    lDamageAction.mbPlayerTotalled         = false;

    // current/max + 0.1, clamped to [0, 1] by the two-fsel idiom (constants dumped from the image:
    // flt_82020F70 == 0.1f, flt_82001CC0 == 0.0f, flt_82001C98 == 1.0f).
    f32 lfHowCloseToTotalled = static_cast<f32>(miCurrentPlayerCrashedNumber) /
                               static_cast<f32>(miMaximumPlayerCrashedNumber) + 0.1f;
    if (-lfHowCloseToTotalled >= 0.0f)
    {
        lfHowCloseToTotalled = 0.0f;
    }
    if (!(1.0f - lfHowCloseToTotalled >= 0.0f))
    {
        lfHowCloseToTotalled = 1.0f;
    }
    lDamageAction.mfHowCloseToTotalled = lfHowCloseToTotalled;

    if (liCrashesRemaining == 1)
    {
        lDamageAction.mbOneMoreCrashToTotalled = true;

        GameStateModuleIO::RequestGameTrainingAction lTrainingRequest;
        lTrainingRequest.meTrainingType = BrnProgression::E_TRAINING_TYPE_DAMAGE_CRITICAL;
        lpOutput->GetGameActionQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lTrainingRequest),
                                                 GameStateModuleIO::E_ACTION_REQUEST_GAME_TRAINING,
                                                 sizeof(lTrainingRequest));
    }

    if (liCrashesRemaining == 0)
    {
        lDamageAction.mbPlayerTotalled = true;
        mbPlayerTotalled               = true;      // stb r29(1), 0x4B70(this)
    }

    lpOutput->GetGameActionQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lDamageAction),
                                             GameStateModuleIO::E_ACTION_ROAD_RAGE_PLAYER_DAMAGE,
                                             sizeof(lDamageAction));
}

}
