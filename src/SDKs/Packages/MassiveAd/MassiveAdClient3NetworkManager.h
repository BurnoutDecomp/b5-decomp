#pragma once

// ===========================================================================
// MassiveAdClient3::CNetworkManager -- the MassiveAd client's network manager
// and transport driver (vendor middleware). Reconstructed from the X360
// ARTIST.XEX (no leak / DecFIGS). Per-function X360 addresses:
//     MassiveAdClient3::CNetworkManager::CNetworkManager            @ 0x82BD0C48
//     MassiveAdClient3::CNetworkManager::~CNetworkManager           @ 0x82BD0CE8
//     MassiveAdClient3::CNetworkManager::`scalar deleting destructor'@ 0x82BD15E8
//     MassiveAdClient3::CNetworkManager::Initialize                 @ 0x82BD1638
//     MassiveAdClient3::CNetworkManager::Shutdown                   @ 0x82BD0D80
//     MassiveAdClient3::CNetworkManager::Connect                    @ 0x82BD1CA8
//     MassiveAdClient3::CNetworkManager::Disconnect                 @ 0x82BD12F0
//     MassiveAdClient3::CNetworkManager::Send                       @ 0x82BD19F8
//     MassiveAdClient3::CNetworkManager::Receive                    @ 0x82BD1B00
//     MassiveAdClient3::CNetworkManager::Tick                       @ 0x82BD1E40
//     MassiveAdClient3::CNetworkManager::CreateHeartbeat            @ 0x82BD14F8
//     MassiveAdClient3::CNetworkManager::ResetHeartbeatWaitTime     @ 0x82BD1598
//     MassiveAdClient3::CNetworkManager::HandleError               @ 0x82BD14E8
//     MassiveAdClient3::CNetworkManager::RemoveCurrentRequest       @ 0x82BD13D0
//     MassiveAdClient3::CNetworkManager::PrepareForShutdown         @ 0x82BD1DE8
//     MassiveAdClient3::CNetworkManager::ResolveAddresses           @ 0x82BD0E80
//     MassiveAdClient3::CNetworkManager::KillAndWaitForDNS          @ 0x82BD1448
//     MassiveAdClient3::CNetworkManager::SetDNSPending              @ 0x82BD1428
//     MassiveAdClient3::CNetworkManager::SetDNSFinished             @ 0x82BD1438
//     MassiveAdClient3::CNetworkManager::GetServerAddressU32        @ 0x82BD1078
//     MassiveAdClient3::CNetworkManager::GetServerHostName          @ 0x82BD1260
//     MassiveAdClient3::CNetworkManager::SetServerAddress           @ 0x82BD0F10
//     MassiveAdClient3::CNetworkManager::SetServerPort              @ 0x82BD1110
//
// CNetworkManager is the request-owning CRequestBuilder subsystem that drives the
// single client<->server socket: it pumps the CRequestManager queue, connects to
// the resolved server, sends each request block and receives the response through
// an owned CTransactionHTTP, emits keep-alive heartbeats, and runs the DNS-resolve
// thread. It is a lazy process singleton (spNetworkManager); the server address /
// port / host-name table and the DNS/lock/refcount state are process-wide statics
// homed in the .cpp.
//
// Layout over the CRequestBuilder base (base ends at +0x28; offsets X360-relative,
// reproduced BY NAME -- the host base, pointers and embedded socket are wider):
//   +0x00  vftable            (off_82184CE8; slot 1 HandleResponse, slot 2 HandleError)
//   +0x28  mnField28/mnField2A(two zeroed halfwords; untouched after the ctor)
//   +0x2C  mnMaxBytes         (per-tick send/receive chunk size, set by DetermineMaxBytes)
//   +0x30  mpReceiveBuffer    (MassiveMalloc'd receive scratch, or 0)
//   +0x38  mnCurrentTime      (i64; this tick's system time)
//   +0x40  mnLastTime         (i64; previous tick's system time)
//   +0x48  mnIdleTimer        (i64; connected-idle elapsed accumulator)
//   +0x50  mnConnectTimer     (i64; connect-in-progress elapsed accumulator)
//   +0x58  mnReconnectWait    (i64; back-off countdown before the next connect)
//   +0x60  mnHeartbeatTimer   (i64; heartbeat elapsed accumulator)
//   +0x68  mnHeartbeatInterval(i64; 60000 at ctor)
//   +0x70  mpCurrentRequest   (in-flight CRequestObject, or 0)
//   +0x74  mpHeartbeatRequest (pending CRequestHeartbeat, or 0)
//   +0x78  mpTransaction      (owned CTransactionHTTP)
//   +0x7C  mnConnectedAddress (address of the currently-connected server)
//   +0x80  mnConnectedPort    (port of the currently-connected server, u16)
//   +0x84  mnSendOffset       (bytes of the current request already sent)
//   +0x88  mnState            (E_ConnectionState machine)
//   +0x8C  mSocket            (embedded CMassiveSocket, by value)
// ===========================================================================

#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestBuilder.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3Socket.h"

namespace MassiveAdClient3
{

class CRequestObject;
class CRequestHeartbeat;
class CTransactionHTTP;
struct SMassiveClientInit;

// MassiveInet_ntop -- the MassiveAd client's inet_ntop shim: formats the network-
// order address at pSrc into pcDst ("a.b.c.d"), writing at most nSize bytes.
// Separate ledger TU (a vendor wrapper over the platform inet_ntop); declared here
// because CNetworkManager's Connect / Disconnect / Tick format the connected /
// target address for their trace lines.
char* MassiveInet_ntop(int nFamily, const void* pSrc, char* pcDst, int nSize);

// ---------------------------------------------------------------------------
// CNetworkManager -- the client transport driver.
// ---------------------------------------------------------------------------
class CNetworkManager : public CRequestBuilder
{
public:
    // The socket connection state machine (mnState). Reconstructed from the Tick
    // dispatch + the transitions in Connect / Disconnect / Send / Receive; the
    // numeric values are attested, the names are reconstructed from their roles.
    enum E_ConnectionState
    {
        E_CONN_IDLE       = 0, // disconnected; counting down mnReconnectWait to reconnect
        E_CONN_CONNECTING = 1, // socket Connect in progress
        E_CONN_CONNECTED  = 2, // connected; awaiting / matching a request to service
        E_CONN_SENDING    = 3, // sending the current request block
        E_CONN_RECEIVING  = 4  // receiving the response
    };

    // @ 0x82BD0C48. Chains the CRequestBuilder base ("CNetworkManager"), installs
    // the network-manager vftable, zeroes the transport state, defaults the
    // heartbeat interval to 60000 ms, and constructs the embedded socket.
    CNetworkManager();

    // @ 0x82BD0CE8 (slot 0; the scalar deleting destructor @ 0x82BD15E8 is the
    // compiler-emitted thunk around it). Destroys the owned transaction, frees the
    // receive buffer, then lets the embedded socket and the CRequestBuilder base
    // destruct (compiler-emitted).
    virtual ~CNetworkManager();

    // Slot 1 (+0x04). The CRequestBuilder request-complete callback override.
    // Separate ledger TU (its body is not in this TU's slice); declared as an
    // override so the class is concrete (the ctor installs the vftable and
    // Initialize instantiates it). Body lives in the CNetworkManager response TU.
    virtual int HandleResponse(CRequestObject* pRequest);

    // @ 0x82BD14E8 (slot 2, +0x08). Clears the pending heartbeat and tail-calls the
    // base CRequestBuilder::RemoveFromRequestCollect to drop pRequest. Ignores the
    // error code (the asm uses only this + the request).
    virtual int HandleError(CRequestObject* pRequest, int nErrorCode);

    // ----- lazy process singleton -----------------------------------------

    // @ 0x82BD1638. Lazily creates the singleton, its process-wide server-array
    // lock (refcounted) and its transaction object, sizes + allocates the receive
    // buffer from the init flags, arms DNS, and -- when pResolveParam is supplied --
    // kicks off address resolution. Returns the singleton, or null on any failure
    // (unwinding what it built).
    static CNetworkManager* Initialize(const SMassiveClientInit* pInit,
                                       void* pResolveParam);

    // @ 0x82BD0D80. Destroys the singleton and the DNS thread, decrements the
    // server-array refcount, and -- when it reaches zero -- destroys the lock and
    // resets the server table.
    static void Shutdown();

    // @ 0x82BD1E40. One transport tick: advances the clocks, fires heartbeats,
    // reaps the finished DNS thread, pumps the next queued request, and runs the
    // connect/send/receive state machine.
    void Tick();

    // ----- connection / transfer -------------------------------------------

    // @ 0x82BD1CA8. Resolves the current request's server and opens the socket
    // (E_CONN_CONNECTING). Errors the request (-799) when its address is
    // unresolved, returns 0 while DNS is still pending (address -1), or -800 on a
    // socket failure.
    int Connect();

    // @ 0x82BD12F0. Closes the socket (E_CONN_IDLE) and clears the connected
    // address. Returns 0, or the -800 SetLastError result when the socket close
    // failed.
    int Disconnect();

    // @ 0x82BD19F8. Sends the next chunk (<= mnMaxBytes) of the request block at
    // pData/nTotalLength from mnSendOffset; advances to E_CONN_RECEIVING once fully
    // sent. Returns 0, or the -797 SetLastError result on a fatal socket error.
    int Send(const void* pData, int nTotalLength);

    // @ 0x82BD1B00. Receives the next chunk into the receive buffer and feeds it to
    // the transaction; retries (still have request data) or errors the request
    // otherwise. Returns 0, or the -796 SetLastError result on an unrecoverable
    // receive error.
    int Receive();

    // ----- request lifecycle ----------------------------------------------

    // @ 0x82BD13D0. Drops the in-flight request and disconnects. Returns the
    // Disconnect result. (Direct bl target of CRequestObject::Cancel.)
    int RemoveCurrentRequest();

    // @ 0x82BD1DE8. Shutdown helper: cancels + removes the in-flight request unless
    // its stored type is a keep-state (101 / 115). Returns the RemoveCurrentRequest
    // result, or the (incidental) in-flight request pointer otherwise.
    int PrepareForShutdown();

    // @ 0x82BD14F8. Creates, wires and submits a keep-alive heartbeat request when
    // none is pending.
    void CreateHeartbeat();

    // @ 0x82BD1598. Rewinds the heartbeat elapsed accumulator.
    void ResetHeartbeatWaitTime();

    // ----- DNS / server address table (process-wide statics) ---------------

    // @ 0x82BD0E80. Spawns the DNS-resolve thread on MassiveAdClient3::DNS with
    // pResolveParam, once. Returns 0 when started, -1 when a thread already exists
    // / pResolveParam is null / the thread could not be created.
    int ResolveAddresses(void* pResolveParam);

    // @ 0x82BD1448. Signals the DNS thread to stop and busy-waits (up to
    // nTimeoutMs) for it to clear the pending flag.
    static void KillAndWaitForDNS(unsigned int nTimeoutMs);

    // @ 0x82BD1428 / 0x82BD1438. Set / clear the process-wide "DNS pending" flag
    // (the DNS thread toggles it around resolution).
    static void SetDNSPending();
    static void SetDNSFinished();

    // @ 0x82BD1078. Returns the resolved address for server nIndex, or -1 while DNS
    // is pending or when the array lock could not be taken.
    static int GetServerAddressU32(unsigned int nIndex);

    // @ 0x82BD1260. Returns the host-name string for server nIndex, or the shared
    // empty sentinel when none is set / the lock could not be taken.
    static const char* GetServerHostName(unsigned int nIndex);

    // @ 0x82BD0F10 / 0x82BD1110. Install a resolved address (+ optional host name)
    // / port for server nIndex (<= 16), under the array lock.
    static void SetServerAddress(unsigned int nIndex, int nAddress, const char* pcHostName);
    static void SetServerPort(unsigned int nIndex, unsigned short nPort);

    // FLAG additive accessor (not its own X360 function): CRequestObject::Cancel
    // reads the in-flight request pointer directly at manager+0x70. Exposed BY NAME
    // as the manager's mpCurrentRequest load.
    CRequestObject* GetCurrentRequest() const { return mpCurrentRequest; }

    // ----- collaborators homed in other TUs (declared, not defined here) ----

    // The per-server port getter (mirror of GetServerAddressU32; separate ledger
    // TU). Declared here -- CNetworkManager is its home -- because Connect / Tick
    // bl into it. Returns the u16 port for server nIndex.
    static unsigned short GetServerPortU16(unsigned int nIndex);

    // Recomputes mnMaxBytes for the current tick's send/receive chunk (separate
    // ledger TU). Declared here because Tick bl's it before Send / Receive.
    void DetermineMaxBytes();

private:
    unsigned short     mnField28;           // +0x28 (0; untouched after ctor)
    unsigned short     mnField2A;           // +0x2A (0; untouched after ctor)
    int                mnMaxBytes;          // +0x2C
    void*              mpReceiveBuffer;     // +0x30
    long long          mnCurrentTime;       // +0x38
    long long          mnLastTime;          // +0x40
    long long          mnIdleTimer;         // +0x48
    long long          mnConnectTimer;      // +0x50
    long long          mnReconnectWait;     // +0x58
    long long          mnHeartbeatTimer;    // +0x60
    long long          mnHeartbeatInterval; // +0x68 (60000)
    CRequestObject*    mpCurrentRequest;    // +0x70
    CRequestHeartbeat* mpHeartbeatRequest;  // +0x74
    CTransactionHTTP*  mpTransaction;       // +0x78
    unsigned int       mnConnectedAddress;  // +0x7C
    unsigned short     mnConnectedPort;     // +0x80
    int                mnSendOffset;        // +0x84
    int                mnState;             // +0x88 (E_ConnectionState)
    CMassiveSocket     mSocket;             // +0x8C (embedded by value)
};

// spNetworkManager -- the live CNetworkManager singleton pointer (the X360 .data
// symbol name, kept verbatim; null before init / after shutdown). Defined by the
// CNetworkManager TU.
extern CNetworkManager* spNetworkManager;

} // namespace MassiveAdClient3
