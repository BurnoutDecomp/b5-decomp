#include "SDKs/Packages/MassiveAd/MassiveAdClient3Socket.h"

// The X360 socket bodies call the platform Winsock primitives directly
// (closesocket / select / recv / send / WSAGetLastError, and the WSAE* error
// codes). MassiveAd is vendor middleware over the OS sockets layer, so these are
// FLAGGED platform APIs supplied by <winsock2.h> on the PC target -- the same
// direct calls the ARTIST asm makes, not routed through a Massive* hook.
#include <winsock2.h>
#include <cstring>

// ===========================================================================
// MassiveAdClient3::CMassiveSocket -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// SHAPE and BODIES both from the X360 asm (no leak / DecFIGS). Stores reproduced
// member-for-member; see MassiveAdClient3Socket.h for the layout map. The class
// installs its own vftable (off_82186AAC) over the CMassiveBaseObject slot --
// modelled by the virtual destructor, so no vftable store is written by hand.
// ===========================================================================

namespace MassiveAdClient3
{

// ---------------------------------------------------------------------------
// CMassiveSocket::CMassiveSocket @ 0x82BD8220
//
//   CMassiveBaseObject::CMassiveBaseObject(this, "CMassiveSocket");
//   vftable = off_82186AAC;   (compiler-emitted)
//   +0x18 = 0;  +0x14 = -1;  +0x1C = 0;
// ---------------------------------------------------------------------------
CMassiveSocket::CMassiveSocket()
    : CMassiveBaseObject("CMassiveSocket")
{
    mnPeerAddress = 0;   // +0x18
    mnSocket = -1;       // +0x14 (no OS handle yet)
    mnPeerPort = 0;      // +0x1C
}

// ---------------------------------------------------------------------------
// CMassiveSocket::~CMassiveSocket @ 0x82BD8278
//
// Rewrites the vftable (compiler-emitted for the virtual dtor), closes the OS
// handle when one is open, then chains ~CMassiveBaseObject (compiler-emitted).
// Note: unlike Disconnect, the dtor does NOT reset mnSocket -- the object is
// going away.
// ---------------------------------------------------------------------------
CMassiveSocket::~CMassiveSocket()
{
    if (mnSocket != -1)
        closesocket(mnSocket);
}

// ---------------------------------------------------------------------------
// CMassiveSocket::`vector deleting destructor' @ 0x82BD8408
//
// The X360 vftable slot-0 thunk: runs the dtor, then frees the object through
// CMassiveBaseObject::operator delete when the low bit of bDelete is set.
// ---------------------------------------------------------------------------
void* CMassiveSocket::VectorDeletingDestructor(char bDelete)
{
    this->~CMassiveSocket();
    if (bDelete & 1)
        CMassiveBaseObject::operator delete(this);
    return this;
}

// ---------------------------------------------------------------------------
// CMassiveSocket::IsConnected @ 0x82BD82C8
//
// A zero-timeout select() over a single-socket write set: the socket is
// connected once it reports writable. Returns 0 on select() error, else true
// iff the socket is in the ready set (a positive count).
// ---------------------------------------------------------------------------
int CMassiveSocket::IsConnected()
{
    fd_set  lWriteSet;
    timeval lTimeout;

    lTimeout.tv_sec = 0;
    lTimeout.tv_usec = 0;
    lWriteSet.fd_count = 1;
    lWriteSet.fd_array[0] = mnSocket;

    int lnReady = select(0, 0, &lWriteSet, 0, &lTimeout);
    return lnReady != -1 && lnReady != 0;
}

// ---------------------------------------------------------------------------
// CMassiveSocket::Send @ 0x82BD8340
// ---------------------------------------------------------------------------
int CMassiveSocket::Send(const void* pData, int nLength)
{
    return send(mnSocket, static_cast<const char*>(pData), nLength, 0);
}

// ---------------------------------------------------------------------------
// CMassiveSocket::Receive @ 0x82BD8370
// ---------------------------------------------------------------------------
int CMassiveSocket::Receive(void* pBuffer, int nLength)
{
    return recv(mnSocket, static_cast<char*>(pBuffer), nLength, 0);
}

// ---------------------------------------------------------------------------
// CMassiveSocket::ProcessError @ 0x82BD83A0
//
// Classifies the last Winsock error after a failed transfer/close:
//   WSAEWOULDBLOCK (10035) / WSAEISCONN  (10056) -> transient, return 0
//   WSAENOTSOCK    (10038)                       -> forget the handle, return 1
//   WSAECONNABORTED(10053) / WSAECONNRESET(10054) / anything else -> return 1
// ---------------------------------------------------------------------------
int CMassiveSocket::ProcessError()
{
    switch (WSAGetLastError())
    {
    case WSAEWOULDBLOCK: // 10035 -- would block; nothing to do yet
    case WSAEISCONN:     // 10056 -- already connected
        return 0;

    case WSAENOTSOCK:    // 10038 -- the handle is not a socket; drop it
        mnSocket = -1;
        break;

    case WSAECONNABORTED: // 10053
    case WSAECONNRESET:   // 10054
        break;

    default:
        break;
    }

    return 1;
}

// ---------------------------------------------------------------------------
// CMassiveSocket::Disconnect @ 0x82BD8598
//
// Closes the socket when open, always resets the handle to -1, and treats a
// non-fatal close error (per ProcessError) as success. Returns 1 on a fatal
// close error, else 0.
// ---------------------------------------------------------------------------
int CMassiveSocket::Disconnect()
{
    if (mnSocket != -1)
    {
        int lnClose = closesocket(mnSocket);
        mnSocket = -1;
        if (lnClose != -1 || !ProcessError())
            return 1;
    }
    else
    {
        mnSocket = -1;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// CMassiveSocket::Connect
//
// Records the peer, opens a non-blocking TCP socket with the requested buffer
// sizes and starts the connect. 1 when the connect is under way (or done), 0
// when a socket is already open, socket creation failed, or the connect failed
// with a hard error. FLAG PC-platform: the peer address and port are host-order
// values; Winsock takes them in network order, which the big-endian console
// stored them in natively.
// ---------------------------------------------------------------------------
int CMassiveSocket::Connect(unsigned int nAddress, unsigned short nPort, int nSendBufferSize,
                            int nReceiveBufferSize)
{
    sockaddr_in lPeer;
    lPeer.sin_family = AF_INET;
    lPeer.sin_port = htons(nPort);
    lPeer.sin_addr.s_addr = htonl(nAddress);
    std::memset(lPeer.sin_zero, 0, sizeof(lPeer.sin_zero));

    mnPeerAddress = nAddress;
    mnPeerPort = nPort;
    if (mnSocket != -1)
        return 0;

    mnSocket = static_cast<int>(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (mnSocket == -1)
        return 0;

    u_long luNonBlocking = 1;
    ioctlsocket(mnSocket, FIONBIO, &luNonBlocking);
    setsockopt(mnSocket, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&nSendBufferSize), 4);
    setsockopt(mnSocket, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&nReceiveBufferSize), 4);

    int lnSendBufferSize = 0;
    int lnReceiveBufferSize = 0;
    int lnOptionLength = 4;
    getsockopt(mnSocket, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char*>(&lnSendBufferSize), &lnOptionLength);
    getsockopt(mnSocket, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char*>(&lnReceiveBufferSize), &lnOptionLength);

    if (connect(mnSocket, reinterpret_cast<const sockaddr*>(&lPeer), sizeof(lPeer)) == -1 && ProcessError())
        return 0;
    return 1;
}

} // namespace MassiveAdClient3
