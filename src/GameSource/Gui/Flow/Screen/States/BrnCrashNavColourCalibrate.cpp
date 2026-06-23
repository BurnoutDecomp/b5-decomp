// ===================================================================================
// BrnGui::CrashNavColourCalibrate  -- implementation
//   class:BrnGui::CrashNavColourCalibrate
//
// CrashNavColourCalibrate (ctor) @ 0x82500120
//   Compiler-emitted construction of the colour-calibrate flow state and its embedded GUI
//   sub-objects. The X360 writes the state's own vtable (+0x000), two embedded sub-object
//   vtables (+0x038, +0x050), constructs the embedded TextSelection widget (r3 = this+0xE0,
//   bl TextSelection::TextSelection), then sets four more embedded sub-object slots
//   (+0xDA0, +0xEE4, +0xF70, +0x1000). The per-sub-object vtable stores belong to widget
//   types not individually recoverable from this ctor; the recovered, modelled effect is
//   "construct the CgsGui::State base and the embedded TextSelection", which the member
//   initialisation here reproduces. Reconstructed from the X360 asm.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavColourCalibrate.h"

namespace BrnGui
{
    // @ 0x82500120
    CrashNavColourCalibrate::CrashNavColourCalibrate()
        : CgsGui::State()       // state vtable + base bookkeeping (X360 +0x000)
        , mTextSelection()      // embedded widget ctor (X360 bl TextSelection::TextSelection)
    {
    }
}
