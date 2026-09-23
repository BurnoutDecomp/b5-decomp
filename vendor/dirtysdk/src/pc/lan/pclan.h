#pragma once

#include <stdint.h>

// ============================================================================
// pclan.h -- the PC LAN transport shared by the DirtySDK PC backend.
//
// [PC platform layer] The console reached other players through the online lobby service. The PC
// build has no service: with BP_LAN=1 each instance opens one UDP socket that carries discovery,
// lobby traffic and game data, and the instance that creates a game acts as the lobby record
// authority for it. Without BP_LAN=1 (offline) PcLanStartup() opens nothing and every send fails,
// so the backend answers exactly like a console with no network.
//
// Identity (persona, XUID, lobby ident) comes from CgsPcNetIdentity.h; this layer never invents
// its own.
//
// Wire: every datagram starts with PcLanHeaderT (little-endian) followed by iLen body bytes.
// Control messages (every type except PCLAN_DATA and PCLAN_ACK) are acknowledged and retransmitted
// every 250 ms until acked or the peer is dropped; PCLAN_DATA is fire-and-forget because the game
// runs its own reliability on top.
// ============================================================================

#define PCLAN_MAGIC         0x314C5042u   // 'BPL1' read as a little-endian u32
#define PCLAN_VERSION       1
#define PCLAN_PORT_DEFAULT  27800         // BP_LAN_PORT overrides; the first free port of base..base+7 is bound
#define PCLAN_PORT_WINDOW   8
#define PCLAN_MAX_BODY      4096          // a PLAY record for a full game exceeds one MTU; UDP fragments it

enum PcLanMsgE
{
    PCLAN_ACK = 0,
    PCLAN_DISCOVER,     // search sweep; hosts answer with ADVERT
    PCLAN_ADVERT,       // host -> searcher: game record for list 3
    PCLAN_JOIN_REQ,     // member -> host
    PCLAN_JOIN_RESP,    // host -> member: 0 or a lobby error code ('full' 'asta' 'lock' 'pass' 'ugam')
    PCLAN_PLAY,         // host -> members: play record + member table (on change and at 1 Hz)
    PCLAN_LOBBYREQ,     // member -> host: game-record request ('gset' 'gsta' 'glea' ...) + tagfield text
    PCLAN_KICK,         // host -> member
    PCLAN_LEAVE,        // member -> host
    PCLAN_BYE,          // either side, process shutting down
    PCLAN_LINK_HELLO,   // peer link handshake (ConnApi client status 3 once acked)
    PCLAN_LINK_ACK,
    PCLAN_DATA,         // NetGameLink packet: u16 len + body
    PCLAN_NUM_TYPES
};

#pragma pack(push, 1)
struct PcLanHeaderT
{
    uint32_t uMagic;         // PCLAN_MAGIC
    uint8_t  uVersion;       // PCLAN_VERSION
    uint8_t  uType;          // PcLanMsgE
    uint16_t uLen;           // body bytes after the header
    uint32_t uSenderIdent;   // sender's lobby ident
    uint32_t uGameIdent;     // game the message belongs to, 0 when none
    uint16_t uSeq;           // control sequence number (0 for DATA); ACK echoes it
    uint16_t uPad;
};
#pragma pack(pop)

// A peer's transport address, host byte order.
struct PcLanPeerT
{
    uint32_t uAddr;
    uint16_t uPort;
    uint16_t uPad;
};

typedef void PcLanHandlerT(void *pRef, const PcLanPeerT *pFrom, const PcLanHeaderT *pHeader, const uint8_t *pBody);

// Lifecycle. PcLanStartup reads the environment once; it returns false offline (BP_LAN unset) or
// when no port in the window could be bound, and the layer then stays inert. Safe to call again.
bool PcLanStartup();
void PcLanShutdown();
bool PcLanActive();

// This instance's bound address (loopback or first LAN interface) and port.
PcLanPeerT PcLanSelf();

// Pump: drains the socket, dispatches each datagram to the handler registered for its type,
// sends ACKs, retransmits unacked control messages. Called from NetConnIdle.
void PcLanIdle();

// One handler per message type; a later registration replaces the earlier one. pHandler 0 clears.
void PcLanRegister(uint8_t uType, PcLanHandlerT *pHandler, void *pRef);

// Sends. Return 0 on success, negative when offline, oversized or the socket refused.
int32_t PcLanSendControl(const PcLanPeerT *pTo, uint8_t uType, uint32_t uGameIdent, const void *pBody, int32_t iLen);
int32_t PcLanSendData(const PcLanPeerT *pTo, uint32_t uGameIdent, const void *pBody, int32_t iLen);
// DISCOVER sweep: 255.255.255.255 and 127.0.0.1, every port of the window except our own.
int32_t PcLanBroadcast(uint8_t uType, uint32_t uGameIdent, const void *pBody, int32_t iLen);

// Peer book: lobby code records which transport address belongs to which XUID / lobby ident as
// it learns them (ADVERT, JOIN_REQ, PLAY); link code looks them up to open game links.
void PcLanPeerRemember(uint64_t uXuid, uint32_t uIdent, const PcLanPeerT *pPeer);
bool PcLanPeerByXuid(uint64_t uXuid, PcLanPeerT *pPeer);
bool PcLanPeerByIdent(uint32_t uIdent, PcLanPeerT *pPeer);
void PcLanPeerForget(uint32_t uIdent);

// A peer whose control messages go unacked for this long is dropped and PCLAN_BYE is synthesised
// to its handlers (matches the game's 5 s disconnect timeout).
#define PCLAN_PEER_TIMEOUT_MS 5000

// ---- additions (lane P4) -------------------------------------------------------------------
//
// Reliability rules, as pclan_core.cpp implements them:
//  - uSeq 0 marks an unreliable datagram: never acked, never retransmitted, never de-duplicated.
//    PcLanBroadcast always sends uSeq 0 (a sweep hits ports nobody listens on, which must not
//    time anybody out), and so does the BYE PcLanShutdown sends on the way out.
//  - PcLanSendControl picks a non-zero uSeq; the receiver acks every copy and dispatches the
//    first one only (a copy whose ack was lost is recognised by sender address + ident + uSeq).
//  - PCLAN_ACK is consumed by the core and never dispatched.
//  - A control message with no registered handler is still acked (the transport delivered it).
//  - A real or synthesised PCLAN_BYE is dispatched first; afterwards the core drops every unacked
//    message to that address and forgets the peer book entries at that address or under the
//    BYE's sender ident.
//  - A synthesised BYE carries the dropped peer's lobby ident (0 when the peer book does not know
//    the address) and the uGameIdent of the message that timed out.
//  - Datagrams from our own socket (a broadcast echo) are ignored.
//  - Handlers may call any PcLan function, including sends; the pending message that timed out is
//    already removed when its BYE is dispatched.

#define PCLAN_RETRANSMIT_MS 250
#define PCLAN_MAX_PENDING   128   // unacked control messages in flight; a send beyond this fails
#define PCLAN_MAX_PEERS     32    // peer book entries; the least recently remembered is reused

// Transport counters since PcLanStartup (for witness lines and the loopback test).
struct PcLanStatsT
{
    uint32_t uSent;       // datagrams sent (first transmissions, acks and data included)
    uint32_t uResent;     // control retransmissions
    uint32_t uRecv;       // well-formed datagrams received
    uint32_t uDup;        // control copies acked again but not dispatched
    uint32_t uAcked;      // pending control messages closed by an ack
    uint32_t uTimeouts;   // peers dropped for unacked control messages
    uint32_t uBad;        // datagrams rejected (magic, version, type or length)
    uint32_t uPending;    // control messages still waiting for an ack
};
void PcLanGetStats(PcLanStatsT *pStats);

// "DISCOVER", "JOIN_REQ", ... for log lines; "?" for an unknown type.
const char *PcLanTypeName(uint8_t uType);
