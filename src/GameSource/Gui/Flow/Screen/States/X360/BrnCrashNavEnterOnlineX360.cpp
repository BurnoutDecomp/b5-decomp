#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnlineMod.h"

// BrnGui::CrashNavEnterOnlineX360 -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// The Xbox-360 platform flavour of the crash-nav "enter online" sign-in screen. It owns
// the system XNotify listener (created on entry, closed on leave) and drives the XDK
// sign-in UI. DWARF primary file
// GameSource/Gui/Flow/Screen/States/X360/BrnCrashNavEnterOnlineX360.cpp (the assert path
// strings in the Update body name this file).
//
// Bodied here (3 ledger functions):
//   CrashNavEnterOnlineX360::OnEnter       @ 0x82487E68
//   CrashNavEnterOnlineX360::OnLeave       @ 0x82487EA0
//   CrashNavEnterOnlineX360::ShowSignInUI  @ 0x82488010
//
// CrashNavEnterOnlineX360::Update @0x82487EE8 is a separate ledger function left to the
// class:BrnGui::CrashNavEnterOnlineX360 owner: it reaches un-homed flow-state
// collaborators -- the base sign-in-phase member (this+0xC8), the base
// CrashNavEnterOnlineBase::AnswerLoginQuestion, and a local-user-index accessor at
// GuiCache+0x4B38 on the deliberately-opaque GuiCache boundary object (which the corpus
// touches only through methods, not by offset).
//
// The XNotify* / XShowSigninUI / CloseHandle entry points are the Xbox 360 XDK C-API;
// declared extern "C" at file scope with plain types, mirroring the committed
// BrnNetworkNotificationManagerX360.cpp precedent (the Xbox HANDLE / DWORD typedefs are
// not modelled in types.hpp).
extern "C"
{
    void*         XNotifyCreateListener(unsigned long long luqwAreas);
    int           CloseHandle(void* lhObject);
    unsigned long XShowSigninUI(unsigned long lcPanes, unsigned long ludwFlags);
}

namespace BrnGui
{
    // X360 0x82487E68 -- chain to the base OnEnter, then create the system notification
    // listener (qwAreas = 1, XNOTIFY_SYSTEM) and stash the handle.
    void CrashNavEnterOnlineX360::OnEnter()
    {
        CrashNavEnterOnlineBase::OnEnter();                   // bl ...Base__OnEnter
        mhNotificationListener = XNotifyCreateListener(1);    // li r3, 1 ; bl ; stw r3, 0x37F0(this)
    }

    // X360 0x82487EA0 -- chain to the base OnLeave, then close and clear the notification
    // listener (only if one is open).
    void CrashNavEnterOnlineX360::OnLeave()
    {
        CrashNavEnterOnlineBase::OnLeave();                   // bl ...Base__OnLeave
        if (mhNotificationListener != nullptr)               // lwz r3, 0x37F0(this) ; beq
        {
            CloseHandle(mhNotificationListener);             // bl CloseHandle
            mhNotificationListener = nullptr;                // li r11, 0 ; stw r11, 0x37F0(this)
        }
    }

    // X360 0x82488010 -- trigger the Xbox sign-in UI for one pane. Ignores `this`; the asm
    // tail-calls XShowSigninUI(cPanes = 1, dwFlags = 2 == XSSUI_FLAGS_SHOWONLYONLINEENABLED).
    u32 CrashNavEnterOnlineX360::ShowSignInUI()
    {
        return (u32)XShowSigninUI(1, 2);   // li r4, 2 ; li r3, 1 ; b XShowSigninUI
    }
}
