#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                  // CgsID
#include "GameSource/BurnoutConstants.h"                     // EActiveRaceCarIndex
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"  // BrnNetwork::EPaybackType

// Owning header for the GameState -> GUI notification event records (DWARF: BrnGameStateToGuiEvents.h)
// reconstructed by the GameMode leaf batch. These are the element types of the EventQueue<T,N>
// instantiations in this directory; flat PODs (no event base on the X360 spine). Single owner --
// grow in place, do not fork in the per-instantiation .cpps.

namespace BrnGui
{
// DWARF BrnGuiEventTypeDefs.h:893 -- how a car's event ended: its race place, or the outcome of a
// target mode. Homed here rather than in the GUI event header because its producer,
// ModeManager::FinishCurrentMode, is game-state code (GameStateToGuiFinishedRaceEvent, GUI 372).
// X360 FinishCurrentMode @0x8234B978 writes exactly these values into r30, the argument of
// AddFinishedRaceEvent (bl @0x8234BE0C): the race place minus one, range-asserted against 0 and 7
// (`cmpwi r30, 0` @0x8234BABC "leFinishType >= BrnGui::E_FINISH_TYPE_1ST", `cmpwi r30, 7`
// @0x8234BAE0 "... <= BrnGui::E_FINISH_TYPE_8TH"), `li r30, 8` (0x8234BA4C, the timed-out arm),
// `li r30, 9` / `li r30, 0xA` (0x8234BBD8 / 0x8234BBE0, won / lost) and `li r30, 0xB` in the
// unknown-mode default (0x8234BDA4). (A single invented E_FINISH_TYPE_NONE stood here before.)
enum EFinishType : s32
{
    E_FINISH_TYPE_1ST       = 0,
    E_FINISH_TYPE_2ND       = 1,
    E_FINISH_TYPE_3RD       = 2,
    E_FINISH_TYPE_4TH       = 3,
    E_FINISH_TYPE_5TH       = 4,
    E_FINISH_TYPE_6TH       = 5,
    E_FINISH_TYPE_7TH       = 6,
    E_FINISH_TYPE_8TH       = 7,
    E_FINISH_TYPE_TIMED_OUT = 8,
    E_FINISH_TYPE_WON       = 9,
    E_FINISH_TYPE_LOST      = 10,
    E_FINISH_TYPE_COUNT     = 11,
};
}

namespace BrnGameState
{
// DWARF BrnGameStateToGuiEvents.h:96 -- sizeof 8.
struct GameStateToGuiOvertakeEvent
{
    u8                  mu8NewPosition;        // 0x00
    EActiveRaceCarIndex meActiveRaceCarIndex;  // 0x04
};

// DWARF BrnGameStateToGuiEvents.h:41 -- the "new dirty trick" notification (3 enum fields).
struct GameStateToGuiNewDirtyTrick
{
    EActiveRaceCarIndex      meAggressorActiveRaceCarIndex;
    EActiveRaceCarIndex      meVictimActiveRaceCarIndex;
    BrnNetwork::EPaybackType meTrickType;
};

// DWARF BrnGameStateToGuiEvents.h:59 -- "dirty trick triggered".
struct GameStateToGuiTriggeredDirtyTrick
{
    EActiveRaceCarIndex      meAggressorActiveRaceCarIndex;
    EActiveRaceCarIndex      meVictimActiveRaceCarIndex;
    BrnNetwork::EPaybackType meTrickType;
};

// DWARF BrnGameStateToGuiEvents.h (dirty-trick ending) -- adds the survived flag.
struct GameStateToGuiEndingDirtyTrick
{
    EActiveRaceCarIndex      meAggressorActiveRaceCarIndex;
    EActiveRaceCarIndex      meVictimActiveRaceCarIndex;
    BrnNetwork::EPaybackType meTrickType;
    bool                     mbSurvived;
};

// DWARF BrnGameStateToGuiEvents.h -- race-finished notification. sizeof 8.
struct GameStateToGuiFinishedRaceEvent
{
    BrnGui::EFinishType meFinishType;
    EActiveRaceCarIndex meActiveRaceCarIndex;
};

// DWARF BrnGameStateToGuiEvents.h:157 -- "on tail of rival". CgsID (u64) -> sizeof 16.
struct GameStateToGuiOnTailEvent
{
    CgsID               mOfflineRivalCarID;
    EActiveRaceCarIndex meOnTailActiveRaceCarIndex;
};

// DWARF BrnGameStateToGuiEvents.h:142 -- "took last place". sizeof 16.
struct GameStateToGuiTookLastEvent
{
    CgsID               mOfflineRivalCarID;
    EActiveRaceCarIndex meActiveRaceCarIndex;
};

// DWARF BrnGameStateToGuiEvents.h -- "took the lead" (mirror of TookLast). sizeof 16.
struct GameStateToGuiTookLeadEvent
{
    CgsID               mOfflineRivalCarID;
    EActiveRaceCarIndex meActiveRaceCarIndex;
};
}
