// CgsGui::SaveLoadSystem -- wave-B fan-out partfile 06 (sign-in lane).
// X360 ARTIST reconstruction of the not-signed-in / Xbox sign-in UI handlers:
//   BootupHandleNotSignedInOption (0x8285F200), SignIn (0x8285D9E8),
//   SignInUIClosed (0x82859D48). Bodies only -- layout/decls are frozen in the headers.
//
// Include-order rule (load-bearing): CgsSaveLoadX360.h FIRST -- it forward-declares
// ::SystemUserProfile / ::LanguageManager at global scope; pulling CgsGuideIntegration.h
// (which defines CgsGui::SystemUserProfile) in first would flip the member types inside
// namespace CgsGui (ODR hazard). CgsGuideIntegration.h therefore goes LAST so SignIn can
// poke SystemUserProfile::mbSignedIn through the wave-B friendship.
#include "GameShared/GameClasses/Gui/CgsSaveLoadX360.h"

#include "SDKs/Realmc/RealmcMemcardInterface.h"
#include "SDKs/Realmc/RealmcMemcardInterfaceImpl.h"
#include "SDKs/Realmc/RealmcLoadEntryInfo.h"

#include "GameShared/GameClasses/Gui/CgsGuideIntegration.h"

namespace
{
    // Xbox 360 XDK C-API sign-in blade. Precedent: BrnNetworkGamerCardManagerX360.cpp
    // (`u32 XShowSigninUI(u32 luPanes, u32 luFlags)`); SignIn tail-uses (cPanes=1, dwFlags=0).
    u32 XShowSigninUI(u32 luPanes, u32 luFlags);
}

namespace CgsGui
{
    // X360 0x8285F200. Bootup "not signed in" prompt choice. Option 1 pops the sign-in UI
    // once the message box closes; any other option reports failure to the live handler.
    void SaveLoadSystem::BootupHandleNotSignedInOption(u32 luOption)
    {
        if (luOption == 1)
        {
            WaitForUIClosed(&SaveLoadSystem::SignIn);   // 0x194 pair -> &SignIn
            return;
        }

        // 0x130 is the RESULT HANDLER (committed idiom): report failure (1).
        reinterpret_cast<SaveLoadTaskResultHandler*>(mpActiveMessageDisplay)
            ->HandleSaveLoadTaskResult(static_cast<ESaveLoadTaskResult>(1));
    }

    // X360 0x8285D9E8. Pop the Xbox sign-in UI (XShowSigninUI(1, 0)). On an immediate
    // (already-signed-in) return of 0 the cached profile flag is set directly through the
    // wave-B friendship, mirroring `stb r11,0x1C(profile)`; then wait for the UI to close.
    void SaveLoadSystem::SignIn()
    {
        auto* lpProfile = mpSystemUserProfile;          // r30 = lwz 0x194(this)
        if (XShowSigninUI(1, 0) == 0)
            reinterpret_cast<CgsGui::SystemUserProfile*>(lpProfile)->mbSignedIn = true;
        WaitForUIClosed(&SaveLoadSystem::SignInUIClosed);
    }

    // X360 0x82859D48. Sign-in UI closed: a signed-in user (miField210 >= 0) continues to
    // the autosave warning; otherwise reports failure to the live handler.
    void SaveLoadSystem::SignInUIClosed()
    {
        if (miField210 >= 0)
        {
            BootupShowAutosaveWarning();
            return;
        }

        reinterpret_cast<SaveLoadTaskResultHandler*>(mpActiveMessageDisplay)
            ->HandleSaveLoadTaskResult(static_cast<ESaveLoadTaskResult>(1));
    }
}
