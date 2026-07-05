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

} // namespace BrnGui
