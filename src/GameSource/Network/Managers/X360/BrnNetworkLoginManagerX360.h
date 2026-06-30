#pragma once

// ===================================================================================
// BrnNetwork::LoginManagerX360 -- owning header
//   b5-decomp/src/GameSource/Network/Managers/X360/BrnNetworkLoginManagerX360.{h,cpp}
//
// The Xbox-360 concrete login manager owned by BrnNetworkManager. It is the platform leaf
// of the network login state machine:
//
//   BrnNetwork::LoginManagerBase   (cross-platform sign-in flow, BrnNetworkLoginManagerBase.h)
//     <- BrnNetwork::LoginManagerX360 (this type)
//
// On top of the base's connect/login/TOS/settings flow, this leaf drives the Xbox-LIVE
// front end of the platform-specific sub-state (E_SUBSTATE_PLATFORM_SPECIFIC_UPDATE): it
// polls the local user's Live sign-in (XUserGetSigninState), pops the sign-in / gamertag
// selection UI (TriggerEventFromLogin event 5), waits for the gamertag selection to settle
// (XUserGetSigninInfo + the privilege/guest flag), and supplies the user name (the Live
// gamertag, via the DirtySDK 'gtag' NetConnStatus selector) for the DS login. It overrides
// the base virtuals Start / Disconnected / PlatformSpecificUpdate / GetUserNameAndPassword,
// and adds its own non-virtual lifecycle (Construct / Destruct / Prepare / Release) called
// directly by BrnNetworkManager.
//
// MEMBER LAYOUT (X360-authoritative). The base BrnNetwork::LoginManagerBase ends at +0x20
// (mpTOS); this leaf adds a single member just past it:
//   +0x24  mePlatformSpecificSubState  EPlatformSpecificSubState
// Every store/compare against it is a grounded immediate (Construct/Destruct/Prepare/Release
// store E_PLATFORM_SPECIFIC_SUBSTATE_COUNT == 2; Start stores E_..._SIGN_IN_TO_LIVE == 0;
// PlatformSpecificUpdate cmplwi 1 against E_..._GAMERTAG_SELECTION; UpdateSignInToLive stores
// E_..._GAMERTAG_SELECTION == 1).
//
// The X360 byte offset above is a 32-bit-pointer/Xenon layout fact; on a 64-bit host the base
// sub-object and its pointer member widen, so the absolute offset is NOT static_asserted --
// the member is accessed BY NAME.
//
// Original home (from the asm assert strings):
//   ..\..\..\GameSource\Network/Managers/X360/BrnNetworkLoginManagerX360.cpp
// ===================================================================================

#include "types.hpp"
#include "GameSource/Network/Managers/BrnNetworkLoginManagerBase.h"  // BrnNetwork::LoginManagerBase (real base)

namespace BrnNetwork
{
    class BrnNetworkModule;

    namespace BrnNetworkModuleIO
    {
        struct PostSimulationInputBuffer;   // PlatformSpecificUpdate param (pointer only; unused by the leaf)
    }

    class LoginManagerX360 : public LoginManagerBase
    {
    public:
        // The Xbox-LIVE front-end step the platform-specific sub-state walks (the +0x24 member).
        // E_..._SIGN_IN_TO_LIVE (0) polls the local sign-in and raises the sign-in UI event;
        // E_..._GAMERTAG_SELECTION (1) waits for the gamertag selection to settle; COUNT (2) is
        // the idle/not-started sentinel. Values are the stored immediates / cmplwi operands.
        enum EPlatformSpecificSubState
        {
            E_PLATFORM_SPECIFIC_SUBSTATE_SIGN_IN_TO_LIVE     = 0,
            E_PLATFORM_SPECIFIC_SUBSTATE_GAMERTAG_SELECTION  = 1,
            E_PLATFORM_SPECIFIC_SUBSTATE_COUNT               = 2
        };

        // ---- lifecycle (non-virtual; called directly by BrnNetworkManager) ----------------
        // X360 0x8254C0C0 -- chain the base Construct, then mark the platform sub-state idle.
        void Construct(BrnNetworkModule* lpNetworkModule);
        // X360 0x8254C0F8 -- reset the base state-machine members + the platform sub-state idle.
        void Prepare();
        // X360 0x8254C130 -- reset to idle (called by BrnNetworkManager::Release).
        void Release();
        // X360 0x8254C168 -- reset to idle (called by BrnNetworkManager::Destruct).
        void Destruct();

        // ---- base virtual overrides --------------------------------------------------------
        // X360 0x82557308 -- chain the base Start, then arm the platform sub-state at SIGN_IN_TO_LIVE.
        void Start(ESignInType leSignInType) /*override*/;
        // X360 0x8254C228 -- mark the platform sub-state idle, then chain the base Disconnected.
        void Disconnected() /*override*/;

    protected:
        // X360 0x8255E7C0 -- dispatch the platform sub-state (SIGN_IN_TO_LIVE / GAMERTAG_SELECTION).
        // The lpInput param is part of the inherited virtual signature; the X360 leaf does not use it.
        void PlatformSpecificUpdate(const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInput) /*override*/;
        // X360 0x8254C198 -- fill lpcUserName with the Live gamertag ('gtag' NetConnStatus selector);
        // the password is left empty (single NUL).
        void GetUserNameAndPassword(char* lpcUserName, char* lpcPassword) /*override*/;

    private:
        // X360 0x82557340 -- poll the local Live sign-in: signed in -> advance to CONNECTING_ONLINE;
        // silent sign-in failed -> Finished(false); otherwise raise the sign-in/gamertag-selection UI.
        void UpdateSignInToLive();
        // X360 0x825573E8 -- wait for the gamertag selection to settle (XUserGetSigninInfo): on the
        // privilege/guest flag or a query failure, finish; on a clean signed-in selection, advance.
        void UpdateGamerTagSelection();

        // The 'gtag' DirtySDK NetConnStatus selector GetUserNameAndPassword reads the Live gamertag
        // through (X360 lis 0x6774 / ori 0x6167 -> 0x67746167 == 'gtag'). NetConnStatus returns 1 on
        // success. The gamertag buffer is 16 bytes (X360 li r6, 0x10).
        static const s32 KI_NETCONNSTATUS_GAMERTAG_SELECTOR = 0x67746167;   // 'gtag'
        static const s32 KI_GAMERTAG_BUFFER_SIZE            = 16;
        static const s32 KI_NETCONNSTATUS_SUCCESS           = 1;

        // The "signed in to Live" value XUserGetSigninState returns (X360 cmpwi r3, 2). Mirrors the
        // committed BuddyManagerX360 / GamerCardManagerX360 (XUserSigninState_SignedInToLive == 2).
        static const s32 KI_SIGNIN_STATE_LIVE              = 2;

        // The login-event code raised to the manager to pop the sign-in / gamertag-selection front
        // end (X360 li r4, 5 at the UpdateSignInToLive TriggerEventFromLogin call site).
        static const s32 KI_LOGIN_EVENT_SHOW_SIGN_IN_UI    = 5;

        // XUserGetSigninInfo fills an XUSER_SIGNIN_INFO; the privilege/guest flags are the WORD at
        // +0x08 (XUserSigninInfo::muFlags). The leaf finishes the flow when bit 1 (mask 2) is set
        // (X360 lwz +0x08, rlwinm ...,0,30,30 -> isolate bit 1, cmplwi 2). Grounded asm immediates.
        static const u8  KU8_SIGNIN_INFO_GUEST_FLAG_MASK   = 0x02;

        // ---- data members (X360 offset in the header note) --------------------------------
        EPlatformSpecificSubState mePlatformSpecificSubState;   // +0x24
    };
}
