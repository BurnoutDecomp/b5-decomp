#ifndef BRN_NETWORK_PLAYER_PARAMS_CLASS_H
#define BRN_NETWORK_PLAYER_PARAMS_CLASS_H

#include "types.hpp"
#include "GameSource/Network/Parameters/BrnNetworkPlayerParams.h"  // BrnNetwork::PlayerParamsBase (base)

// ===========================================================================
// BrnNetwork::PlayerParams
//   Home: GameSource/Network/Parameters/BrnNetworkPlayerParamsClass.{h,cpp}
//
// The game-side player-parameter LEAF: the concrete object the BrnNetwork
// managers (Launch / MatchMaking / PostRound / Connection / TeamSelection /
// State / GamerCard) build on the stack to push or fetch a player's lobby
// parameters through the ServerInterfaceGames component.
//
// VTABLE off_82083550 (recovered from the .i64, waveE rodata dump) -- the
// ctor's surviving vptr store at every stack-construction site:
//   +0x00  0x825662D8  PlayerParams::`scalar deleting destructor'
//   +0x04  0x825841B8  PlayerParamsBase::GetPattern
//   +0x08  0x82876410  (COMDAT-folded `return 20`) == StructureInterface::GetPatternLength
//   +0x0C  0x825841C8  PlayerParamsBase::GetDataSize
//   +0x10  0x825841D0  PlayerParamsBase::GetData
//   +0x14  0x825841D0  PlayerParamsBase::GetData (const; folded onto the same body)
//   +0x18  0x8258A818  PlayerParamsBase::Prepare
//   +0x1C  0x8287A340  ServerInterfacePlayerParamsX360::SerialiseFromPlayer
// Every slot except the destructor is INHERITED (slot +0x08 is identical in the
// base-chain vtable off_8207C88C), so the leaf adds no overrides and no data --
// it only makes the chain concrete. The recovered leaf function is the virtual
// destructor (X360 scalar deleting destructor @ 0x825662D8; its surviving vptr
// store is the fully-inlined base chain's off_8207C88C, the
// ServerInterfaceStructureInterface vtable).
// ===========================================================================

namespace BrnNetwork
{
    class PlayerParams : public PlayerParamsBase
    {
    public:
        PlayerParams();
        virtual ~PlayerParams();
    };
}

#endif // BRN_NETWORK_PLAYER_PARAMS_CLASS_H
