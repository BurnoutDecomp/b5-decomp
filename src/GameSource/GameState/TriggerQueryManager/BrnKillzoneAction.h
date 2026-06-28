#ifndef BRN_KILLZONE_ACTION_H
#define BRN_KILLZONE_ACTION_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                       // CgsID (u64)
#include "GameShared/GameClasses/Containers/CgsArray.h"           // Array<T,N>
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::Event (game-action base)

// ============================================================================
// MINIMAL OWNING SLICE for BrnGameState::GameStateModuleIO::KillzoneAction -- the game-action
// the trigger dispatcher posts when the player enters a killzone region. It carries the killzone's
// list of CgsID region ids that the killzone applies to.
//
// SHAPE (X360-attested via TriggerQueryManager::ProcessPlayerTriggers @0x8239BF80 and the DWARF
// BrnTriggerQueryManager.cpp:975 `KillzoneAction lAction; Array<CgsID,32>::Construct/Append`):
// the action's payload is an Array<CgsID,32> region-id list. The X360 posts it onto the
// game-action VariableEventQueue<13312,16> with event type 110 (0x6E) and record size 264
// (0x108) -- which equals sizeof(Array<CgsID,32>) (32*8 + 4 count, padded to 8) plus the
// CgsModule::Event base (empty -> EBO), i.e. 264 bytes.
//
// FLAG (minimal slice): the real KillzoneAction in the full game-action union almost certainly
// carries more than the region-id list (a kind discriminant, the killzone id, etc.); only the
// region-id list this TU fills is modelled here, sized so the 264-byte event record matches. The
// canonical home is the game-action header (GameStateModuleIO); promote/merge there when the full
// game-action set lands. Declared in namespace BrnGameState::GameStateModuleIO to match the DWARF.
// ============================================================================

namespace BrnGameState
{
namespace GameStateModuleIO
{
    struct KillzoneAction : public CgsModule::Event
    {
        // The killzone's region-id list (DWARF BrnTriggerQueryManager.cpp:976 -- Array<CgsID,32>).
        Array<CgsID, 32> maRegionIds;
    };
}
}

#endif // BRN_KILLZONE_ACTION_H
