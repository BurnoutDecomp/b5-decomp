#pragma once

#include "types.hpp"
#include "netgamelink.h"
#include "pclan.h"

// ============================================================================
// pclan_link.h -- game links over the PC LAN transport (internal to the PC backend).
//
// [PC platform layer] The console's ConnApi opened a secure peer-to-peer game connection to
// every session member. The PC backend opens one NetGameLink per remote member over the pclan
// socket: ConnApi asks for a link to a member's XUID, the link layer finds the member's
// transport address in the pclan peer book, and the two instances shake hands (LINK_HELLO /
// LINK_ACK). Once either side has heard from the other the link is up and ConnApi reports the
// game connection active. Game packets then travel as PCLAN_DATA datagrams and queue in the
// link's receive ring until NetGameLinkRecv pops them.
//
// Wire bodies (little-endian, after the PcLanHeaderT):
//   LINK_HELLO / LINK_ACK  PcLinkHelloT
//   DATA                   u16 payload length, u8 packet kind, u8 pad, payload bytes
//
// Offline (pclan inactive) no link ever comes up: ConnApi never gets a member list anyway.
// ============================================================================

// Receive ring depth per link. The game drains every link each frame.
#define PCLINK_RECV_SLOTS   16

// Handshake retry interval while a link is opening (the pclan core retransmits each
// control message on its own; this re-sends HELLO when the peer's address becomes known late).
#define PCLINK_HELLO_RETRY_MS 1000

#pragma pack(push, 1)
struct PcLinkHelloT
{
    uint64_t uXuid;      // sender's XUID
    uint32_t uIdent;     // sender's lobby ident
};

struct PcLinkDataHeadT
{
    uint16_t uLen;       // payload bytes
    uint8_t  uKind;      // NetGamePacketHeadT.kind
    uint8_t  uPad;
};
#pragma pack(pop)

// One queued packet.
struct PcLinkPacketT
{
    u16 uLen;
    u8  uKind;
    u8  uPad;
    u8  aData[NETGAME_DATAPKT_MAXSIZE];
};

struct NetGameLinkRefT
{
    NetGameLinkRefT* pNext;          // live-link chain (pclan_link.cpp)
    s32              iMemGroup;      // DirtyMem group the link was allocated in
    u64              uPeerXuid;      // the member this link reaches
    u32              uPeerIdent;     // the member's lobby ident, 0 until it greets us
    PcLanPeerT       Peer;           // transport address, valid once bResolved
    bool             bResolved;      // Peer is known
    bool             bUp;            // the member greeted us (HELLO or ACK received)
    bool             bDown;          // the member left (BYE); the link stays until destroyed
    u8               uPad;
    u32              uOpenTick;      // when the link was created
    u32              uHelloTick;     // when HELLO was last sent (0 = never)

    s32              iRecvHead;      // oldest queued packet
    s32              iRecvCount;     // queued packets
    u32              uRecvDropped;   // packets dropped because the ring was full
    PcLinkPacketT    aRecv[PCLINK_RECV_SLOTS];

    NetGameLinkStatT Stat;           // NetGameLinkStatus block
    u32              uSampleBytes;   // payload bytes sent since the last rate sample
    u32              uSamplePkts;    // packets sent since the last rate sample
};

// Millisecond clock shared by the link, ConnApi and NetConn code.
u32 PcLinkTick();

// ---- link lifetime (pclan_link.cpp) ----
// Create a link to the member with this XUID (allocated in the current DirtyMem group) and
// start the handshake as soon as the member's address is known. Never null.
NetGameLinkRefT* PcLinkCreate(u64 uPeerXuid);
// Unchain and free a link. Safe on null.
void PcLinkDestroy(NetGameLinkRefT* pLink);
// Drive a link that is still opening: resolve the peer address, (re)send HELLO.
void PcLinkPoll(NetGameLinkRefT* pLink);
// Send one game packet to the link's peer. Returns the pclan result (0 = sent).
s32 PcLinkSendData(NetGameLinkRefT* pLink, const u8* pData, u16 uLen, u8 uKind);

// Parse a DirtyAddrT machine address ("$" + 16 hex digits, either case, optionally followed by
// other text) into its XUID. Returns false for anything else.
bool PcLinkXuidFromAddr(const char* pMachineAddr, u64* pXuid);

// ---- departed peers (netconnpc.cpp) ----
// pclan dispatches each message type to a single handler; every PC module that needs to hear
// about a departed peer registers here instead of claiming PCLAN_BYE itself. NetConnStartup
// installs the fan-out. Up to four handlers; the same (handler, ref) pair is kept once.
bool PcNetConnByeAdd(PcLanHandlerT* pHandler, void* pRef);
void PcNetConnByeDel(PcLanHandlerT* pHandler, void* pRef);

// ---- LAN attach (pclan_link.cpp) ----
// Register the LINK_HELLO / LINK_ACK / DATA handlers and the BYE fan-out entry. Called from
// NetConnStartup; harmless to repeat.
void PcLinkAttach();
