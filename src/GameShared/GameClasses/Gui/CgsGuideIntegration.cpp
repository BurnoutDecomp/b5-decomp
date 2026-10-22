#include "GameShared/GameClasses/Gui/CgsGuideIntegration.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/System/PC/CgsXUserProfileSettings.h"

#include <cstring>   // memset / memcpy (UpdateUserName)

// CgsGui::SystemUserProfile -- the X360 XNotify/XUserGetName system-user watcher.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The PC profile-settings API and
// its native pointer-bearing records share CgsXUserProfileSettings.h. Other
// platform imports below retain their existing PC providers.
extern "C"
{
    void* XNotifyCreateListener(unsigned long long luqwAreas);
    int   XNotifyGetNext(void* lhListener, unsigned long ludwMsgFilter,
                         unsigned long* lpdwId, unsigned long* lpParam);
    int   CloseHandle(void* lhObject);
    u32   XUserGetName(u32 luUserIndex, char* lpszUserName, u32 luCchUserName);
    u32   XUserGetSigninState(u32 luUserIndex);
}

namespace CgsGui
{
    // DecFIGS CgsGuideIntegration.h:472; ARTIST GuiModule::Construct82518798/A0/A8.
    void SystemUserProfile::Construct()
    {
        mpListener = 0;
        mhNotifyListener = 0;
        miUserIndex = KU_USERINDEX_NONE;
    }

    // X360 0x8284CCF8. One-time setup: no user selected (miUserIndex=4), not ready (miState=0),
    // and create the Xbox notification listener for the system areas (qwAreas=1, XNOTIFY_SYSTEM).
    // Returns true.
    bool SystemUserProfile::Prepare()
    {
        miState     = 0;                             // stw r11(0),4(this)
        miUserIndex = KU_USERINDEX_NONE;             // stw r10(4),0(this)
        mhNotifyListener = XNotifyCreateListener(1); // li r3,1 ; bl ... ; stw r3,0x18(this)
        return true;
    }

    // X360 0x8284CD48. Tear down: close the notification listener. Returns true. (No store-back
    // of mhNotifyListener afterwards, matching the asm.)
    bool SystemUserProfile::Release()
    {
        CloseHandle(mhNotifyListener);   // lwz r3,0x18(this) ; bl CloseHandle
        return true;
    }

    // X360 0x828561B8. Drain the Xbox notification queue and react to sign-in / storage /
    // profile-setting / invite notifications.
    void SystemUserProfile::Update()
    {
        if (mhNotifyListener == nullptr)
            return;

        unsigned long luId    = 0;   // var_8C (pdwId)
        unsigned long luParam = 0;   // var_90 (pParam)
        while (XNotifyGetNext(mhNotifyListener, 0, &luId, &luParam))
        {
            switch (luId)
            {
            case 9: // XN_SYS_SIGNINCHANGED
            {
                bool lbSignedIn = (luParam == 1);
                mbSignedIn = lbSignedIn;            // stb r11,0x1C(this)
                if (!lbSignedIn)
                {
                    UpdateUserSigninState();
                    if (mpListener != nullptr)
                        mpListener->UIClosed();     // vtable +12
                }
                break;
            }
            case 0xA: // XN_SYS_STORAGEDEVICESCHANGED
                UpdateUserSigninState();
                break;
            case 0xE: // XN_SYS_PROFILESETTINGCHANGED
                if (miUserIndex != KU_USERINDEX_NONE)
                {
                    UpdateUserSigninState();
                    if (((1u << miUserIndex) & luParam) != 0)
                        UpdateProfileSettings();
                }
                break;
            case 0x2000002: // XN_LIVE_INVITE_ACCEPTED
                CGS_ASSERT(false,
                           "Invite notification processed by Gui Integration class. This could break cross game invites\n");
                break;
            default:
                break;
            }
        }
    }

    // X360 0x824F7720. Bind lrListener as the profile listener; asserts if one is already
    // attached. When the profile is already live (miState==1) the new listener is immediately
    // primed (sign-in state, current user name, current profile settings).
    void SystemUserProfile::AttachListener(Listener& lrListener)
    {
        CGS_ASSERT(mpListener == nullptr,
                   "SystemUserProfile::AttachListener: listener already attached");

        u32 luState = miState;              // lwz r11,4(this)
        mpListener = &lrListener;           // stw r27,0x20(this)
        if (luState == 1)
        {
            lrListener.SigninStateChanged(E_USERSIGNINSTATE_SIGNEDIN); // vtable +8
            mpListener->UserNameChanged(macUserName);                 // vtable +0
            UpdateProfileSettings();
        }
    }

    // X360 0x824EAC10. Unbind lrListener. If it does not match the attached listener this is
    // asserted, but the field is cleared either way (both asm branches store 0 to mpListener).
    void SystemUserProfile::DetachListener(Listener& lrListener)
    {
        if (mpListener == &lrListener)
        {
            mpListener = nullptr;
        }
        else
        {
            CGS_ASSERT(false,
                       "SystemUserProfile::AttachListener: listener not already attached");
            mpListener = nullptr;
        }
    }

    // ARTIST8284CE90: read camera then vibration for the selected signed-in
    // user, and publish the same derived settings through the real Listener.
    // The platform API owns native pointer-bearing records, never guest offsets.
    void SystemUserProfile::UpdateProfileSettings()
    {
        const u32 luUserIndex = miUserIndex;
        if (luUserIndex == KU_USERINDEX_NONE || miState != 1 || mpListener == nullptr)
            return;

        const u32 laSettingIds[2] = {
            XPROFILE_GAMER_RACE_CAMERA_LOCATION,
            XPROFILE_OPTION_CONTROLLER_VIBRATION,
        };
        // Keep the original128-byte capacity. The native header plus two full
        // records occupies112 bytes on x64; typed members supply their alignment.
        constexpr u32 KU_READ_CAPACITY = 128;
        struct ReadBuffer
        {
            XUSER_READ_PROFILE_SETTING_RESULT mResults;
            XUSER_PROFILE_SETTING maSettings[2];
            u8 maUnused[KU_READ_CAPACITY - sizeof(XUSER_READ_PROFILE_SETTING_RESULT) -
                        2 * sizeof(XUSER_PROFILE_SETTING)];
        } lReadBuffer;
        static_assert(sizeof(ReadBuffer) == KU_READ_CAPACITY, "original profile-read capacity");
        u32 lcbResults = sizeof(lReadBuffer);

        if (XUserReadProfileSettings(0, luUserIndex, 2, laSettingIds,
                                     &lcbResults, &lReadBuffer.mResults, nullptr))
        {
            CGS_ASSERT(false, " problem reading user settings - check buffer length");
            // FLAG PC-platform leaf: host assertions can continue. A failed API
            // published no settings; retain its diagnostic without reading unwritten memory.
            return;
        }
        if (lcbResults > sizeof(lReadBuffer))
        {
            CGS_ASSERT(false, " data returned is too large ");
            return;
        }

        const XUSER_PROFILE_SETTING* lpSettings = lReadBuffer.mResults.pSettings;
        ProfileSettings lSettings;
        lSettings.mbExternalCamera =
            (static_cast<u32>(lpSettings[0].data.nData) - 1u) > 1u;
        lSettings.miRumble = static_cast<u32>(lpSettings[1].data.nData);
        mpListener->ProfileSettingsChanged(lSettings);
    }

    // X360 0x82852440. Re-derive the current user's sign-in state (XUserGetSigninState) and,
    // when signed in, a fresh copy of the user name; if either the state or the name changed,
    // latch the new state, notify the listener (SigninStateChanged, vtable +8) and refresh the
    // name + profile settings. (This function was missing from the batch export set; recovered
    // with a one-off IDA dump of 0x82852440 -- the assert cites CgsGuideIntegration.cpp:231.)
    void SystemUserProfile::UpdateUserSigninState()
    {
        // Snapshot the cached name; a signed-in user's fresh name is read over the snapshot.
        char lacUserName[16];
        memcpy(lacUserName, macUserName, sizeof(lacUserName));

        // New state: 0 with no user selected, else XUserGetSigninState(index) != 0
        // (cntlzw/extrwi/xori == the != 0 test).
        u32 luSignedIn;
        if (miUserIndex == KU_USERINDEX_NONE)
        {
            luSignedIn = 0;
        }
        else
        {
            luSignedIn = (XUserGetSigninState(miUserIndex) != 0) ? 1u : 0u;
        }

        // A real signed-in user: re-read the name into the snapshot buffer.
        if (miUserIndex != KU_USERINDEX_NONE && luSignedIn == 1)
        {
            const u32 luErr = XUserGetName(miUserIndex, lacUserName, 0x10);
            CGS_ASSERT(luErr == 0, "XUserGetName failed");
        }

        // Changed when the sign-in state differs, or (state unchanged) the cached name and the
        // freshly read snapshot differ (the X360's inline byte-compare loop).
        bool lbChanged = (miState != luSignedIn);
        if (!lbChanged)
        {
            const u8* lpcOld = reinterpret_cast<const u8*>(macUserName);
            const u8* lpcNew = reinterpret_cast<const u8*>(lacUserName);
            s32 liDiff;
            do
            {
                liDiff = static_cast<s32>(*lpcOld) - static_cast<s32>(*lpcNew);
                if (*lpcOld == 0)
                    break;
                ++lpcOld;
                ++lpcNew;
            }
            while (liDiff == 0);
            lbChanged = (liDiff != 0);
        }

        if (lbChanged)
        {
            Listener* lpListener = mpListener;   // read before the state store, as the X360 does
            miState = luSignedIn;                // stw r28, 4(this)
            if (lpListener != nullptr)
            {
                lpListener->SigninStateChanged(static_cast<EUserSigninState>(luSignedIn)); // vtable +8
            }
            UpdateUserName();
            UpdateProfileSettings();
        }
    }

    // X360 0x8284CD70. Re-read the console signed-in user name; if it changed from the cached copy,
    // latch the new name and notify the attached listener (Listener::UserNameChanged, vtable 0).
    void SystemUserProfile::UpdateUserName()
    {
        char lacUserName[16];
        memset(lacUserName, 0, sizeof(lacUserName));   // stb 0 + memset(var_3F,0,0xF)

        // Only query the SDK for a real, signed-in user (index 4 == no-user sentinel).
        if (miUserIndex != KU_USERINDEX_NONE && miState == E_USERSIGNINSTATE_SIGNEDIN)
        {
            const u32 luErr = XUserGetName(miUserIndex, lacUserName, 0x10);
            CGS_ASSERT(luErr == 0, "XUserGetName failed");
        }

        // strcmp-style diff between the cached name and the freshly read one.
        const u8* lpcOld = reinterpret_cast<const u8*>(macUserName);
        const u8* lpcNew = reinterpret_cast<const u8*>(lacUserName);
        s32 liDiff;
        do
        {
            liDiff = static_cast<s32>(*lpcOld) - static_cast<s32>(*lpcNew);
            if (*lpcOld == 0)
                break;
            ++lpcOld;
            ++lpcNew;
        }
        while (liDiff == 0);

        // Name changed -> latch it and fire the listener callback (vtable slot 0).
        if (liDiff != 0)
        {
            memcpy(macUserName, lacUserName, sizeof(macUserName));
            if (mpListener != nullptr)
                mpListener->UserNameChanged(macUserName);
        }
    }
}
