#include "GameSource/Gui/Flow/Shared/Components/BrnTableCell.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "GameSource/Gui/BrnGuiTextField.h"                            // BrnGui::TextField (GetText/SetText/SetColour)
#include "GameSource/Gui/Flow/Shared/Components/BrnIcon.h"             // BrnGui::IconComponent (SetState)

// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX.
//
//   Construct @0x824E68C0 -- validate the cell's construction inputs (state interface, name,
//   child component, and the component-type enum, in that X360 order) then record the
//   component kind (meComponentType, +0x04) and take ownership of the child component pointer
//   (mpComponent, +0x08). This symbol's body consumes only those four arguments; the trailing
//   parent-name / apt-id-seed parameters (declared on the method for callers such as
//   BrnGui::TableRow::Construct) are not touched here.

namespace BrnGui
{

// @ 0x824E68C0
void TableCell::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                          CgsGui::GuiComponent* lpComponent, TableCellComponentTypes leType,
                          const char* /*lpacParentName*/, u64 /*luAptId*/)
{
    CGS_ASSERT(lpStateInterface != NULL, "Invalid state interface passed");
    CGS_ASSERT(lpacName != NULL, "Invalid name passed in");
    CGS_ASSERT(lpComponent != NULL, "Invalid component passed in");
    CGS_ASSERT(leType < E_TABLECELLCOMPONENTTYPES_COUNT, "Invalid component type passed in");

    meComponentType = leType;
    mpComponent     = lpComponent;
}

// @ 0x824F2140 -- default constructor. The X360 body only installs this class's vtable
// (off_82071868) at this+0 and returns; it leaves the members for Construct/Clear to
// fill. Because TableCell is polymorphic (virtual Select/Update), the compiler emits the
// vptr store implicitly, so the C++ body is empty. DWARF attests TableCell() at h:42.
TableCell::TableCell()
{
}

// @ 0x82482DD8 -- return the wrapped text field's text. Valid only on a TextField
// cell: assert a live component (+0x08) then the TEXTFIELD kind (+0x04 == 1), then read
// the field's text buffer.
const char* TableCell::GetText() const
{
    CGS_ASSERT(mpComponent != NULL, "TableCell::GetText() Invalid cell component");
    CGS_ASSERT(meComponentType == E_TABLECELLCOMPONENTTYPES_TEXTFIELD,
               "TableCell::GetText() Cannot get text from a non-TextField cell");

    return static_cast<const TextField*>(mpComponent)->GetText();
}

// @ 0x824E95D0 -- set the wrapped text field's text. Valid only on a TextField cell
// (+0x04 == 1); forwards straight to TextField::SetText.
void TableCell::SetText(const char* lpacText)
{
    CGS_ASSERT(mpComponent != NULL, "TableCell::SetText() Invalid cell component");
    CGS_ASSERT(meComponentType == E_TABLECELLCOMPONENTTYPES_TEXTFIELD,
               "TableCell::SetText() Cannot set text in a non-TextField cell");

    static_cast<TextField*>(mpComponent)->SetText(lpacText);
}

// @ 0x82482EE8 -- set the wrapped text field's colour by numeric value. Valid only on a
// TextField cell. The second assert message is verbatim a copy-paste of SetText()'s (a
// defect in the original source, reproduced).
void TableCell::SetColourValue(u32 luColour)
{
    CGS_ASSERT(mpComponent != NULL, "TableCell::SetColourValue() Invalid cell component");
    CGS_ASSERT(meComponentType == E_TABLECELLCOMPONENTTYPES_TEXTFIELD,
               "TableCell::SetText() Cannot set text in a non-TextField cell");

    static_cast<TextField*>(mpComponent)->SetColour(luColour);
}

// @ 0x824E4330 -- select the wrapped icon's state by index. Valid only on an ICON cell
// (+0x04 == 2). The X360 body is IconComponent::SetState(u32) inlined.
void TableCell::SetIconState(u32 luState)
{
    CGS_ASSERT(mpComponent != NULL, "TableCell::SetIconState() Invalid cell component");
    CGS_ASSERT(meComponentType == E_TABLECELLCOMPONENTTYPES_ICON,
               "TableCell::SetIconState() Cannot set state of a non-Icon cell");

    static_cast<IconComponent*>(mpComponent)->SetState(luState);
}

// =======================================================================================
// The five members below were DECLARED in BrnTableCell.h with no body anywhere in the tree.
// That was invisible while this TU was unmounted; it is mounted from 2026-08-03 (the
// BrnOnlineCustomMatch.h grow put a BrnGui::Table in BrnScreenFlow's state pool, and Table
// -> TableRow -> TableCell is a by-value chain), so they had to land with it.
// A scan of the ARTIST export set's TOP-LEVEL function names (matches inside xrefs_to /
// pseudocode are references, not bodies) finds exactly seven BrnGui::TableCell definitions:
// TableCell / Construct / GetText / SetText / SetColourValue / SetIconState / SetLocalisedText.
// So SetLocalisedText @0x82483020 IS an out-of-line body and is reconstructed from it below;
// Clear and IsText are inlined at every console call site and are recovered FROM those sites;
// Select and Update have NO body in the image at all -- see the FLAG note on them.
// =======================================================================================

// Inlined on the console; recovered from its one call site. TableRow::Construct @0x824E6F00
// pre-clears its 16-cell array by zeroing each cell's TWO DATA WORDS (+0x04 meComponentType
// and +0x08 mpComponent) and leaving the vptr at +0x00 alone -- that inline IS this method,
// as BrnTableRow.cpp's own comment at the site records.
void TableCell::Clear()
{
    meComponentType = E_TABLECELLCOMPONENTTYPES_NOTYPE;
    mpComponent     = 0;
}

// Inlined on the console. Every TextField-only method above guards on
// `meComponentType == E_TABLECELLCOMPONENTTYPES_TEXTFIELD` (the X360 `*(a1 + 4) != 1`), and
// TableRow::SetLocalisedText asserts this predicate with the message "Trying to set localised
// text to something that is not a textfield" -- so the predicate is that same compare.
bool TableCell::IsText()
{
    return meComponentType == E_TABLECELLCOMPONENTTYPES_TEXTFIELD;
}

// @ 0x82483020 -- the localised-text face of the cell. Four entry guards (BrnTableCell.h
// lines 232/233/234/235) then a fifth inside the parameterised arm (line 243), in the X360's
// order, then ONE OF TWO TextField overloads depending on whether parameters were supplied:
//   * liNumParams != 0 -> the ARRAY form @0x824E7A20 (parameter texts and their format types
//     as two parallel arrays), which is the shape TableRow::SetLocalisedText hands down;
//   * liNumParams == 0 -> the plain two-argument form @0x824E7418.
// Both return the language-manager's success flag, which this method tail-returns.
// The first guard is the interesting one: parameters may only be substituted into text that
// is itself a language-database id, so `leFormat != E_FORMAT_ID_LOOKUP` (the X360's literal
// `a3 != 9`) with parameters present is the error it names.
bool TableCell::SetLocalisedText(const char* lpacText,
                                 CgsLanguage::LanguageManager::ParameterFormatType leFormat,
                                 s32 liNumParams, const char* const* lppacParams,
                                 CgsLanguage::LanguageManager::ParameterFormatType* lpeParamFormats)
{
    CGS_ASSERT(leFormat == CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP || liNumParams == 0,
               "Attempting to localise parameterised text not in language DB in TableCell::SetLocalisedText()"); // h:232
    CGS_ASSERT(mpComponent != NULL,
               "TableCell::SetLocalisedText() Invalid cell component");                                          // h:233
    CGS_ASSERT(meComponentType == E_TABLECELLCOMPONENTTYPES_TEXTFIELD,
               "TableCell::SetLocalisedText() Cannot set text in a non-TextField cell");                         // h:234
    CGS_ASSERT(lpacText != NULL,
               "Incorrect Text Data passed to TableCell::SetLocalisedText()");                                   // h:235

    TextField* lpTextField = static_cast<TextField*>(mpComponent);

    if (liNumParams != 0)
    {
        CGS_ASSERT(lppacParams != NULL,
                   "Bad Parameter Data passed through to TableCell::SetLocalisedText()");                        // h:243

        return lpTextField->SetLocalisedText(lpacText, leFormat, liNumParams,
                                             lppacParams, lpeParamFormats);
    }

    return lpTextField->SetLocalisedText(lpacText, leFormat);
}

// FLAG link scaffold: EMPTY BODIES, NOT RECONSTRUCTIONS.
// TableCell is polymorphic (its vptr at +0x00 is X360-attested, off_82071868), so defining
// TableCell::TableCell() above makes MSVC emit the vtable here, and the vtable references
// every virtual -- these two are the cost of that ctor, exactly as the CN_ENTER_ONLINE block
// in BrnScreenStatesLinkStubs.cpp documents. There is NO evidence to reconstruct them from:
// the class ledger has no Select/Update entry, the ARTIST export set has no
// BrnGui::TableCell::Select or ::Update symbol, and nothing in this tree dispatches either
// (the only Table consumer, BrnGui::OnlineCustomMatch, has no .cpp at all). On the console
// the two slots are therefore either bodies IDA never named or the image-wide ICF `{}` fold;
// that cannot be settled here because the decrypted XEX needed to read off_82071868 is not in
// the working tree -- only the .i64 is.
// ⚠️ DELETE-WHEN: replace these the moment a Table consumer lands, and do NOT let a caller
// appear while they are still empty.
void TableCell::Select() {}
// FLAG link scaffold: EMPTY BODY, NOT A RECONSTRUCTION -- see the note on Select() above.
void TableCell::Update() {}

} // namespace BrnGui
