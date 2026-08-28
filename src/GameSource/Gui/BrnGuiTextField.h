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
// SetLocalisedText / SetDatabaseText / OutputAptData / operator=, per the X360 ledger --
// SetDatabaseText @0x824E5020 IS X360-emitted; an earlier revision of this note wrongly
// listed it as PS3-only). The remaining PS3-DWARF-only helpers (GetText(non-inline) /
// RefreshText / Scroll* / ResetScroll / SetAutoSize) stay omitted.
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

        // @0x824E7708 (BrnTextField.cpp:224) -- the INTEGER variant (the ledger carries
        // it un-named as sub_824E7708): format liValue under leFormat straight into
        // macText (128 cap, the language manager's integer formatter) and push the apt
        // data. Always reports success (the X360 returns 1). Body links from the
        // BrnTextField TU (which also carries the ledger-named free forwarder the
        // ShowtimeInstantResults consumer declared and links against).
        bool SetLocalisedText(s32 liValue,
                              CgsLanguage::LanguageManager::ParameterFormatType leFormat);

        // @0x824E7608 (BrnTextField.cpp:194) -- the FLOAT variant (ledger-unnamed
        // sub_824E7608, the sibling of the integer one above): format lfValue under
        // leFormat into macText (128 cap) through the language manager's float formatter
        // and push the apt data. Always reports success (the X360 returns 1).
        // ⚠️ THE ARGUMENT IS AN f32 AND THAT IS ASM-PINNED, NOT INFERRED. The value
        // arrives in f1 and the format in r5 -- the float consumes the r4 GPR slot, which
        // is exactly the documented Hex-Rays phantom-parameter shape (its listing shows a
        // spurious `int a3`). CrashNavDriverDetails::HandleStatData @0x824B8618 converts
        // its integer stat words up with `extsw/std/lfd/fcfid/frsp` before every call,
        // which is only meaningful if the callee takes a float.
        // ⛔ THIS OVERLOAD MATTERS FOR CORRECTNESS, not just for linking: without it an
        // `f32` argument silently narrows into the s32 overload above and the value is
        // formatted by the INTEGER formatter (so an AUTO_DISTANCE mileage would print as
        // a bare count). Declared before the first caller landed for that reason.
        bool SetLocalisedText(f32 lfValue,
                              CgsLanguage::LanguageManager::ParameterFormatType leFormat);

        // @0x824E7C30 (BrnTextField.cpp:356) -- the ONE-INTEGER-PARAMETER variant
        // (ledger-unnamed sub_824E7C30): resolve lpacText under leFormat with liValue
        // substituted as its single parameter under leValueFormat, through
        // LanguageManager::FormatTextFromInt (1024-byte scratch), then adopt the result
        // with SetDatabaseText. Asserts a non-null, <127-character source string
        // (BrnTextField.cpp:356/:357). Always reports success (the X360 returns 1).
        // ⚠️ Distinct from the variadic form below, whose third argument is a PARAMETER
        // COUNT -- a 4-argument call binds here, and must, or the value would be read as
        // a count.
        bool SetLocalisedText(const char* lpacText,
                              CgsLanguage::LanguageManager::ParameterFormatType leFormat,
                              s32 liValue,
                              CgsLanguage::LanguageManager::ParameterFormatType leValueFormat);

        // @0x824E7800 (BrnTextField.cpp:264/:265/:266) -- the POSITIONAL-PARAMETER variant
        // (ledger-unnamed sub_824E7800). liNumParams (1..3) `(const char* text, u32 format)`
        // pairs follow in the varargs; the source id and every parameter are resolved through
        // LanguageManager::FormatTextV, and the result is adopted with SetDatabaseText.
        // Always reports success (the X360 returns 1). Body links from the BrnTextField TU.
        bool SetLocalisedText(const char* lpacText,
                              CgsLanguage::LanguageManager::ParameterFormatType leFormat,
                              s32 liNumParams, ...);

        // @0x824E7A20 (BrnTextField.cpp:314/:315/:316) -- the ARRAY form of the above
        // (ledger-unnamed sub_824E7A20): the parameter texts and their format types arrive as
        // two parallel arrays and are resolved through
        // LanguageManager::Obsolete_FormatTextByArray. Body links from the BrnTextField TU.
        bool SetLocalisedText(const char* lpacText,
                              CgsLanguage::LanguageManager::ParameterFormatType leFormat,
                              s32 liNumParams, const char* const* lppacParams,
                              const CgsLanguage::LanguageManager::ParameterFormatType* lpeParamFormats);

        // @0x824E5020 (BrnTextField.cpp:100, DWARF h:191) -- adopt already-resolved text:
        // strings that fit go straight into macText; over-long strings are registered in
        // the localisation database under this component's name and displayed as the
        // "$<name>" database key. Body links from the BrnTextField TU.
        void SetDatabaseText(const char* lpcActualText);

        // @0x824E52B8 (BrnTextField.cpp:511) -- push the field's current contents to its bound
        // apt clip. Body links from the BrnTextField TU.
        void OutputAptData();

        // @0x824470F0 - copy-assign from lrSource. Reproduces the X360 byte-copy, which copies
        // everything from +0x04 onward and leaves the +0x00 vtable slot untouched.
        TextField& operator=(const TextField& lrSource);

        // ADDITIVE GROW (ImageGalleryCarouselItem::HandleLoadNotifications @0x82419BF8):
        // the field's current text. The X360 caller reads macText's address directly
        // (field+0xA4) to re-push the stored text through SetText after a load
        // notification; exposed by name so that caller stays off the raw offset.
        const char* GetText() const { return macText; }

        // ADDITIVE GROW (ReplayHudMessageComponent::HideMessage @0x8241C078): blank the
        // field's displayed text. The X360 caller writes macText[0] = 0 inline (the
        // stb 0,0xA4(field) store) before re-outputting; exposed by name so that caller
        // stays off the raw offset. Faithful 1-store accessor (macText is private).
        void ClearText() { macText[0] = 0; }

        // ADDITIVE GROW (BrnGui::TextSelection::Construct @0x824E8128): raise the field's
        // autosize flag. DWARF names the helper SetAutoSize (BrnTextField.h); the X360
        // inlines it to the single `li 1 / stb r11, +0x126(field)` store that closes
        // TextSelection::Construct (asm 0x824E8264..0x824E8268, i.e. this+0xCBE with the
        // field at this+0xB98). Faithful 1-store accessor (mbAutosize is private).
        void SetAutoSize(bool lbAutosize) { mbAutosize = lbAutosize; }

        // ADDITIVE GROW (BrnGui::CrashNavEnterOnlineBase::ShowTOS @0x824BC598 and the
        // Show{CreateAccount,OpenAccountInUS} siblings): raise the reset-scroll flag
        // before OutputAptData. The X360 inlines it to the single li 1 / stb r11,
        // +0x125(field) store (field+0x125 == mbResetScroll).
        // NO-ARG per the DecFIGS DWARF (BrnTextField.h:98/124 `void ResetScroll();`),
        // which also lists it as the inlined callee at those sites. An earlier wave-I
        // draft proposed a SetResetScroll(bool) accessor -- that spelling appears nowhere
        // in the DWARF and was a fabrication; do not reintroduce it.
        void ResetScroll() { mbResetScroll = true; }

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
