// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::NetworkAdapterX360::Construct          @ 0x8287F4D8
//   CgsNetwork::NetworkAdapterX360::Release            @ 0x8287F5B8
//   CgsNetwork::NetworkAdapterX360::StartupNetworking  @ 0x8287F610
//   CgsNetwork::NetworkAdapterX360::Prepare            @ 0x8288BD90
//   CgsNetwork::NetworkAdapterX360::Update             @ 0x8288BEC0
//
// The Xbox-360 concrete network adapter. It wraps the DirtySock (DirtySDK) NetConn* API:
// NetConnStartup brings the stack up against a named environment, NetConnConnect opens the
// online connection, NetConnIdle pumps it each frame, NetConnStatus polls it, and
// NetConnDisconnect / NetConnShutdown tear it down. The base NetworkAdapter handles the
// platform-agnostic packet path; this subclass layers the X360 connection lifecycle and the
// per-server-type network-environment selection on top.

#include "GameShared/GameClasses/Network/Packeting/X360/CgsNetworkAdapterX360.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// ----------------------------------------------------------------------------------------
// DirtySock (DirtySDK) network-connection API. These are platform-SDK free functions; the
// X360 build links them from the Xbox DirtySDK. Declared here in their canonical SDK shape
// (matching the register usage in the call sites) so this TU compiles standalone; their
// bodies live in the SDK / platform shim. The integer control selectors below are the
// four-character codes DirtySDK uses ('frrt', 'xlsp', 'serv', 'conn'), recovered verbatim
// from the asm immediates.
// ----------------------------------------------------------------------------------------
extern "C"
{
    int  NetConnStartup(const char* lpcParams);
    int  NetConnControl(int liControl, int liValue, int liValue2, void* lpValue, void* lpValue2);
    int  NetConnConnect(int liData, const char* lpcParams);
    int  NetConnIdle();
    int  NetConnStatus(int liSelector, int liData, void* lpBuf, int liBufSize);
    int  NetConnDisconnect();
    int  NetConnShutdown(unsigned int luFlags);
}

// XDK signin-state query. XUSER_SIGNIN_STATE_SIGNED_IN_TO_LIVE == 2 (the value the asm
// compares against). Declared in its XDK shape; provided by the platform layer.
extern "C" int XUserGetSigninState(int liUserIndex);

// X360 online-lobby disconnect hook (Demonware/Lobby glue), reached through the server
// interface's connection component. SDK-side body.
extern "C" int LobbyApiDisconnect(int liHandle, int liReason);

namespace CgsNetwork
{

// The network-environment names selected per server type (X360 rodata off_82F335BC /
// off_82F335C0). "45410000" is the live/insecure-capable environment, "45410004" the
// secure one; both are DirtySDK environment identifiers passed via NetConnControl 'serv'.
static const char* const KPC_ENVIRONMENT_LIVE   = "45410000";
static const char* const KPC_ENVIRONMENT_SECURE = "45410004";

// DirtySDK NetConnControl four-character control selectors (asm immediates).
static const int KI_NETCONN_CTRL_FRRT = 0x66727274; // 'frrt' -- connect retry/timeout
static const int KI_NETCONN_CTRL_XLSP = 0x786C7370; // 'xlsp'
static const int KI_NETCONN_CTRL_SERV = 0x73657276; // 'serv' -- select environment
static const int KI_NETCONN_CTRL_CONN = 0x636F6E6E; // 'conn' -- connection status selector

// Connect-timeout in milliseconds (asm: NetConnControl('frrt', 5000, ...)).
static const int KI_CONNECT_TIMEOUT_MS = 5000;

// Environment selector values paired with the 'serv' control (asm immediates 0x45410006/7).
static const int KI_SERV_VALUE_6 = 0x45410006;
static const int KI_SERV_VALUE_7 = 0x45410007;

// XUserGetSigninState result for "signed in to Xbox Live" (asm cmpwi 2).
static const int KI_SIGNIN_STATE_LIVE = 2;

// NetConnStatus high-byte sentinel meaning "the connection is down" ('-', asm cmpwi 0x2D on
// the sign-extended top byte of the status word).
static const int KI_NETCONN_STATUS_DOWN_HIGH_BYTE = 0x2D;

// Specific NetConnStatus value that means "disconnected because of a duplicate login"
// ('-dup', asm subtracts the status from 0x2D647570 and tests for zero).
static const int KI_NETCONN_STATUS_DUPLICATE_LOGIN = 0x2D647570;

// ----------------------------------------------------------------------------------------
// Construct @ 0x8287F4D8
// ----------------------------------------------------------------------------------------
void NetworkAdapterX360::Construct(s32 liParentPerfMon)
{
    mePrepareState = E_PREPARE_INIT;   // +0x2C = 0
    mbConnecting   = false;            // +0x30 = 0
    mpEnvironment  = nullptr;          // +0x28 = 0

    miIdlePerfMon       = CgsDev::PerfMonCpu::AddMonitor("AdaptorX360 - Idle",       8, 0, 1.0, liParentPerfMon, 1);
    miConnectPerfMon    = CgsDev::PerfMonCpu::AddMonitor("AdaptorX360 - Connect",    8, 0, 1.0, liParentPerfMon, 1);
    miDisconnectPerfMon = CgsDev::PerfMonCpu::AddMonitor("AdaptorX360 - Disconnect", 8, 0, 1.0, liParentPerfMon, 1);
    miCallBasePerfMon   = CgsDev::PerfMonCpu::AddMonitor("AdaptorX360 - CallBase",   8, 0, 1.0, liParentPerfMon, 1);

    NetworkAdapterBase::Construct();
}

// ----------------------------------------------------------------------------------------
// Prepare @ 0x8288BD90
// ----------------------------------------------------------------------------------------
NetworkAdapterBase::ENetworkStatus NetworkAdapterX360::Prepare(NetworkAdapterPrepareParams* lpParams)
{
    if (mePrepareState == E_PREPARE_INIT || mePrepareState == E_PREPARING_NETADAPTER)
    {
        mePrepareState = E_PREPARING_NETADAPTER;

        // Run the base prepare first; while it is still busy, stay in PREPARING and report busy.
        if (NetworkAdapterBase::Prepare(lpParams) == E_NET_STATUS_BUSY)
        {
            return E_NET_STATUS_BUSY;
        }

        // Base is up: bring the X360 network stack up, then fall into the shared tail.
        StartupNetworking();
    }
    else if (mePrepareState != E_FULLY_PREPARED)
    {
        // state >= 3 (asm cmplwi 3 / bge): unexpected -> assert + report ready, no re-arm.
        CGS_ASSERT(false, "CgsNetwork::NetworkAdapterX360::Prepare: Unknown prepare state");
        return E_NET_STATUS_READY;
    }

    // Shared tail (asm loc_8288BE8C, reached for states INIT/PREPARING after StartupNetworking AND
    // for FULLY_PREPARED): re-arm the connect timeout, clear the connecting flag, settle as prepared.
    // Calling Prepare again while already FULLY_PREPARED re-arms 'frrt' and clears mbConnecting.
    NetConnControl(KI_NETCONN_CTRL_FRRT, KI_CONNECT_TIMEOUT_MS, 0, nullptr, nullptr);
    mbConnecting   = false;
    mePrepareState = E_FULLY_PREPARED;
    return E_NET_STATUS_READY;
}

// ----------------------------------------------------------------------------------------
// Release @ 0x8287F5B8
// ----------------------------------------------------------------------------------------
bool NetworkAdapterX360::Release()
{
    if (mePrepareState != E_PREPARE_INIT)
    {
        NetConnDisconnect();
        NetConnShutdown(0);
    }

    mpEnvironment  = nullptr;        // +0x28
    mbConnecting   = false;          // +0x30
    mePrepareState = E_PREPARE_INIT; // +0x2C

    return NetworkAdapterBase::Release();
}

// ----------------------------------------------------------------------------------------
// StartupNetworking @ 0x8287F610
// ----------------------------------------------------------------------------------------
void NetworkAdapterX360::StartupNetworking()
{
    const char* lpcEnvironment = nullptr;
    const char* lpcFlags       = nullptr;
    bool        lbStart        = true;

    switch (meServerType)
    {
        case E_SERVER_TYPE_LOCAL:
        case E_SERVER_TYPE_ARTIST:
        case E_SERVER_TYPE_DEMO_1:
            lpcEnvironment = KPC_ENVIRONMENT_LIVE;
            lpcFlags       = "-nosecure";
            break;

        case E_SERVER_TYPE_TEST:
            lpcEnvironment = KPC_ENVIRONMENT_LIVE;
            lpcFlags       = "";
            break;

        case E_SERVER_TYPE_DEV:
        case E_SERVER_TYPE_JUICE:
        case E_SERVER_TYPE_DEMO_2:
            lpcEnvironment = KPC_ENVIRONMENT_SECURE;
            lpcFlags       = "";
            break;

        case E_SERVER_TYPE_COUNT:
            CGS_ASSERT(false, "Unknown server type");
            lpcEnvironment = KPC_ENVIRONMENT_SECURE;
            lpcFlags       = "";
            break;

        default:
            lbStart = false;
            break;
    }

    if (lbStart)
    {
        mpEnvironment = const_cast<char*>(lpcEnvironment);
        NetConnStartup(lpcFlags);
    }

    NetConnControl(KI_NETCONN_CTRL_XLSP, 0, 0, nullptr, nullptr);
    NetConnControl(KI_NETCONN_CTRL_SERV, KI_SERV_VALUE_6, 0, nullptr, nullptr);
    NetConnControl(KI_NETCONN_CTRL_SERV, KI_SERV_VALUE_7, 0, nullptr, nullptr);
}

// ----------------------------------------------------------------------------------------
// Update @ 0x8288BEC0
// ----------------------------------------------------------------------------------------
void NetworkAdapterX360::Update()
{
    CgsDev::PerfMonCpu::StartMonitor(miIdlePerfMon);
    NetConnIdle();
    CgsDev::PerfMonCpu::StopMonitor(miIdlePerfMon);

    // --- connect-when-ready -------------------------------------------------------------
    CgsDev::PerfMonCpu::StartMonitor(miConnectPerfMon);
    if (mePrepareState == E_FULLY_PREPARED && !mbConnecting)
    {
        // mpNetworkManager's active-user index lives at +0x60 (asm lwz r3,0x60(r11)); the
        // NetworkManager layout is owned by its own home, so the attested offset is read here.
        const s32 liUserIndex = *reinterpret_cast<s32*>(static_cast<u8*>(mpNetworkManager) + 0x60);
        if (liUserIndex > -1 && XUserGetSigninState(liUserIndex) == KI_SIGNIN_STATE_LIVE)
        {
            CGS_ASSERT(mpEnvironment != nullptr, "Environment to use not set up");

            const int liConnectResult = NetConnConnect(0, static_cast<const char*>(mpEnvironment));
            mbConnecting = (liConnectResult == 1);
            if (liConnectResult == 1)
            {
                mbDuplicateLogin = false;
            }
        }
    }
    CgsDev::PerfMonCpu::StopMonitor(miConnectPerfMon);

    // --- poll for disconnect ------------------------------------------------------------
    CgsDev::PerfMonCpu::StartMonitor(miDisconnectPerfMon);
    if (mbConnecting)
    {
        const int liStatus = NetConnStatus(KI_NETCONN_CTRL_CONN, 0, nullptr, 0);
        if (((liStatus >> 24) & 0xFF) == KI_NETCONN_STATUS_DOWN_HIGH_BYTE)
        {
            mbConnecting = false;
            mbDuplicateLogin = (liStatus == KI_NETCONN_STATUS_DUPLICATE_LOGIN);
            NetConnDisconnect();

            CGS_ASSERT(mpServerInterface != nullptr, "mpServerInterface");
            CGS_ASSERT(*reinterpret_cast<void**>(static_cast<u8*>(mpServerInterface) + 4) != nullptr,
                       "mpServerInterface->GetConnectionComponent()");

            // mpServerInterface->GetConnectionComponent()->...->mLobbyHandle (asm walks
            // +0x04 -> +0x10 -> +0x78). Reconstructed as the attested pointer walk; the
            // intermediate types belong to the ServerInterface / connection-component homes.
            u8* lpServerInterface  = static_cast<u8*>(mpServerInterface);
            u8* lpConnComponent    = *reinterpret_cast<u8**>(lpServerInterface + 4);
            u8* lpConnInner        = *reinterpret_cast<u8**>(lpConnComponent + 16);
            int liLobbyHandle      = *reinterpret_cast<int*>(lpConnInner + 120);
            LobbyApiDisconnect(liLobbyHandle, 0);
        }
    }
    CgsDev::PerfMonCpu::StopMonitor(miDisconnectPerfMon);

    // --- the base-update timing bracket (empty body besides the perfmon pair) -----------
    CgsDev::PerfMonCpu::StartMonitor(miCallBasePerfMon);
    CgsDev::PerfMonCpu::StopMonitor(miCallBasePerfMon);
}

}
