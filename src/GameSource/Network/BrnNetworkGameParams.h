#ifndef BRN_NETWORK_GAME_PARAMS_H
#define BRN_NETWORK_GAME_PARAMS_H

#include "types.hpp"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGameParams.h"

// ===========================================================================
// BrnNetwork::GameParams
//   Home: GameSource/Network/BrnNetworkGameParams.{h,cpp}
//
// The Burnout game-side game-parameter object: the game leaf of the DirtySock
// game-params hierarchy,
//
//   CgsNetwork::ServerInterfaceGameParamsBase   (CgsServerInterfaceGameParams.h)
//       <- BrnNetwork::GameParams               (this type)
//
// proven by the X360 PostRoundManager asm: ActionChangeGameParams @ 0x82567978
// constructs a BrnNetwork::GameParams on the stack and round-trips it through
// CgsNetwork::ServerInterfaceGames::GetGameParameters / UpdateGameParameters,
// both of which take a ServerInterfaceGameParamsBase* -- so GameParams IS one.
//
// FLAGGED: only the game-mode surface PostRoundManager drives is recovered here
// (Prepare / GameMode / SetPreviousGameMode / SetGameMode). The full Burnout-
// specific parameter field set is owned by this type's own (not-yet-homed)
// behavioural TU; no field bytes beyond the inherited base are fabricated. The
// methods are declared-only (bodies land with the GameParams TU); the inherited
// base storage keeps a stack GameParams a complete object for `cl /c`.
// ===========================================================================

namespace BrnNetwork
{
    class GameParams : public CgsNetwork::ServerInterfaceGameParamsBase
    {
    public:
        // The X360 GameParams leaf is a CONCRETE object built on the stack
        // (PostRoundManager::ActionChangeGameParams). The game-side leaf overrides the
        // ServerInterfaceStructureInterface pure-virtuals (the replicated-payload pattern
        // + data accessors); declared here so GameParams is instantiable. Bodies are homed
        // in the game-params TU.
        virtual const char* GetPattern() const override;
        virtual s32         GetPatternLength() const override;
        virtual u32         GetDataSize() const override;
        virtual void*       GetData() override;
        virtual const void* GetData() const override;

        // Reset to the prepared/default state (X360: called before the params are
        // re-read from the games component). Declared-only.
        void Prepare();

        // The Burnout game-mode discriminant (E_GAME_MODE_*). The post-round flow reads
        // the current mode, stashes it as the "previous" mode, then forces the mode to
        // the post-game lobby value. Declared-only (bodies in the GameParams TU).
        s32  GameMode() const;
        void SetPreviousGameMode(s32 liGameMode);
        void SetGameMode(s32 liGameMode);
    };
}

#endif // BRN_NETWORK_GAME_PARAMS_H
