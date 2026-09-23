#ifndef BRN_NETWORK_GAME_RESULTS_H
#define BRN_NETWORK_GAME_RESULTS_H

#include <cstddef>   // offsetof (_AssertLayout)

#include "types.hpp"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGameResults.h"

// ===========================================================================
// BrnNetwork::GameResults
//   Home: GameSource/Network/BrnNetworkGameResults.{h,cpp}
//
// The game-side end-of-game result record that BrnNetwork::PostRoundManager
// embeds by value (mGameResults @ +0xC4) and uploads through
// CgsNetwork::ServerInterfaceGames::SendGameResult. It is the game leaf of the
// server-interface game-results hierarchy:
//
//   CgsNetwork::ServerInterfaceStructureInterface
//       <- CgsNetwork::ServerInterfaceGameResultsBase   (Prepare / SerialiseToString)
//           <- BrnNetwork::GameResults                  (this type)
//
// Vtable: destructor, GetPattern, GetPatternLength, GetDataSize, GetData const,
// GetData, Prepare, SerialiseToString. SendGameResult calls SerialiseToString through
// slot +0x1C; the four pattern/data accessors are dead redirects that assert.
//
// The payload mGameData is console +0x04..+0xE0 (ClearGameData zeroes exactly that span):
//   +0x04  GEN block, 3 longs: entry count, a scalar, the event type
//   +0x10  STAT block, 8 longs + the 13-char car name at +0x30
//   +0x40  ten RACE records, 8 bytes each (+0x40..+0x90)
//   +0x90  ten STUNT records, 8 bytes each (+0x90..+0xE0)
// The bodies address payload fields by their console offsets from GetPayloadBase() (the
// object start as the console lays it out), which keeps every field on the right byte
// although the host vptr is wider. The console record layout is not the reference
// GameDataT shape, so the payload stays a byte block.
// ===========================================================================

namespace BrnGameState
{
    namespace GameStateModuleIO
    {
        struct OnlineGameResults;   // SetGameStats input (home: BrnGameActions.h)
    }
}

namespace BrnNetwork
{
    struct GameResults : public CgsNetwork::ServerInterfaceGameResultsBase
    {
    public:
        // X360 @ 0x827DFB60 (`scalar deleting destructor'). Installs the shared
        // ServerInterfaceStructureInterface base vptr (off_8207C88C) as the object
        // dies. Implicitly virtual via the base's virtual destructor.
        virtual ~GameResults();

        // Serialise the GEN block, each per-entry RACE/STUNT record, and the STAT
        // block into the lobby message record.
        virtual void SerialiseToString(char* lpcRecord, s32 liRecLen) const;

        // Fill the end-game result payload from the round-by-round results and the
        // number of online rivals (X360: called from PostRoundManager::ProcessRaceResults
        // after Prepare()).
        void SetGameStats(const BrnGameState::GameStateModuleIO::OnlineGameResults* lpRaceResults,
                          s32 liNumberOfRivals);

        // DWARF BrnNetworkGameResults.h:50 -- event-type index produced by
        // GameModeToEvent (leGameMode - 10). The asm does NOT clamp, so an index beyond
        // COUNT is representable; the enum names only the valid subset.
        enum EEventType
        {
            E_EVENT_TYPE_UNDEFINED        = 0,
            E_EVENT_TYPE_RACE             = 1,
            E_EVENT_TYPE_STUNT            = 2,
            E_EVENT_TYPE_BURNING_HOME_RUN = 3,
            E_EVENT_TYPE_ROAD_RAGE        = 4,
            E_EVENT_TYPE_COUNT            = 5
        };

        // X360 @ 0x825842F0. Reset the game-data block, then report ready. Overrides the
        // inherited ServerInterface Prepare().
        virtual bool Prepare();

    protected:
        // Dead redirects (results serialise via SerialiseToString, never the generic
        // sized-blob path): each asserts, then returns a filler value.
        virtual const char* GetPattern() const;        // X360 0x82584318
        virtual s32         GetPatternLength() const;   // X360 0x825843B0
        virtual u32         GetDataSize() const;        // X360 0x82584440
        virtual void*       GetData();                  // X360 0x825844D0
        virtual const void* GetData() const;

    private:
        // X360 @ 0x82584268 -- zero the whole payload (+0x04..+0xE0). Called by Prepare
        // and SetGameStats. (DWARF cpp:51)
        void ClearGameData();

        // X360 @ 0x825847D8 -- wire game-mode (valid [10,18)) -> event index (mode-10).
        // DWARF param type BrnGameState::GameStateModuleIO::EGameModeType; modelled as s32
        // (the asm treats it as int). (DWARF cpp:266)
        EEventType GameModeToEvent(s32 liGameMode);

        // The payload's console base: its +0x04 is mGameData[0] (see the header note).
        u8* GetPayloadBase() { return mGameData - 0x04; }
        const u8* GetPayloadBase() const { return mGameData - 0x04; }

        // Console layout, pinned in a 32-bit build; inert on the x64 host.
        static void _AssertLayout();

        // The whole result payload, console +0x04..+0xE0 (see the header note).
        alignas(4) u8 mGameData[0xDC];   // +0x04
    };

    inline void GameResults::_AssertLayout()
    {
        static_assert(sizeof(void*) != 4 || offsetof(GameResults, mGameData) == 0x04, "mGameData @ +0x04");
        static_assert(sizeof(void*) != 4 || sizeof(GameResults) == 0xE0, "GameResults is 0xE0 bytes");
    }
}

#endif // BRN_NETWORK_GAME_RESULTS_H
