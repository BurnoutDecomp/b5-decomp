// FX-FLOW (crash parity 2026-09-24, NEW-ACTION219): the game-action ids of the three records whose
// PC enumerators carried the raw PS3 DWARF value instead of the X360 +8 band, read off the REAL
// BrnGameActions.h (the record types' GameAction<T> tags).
//
// Producer-pinned on the ARTIST image:
//   SoundTriggerAction         218  TriggerQueryManager::PreWorldUpdate @0x8239F894 `li r5, 0xDA ; li r6, 0x20`
//                                   (consumed by SoundLogicModule::ProcessGameActionQueue: `addi -0x58`
//                                   switch, case 130 == 218)
//   OnlinePlayerAddedAction    219  ProcessGameEvents case 127 @0x823A2390 `li r5, 0xDB ; li r6, 0x28`
//                                   (consumed by RaceCarEntityModule::HandleGameActions: `addi -0x6B`
//                                   switch, case 112 == 219, `ld 0(rec)` model / `ld 8(rec)` wheel)
//   OnlinePlayerRemovedAction  220  ProcessGameEvents case 129 @0x823A261C `li r5, 0xDC ; li r6, 8`
// and the live ids they collided with:
//   210 ALL_RIVALS_SHUTDOWN (ProgressionManager::PreWorldUpdate @0x823A5254 `li r5, 0xD2`)
//   211 PAYBACK_LOST        (PaybackManager::HandleAwardingPayback @0x82397AD8 `li r5, 0xD3`)
//   212                     (PaybackManager::HandleHavingPayback  @0x82397BE4 `li r5, 0xD4`)
#include "GameSource/GameState/BrnGameActions.h"
#include <cstdio>

namespace GsmIO = BrnGameState::GameStateModuleIO;

// The id a record type carries: the tag of its GameAction<T> base.
template <GsmIO::EGameActionType T>
static int IdOf(const GsmIO::GameAction<T>*)
{
    return static_cast<int>(T);
}

template <class R>
static int IdOfRecord()
{
    return IdOf(static_cast<const R*>(nullptr));
}

static unsigned gChecks = 0, gFailures = 0;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("  FAIL %s\n", lpcName);
    }
    else
    {
        std::printf("  ok   %s\n", lpcName);
    }
}

int main()
{
    const int liSound   = IdOfRecord<GsmIO::SoundTriggerAction>();
    const int liAdded   = IdOfRecord<GsmIO::OnlinePlayerAddedAction>();
    const int liRemoved = IdOfRecord<GsmIO::OnlinePlayerRemovedAction>();
    std::printf("  ids: SoundTrigger %d, OnlinePlayerAdded %d, OnlinePlayerRemoved %d\n",
                liSound, liAdded, liRemoved);

    Check(liSound == 218, "SoundTriggerAction is action 218 (`li r5, 0xDA` @0x8239F894)");
    Check(liAdded == 219, "OnlinePlayerAddedAction is action 219 (`li r5, 0xDB` @0x823A2390)");
    Check(liRemoved == 220, "OnlinePlayerRemovedAction is action 220 (`li r5, 0xDC` @0x823A261C)");
    Check(liSound != static_cast<int>(GsmIO::E_ACTION_ALL_RIVALS_SHUTDOWN),
          "the sound trigger no longer shares 210 with ALL_RIVALS_SHUTDOWN (`li r5, 0xD2` @0x823A5254)");
    Check(liAdded != 211 && liRemoved != 211 && liAdded != 212 && liRemoved != 212,
          "neither online-player record shares the payback ids 211 / 212 (@0x82397AD8 / @0x82397BE4)");

    std::printf("FxFlowAction219: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
