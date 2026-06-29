#pragma once

// ===================================================================================
// BrnGui::Intro  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnIntro.h
//
// The game's intro screen flow state (the boot intro: licence / photo-booth / welcome-text
// sequence). Constructed by BrnGui::BrnScreenFlow::Prepare. It is a large composite GUI
// state built from many embedded widget/component sub-objects (animation, licence,
// photo-booth and text-field components).
//
// No prior source carries this state's member layout (no Feb-2007 leak entry and no
// DecFIGS DWARF for this TU). The base is modelled as CgsGui::State on two independent
// grounds: (1) the ctor's only caller is BrnGui::BrnScreenFlow::Prepare, the construction
// path every screen flow state goes through; and (2) exact structural parity with the
// committed sibling flow states (BrnCarSelectVehicle / BrnCrashNavColourCalibrate), which
// are CgsGui::State and share this ctor shape. The ctor (@0x824FFF58) is the
// compiler-emitted construction of the state plus its embedded sub-objects: it writes the
// state's own vtable (+0x000, off_820740E0) and ~20 further embedded sub-object vtable
// pointers across the object (the highest store is at +0x1044, so the object is at least
// 0x1048 = 4168 bytes). Those per-sub-object vtable stores belong to component/widget types
// not homed by this single ctor TU; following the committed reserved-body convention (cf.
// BrnCarSelectVehicle / BrnCrashNavColourCalibrate), the recovered, modelled effect is
// "construct the CgsGui::State base", with the remaining sub-object body kept as unrecovered
// storage rather than emitting raw-offset vtable stores. Unlike those siblings, Intro's asm
// contains no embedded-widget constructor call (no `bl ...::TextSelection`), so no embedded
// widget member is modelled here. The X360 offsets are documentary (the x64 gate's 8-byte
// pointers do not reproduce them). All access is by name.
// ===================================================================================

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"

namespace BrnGui
{
    struct Intro : public CgsGui::State
    {
        // @ 0x824FFF58 - construct the intro state and its embedded component widgets.
        Intro();
    };
}
