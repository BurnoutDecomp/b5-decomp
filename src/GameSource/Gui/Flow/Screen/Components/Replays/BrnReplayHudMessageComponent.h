#ifndef GAMESOURCE_GUI_FLOW_SCREEN_COMPONENTS_REPLAYS_BRNREPLAYHUDMESSAGECOMPONENT_H
#define GAMESOURCE_GUI_FLOW_SCREEN_COMPONENTS_REPLAYS_BRNREPLAYHUDMESSAGECOMPONENT_H

#include "types.hpp"
#include "GameSource/Gui/BrnGuiTextField.h"                              // BrnGui::TextField (embedded x3)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"      // CgsGui::GuiComponent (base)

// ============================================================================
// GameSource/Gui/Flow/Screen/Components/Replays/BrnReplayHudMessageComponent.h
//
// BrnGui::ReplayHudMessageComponent -- the replay in-game HUD message component:
// a three-line HUD text banner driven off the GUI-module replay static layout.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (no DWARF for this TU).
//
// CLASS SHAPE (X360-asm authoritative):
//   * ReplayHudMessageComponent : public CgsGui::GuiComponent -- Construct calls
//     the 4-arg base GuiComponent::Construct on `this` (r6=0), and Update reads the
//     base mpStateInterface @+0x88. So the component IS a GuiComponent.
//   * Three embedded BrnGui::TextField (each sizeof 0x128) at +0x8C / +0x1B4 / +0x2DC
//     (0x128 stride), constructed with fixed ids "string_0/1/2" and this component's
//     name as parent.
//   * u8 mbMessageShowing @+0x404 (Construct zeroes it; Update raises/lowers it).
//
// LAYOUT (guest 32-bit byte offsets; the gate compiles 64-bit so all access is BY NAME):
//   +0x000  CgsGui::GuiComponent base   (vptr, macName[128], muHashedName, mpStateInterface@+0x88)
//   +0x08C  BrnGui::TextField maTextFields[3]   (0x128 stride: +0x8C / +0x1B4 / +0x2DC)
//   +0x404  u8               mbMessageShowing
// ============================================================================

namespace BrnGui
{
    class ReplayHudMessageComponent : public CgsGui::GuiComponent
    {
    public:
        static const s32 KI_NUM_TEXT_FIELDS = 3;

        // @0x8241BE68 -- run the base component Construct then construct the three
        // embedded text fields ("string_0/1/2", parent = this component's name).
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName);

        // @0x8241BF18 -- push the message text (3 lines, 0x80 stride) to the banner:
        // make the apt clip visible and set each field's text.
        void ShowMessage(const char* lpacMessageText);

        // @0x8241C078 -- hide the banner: make the apt clip invisible and blank+re-output
        // each field.
        void HideMessage();

        // @0x82427AA0 -- per-frame poll of the GUI-module replay static layout: raise/lower
        // the banner on the layout's message-start/active flags and, while showing, refresh
        // the three lines from the layout's message region.
        void Update();

    private:
        BrnGui::TextField maTextFields[KI_NUM_TEXT_FIELDS];   // +0x8C / +0x1B4 / +0x2DC (0x128 stride)
        u8                mbMessageShowing;                   // +0x404
    };
}

#endif // GAMESOURCE_GUI_FLOW_SCREEN_COMPONENTS_REPLAYS_BRNREPLAYHUDMESSAGECOMPONENT_H
