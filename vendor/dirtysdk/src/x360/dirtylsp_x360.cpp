// Native DirtySDK/Xenon title-server lookup, ARTIST 82B3AE78..82B3B1F8.
#include "dirtylsp.h"
#include "dirtymem.h"
#include "dirtylib.h"
#include "netconn.h"
#include <xtl.h>
#include <cstddef>
#include <cstring>

extern "C" const char* DirtyErrGetName(u32 uError);
extern "C" const char* SocketInAddrGetText(u32 uAddr);
extern "C" u32 SocketNtohl(u32 uAddr);

// The module's first word is its allocation group. ARTIST allocates/zeros 0x1544
// bytes; the remaining module fields are not consumed by these five entry points.
struct DirtyLSPRefT
{
    s32 iMemGroup;
    u8 aUnrecovered[0x1540];
};
CGS_STATIC_ASSERT((sizeof(DirtyLSPRefT) == 0x1544), "ARTIST LSP module allocation");
struct DirtyLSPQueryT
{
    HANDLE hEnumerator;
    XOVERLAPPED Overlapped;
    XTITLE_SERVER_INFO aServers[1];
};
CGS_STATIC_ASSERT((offsetof(HostentT, pDns) == 0x50), "ARTIST Hostent DNS");
CGS_STATIC_ASSERT((offsetof(HostentT, pPlatformData) == 0x54), "ARTIST Hostent platform data");
CGS_STATIC_ASSERT((sizeof(HostentT) == 0x5c), "ARTIST Hostent size");
CGS_STATIC_ASSERT((offsetof(DirtyLSPQueryT, Overlapped) == 4), "ARTIST LSP overlapped");
CGS_STATIC_ASSERT((offsetof(DirtyLSPQueryT, aServers) == 0x20), "ARTIST LSP server list");
CGS_STATIC_ASSERT((sizeof(XTITLE_SERVER_INFO) == 0xd0), "ARTIST title server stride");
static const s32 kDirtyLSPMemId = 0x646c7370;

extern "C" DirtyLSPRefT* DirtyLSPCreate(void)
{
    const s32 group = DirtyMemGroupQuery();
    DirtyLSPRefT* const ref = static_cast<DirtyLSPRefT*>(DirtyMemAlloc(0x1544, kDirtyLSPMemId, group));
    if (!ref)
    {
        NetPrintfCode("dirtylspxenon: could not allocate module state\n");
        return 0;
    }
    std::memset(ref, 0, 0x1544);
    ref->iMemGroup = group;
    return ref;
}

extern "C" void DirtyLSPDestroy(DirtyLSPRefT* ref)
{
    DirtyMemFree(ref, kDirtyLSPMemId, ref->iMemGroup);
}

extern "C" void DirtyLSPQueryFree(DirtyLSPRefT* ref, HostentT* host)
{
    DirtyLSPQueryT* const query = static_cast<DirtyLSPQueryT*>(host->pPlatformData);
    if (query)
    {
        if (query->hEnumerator)
            CloseHandle(query->hEnumerator);
        DirtyMemFree(query, kDirtyLSPMemId, ref->iMemGroup);
        host->pPlatformData = 0;
    }
}

extern "C" s32 DirtyLSPQueryDone(DirtyLSPRefT*, HostentT* host)
{
    if (host->done != 0)
        return host->done;
    DirtyLSPQueryT* const query = static_cast<DirtyLSPQueryT*>(host->pPlatformData);
    if (query->Overlapped.InternalLow == ERROR_IO_PENDING)
        return host->done;
    host->done = -1;
    host->addr = 0;
    DWORD count;
    if (XGetOverlappedResult(&query->Overlapped, &count, TRUE) != 0)
    {
        NetPrintfCode("dirtylspxenon: XGetOverlappedResult failed (err=%s)\n",
            DirtyErrGetName(XGetOverlappedExtendedError(&query->Overlapped)));
        return host->done;
    }
    if (count == 0)
    {
        NetPrintfCode("dirtylspxenon: title server address query returned no results\n");
        return host->done;
    }
    NetPrintfCode("dirtylspxenon: got %d title service results\n", count);
    for (DWORD i = 0; i < count; ++i)
        NetPrintfCode("dirtylspxenon: [%d] %s\n", i, SocketInAddrGetText(query->aServers[i].inaServer.s_addr));
    DWORD index;
    if (count > 1)
    {
        XNetRandom(reinterpret_cast<BYTE*>(&index), sizeof(index));
        index %= count;
        NetPrintfCode("dirtylspxenon: choosing server %d\n", index);
    }
    else
        index = 0;
    const DWORD service = static_cast<DWORD>(NetConnStatus(0x74737276, 0, 0, 0));
    IN_ADDR address;
    const s32 result = XNetServerToInAddr(query->aServers[index].inaServer, service, &address);
    if (result != 0)
    {
        NetPrintfCode("dirtylspxenon: XNetServerToInAddr failed (err=%s)\n", DirtyErrGetName(result));
        return host->done;
    }
    host->addr = SocketNtohl(address.s_addr);
    host->done = 1;
    NetPrintfCode("dirtylspxenon: resolved secure address %s for name %s\n",
        SocketInAddrGetText(host->addr), host->name);
    return host->done;
}

extern "C" s32 DirtyLSPQueryStart(DirtyLSPRefT* ref, HostentT* host)
{
    DWORD size;
    HANDLE enumerator;
    const DWORD createResult = XTitleServerCreateEnumerator(host->name, 8, &size, &enumerator);
    if (createResult != 0)
    {
        NetPrintfCode("dirtylspxenon: could not create LSP enumerator (err=%s)\n", DirtyErrGetName(createResult));
        return -1;
    }
    DirtyLSPQueryT* const query = static_cast<DirtyLSPQueryT*>(DirtyMemAlloc(size + 0xf0, kDirtyLSPMemId, ref->iMemGroup));
    std::memset(query, 0, size + 0xf0);
    query->hEnumerator = enumerator;
    const DWORD result = XEnumerate(enumerator, query->aServers, size, 0, &query->Overlapped);
    if (result != ERROR_IO_PENDING)
    {
        NetPrintfCode("dirtylspxenon: could not enumerate LSP servers (err=%s)\n", DirtyErrGetName(result));
        // ARTIST calls QueryFree before assigning this query to host->pPlatformData.
        DirtyLSPQueryFree(ref, host);
        return -2;
    }
    host->pPlatformData = query;
    return 0;
}
