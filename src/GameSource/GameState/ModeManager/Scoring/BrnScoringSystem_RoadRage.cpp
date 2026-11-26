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
#include "rw/math/vpu/vector3_operation.h"                      // rw::math::vpu::Clamp (DWARF call list)
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"      // CgsDev::Log::gpDebugPrint (the [rr] witness)

namespace BrnGameState
{

// X360 0x823444B0. Bank one more player crash against the road-rage crash allowance, tell the
// HUD how close the car is to being totalled (action 205), request the DAMAGE_CRITICAL training
// tip on the last-but-one crash (action 149) and latch mbPlayerTotalled on the last one.
void ScoringSystem::OnRoadRagePlayerCrashed(GameStateModuleIO::OutputBuffer* lpOutput,
                                            GameStateModuleIO::ERoadRageCrashType /* leCrashType -- unread on the X360 */)
{
    CGS_ASSERT_AT(miMaximumPlayerCrashedNumber != 0, "miMaximumPlayerCrashedNumber != 0", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/ModeManager/Scoring/BrnScoringSystem.cpp", 2550);   // BrnScoringSystem.cpp:2550

    // DWARF cpp:2416..2418 locals; the clamp is rw::math::vpu::Clamp (DWARF call list: Clamp,
    // Max<float>), the two-fsel Min(1, Max(0, x)) at 0x82344560..0x82344570.
    GameStateModuleIO::RoadRagePlayerDamageAction lRoadRagePlayerDamageAction;
    f32    lfAmountWrecked;
    int8_t liAmountOfCrashesLeft;

    ++miCurrentPlayerCrashedNumber;

    liAmountOfCrashesLeft = static_cast<int8_t>(miMaximumPlayerCrashedNumber - miCurrentPlayerCrashedNumber);

    lRoadRagePlayerDamageAction.mbOneMoreCrashToTotalled = false;
    lRoadRagePlayerDamageAction.mbPlayerTotalled         = false;

    // flt_82020F70 == 0.1f (image: 3DCCCCCD).
    lfAmountWrecked = static_cast<f32>(miCurrentPlayerCrashedNumber) /
                      static_cast<f32>(miMaximumPlayerCrashedNumber) + 0.1f;
    lRoadRagePlayerDamageAction.mfHowCloseToTotalled = rw::math::vpu::Clamp(lfAmountWrecked, 0.0f, 1.0f);

    if (liAmountOfCrashesLeft == 1)
    {
        lRoadRagePlayerDamageAction.mbOneMoreCrashToTotalled = true;

        // DWARF cpp:2436 lRequestTraining; AddGameAction (BrnGameActions.h:501) inlined.
        GameStateModuleIO::RequestGameTrainingAction lRequestTraining;
        lRequestTraining.meTrainingType = BrnProgression::E_TRAINING_TYPE_DAMAGE_CRITICAL;
        lpOutput->GetGameActionQueue()->AddGameAction(lRequestTraining);
    }

    if (liAmountOfCrashesLeft == 0)
    {
        lRoadRagePlayerDamageAction.mbPlayerTotalled = true;
        mbPlayerTotalled                             = true;      // stb r29(1), 0x4B70(this)
    }

    lpOutput->GetGameActionQueue()->AddGameAction(lRoadRagePlayerDamageAction);
}

}
