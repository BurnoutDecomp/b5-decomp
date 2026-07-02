#include "GameSource/Gui/Flow/Shared/Components/BrnComplexBar.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf

// BrnGui::ComplexBar -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (4 ledger functions, DWARF primary file
// GameSource/Gui/Flow/Shared/Components/BrnComplexBar.cpp):
//   ComplexBar::Construct                @0x824E5890
//   ComplexBar::UpdateFlash              @0x824E58F8
//   ComplexBar::SetColour                @0x824E5988
//   ComplexBar::HandleTransitionComplete @0x824E5A10

namespace BrnGui
{

// @ 0x824E5890 -- base component Construct, zero the index state (queued = -1),
// default the colour scratch to "%u" of 0 ("0"; asm `li r6,0` is the vararg --
// the pseudocode's -1 belongs to the miQueuedIndex store) with the last byte
// cleared.
void ComplexBar::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                           const char* lpacParentName)
{
    CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);
    miCurrentIndex          = 0;
    miTargetIndex           = 0;
    miQueuedIndex           = -1;
    mbTransitionInProgress  = false;
    CgsCore::SPrintf(macColour, KU_MAX_COLOUR_LEN - 1, "%u", 0u);
    macColour[KU_MAX_COLOUR_LEN - 1] = 0;
}

// @ 0x824E58F8 -- latch the in-progress flag, then push the current/target segment
// indices as the bar's apt start/end states.
void ComplexBar::UpdateFlash(bool lbTransitionInProgress)
{
    char lacStart[32];   // X360 sp+0x50
    char lacEnd[32];     // X360 sp+0x70 (a 64-byte pseudo artifact; only 32 written)

    mbTransitionInProgress = lbTransitionInProgress;

    CgsCore::SPrintf(lacStart, 32, "%d", miCurrentIndex);
    lacStart[31] = 0;
    AddOutputAptViewState("apt_Start", lacStart, false);

    CgsCore::SPrintf(lacEnd, 32, "%d", miTargetIndex);
    lacEnd[31] = 0;
    AddOutputAptViewState("apt_End", lacEnd, false);
}

// @ 0x824E5988 -- format the colour word into the scratch buffer and push the apt
// use-colour / colour states.
void ComplexBar::SetColour(u32 luColour)
{
    CgsCore::SPrintf(macColour, KU_MAX_COLOUR_LEN - 1, "%u", luColour);
    macColour[KU_MAX_COLOUR_LEN - 1] = 0;
    AddOutputAptViewState("apt_useColour", "1", false);
    AddOutputAptViewState("apt_colour", macColour, false);
}

// @ 0x824E5A10 -- cpp:116/:117. A transition finished at liBarSize: adopt it as the
// current index, promote any queued index to the new target, and kick another
// transition if the bar is not yet at its target.
void ComplexBar::HandleTransitionComplete(s32 liBarSize)
{
    CGS_ASSERT(mbTransitionInProgress, "Transition should be currently running");   // :116 (streamed on the X360; folded static)
    CGS_ASSERT(liBarSize >= 0, "liBarSize >= 0");                                    // :117

    const s32 liQueuedIndex = miQueuedIndex;
    mbTransitionInProgress = false;
    miCurrentIndex = liBarSize;
    if (liQueuedIndex != -1)
    {
        miTargetIndex = liQueuedIndex;
        miQueuedIndex = -1;
    }
    if (liBarSize != miTargetIndex)
        UpdateFlash(true);
}

}
