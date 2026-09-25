// OnlineCarSelectManager::ExitOnlineCarSelect -- the online car-select exit ProcessGameEvents'
// local-player-disconnected arm (case 123) runs when the lobby drops while the player is still
// choosing a car.

#include "GameSource/GameState/CarSelect/BrnOnlineCarSelectManager.h"

#include "GameSource/GameState/BrnGameStateModule.h"             // GameStateModule::RequestUnpause
#include "GameSource/GameState/BrnGameActions.h"                 // actions 76 / 77 / 7
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"      // CgsDev::Log::gpDebugPrint, CgsDev::Message::gxMessageFilterFlags

namespace BrnGameState
{
// 66 console asm lines. In the console's order:
//   the filter-gated "RG :: CS Manager : ExitOnlineCarSelect" line;
//   meInternalState = NONE (+0), meStateOfChangingCars = NONE (+0x64),
//   mCacheDuringChangeCarId = 0 (+0x58), mbIsInOnlineCarSelect = false (+0x14);
//   action 76 {ONLINE_EVENT_START (2), leaving}                  size 8
//   action 77 {online car select = 1 at +0x10}                   size 32
//   action 7  {entity-module driver (1), not a drive-thru}       size 48
//   GameStateModule::RequestUnpause(1, queue) on the owner.
// The console builds 77 and 7 in one reused stack buffer and writes only the fields listed; the
// rest of each record is whatever the stack held. No reader looks at those bytes (the director
// reads +0x10 of 77; the world reads the box region of 7 only for a drive-thru), so the records
// are value-initialised here.
void OnlineCarSelectManager::ExitOnlineCarSelect(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "RG :: CS Manager : " << "ExitOnlineCarSelect\n" << "\n";
    }

    meInternalState         = E_INTERNAL_STATE_NONE;
    meStateOfChangingCars   = E_CAR_CHANGE_NONE;
    mCacheDuringChangeCarId = 0;
    mbIsInOnlineCarSelect   = false;

    GameStateModuleIO::CarSelectModificationScreen lModificationScreen;
    lModificationScreen.meCarSelectType = GameStateModuleIO::E_CAR_SELECT_TYPE_ONLINE_EVENT_START;
    lModificationScreen.mbEntering      = false;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lModificationScreen),
                            GameStateModuleIO::E_ACTION_CAR_SELECT_MODIFICATION_SCREEN,
                            sizeof(lModificationScreen));

    GameStateModuleIO::CarSelectExitAction lExitAction = {};
    lExitAction.mbOnlineCarSelect = true;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lExitAction),
                            GameStateModuleIO::E_ACTION_CAR_SELECT_FINISHED,
                            sizeof(lExitAction));

    GameStateModuleIO::SetPlayerCarDriverAction lDriverAction = {};
    lDriverAction.meCarControl  = BrnWorld::E_CAR_CONTROL_ENTITY_MODULE;
    lDriverAction.mbIsDriveThru = false;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lDriverAction),
                            GameStateModuleIO::E_ACTION_SET_PLAYER_CAR_DRIVER,
                            sizeof(lDriverAction));

    mpGameStateModule.Get()->RequestUnpause(1, lpActionQueue);
}
}
