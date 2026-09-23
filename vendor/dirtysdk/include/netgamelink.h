#ifndef DIRTYSDK_NETGAMELINK_H
#define DIRTYSDK_NETGAMELINK_H

#include "types.hpp"

// DirtySDK 5.5.3 - core/include/netgamelink.h
// A NetGameLink is one peer-to-peer game connection: the game hands it whole packets
// (NetGamePacketT) and pulls whole packets back out. ConnApi owns the links; the game only
// reaches them through the NetGameLinkRefT pointer ConnApi publishes in each client entry.
//
// The packet and stat layouts below are the ones the game reads: the network adapter builds a
// NetGamePacketT on its stack, fills head.len / head.kind / body.data and sends it; it receives
// into a sizeof(NetGamePacketT) buffer and reads head.len and body.data. The player registry reads
// three per-second rates out of NetGameLinkStatT.
// Bodies (PC backend): ../src/pc/netgamelinkpc.cpp.

// Opaque link handle (defined by the backend).
typedef struct NetGameLinkRefT NetGameLinkRefT;

// Largest payload one packet carries.
#define NETGAME_DATAPKT_MAXSIZE 1200

// Packet header. head.size is written by the link on send and read back on receive: the whole
// packet's footprint rounded up to 4 bytes (header + payload + trailer), which is also the stride
// between packets when several are received into one buffer.
typedef struct NetGamePacketHeadT
{
    u32 size;   // +0x00
    u16 len;    // +0x04  payload bytes in body.data
    u8  kind;   // +0x06  packet kind (the game sends 7)
    u8  pad;    // +0x07
} NetGamePacketHeadT;

typedef struct NetGamePacketT
{
    NetGamePacketHeadT head;                              // +0x000
    union
    {
        u8 data[NETGAME_DATAPKT_MAXSIZE];                 // +0x008
    } body;
    // Trailer counted in head.size; the link keeps its bookkeeping here, the game never reads it.
    u8 trailer[12];                                       // +0x4B8
} NetGamePacketT;                                         // 0x4C4 (1220) bytes

static_assert(sizeof(NetGamePacketT) == 1220, "NetGamePacketT is the 1220-byte packet block");

// Link statistics (NetGameLinkStatus). The per-second rates are recomputed once a second from
// the bytes and packets sent since the previous sample.
typedef struct NetGameLinkStatT
{
    u32 tick;           // +0x00  time of this snapshot (ms)
    u32 tickseqn;       // +0x04  bumped on every link update
    u8  aReserved08[0x2C];  // +0x08  link timing / latency block (not read by the game)
    s32 outbps;         // +0x34  game payload bytes sent per second
    s32 outrps;         // +0x38  bytes per second handed to the transport (payload + link framing)
    s32 outnps;         // +0x3C  bytes per second on the wire (+ datagram overhead)
    s32 outpps;         // +0x40  game packets sent per second
    s32 outrpps;        // +0x44  transport packets sent per second
    u32 stattick;       // +0x48  time of the last rate sample
    u32 aTransport[3];  // +0x4C  transport-layer counters copied in on each status query
    u8  isconn;         // +0x58  link established
    u8  isopen;         // +0x59  link established or still opening
    u8  aPad5A[2];      // +0x5A
} NetGameLinkStatT;

#ifdef __cplusplus
extern "C" {
#endif

// Send packets. With iLen == 1, pPacket is exactly one packet (head.len payload bytes); otherwise
// iLen is the byte length of a run of packets laid out head.size apart. Each sent packet's
// head.size is rewritten. Returns the number of bytes consumed (> 0 on success), 0 or negative
// when nothing could be sent.
s32 NetGameLinkSend(NetGameLinkRefT* pRef, NetGamePacketT* pPacket, s32 iLen);

// Receive packets into pBuffer. With iLen == 1 exactly one packet is returned; otherwise as
// many whole packets as fit in iLen bytes. Returns the bytes written (0 when nothing is queued).
s32 NetGameLinkRecv(NetGameLinkRefT* pRef, NetGamePacketT* pBuffer, s32 iLen);

// Refresh and return the link's statistics block (owned by the link).
const NetGameLinkStatT* NetGameLinkStatus(NetGameLinkRefT* pRef);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_NETGAMELINK_H
