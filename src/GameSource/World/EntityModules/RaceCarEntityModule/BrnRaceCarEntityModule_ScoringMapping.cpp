// ============================================================================
// BrnWorld::RaceCarEntityModule -- player-scoring mapping + game-mode-flag query.
//
// This TU bodies the self-contained, fully-asm-grounded slice of the
// class:BrnWorld::RaceCarEntityModule work item:
//   ClearAllActiveRaceCarToPlayerScoringMappings        X360 0x822A3760
//   ClearActiveRaceCarToPlayerScoringMapping            X360 0x822A37C8
//   SetActiveRaceCarForPlayerScoringIndex               X360 0x822A3888
//   CopyActiveRaceCarToPlayerScoringMappingToOutput     X360 0x822A3918
//   GetGameModeFlag                                     X360 0x822A3A20
//
// All five touch only the module's game-mode/scoring tail
// (mxGameModeFlags @+0x18358, maActiveRaceCarForPlayerScoringIndex @+0x187BC) and the
// active-race-car output interface, so they reconstruct cleanly BY NAME without
// modelling the full ~100KB object. The remaining three functions on the work item
// (the whole-object constructor @0x827E5160, GetPlayerRaceCar @0x822BA068, and
// UpdateRaceCarCollisionTagging @0x822D2280) need the full class layout / a not-yet-
// homed vtable'd traffic-density interface and are deliberately NOT bodied here.
//
// The sentinel that marks "this player has no active race car" is
// E_ACTIVE_RACE_CAR_INDEX_COUNT (8) -- the literal the X360 stores (`li r10,8` /
// `li r27,8`), NOT an invented value.
// ============================================================================
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"   // EPlayerScoringIndex
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT

namespace BrnWorld
{

// X360 0x822A3760. Reset every player-scoring slot to the "no mapping" sentinel
// (E_ACTIVE_RACE_CAR_INDEX_COUNT == 8). The X360 walks the 8-wide array with the
// range-guarded EActiveRaceCarIndex post-increment, which is what fires the
// "leEnumIndex <= E_PLAYER_SCORING_INDEX_COUNT" assert when the index runs past the end.
void RaceCarEntityModule::ClearAllActiveRaceCarToPlayerScoringMappings()
{
    for (s32 liPlayerScoringIndex = 0;
         liPlayerScoringIndex < BrnGameState::GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT;
         ++liPlayerScoringIndex)
    {
        maActiveRaceCarForPlayerScoringIndex[liPlayerScoringIndex] =
            E_ACTIVE_RACE_CAR_INDEX_COUNT;
    }
}

// X360 0x822A37C8. Find the (single) player slot currently mapped to
// leActiveRaceCarIndex and reset it to the sentinel. The X360 scans the array for the
// first cell equal to the index; if none matches it falls out of the loop and stores
// nothing. (asm: `cmpw r11,r27` / `beq` -> store 8 at the matching slot.)
void RaceCarEntityModule::ClearActiveRaceCarToPlayerScoringMapping(
    EActiveRaceCarIndex leActiveRaceCarIndex)
{
    CGS_ASSERT(leActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID &&
                   leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "(leActiveRaceCarIndex>E_ACTIVE_RACE_CAR_INDEX_INVALID) && "
               "(leActiveRaceCarIndex<E_ACTIVE_RACE_CAR_INDEX_COUNT)");

    for (s32 liPlayerScoringIndex = 0;
         liPlayerScoringIndex < BrnGameState::GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT;
         ++liPlayerScoringIndex)
    {
        if (maActiveRaceCarForPlayerScoringIndex[liPlayerScoringIndex] == leActiveRaceCarIndex)
        {
            maActiveRaceCarForPlayerScoringIndex[liPlayerScoringIndex] =
                E_ACTIVE_RACE_CAR_INDEX_COUNT;
            return;
        }
    }
}

// X360 0x822A3888. Map one player-scoring slot to an active-race-car slot.
void RaceCarEntityModule::SetActiveRaceCarForPlayerScoringIndex(
    BrnGameState::GameStateModuleIO::EPlayerScoringIndex lePlayerScoringIndex,
    EActiveRaceCarIndex leActiveRaceCarIndex)
{
    CGS_ASSERT(lePlayerScoringIndex >= BrnGameState::GameStateModuleIO::E_PLAYER_SCORING_INDEX_0 &&
                   lePlayerScoringIndex < BrnGameState::GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT,
               "(lePlayerScoringIndex>=BrnGameState::GameStateModuleIO::E_PLAYER_SCORING_INDEX_0) && "
               "(lePlayerScoringIndex<BrnGameState::GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT)");
    CGS_ASSERT(leActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID &&
                   leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "(leActiveRaceCarIndex>E_ACTIVE_RACE_CAR_INDEX_INVALID) && "
               "(leActiveRaceCarIndex<E_ACTIVE_RACE_CAR_INDEX_COUNT)");

    maActiveRaceCarForPlayerScoringIndex[lePlayerScoringIndex] = leActiveRaceCarIndex;
}

// X360 0x822A3918. Copy the whole player-scoring mapping into the active-race-car
// output interface. The X360 stores each cell straight into the interface's
// maeActiveRaceCarIndex[] (the canonical setter SetActiveRaceCarIndex), bracketed by the
// scoring-index range asserts on read and the active-race-car-index range assert on the
// value being copied out.
void RaceCarEntityModule::CopyActiveRaceCarToPlayerScoringMappingToOutput(
    BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpOutputInterface)
{
    for (s32 liIndex = 0;
         liIndex < BrnGameState::GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT;
         ++liIndex)
    {
        CGS_ASSERT(liIndex >= BrnGameState::GameStateModuleIO::E_PLAYER_SCORING_INDEX_0 &&
                       liIndex < BrnGameState::GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT,
                   "(lePlayerScoringIndex>=BrnGameState::GameStateModuleIO::E_PLAYER_SCORING_INDEX_0) && "
                   "(lePlayerScoringIndex<BrnGameState::GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT)");

        const EActiveRaceCarIndex leActiveRaceCarIndex =
            maActiveRaceCarForPlayerScoringIndex[liIndex];

        // The copied value is allowed to be the sentinel (== COUNT), hence the inclusive
        // upper bound here (X360: `cmpwi r30,8 / ble`).
        CGS_ASSERT(leActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID &&
                       leActiveRaceCarIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "(leActiveRaceCarIndex>E_ACTIVE_RACE_CAR_INDEX_INVALID) && "
                   "(leActiveRaceCarIndex<=E_ACTIVE_RACE_CAR_INDEX_COUNT)");

        lpOutputInterface->SetActiveRaceCarIndex(
            static_cast<BrnGameState::GameStateModuleIO::EPlayerScoringIndex>(liIndex),
            leActiveRaceCarIndex);
    }
}

// X360 0x822A3A20. (mxGameModeFlags & lxFlagMask) != 0. The X360 loads the flags with a
// 64-bit `ldx` and ANDs with the (64-bit) mask argument; DWARF BrnRaceCarEntityModule.h:887
// declares it const taking uint64_t.
bool RaceCarEntityModule::GetGameModeFlag(u64 lxFlagMask) const
{
    return (mxGameModeFlags & lxFlagMask) != 0;
}

}   // namespace BrnWorld
