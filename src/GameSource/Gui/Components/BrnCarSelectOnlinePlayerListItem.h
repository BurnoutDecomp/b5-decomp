#pragma once

// ===================================================================================
// BrnGui::CarSelectOnlinePlayerListItem  -- owning header
//   b5-decomp/src/GameSource/Gui/Components/BrnCarSelectOnlinePlayerListItem.h
//
// One row of the online car-select player list: a CgsGui::GuiComponent that drives an
// apt/Flash view-state when shown. Only Show (@0x8241A598) is binary-recovered for this
// TU; it marks the item visible and pushes the appropriate apt view-state (a "final"
// state once the row's selection has been locked in, otherwise the plain "visible"
// state).
//
// No prior source carries this component's full member layout (the DecFIGS DWARF
// forward-declares the class only). The two fields Show touches are recovered from the
// X360 store/load sequence (mbShown byte @+0x2E8, mbFinalSelection byte @+0x2E9). The
// large unrecovered component body between the GuiComponent base and these two flags is
// represented by an explicit reserved-storage block so the flags land at their binary
// offsets without raw-offset casting; all access is by name.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"   // CgsGui::GuiComponent

namespace BrnGui
{
    struct CarSelectOnlinePlayerListItem : public CgsGui::GuiComponent
    {
        // Reserved storage for the unrecovered component body that sits between the
        // GuiComponent base (sizeof 0x88) and the two flags Show touches (object +0x2E8).
        // Layout-recovery padding only; the contents are not reconstructed by this TU.
        u8  maComponentBodyReserved[0x2E8 - sizeof(CgsGui::GuiComponent)];

        bool mbShown;            // object +0x2E8 -- set true on Show
        bool mbFinalSelection;   // object +0x2E9 -- selects the "final" vs "visible" apt state

        // Owned by THIS TU: drive the row's apt view-state (@0x8241A598).
        void Show();
    };
}
