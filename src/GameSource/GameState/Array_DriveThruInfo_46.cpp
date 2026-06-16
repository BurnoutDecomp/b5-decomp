#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/GameState/BrnGameActions.h"   // SetUpAllDriveThrusAction::DriveThruInfo element home

// Explicit instantiation(s) of the generic Array<T,N> container methods (inline in CgsArray.h)
// for the SetUpAllDriveThrusAction::DriveThruInfo,46 leaf instantiation -- the committed
// Array_/EventQueue_ explicit-instantiation pattern.
//   X360 0x8235C8E8 = Array<...DriveThruInfo,46u>::Append (ledger "::DriveThruIn")
//   X360 0x823AC200 = Array<...DriveThruInfo,46u>::GetLength (ledger "::Dri")
template void Array<BrnGameState::GameStateModuleIO::SetUpAllDriveThrusAction::DriveThruInfo, 46>::Append(
    const BrnGameState::GameStateModuleIO::SetUpAllDriveThrusAction::DriveThruInfo&);
template u32 Array<BrnGameState::GameStateModuleIO::SetUpAllDriveThrusAction::DriveThruInfo, 46>::GetLength() const;
