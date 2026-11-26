#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h" // CgsModule::VariableEventQueue<N,16> (base)

// The game-action queue class template, kept in a header of its own so that
// BrnGameStateSharedIO.h (the GameActionQueue typedef's home in this tree) can see it without
// pulling in BrnGameActions.h, which itself includes BrnGameStateSharedIO.h.
//
// DWARF BrnGameActions.h:492 declares the two instances the build uses:
//     struct BaseGameActionQueue<13312> : public VariableEventQueue<13312,16> {}
//     struct BaseGameActionQueue<256>   : public VariableEventQueue<256,16>   {}
// and h:501 its member template AddGameAction<T>(const T& lAction) (263 instantiations). The
// body is LegacySrc BrnGameActions.h:286..305: post the action with its own type tag and size.
//
// ARTIST inlines AddGameAction at every post: the caller fetches the queue, then
// li r6,sizeof / li r5,mseType / addi r4,&action and calls
// VariableEventQueue<13312,16>::AddEvent (0x8233FAE8) itself, forming the action's address
// after the queue getter (e.g. ModeManager::TransmitCheckPointDistancesToFinishLine
// 0x82342070..0x82342080). Spelling the post as AddEvent(&lAction, id, n) keeps &lAction live
// across the getter instead.

namespace BrnGameState
{
namespace GameStateModuleIO
{
    template <s32 liBufferSize>
    class BaseGameActionQueue : public CgsModule::VariableEventQueue<liBufferSize, 16>
    {
    public:
        template <class DerivedGameAction>
        bool
        AddGameAction(
            const DerivedGameAction& lAction)
        {
            return this->AddEvent(&lAction,
                                  DerivedGameAction::mseType,
                                  sizeof(DerivedGameAction));
        }
    };
}
}
