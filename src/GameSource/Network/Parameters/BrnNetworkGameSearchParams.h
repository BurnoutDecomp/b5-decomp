#ifndef BRN_NETWORK_GAME_SEARCH_PARAMS_H
#define BRN_NETWORK_GAME_SEARCH_PARAMS_H

#include "types.hpp"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGameSearchParams.h"

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
//   +0x6C  maGameSearchPayload[0x2A0]   game-side block copied by memcpy(.., 672)
//   +0x30C maGameSearchTail[0x83]       trailing block copied byte-by-byte (131)
//
// operator= (asm @ 0x8255EA80):
//   1. invoke the base ServerInterfaceGameSearchParams copy-assignment on the
//      base subobject (bl CgsNetwork__ServerInterfaceGameSearchParamsX360),
//   2. memcpy(this+0x6C, rhs+0x6C, 0x2A0 == 672),
//   3. a 131-byte (0x83) byte-by-byte copy of this+0x30C from rhs+0x30C
//      (the asm copies bytes one at a time via lbzx/stb over the +0x30C region).
// The two regions are contiguous (0x6C + 0x2A0 == 0x30C), together forming the
// game-side replicated payload. Their internal field structure is not described
// by any DWARF for this TU, so they are modelled as named byte spans and the
// store sequence is reproduced verbatim from the asm.
// ===========================================================================

namespace BrnNetwork
{
    class GameSearchParamsBase : public CgsNetwork::ServerInterfaceGameSearchParams
    {
    public:
        GameSearchParamsBase& operator=(const GameSearchParamsBase& lrhs);

    protected:
        // +0x6C  bulk game-side block (asm: memcpy size 0x2A0 == 672)
        u8 maGameSearchPayload[0x2A0];
        // +0x30C trailing block (asm: 0x83 == 131 byte-by-byte copy)
        u8 maGameSearchTail[0x83];
    };

    class BrnNetworkManager;   // pointer-only (AreRivalsInSameGame param)

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
