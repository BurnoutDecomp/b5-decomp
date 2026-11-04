#include "netgameutil.h"
#include "commall_x360_internal.h"
#include "dirtylib.h"
#include "dirtymem.h"
#include <cstddef>
#include <cstring>

struct ProtoAdvtRef;
extern "C" void ProtoAdvtDestroy(ProtoAdvtRef* ref);

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
    ProtoAdvtRef* advt;
    ProtoAdvtRef* find;
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
