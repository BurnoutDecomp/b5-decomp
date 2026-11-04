#include "netgameutil.h"
#include "commall_x360_internal.h"
#include "dirtylib.h"
#include "dirtymem.h"
#include "platform.h"
#include "protoadvt.h"
#include <cstddef>
#include <cstring>

struct CommUDPRef;
struct CommSRPRef;
struct CommTCPRef;
extern "C" CommUDPRef* CommUDPConstruct(s32 maxWidth, s32 maxInput, s32 maxOutput);
extern "C" CommSRPRef* CommSRPConstruct(s32 maxWidth, s32 maxInput, s32 maxOutput);
extern "C" CommTCPRef* CommTCPConstruct(s32 maxWidth, s32 maxInput, s32 maxOutput);
extern "C" char* ds_strnzcpy(char* pDest, const char* pSource, s32 size);

struct NetGameUtilRefT
{
    struct Port
    {
        CommRef* comm;
        u32 tick;
        u32 wait;
    };

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
    Port port[4];
};

CGS_STATIC_ASSERT(sizeof(NetGameUtilRefT) == 0x114, "ARTIST NetGameUtil allocation");
CGS_STATIC_ASSERT(offsetof(NetGameUtilRefT, advt) == 0xA8, "ARTIST advertisement");
CGS_STATIC_ASSERT(offsetof(NetGameUtilRefT, find) == 0xAC, "ARTIST discovery");
CGS_STATIC_ASSERT(offsetof(NetGameUtilRefT, proto) == 0xC8, "ARTIST protocol selector");
CGS_STATIC_ASSERT(offsetof(NetGameUtilRefT, port) == 0xE4, "ARTIST transport ports");

namespace
{
    const s32 NETGAMEUTIL_MEMID = 0x6e677574;
}

// ARTIST 0x82B2FCC0
extern "C" NetGameUtilRefT* NetGameUtilCreate()
{
    const s32 group = DirtyMemGroupQuery();
    NetGameUtilRefT* ref = static_cast<NetGameUtilRefT*>(
        DirtyMemAlloc(sizeof(NetGameUtilRefT), NETGAMEUTIL_MEMID, group));
    if (!ref)
    {
        NetPrintfCode("netgameutil: unable to allocate module state\n");
        return 0;
    }

    std::memset(ref, 0, sizeof(*ref));
    ref->memgroup = group;
    ref->maxwid = 252;
    NetPrintfCode("netgameutil: setting mwid=%d\n", 252);
    ref->maxinp = 32;
    NetPrintfCode("netgameutil: setting minp=%d\n", 32);
    ref->maxout = 32;
    NetPrintfCode("netgameutil: setting mout=%d\n", 32);
    return ref;
}

// ARTIST 0x82B2F5A8
extern "C" void NetGameUtilReset(NetGameUtilRefT* ref)
{
    for (s32 i = 0; i < 4; ++i)
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

// ARTIST 0x82B2FD68
extern "C" void NetGameUtilDestroy(NetGameUtilRefT* ref)
{
    NetGameUtilReset(ref);
    DirtyMemFree(ref, NETGAMEUTIL_MEMID, ref->memgroup);
}

// ARTIST 0x82B2F620
extern "C" void NetGameUtilControl(NetGameUtilRefT* ref, s32 selector, s32 value)
{
    switch (selector)
    {
    case 0x636c6964: // clid
        ref->clientid = value;
        NetPrintfCode("netgameutil: setting clid=0x%08x\n", static_cast<u32>(value));
        break;
    case 0x72636964: // rcid
        ref->rclientid = value;
        NetPrintfCode("netgameutil: setting rcid=0x%08x\n", static_cast<u32>(value));
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

// ARTIST 0x82B2FBD8
extern "C" s32 NetGameUtilStatus(NetGameUtilRefT* ref, s32 selector, void* pBuffer, s32 size)
{
    switch (selector)
    {
    case 0x686f7374: return ref->hosting == 0; // host
    case 0x6a6f696e: return ref->hosting == 1; // join
    case 0x686f6970: return static_cast<s32>(ref->hostip);   // hoip
    case 0x68707274: return static_cast<s32>(ref->hostport); // hprt
    case 0x70656970: return static_cast<s32>(ref->peerip);   // peip
    case 0x70707274: return static_cast<s32>(ref->peerport); // pprt
    case 0x736f636b: // sock
        if (size == sizeof(ref->pSocket))
        {
            std::memcpy(pBuffer, &ref->pSocket, sizeof(ref->pSocket));
            return sizeof(ref->pSocket);
        }
        break;
    }
    return -1;
}

// ARTIST 0x82B2F720
extern "C" s32 NetGameUtilConnect(NetGameUtilRefT* ref, s32 connection, const char* address)
{
    const s32 mode = connection & 3;
    if (mode == 0 || (connection & 0x700) == 0)
    {
        NetPrintfCode("netgameutil: invalid conn param\n");
        return -1;
    }

    std::strcpy(ref->addr, address);
    ref->hosting = (connection >> 1) & 1;
    ref->proto = connection & ~3u;
    NetPrintfCode("netgameutil: connect %d %s\n", connection, address);

    for (s32 i = 0; i < 4; ++i)
    {
        NetGameUtilRefT::Port& port = ref->port[i];
        if (port.comm)
            port.comm->Destroy(port.comm);
        port.comm = 0;
        port.tick = 0;
    }

    if (mode == 3)
    {
        if (!ref->find)
        {
            DirtyMemGroupEnter(ref->memgroup);
            ProtoAdvtRefT* find = ProtoAdvtConstruct(8);
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
        ref->port[0].comm = reinterpret_cast<CommRef*>(
            CommUDPConstruct(ref->maxwid, ref->maxinp, ref->maxout));
        ref->port[0].wait = 0;
    }
    else if (connection & 0x400)
    {
        ref->port[1].comm = reinterpret_cast<CommRef*>(
            CommSRPConstruct(ref->maxwid, ref->maxinp, ref->maxout));
        ref->port[1].wait = 0;
    }
    else if (connection & 0x200)
    {
        ref->port[2].comm = reinterpret_cast<CommRef*>(
            CommTCPConstruct(ref->maxwid, ref->maxinp, ref->maxout));
        ref->port[2].wait = (ref->port[0].comm || ref->port[1].comm) ? 5000 : 0;
    }

    s32 result = 0;
    for (s32 i = 0; i < 4; ++i)
    {
        NetGameUtilRefT::Port& port = ref->port[i];
        if (!port.comm)
            continue;
        if (port.comm->Control)
        {
            port.comm->Control(port.comm, 0x636c6964, ref->clientid, 0); // clid
            port.comm->Control(port.comm, 0x72636964, ref->rclientid, 0); // rcid
        }
        if (mode & 2)
            result = port.comm->Connect(port.comm, address);
        else if (mode & 1)
            result = port.comm->Listen(port.comm, address);
        ref->pSocket = port.comm->sockptr;
        break;
    }
    DirtyMemGroupLeave();
    return result;
}

// ARTIST 0x82B2F9A0
extern "C" void* NetGameUtilComplete(NetGameUtilRefT* ref)
{
    if (ref->find && !ref->port[0].comm && !ref->port[1].comm &&
        !ref->port[2].comm && !ref->port[3].comm)
    {
        u32 localAddress;
        const u32 remoteAddress = ProtoAdvtLocate(ref->find, "GmUtil", ref->addr, &localAddress, 0);
        if (remoteAddress != 0)
        {
            NetPrintfCode("netgameutil: located peer=%08x, host=%08x\n", remoteAddress, localAddress);
            char connectAddress[256];
            s32 connection;
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

    u32 now = 0;
    s32 winner = 0;
    for (; winner < 4; ++winner)
    {
        NetGameUtilRefT::Port& port = ref->port[winner];
        if (!port.comm)
            continue;
        if (now == 0)
            now = port.comm->Tick(port.comm);
        if (port.comm->Status(port.comm) != 3)
            continue;
        if (port.tick == 0)
            port.tick = now;
        if (now >= port.tick + port.wait)
            break;
    }
    if (winner == 4)
        return 0;

    CommRef* comm = ref->port[winner].comm;
    ref->hostip = comm->hostip;
    ref->hostport = comm->hostport;
    ref->peerip = comm->peerip;
    ref->peerport = comm->peerport;
    NetPrintfCode("netgameutil: connection complete\n");

    for (s32 i = 0; i < 4; ++i)
    {
        NetGameUtilRefT::Port& port = ref->port[i];
        if (i != winner && port.comm)
        {
            port.comm->Destroy(port.comm);
            port.comm = 0;
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
