// ===================================================================================
// GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.cpp
//
// The one body BrnGui::AnimationComponent owns: Run(const char*), DWARF-attested at
// BrnAnimationComponent.h:64. This is the class's FIRST .cpp -- the header has stood
// alone since the intro wave -- and it exists because Run is referenced from twenty-odd
// reconstructed call sites across the HUD and screen families (BrnEventInfo,
// BrnEventInfo_wS1/_wS2, BrnCompassComponent, ...) and defined by none of them.
//
// NO X360 SYMBOL, AND THAT IS THE POINT. Run is one call and the compiler folded it into
// every call site, so it never became a symbol of its own. The body is therefore
// recovered from the inline expansions rather than from an address of its own, and the
// expansion is unambiguous: every site that a reconstructed TU spells `<animator>.Run(s)`
// is emitted as a single
//     CgsGui::GuiComponent::AddOutputAptViewState(<animator>, "apt_Transition", s, 0)
// call on the animator's own GuiComponent base. Witnessed cleanly in
// BrnGui::Intro::SetupComponents @0x824D1718, which drives its three embedded
// AnimationComponents that way in three consecutive switch arms:
//     AddOutputAptViewState((v1 + 820), "apt_Transition", "transin",  0)   case 7
//     AddOutputAptViewState((v1 + 820), "apt_Transition", "transout", 0)   case 8
//     AddOutputAptViewState((v1 + 855), "apt_Transition", "transin",  0)   case 9
// -- literal "apt_Transition", the caller's view-state string, immediate = false.
//
// The apt-name literal is also the DWARF's own macTransitionVar (a const char[15], i.e.
// 14 characters plus the terminator -- exactly "apt_Transition"), declared as a private
// static of GuiCursor at BrnCursor.h:174 and clearly shared boilerplate across this
// component family. It is spelled inline here, as the X360 call sites do.
//
// AnimationComponent adds no data members of its own (X360 sizeof == 0x8C == the
// GuiComponent base; see the header banner for the three independent stride witnesses),
// so this TU has nothing else to carry.
// ===================================================================================

#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h"

namespace BrnGui
{
    // DWARF BrnAnimationComponent.h:64. Inlined at every X360 call site.
    void AnimationComponent::Run(const char* lpacViewState)
    {
        AddOutputAptViewState("apt_Transition", lpacViewState, false);
    }
}
