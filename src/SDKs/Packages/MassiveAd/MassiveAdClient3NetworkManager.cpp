#include "SDKs/Packages/MassiveAd/MassiveAdClient3NetworkManager.h"

#include <new>       // placement new (raw heap-hook allocation + explicit construction)
#include <cstring>   // std::memset / std::strlen / std::strncpy
#include <cstdlib>   // std::srand / std::rand
#include <cstdint>   // std::intptr_t

#include "SDKs/Packages/MassiveAd/MassiveAdClient3Request.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestManager.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestHeartbeat.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3Transaction.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3Objects.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestLocateService.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ClientCore.h"

// ===========================================================================
// MassiveAdClient3::CNetworkManager -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// SHAPE and BODIES both from the X360 asm (no leak / DecFIGS). The transport
// driver embeds a CMassiveSocket by value (+0x8C), owns a CTransactionHTTP
// (+0x78), and drives the CRequestManager queue through the state machine in Tick.
// See MassiveAdClient3NetworkManager.h for the layout map. The vftable
// (off_82184CE8) is modelled by the virtual functions, so no vftable store is
// written by hand.
//
// The `STUB(level, name, format, ...)` placeholder in the pseudocode is the
// MassiveAd verbose-logging sink MassiveLog(level, name, format, ...); every trace
// string below is verbatim X360 rodata. The name argument `*(spNetworkManager+3)`
// is the singleton's base name (spNetworkManager->GetName()).
// ===========================================================================

namespace MassiveAdClient3
{

// spNetworkManager -- the live singleton pointer (null before init / after shutdown).
CNetworkManager* spNetworkManager = 0;

namespace
{

// ---------------------------------------------------------------------------
// Process-wide CNetworkManager .data state (touched only through the manager's
// static methods, so it is file-private here).
//   dword_82F91AD8  gbDNSPending          -- 1 while the DNS thread is resolving
//   dword_82F91ADC  gnReceiveBufferSize   -- receive scratch size (also socket recv)
//   dword_82F91AE0  gnSendBufferSize      -- socket send buffer size
//   dword_8327F374  gbDNSKillSignal       -- 1 asks the DNS thread to stop
//   dword_8327F37C  gnServerArrayRefCount -- server-array lock refcount
//   off_8327F370    gpDNSThread           -- the DNS-resolve thread object, or 0
//   off_8327F378    gpServerArrayLock     -- guards gaServers, or 0
// ---------------------------------------------------------------------------
int                      gbDNSPending          = 1;
int                      gnReceiveBufferSize    = 0x2000;
int                      gnSendBufferSize       = 0x2000;
int                      gbDNSKillSignal        = 0;
int                      gnServerArrayRefCount  = 0;
CMassiveThread*          gpDNSThread            = 0;
CMassiveCriticalSection* gpServerArrayLock      = 0;

// The server address / port / host-name table (dword_82F91AE8, a 3-dword stride:
// address @ +0x00, port @ +0x04 stored as a full dword, host-name pointer @ +0x08).
// Index range 0..16 (the setters bound-check `index <= 0x10`).
struct SServerEntry
{
    unsigned int mnAddress;   // dword_82F91AE8
    unsigned int mnPort;      // dword_82F91AEC (u16 value in a dword slot)
    char*        mpcHostName; // dword_82F91AF0
};
// Every entry starts with no address, port 1000 and no host name (the image holds
// sixteen initialised entries; the setters also accept index 16).
SServerEntry gaServers[17] = {
    { 0, 1000, 0 }, { 0, 1000, 0 }, { 0, 1000, 0 }, { 0, 1000, 0 }, { 0, 1000, 0 }, { 0, 1000, 0 },
    { 0, 1000, 0 }, { 0, 1000, 0 }, { 0, 1000, 0 }, { 0, 1000, 0 }, { 0, 1000, 0 }, { 0, 1000, 0 },
    { 0, 1000, 0 }, { 0, 1000, 0 }, { 0, 1000, 0 }, { 0, 1000, 0 }, { 0, 1000, 0 }
};

// &unk_820046A7 -- a shared client rodata byte the X360 passes both as an ignored
// SetLastError format placeholder and as GetServerHostName's empty-host-name
// sentinel. Modelled as an empty string (the SetLastError leaf ignores the format;
// the host-name path returns a valid, non-null empty pointer).
const char gcEmptyRodata820046A7[] = "";

} // anonymous namespace

// ---------------------------------------------------------------------------
// CNetworkManager::CNetworkManager @ 0x82BD0C48
//
//   CRequestBuilder::CRequestBuilder(this, "CNetworkManager");
//   vftable = off_82184CE8;
//   +0x28..+0x60 = 0; +0x68 (heartbeat interval) = 60000;
//   +0x70..+0x88 = 0; CMassiveSocket::CMassiveSocket(this+0x8C);
// ---------------------------------------------------------------------------
CNetworkManager::CNetworkManager()
    : CRequestBuilder("CNetworkManager")  // bl CRequestBuilder::CRequestBuilder
    , mnField28(0)
    , mnField2A(0)
    , mnMaxBytes(0)
    , mpReceiveBuffer(0)
    , mnCurrentTime(0)
    , mnLastTime(0)
    , mnIdleTimer(0)
    , mnConnectTimer(0)
    , mnReconnectWait(0)
    , mnHeartbeatTimer(0)
    , mnHeartbeatInterval(60000)          // std 0xEA60, 0x68
    , mpCurrentRequest(0)
    , mpHeartbeatRequest(0)
    , mpTransaction(0)
    , mnConnectedAddress(0)
    , mnConnectedPort(0)
    , mnSendOffset(0)
    , mnState(E_CONN_IDLE)
    , mSocket()                           // bl CMassiveSocket::CMassiveSocket @ +0x8C
{
}

// ---------------------------------------------------------------------------
// CNetworkManager::~CNetworkManager @ 0x82BD0CE8
//
// Rewrites the vftable (compiler-emitted for the virtual dtor), destroys the owned
// transaction through the heap hook, frees the receive buffer, then lets the
// embedded socket and the CRequestBuilder base destruct (both compiler-emitted).
// ---------------------------------------------------------------------------
CNetworkManager::~CNetworkManager()
{
    if (mpTransaction)                     // lwz 0x78; cmplwi; beq
    {
        delete mpTransaction;              // (**(txn+0xC))(txn+0xC, 1) -- heap-hook teardown
        mpTransaction = 0;                 // stw 0, 0x78
    }
    if (mpReceiveBuffer)                   // lwz 0x30; cmplwi; beq
    {
        MassiveFree(mpReceiveBuffer);      // off_82F91C18(buffer)
        mpReceiveBuffer = 0;               // stw 0, 0x30
    }
    // mSocket + CRequestBuilder base destruct: compiler-emitted (bl ~CMassiveSocket
    // @ +0x8C then bl ~CRequestBuilder).
}

// ---------------------------------------------------------------------------
// CNetworkManager::`scalar deleting destructor' @ 0x82BD15E8
//
// The compiler-emitted vftable slot-0 thunk (run ~CNetworkManager, then free the
// object through CMassiveBaseObject::operator delete when the low bit is set). It
// is synthesised by the compiler from the virtual destructor above -- not written
// by hand -- and is what `delete spNetworkManager` (Initialize / Shutdown) invokes.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// CNetworkManager::HandleResponse (slot 1)
//
// Same body as HandleError (the console folds both slots onto one function): drop
// the pending heartbeat and remove the finished request from the collection.
// ---------------------------------------------------------------------------
int CNetworkManager::HandleResponse(CRequestObject* pRequest)
{
    mpHeartbeatRequest = 0;
    return RemoveFromRequestCollect(pRequest);
}

// ---------------------------------------------------------------------------
// CNetworkManager::HandleError @ 0x82BD14E8 (slot 2)
//
// Clear the pending heartbeat, then tail-call the base RemoveFromRequestCollect to
// drop the failed request. The error code is unused (the asm passes only this +
// the request into RemoveFromRequestCollect).
// ---------------------------------------------------------------------------
int CNetworkManager::HandleError(CRequestObject* pRequest, int /*nErrorCode*/)
{
    mpHeartbeatRequest = 0;                                 // stw 0, 0x74
    return RemoveFromRequestCollect(pRequest);             // b ...RemoveFromRequestCollect
}

// ---------------------------------------------------------------------------
// CNetworkManager::Initialize @ 0x82BD1638
//
// Lazy singleton bring-up: construct the manager, its refcounted server-array lock
// and its transaction, size + allocate the receive buffer from the init flags, arm
// DNS, and (optionally) resolve addresses. Every failure unwinds what it built.
// ---------------------------------------------------------------------------
CNetworkManager* CNetworkManager::Initialize(const CFlag* pFlags,
                                             void* pResolveParam)
{
    if (spNetworkManager)                                  // lwz off_...; bne
    {
        MassiveLog(2, spNetworkManager->GetName(), "NetworkManager was already initialized");
        return spNetworkManager;
    }

    void* lpMemory = CMassiveListNode::operator new(sizeof(CNetworkManager));
    spNetworkManager = lpMemory ? ::new (lpMemory) CNetworkManager() : 0;
    if (!spNetworkManager)
    {
        MassiveLog(1, "CNetworkManager", "Could not create a new instance of CNetworkManager");
        return 0;
    }

    if (!gpServerArrayLock)                                // lwz off_8327F378; bne
    {
        MassiveLog(5, "CNetworkManager", "Lock for Server Address Array does not exist, creating it...");
        void* lpLockMemory = CMassiveListNode::operator new(sizeof(CMassiveCriticalSection));
        gpServerArrayLock = lpLockMemory
                                ? ::new (lpLockMemory) CMassiveCriticalSection("ServerAddressArray")
                                : 0;
        if (!gpServerArrayLock)
        {
            MassiveLog(1, "CNetworkManager", "Could not create Server Array lock.");
            if (spNetworkManager)                          // lwz off_...; beq (skip)
                delete spNetworkManager;
            spNetworkManager = 0;
            return 0;
        }
    }

    ++gnServerArrayRefCount;                               // addi +1; stw dword_8327F37C
    MassiveLog(5, "CNetworkManager",
               "Incrementing Server Array references. It now has %d references.",
               gnServerArrayRefCount);

    void* lpTxMemory = CMassiveListNode::operator new(sizeof(CTransactionHTTP));
    spNetworkManager->mpTransaction = lpTxMemory ? ::new (lpTxMemory) CTransactionHTTP() : 0;
    if (!spNetworkManager->mpTransaction)                  // lwz 0x78; bne
    {
        MassiveLog(1, spNetworkManager->GetName(), "Could not create transaction object");
        delete spNetworkManager;                           // (**spNM)(spNM, 1)
        spNetworkManager = 0;
        return 0;
    }
    MassiveLog(5, spNetworkManager->GetName(), "Successfully created transaction object");

    // Buffer size from the client flags (the title passes 0, giving the 0x2000
    // default). 0x20 -> 16 KiB, 0x40 -> 4 KiB, else 8 KiB.
    int lnBufferSize;
    int lnFlags = pFlags->mnFlags;
    if (lnFlags & 0x20)                                    // rlwinm. bit 0x20
        lnBufferSize = 0x4000;
    else if (lnFlags & 0x40)                               // rlwinm. bit 0x40
        lnBufferSize = 0x1000;
    else
        lnBufferSize = 0x2000;
    gnSendBufferSize = lnBufferSize;                       // stw dword_82F91AE0
    gnReceiveBufferSize = lnBufferSize;                    // stw dword_82F91ADC

    spNetworkManager->mpReceiveBuffer = MassiveMalloc(gnReceiveBufferSize); // stw 0x30
    if (!spNetworkManager->mpReceiveBuffer)                // lwz 0x30; bne
    {
        MassiveLog(1, spNetworkManager->GetName(), "Could not allocate receive buffer");
        gnReceiveBufferSize = 0;                           // stw 0, dword_82F91ADC
        MassiveLog(1, spNetworkManager->GetName(), "NetworkManager intialization failed");
        delete spNetworkManager;
        spNetworkManager = 0;
        return 0;
    }
    MassiveLog(5, spNetworkManager->GetName(),
               "Successfully allocated receive buffer(%d bytes)", gnReceiveBufferSize);

    gbDNSKillSignal = 0;                                   // stw 0, dword_8327F374
    gbDNSPending = 1;                                      // stw 1, dword_82F91AD8
    if (pResolveParam)                                     // cmplwi r27, 0; beq (skip)
        spNetworkManager->ResolveAddresses(pResolveParam);
    MassiveLog(5, spNetworkManager->GetName(), "NetworkManager successfully intialized");
    return spNetworkManager;
}

// ---------------------------------------------------------------------------
// CNetworkManager::Shutdown @ 0x82BD0D80
//
// Destroy the singleton and the DNS thread, decrement the server-array refcount,
// and -- when it reaches zero -- destroy the lock and reset the server table.
// ---------------------------------------------------------------------------
void CNetworkManager::Shutdown()
{
    if (!spNetworkManager)                                 // lwz off_...; beq (done)
        return;

    delete spNetworkManager;                               // (**spNM)(spNM, 1)
    spNetworkManager = 0;
    // X360 @ 0x82BD0DBC then emits a MassiveLog whose arguments are leftover
    // registers -- the asm sets up no format string (unlike every other MassiveLog
    // in this TU) and the pseudocode shows STUB(v0, v2, v1) with v1/v2
    // uninitialised. A debug-only trace with unrecoverable arguments is not
    // reconstructed here rather than fabricating a format string; all of Shutdown's
    // program-state side effects below are reproduced.

    if (gpDNSThread)                                       // lwz off_8327F370; beq
    {
        delete gpDNSThread;                                // (**thread)(thread, 1)
        gpDNSThread = 0;
    }

    if (--gnServerArrayRefCount == 0)                      // addic. -1; stw; bne (done)
    {
        if (gpServerArrayLock)                             // lwz off_8327F378; beq
        {
            delete gpServerArrayLock;                      // (**lock)(lock, 1)
            gpServerArrayLock = 0;
        }
        for (int i = 0; i < 16; ++i)                       // li 0x10; addic. -1; bne (16 entries)
        {
            char* lpcHostName = gaServers[i].mpcHostName;  // lwz 0(host)
            gaServers[i].mnAddress = 0;                    // stw 0, -8(host) (address)
            gaServers[i].mnPort = 1000;                    // stw 0x3E8, -4(host) (port)
            if (lpcHostName)                               // cmplwi; beq
            {
                MassiveFree(lpcHostName);                  // off_82F91C18(host)
                gaServers[i].mpcHostName = 0;              // stw 0, 0(host)
            }
        }
    }
}

// ---------------------------------------------------------------------------
// CNetworkManager::Connect @ 0x82BD1CA8
//
// Resolve the current request's server and open the socket. Error the request
// (-799) when its address is unresolved (0), no-op (return 0) while DNS is still
// pending (address -1), else connect and enter E_CONN_CONNECTING (-800 on failure).
// ---------------------------------------------------------------------------
int CNetworkManager::Connect()
{
    int lnAddress = GetServerAddressU32(mpCurrentRequest->GetServerIndex());
    unsigned short lnPort = GetServerPortU16(mpCurrentRequest->GetServerIndex());

    if (!lnAddress)
    {
        MassiveLog(1, spNetworkManager->GetName(), "Server address is 0.0.0.0 for current request.");
        MassiveLog(1, spNetworkManager->GetName(),
                   "DNS must have failed for current request or we have not Located Services yet. Cancelling it.");
        mpCurrentRequest->Error(2);
        mpCurrentRequest = 0;
        return SetLastError(-799, gcEmptyRodata820046A7);
    }

    if (lnAddress == -1)                                   // cmpwi -1; DNS still resolving
        return 0;

    int lacAddr[4];
    lacAddr[0] = lnAddress;
    char lacAddrStr[48];
    MassiveInet_ntop(2, lacAddr, lacAddrStr, 16);
    MassiveLog(5, spNetworkManager->GetName(), "Connecting to %s on Port %d...", lacAddrStr, lnPort);

    unsigned int lnConnectAddress =
        static_cast<unsigned int>(GetServerAddressU32(mpCurrentRequest->GetServerIndex()));
    if (!mSocket.Connect(lnConnectAddress, lnPort, gnSendBufferSize, gnReceiveBufferSize))
    {
        Disconnect();
        return SetLastError(-800, gcEmptyRodata820046A7);
    }

    mnState = E_CONN_CONNECTING;                           // stw 1, 0x88
    return 0;
}

// ---------------------------------------------------------------------------
// CNetworkManager::Disconnect @ 0x82BD12F0
//
// Close the socket (E_CONN_IDLE) and clear the connected address. Returns 0, or the
// -800 SetLastError result when the socket close failed.
// ---------------------------------------------------------------------------
int CNetworkManager::Disconnect()
{
    if (!mnState)                                          // lwz 0x88; beq -> return 0
        return 0;

    int lacAddr[4];
    lacAddr[0] = static_cast<int>(mnConnectedAddress);     // lwz 0x7C
    char lacAddrStr[24];
    MassiveInet_ntop(2, lacAddr, lacAddrStr, 16);
    MassiveLog(5, spNetworkManager->GetName(), "Disconnecting from %s...", lacAddrStr);

    if (mSocket.Disconnect())
    {
        mnState = E_CONN_IDLE;                             // stw 0, 0x88
        mnConnectedAddress = 0;                            // stw 0, 0x7C
        return 0;
    }

    MassiveLog(2, spNetworkManager->GetName(), "Failed to disconnected from %s", lacAddrStr);
    mnState = E_CONN_IDLE;
    mnConnectedAddress = 0;
    return SetLastError(-800, gcEmptyRodata820046A7);
}

// ---------------------------------------------------------------------------
// CNetworkManager::Send @ 0x82BD19F8
//
// Send the next <= mnMaxBytes chunk of the request block (pData/nTotalLength) from
// mnSendOffset. Advance to E_CONN_RECEIVING once fully sent. Returns 0, or the -797
// SetLastError result on a fatal socket error.
// ---------------------------------------------------------------------------
int CNetworkManager::Send(const void* pData, int nTotalLength)
{
    int lnRemaining = nTotalLength - mnSendOffset;         // subf r30 = a3 - mnSendOffset
    mpCurrentRequest->SetStatus(0x200);                   // request status -> sending

    if (lnRemaining == 0)
    {
        mnState = E_CONN_RECEIVING;                        // stw 4, 0x88
        return 0;
    }

    int lnCount = mnMaxBytes;                              // lwz 0x2C
    if (!lnCount)
        return 0;
    if (lnCount >= lnRemaining)                            // cmpw; blt keeps max, else clamp
        lnCount = lnRemaining;

    int lnSent = mSocket.Send(static_cast<const char*>(pData) + mnSendOffset, lnCount);
    if (lnSent == -1 && mSocket.ProcessError())
    {
        MassiveLog(1, spNetworkManager->GetName(),
                   "Fatal network error while sending.  Will try to send again.", -1);
        mnSendOffset = 0;                                  // stw 0, 0x84
        Disconnect();
        return SetLastError(-797, gcEmptyRodata820046A7);
    }

    mnSendOffset += lnSent;                                // stw 0x84
    MassiveLog(5, spNetworkManager->GetName(), "Sent %d bytes", lnSent);
    return 0;
}

// ---------------------------------------------------------------------------
// CNetworkManager::Receive @ 0x82BD1B00
//
// Receive the next chunk into the receive buffer and feed it to the transaction.
// On a socket error, retry (still have request data, status 0x200) or fail the
// request (can't retry). Returns 0, or the -796 SetLastError result on an
// unrecoverable receive error.
// ---------------------------------------------------------------------------
int CNetworkManager::Receive()
{
    std::memset(mpReceiveBuffer, 0, gnReceiveBufferSize);  // memset(buffer, 0, recvSize)

    int lnCount = mnMaxBytes;                              // lwz 0x2C
    if (!lnCount)
        return 0;
    if (lnCount > gnReceiveBufferSize)                    // clamp to receive scratch
        lnCount = gnReceiveBufferSize;

    int lnReceived = mSocket.Receive(mpReceiveBuffer, lnCount);
    if (lnReceived == -1)
    {
        if (!mSocket.ProcessError())                      // transient -> return 0
            return 0;

        if (mpCurrentRequest->GetStatus() != 0x200)       // lwz 0x18(req); not sending -> can't retry
        {
            MassiveLog(1, spNetworkManager->GetName(),
                       "Error while receiving.  Can not retry request since we don't have original request data.");
            mpCurrentRequest->Error(2);
            mpCurrentRequest = 0;                          // stw 0, 0x70
            Disconnect();
            return SetLastError(-796, gcEmptyRodata820046A7);
        }

        // status == 0x200: still have the request data -> resend
        MassiveLog(2, spNetworkManager->GetName(),
                   "Error while receiving.  Still have original request data, will try resending the request..");
        mnSendOffset = 0;                                  // stw 0, 0x84
        Disconnect();
        return SetLastError(-796, gcEmptyRodata820046A7);
    }

    MassiveLog(5, spNetworkManager->GetName(), "Received %d bytes", lnReceived);

    if (mpCurrentRequest->GetStatus() != 0x300)           // lwz 0x18(req); not yet receiving
    {
        if (lnReceived == 0)                              // 0 bytes -> same resend path
        {
            MassiveLog(2, spNetworkManager->GetName(),
                       "Error while receiving.  Still have original request data, will try resending the request..");
            mnSendOffset = 0;
            Disconnect();
            return SetLastError(-796, gcEmptyRodata820046A7);
        }
        mpCurrentRequest->SetStatus(0x300);               // request status -> receiving
        mnSendOffset = 0;                                  // stw 0, 0x84
        mpCurrentRequest->ResetDataBuffer();
    }

    if (mpTransaction->ProcessResponse(mpReceiveBuffer, lnReceived))
    {
        mpCurrentRequest = 0;                              // stw 0, 0x70
        mnSendOffset = 0;                                  // stw 0, 0x84
        mnState = E_CONN_CONNECTED;                        // stw 2, 0x88
    }

    if (lnReceived == 0)
        Disconnect();
    return 0;
}

// ---------------------------------------------------------------------------
// CNetworkManager::Tick @ 0x82BD1E40
//
// One transport tick: advance the clocks, fire heartbeats, reap the finished DNS
// thread, pump the next queued request, then run the connection state machine.
// ---------------------------------------------------------------------------
void CNetworkManager::Tick()
{
    ClearLastError();                                      // stw 0, 0x04 (base last-error)

    // Advance the tick clock: mnLastTime <- previous now, mnCurrentTime <- now. The
    // first tick (mnLastTime == 0) seeds both from a single system-time reading.
    long long lnNow;
    if (mnLastTime == 0)
    {
        CMassiveSystem::Instance();
        lnNow = CMassiveSystem::GetSystemTime();
        mnLastTime = lnNow;
    }
    else
    {
        mnLastTime = mnCurrentTime;
        CMassiveSystem::Instance();
        lnNow = CMassiveSystem::GetSystemTime();
    }
    mnCurrentTime = lnNow;

    // Heartbeat: while a session is live and the interval is armed, accumulate the
    // elapsed time and emit a heartbeat once it exceeds the interval.
    if (gnMassiveSessionID)                               // dword_8327F2CC
    {
        if (mnHeartbeatInterval > 0)
        {
            mnHeartbeatTimer += (mnCurrentTime - mnLastTime);
            if (mnHeartbeatTimer > mnHeartbeatInterval && !mpHeartbeatRequest)
            {
                MassiveLog(5, spNetworkManager->GetName(), "Heartbeat wait timer has expired.");
                CreateHeartbeat();
            }
        }
    }

    // Reap the DNS thread once it has finished resolving.
    if (gpDNSThread && !gbDNSPending)                     // off_8327F370 && !dword_82F91AD8
    {
        delete gpDNSThread;                               // (**thread)(thread, 1)
        gpDNSThread = 0;
    }

    // Pump the next queued request whenever idle or the current one has completed.
    if (!mpCurrentRequest || mpCurrentRequest->IsComplete())
    {
        mnSendOffset = 0;                                 // stw 0, 0x84
        mpCurrentRequest = spRequestManager->GetNextRequest();
        if (mpCurrentRequest)
            mpTransaction->SetRequest(mpCurrentRequest);
    }

    // Connection state machine.
    switch (mnState)
    {
    case E_CONN_IDLE: // 0 -- count the back-off down, then reconnect once due
    {
        mnReconnectWait += (mnLastTime - mnCurrentTime);  // subtract elapsed
        if (mpCurrentRequest && mnReconnectWait <= 0)
        {
            Connect();
            mnConnectTimer = 0;                           // std 0, 0x50
        }
        break;
    }

    case E_CONN_CONNECTING: // 1 -- wait for the handshake, then move to sending
    {
        mnConnectTimer += (mnCurrentTime - mnLastTime);
        if (mSocket.IsConnected())
        {
            unsigned int lnPeerAddress = mSocket.GetPeerAddress();  // socket+0x18
            mnConnectedPort = mSocket.GetPeerPort();                // socket+0x1C -> 0x80
            mnConnectedAddress = lnPeerAddress;                     // stw 0x7C

            int lacAddr[4];
            lacAddr[0] = static_cast<int>(lnPeerAddress);
            char lacAddrStr[48];
            MassiveInet_ntop(2, lacAddr, lacAddrStr, 16);
            mnState = E_CONN_SENDING;                     // stw 3, 0x88
        }
        else if (mnConnectTimer > 0x7530)                 // 30000 ms
        {
            MassiveLog(5, spNetworkManager->GetName(),
                       "Connection attempt has exceeded timeout of %d ms", 30000);
            Disconnect();
            std::srand(static_cast<unsigned int>(mnCurrentTime));
            mnReconnectWait = std::rand() % 30000;        // std 0x58
            MassiveLog(5, spNetworkManager->GetName(),
                       "Waiting %d ms before next connection attempt",
                       static_cast<int>(mnReconnectWait));
        }
        break;
    }

    case E_CONN_CONNECTED: // 2 -- idle-timeout, or match the request's server
    {
        if (!mpCurrentRequest)
        {
            mnIdleTimer += (mnCurrentTime - mnLastTime);
            if (mnIdleTimer > 120000)                     // 0x1D4C0
            {
                MassiveLog(5, spNetworkManager->GetName(),
                           "Idle timeout of %d ms has been exceeded.  Disconnecting...", 120000);
                Disconnect();
            }
        }
        else if (GetServerAddressU32(mpCurrentRequest->GetServerIndex()) == -1)
        {
            // address still unresolved: wait
        }
        else
        {
            unsigned int lnRequestAddress =
                static_cast<unsigned int>(GetServerAddressU32(mpCurrentRequest->GetServerIndex()));
            bool lbSameServer = (mnConnectedAddress == lnRequestAddress);
            if (lbSameServer)
            {
                unsigned short lnRequestPort = GetServerPortU16(mpCurrentRequest->GetServerIndex());
                lbSameServer = (mnConnectedPort == lnRequestPort);
            }

            if (lbSameServer)
            {
                mnState = E_CONN_SENDING;                 // stw 3, 0x88
            }
            else
            {
                MassiveLog(5, spNetworkManager->GetName(),
                           "New request uses a different server.  Need to disconnect from current server.");
                Disconnect();
            }
        }
        break;
    }

    case E_CONN_SENDING: // 3 -- frame + send the request block
    {
        if (mpCurrentRequest)
        {
            mnIdleTimer = 0;                              // std 0, 0x48
            mpTransaction->ProcessRequest();
            DetermineMaxBytes();
            Send(mpCurrentRequest->GetDataBuffer(), mpCurrentRequest->GetDataLength());

            int lnType = mpCurrentRequest->GetServerType(); // lwz 0x14(req)
            if (lnType != 84 && lnType != 17)
            {
                MassiveLog(5, spNetworkManager->GetName(),
                           "Resetting Heartbeat wait timer since current request is a Hearbeat Invalidator...");
                ResetHeartbeatWaitTime();
            }
        }
        break;
    }

    case E_CONN_RECEIVING: // 4 -- receive the response
    {
        if (mpCurrentRequest)
        {
            mnIdleTimer = 0;                              // std 0, 0x48
            DetermineMaxBytes();
            Receive();
        }
        break;
    }

    default: // >= 5 -- no-op
        break;
    }
}

// ---------------------------------------------------------------------------
// CNetworkManager::CreateHeartbeat @ 0x82BD14F8
//
// Create, wire and submit a keep-alive heartbeat request, unless one is already
// pending.
// ---------------------------------------------------------------------------
void CNetworkManager::CreateHeartbeat()
{
    if (mpHeartbeatRequest)                               // lwz 0x74; bne
    {
        MassiveLog(5, spNetworkManager->GetName(),
                   "Heartbeat request is already pending, not creating another");
        return;
    }

    void* lpMemory = CMassiveListNode::operator new(sizeof(CRequestHeartbeat));
    mpHeartbeatRequest = lpMemory ? ::new (lpMemory) CRequestHeartbeat() : 0;
    if (mpHeartbeatRequest)                               // stw 0x74; bne
    {
        mpHeartbeatRequest->CreateRequest(this);          // CreateRequest(hb, this)
        mpHeartbeatRequest->Submit();                     // Submit(*(this+0x74))
    }
    else
    {
        MassiveLog(1, spNetworkManager->GetName(), "Could not create heartbeat object");
    }
}

// ---------------------------------------------------------------------------
// CNetworkManager::ResetHeartbeatWaitTime @ 0x82BD1598
//
// Rewind the heartbeat elapsed accumulator.
// ---------------------------------------------------------------------------
void CNetworkManager::ResetHeartbeatWaitTime()
{
    MassiveLog(5, spNetworkManager->GetName(), "Reset Heartbeat wait timer.");
    mnHeartbeatTimer = 0;                                 // std 0, 0x60
}

// ---------------------------------------------------------------------------
// CNetworkManager::RemoveCurrentRequest @ 0x82BD13D0
//
// Drop the in-flight request and disconnect.
// ---------------------------------------------------------------------------
int CNetworkManager::RemoveCurrentRequest()
{
    mpCurrentRequest = 0;                                 // stw 0, 0x70
    MassiveLog(5, spNetworkManager->GetName(), "Removed currently executing request.");
    return Disconnect();                                 // b ...Disconnect
}

// ---------------------------------------------------------------------------
// CNetworkManager::PrepareForShutdown @ 0x82BD1DE8
//
// Cancel + remove the in-flight request unless its stored type is a keep-state
// (101 / 115). Returns the RemoveCurrentRequest result, or the (incidental)
// in-flight request pointer otherwise (the X360 r3 carry-over).
// ---------------------------------------------------------------------------
int CNetworkManager::PrepareForShutdown()
{
    CRequestObject* lpRequest = mpCurrentRequest;        // lwz 0x70
    if (lpRequest)
    {
        int lnType = lpRequest->GetServerType();         // lwz 0x14
        if (lnType != 101 && lnType != 115)              // 0x65 / 0x73
        {
            lpRequest->Cancel();
            return RemoveCurrentRequest();
        }
    }
    // Incidental r3 carry-over (the in-flight request pointer, or 0).
    return static_cast<int>(reinterpret_cast<std::intptr_t>(lpRequest));
}

// ---------------------------------------------------------------------------
// CNetworkManager::ResolveAddresses @ 0x82BD0E80
//
// Spawn the DNS-resolve thread on MassiveAdClient3::DNS once. Returns 0 when
// started, -1 when a thread already exists / pResolveParam is null / the thread
// could not be created.
// ---------------------------------------------------------------------------
int CNetworkManager::ResolveAddresses(void* pResolveParam)
{
    if (!gpDNSThread && pResolveParam)                   // off_8327F370==0 && a2!=0
    {
        void* lpMemory = CMassiveListNode::operator new(sizeof(CMassiveThread));
        gpDNSThread = lpMemory ? ::new (lpMemory) CMassiveThread() : 0;
        if (gpDNSThread)                                 // stw off_8327F370; bne
        {
            gpDNSThread->Create(DNS, pResolveParam);
            return 0;
        }
        SetLastError(-99, gcEmptyRodata820046A7);
    }
    return -1;
}

// ---------------------------------------------------------------------------
// CNetworkManager::KillAndWaitForDNS @ 0x82BD1448
//
// Signal the DNS thread to stop and busy-wait (up to nTimeoutMs) for it to clear
// the pending flag.
// ---------------------------------------------------------------------------
void CNetworkManager::KillAndWaitForDNS(unsigned int nTimeoutMs)
{
    if (!gbDNSPending)                                    // lwz dword_82F91AD8; beq
        return;

    MassiveLog(5, spNetworkManager->GetName(), "Sending kill signal to DNS thread.");
    gbDNSKillSignal = 1;                                 // stw 1, dword_8327F374

    CMassiveSystem::Instance();
    unsigned int lnStart = CMassiveSystem::GetSystemTime();
    while (gbDNSPending)
    {
        CMassiveSystem::Instance();
        if ((CMassiveSystem::GetSystemTime() - lnStart) > nTimeoutMs)  // cmpld
        {
            MassiveLog(1, spNetworkManager->GetName(), "DNS thread long than %I64d",
                       static_cast<long long>(nTimeoutMs));
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// CNetworkManager::SetDNSPending @ 0x82BD1428 / SetDNSFinished @ 0x82BD1438
//
// Set / clear the process-wide "DNS pending" flag.
// ---------------------------------------------------------------------------
void CNetworkManager::SetDNSPending()  { gbDNSPending = 1; }
void CNetworkManager::SetDNSFinished() { gbDNSPending = 0; }

// ---------------------------------------------------------------------------
// CNetworkManager::GetServerAddressU32 @ 0x82BD1078
//
// The resolved address for server nIndex, or -1 while DNS is pending or when the
// array lock could not be taken.
// ---------------------------------------------------------------------------
int CNetworkManager::GetServerAddressU32(unsigned int nIndex)
{
    if (gbDNSPending)                                    // lwz dword_82F91AD8; bne
    {
        MassiveLog(5, spNetworkManager->GetName(),
                   "Cannot get Server Address because DNS is still Pending");
        return -1;
    }

    if (!gpServerArrayLock->TryEnter("NetworkManager::GetServerAddressU32"))
    {
        MassiveLog(5, spNetworkManager->GetName(),
                   "CNetworkManager(Static): Can not obtain lock to server address array.");
        return -1;
    }

    int lnAddress = static_cast<int>(gaServers[nIndex].mnAddress);  // dword_82F91AE8[3*i]
    gpServerArrayLock->Exit("NetworkManager::GetServerAddressU32");
    return lnAddress;
}

// ---------------------------------------------------------------------------
// CNetworkManager::GetServerPortU16
// ---------------------------------------------------------------------------
unsigned short CNetworkManager::GetServerPortU16(unsigned int nIndex)
{
    if (!gpServerArrayLock->TryEnter("NetworkManager::GetServerPortU16"))
    {
        MassiveLog(5, spNetworkManager->GetName(),
                   "CNetworkManager(Static): Could not get access to server address/port array.");
        return static_cast<unsigned short>(-1);
    }

    unsigned short lnPort = static_cast<unsigned short>(gaServers[nIndex].mnPort);
    gpServerArrayLock->Exit("NetworkManager::GetServerPortU16");
    return lnPort;
}

// ---------------------------------------------------------------------------
// CNetworkManager::GetServerHostName @ 0x82BD1260
//
// The host-name string for server nIndex, or the shared empty sentinel when none
// is set / the lock could not be taken.
// ---------------------------------------------------------------------------
const char* CNetworkManager::GetServerHostName(unsigned int nIndex)
{
    if (gpServerArrayLock->TryEnter("NetworkManager::GetServerHostName"))
    {
        if (gaServers[nIndex].mpcHostName)               // dword_82F91AF0[3*i]
        {
            gpServerArrayLock->Exit("NetworkManager::GetServerHostName");
            return gaServers[nIndex].mpcHostName;
        }
    }
    else
    {
        MassiveLog(5, spNetworkManager->GetName(),
                   "CNetworkManager(Static): Could not get access to server address/port array.");
    }
    return gcEmptyRodata820046A7;                        // &unk_820046A7
}

// ---------------------------------------------------------------------------
// CNetworkManager::SetServerAddress @ 0x82BD0F10
//
// Install a resolved address (+ optional owned host-name copy) for server nIndex
// (<= 16), under the array lock.
// ---------------------------------------------------------------------------
void CNetworkManager::SetServerAddress(unsigned int nIndex, int nAddress, const char* pcHostName)
{
    if (!spNetworkManager)                               // lwz off_...; bne
    {
        MassiveLog(1, "CNetworkManager", "Invalid m_pInstance in SetServerAddress");
        return;
    }
    if (nIndex > 0x10)                                   // cmplwi 0x10; ble
    {
        MassiveLog(2, "CNetworkManager",
                   "CNetworkManager(Static): New server index out of bounds (%d).", nIndex);
        return;
    }

    gpServerArrayLock->Enter("CNetworkManager::SetServerAddress");
    gaServers[nIndex].mnAddress = static_cast<unsigned int>(nAddress);  // stwx dword_82F91AE8

    if (pcHostName && std::strlen(pcHostName) > 1)       // strlen > 1
    {
        int lnLength = static_cast<int>(std::strlen(pcHostName)) + 1;
        gaServers[nIndex].mpcHostName = static_cast<char*>(MassiveMalloc(lnLength));
        // The X360 zero-fills the (possibly null) allocation before the null-check;
        // reproduced verbatim.
        std::memset(gaServers[nIndex].mpcHostName, 0, lnLength);
        if (!gaServers[nIndex].mpcHostName)              // lwzx; cmplwi; bne
        {
            MassiveLog(5, "CNetworkManager", "ALLOCATION Failed for pHostName");
            gpServerArrayLock->Exit("CNetworkManager::SetServerAddress");
            return;
        }
        std::strncpy(gaServers[nIndex].mpcHostName, pcHostName, std::strlen(pcHostName));
    }

    gpServerArrayLock->Exit("CNetworkManager::SetServerAddress");
    MassiveLog(5, "CNetworkManager",
               "Set new address (%d) at index (%d) with host (%s).", nAddress, nIndex, pcHostName);
}

// ---------------------------------------------------------------------------
// CNetworkManager::SetServerPort @ 0x82BD1110
//
// Install a port for server nIndex (<= 16), under a non-blocking array lock.
// ---------------------------------------------------------------------------
void CNetworkManager::SetServerPort(unsigned int nIndex, unsigned short nPort)
{
    if (nIndex > 0x10)                                   // cmplwi 0x10; ble
    {
        MassiveLog(2, spNetworkManager->GetName(),
                   "CNetworkManager(Static): New server index out of bounds (%d).", nIndex);
        return;
    }

    if (gpServerArrayLock->TryEnter("CNetworkManager::SetServerPort"))
    {
        gaServers[nIndex].mnPort = nPort;               // stwx dword_82F91AEC (u16 in dword)
        gpServerArrayLock->Exit("CNetworkManager::SetServerPort");
        MassiveLog(5, spNetworkManager->GetName(),
                   "CNetworkManager(Static): Set new port (%d) at index(%d).", nPort, nIndex);
    }
    else
    {
        MassiveLog(2, spNetworkManager->GetName(),
                   "CNetworkManager(Static): Can't set new port (%d) at index(%d).", nPort, nIndex);
    }
}


// ---------------------------------------------------------------------------
// CNetworkManager::DetermineMaxBytes
// ---------------------------------------------------------------------------
int CNetworkManager::DetermineMaxBytes()
{
    float lfSeconds = static_cast<float>(static_cast<unsigned long long>(mnCurrentTime - mnLastTime)) * 0.001f;

    if (mnState == E_CONN_SENDING)
    {
        if (!mnField28)
        {
            mnMaxBytes = gnSendBufferSize;
            return 0;
        }
        mnMaxBytes = static_cast<int>(static_cast<float>(mnField28 << 10) * lfSeconds);
        MassiveLog(7, spNetworkManager->GetName(), "BW Send Limit: %d bytes", mnMaxBytes);
        return 0;
    }

    if (mnState == E_CONN_RECEIVING)
    {
        if (!mnField2A)
        {
            mnMaxBytes = gnReceiveBufferSize;
            return 0;
        }
        mnMaxBytes = static_cast<int>(static_cast<float>(mnField2A << 10) * lfSeconds);
        MassiveLog(7, spNetworkManager->GetName(), "BW Receive Limit: %d bytes", mnMaxBytes);
        return 0;
    }

    return -793;
}


// ---------------------------------------------------------------------------
// MassiveSetServerPorts
// ---------------------------------------------------------------------------
void MassiveSetServerPorts(CMassiveList* pPorts)
{
    pPorts->GoToStart();
    while (pPorts->GetCurrent())
    {
        CPortIndexPair* lpPair = static_cast<CPortIndexPair*>(pPorts->GetCurrData());
        CNetworkManager::SetServerPort(static_cast<unsigned char>(lpPair->GetType()), lpPair->GetPort());
        pPorts->GoToNext();
    }

    pPorts->GoToStart();
    while (pPorts->GetCurrent())
    {
        CMassiveBaseObject* lpPair = static_cast<CMassiveBaseObject*>(pPorts->GetCurrData());
        if (lpPair)
            delete lpPair;
        pPorts->GoToNext();
    }
    pPorts->RemoveAll();
    pPorts->~CMassiveList();
    CMassiveBaseObject::operator delete(pPorts);
}

// ---------------------------------------------------------------------------
// MassiveInet_ntop
// ---------------------------------------------------------------------------
char* MassiveInet_ntop(int /*nFamily*/, const void* pSrc, char* pcDst, int nSize)
{
    if (!pSrc || nSize < 16 || !pcDst)
        return 0;

    unsigned int luAddress = *static_cast<const unsigned int*>(pSrc);
    MassiveFormatString(pcDst, nSize, "%d.%d.%d.%d", luAddress >> 24, (luAddress >> 16) & 0xFF,
                        (luAddress >> 8) & 0xFF, luAddress & 0xFF);
    return pcDst;
}

// ---------------------------------------------------------------------------
// DNS
//
// The console service-info and title-server records the lookup reads (the
// title-server record is 208 bytes).
// ---------------------------------------------------------------------------
namespace
{

struct XONLINE_SERVICE_INFO
{
    unsigned int   dwServiceID;
    unsigned int   serviceIP;
    unsigned short wServicePort;
    unsigned short wReserved;
};

struct XTITLE_SERVER_INFO
{
    unsigned char inaServer[4];
    unsigned int  dwFlags;
    char          szServerInfo[200];
};

// The log channel name the lookup traces under.
const char gcLspLogName[] = "X" "360 LSP";

const unsigned int KU_MASSIVE_SERVICE_ID = 22;
const unsigned int KU_MAX_TITLE_SERVERS  = 10;

} // anonymous namespace

} // namespace MassiveAdClient3

// Console system-software calls (PC bodies: GameShared/GameClasses/System/PC).
extern "C"
{
long XOnlineGetServiceInfo(unsigned int dwServiceID, void* pServiceInfo);
long XTitleServerCreateEnumerator(const char* pszServerInfo, unsigned int cItem, unsigned int* pcbBuffer,
                                  void** phEnum);
unsigned int XEnumerate(void* hEnum, void* pvBuffer, unsigned int cbBuffer, unsigned int* pcItemsReturned,
                        void* pOverlapped);
long XNetRandom(unsigned char* pb, unsigned int cb);
int XNetServerToInAddr(unsigned int ina, unsigned int dwServiceId, unsigned int* pina);
__declspec(dllimport) int __stdcall CloseHandle(void* hObject);
}

namespace MassiveAdClient3
{

unsigned long DNS(void* pParam)
{
    MassiveLog(5, gcLspLogName, "Starting");
    CNetworkManager::SetDNSPending();

    if (pParam)
    {
        CMassiveList* lpAddresses = static_cast<CMassiveList*>(pParam);
        lpAddresses->GoToStart();
        while (lpAddresses->GetCurrent())
        {
            CMassiveBaseObject* lpPair = static_cast<CMassiveBaseObject*>(lpAddresses->GetCurrData());
            if (lpPair)
                delete lpPair;
            lpAddresses->GoToNext();
        }
        lpAddresses->RemoveAll();
        lpAddresses->~CMassiveList();
        CMassiveBaseObject::operator delete(lpAddresses);
    }

    unsigned int lnServersReturned = 0;
    unsigned int lnMassiveServers = 0;
    void* lhEnum = reinterpret_cast<void*>(-1);
    unsigned int lnBufferSize = 0;
    XTITLE_SERVER_INFO laServers[KU_MAX_TITLE_SERVERS];
    XTITLE_SERVER_INFO laMassiveServers[KU_MAX_TITLE_SERVERS];
    XONLINE_SERVICE_INFO lServiceInfo;
    std::memset(laServers, 0, sizeof(laServers));
    std::memset(laMassiveServers, 0, sizeof(laMassiveServers));

    long lnResult = XOnlineGetServiceInfo(KU_MASSIVE_SERVICE_ID, &lServiceInfo);
    if (lnResult < 0)
    {
        MassiveLog(2, gcLspLogName, "XOnlineGetServiceInfo failed with 0x%0x.", lnResult);
    }
    else if ((lnResult = XTitleServerCreateEnumerator(0, KU_MAX_TITLE_SERVERS, &lnBufferSize, &lhEnum)) < 0)
    {
        MassiveLog(2, gcLspLogName, "XTitleServerCreateEnumerator failed with 0x%0x.", lnResult);
    }
    else if ((lnResult = static_cast<long>(
                  XEnumerate(lhEnum, laServers, sizeof(laServers), &lnServersReturned, 0))) < 0)
    {
        MassiveLog(2, gcLspLogName, "XEnumerate failed with 0x%0x.", lnResult);
        CloseHandle(lhEnum);
    }
    else
    {
        CloseHandle(lhEnum);
        if (!lnServersReturned)
        {
            MassiveLog(2, gcLspLogName, "No servers were returned from XTitleServerCreateEnumerator.");
        }
        else
        {
            MassiveLog(5, gcLspLogName, "Found %d servers:", lnServersReturned);
            for (unsigned int i = 0; i < lnServersReturned; ++i)
            {
                XTITLE_SERVER_INFO& lServer = laServers[i];
                if (std::strstr(lServer.szServerInfo, "MASSIVE"))
                {
                    std::memcpy(&laMassiveServers[lnMassiveServers], &lServer, sizeof(XTITLE_SERVER_INFO));
                    ++lnMassiveServers;
                }
                MassiveLog(5, gcLspLogName, "%d:\"%hs\" located at %d.%d.%d.%d", i, lServer.szServerInfo,
                           lServer.inaServer[0], lServer.inaServer[1], lServer.inaServer[2],
                           lServer.inaServer[3]);
            }

            if (!lnMassiveServers)
            {
                MassiveLog(2, gcLspLogName, "No MASSIVE SG's were returned from XTitleServerCreateEnumerator.");
            }
            else
            {
                MassiveLog(5, gcLspLogName, "Found %d Massive Servers:", lnMassiveServers);
                for (unsigned int i = 0; i < lnMassiveServers; ++i)
                {
                    XTITLE_SERVER_INFO& lServer = laMassiveServers[i];
                    MassiveLog(5, gcLspLogName, "%d:\"%hs\" located at %d.%d.%d.%d", i, lServer.szServerInfo,
                               lServer.inaServer[0], lServer.inaServer[1], lServer.inaServer[2],
                               lServer.inaServer[3]);
                }

                unsigned int luRandom;
                XNetRandom(reinterpret_cast<unsigned char*>(&luRandom), sizeof(luRandom));
                unsigned int luSelected = luRandom % lnMassiveServers;
                MassiveLog(5, gcLspLogName, "Selected Server %d...", luSelected);
                MassiveLog(5, gcLspLogName, "Getting secure address for Server %d...", luSelected);

                unsigned int luServerAddress;
                std::memcpy(&luServerAddress, laMassiveServers[luSelected].inaServer, sizeof(luServerAddress));
                unsigned int luSecureAddress;
                int lnError = XNetServerToInAddr(luServerAddress, KU_MASSIVE_SERVICE_ID, &luSecureAddress);
                if (lnError)
                {
                    MassiveLog(2, gcLspLogName, "XNetServerToInAddr failed with %d.", lnError);
                }
                else
                {
                    for (unsigned int i = 0; i < 16; ++i)
                        CNetworkManager::SetServerAddress(i, static_cast<int>(luSecureAddress), 0);
                }
            }
        }
    }

    CNetworkManager::SetDNSFinished();
    MassiveLog(5, gcLspLogName, "Exiting");
    return 0;
}

} // namespace MassiveAdClient3
