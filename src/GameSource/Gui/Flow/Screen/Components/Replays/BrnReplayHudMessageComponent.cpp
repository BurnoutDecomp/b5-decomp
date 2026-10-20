#include "GameSource/Gui/Flow/Screen/Components/Replays/BrnReplayHudMessageComponent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT (Update tripwires)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"               // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                     // CgsGui::GuiAccessPointers
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // CgsGui::StateInterface::GetAccessPointers
#include "GameSource/Replays/BrnReplayGuiModuleStaticLayout.h"          // BrnReplays::GuiModuleStaticLayout

// BrnGui::ReplayHudMessageComponent -- reconstructed from BURNOUT_X360_ARTIST.XEX.
// The replay in-game HUD three-line message banner.
//
//   Construct   @0x8241BE68
//   ShowMessage @0x8241BF18
//   HideMessage @0x8241C078
//   Update      @0x82427AA0
//
// The message text is a 3 x 0x80 region at the head of the GUI-module replay static
// layout (BrnReplays::GuiModuleStaticLayout maMessageSlots[3]); line N == layout + N*0x80.

#include "GameSource/Replays/Serialisers/BrnReplayGuiModuleSerialiser.h"

namespace BrnGui
{

// @ 0x8241BE68 -- base component Construct, then construct the three embedded text
// fields with their fixed ids and this component's name as parent, and clear the
// "showing" latch. The X360 dispatches each field's Construct through its own vtable
// (the field IS a GuiComponent); reads mpStateInterface @+0x88 and macName @+0x04.
void ReplayHudMessageComponent::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                                          const char* lpacParentName)
{
    CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);

    maTextFields[0].Construct("string_0", mpStateInterface, GetName());
    maTextFields[1].Construct("string_1", mpStateInterface, GetName());
    maTextFields[2].Construct("string_2", mpStateInterface, GetName());

    mbMessageShowing = 0;   // stb 0, 0x404
}

// @ 0x8241BF18 -- show the banner. Under the message filter, log the three lines
// (null lines print "<NULLSTRING>"; each guard tests the COMPUTED line pointer).
// Then make the apt clip visible and set each field's text (lines at 0/0x80/0x100).
void ReplayHudMessageComponent::ShowMessage(const char* lpacMessageText)
{
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        const char* lpLine0 = lpacMessageText;
        if (!lpLine0)
            lpLine0 = "<NULLSTRING>";
        const char* lpLine1 = lpacMessageText + 0x80;
        if (!lpLine1)
            lpLine1 = "<NULLSTRING>";
        const char* lpLine2 = lpacMessageText + 0x100;
        if (!lpLine2)
            lpLine2 = "<NULLSTRING>";

        *CgsDev::Log::gpDebugPrint << "SHOWING HUD MESSAGE :\n"
                                   << lpLine0 << "\n"
                                   << lpLine1 << "\n"
                                   << lpLine2 << "\n\n\n";
    }

    AddOutputAptViewState("apt_state", "visible", false);

    maTextFields[0].SetText(lpacMessageText);
    maTextFields[1].SetText(lpacMessageText + 0x80);
    maTextFields[2].SetText(lpacMessageText + 0x100);
}

// @ 0x8241C078 -- hide the banner. Under the message filter, log the hide. Make the
// apt clip invisible, then blank each field's text (macText[0] = 0, the inlined store
// at field+0xA4) and re-output so the cleared text reaches the bound apt clip.
void ReplayHudMessageComponent::HideMessage()
{
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        *CgsDev::Log::gpDebugPrint << "HIDING HUD MESSAGE\n\n\n";

    AddOutputAptViewState("apt_state", "invisible", false);

    maTextFields[0].ClearText();
    maTextFields[0].OutputAptData();
    maTextFields[1].ClearText();
    maTextFields[1].OutputAptData();
    maTextFields[2].ClearText();
    maTextFields[2].OutputAptData();
}

// @ 0x82427AA0 -- per-frame poll. Walk interface -> access-pointers -> serialiser ->
// static layout (each hop tripwire-guarded), then act on the layout's message flags.
void ReplayHudMessageComponent::Update()
{
    CgsGui::GuiAccessPointers* lpAccessPointers = mpStateInterface->GetAccessPointers();
    BrnReplays::GuiModuleSerialiser* lpSerialiser = lpAccessPointers->GetSerialiser();

    BrnReplays::GuiModuleStaticLayout* lpLayout = lpSerialiser->GetStaticLayout();
    CGS_ASSERT(lpLayout != 0, "lpLayout != NULL");                  // BrnReplayHudMessageComponent.cpp:66

    if (lpLayout->mbMessageStart == 1)
    {
        mbMessageShowing = 1;
        ShowMessage(reinterpret_cast<const char*>(lpLayout));
    }
    else
    {
        const u8 luActive = lpLayout->mbMessageActive;
        if (mbMessageShowing != luActive)   // cmplw byte@+0x404 vs byte@+0x181
        {
            mbMessageShowing = luActive;
            if (lpLayout->mbMessageActive == 1)
                ShowMessage(reinterpret_cast<const char*>(lpLayout));
            else
                HideMessage();
        }
    }

    if (mbMessageShowing)
    {
        const char* lpcLayout = reinterpret_cast<const char*>(lpLayout);
        maTextFields[0].SetText(lpcLayout);
        maTextFields[1].SetText(lpcLayout + 0x80);
        maTextFields[2].SetText(lpcLayout + 0x100);
    }
}

}
