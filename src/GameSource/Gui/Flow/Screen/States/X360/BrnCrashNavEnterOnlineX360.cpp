#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnlineMod.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/Gui/BrnGuiCache.h"             // GuiCache::GetActiveControllerIndex

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
// The platform Update is bodied here too: after the base pump it answers the
// "sign in now?" question from the system sign-in notification.
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
    int           XNotifyGetNext(void* lhListener, unsigned long ludwMsgFilter,
                                 unsigned long* lpdwId, unsigned long* lpParam);
    u32           XUserGetSigninState(u32 luUserIndex);
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

    namespace
    {
        // The two system notifications the sign-in page listens for.
        const unsigned long KUL_NOTIFY_SIGNIN_CHANGED  = 9;           // sign-in state changed
        const unsigned long KUL_NOTIFY_INVITE_ACCEPTED = 0x2000002;   // a game invite was accepted

        // XUserGetSigninState's "signed in to the online service" answer.
        const u32 KU_SIGNIN_STATE_SIGNED_IN_TO_LIVE = 2;
    }

    // While the "sign in now?" page is up, a sign-in change (notification parameter 0)
    // answers the question with whether the active controller is now signed in to the
    // service. An accepted invite must not arrive here: this screen is not the one that
    // handles it.
    void CrashNavEnterOnlineX360::Update()
    {
        CrashNavEnterOnlineBase::Update();

        if (meCurrentLoginQuestion != CgsGui::E_LOGIN_QUESTION_SHOW_SIGN_IN)
        {
            return;
        }

        unsigned long luId    = 0;
        unsigned long luParam = 0;
        if (XNotifyGetNext(mhNotificationListener, 0, &luId, &luParam) == 0)
        {
            return;
        }

        if (luId == KUL_NOTIFY_SIGNIN_CHANGED)
        {
            if (luParam == 0)
            {
                CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");
                const bool lbSignedIn =
                    XUserGetSigninState(static_cast<u32>(mpGuiCache->GetActiveControllerIndex()))
                    == KU_SIGNIN_STATE_SIGNED_IN_TO_LIVE;
                AnswerLoginQuestion(lbSignedIn, false);
            }
        }
        else if (luId == KUL_NOTIFY_INVITE_ACCEPTED)
        {
            CGS_ASSERT(false,
                       "Invite notification processed by EnterOnline screen. This could break cross game invites\n");
        }
    }

    // X360 0x82488010 -- trigger the Xbox sign-in UI for one pane. Ignores `this`; the asm
    // tail-calls XShowSigninUI(cPanes = 1, dwFlags = 2 == XSSUI_FLAGS_SHOWONLYONLINEENABLED).
    u32 CrashNavEnterOnlineX360::ShowSignInUI()
    {
        return (u32)XShowSigninUI(1, 2);   // li r4, 2 ; li r3, 1 ; b XShowSigninUI
    }
}
