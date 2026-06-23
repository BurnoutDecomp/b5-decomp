// ===================================================================================
// BrnGui::CarSelectVehicle  -- implementation
//   class:BrnGui::CarSelectVehicle
//
// CarSelectVehicle (ctor) @ 0x82508670
//   Compiler-emitted construction of the vehicle-select flow state and its embedded GUI
//   sub-objects. The X360 writes the state's own vtable (+0x000, off_82075470), a long
//   run of embedded sub-object vtables across the object (up to +0x3F4C), and constructs
//   the embedded TextSelection widget (r3 = this+0x8F0, bl TextSelection::TextSelection).
//   Those per-sub-object vtable stores belong to widget types not individually
//   recoverable from this ctor; the recovered, modelled effect is "construct the
//   CgsGui::State base and the embedded TextSelection", which the member initialisation
//   here reproduces. Reconstructed from the X360 asm.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/States/BrnCarSelectVehicle.h"

namespace BrnGui
{
    // @ 0x82508670
    CarSelectVehicle::CarSelectVehicle()
        : CgsGui::State()       // state vtable + base bookkeeping (X360 +0x000)
        , mTextSelection()      // embedded widget ctor (X360 bl TextSelection::TextSelection)
    {
    }
}
