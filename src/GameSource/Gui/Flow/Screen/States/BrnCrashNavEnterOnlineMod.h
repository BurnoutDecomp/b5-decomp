#pragma once

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnline.h"

// BrnGui::CrashNavEnterOnlineX360 / ...Full / ...NoTitle - the platform sign-in
// screen flavours over CrashNavEnterOnlineBase. DWARF home
// BrnCrashNavEnterOnlineMod.h (the PS3 DWARF spells the platform class
// CrashNavEnterOnlinePS3; the X360 symbol set names it CrashNavEnterOnlineX360 --
// the binary wins). Full/NoTitle pick the sign-in flavour on entry.
namespace BrnGui
{
    // The X360 platform sign-in state (DWARF-mirror of CrashNavEnterOnlinePS3).
    // Its OnEnter/machinery are its own ledger functions (declaration-only here).
    struct CrashNavEnterOnlineX360 : public CrashNavEnterOnlineBase
    {
        // @0x82487E68 -- base OnEnter, then create the Xbox system XNotify listener.
        virtual void OnEnter();
        // @0x82487EA0 -- base OnLeave, then CloseHandle + clear the listener.
        virtual void OnLeave();

        // @0x82488010 -- trigger the Xbox sign-in UI (XShowSigninUI(1, 2)). The asm ignores
        // `this` and tail-calls the XDK; modelled non-virtual (no vtable slot attested).
        u32 ShowSignInUI();

        // FLAG additive layout member (asm-attested this+0x37F0): the XNotifyCreateListener
        // system-notification listener handle -- created in OnEnter, CloseHandle'd + zeroed in
        // OnLeave, and read by Update as the XNotifyGetNext handle. Corpus convention is a
        // plain void* (the Xbox HANDLE typedef is not modelled in types.hpp).
        void* mhNotificationListener;   // @ this+0x37F0 (stw r3, 0x37F0(this))
    };

    // DWARF Mod.h:59 -- the full (titled) sign-in screen. OnEnter (Mod.cpp:38) is
    // its own ledger function (declaration-only here).
    struct CrashNavEnterOnlineFull : public CrashNavEnterOnlineX360
    {
        virtual void OnEnter();
    };

    // DWARF Mod.h:79 -- the title-less sign-in screen.
    struct CrashNavEnterOnlineNoTitle : public CrashNavEnterOnlineX360
    {
        // @0x824B6628 (this TU, Mod.cpp:61) -- platform OnEnter, then flag the
        // NO_TITLE sign-in flavour.
        virtual void OnEnter();
    };
}
