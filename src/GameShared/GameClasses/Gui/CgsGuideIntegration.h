#pragma once

// ===================================================================================
// CgsGui::SystemUserProfile -- owning header
//   b5-decomp/src/GameShared/GameClasses/Gui/CgsGuideIntegration.{h,cpp}
//
// Xbox-360 "system user profile" watcher. Owns an Xbox XNotify listener, tracks the
// current signed-in user (index/name/sign-in state), re-reads the two gameplay profile
// settings (external-camera + rumble) and forwards changes to a single attached Listener.
// Driven by BrnGui::GuiModule (Prepare/Release/Update) and BrnGui::ProfileHost /
// BrnGui::ProfileManager (Attach/DetachListener).
//
// SHAPE + NAMES are DWARF-authoritative (references/DecFIGS/.../CgsGuideIntegration.h):
//   Listener vtable order: 0 UserNameChanged(const char*), 1 ProfileSettingsChanged(const
//   ProfileSettings&), 2 SigninStateChanged(EUserSigninState), 3 UIClosed().
//   ProfileSettings { bool mbExternalCamera; u32 miRumble; }.
//   EUserSigninState { NOTSIGNEDIN=0, SIGNEDIN=1 }.
//
// MEMBER LAYOUT (X360-authoritative byte offsets; base ptr is u8* so displacement == offset):
//   +0x00  miUserIndex        u32     current user index; 4 == "no user" sentinel
//   +0x04  miState            u32     0 = not ready, 1 = live/ready (== signed-in state)
//   +0x08  macUserName[16]    char    current user name (XUserGetName / memcpy 16)
//   +0x18  mhNotifyListener   void*   XNotifyCreateListener handle
//   +0x1C  mbSignedIn         bool    cached sign-in flag (Update case 9, stb 0x1C)
//   +0x20  mpListener         Listener*  the single attached listener (0 if none)
// X360 32-bit offsets are NOT static_asserted (mpListener widens on the 64-bit host); members
// are accessed BY NAME.
// ===================================================================================

#include "types.hpp"

namespace CgsGui
{
    class SystemUserProfile
    {
    public:
        // DWARF CgsGuideIntegration.h:437
        enum EUserSigninState
        {
            E_USERSIGNINSTATE_NOTSIGNEDIN = 0,
            E_USERSIGNINSTATE_SIGNEDIN    = 1,
        };

        // DWARF CgsGuideIntegration.h:445
        struct ProfileSettings
        {
            bool mbExternalCamera;   // +0x00
            u32  miRumble;           // +0x04
        };

        // DWARF CgsGuideIntegration.h:451 -- abstract listener callback interface.
        class Listener
        {
        public:
            virtual void UserNameChanged(const char* lpcUserName) = 0;                     // vtable +0x00
            virtual void ProfileSettingsChanged(const ProfileSettings& lrSettings) = 0;    // vtable +0x04
            virtual void SigninStateChanged(EUserSigninState leState) = 0;                 // vtable +0x08
            virtual void UIClosed() = 0;                                                   // vtable +0x0C
        };

    public:
        // X360 0x8284CCF8 -- one-time setup: no user (miUserIndex=4), not ready (miState=0),
        // create the Xbox system-area notification listener. Returns true.
        bool Prepare();

        // X360 0x8284CD48 -- CloseHandle the notification listener. Returns true.
        bool Release();

        // X360 0x828561B8 -- drain the XNotify queue and react to sign-in / storage /
        // profile-setting / invite notifications.
        void Update();

        // X360 0x824F7720 -- bind lrListener (must be none attached). If already live,
        // immediately prime it (sign-in, user name, profile settings).
        void AttachListener(Listener& lrListener);

        // X360 0x824EAC10 -- unbind lrListener (must match attached). Clears the field.
        void DetachListener(Listener& lrListener);

    private:
        // X360 0x8284CE90 -- re-read profile settings for the current user and forward them.
        void UpdateProfileSettings();
        // X360 0x8284CD70 -- re-read the console user name; on change cache it + notify.
        void UpdateUserName();
        // X360 0x82852440 -- re-derive the sign-in state (XUserGetSigninState) + fresh user
        // name; on change latch it, notify (SigninStateChanged) and refresh name + settings.
        void UpdateUserSigninState();

        static const u32 KU_USERINDEX_NONE = 4;   // miUserIndex sentinel for "no user selected"

        // Wave B: CgsGui::SaveLoadSystem::SignIn (X360 0x8285D9E8) pokes mbSignedIn (+0x1C)
        // directly (`stb r11,0x1C(profile)`) when XShowSigninUI reports an immediate
        // failure; the X360 module writes the field without an accessor, so the save/load
        // front-end is befriended rather than fabricating a setter the binary lacks.
        friend class SaveLoadSystem;

        u32       miUserIndex;       // +0x00
        u32       miState;           // +0x04 (== EUserSigninState; 1 == signed in)
        char      macUserName[16];   // +0x08
        void*     mhNotifyListener;  // +0x18
        bool      mbSignedIn;        // +0x1C
        u8        maPad001D[3];      // +0x1D  alignment to +0x20
        Listener* mpListener;        // +0x20
    };
}
