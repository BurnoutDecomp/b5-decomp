// ===================================================================================
// BrnNetwork::NetworkGamerCardManagerX360 -- behaviour
//   b5-decomp/src/GameSource/Network/Managers/X360/BrnNetworkGamerCardManagerX360.cpp
//
// The 21 X360-recovered members of the Xbox-360 gamer-card / player-profile-card manager.
// Layout, dispatch-table contents and every constant below are grounded in the recovered
// asm (see the owning header for the per-offset map and the dispatch-table dimensions).
//
// The state machine is a tiny three-level table walk:
//   * meProcess (KI_PROCESS_NONE == idle) selects a row of maProcessActions[meProcess*4 + i];
//   * SetNextAction reads the next action id from that row and dispatches mapActionFunctions[id];
//   * meSubState selects the per-frame mapUpdateFunctions[meSubState] that ProcessBeforeSimulation
//     ticks, which (on completion) calls SetNextAction to advance the action cursor.
// ===================================================================================

#include "GameSource/Network/Managers/X360/BrnNetworkGamerCardManagerX360.h"

#include "GameSource/Network/BrnServerInterface.h"  // BrnServerInterface::GetPlayerInfoComponent / GetGameComponent
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfo.h" // ServerInterfacePlayerInfo (GetStatus / GetLastError / ClearLastError)
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"      // ServerInterfaceGames (IsLocalPlayerInGame / IsPlayerInGame / GetPlayerParametersByPlayerName)
#include "GameSource/Network/Parameters/BrnNetworkPlayerParamsClass.h" // BrnNetwork::PlayerParams (stack params object for ShowGamerCard)
#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT

#include <cstring>   // std::memcpy (X360 GetXuidForPlayer / ShowGamerCard)

// ------------------------------------------------------------------------------------
// Xbox Guide / Live entry points used by the show / sign-in / notification paths. The
// game declares these as extern "C" free functions, mirroring the committed precedent in
// CgsBuddyManagerDirtySockX360.cpp / the System/X360 glue. The argument shapes are taken
// from the X360 call sites (register usage), not the PPC Hex-Rays.
// ------------------------------------------------------------------------------------
extern "C"
{
    // Guide UI -- keyed by the signed-in controller/user index, for a 64-bit XUID. 0 == success.
    // (r3 = user index, r4 = the full 64-bit XUID; matches the committed CgsBuddyManagerDirtySockX360.)
    u32 XShowGamerCardUI(u32 luUserIndex, u64 luXuid);
    // Pop the sign-in UI for cPanes panes with dwFlags. 0 == success.
    u32 XShowSigninUI(u32 luPanes, u32 luFlags);
    // Sign-in state: KI_SIGNIN_STATE_LIVE (2) == XUserSigninState_SignedInToLive.
    s32 XUserGetSigninState(u32 luUserIndex);

    // Live notification listener. CloseHandle closes it on Release.
    void* XNotifyCreateListener(unsigned long long luAreas);
    int   XNotifyGetNext(void* lpListener, u32 luMsgFilter, u32* lpidNotification, void* lpParam);
    int   CloseHandle(void* lpObject);

    // DirtySock address -> host address helper (external SDK; the manager reads the host
    // XUID back out of the filled buffer). Declared with the X360 call shape (r3 = out
    // buffer, r4 = size 8, r5 = scratch).
    int DirtyAddrToHostAddr(void* lpOutHostAddr, int liSize, void* lpScratch);
}

namespace BrnNetwork
{
    // X360 0x8254CA78 -- fill maProcessActions with the per-process action sequences.
    // Every slot starts at KI_NO_ACTION (3); the real action ids are then written in. The
    // trailing assert loop walks the 5 processes and fires if a process's FIRST action slot
    // is still KI_NO_ACTION (no actions supplied).
    void NetworkGamerCardManagerX360::InitialiseProcessArray()
    {
        for (s32 li = 0; li < KI_NUMBER_OF_PROCESSES * KI_ACTIONS_PER_PROCESS; ++li)
        {
            maProcessActions[li] = KI_NO_ACTION;   // *v1++ = 3
        }

        // Action ids: 0 == ActionReqXuidByName, 1 == ActionShowSignInUI,
        //             2 == ActionShowGamercardUI, 3 == KI_NO_ACTION.
        maProcessActions[1]  = 2;   // result[1]  = 2
        maProcessActions[6]  = 2;   // result[6]  = 2
        maProcessActions[8]  = 2;   // result[8]  = 2
        maProcessActions[13] = 2;   // result[13] = 2
        maProcessActions[0]  = 0;   // *result    = 0
        maProcessActions[4]  = 0;   // result[4]  = 0
        maProcessActions[5]  = 1;   // result[5]  = 1
        maProcessActions[12] = 1;   // result[12] = 1
        maProcessActions[16] = 0;   // result[16] = 0

        // The X360 asserts the first action slot of each of the 5 processes is not KI_NO_ACTION.
        for (s32 liProcess = 0; liProcess < KI_NUMBER_OF_PROCESSES; ++liProcess)
        {
            CGS_ASSERT(maProcessActions[liProcess * KI_ACTIONS_PER_PROCESS] != KI_NO_ACTION,
                       "No actions supplied for this process");
        }
    }

    // X360 0x8255E838 -- fill mapUpdateFunctions and assert every slot is non-null. Slot 0 is
    // the idle/no-op update (COMDAT-folded with an empty Destruct in the image).
    void NetworkGamerCardManagerX360::InitialiseUpdateArray()
    {
        mapUpdateFunctions[0] = &NetworkGamerCardManagerX360::UpdateIdle;            // result[20]
        mapUpdateFunctions[1] = &NetworkGamerCardManagerX360::UpdateWaitPlayerInfo;  // result[21]
        mapUpdateFunctions[2] = &NetworkGamerCardManagerX360::UpdateWaitSignInFinish;// result[22]

        for (s32 li = 0; li < KI_NUMBER_OF_SUBSTATES; ++li)
        {
            CGS_ASSERT(mapUpdateFunctions[li] != nullptr,
                       "No update function supplied for this substate");
        }
    }

    // X360 0x82557AC8 -- fill mapActionFunctions and assert every slot is non-null.
    void NetworkGamerCardManagerX360::InitialiseActionArray()
    {
        mapActionFunctions[0] = &NetworkGamerCardManagerX360::ActionReqXuidByName;   // result[23]
        mapActionFunctions[1] = &NetworkGamerCardManagerX360::ActionShowSignInUI;    // result[24]
        mapActionFunctions[2] = &NetworkGamerCardManagerX360::ActionShowGamercardUI; // result[25]

        for (s32 li = 0; li < KI_NUMBER_OF_ACTIONS; ++li)
        {
            CGS_ASSERT(mapActionFunctions[li] != nullptr,
                       "No function supplied for this action");
        }
    }

    // X360 0x82561890 -- build the dispatch tables and bring the manager to its idle state.
    void NetworkGamerCardManagerX360::Construct(BrnServerInterface* lpServerInterface)
    {
        InitialiseUpdateArray();
        InitialiseActionArray();
        InitialiseProcessArray();

        CGS_ASSERT(lpServerInterface != nullptr, "lpServerInterface");

        mpServerInterface          = lpServerInterface;   // *(a1 + 148) = a2
        maRequestedPlayerName.macName[0] = '\0';          // stb 0, 0x68
        mRequestedGamerCardXUID    = 0;                   // std 0, 0x78
        muRequestingControllerPort = KU_NUMBER_OF_PADS;   // stw 4, 0x80
        meSubState                 = 0;                   // stw 0, 0x84
        meProcess                  = KI_PROCESS_NONE;     // stw 5, 0x88
        miActionIndex              = 0;                   // stb 0, 0x8C
        mpCompleteCallback         = nullptr;             // stw 0, 0x98
        miCompleteUserData         = 0;                   // stw 0, 0x9C
    }

    // X360 0x8254CA40 -- tear the manager back down to its zero/idle state.
    void NetworkGamerCardManagerX360::Destruct()
    {
        miCompleteUserData         = 0;                   // stw 0, 0x9C
        mpCompleteCallback         = nullptr;             // stw 0, 0x98
        miActionIndex              = 0;                   // stb 0, 0x8C
        meProcess                  = KI_PROCESS_NONE;     // stw 5, 0x88
        meSubState                 = 0;                   // stw 0, 0x84
        maRequestedPlayerName.macName[0] = '\0';          // stb 0, 0x68
        mRequestedGamerCardXUID    = 0;                   // std 0, 0x78
        muRequestingControllerPort = KU_NUMBER_OF_PADS;   // stw 4, 0x80
        mpServerInterface          = nullptr;             // stw 0, 0x94
    }

    // X360 0x8254D348 -- reset the request state on a network disconnect.
    void NetworkGamerCardManagerX360::Disconnected()
    {
        maRequestedPlayerName.macName[0] = '\0';          // stb 0, 0x68
        mRequestedGamerCardXUID    = 0;                   // std 0, 0x78
        muRequestingControllerPort = KU_NUMBER_OF_PADS;   // stw 4, 0x80
        meSubState                 = 0;                   // stw 0, 0x84
        meProcess                  = KI_PROCESS_NONE;     // stw 5, 0x88
        miActionIndex              = 0;                   // stb 0, 0x8C
    }

    // X360 0x8254C9A8 -- create the Live notification listener and clear the sub-state.
    bool NetworkGamerCardManagerX360::Prepare()
    {
        mNotifyListener = XNotifyCreateListener(1);   // stw XNotifyCreateListener(1), 0x90
        meSubState      = 0;                          // stw 0, 0x84
        return true;                                  // li r3, 1
    }

    // X360 0x8254C9F0 -- close the Live notification listener.
    bool NetworkGamerCardManagerX360::Release()
    {
        void* lpListener = mNotifyListener;           // r3 = *(a1 + 144)
        meSubState       = 0;                         // stw 0, 0x84
        if (lpListener)
        {
            CloseHandle(lpListener);
            mNotifyListener = nullptr;                // stw 0, 0x90
        }
        return true;                                  // li r3, 1
    }

    // X360 0x8254CC50 -- dispatch the update function for the current sub-state.
    s32 NetworkGamerCardManagerX360::ProcessBeforeSimulation()
    {
        CGS_ASSERT(mapUpdateFunctions[meSubState] != nullptr,
                   "No update function supplied for the current substate");
        return (this->*mapUpdateFunctions[meSubState])();
    }

    // X360 0x8254CB28 -- finish the current request: fire the completion callback (if any) and
    // reset the manager to idle. Returns liStatus (or the callback's return when it fired).
    s32 NetworkGamerCardManagerX360::ProcessComplete(s32 liStatus)
    {
        s32 liResult = liStatus;                      // r3 = a2

        CompleteCallback lpfnComplete = mpCompleteCallback;   // r11 = *(a1 + 152)
        if (lpfnComplete)
        {
            s32 liUserData = miCompleteUserData;      // r5 = *(a1 + 156)
            u64 lXuid      = mRequestedGamerCardXUID; // r4 = *(a1 + 120)
            mpCompleteCallback = nullptr;             // stw 0, 0x98
            miCompleteUserData = 0;                   // stw 0, 0x9C
            liResult = lpfnComplete(liStatus, lXuid, liUserData);  // bctrl (r3=status, r4=xuid, r5=userdata)
        }

        maRequestedPlayerName.macName[0] = '\0';      // stb 0, 0x68
        mRequestedGamerCardXUID    = 0;               // std 0, 0x78
        meSubState                 = 0;               // stw 0, 0x84
        miActionIndex              = 0;               // stb 0, 0x8C
        muRequestingControllerPort = KU_NUMBER_OF_PADS;// stw 4, 0x80
        meProcess                  = KI_PROCESS_NONE; // stw 5, 0x88
        return liResult;
    }

    // X360 0x8254CBA8 -- advance to the next action in the current process (or complete it).
    s32 NetworkGamerCardManagerX360::SetNextAction()
    {
        s32 liActionId = maProcessActions[meProcess * KI_ACTIONS_PER_PROCESS + miActionIndex];

        // The end-of-process cursor (4) or a KI_NO_ACTION slot both finish the process with
        // status 1.
        if (miActionIndex == KI_ACTIONS_PER_PROCESS || liActionId == KI_NO_ACTION)
        {
            return ProcessComplete(1);
        }

        s32 liActionResult = (this->*mapActionFunctions[liActionId])();   // bctrl mapActionFunctions[id]
        if (liActionResult)
        {
            ++miActionIndex;                          // ++*(a1 + 140)
            return liActionResult;
        }
        return ProcessComplete(0);
    }

    // X360 0x82557B60 -- poll the by-name xuid request, then advance the action sequence.
    s32 NetworkGamerCardManagerX360::UpdateWaitPlayerInfo()
    {
        s32 liResult = PlayerInfoIdle();
        if (liResult)
        {
            return SetNextAction();
        }
        return liResult;
    }

    // X360 0x82557BA0 -- poll the sign-in, then advance the action sequence.
    s32 NetworkGamerCardManagerX360::UpdateWaitSignInFinish()
    {
        s32 liResult = IsSignedInToLive();
        if (liResult)
        {
            return SetNextAction();
        }
        return liResult;
    }

    // mapUpdateFunctions[0] -- the idle/no-op sub-state update (empty in the image).
    s32 NetworkGamerCardManagerX360::UpdateIdle()
    {
        return 0;
    }

    // X360 0x8254CD18 -- ask the player-info component to resolve the requested name's XUID.
    s32 NetworkGamerCardManagerX360::ActionReqXuidByName()
    {
        if (maRequestedPlayerName.macName[0] != '\0')
        {
            // BrnNetwork::PlayerName and CgsNetwork::PlayerName share the same 16-byte char[16]
            // layout (BrnNetworkSharedIO.h mirrors the committed CgsNetwork::PlayerName); the X360
            // passes the raw this+0x68 name pointer to the Cgs query.
            mpServerInterface->GetPlayerInfoComponent()->GetPlayerXUIDByName(
                reinterpret_cast<const CgsNetwork::PlayerName*>(&maRequestedPlayerName),
                &mRequestedGamerCardXUID);
            meSubState = 1;                           // stw 1, 0x84
            return 1;                                 // li r3, 1
        }

        CGS_ASSERT(false, "Trying to request gamercard for an empty player name");
        return 0;
    }

    // X360 0x8254CDE0 -- pop the Xbox Guide sign-in UI; enter the wait-for-sign-in sub-state.
    s32 NetworkGamerCardManagerX360::ActionShowSignInUI()
    {
        u32 luResult = XShowSigninUI(1, 2);           // XShowSigninUI(1u, 2u)
        meSubState   = 2;                             // stw 2, 0x84
        return luResult == 0;                         // return v2 == 0
    }

    // X360 0x8254CE28 -- pop the Xbox Guide gamer-card UI for the requested XUID; enter the
    // wait-for-sign-in sub-state.
    s32 NetworkGamerCardManagerX360::ActionShowGamercardUI()
    {
        CGS_ASSERT(muRequestingControllerPort != KU_NUMBER_OF_PADS,
                   "muRequestingControllerPort != CgsInput::KU_NUMBER_OF_PADS");
        CGS_ASSERT(mRequestedGamerCardXUID != 0, "mRequestedGamerCardXUID != 0");

        // The X360 sets up only two arg registers: r3 = the controller/user-index member
        // (lwz r3, 0x80(r30)) and r4 = the full 64-bit requested XUID (ld r4, 0x78(r30)).
        s32 liResult = XShowGamerCardUI(muRequestingControllerPort, mRequestedGamerCardXUID);
        meSubState   = 2;                             // stw 2, 0x84
        return liResult == 0;                         // return v9 == 0
    }

    // X360 0x8254CED0 -- poll the player-info component's idle/error state for the request.
    s32 NetworkGamerCardManagerX360::PlayerInfoIdle()
    {
        CGS_ASSERT(mpServerInterface != nullptr, "mpServerInterface");
        CGS_ASSERT(mpServerInterface->GetPlayerInfoComponent() != nullptr,
                   "mpServerInterface->GetPlayerInfoComponent()");

        u32 luStatus = mpServerInterface->GetPlayerInfoComponent()->GetStatus();
        if (luStatus == 0)
        {
            // Busy: nothing to do yet.
            return 0;
        }
        if (luStatus == 1)
        {
            // Error: report it, clear it, and abandon the request.
            CGS_ASSERT(false, "ERROR running gamercard process");
            mpServerInterface->GetPlayerInfoComponent()->ClearLastError();
            ProcessComplete(0);
            return 0;
        }
        if (luStatus < 3)
        {
            // Idle / complete: proceed.
            return 1;
        }

        CGS_ASSERT(false, "Player info component is in a bad state.");
        return 0;
    }

    // X360 0x8254D0B0 -- poll the Live notification queue for the sign-in-completed event.
    s32 NetworkGamerCardManagerX360::IsSignedInToLive()
    {
        u32  luNotification = 0;
        u32  luParam[3]     = { 0, 0, 0 };
        if (XNotifyGetNext(mNotifyListener, 0, &luNotification, luParam))
        {
            if (luNotification == static_cast<u32>(KI_NOTIFY_SIGNIN_CHANGED))
            {
                if (!luParam[0])
                {
                    if (XUserGetSigninState(muRequestingControllerPort) == KI_SIGNIN_STATE_LIVE)
                    {
                        return 1;
                    }
                    ProcessComplete(0);
                }
            }
            else if (luNotification == static_cast<u32>(KI_NOTIFY_INVITE_ACCEPTED))
            {
                CGS_ASSERT(false,
                    "Invite notification processed by Online main menu screen. "
                    "This could break cross game invites");
            }
        }
        return 0;
    }

    // X360 0x8254D258 -- request a remote player's XUID by gamer name. Only starts from idle
    // (meProcess == KI_PROCESS_NONE). Arms the by-name xuid process (process 4).
    s32 NetworkGamerCardManagerX360::GetXuidForPlayer(u32 luRequestingControllerPort,
                                                      const PlayerName* lpPlayerName,
                                                      CompleteCallback lpfnComplete,
                                                      s32 liUserData)
    {
        if (meProcess != KI_PROCESS_NONE)
        {
            return 0;
        }

        std::memcpy(&maRequestedPlayerName, lpPlayerName, sizeof(PlayerName)); // memcpy(a1+104, a3, 16)
        muRequestingControllerPort = luRequestingControllerPort;              // stw r31, 0x80
        mRequestedGamerCardXUID    = 0;                                       // std 0, 0x78

        CGS_ASSERT(maRequestedPlayerName.macName[0] != '\0',
                   "Trying to request xuid for an empty player name");

        mpCompleteCallback = lpfnComplete;            // stw r26, 0x98
        miCompleteUserData = liUserData;              // stw r25, 0x9C
        miActionIndex      = 0;                        // stb 0, 0x8C
        meProcess          = 4;                        // stw 4, 0x88
        meSubState         = 1;                        // stw 1, 0x84
        return 1;
    }

    // X360 0x8254D1A0 -- show a remote player's gamer card by XUID. Only starts from idle.
    s32 NetworkGamerCardManagerX360::ShowGamerCardForXuid(u32 luRequestingControllerPort,
                                                          u64 lPlayerXuid)
    {
        s32 liResult = 0;
        if (meProcess == KI_PROCESS_NONE)
        {
            CGS_ASSERT(luRequestingControllerPort != KU_NUMBER_OF_PADS,
                       "luRequestingControllerPort != CgsInput::KU_NUMBER_OF_PADS");
            CGS_ASSERT(lPlayerXuid != 0, "lPlayerXuid != 0");

            mRequestedGamerCardXUID    = lPlayerXuid;             // std r28, 0x78
            muRequestingControllerPort = luRequestingControllerPort; // stw r29, 0x80
            liResult = XUserGetSigninState(luRequestingControllerPort);

            mpCompleteCallback = nullptr;             // stw 0, 0x98
            miCompleteUserData = 0;                   // stw 0, 0x9C
            miActionIndex      = 0;                    // stb 0, 0x8C
            meSubState         = 1;                    // stw 1, 0x84

            // Signed in to Live -> jump straight to the show-card process (2); otherwise the
            // sign-in-first process (3).
            meProcess = (liResult == KI_SIGNIN_STATE_LIVE) ? 2 : 3;   // li 3 / li 2 ; stw 0x88
        }
        return liResult;
    }

    // X360 0x8256C9F0 -- show a remote player's gamer card, identifying them by name.
    //   * If the player is in the local game, resolve their XUID through the games component's
    //     player-parameters host address and arm the show-card process (2).
    //   * Otherwise, when idle, latch the name, query the local sign-in state, and arm the
    //     show process (signed-in -> 0, else 1) keyed by the by-name path.
    s32 NetworkGamerCardManagerX360::ShowGamerCard(u32 luRequestingControllerPort,
                                                   const PlayerName* lpPlayerName)
    {
        CgsNetwork::ServerInterfaceGames* lpGameComponent = mpServerInterface->GetGameComponent();
        CGS_ASSERT(lpGameComponent != nullptr, "lpGameComponent");

        s32 liResult = lpGameComponent->IsLocalPlayerInGame();
        if (liResult)
        {
            CGS_ASSERT(lpPlayerName->macName[0] != '\0',
                       "Trying to request gamercard for an empty player name");

            liResult = lpGameComponent->IsPlayerInGame(lpPlayerName->macName);
            if (liResult)
            {
                // Resolve the in-game player's host address (and thence XUID) through the
                // games component's player parameters. The X360 builds a PlayerParams object
                // on the stack (vtable off_82083550), arms it via the inherited
                // PlayerParamsBase::Prepare, then fills it through the games component; the
                // host-address helper reads the XUID back out.
                BrnNetwork::PlayerParams lPlayerParams;
                lPlayerParams.Prepare();
                lpGameComponent->GetPlayerParametersByPlayerName(lpPlayerName->macName, &lPlayerParams);

                u64 lHostAddr = 0;
                u8  laScratch[16] = { 0 };
                DirtyAddrToHostAddr(&lHostAddr, 8, laScratch);

                muRequestingControllerPort = luRequestingControllerPort;  // stw r21, 0x80
                mRequestedGamerCardXUID    = lHostAddr;                    // std r11, 0x78
                meProcess                  = 2;                           // li 2 ; stw 0x88

                mpCompleteCallback = nullptr;         // *(a1 + 152) = 0
                miCompleteUserData = 0;               // *(a1 + 156) = 0
                miActionIndex      = 0;               // *(a1 + 140) = 0
                meSubState         = 1;               // *(a1 + 132) = 1
                return liResult;
            }
        }

        if (meProcess == KI_PROCESS_NONE)
        {
            std::memcpy(&maRequestedPlayerName, lpPlayerName, sizeof(PlayerName)); // memcpy(a1+104, a3, 16)
            mRequestedGamerCardXUID    = 0;                              // *(a1 + 120) = 0
            muRequestingControllerPort = luRequestingControllerPort;     // *(a1 + 128) = a2

            CGS_ASSERT(maRequestedPlayerName.macName[0] != '\0',
                       "Trying to request gamercard for an empty player name");

            liResult  = XUserGetSigninState(luRequestingControllerPort);
            // Signed in to Live -> process 0 (show), else process 1 (sign-in then show).
            meProcess = (liResult != KI_SIGNIN_STATE_LIVE);              // *(a1+136) = result != 2

            mpCompleteCallback = nullptr;             // *(a1 + 152) = 0
            miCompleteUserData = 0;                   // *(a1 + 156) = 0
            miActionIndex      = 0;                   // *(a1 + 140) = 0
            meSubState         = 1;                   // *(a1 + 132) = 1
        }
        return liResult;
    }
}
