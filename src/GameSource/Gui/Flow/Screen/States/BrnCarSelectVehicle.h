#pragma once

// ===================================================================================
// BrnGui::CarSelectVehicle  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnCarSelectVehicle.h
//
// The car-select "vehicle" screen flow state (the vehicle-browsing page of car select).
// Constructed by BrnGui::BrnScreenFlow::Prepare. It is a large composite GUI state built
// from many embedded widget sub-objects, including a BrnGui::TextSelection list.
//
// No prior source carries this state's full member layout (no Feb-2007 leak entry and no
// DecFIGS DWARF for this TU). The ctor (@0x82508670) is the compiler-emitted construction
// of the state plus its embedded sub-objects: it writes the state's own vtable (+0x000),
// a long sequence of embedded sub-object vtables across the object (the highest store is
// +0x3F4C, so the object size is at least 0x3F50), and constructs the embedded
// BrnGui::TextSelection widget (X360 r3 = this+0x8F0). Those per-sub-object vtable stores
// belong to widget types not individually recoverable from this single ctor; the
// recovered, modelled shape is the CgsGui::State base plus the embedded TextSelection,
// with the remaining sub-object body kept as reserved storage. The X360 offsets are
// documentary (the x64 gate's 8-byte pointers do not reproduce them). All access is by
// name.
// ===================================================================================

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnTextSelection.h"

namespace BrnGui
{
    struct CarSelectVehicle : public CgsGui::State
    {
        // @ 0x82508670 - construct the vehicle-select state and its embedded widgets.
        CarSelectVehicle();

    private:
        // The embedded text-selection widget the ctor constructs (X360 this+0x8F0).
        // Modelled by name; the many sibling sub-objects across the object are unrecovered.
        TextSelection mTextSelection;
    };
}
