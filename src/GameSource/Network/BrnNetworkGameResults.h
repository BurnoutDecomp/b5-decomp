#ifndef BRN_NETWORK_GAME_RESULTS_H
#define BRN_NETWORK_GAME_RESULTS_H

#include <cstddef>   // offsetof (_AssertLayout)

#include "types.hpp"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceEndGameData.h"

// ===========================================================================
// BrnNetwork::GameResults
//   Home: GameSource/Network/BrnNetworkGameResults.{h,cpp}
//
// The game-side end-of-game result record that BrnNetwork::PostRoundManager
// embeds by value (mGameResults @ +196) and submits to the server. It is the
// Burnout leaf of the DirtySock end-game-data hierarchy:
//
//   CgsNetwork::ServerInterfaceEndGameDataBase
//       <- CgsNetwork::ServerInterfaceEndGameDataX360   (platform leaf)
//           <- BrnNetwork::GameResults                  (game leaf, this type)
//
// proven by the X360 PostRoundManager asm:
//   * ActionSendResults @ 0x82545040 passes &mGameResults (this+196) where
//     CgsNetwork::ServerInterfaceGames::SendGameResult expects a
//     ServerInterfaceEndGameDataBase* -- so GameResults IS one.
//   * Prepare / ProcessComplete / ProcessRaceResults make a virtual call through
//     mGameResults' vtable (the inherited Prepare()), confirming the polymorphic
//     base edge.
//
// FLAGGED: only the surface PostRoundManager drives is recovered here --
//   * Prepare()      (inherited virtual, re-Prepares the payload), and
//   * SetGameStats() (X360 @ ~0x8255Exxx; fills the result payload from the
//     per-round OnlineGameResults plus the rival count).
// The full game-specific result-field layout is owned by this type's own
// (not-yet-homed) behavioural TU; no field bytes beyond the inherited
// ServerInterfaceEndGameDataX360 payload are fabricated here.
//
// The result payload is console +0x04..+0xE0 (ClearGameData zeroes exactly that span):
// the inherited maResultWords[16] (+0x04..+0x44) followed by maCustomResults (+0x44..+0xE0),
// so the object is 0xE0 bytes on the console. The bodies address payload fields by their
// console offsets from GetPayloadBase() (the object start as the console lays it out: the
// payload's first byte, maResultWords, sits at +0x04 from it), which keeps every field on the
// right byte although the host vptr is wider.
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
    class GameResults : public CgsNetwork::ServerInterfaceEndGameDataX360
    {
    public:
        // X360 @ 0x827DFB60 (`scalar deleting destructor'). Installs the shared
        // ServerInterfaceStructureInterface base vptr (off_8207C88C) as the object
        // dies. Implicitly virtual via the base's virtual destructor.
        virtual ~GameResults();

        // X360 @ 0x82584600. Serialise the GEN header, each per-entry RACE/STUNT
        // custom-results record, and the STAT block into the lobby message record.
        void SerialiseToString(char* lpcRecord, s32 liRecLen) const;

        // Fill the end-game result payload from the round-by-round results and the
        // number of online rivals (X360: called from PostRoundManager::ProcessRaceResults
        // after the inherited Prepare()). Declared-only; bodied in the GameResults TU.
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
        // DWARF vtable order; these are dead redirect stubs (results serialise via
        // SerialiseToString, never the generic sized-blob path). Declared plain `virtual`
        // (the committed base ServerInterfaceEndGameDataX360 does not declare them).
        virtual const char* GetPattern() const;        // X360 0x82584318
        virtual s32         GetPatternLength() const;   // X360 0x825843B0
        virtual u32         GetDataSize() const;        // X360 0x82584440
        virtual void*       GetData();                  // X360 0x825844D0

    private:
        // X360 @ 0x82584268 -- zero the whole payload (+0x04..+0xE0). Called by Prepare
        // and SetGameStats. (DWARF cpp:51)
        void ClearGameData();

        // X360 @ 0x825847D8 -- wire game-mode (valid [10,18)) -> event index (mode-10).
        // DWARF param type BrnGameState::GameStateModuleIO::EGameModeType; modelled as s32
        // (the asm treats it as int). (DWARF cpp:266)
        EEventType GameModeToEvent(s32 liGameMode);

        // The payload's console base: its +0x04 is maResultWords[0] (see the header note).
        u8* GetPayloadBase() { return reinterpret_cast<u8*>(maResultWords) - 0x04; }
        const u8* GetPayloadBase() const { return reinterpret_cast<const u8*>(maResultWords) - 0x04; }

        // Console layout, pinned in a 32-bit build; inert on the x64 host.
        static void _AssertLayout();

    protected:
        // +0x44  leaf custom-results tail, right after the inherited {vptr + maResultWords[16]}
        // (base size 0x44), up to the payload end at +0xE0: the ten RACE records
        // (+0x40..+0x90, starting inside the base words) and the ten STUNT records
        // (+0x90..+0xE0), 8 bytes each. SerialiseToString reaches records by console byte
        // offset, so no per-field names are asserted.
        u8 maCustomResults[0x9C];   // +0x44
    };

    inline void GameResults::_AssertLayout()
    {
        static_assert(sizeof(void*) != 4 || offsetof(GameResults, maCustomResults) == 0x44, "maCustomResults @ +0x44");
        static_assert(sizeof(void*) != 4 || sizeof(GameResults) == 0xE0, "GameResults is 0xE0 bytes");
    }
}

#endif // BRN_NETWORK_GAME_RESULTS_H
