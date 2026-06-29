// ===================================================================================
// BrnGui::Intro  -- implementation
//   class:BrnGui::Intro
//
// Intro (ctor) @ 0x824FFF58
//   Compiler-emitted construction of the intro flow state and its embedded GUI sub-objects.
//   The X360 writes the state's own vtable (+0x000, off_820740E0) followed by ~20 embedded
//   sub-object vtable pointers spread across the object (+0x44, +0xD0, +0x180, +0x2A8,
//   +0x3D0, +0x4F8, +0x620, +0x748, +0x89C, +0x93C, +0x9C8, +0xA58, +0xAE8, +0xB74, +0xC04,
//   +0xCD0, +0xD5C, +0xDF4, +0xF1C, +0x1044). The highest store is at +0x1044 (4-byte
//   pointer), so the recovered object size is at least 0x1048 = 4168 bytes. The asm contains
//   no embedded-widget constructor call (no `bl`), so no sub-object ctor is modelled. Those
//   per-sub-object vtable stores are the compiler's initialisation of component/widget types
//   not homed by this ctor; per the committed reserved-body convention the recovered,
//   modelled effect is "construct the CgsGui::State base", which the member initialisation
//   here reproduces. Reconstructed from the X360 asm. Mirrors BrnCarSelectVehicle.cpp.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/States/BrnIntro.h"

namespace BrnGui
{
    // @ 0x824FFF58
    Intro::Intro()
        : CgsGui::State()       // state vtable + base bookkeeping (X360 +0x000, off_820740E0)
    {
    }
}
