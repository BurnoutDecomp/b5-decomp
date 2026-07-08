#pragma once

// ===========================================================================
// MassiveAdClient3::CMassiveSocket -- the MassiveAd client's blocking TCP socket
// (vendor middleware). Reconstructed from the X360 ARTIST.XEX (no leak / DecFIGS)
// as the type CNetworkManager embeds by value at manager+0x8C.
//
// The socket bodies (ctor/dtor/Connect/Disconnect/Send/Receive/ProcessError/
// IsConnected) are their own ledger TUs; this header is the owning home so the
// CNetworkManager TU can embed the object by value and drive it BY NAME. Only the
// surface CNetworkManager touches is modelled:
//   - Connect(address, port, sendBufSize, recvBufSize): direct bl from
//         CNetworkManager::Connect (socket+0x8C, r6/r7 = the send/receive buffer
//         sizes the manager sized in Initialize)
//   - Disconnect / Send / Receive / ProcessError / IsConnected: the transport
//         primitives driven by Send / Receive / Tick / Disconnect
//   - the connected-peer address / port the socket records once connected, which
//         CNetworkManager::Tick copies out (manager reads socket+0x18 / socket+0x1C
//         after IsConnected succeeds) into its own mnConnectedAddress / mnConnectedPort
//
// Layout: the whole CNetworkManager object is a single MassiveMalloc(176) block
// (CNetworkManager::Initialize) whose socket subobject runs manager+0x8C..0xB0
// (36 bytes on the X360). The connected-peer fields are attested at socket+0x18
// (u32 address) and socket+0x1C (u16 port); the preceding bytes are the OS socket
// handle + connection state, modelled as an opaque block. Offsets are X360-
// relative and reproduced BY NAME -- the host widths of the opaque handle area are
// not asserted (semantic parity, not a byte-exact 36-byte object on the 64-bit
// host).
//
// Per the naming convention the vendor SDK identifiers (the MassiveAdClient3
// namespace and the CMassiveSocket class / method names) are PRESERVED VERBATIM --
// external middleware API, not project-owned code.
// ===========================================================================

namespace MassiveAdClient3
{

class CMassiveSocket
{
public:
    // Constructs an unconnected socket (the CNetworkManager ctor bl's this on
    // manager+0x8C). Body in the CMassiveSocket TU.
    CMassiveSocket();

    // Closes the socket if open. Non-virtual (the CNetworkManager dtor bl's it as
    // a plain member destructor on manager+0x8C). Body in the CMassiveSocket TU.
    ~CMassiveSocket();

    // Opens a blocking connection to nAddress:nPort, sizing the OS send/receive
    // buffers. Returns non-zero on success, 0 on failure. Direct bl from
    // CNetworkManager::Connect.
    int Connect(unsigned int nAddress, unsigned short nPort,
                int nSendBufferSize, int nReceiveBufferSize);

    // Closes the connection. Returns non-zero on success, 0 on failure.
    int Disconnect();

    // Sends up to nLength bytes; returns the count sent, or -1 on error.
    int Send(const void* pData, int nLength);

    // Receives up to nLength bytes into pBuffer; returns the count read, or -1 on
    // error.
    int Receive(void* pBuffer, int nLength);

    // After a -1 Send/Receive, classifies the socket error: returns non-zero when
    // the error is fatal (the caller must disconnect), 0 when it is transient.
    int ProcessError();

    // Non-zero once the pending Connect has completed the handshake.
    int IsConnected();

    // Additive accessors (FLAG: not their own X360 functions). CNetworkManager::
    // Tick reads the connected peer's address (socket+0x18) and port (socket+0x1C)
    // directly after IsConnected succeeds, to record them into the manager's own
    // mnConnectedAddress / mnConnectedPort. Exposed BY NAME so the manager does not
    // reach into the socket's fields by offset.
    unsigned int   GetPeerAddress() const { return mnPeerAddress; }
    unsigned short GetPeerPort() const    { return mnPeerPort; }

private:
    // OS socket handle + connection state (family/flags). Opaque -- touched only by
    // the socket's own bodies; sized to keep the connected-peer fields below at
    // their X360 socket-relative offsets by name.
    unsigned char  mabState[0x18];   // +0x00 (socket handle + state)
    unsigned int   mnPeerAddress;    // +0x18 (connected peer address, network order)
    unsigned short mnPeerPort;       // +0x1C (connected peer port)
    unsigned char  mabPad[6];        // +0x1E..0x24 (X360 socket subobject is 36 bytes)
};

} // namespace MassiveAdClient3
