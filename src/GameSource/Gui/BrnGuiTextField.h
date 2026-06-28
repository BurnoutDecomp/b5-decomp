#pragma once

// ===================================================================================
// BrnGui::TextField  -- owning header
//   b5-decomp/src/GameSource/Gui/BrnGuiTextField.h
//   class:BrnGui::TextField   (canonical primary_file GameSource/Gui/Flow/Shared/Components/BrnTextField.cpp)
//
// A GUI text field component: a renderable string slot driven through the apt view-state
// machinery, with a colour, a small colour-string scratch buffer, a scroll cursor and per-
// field flags. Embedded by value inside map-icon / panel GUI elements (e.g.
// CrashNavMapIcon::mIconText copied via operator=, DriveThruMapPanel::maTextfields[2]).
//
// CLASS SHAPE (DecFIGS DWARF GameSource/Gui/Flow/Shared/Components/BrnTextField.h:45,
// X360-attested per the ledger):
//   * TextField : public CgsGui::GuiComponent -- so the field IS a component: it carries
//     the base vptr @+0x00, macName[128] @+0x04, muHashedName @+0x84 (GetNameHash()) and
//     mpStateInterface @+0x88 from the base, and overrides the base virtual Construct.
//     This is why the X360 reads a TextField's apt-component hash at field+0x84 and calls
//     Construct on it virtually -- see DriveThruMapPanel.
//
// Layout proven from BURNOUT_X360_ARTIST.XEX (guest 32-bit byte offsets; the gate compiles
// 64-bit so members are accessed BY NAME, not raw offsets):
//   * SetColour @0x82481E48 - stores the colour word @+0x8C (muTextColour), formats it as
//       "%u" (max 15 chars) into the 16-byte colour buffer @+0x94 (macColour), clears that
//       buffer's last byte @+0xA3, and raises the dirty/use-colour flag @+0x124 (mbUseColour).
//   * operator= @0x824470F0 - copies the field byte-for-byte EXCEPT the leading 4 bytes
//       (copy anchor this+0x04): the +0x00 word is the vtable slot, intentionally not
//       assigned (matching C++ copy-assign of a polymorphic object). Everything from +0x04
//       onward is the base name region + the field's own members.
//
// Member names/types follow the DWARF (muTextColour / miScroll / macColour / macText /
// mbUseColour / mbResetScroll / mbAutosize). sizeof == 0x128 (last touched byte +0x126,
// word-aligned). All access is by name; the fixed string regions are named char buffers.
//
// Only the X360-ATTESTED methods are declared (Construct / SetText / SetColour /
// SetLocalisedText / OutputAptData / operator=, per the X360 ledger). The PS3-DWARF-only
// helpers (GetText / RefreshText / ClearText / Scroll* / ResetScroll / SetAutoSize /
// SetDatabaseText) are intentionally omitted -- they are not in the X360 ledger.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"          // CgsCore::SPrintf (SetColour)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h" // CgsGui::GuiComponent (base)
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"   // CgsLanguage::LanguageManager::ParameterFormatType

namespace BrnGui
{
    class TextField : public CgsGui::GuiComponent
    {
    public:
        // BrnTextField.h:48 -- primary text capacity.
        static const u32 KU_MAX_TEXTFIELD_LEN = 128;
        // BrnTextField.h:49 -- colour scratch-buffer size (SetColour passes 15 as the SPrintf
        // capacity and clears index 15, the last byte, so the buffer is 16 bytes).
        static const u32 KU_MAX_COLOUR_LEN = 16;

        // @0x824E4FA8 (BrnTextField.cpp:56) -- run the base component Construct then prime the
        // field's apt binding. Virtual: vtable slot 0 (overrides GuiComponent::Construct); the
        // X360 dispatches it through the vtable (e.g. DriveThruMapPanel::Construct). Body links
        // from the BrnTextField TU.
        virtual void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                               const char* lpacParentName);

        // @0x824E7240 (BrnTextField.cpp:84) -- set the field's text and refresh its apt output.
        // Body links from the BrnTextField TU.
        void SetText(const char* lpacText);

        // @0x82481E48 - set the field's colour and refresh its displayed value. Stores
        // luColour, formats it as an unsigned decimal string into the colour buffer, clears
        // the buffer's last byte and marks the field as using a colour.
        void SetColour(u32 luColour);

        // @0x824E7418 (BrnTextField.cpp:154) -- look up / format luText under leFormat and push
        // it as the field's text. For E_FORMAT_ID_LOOKUP the string is treated as a localisation
        // database id. Returns whether the lookup succeeded. Body links from the BrnTextField TU.
        bool SetLocalisedText(const char* lpacText,
                              CgsLanguage::LanguageManager::ParameterFormatType leFormat);

        // @0x824E52B8 (BrnTextField.cpp:511) -- push the field's current contents to its bound
        // apt clip. Body links from the BrnTextField TU.
        void OutputAptData();

        // @0x824470F0 - copy-assign from lrSource. Reproduces the X360 byte-copy, which copies
        // everything from +0x04 onward and leaves the +0x00 vtable slot untouched.
        TextField& operator=(const TextField& lrSource);

    private:
        u32  muTextColour;          // +0x8C  (set by SetColour)
        s32  miScroll;              // +0x90
        char macColour[KU_MAX_COLOUR_LEN];   // +0x94 .. +0xA3 (formatted colour scratch buffer)
        char macText[KU_MAX_TEXTFIELD_LEN];  // +0xA4 .. +0x123 (primary text region)
        bool mbUseColour;           // +0x124 (raised by SetColour)
        bool mbResetScroll;         // +0x125
        bool mbAutosize;            // +0x126
    };
}
