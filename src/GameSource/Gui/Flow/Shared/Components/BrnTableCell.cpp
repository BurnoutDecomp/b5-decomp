#include "GameSource/Gui/Flow/Shared/Components/BrnTableCell.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

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

} // namespace BrnGui
