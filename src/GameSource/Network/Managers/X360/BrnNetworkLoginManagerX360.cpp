// ===================================================================================
// BrnNetwork::LoginManagerX360 -- behaviour
//   b5-decomp/src/GameSource/Network/Managers/X360/BrnNetworkLoginManagerX360.cpp
//
// The 10 X360-recovered members of the Xbox-360 concrete login manager (see the owning
// header for the layout note and the platform-sub-state enum). On top of the cross-platform
// BrnNetwork::LoginManagerBase flow, this leaf drives the Xbox-LIVE sign-in front end:
//   * UpdateSignInToLive polls XUserGetSigninState; when signed in it hands off to the base
//     connect flow, when a silent sign-in failed it finishes, otherwise it raises the
//     sign-in / gamertag-selection UI event and enters E_..._GAMERTAG_SELECTION;
//   * UpdateGamerTagSelection waits for the selection to settle (XUserGetSigninInfo) -- the
//     privilege/guest flag or a failed query finishes, a clean selection advances;
//   * GetUserNameAndPassword supplies the Live gamertag (the DirtySDK 'gtag' selector) as the
//     DS user name, with an empty password.
//
// Every literal/constant below is grounded in a stored immediate / cmpwi-cmplwi operand /
// rodata string in the recovered asm; the X360 method address is given at each function head.
// ===================================================================================

#include "GameSource/Network/Managers/X360/BrnNetworkLoginManagerX360.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Network/BrnNetworkManager.h"   // BrnNetworkManager (TriggerEventFromLogin / GetServerInterface / GetLocalUserControllerPort)
#include "GameSource/Network/BrnServerInterface.h"   // BrnServerInterface::GetConnectionComponent
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h"  // ServerInterfaceConnection::DisconnectFromServer

// ------------------------------------------------------------------------------------
// DirtySDK / Xbox-LIVE entry points used by the X360 sign-in front end. The game declares
// these as extern "C" free functions, mirroring the committed precedent in
// BrnNetworkGamerCardManagerX360.cpp / BrnNetworkLoginManagerBase.cpp. The argument shapes
// are taken from the X360 call sites (register usage), not the PPC Hex-Rays.
// ------------------------------------------------------------------------------------

// DirtySDK connection-status query (vendor SDK; same declaration as BrnNetworkLoginManagerBase.cpp).
// The selector is a FourCC; 'gtag' fills the supplied buffer with the local Live gamertag.
extern "C" int NetConnStatus(int luSelector, int liData, void* lpBuf, int liBufSize);

extern "C"
{
    // Live sign-in state for a user/controller index: KI_SIGNIN_STATE_LIVE (2) ==
    // XUserSigninState_SignedInToLive (matches the committed GamerCard / Buddy managers).
    s32 XUserGetSigninState(u32 luUserIndex);

    // Fill an XUSER_SIGNIN_INFO for the user index. Returns 0 on success (a non-zero result is an
    // error). The X360 reads the privilege/guest flags word at byte offset +0x08 of the filled
    // struct. dwFlags is 0 at the call site.
    s32 XUserGetSigninInfo(u32 luUserIndex, u32 luFlags, void* lpSigninInfo);
}

namespace BrnNetwork
{
    namespace
    {
        // The XUSER_SIGNIN_INFO buffer XUserGetSigninInfo fills (the X360 builds a 48-byte stack
        // BYREF buffer and reads only the flags word at +0x08). Only the grounded field is named;
        // the surrounding bytes are reserved as opaque storage rather than fabricated. FLAGGED: the
        // struct's full Xbox-SDK layout is not reproduced -- only the +0x08 flags word is read.
        struct XUserSigninInfo
        {
            u8  maPad00[0x08];   // +0x00 .. +0x08 (xuid + sign-in state; opaque)
            u32 muFlags;         // +0x08 -- privilege / guest flags word
            u8  maPad0C[48 - 0x0C];  // +0x0C .. +0x30 (gamertag etc.; opaque)
        };
    }

    // X360 0x8254C0C0 -- chain the base Construct, then mark the platform sub-state idle.
    void LoginManagerX360::Construct(BrnNetworkModule* lpNetworkModule)
    {
        LoginManagerBase::Construct(lpNetworkModule);
        mePlatformSpecificSubState = E_PLATFORM_SPECIFIC_SUBSTATE_COUNT;   // li 2 ; stw 0x24
    }

    // X360 0x8254C0F8 -- reset the base state-machine members and the platform sub-state idle.
    void LoginManagerX360::Prepare()
    {
        meSignInState         = E_SIGN_IN_STATE_COUNT;                     // stw 3,  0x10
        meSubState            = E_SUBSTATE_COUNT;                          // stw 0xF, 0x0C
        meSignInType          = E_SIGN_IN_TYPE_COUNT;                      // stw 3,  0x14
        meCompletedSignInType = E_SIGN_IN_TYPE_COUNT;                      // stw 3,  0x18
        mpTOS                 = nullptr;                                   // stw 0,  0x20
        mePlatformSpecificSubState = E_PLATFORM_SPECIFIC_SUBSTATE_COUNT;   // stw 2,  0x24
    }

    // X360 0x8254C130 -- reset to idle (called by BrnNetworkManager::Release).
    void LoginManagerX360::Release()
    {
        meSignInState         = E_SIGN_IN_STATE_COUNT;                     // stw 3,  0x10
        mePlatformSpecificSubState = E_PLATFORM_SPECIFIC_SUBSTATE_COUNT;   // stw 2,  0x24
        meSubState            = E_SUBSTATE_COUNT;                          // stw 0xF, 0x0C
        meSignInType          = E_SIGN_IN_TYPE_COUNT;                      // stw 3,  0x14
        meCompletedSignInType = E_SIGN_IN_TYPE_COUNT;                      // stw 3,  0x18
        mpTOS                 = nullptr;                                   // stw 0,  0x20
    }

    // X360 0x8254C168 -- reset to idle (called by BrnNetworkManager::Destruct).
    void LoginManagerX360::Destruct()
    {
        meSignInState         = E_SIGN_IN_STATE_COUNT;                     // stw 3,  0x10
        mePlatformSpecificSubState = E_PLATFORM_SPECIFIC_SUBSTATE_COUNT;   // stw 2,  0x24
        meSubState            = E_SUBSTATE_COUNT;                          // stw 0xF, 0x0C
        meSignInType          = E_SIGN_IN_TYPE_COUNT;                      // stw 3,  0x14
        meCompletedSignInType = E_SIGN_IN_TYPE_COUNT;                      // stw 3,  0x18
        mpTOS                 = nullptr;                                   // stw 0,  0x20
    }

    // X360 0x82557308 -- chain the base Start, then arm the platform sub-state at SIGN_IN_TO_LIVE.
    void LoginManagerX360::Start(ESignInType leSignInType)
    {
        LoginManagerBase::Start(leSignInType);
        mePlatformSpecificSubState = E_PLATFORM_SPECIFIC_SUBSTATE_SIGN_IN_TO_LIVE;   // li 0 ; stw 0x24
    }

    // X360 0x8254C228 -- mark the platform sub-state idle, then chain the base Disconnected.
    void LoginManagerX360::Disconnected()
    {
        mePlatformSpecificSubState = E_PLATFORM_SPECIFIC_SUBSTATE_COUNT;   // li 2 ; stw 0x24
        LoginManagerBase::Disconnected();                                 // b base::Disconnected
    }

    // X360 0x8255E7C0 -- dispatch the platform sub-state.
    void LoginManagerX360::PlatformSpecificUpdate(const BrnNetworkModuleIO::PostSimulationInputBuffer* /*lpInput*/)
    {
        if (mePlatformSpecificSubState == E_PLATFORM_SPECIFIC_SUBSTATE_SIGN_IN_TO_LIVE)
        {
            UpdateSignInToLive();
        }
        else if (mePlatformSpecificSubState == E_PLATFORM_SPECIFIC_SUBSTATE_GAMERTAG_SELECTION)
        {
            UpdateGamerTagSelection();
        }
        else
        {
            CGS_ASSERT(false, "Unknown platform specific update state");
        }
    }

    // X360 0x8254C198 -- fill lpcUserName with the Live gamertag; leave the password empty.
    void LoginManagerX360::GetUserNameAndPassword(char* lpcUserName, char* lpcPassword)
    {
        const u32 luUserIndex = static_cast<u32>(mpNetworkManager->GetLocalUserControllerPort());
        if (NetConnStatus(KI_NETCONNSTATUS_GAMERTAG_SELECTOR, static_cast<int>(luUserIndex),
                          lpcUserName, KI_GAMERTAG_BUFFER_SIZE) != KI_NETCONNSTATUS_SUCCESS)
        {
            CGS_ASSERT(false, "Error getting gamertag");
            lpcUserName[0] = '\0';
        }
        lpcPassword[0] = '\0';
    }

    // X360 0x82557340 -- poll the local Live sign-in.
    void LoginManagerX360::UpdateSignInToLive()
    {
        const u32 luUserIndex = static_cast<u32>(mpNetworkManager->GetLocalUserControllerPort());
        if (XUserGetSigninState(luUserIndex) == KI_SIGNIN_STATE_LIVE)
        {
            // Signed in to Live -> hand off to the base connect flow.
            meSubState = E_SUBSTATE_CONNECTING_ONLINE;            // li 1 ; stw 0x0C
        }
        else if (meSignInType == E_SIGN_IN_TYPE_SILENT)
        {
            // A silent sign-in that found no signed-in user just fails the flow.
            Finished(false);
        }
        else
        {
            // Raise the sign-in / gamertag-selection front end and wait for it.
            mePlatformSpecificSubState = E_PLATFORM_SPECIFIC_SUBSTATE_GAMERTAG_SELECTION;  // li 1 ; stw 0x24
            meSignInState              = E_SIGN_IN_STATE_SIGNING_IN;                       // li 0 ; stw 0x10
            mpNetworkManager->TriggerEventFromLogin(KI_LOGIN_EVENT_SHOW_SIGN_IN_UI, 0);    // li 5 / li 0
        }
    }

    // X360 0x825573E8 -- wait for the gamertag selection to settle.
    void LoginManagerX360::UpdateGamerTagSelection()
    {
        if (meSignInState == E_SIGN_IN_STATE_FAILED)
        {
            // The front end reported failure: drop the DS connection and store the DONE tail
            // directly (X360: DisconnectFromServer then the meSignInState/meSubState/
            // meCompletedSignInType/meSignInType stores -- the same tail Finished writes, but with
            // an unconditional disconnect).
            mpNetworkManager->GetServerInterface()->GetConnectionComponent()->DisconnectFromServer();
            const ESignInType leSignInType = meSignInType;
            meSignInState         = E_SIGN_IN_STATE_COUNT;       // stw 3,   0x10
            meSubState            = E_SUBSTATE_DONE;             // stw 0xE, 0x0C
            meCompletedSignInType = leSignInType;                // stw r10, 0x18
            meSignInType          = E_SIGN_IN_TYPE_COUNT;        // stw 3,   0x14
            return;
        }

        if (meSignInState == E_SIGN_IN_STATE_SUCCESS)
        {
            XUserSigninInfo lSigninInfo;
            const u32 luUserIndex = static_cast<u32>(mpNetworkManager->GetLocalUserControllerPort());
            // A failed query (non-zero) or the guest/privilege flag (bit 1) finishes the flow;
            // a clean signed-in selection advances to the base connect flow.
            if (XUserGetSigninInfo(luUserIndex, 0, &lSigninInfo) != 0 ||
                (lSigninInfo.muFlags & KU8_SIGNIN_INFO_GUEST_FLAG_MASK) == KU8_SIGNIN_INFO_GUEST_FLAG_MASK)
            {
                Finished(false);
            }
            else
            {
                meSubState = E_SUBSTATE_CONNECTING_ONLINE;       // li 1 ; stw 0x0C
            }
        }
    }
}
