#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePingRegions.h"

#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterfaceErrors.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceEvents.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"   // GetLobbyAPIRef
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySockErrorHelpers.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "lobbyapi.h"        // LobbyApiInfo
#include "lobbytagfield.h"   // TagFieldFind / TagFieldGetDelim

// Region pinging: StartPingRegions resolves region 0; Update polls the name lookup and
// issues the ping; HandlePingResults records the ping (or -1) and resolves the next region,
// ending the action when the "GPS_REGIONS" list runs out.

namespace CgsNetwork
{
    namespace
    {
        // Ping-manager record cache size handed to PingManagerCreate.
        const u32 KU_LOBBYAPI_PINGMANAGERCACHE = 128;

        // Region host name buffer and the name-lookup timeout (ms).
        const s32 KI_MAX_HOST_NAME_LENGTH = 128;
        const s32 KI_NAME_RESOLVE_TIMEOUT = 5000;

        // LobbyApiInfo selector of the lobby config record.
        const s32 KI_SELECT_CONF = 0x636F6E66;   // 'conf'

        // Recorded ping for a region whose lookup or ping failed.
        const s32 KI_PING_FAILED = -1;

        // A name lookup that failed.
        const s32 KI_HOST_LOOKUP_FAILED = -1;

        const DSErrorToServerInterfaceError KA_PING_REGIONS_DS_SERVER_INTERFACE_ERROR_MAPPING[1] =
        {
            { 0, E_SERVER_INTERFACE_ERROR_NONE },
        };
    }

    const DSErrorToServerInterfaceErrorTable
    ServerInterfacePingRegions::KA_DS_ERROR_TABLE_LOOKUP[ServerInterfacePingRegions::E_ACTION_COUNT] =
    {
        { KA_PING_REGIONS_DS_SERVER_INTERFACE_ERROR_MAPPING, 1 },
    };

    const char* ServerInterfacePingRegions::KAPC_ACTION_NAMES[ServerInterfacePingRegions::E_ACTION_COUNT] =
    {
        "Ping Regions",
    };

    // The console object is laid out by Construct; the constructor only installs the vtable.
    ServerInterfacePingRegions::ServerInterfacePingRegions()
    {
    }

    ServerInterfacePingRegions::~ServerInterfacePingRegions()
    {
    }

    s32 ServerInterfacePingRegions::GetPingValue(s32 liRegion) const
    {
        CGS_ASSERT(liRegion >= 0, "liRegion >= 0");
        CGS_ASSERT(liRegion < miCurrentRegion, "liRegion < miCurrentRegion");

        return maiPingResults[liRegion];
    }

    void ServerInterfacePingRegions::Construct()
    {
        meStatus          = 2;
        mpcCurrentAction  = "";
        miLastError       = 0;
        mpServerInterface = 0;
        miCurrentRegion   = 0;
        mpPingManagerRefT = 0;
        mpHostAddress     = 0;
        miPingRequest     = -1;
        meState           = E_STATE_COUNT;
        meCurrentAction   = E_ACTION_COUNT;
        for (s32 liRegion = 0; liRegion < KI_MAX_PING_REGIONS; ++liRegion)
        {
            maiPingResults[liRegion] = KI_PING_FAILED;
        }
    }

    void ServerInterfacePingRegions::Destruct()
    {
        mpServerInterface = 0;
        miCurrentRegion   = 0;
        miPingRequest     = -1;
        mpPingManagerRefT = 0;
        mpHostAddress     = 0;
        meState           = E_STATE_COUNT;
        meCurrentAction   = E_ACTION_COUNT;
        for (s32 liRegion = 0; liRegion < KI_MAX_PING_REGIONS; ++liRegion)
        {
            maiPingResults[liRegion] = KI_PING_FAILED;
        }
    }

    bool ServerInterfacePingRegions::Prepare(ServerInterfaceDirtySock* lpServerInterface)
    {
        mpServerInterface = lpServerInterface;
        meCurrentAction   = E_ACTION_COUNT;
        mpPingManagerRefT = PingManagerCreate(KU_LOBBYAPI_PINGMANAGERCACHE);
        meCurrentAction   = E_ACTION_COUNT;
        miPingRequest     = -1;
        miCurrentRegion   = 0;
        meState           = E_STATE_COUNT;
        mpHostAddress     = 0;
        for (s32 liRegion = 0; liRegion < KI_MAX_PING_REGIONS; ++liRegion)
        {
            maiPingResults[liRegion] = KI_PING_FAILED;
        }
        return true;
    }

    bool ServerInterfacePingRegions::Release()
    {
        StopPingRegions();

        mpServerInterface = 0;
        meCurrentAction   = E_ACTION_COUNT;
        if (mpPingManagerRefT != 0)
        {
            PingManagerDestroy(mpPingManagerRefT);
            mpPingManagerRefT = 0;
        }
        miCurrentRegion = 0;
        meCurrentAction = E_ACTION_COUNT;
        miPingRequest   = -1;
        meState         = E_STATE_COUNT;
        for (s32 liRegion = 0; liRegion < KI_MAX_PING_REGIONS; ++liRegion)
        {
            maiPingResults[liRegion] = KI_PING_FAILED;
        }
        return true;
    }

    void ServerInterfacePingRegions::Update()
    {
        if (meState != E_STATE_RESOLVING)
        {
            return;
        }

        CGS_ASSERT(mpHostAddress != 0, "mpHostAddress");

        const s32 liDone = mpHostAddress->Done(mpHostAddress);
        if (liDone == KI_HOST_LOOKUP_FAILED)
        {
            HandlePingResults(KI_PING_FAILED);
            return;
        }
        if (liDone == 0)
        {
            return;
        }

        miPingRequest = PingManagerPingServer2(mpPingManagerRefT, mpHostAddress->addr, PingManagerCallback, this);
        if (miPingRequest <= 0)
        {
            HandlePingResults(KI_PING_FAILED);
            return;
        }
        meState = E_STATE_PINGING;
    }

    // Nothing to pause or restart: the server interface's suspend / resume fan-out reaches the
    // shared empty body for this component.
    void ServerInterfacePingRegions::Suspend()
    {
    }

    void ServerInterfacePingRegions::Resume()
    {
    }

    void ServerInterfacePingRegions::OnEvent(EServerInterfaceEvent leEvent, void* /*lpData*/)
    {
        if (leEvent == E_SERVER_INTERFACE_CONNECTION_EVENT_DISCONNECTED)
        {
            miLastError      = 0;
            meCurrentAction  = E_ACTION_COUNT;
            meStatus         = 2;
            mpcCurrentAction = 0;
            StopPingRegions();
        }
    }

    void ServerInterfacePingRegions::StartPingRegions()
    {
        CGS_ASSERT(meCurrentAction == E_ACTION_COUNT, "meCurrentAction == E_ACTION_COUNT");
        CGS_ASSERT(mpHostAddress == 0, "mpHostAddress == NULL");

        miPingRequest   = -1;
        miCurrentRegion = 0;
        meState         = E_STATE_COUNT;
        mpHostAddress   = 0;
        for (s32 liRegion = 0; liRegion < KI_MAX_PING_REGIONS; ++liRegion)
        {
            maiPingResults[liRegion] = KI_PING_FAILED;
        }

        if (ResolveRegion(miCurrentRegion))
        {
            meCurrentAction = E_ACTION_PING_REGIONS;
            ServerInterfaceComponent::StartActionCore(KAPC_ACTION_NAMES[E_ACTION_PING_REGIONS]);
        }
    }

    void ServerInterfacePingRegions::StopPingRegions()
    {
        if (meState == E_STATE_PINGING || meState == E_STATE_RESOLVING)
        {
            if (meState == E_STATE_PINGING)
            {
                CGS_ASSERT(miPingRequest > -1, "miPingRequest > -1");
                PingManagerCancelServerRequest(mpPingManagerRefT, static_cast<u32>(miPingRequest));
                miPingRequest = -1;
            }

            if (mpHostAddress != 0)
            {
                mpHostAddress->Free(mpHostAddress);
                mpHostAddress = 0;
            }
        }

        meState = E_STATE_COUNT;
        if (meCurrentAction != E_ACTION_COUNT)
        {
            EndAction(0);
        }
    }

    void ServerInterfacePingRegions::EndAction(s32 liError)
    {
        CGS_ASSERT(meCurrentAction != E_ACTION_COUNT, "Trying to end an action when not doing anything\n");

        const DSErrorToServerInterfaceErrorTable& lrTable = KA_DS_ERROR_TABLE_LOOKUP[meCurrentAction];
        ServerInterfaceComponent::EndActionCore(
            ConvertError(liError, lrTable.mpMappingTable, lrTable.miNumMappings));
        meCurrentAction = E_ACTION_COUNT;
    }

    bool ServerInterfacePingRegions::ResolveRegion(s32 liRegion)
    {
        const char* lpcConf = static_cast<const char*>(
            LobbyApiInfo(mpServerInterface->GetLobbyAPIRef(), KI_SELECT_CONF));
        const char* lpcRegions = TagFieldFind(lpcConf, "GPS_REGIONS");

        char lacHostName[KI_MAX_HOST_NAME_LENGTH];
        if (lpcRegions == 0 ||
            TagFieldGetDelim(lpcRegions, lacHostName, KI_MAX_HOST_NAME_LENGTH, "", liRegion, ',') == 0)
        {
            return false;
        }

        CGS_ASSERT(mpHostAddress == 0, "!mpHostAddress");

        mpHostAddress = ProtoNameAsync(lacHostName, KI_NAME_RESOLVE_TIMEOUT);
        if (mpHostAddress == 0)
        {
            return false;
        }

        meState = E_STATE_RESOLVING;
        return true;
    }

    void ServerInterfacePingRegions::HandlePingResults(s32 liPing)
    {
        CGS_ASSERT(miCurrentRegion >= 0, "miCurrentRegion >= 0");
        CGS_ASSERT(miCurrentRegion < KI_MAX_PING_REGIONS, "miCurrentRegion < KI_MAX_PING_REGIONS");

        miPingRequest                   = -1;
        maiPingResults[miCurrentRegion] = liPing;
        ++miCurrentRegion;

        if (mpHostAddress != 0)
        {
            mpHostAddress->Free(mpHostAddress);
            mpHostAddress = 0;
        }

        if (!ResolveRegion(miCurrentRegion))
        {
            meState = E_STATE_COUNT;
            EndAction(0);
        }
    }

    void ServerInterfacePingRegions::PingManagerCallback(void* /*lpAddress*/, u32 luPing, void* lpUserData)
    {
        static_cast<ServerInterfacePingRegions*>(lpUserData)->HandlePingResults(static_cast<s32>(luPing));
    }
}
