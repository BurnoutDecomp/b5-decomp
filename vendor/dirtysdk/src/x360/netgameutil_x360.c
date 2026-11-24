// XDK size optimization matches ARTIST at 0x82B2F5A8.
// C, as the original (DWARF home core/source/game/netgameutil.c), definitions in its DWARF line
// order (Create :174, Destroy :211, Reset :234, Control :290, Connect :348, Complete :465,
// Status :598): the module's unpooled literals sit in .data in that order (ARTIST 0x82F829D8..).
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

#include "netgameutil.h"
#include "commall_x360_internal.h"
#include "dirtylib.h"
#include "dirtymem.h"
#include "platform.h"
#include "protoadvt.h"
#include <stddef.h>
#include <string.h>

typedef struct CommUDPRef CommUDPRef;   // DirtySDK is C
struct CommUDPRef;
typedef struct CommSRPRef CommSRPRef;   // DirtySDK is C
struct CommSRPRef;
typedef struct CommTCPRef CommTCPRef;   // DirtySDK is C
struct CommTCPRef;
CommUDPRef* CommUDPConstruct(s32 maxWidth, s32 maxInput, s32 maxOutput);
CommSRPRef* CommSRPConstruct(s32 maxWidth, s32 maxInput, s32 maxOutput);
CommTCPRef* CommTCPConstruct(s32 maxWidth, s32 maxInput, s32 maxOutput);
char* ds_strnzcpy(char* pDest, const char* pSource, s32 size);

typedef struct NetGameUtilRefT NetGameUtilRefT;   // DirtySDK is C
struct NetGameUtilRefT
{

    s32 memgroup;
    char* table;
    char kind[32];
    char addr[128];
    ProtoAdvtRefT* advt;
    ProtoAdvtRefT* find;
    s32 hosting;
    u32 hostip;
    u32 hostport;
    u32 peerip;
    u32 peerport;
    SocketT* pSocket;
    u32 proto;
    s32 maxwid;
    s32 maxout;
    s32 maxinp;
    s32 advtfreq;
    s32 clientid;
    s32 rclientid;
    // DWARF netgameutil.c:104-111: the transport ports are an array of an anonymous struct.
    struct
    {
        CommRef* comm;
        u32 tick;
        u32 wait;
    } port[4];
};

CGS_STATIC_ASSERT(sizeof(NetGameUtilRefT) == 0x114, "ARTIST NetGameUtil allocation");
CGS_STATIC_ASSERT(offsetof(NetGameUtilRefT, advt) == 0xA8, "ARTIST advertisement");
CGS_STATIC_ASSERT(offsetof(NetGameUtilRefT, find) == 0xAC, "ARTIST discovery");
CGS_STATIC_ASSERT(offsetof(NetGameUtilRefT, proto) == 0xC8, "ARTIST protocol selector");
CGS_STATIC_ASSERT(offsetof(NetGameUtilRefT, port) == 0xE4, "ARTIST transport ports");

static const s32 NETGAMEUTIL_MEMID = 0x6e677574;


// ARTIST 0x82B2FCC0
NetGameUtilRefT* NetGameUtilCreate()
{
    const s32 group = DirtyMemGroupQuery();
    NetGameUtilRefT* ref = (NetGameUtilRefT*)(DirtyMemAlloc(sizeof(NetGameUtilRefT), NETGAMEUTIL_MEMID, group));
    if (!ref)
    {
        NetPrintfCode("netgameutil: unable to allocate module state\n");
        return 0;
    }

    memset(ref, 0, sizeof(*ref));
    ref->memgroup = group;
    // The defaults go through the control selects: ARTIST inlines NetGameUtilControl here and
    // prints its own literals (0x82F82A50/9C/BC, behind Control's clid/rcid texts).
    NetGameUtilControl(ref, 0x6d776964, 240); // mwid (stored +12: 252)
    NetGameUtilControl(ref, 0x6d696e70, 32);  // minp
    NetGameUtilControl(ref, 0x6d6f7574, 32);  // mout
    return ref;
}

// ARTIST 0x82B2FD68
void NetGameUtilDestroy(NetGameUtilRefT* ref)
{
    NetGameUtilReset(ref);
    DirtyMemFree(ref, NETGAMEUTIL_MEMID, ref->memgroup);
}

// ARTIST 0x82B2F5A8
void NetGameUtilReset(NetGameUtilRefT* ref)
{
    s32 i;
    for (i = 0; i < 4; ++i)
    {
        CommRef* comm = ref->port[i].comm;
        if (comm)
            comm->Destroy(comm);
        ref->port[i].comm = 0;
    }
    if (ref->find)
    {
        ProtoAdvtDestroy(ref->find);
        ref->find = 0;
    }
    if (ref->advt)
    {
        ProtoAdvtDestroy(ref->advt);
        ref->advt = 0;
    }
    ref->proto = 0;
}

// ARTIST 0x82B2F620
void NetGameUtilControl(NetGameUtilRefT* ref, s32 selector, s32 value)
{
    switch (selector)
    {
    case 0x636c6964: // clid
        ref->clientid = value;
        NetPrintfCode("netgameutil: setting clid=0x%08x\n", (u32)value);
        break;
    case 0x72636964: // rcid
        ref->rclientid = value;
        NetPrintfCode("netgameutil: setting rcid=0x%08x\n", (u32)value);
        break;
    case 0x6d776964: // mwid
        if (value <= 1200)
        {
            ref->maxwid = value + 12;
            NetPrintfCode("netgameutil: setting mwid=%d\n", ref->maxwid);
        }
        else
        {
            NetPrintfCode("netgameutil: mwid value of %d is too large\n", value);
        }
        break;
    case 0x6d696e70: // minp
        ref->maxinp = value;
        NetPrintfCode("netgameutil: setting minp=%d\n", value);
        break;
    case 0x6d6f7574: // mout
        ref->maxout = value;
        NetPrintfCode("netgameutil: setting mout=%d\n", value);
        break;
    case 0x61647666: // advf
        ref->advtfreq = value;
        NetPrintfCode("netgameutil: setting advf=%d\n", value);
        break;
    }
}

// ARTIST 0x82B2F720
s32 NetGameUtilConnect(NetGameUtilRefT* ref, s32 connection, const char* address)
{
    const s32 mode = connection & 3;
    s32 i;
    s32 result;
    if (mode == 0 || (connection & 0x700) == 0)
    {
        NetPrintfCode("netgameutil: invalid conn param\n");
        return -1;
    }

    strcpy(ref->addr, address);
    ref->hosting = (connection >> 1) & 1;
    ref->proto = connection & ~3u;
    NetPrintfCode("netgameutil: connect %d %s\n", connection, address);

    for (i = 0; i < 4; ++i)
    {
        if (ref->port[i].comm)
            ref->port[i].comm->Destroy(ref->port[i].comm);
        ref->port[i].comm = 0;
        ref->port[i].tick = 0;
    }

    if (mode == 3)
    {
        if (!ref->find)
        {
            ProtoAdvtRefT* find;
            DirtyMemGroupEnter(ref->memgroup);
            find = ProtoAdvtConstruct(8);
            DirtyMemGroupLeave();
            ref->find = find;
        }
        // ARTIST's 0x8324EBD8 note is empty and only read by this call.
        ProtoAdvtAnnounce(ref->find, "GmUtil", address, "", "TCP:~1:1024\tUDP:~1:1024", 0);
        return 0;
    }

    DirtyMemGroupEnter(ref->memgroup);
    if (connection & 0x100)
    {
        ref->port[0].comm = (CommRef*)(CommUDPConstruct(ref->maxwid, ref->maxinp, ref->maxout));
        ref->port[0].wait = 0;
    }
    else if (connection & 0x400)
    {
        ref->port[1].comm = (CommRef*)(CommSRPConstruct(ref->maxwid, ref->maxinp, ref->maxout));
        ref->port[1].wait = 0;
    }
    else if (connection & 0x200)
    {
        ref->port[2].comm = (CommRef*)(CommTCPConstruct(ref->maxwid, ref->maxinp, ref->maxout));
        ref->port[2].wait = (ref->port[0].comm || ref->port[1].comm) ? 5000 : 0;
    }

    result = 0;
    for (i = 0; i < 4; ++i)
    {
        if (!ref->port[i].comm)
            continue;
        if (ref->port[i].comm->Control)
        {
            ref->port[i].comm->Control(ref->port[i].comm, 0x636c6964, ref->clientid, 0); // clid
            ref->port[i].comm->Control(ref->port[i].comm, 0x72636964, ref->rclientid, 0); // rcid
        }
        if (mode & 2)
            result = ref->port[i].comm->Connect(ref->port[i].comm, address);
        else if (mode & 1)
            result = ref->port[i].comm->Listen(ref->port[i].comm, address);
        ref->pSocket = ref->port[i].comm->sockptr;
        break;
    }
    DirtyMemGroupLeave();
    return result;
}

// ARTIST 0x82B2F9A0
void* NetGameUtilComplete(NetGameUtilRefT* ref)
{
    u32 now;
    s32 winner;
    CommRef* comm;
    s32 i;
    if (ref->find && !ref->port[0].comm && !ref->port[1].comm &&
        !ref->port[2].comm && !ref->port[3].comm)
    {
        u32 localAddress;
        const u32 remoteAddress = ProtoAdvtLocate(ref->find, "GmUtil", ref->addr, &localAddress, 0);
        if (remoteAddress != 0)
        {
            char connectAddress[256];
            s32 connection;
            NetPrintfCode("netgameutil: located peer=%08x, host=%08x\n", remoteAddress, localAddress);
            if (remoteAddress > localAddress)
            {
                ref->peerip = localAddress;
                ref->hostip = remoteAddress;
                ds_snzprintf(connectAddress, sizeof(connectAddress), "%d.%d.%d.%d%s",
                             remoteAddress >> 24, (remoteAddress >> 16) & 0xff,
                             (remoteAddress >> 8) & 0xff, remoteAddress & 0xff, ref->addr);
                connection = ref->proto | 2;
            }
            else
            {
                ref->hostip = localAddress;
                ref->peerip = remoteAddress;
                ds_strnzcpy(connectAddress, ref->addr, sizeof(connectAddress));
                connection = ref->proto | 1;
            }
            NetGameUtilConnect(ref, connection, connectAddress);
        }
    }

    now = 0;
    winner = 0;
    for (; winner < 4; ++winner)
    {
        if (!ref->port[winner].comm)
            continue;
        if (now == 0)
            now = ref->port[winner].comm->Tick(ref->port[winner].comm);
        if (ref->port[winner].comm->Status(ref->port[winner].comm) != 3)
            continue;
        if (ref->port[winner].tick == 0)
            ref->port[winner].tick = now;
        if (now >= ref->port[winner].tick + ref->port[winner].wait)
            break;
    }
    if (winner == 4)
        return 0;

    comm = ref->port[winner].comm;
    ref->hostip = comm->hostip;
    ref->hostport = comm->hostport;
    ref->peerip = comm->peerip;
    ref->peerport = comm->peerport;
    NetPrintfCode("netgameutil: connection complete\n");

    for (i = 0; i < 4; ++i)
    {
        if (i != winner && ref->port[i].comm)
        {
            ref->port[i].comm->Destroy(ref->port[i].comm);
            ref->port[i].comm = 0;
        }
    }
    if (ref->advt)
    {
        ProtoAdvtDestroy(ref->advt);
        ref->advt = 0;
    }
    if (ref->find)
    {
        ProtoAdvtDestroy(ref->find);
        ref->find = 0;
    }
    return ref->port[winner].comm;
}

// ARTIST 0x82B2FBD8
s32 NetGameUtilStatus(NetGameUtilRefT* ref, s32 selector, void* pBuffer, s32 size)
{
    switch (selector)
    {
    case 0x686f7374: return ref->hosting == 0; // host
    case 0x6a6f696e: return ref->hosting == 1; // join
    case 0x686f6970: return (s32)ref->hostip;   // hoip
    case 0x68707274: return (s32)ref->hostport; // hprt
    case 0x70656970: return (s32)ref->peerip;   // peip
    case 0x70707274: return (s32)ref->peerport; // pprt
    case 0x736f636b: // sock
        if (size == sizeof(ref->pSocket))
        {
            memcpy(pBuffer, &ref->pSocket, sizeof(ref->pSocket));
            return sizeof(ref->pSocket);
        }
        break;
    }
    return -1;
}
