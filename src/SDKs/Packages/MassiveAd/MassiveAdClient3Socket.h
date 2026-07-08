#pragma once

// ===========================================================================
// MassiveAdClient3::CMassiveSocket -- the MassiveAd client's blocking TCP socket
// (vendor middleware). Reconstructed from the X360 ARTIST.XEX (no leak / DecFIGS)
// as the type CNetworkManager embeds by value at manager+0x8C. Per-function X360
// addresses:
//     MassiveAdClient3::CMassiveSocket::CMassiveSocket   @ 0x82BD8220
//     MassiveAdClient3::CMassiveSocket::~CMassiveSocket  @ 0x82BD8278
//     MassiveAdClient3::CMassiveSocket::`vector deleting destructor' @ 0x82BD8408
//     MassiveAdClient3::CMassiveSocket::IsConnected      @ 0x82BD82C8
//     MassiveAdClient3::CMassiveSocket::Send             @ 0x82BD8340
//     MassiveAdClient3::CMassiveSocket::Receive          @ 0x82BD8370
//     MassiveAdClient3::CMassiveSocket::ProcessError     @ 0x82BD83A0
//     MassiveAdClient3::CMassiveSocket::Disconnect       @ 0x82BD8598
// (Connect @ ~0x82BD84xx is a separate ledger slice; it sets the connected-peer
//  fields below and is declared here because CNetworkManager::Connect bl's it.)
//
// SHAPE and BODIES are both from the X360 asm. The class derives from
// CMassiveBaseObject: the ctor chains CMassiveBaseObject::CMassiveBaseObject with
// the name "CMassiveSocket", then installs the socket's own vftable (off_82186AAC,
// modelled by the virtual dtor) and defaults the OS handle to -1. The dtor closes
// the handle (when open) and chains the base dtor.
//
// Layout (members BY NAME over the 0x14-byte CMassiveBaseObject base; X360-absolute
// offsets are reproduced by member name, not asserted on the 64-bit host where the
// base and any handle are wider -- semantic parity, not a byte-exact object):
//   +0x00  vftable        (off_82186AAC; slot 0 = vector deleting destructor)
//   +0x04  base CMassiveBaseObject body (mnLastError/mnReportedError/mpcName/mbIsValid)
//   +0x14  mnSocket       (OS socket descriptor; -1 = closed. ctor sets -1, IsConnected/
//                          Send/Receive/Disconnect read it, ProcessError/Disconnect/
//                          the dtor clear it)
//   +0x18  mnPeerAddress  (connected peer address, network order; 0 at ctor, filled by
//                          Connect, read out by CNetworkManager::Tick)
//   +0x1C  mnPeerPort     (connected peer port, u16; 0 at ctor, filled by Connect,
//                          read out by CNetworkManager::Tick)
//
// Per the naming convention the vendor SDK identifiers (the MassiveAdClient3
// namespace and the CMassiveSocket class / method names) are PRESERVED VERBATIM --
// external middleware API, not project-owned code.
// ===========================================================================

#include "SDKs/Packages/MassiveAd/MassiveAdClient3.h"

namespace MassiveAdClient3
{

class CMassiveSocket : public CMassiveBaseObject
{
public:
    // @ 0x82BD8220. Constructs an unconnected socket: chains the base ctor
    // ("CMassiveSocket"), installs this class's vftable, sets the OS handle to -1,
    // and zeroes the connected-peer address / port. (The CNetworkManager ctor bl's
    // this on manager+0x8C.)
    CMassiveSocket();

    // @ 0x82BD8278. Virtual (it overrides the base's virtual dtor and rewrites the
    // vftable). Closes the OS handle when open, then chains the base dtor. (The
    // CNetworkManager dtor bl's it as the embedded member's destructor.)
    virtual ~CMassiveSocket();

    // @ 0x82BD8408. The X360 vftable slot-0 thunk: runs ~CMassiveSocket, then frees
    // the object through CMassiveBaseObject::operator delete when the low bit of
    // bDelete is set. Returns this.
    void* VectorDeletingDestructor(char bDelete);

    // Opens a blocking connection to nAddress:nPort, sizing the OS send/receive
    // buffers, and records the connected peer into mnPeerAddress / mnPeerPort.
    // Returns non-zero on success, 0 on failure. Separate ledger slice; declared
    // here (CMassiveSocket is its home) because CNetworkManager::Connect bl's it.
    int Connect(unsigned int nAddress, unsigned short nPort,
                int nSendBufferSize, int nReceiveBufferSize);

    // @ 0x82BD8598. Closes the connection: closesocket() when the handle is open,
    // resets the handle to -1, and classifies any close error through ProcessError.
    // Returns 1 when the close failed with a fatal error, else 0.
    int Disconnect();

    // @ 0x82BD8340. Sends up to nLength bytes; returns the count sent, or -1 on
    // error.
    int Send(const void* pData, int nLength);

    // @ 0x82BD8370. Receives up to nLength bytes into pBuffer; returns the count
    // read, or -1 on error.
    int Receive(void* pBuffer, int nLength);

    // @ 0x82BD83A0. After a -1 Send/Receive, classifies the last socket error:
    // returns 0 for the transient/already-connected cases (WSAEWOULDBLOCK /
    // WSAEISCONN), else 1; WSAENOTSOCK also forgets the (invalid) handle.
    int ProcessError();

    // @ 0x82BD82C8. Non-zero once the pending Connect has completed the handshake
    // (select() reports the socket writable).
    int IsConnected();

    // Additive accessors (FLAG: not their own X360 functions). CNetworkManager::
    // Tick reads the connected peer's address (mnPeerAddress at socket+0x18) and
    // port (mnPeerPort at socket+0x1C) directly after IsConnected succeeds, to
    // record them into the manager's own mnConnectedAddress / mnConnectedPort.
    // Exposed BY NAME so the manager does not reach into the socket's fields by
    // offset.
    unsigned int   GetPeerAddress() const { return mnPeerAddress; }
    unsigned short GetPeerPort() const    { return mnPeerPort; }

private:
    int            mnSocket;       // +0x14 (OS socket descriptor; -1 = closed)
    unsigned int   mnPeerAddress;  // +0x18 (connected peer address, network order)
    unsigned short mnPeerPort;     // +0x1C (connected peer port)
};

} // namespace MassiveAdClient3
