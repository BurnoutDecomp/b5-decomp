#pragma once

// ============================================================================
// GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h
//
// BrnGui::AnimationComponent -- the shared "animation clip" GUI component. It is the
// carrier a screen state embeds for every apt movie-clip it only ever drives through
// view-state transitions ("transin" / "transout" / a named frame label): the state
// Constructs it with the clip's component name and then pushes view states at it.
//
// SHAPE (DecFIGS DWARF, GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h:43):
//     struct BrnGui::AnimationComponent : public CgsGui::GuiComponent
//     {
//         AnimationComponent();
//         AnimationComponent(const AnimationComponent&);
//         void Run(const char*);                          // h:64
//     };
// It declares NO data members of its own, and the X360 confirms that independently:
// CgsGui::GuiComponent is sizeof 0x8C (vptr@+0x00, macName[128]@+0x04, muHashedName@+0x84,
// mpStateInterface@+0x88) and every X360 array/embed of AnimationComponent strides by
// exactly 0x8C == 140 --
//   * BrnGui::CrashNavMap::OnEnter constructs mTitleButtonsAnimation @+24404 then
//     mButtonPromptsAnimation @+24544 then the next component @+24684 (140 apart);
//   * BrnGuiNetworkRouteInfo's maOptionsAnimator[9] strides 0x8C (vptr off_82072F68);
//   * BrnOnlineGameRoomPlayerInfo's five animations sit 140 apart (+84320..+84880).
// So this class adds behaviour only, and the local stopgaps that reserved a 0x40-byte
// "animation tail" (BrnCarSelectUnlock.h / BrnCrashNavMap.h) were 0x40 bytes too large.
// This header replaces them; both now include it.
//
// Callers reach the component through the CgsGui::GuiComponent base surface: the virtual
// Construct(name, stateInterface, parentName) at state OnEnter, and AddOutputAptViewState
// ("apt_Transition", <view state>, immediate) to drive it -- e.g. BrnGui::Intro::
// SetupComponents @0x824D1718 pushes "transin"/"transout" straight at the base method on
// its three embedded AnimationComponents. Run(const char*) is DWARF-attested but has no
// X360 symbol (every call site inlines it) and no reconstructed call site, so it is
// DECLARED ONLY -- it grows a body when a TU that needs it lands.
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"   // CgsGui::GuiComponent (base)

namespace BrnGui
{
    struct AnimationComponent : public CgsGui::GuiComponent
    {
        // DWARF h:64. Declared only -- see the header note.
        void Run(const char* lpacViewState);
    };
}
