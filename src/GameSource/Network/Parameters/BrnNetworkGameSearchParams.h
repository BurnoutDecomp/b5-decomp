#ifndef BRN_NETWORK_GAME_SEARCH_PARAMS_H
#define BRN_NETWORK_GAME_SEARCH_PARAMS_H

#include "types.hpp"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGameSearchParams.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerParams.h" // CgsNetwork::EFirewallSettings

// ===========================================================================
// BrnNetwork::GameSearchParamsBase  (and the BrnNetwork::GameSearchParams leaf)
//   Home: GameSource/Network/Parameters/BrnNetworkGameSearchParams.{h,cpp}
//
// Game-side "search for games" parameter object. It is built on top of the
// platform server-interface search-params block: the X360 leaf
// CgsNetwork::ServerInterfaceGameSearchParams (== ...GameSearchParamsX360) is
// the base subobject occupying this+0x00..this+0x6B (vptr + scalar fields +
// the 80-byte X360 payload + the trailing word at +0x68). Reused BY NAME from
// its committed home rather than re-declared here.
//
// LAYOUT (X360 asm @ 0x8255EA80  GameSearchParamsBase::operator=):
//   +0x00  CgsNetwork::ServerInterfaceGameSearchParams  base subobject (0x6C bytes)
//   +0x6C  mSearchData (SearchData)     the replicated search payload (672 bytes)
//   +0x30C macPattern[131]              its serialisation pattern
//
// operator= (asm @ 0x8255EA80):
//   1. invoke the base ServerInterfaceGameSearchParams copy-assignment on the
//      base subobject (bl CgsNetwork__ServerInterfaceGameSearchParamsX360),
//   2. memcpy(this+0x6C, rhs+0x6C, 0x2A0 == 672),
//   3. a 131-byte (0x83) byte-by-byte copy of this+0x30C from rhs+0x30C
//      (the asm copies bytes one at a time via lbzx/stb over the +0x30C region).
// The two regions are contiguous (0x6C + 0x2A0 == 0x30C). GetData / GetDataSize hand out
// mSearchData, GetPattern / GetPatternLength the pattern buffer.
// ===========================================================================

namespace BrnNetwork
{
    class BrnNetworkManager;   // pointer-only (Prepare / FillInRivals / AreRivalsInSameGame)

    // The lobby state a search asks for.
    enum EBrnGameState
    {
        E_GAMESTATE_ANY                 = 0,
        E_GAMESTATE_IN_PROGRESS         = 1,
        E_GAMESTATE_WAITING_FOR_PLAYERS = 2,
        E_GAMESTATE_COUNT               = 3,
    };

    class GameSearchParamsBase : public CgsNetwork::ServerInterfaceGameSearchParams
    {
    public:
        static const s32 KI_PATTERN_SIZE = 131;

        // The replicated search payload (+0x6C, 0x2A0 bytes). Prepare builds the matching
        // pattern: forty "16s" name fields then "llllllbbwl".
        struct SearchData
        {
            static const s32 KI_MAX_PLAYERS_UPLOAD     = 40;
            static const s32 KI_MAX_PLAYER_NAME_LENGTH = 16;

            char macPlayerNames[KI_MAX_PLAYERS_UPLOAD][KI_MAX_PLAYER_NAME_LENGTH]; // +0x000 (rivals)
            s32  meGameSearchGameMode;    // +0x280 (BrnNetwork::ESearchGameModes)
            s32  meGameState;             // +0x284 (EBrnGameState)
            u32  muSkillLevel;            // +0x288
            s32  meOpponentType;          // +0x28C (BrnNetwork::ESearchOpponentTypes)
            u32  muRequiredSlots;         // +0x290
            s32  meFirewallSettings;      // +0x294 (CgsNetwork::EFirewallSettings)
            bool mbIsRanked;              // +0x298
            bool mbIsFreeburn;            // +0x299
            u16  mu16Field29A;            // +0x29A (Prepare stores 0; FLAG: name unrecovered)
            u32  muField29C;              // +0x29C (Prepare's last argument; FLAG: name unrecovered)
        };

        GameSearchParamsBase& operator=(const GameSearchParamsBase& lrhs);

        // Seed the platform block, build the pattern, store the search fields, fill in the
        // rival names and publish the matchmaking contexts. The game-mode / opponent-type
        // enums have no home yet (BrnNetworkSharedIO.h), so they travel as s32.
        bool Prepare(s32 leGameSearchGameMode, EBrnGameState leGameState, u32 luSkillLevel,
                     s32 leOpponentType, bool lbIsRanked, bool lbIsFreeburn,
                     u32 luRequiredSlots, CgsNetwork::EFirewallSettings leFirewallSettings,
                     BrnNetworkManager* lpNetworkManager, u32 luField29C);

        // ServerInterfaceStructureInterface: the replicated payload and its pattern.
        virtual const char* GetPattern() const override;
        virtual s32         GetPatternLength() const override;
        virtual u32         GetDataSize() const override;
        virtual void*       GetData() override;
        virtual const void* GetData() const override;

    protected:
        // The custom-flag filter words of the platform search block. Both vtable slots of
        // the leaf hold one shared body that returns 0 (no custom-flag filtering).
        virtual u32 GetCustomFlagsMask() const override;
        virtual u32 GetCustomFlagsValue() const override;

    private:
        // Copy the live-revenge rivals' names into macPlayerNames.
        void FillInRivals(BrnNetworkManager* lpNetworkManager);

    protected:
        SearchData mSearchData;                  // +0x6C  (0x2A0)
        char       macPattern[KI_PATTERN_SIZE];  // +0x30C (0x83)
    };

    // X360 leaf. Its `scalar deleting destructor' (X360 @ 0x82569280) stores the
    // class vptr then conditionally frees -- the codegen of a virtual destructor.
    class GameSearchParams : public GameSearchParamsBase
    {
    public:
        virtual ~GameSearchParams();

        // X360 GameSearchParamsX360::AreRivalsInSameGame @ 0x82590FC0.
        // Fills lpbRivalInSameGame[i] for each stored revenge rival with whether that
        // rival is currently present in a Burnout title (via Xbox LIVE presence). Returns
        // the revenge-relationship count. lpNetworkManager reaches the LiveRevengeManager.
        s32 AreRivalsInSameGame(bool* lpbRivalInSameGame, BrnNetworkManager* lpNetworkManager);
    };
}

#endif // BRN_NETWORK_GAME_SEARCH_PARAMS_H
