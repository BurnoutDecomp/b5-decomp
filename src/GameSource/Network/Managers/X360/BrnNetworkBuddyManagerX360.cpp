#include <xtl.h>
#include "GameSource/Network/Managers/X360/BrnNetworkBuddyManagerX360.h"

#include <cstring>   // strncpy, memcpy, strlen
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // CgsModule::VariableEventQueue<14000,16>
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"    // CgsNetwork::PlayerManager::AmIHost
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h" // IsLocalPlayerInGame / GetGameParameters
#include "GameShared/GameClasses/Network/Buddies/DirtySock/X360/CgsBuddyManagerDirtySockX360.h"          // CgsNetwork::BuddyManagerX360::ShowProfile
#include "GameSource/Network/BrnNetworkModule.h"                        // BrnNetwork::BrnNetworkModule
#include "GameSource/Network/BrnNetworkManager.h"                       // BrnNetwork::BrnNetworkManager
#include "GameSource/Network/BrnServerInterface.h"                      // BrnNetwork::BrnServerInterface::GetGameComponent
#include "GameSource/Network/BrnNetworkModuleIO.h"                      // BrnNetworkModuleIO::PostSimulationInputBuffer
#include "GameSource/Network/BrnNetworkGameParams.h"                    // BrnNetwork::GameParams

// ---- XDK / Xbox-LIVE entry points -------------------------------------------
// Real prototypes live in the Xbox 360 XDK (<xapi.h>/<xonline.h>/<winbase.h>); declared here
// as extern "C" free functions, mirroring the sibling CgsBuddyManagerDirtySockX360.cpp /
// BrnNetworkGamerCardManagerX360.cpp glue precedent. Argument shapes are taken from the X360
// call sites (register usage), not the PPC Hex-Rays.
#include "protomangle.h"   // ProtoMangleEncodeSession



namespace BrnNetwork
{
    namespace
    {
        // The CgsModule queue base the BrnNetwork IN-event AddEvent call sites drive.
        typedef CgsModule::VariableEventQueue<14000, 16> NetworkEventQueueConcrete;

        inline NetworkEventQueueConcrete* AsNetworkQueue(BrnNetworkModuleIO::NetworkEventQueue* lpQueue)
        {
            return lpQueue;
        }

        // ---- network IN-event payloads ----------------------------------------
        // The cross-title invite-status events DoInvite posts: a single s32 status code carried
        // as event type 8, size 4. The status values are X360-authoritative immediates:
        //   1 == already in the session (not host)   -- DoInvite var_E0 = 1
        //   2 == already in the session, as host      -- DoInvite var_E0 = 2
        //   3 == invited user is not signed in        -- DoInvite var_E0 = 3
        struct InviteStatusEvent
        {
            s32 miStatus;   // +0x00
        };
        const s32 KI_INVITE_STATUS_EVENT_TYPE = 8;

        const s32 KI_INVITE_STATUS_IN_SESSION_NOT_HOST = 1;
        const s32 KI_INVITE_STATUS_IN_SESSION_AS_HOST  = 2;
        const s32 KI_INVITE_STATUS_NOT_SIGNED_IN       = 3;

        // The "buddy list updated" poke ProcessNetworkQueue mirrors for the online/offline/join
        // event types (8/9/11) -- event type 11, size 1.
        struct BuddyListUpdatedEvent
        {
            u8 muUnused;    // +0x00
        };
        const s32 KI_BUDDY_LIST_UPDATED_EVENT_TYPE = 11;

        // ---- ProcessNetworkQueue inbound event-type tags (X360 switch immediates) -----------
        // The switch is `(eventType - 8)` over a 6-entry jump table (cases 8..13).
        const s32 KI_NETWORK_EVENT_BUDDY_ONLINE   = 8;   // jumptable case 0
        const s32 KI_NETWORK_EVENT_BUDDY_OFFLINE  = 9;   // jumptable case 1
        const s32 KI_NETWORK_EVENT_BUDDY_JOIN     = 11;  // jumptable case 3
        const s32 KI_NETWORK_EVENT_SHOW_PROFILE   = 13;  // jumptable case 5

        // ---- Update / Live-invite immediates ------------------------------------------------
        // XNotifyCreateListener area mask the manager listens on (Construct: li r3, 6 -- the
        // system + 'live' notification areas).
        const unsigned long long KU_NOTIFY_LISTENER_AREAS = 6ull;

        // The accepted cross-title-invite notification id (Update cmplw against 0x2000002 ==
        // XN_LIVE_INVITE_ACCEPTED).
        const unsigned long KU_NOTIFY_INVITE_ACCEPTED = 0x2000002ul;

        // The Burnout title id XInviteGetAcceptedInfo must report for the accepted invite to be
        // acted on (Update cmplw against 0x45410806).
        const u32 KU_BURNOUT_TITLE_ID = 0x45410806u;

        // The number of local controller pads scanned for the inviter's own XUID (Update loops
        // [0,4); a match means the local user invited themselves and the join is skipped).
        const s32 KI_NUMBER_OF_PADS = 4;

        // The session-id string length copied/compared throughout (KI_GAMEPARAMS_SESSION_LENGTH
        // == 128; matches CgsNetwork::KI_GAMEPARAMS_SESSION_LENGTH and the X360 strncpy/strnicmp
        // count immediates 0x80).
        const u32 KU_SESSION_ID_LENGTH = 128u;

        // XINVITE_INFO accessor view (the canonical XDK XINVITE_INFO). XInviteGetAcceptedInfo fills
        // this block; the X360 reads the inviting (host) XUID at +0x08 (an 'ld' at 0x82573B94) and
        // the title id at +0x10 (an 'lwz' at 0x82573B78 / 0x82573BD4). The leading 8 bytes are the
        // invitee XUID; the inviter XUID at +0x08 is the session-host identity. The +0x14 host
        // session-info region (XSESSION_INFO: XNKID/XNKEY/XNADDR) is what ProtoMangleEncodeSession
        // writes the encoded session string into (its lpcSessionOut at 0x82573BEC == pInfo+0x14).

    }

    // =======================================================================
    // Construct @ 0x825579F0
    // =======================================================================
    void BuddyManagerX360::Construct(BrnNetworkModule* lpNetworkModule, BrnServerInterface* lpServerInterface)
    {
        BuddyManagerBase::Construct(lpNetworkModule, lpServerInterface);

        mhNotifier = XNotifyCreateListener(KU_NOTIFY_LISTENER_AREAS);
        CGS_ASSERT(mhNotifier, "mhNotifier");

        macPendingSessionID[0] = 0;
        miPendingInviteUserPort = 0;
    }

    // =======================================================================
    // Destruct @ 0x82557A78
    // =======================================================================
    void BuddyManagerX360::Destruct()
    {
        macPendingSessionID[0] = 0;
        miPendingInviteUserPort = 0;

        CloseHandle(mhNotifier);
        mhNotifier = NULL;

        BuddyManagerBase::Destruct();
    }

    // =======================================================================
    // Prepare @ 0x8254C818
    // =======================================================================
    bool BuddyManagerX360::Prepare(const BrnHW::LaunchData* lpSoftRebootData)
    {
        if (!BuddyManagerBase::Prepare())
        {
            return false;
        }

        CGS_ASSERT(lpSoftRebootData, "lpSoftRebootData");

        // FLAG: view of the launch data's two read fields (see BuddySoftRebootData).
        const BuddySoftRebootData* lpRebootData = reinterpret_cast<const BuddySoftRebootData*>(lpSoftRebootData);

        // Length-guard the soft-reboot session id (the X360 inlined CgsString length check that
        // fires "String too long: " if the id is >= 128 bytes before the strncpy).
        CGS_ASSERT(strlen(lpRebootData->macSessionID) < KU_SESSION_ID_LENGTH, "String too long: ");

        strncpy(macPendingSessionID, lpRebootData->macSessionID, KU_SESSION_ID_LENGTH);
        miPendingInviteUserPort = lpRebootData->miUserControllerPort;
        return true;
    }

    // =======================================================================
    // Release @ 0x8254C978
    // =======================================================================
    bool BuddyManagerX360::Release()
    {
        return BuddyManagerBase::Release();
    }

    // =======================================================================
    // IsUserInGameSession @ 0x8256C718
    //
    // True if the local game's current session id matches lpcSessionID.
    // =======================================================================
    bool BuddyManagerX360::IsUserInGameSession(const char* lpcSessionID)
    {
        CGS_ASSERT(lpcSessionID, "lpcSessionID");

        CGS_ASSERT(GetNetworkModule(), "GetNetworkModule()");
        CGS_ASSERT(GetNetworkModule()->GetNetworkManager(), "GetNetworkModule()->GetNetworkManager()");

        BrnServerInterface* lpServerInterface = GetNetworkModule()->GetNetworkManager()->GetServerInterface();
        CGS_ASSERT(lpServerInterface, "lpServerInterface");
        CGS_ASSERT(lpServerInterface->GetGameComponent(), "lpServerInterface->GetGameComponent()");

        if (!lpServerInterface->GetGameComponent()->IsLocalPlayerInGame())
        {
            return false;
        }

        GameParams lGameParams;
        lGameParams.Prepare();
        lpServerInterface->GetGameComponent()->GetGameParameters(&lGameParams);

        return strnicmp(lGameParams.GetSession(), lpcSessionID, KU_SESSION_ID_LENGTH) == 0;
    }

    // =======================================================================
    // DoInvite @ 0x825700A8
    //
    // Begin a Live invite/join for the given launch parameters.
    // =======================================================================
    void BuddyManagerX360::DoInvite(BrnNetworkModuleIO::InviteOrJoinParams lInviteOrJoinParams)
    {
        CGS_ASSERT(GetNetworkModule(), "GetNetworkModule()");
        CGS_ASSERT(GetNetworkModule()->GetNetworkManager(), "GetNetworkModule()->GetNetworkManager()");
        CGS_ASSERT(GetNetworkModule()->GetNetworkManager()->GetPlayerManager(),
                   "GetNetworkModule()->GetNetworkManager()->GetPlayerManager()");

        BrnNetworkManager* lpNetworkManager = GetNetworkModule()->GetNetworkManager();
        CgsNetwork::PlayerManager* lpPlayerManager = lpNetworkManager->GetPlayerManager();

        // The invite targets a different controller port than the local signed-in user's
        // (X360: miUserControllerPort != NetworkManager+0x60).
        const bool lbDifferentUser =
            lInviteOrJoinParams.miUserControllerPort != lpNetworkManager->GetLocalUserControllerPort();

        if (!IsUserInGameSession(lInviteOrJoinParams.macSessionID)
            || (lbDifferentUser && !lpPlayerManager->AmIHost()))
        {
            if (lbDifferentUser)
            {
                if (!XUserGetSigninState(lInviteOrJoinParams.miUserControllerPort))
                {
                    InviteStatusEvent lEvent;
                    lEvent.miStatus = KI_INVITE_STATUS_NOT_SIGNED_IN;
                    AsNetworkQueue(GetNetworkModule()->GetNetworkEventQueue())
                        ->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lEvent),
                                   KI_INVITE_STATUS_EVENT_TYPE, 4);
                    return;
                }

                CGS_ASSERT(strlen(lInviteOrJoinParams.macSessionID) < KU_SESSION_ID_LENGTH,
                           "String too long: ");

                // Build the Guide soft-reboot launch data (0x98 bytes) and relaunch the title into
                // the joined session. Offsets are X360-authoritative (XSetLaunchData(&var_C0, 0x98)
                // with the named stores at var_C0 + 0x00 / +0x04 / +0x14 / +0x94):
                //   +0x00 miUserControllerPort  (the requesting user port)
                //   +0x04 macUserName[16]       (XUserGetName for that port; the gamertag)
                //   +0x14 macSessionID[128]     (strncpy of the invite session id, 128)
                //   +0x94 mbIsLocalUserChange   (stb 1)
                struct BuddyLaunchData
                {
                    s32  miUserControllerPort;  // +0x00
                    char macUserName[16];       // +0x04
                    char macSessionID[128];     // +0x14
                    u8   mbIsLocalUserChange;   // +0x94 (X360 stb 1)
                    u8   maPad95[0x98 - 0x95];  // -> +0x98
                } lLaunchData;

                lLaunchData.miUserControllerPort = lInviteOrJoinParams.miUserControllerPort;
                XUserGetName(lInviteOrJoinParams.miUserControllerPort, lLaunchData.macUserName, 16);
                strncpy(lLaunchData.macSessionID, lInviteOrJoinParams.macSessionID, KU_SESSION_ID_LENGTH);
                lLaunchData.mbIsLocalUserChange = 1;
                XSetLaunchData(&lLaunchData, 0x98);
            }

            BuddyManagerBase::StartInvite(lInviteOrJoinParams);
            return;
        }

        // Already in the named session: report whether we are the host or not.
        InviteStatusEvent lEvent;
        if (lbDifferentUser && lpPlayerManager->AmIHost())
        {
            lEvent.miStatus = KI_INVITE_STATUS_IN_SESSION_AS_HOST;
        }
        else
        {
            lEvent.miStatus = KI_INVITE_STATUS_IN_SESSION_NOT_HOST;
        }
        AsNetworkQueue(GetNetworkModule()->GetNetworkEventQueue())
            ->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lEvent),
                       KI_INVITE_STATUS_EVENT_TYPE, 4);
    }

    // =======================================================================
    // ProcessNetworkQueue @ 0x8256C8E0  (override)
    //
    // Walk the inbound network-event queue, routing the buddy "show profile" event through the
    // platform leaf's ShowProfile and the online/offline/join events into a buddy-list-updated
    // poke, then defer every event to the game base ProcessEvent.
    // =======================================================================
    void BuddyManagerX360::ProcessNetworkQueue(const BrnNetworkModuleIO::NetworkEventQueue* lpInQueue,
                                               BrnNetworkModuleIO::NetworkEventQueue* /*lpOutQueue*/)
    {
        const NetworkEventQueueConcrete* lpQueue = lpInQueue;

        const CgsModule::Event* lpEvent = NULL;
        s32 liSize = 0;

        for (s32 liEventType = lpQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != NULL;
             liEventType = lpQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            if (liEventType == KI_NETWORK_EVENT_BUDDY_ONLINE
                || liEventType == KI_NETWORK_EVENT_BUDDY_OFFLINE
                || liEventType == KI_NETWORK_EVENT_BUDDY_JOIN)
            {
                BuddyListUpdatedEvent lUpdatedEvent;
                AsNetworkQueue(GetNetworkModule()->GetNetworkEventQueue())
                    ->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lUpdatedEvent),
                               KI_BUDDY_LIST_UPDATED_EVENT_TYPE, 1);
            }
            else if (liEventType == KI_NETWORK_EVENT_SHOW_PROFILE)
            {
                CGS_ASSERT(lpEvent, "lpShowProfileEvent");
                // this IS-A CgsNetwork::BuddyManagerX360 (the platform leaf in the base chain);
                // route the show-profile event name through its Xbox Guide ShowProfile.
                CgsNetwork::BuddyManagerX360::ShowProfile(
                    reinterpret_cast<const CgsNetwork::PlayerName*>(lpEvent));
            }

            ProcessEvent(lpEvent, liEventType);
        }
    }

    // =======================================================================
    // Update @ 0x82573A40
    // =======================================================================
    u32 BuddyManagerX360::Update(const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInputBuffer,
                                 f32 lfTimeStep, bool lbProcessInvites, bool lbCanBlock)
    {
        BuddyManagerBase::Update(lbCanBlock, lfTimeStep);

        ProcessNetworkQueue(lpInputBuffer->GetNetworkEventQueue(),
                            GetNetworkModule()->GetNetworkEventQueue());

        ProcessDebugEvents();

        if (!lbProcessInvites)
        {
            return 0;
        }

        u32 luResult = 0;

        if (macPendingSessionID[0] != 0)
        {
            // A soft-reboot invite is latched: re-issue it, then clear the latch.
            BrnNetworkModuleIO::InviteOrJoinParams lParams;
            XMemCpy(lParams.macSessionID, macPendingSessionID, KU_SESSION_ID_LENGTH);
            lParams.miUserControllerPort = miPendingInviteUserPort;
            DoInvite(lParams);

            macPendingSessionID[0] = 0;
            miPendingInviteUserPort = 0;
            return luResult;
        }

        // Otherwise poll the Live notification listener for a freshly accepted cross-title invite.
        unsigned long luNotifyId = 0;
        unsigned long luNotifyParam = 0;
        luResult = XNotifyGetNext(mhNotifier, 0, &luNotifyId, &luNotifyParam);
        if (!luResult)
        {
            return luResult;
        }

        if (luNotifyId != KU_NOTIFY_INVITE_ACCEPTED)
        {
            return luResult;
        }

        XINVITE_INFO lInviteInfo;
        luResult = XInviteGetAcceptedInfo(luNotifyParam, &lInviteInfo);
        if (lInviteInfo.dwTitleID != KU_BURNOUT_TITLE_ID)
        {
            return luResult;
        }

        // If the inviting host XUID is one of the local users, the invite is a self-invite -- post
        // the "already in session (not host)" status instead of joining.
        bool lbSelfInvite = false;
        for (s32 liPad = 0; liPad < KI_NUMBER_OF_PADS; ++liPad)
        {
            u64 lLocalXuid = 0;
            luResult = XUserGetXUID(liPad, &lLocalXuid);
            if (lInviteInfo.xuidInviter == lLocalXuid)
            {
                lbSelfInvite = true;

                InviteStatusEvent lEvent;
                lEvent.miStatus = KI_INVITE_STATUS_IN_SESSION_NOT_HOST;
                luResult = AsNetworkQueue(GetNetworkModule()->GetNetworkEventQueue())
                    ->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lEvent),
                               KI_INVITE_STATUS_EVENT_TYPE, 4);
                break;
            }
        }

        if (lInviteInfo.dwTitleID == KU_BURNOUT_TITLE_ID && !lbSelfInvite)
        {
            // Encode the invite session into the accepted-invite host-session region and join it
            // through the invite pipeline. The X360 issues ProtoMangleEncodeSession(r3 = the params
            // macSessionID, r4 = 0x80, r5 = the XInviteInfo hostInfo block) at 0x82573BEC -- the
            // encode source is the params session buffer and the destination is the invite's
            // +0x14 hostInfo region (NOT the host XUID).
            BrnNetworkModuleIO::InviteOrJoinParams lParams;
            ProtoMangleEncodeSession(lParams.macSessionID, KU_SESSION_ID_LENGTH,
                                     reinterpret_cast<char*>(&lInviteInfo.hostInfo));
            lParams.miUserControllerPort = static_cast<s32>(luNotifyParam);
            DoInvite(lParams);
        }

        return luResult;
    }

    // =======================================================================
    // _AssertLayout -- never called; pins the recovered leaf-member RELATIONSHIPS.
    //
    // The X360 absolute offsets (mhNotifier @ +85712, miPendingInviteUserPort @ +85716,
    // macPendingSessionID @ +85720) assume the 32-bit base sub-object + 4-byte pointers; the PC
    // gate is 64-bit, so the raw offsets are NOT reproduced. We pin the ABI-stable relationships:
    // the pointer precedes the s32 port, which precedes the 128-byte session buffer, and the
    // buffer is the X360 session length. Same approach as CgsBuddyManagerDirtySockX360.
    // =======================================================================
    void BuddyManagerX360::_AssertLayout()
    {
        CGS_STATIC_ASSERT((sizeof(reinterpret_cast<BuddyManagerX360*>(0)->macPendingSessionID) == 128),
                      "macPendingSessionID is the 128-byte X360 session length");
        CGS_STATIC_ASSERT((offsetof(BuddyManagerX360, miPendingInviteUserPort)
                      > offsetof(BuddyManagerX360, mhNotifier)),
                      "mhNotifier precedes miPendingInviteUserPort");
        CGS_STATIC_ASSERT((offsetof(BuddyManagerX360, macPendingSessionID)
                      - offsetof(BuddyManagerX360, miPendingInviteUserPort) == 4),
                      "miPendingInviteUserPort precedes macPendingSessionID");

        // The XINVITE_INFO field offsets Update reads off the XInviteGetAcceptedInfo buffer
        // (mHostXuid via 'ld' pInfo+0x08 @ 0x82573B94; muTitleId via 'lwz' pInfo+0x10 @
        // 0x82573B78/0x82573BD4; the +0x14 hostInfo region encoded into @ 0x82573BEC).
        CGS_STATIC_ASSERT((offsetof(XINVITE_INFO, dwTitleID) == 0x10),
                      "XINVITE_INFO::dwTitleID at +0x10");
    }
}
