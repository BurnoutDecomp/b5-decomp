#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_SIMPLE_ICE_TAKEDOWN_PLAYER_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_SIMPLE_ICE_TAKEDOWN_PLAYER_H

#include "types.hpp"
#include <cstddef>                                              // offsetof
#include "GameSource/AttribSys/Generated/classes/iceanim.h"     // Attrib::Gen::iceanim

// ============================================================================
// GameSource/Director/Arbitrator/States/BrnSimpleIceTakedownPlayer.h
//
// BrnDirector::SimpleIceTakedownPlayer -- one of the takedown-camera players (the
// ICE-anim-driven variant). MINIMAL SLICE: only the member SetIceAnim touches (the
// iceanim pointer @+0x18) is modeled here. The player's other state and methods
// (HasFinished / Prepare / Release / Update) live in their own TU; grow this header
// to the full layout when that TU is reconstructed.
//
// FLAG: the leading +0x18 is the player's not-yet-modeled state (base/vtable +
//       fields); represented as an explicit pad so mpIceAnim lands at its offset.
// ============================================================================

namespace BrnDirector
{
    class SimpleIceTakedownPlayer
    {
    public:
        // Bind the iceanim that drives this takedown camera. Asserts the passed
        // object carries iceanim's class-key tag.
        void SetIceAnim(Attrib::Gen::iceanim* lpIceAnim);

    private:
        u8                    mPad0[0x18];   // +0x00  not-yet-modeled player state
        Attrib::Gen::iceanim* mpIceAnim;     // +0x18  the bound ICE anim

        static void _AssertLayout()
        {
            static_assert(offsetof(SimpleIceTakedownPlayer, mpIceAnim) == 0x18,
                          "SimpleIceTakedownPlayer::mpIceAnim offset");
        }
    };
}

#endif // GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_SIMPLE_ICE_TAKEDOWN_PLAYER_H
