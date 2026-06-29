#ifndef BRN_NETWORK_PLAYER_PARAMS_CLASS_H
#define BRN_NETWORK_PLAYER_PARAMS_CLASS_H

#include "types.hpp"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerParams.h"  // CgsNetwork::ServerInterfacePlayerParamsBase (base; supplies Prepare()/GetID())
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"  // BrnNetwork::NetworkPlayerID (SetMarkedPlayerID arg)

// ===========================================================================
// BrnNetwork::PlayerParams
//   Home: GameSource/Network/Parameters/BrnNetworkPlayerParamsClass.{h,cpp}
//
// Game-side player-parameter object: the leaf the launch / state managers build on
// the stack to push a player's lobby parameters (BrnNetwork::PlayerParamsBase is the
// game-side intermediate that adds the replicated payload; its accessor/prepare logic
// lives in BrnNetworkPlayerParams.cpp). The recovered leaf function is the scalar
// deleting destructor (X360 @ 0x825662D8): it stores the class vptr (off_8207C88C) at
// this+0 and conditionally calls operator delete -- the codegen of a virtual
// destructor.
//
// The leaf derives from CgsNetwork::ServerInterfacePlayerParamsBase, the type the
// ServerInterfaceGames component takes for GetPlayerParametersByIndex /
// UpdatePlayerParameters; Prepare() and GetID() are inherited from that base. The two
// declared-only members below are the ones the launch state machine drives directly
// (the X360 inlines SetPlaying as an mxFlags |= KX_IS_PLAYING / &= ~KX_IS_PLAYING bit
// twiddle on the replicated payload); their bodies live in the player-params TUs.
// ===========================================================================

namespace BrnNetwork
{
    class PlayerParams : public CgsNetwork::ServerInterfacePlayerParamsBase
    {
    public:
        PlayerParams();
        virtual ~PlayerParams();

        // The X360 PlayerParams leaf is a CONCRETE object built on the stack: the game-side
        // PlayerParamsBase chain overrides the ServerInterfaceStructureInterface pure-virtuals
        // (the replicated-payload pattern + data accessors). Declared here so PlayerParams is
        // instantiable (e.g. BrnNetwork::ConnectionManager::KickUnNATablePlayer builds one on
        // the stack); bodies are homed in the player-params TUs. ADDITIVE GROW
        // (BrnNetworkConnectionManager TU).
        virtual const char* GetPattern() const override;
        virtual s32         GetPatternLength() const override;
        virtual u32         GetDataSize() const override;
        virtual void*       GetData() override;
        virtual const void* GetData() const override;

        // Set/clear the replicated "is playing" flag (BrnNetwork::PlayerParamsBase
        // SetPlaying, DWARF BrnNetworkPlayerParams.h:243). Declared-only; body in the
        // player-params TU.
        void SetPlaying(bool lbPlaying);

        // Set/clear the replicated "is ready" flag and the marked-player id
        // (BrnNetwork::PlayerParamsBase SetReady / SetMarkedPlayerID). The post-round
        // ActionChangePlayerParams clears both when re-pushing the local player's params
        // at the end of a game. Declared-only; bodies in the player-params TU.
        // ADDITIVE GROW (BrnNetworkPostRoundManager TU).
        void SetReady(bool lbReady);
        void SetMarkedPlayerID(NetworkPlayerID lMarkedPlayerID);
    };
}

#endif // BRN_NETWORK_PLAYER_PARAMS_CLASS_H
