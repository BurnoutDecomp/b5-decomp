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
// MINIMAL STUB: BrnGui::EFinishType has no committed home yet (only GameStateToGuiFinishedRaceEvent
// uses it). Replace with the real enum when the BrnGui front-end is reconstructed.
enum EFinishType : s32 { E_FINISH_TYPE_NONE = 0 };
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
