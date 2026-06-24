// BrnFreeburnChallengeStartComponent.cpp
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX.
//
//   Construct           @ 0x824157D8 -- base Construct, then mbActive/mbDownL2/mbDownR2 = false.
//   Show                @ 0x82415818 -- if hidden, publish apt_state="visible"; set mbActive.
//   Hide                @ 0x82415870 -- if shown, publish apt_state="invisible"; clear mbActive.
//   HandleButtonPress   @ 0x82439AF8 -- L2(56)->mbDownL2, R2(57)->mbDownR2; when active and
//                                       both held, post the start-challenge GUI event.
//   HandleButtonRelease @ 0x824158C8 -- L2(56) clears mbDownL2, R2(57) clears mbDownR2.
//
// The event posted by HandleButtonPress is built on the stack as { header0 = 1,
// type = 575, header2 = 12 } (a GuiEvent<575>-shaped record; the X360 leaves the 4th word
// of the 16-byte record uninitialised) and pushed onto the owning state's large output queue
// with channel id 40 (GuiEventOut), record size 16. The X360 reaches the queue as
// `mpStateInterface + 0xC` (the StateInterface's mOutEventQueue); we reach it by name through
// GetOutputEventQueue(). The "apt_state" view-state output goes through the GuiComponent
// apt-view-state plumbing (AddOutputAptViewState with immediate == 0 = false).

#include "GameSource/Gui/Flow/Hud/Components/BrnFreeburnChallengeStartComponent.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // CgsGui::StateInterface, GetOutputEventQueue
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                      // CgsGui::GuiEvent, CgsModule::Event

namespace BrnGui
{

// The controller button ids the prompt watches (X360 cmpwi 56 / 57).
static const s32 KI_BUTTON_L2 = 56;
static const s32 KI_BUTTON_R2 = 57;

// The "start freeburn challenge" GUI event HandleButtonPress posts. The X360 fills a
// GuiEvent<575> header { muHeader0 = 1, muEventType = 575, muHeader2 = 12 } and pushes a
// 16-byte record (channel id 40); the trailing payload word is left uninitialised.
struct GuiEventStartFreeburnChallenge : public CgsGui::GuiEvent<575>
{
    u32 muReserved;   // +0x0C (X360 leaves this gap word uninitialised; record size is 16)

    GuiEventStartFreeburnChallenge() : CgsGui::GuiEvent<575>(1, 12) {}
};

// @ 0x824157D8
void FreeburnChallengeStartComponent::Construct(const char* lpacName,
                                                CgsGui::StateInterface* lpStateInterface,
                                                const char* lpacParentName)
{
    GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);
    mbActive = false;   // stb 0, 0x8C
    mbDownL2 = false;   // stb 0, 0x8D
    mbDownR2 = false;   // stb 0, 0x8E
}

// @ 0x82415818
void FreeburnChallengeStartComponent::Show()
{
    if (!mbActive)
    {
        AddOutputAptViewState("apt_state", "visible", false);
    }
    mbActive = true;   // stb 1, 0x8C
}

// @ 0x82415870
void FreeburnChallengeStartComponent::Hide()
{
    if (mbActive)
    {
        AddOutputAptViewState("apt_state", "invisible", false);
    }
    mbActive = false;   // stb 0, 0x8C
}

// @ 0x82439AF8
void FreeburnChallengeStartComponent::HandleButtonPress(s32 liButton)
{
    if (liButton == KI_BUTTON_L2)
    {
        mbDownL2 = true;   // stb 1, 0x8D
    }
    else if (liButton == KI_BUTTON_R2)
    {
        mbDownR2 = true;   // stb 1, 0x8E
    }
    else
    {
        return;
    }

    // Both triggers held? (X360: r11 = mbDownL2 && mbDownR2)
    const bool lbBothHeld = mbDownL2 && mbDownR2;

    if (mbActive && lbBothHeld)
    {
        GuiEventStartFreeburnChallenge lEvent;
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lEvent), 40, 16);
    }
}

// @ 0x824158C8
void FreeburnChallengeStartComponent::HandleButtonRelease(s32 liButton)
{
    if (liButton == KI_BUTTON_L2)
    {
        mbDownL2 = false;   // stb 0, 0x8D
    }
    else if (liButton == KI_BUTTON_R2)
    {
        mbDownR2 = false;   // stb 0, 0x8E
    }
}

} // namespace BrnGui
