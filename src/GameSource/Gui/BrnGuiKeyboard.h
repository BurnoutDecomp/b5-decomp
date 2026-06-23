#pragma once

// ===================================================================================
// BrnGui::BrnGuiKeyboard  -- owning header
//   b5-decomp/src/GameSource/Gui/BrnGuiKeyboard.h
//
// The on-screen virtual keyboard GUI screen. Prepare (@0x824EACC0) latches the
// system user-profile pointer the keyboard reads/writes through; the GUI module calls
// it once, before the profile has been set, so it asserts the slot is still NULL.
//
// No prior source carries this screen's full member layout (no Feb-2007 leak entry and
// no DecFIGS DWARF for this TU). The single field Prepare touches is recovered from the
// X360 load/store sequence (mpSystemUserProfile @+0x420). The large unrecovered screen
// body that precedes it is represented by an explicit reserved-storage block so the
// pointer lands at its binary offset without raw-offset casting; all access is by name.
// ===================================================================================

#include "types.hpp"

namespace BrnGui
{
    // The system user-profile the keyboard edits. Only the pointer slot is referenced by
    // this TU; the profile type itself is not modelled here.
    class SystemUserProfile;

    class BrnGuiKeyboard
    {
    public:
        // @ 0x824EACC0 - latch the (non-NULL) system user-profile pointer into the
        // empty slot. Returns 1 (the X360 li r3, 1; b __restgprlr_29).
        s32 Prepare(SystemUserProfile* lpSystemUserProfile);

    private:
        // Reserved storage for the unrecovered keyboard-screen body that sits ahead of the
        // one field Prepare touches (object +0x420). Layout-recovery padding only; the
        // contents are not reconstructed by this TU.
        u8 maScreenBodyReserved[0x420];

        SystemUserProfile* mpSystemUserProfile;   // object +0x420 -- set by Prepare
    };
}
