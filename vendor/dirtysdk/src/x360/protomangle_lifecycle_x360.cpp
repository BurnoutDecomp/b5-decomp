// Native X360 ProtoMangle lifecycle, completion and query selectors.
// ARTIST 82B17020, 82B17428, 82B175E8, 82B17D30 and 82B17F58.
#include "types.hpp"
#include "dirtyaddr.h"
#include "dirtymem.h"
#include "dirtylib.h"
#include "netconn.h"
#include "protomangle.h"
#include <xtl.h>
#include <cstddef>
#include <cstring>

extern "C" {
    u32 NetTick(void);
    void NetConnSleep(s32);
    void ProtoMangleUpdate(struct ProtoMangleRefT*);
    s32 ProtoMangleControl(struct ProtoMangleRefT*, s32, s32, s32, const void*);
    u32 SocketHtonl(u32);
    const char* SocketInAddrGetText(u32);
    char* ds_strnzcpy(char*, const char*, s32);
    void NetPrintMemCode(const void*, s32, const char*);
}

struct ProtoMangleRefT
{
    XSESSION_INFO SessionInfo;                   // 000
    u8 pad03c[4];
    u8 aNonce[8];                                // 040
    XOVERLAPPED Overlapped;                      // 048
    HANDLE hSession;                             // 064
    HANDLE hNotify;                              // 068
    s32 iMemGroup;                               // 06c
    s32 eState;                                  // 070
    u32 uUnknown074;
    XUID xuidCreate;                             // 078
    XUID xuidPeer;                               // 080
    XUID xuidSelf;                               // 088
    XUID xuidSession;                            // 090
    u32 uUnknown098;
    s32 iPeerPort;                               // 09c
    u32 uUnknown0a0;
    s32 iMangleEnabled;                          // 0a4
    u32 uSessionFlags;                           // 0a8
    u32 uSessionLiveFlags;                       // 0ac
    s32 iPublicSlots;                            // 0b0
    s32 iPrivateSlots;                           // 0b4
    s32 iTimeout;                                // 0b8
    u8 pad0bc[16];
    void* pSearchBuffer;                         // 0cc
    u32 uSearchBufferSize;                       // 0d0
    XSESSION_SEARCHRESULT_HEADER* pSearchResults;// 0d4
    u8 pad0d8[64];
    char strSession[256];                        // 118
    WCHAR strInviteMessage[128];                 // 218
    char strPeerAddress[64];                     // 318
    u8 aFlags[8];                                // 358
};
CGS_STATIC_ASSERT((sizeof(ProtoMangleRefT) == 0x360), "ARTIST ProtoMangle size");
CGS_STATIC_ASSERT((offsetof(ProtoMangleRefT, aNonce) == 0x40), "ARTIST nonce");
CGS_STATIC_ASSERT((offsetof(ProtoMangleRefT, Overlapped) == 0x48), "ARTIST overlapped");
CGS_STATIC_ASSERT((offsetof(ProtoMangleRefT, hSession) == 0x64), "ARTIST session handle");
CGS_STATIC_ASSERT((offsetof(ProtoMangleRefT, eState) == 0x70), "ARTIST state");
CGS_STATIC_ASSERT((offsetof(ProtoMangleRefT, xuidCreate) == 0x78), "ARTIST XUID");
CGS_STATIC_ASSERT((offsetof(ProtoMangleRefT, iPeerPort) == 0x9c), "ARTIST peer port");
CGS_STATIC_ASSERT((offsetof(ProtoMangleRefT, uSessionFlags) == 0xa8), "ARTIST session flags");
CGS_STATIC_ASSERT((offsetof(ProtoMangleRefT, pSearchResults) == 0xd4), "ARTIST search results");
CGS_STATIC_ASSERT((offsetof(ProtoMangleRefT, strSession) == 0x118), "ARTIST session string");
CGS_STATIC_ASSERT((offsetof(ProtoMangleRefT, strPeerAddress) == 0x318), "ARTIST peer address");
CGS_STATIC_ASSERT((sizeof(XSESSION_SEARCHRESULT) == 92), "ARTIST search result stride");
CGS_STATIC_ASSERT((offsetof(XSESSION_SEARCHRESULT, info) + offsetof(XSESSION_INFO, hostAddress) + offsetof(XNADDR, abEnet) == 18), "ARTIST search Ethernet offset");
CGS_STATIC_ASSERT((offsetof(XINVITE_INFO, hostInfo) == 20), "ARTIST invite session offset");

namespace
{
    // ARTIST 82B164B0, reused for the 'nonc' selector.
    void EncodeNonce(char* out, s32 size, const u8* input)
    {
        if (size < 11)
        {
            NetPrintfCode("protomanglexenon: not enough room to encode system link advert\n");
            return;
        }
        u32 bits = 0;
        s32 count = 0;
        for (s32 i = 0; i < 8; ++i)
        {
            bits |= static_cast<u32>(input[i]) << count;
            count += 8;
            while (count >= 7)
            {
                *out++ = static_cast<char>((bits & 0x7f) | 0x80);
                bits >>= 7;
                count -= 7;
            }
        }
        if (count) *out++ = static_cast<char>((bits & 0x7f) | 0x80);
        *out = 0;
    }

    // ARTIST 82B16590: decode 7-bit text into the native XNADDR.
    void DecodeAddress(void* output, s32 count, const u8* text)
    {
        u8* out = static_cast<u8*>(output);
        u32 bits = 0;
        s32 used = 0;
        while ((*text & 0x80) && count > 0)
        {
            bits |= static_cast<u32>(*text++ & 0x7f) << used;
            used += 7;
            if (used >= 8)
            {
                *out++ = static_cast<u8>(bits);
                bits >>= 8;
                used -= 8;
                --count;
            }
        }
    }
}

// r3 and r5 are DirtyAddrT pointers; r4/r6 are retained API inputs but unused here.
extern "C" ProtoMangleRefT* ProtoMangleCreate(const char* pCreate, s32, const char* pSelf, const char*)
{
    const s32 group = DirtyMemGroupQuery();
    ProtoMangleRefT* ref = static_cast<ProtoMangleRefT*>(DirtyMemAlloc(sizeof(*ref), 0x706d676c, group));
    if (!ref) return 0;
    std::memset(ref, 0, sizeof(*ref));
    ref->iMemGroup = group;
    if (pCreate) DirtyAddrToHostAddr(&ref->xuidCreate, 8, reinterpret_cast<const DirtyAddrT*>(pCreate));
    if (pSelf) DirtyAddrToHostAddr(&ref->xuidSelf, 8, reinterpret_cast<const DirtyAddrT*>(pSelf));
    ref->uSessionFlags = 1070;
    ref->iPublicSlots = 2;
    ref->iPrivateSlots = 0;
    ref->iTimeout = 30000;
    ref->hNotify = XNotifyCreateListener(2);
    if (!ref->hNotify) NetPrintfCode("protomanglexenon: unable to create live event listener\n");
    return ref;
}

extern "C" void ProtoMangleDestroy(ProtoMangleRefT* ref)
{
    NetPrintfCode("protomanglexenon: shutting down\n");
    if (ref->eState != 0 && ref->eState != 17)
    {
        NetPrintfCode("protomanglexenon: blocking on async op %d for up to %dms\n", ref->eState, 5000);
        const u32 deadline = NetTick() + 5000;
        while (ref->eState != 0 && ref->eState != 17 && static_cast<s32>(deadline - NetTick()) > 0)
        {
            ProtoMangleUpdate(ref);
            NetConnSleep(1);
        }
        NetPrintfCode("protomanglexenon: blocked %d mSec\n", NetTick() + 5000 - deadline);
    }
    if (ref->hNotify) CloseHandle(ref->hNotify);
    if (ref->pSearchBuffer) DirtyMemFree(ref->pSearchBuffer, 0x706d676c, ref->iMemGroup);
    if (ref->pSearchResults) DirtyMemFree(ref->pSearchResults, 0x706d676c, ref->iMemGroup);
    if (ref->hSession) XSessionDelete(ref->hSession, 0);
    DirtyMemFree(ref, 0x706d676c, ref->iMemGroup);
}

// ARTIST 82B17D30: status returns the live X360 session state, not a NAT estimate.
extern "C" s32 ProtoMangleStatus(ProtoMangleRefT* ref, s32 select, void* buffer, s32 size)
{
    if (select == 0x6661696c) return ref->eState == 17; // fail
    if (select == 0x69646c65) return ref->eState == 0;  // idle
    if (select == 0x696e7674) // invt
    {
        XINVITE_INFO invite;
        s32 user = 0;
        while (NetConnStatus(select, user, &invite, 84) <= 0)
            if (++user >= 4) return 0;
        if (buffer) ProtoMangleEncodeSession(static_cast<char*>(buffer), size, &invite.hostInfo);
        return 1;
    }
    if (select == 0x6e6f6e63 || select == 0x73657373) // nonc, sess
    {
        if ((ref->eState >= 1 && ref->eState <= 5) || (ref->eState >= 11 && ref->eState <= 13)) return 0;
        if (ref->eState == 17 || ref->strSession[0] != '$') return -1;
        if (select == 0x73657373)
            ds_strnzcpy(static_cast<char*>(buffer), ref->strSession, size);
        else
        {
            NetPrintMemCode(ref->aNonce, 8, "sessionNonce");
            EncodeNonce(static_cast<char*>(buffer), size, ref->aNonce);
        }
        return 1;
    }
    if (select == 0x73666c67) return ref->uSessionFlags; // sflg
    if (select == 0x73726368) // srch
    {
        if (ref->eState == 2) return -1;
        s32 count = -2;
        if (ref->pSearchResults)
        {
            count = ref->pSearchResults->dwSearchResults;
            if (count > 0 && buffer)
            {
                u8* output = static_cast<u8*>(buffer);
                s32 i = 0;
                while (i < count && size >= 6)
                {
                    std::memcpy(output, ref->pSearchResults->pResults[i].info.hostAddress.abEnet, 6);
                    ++i; output += 6; size -= 6;
                }
                count = i;
            }
        }
        return count;
    }
    return -1;
}

extern "C" s32 ProtoMangleComplete(ProtoMangleRefT* ref, s32* address, s32* port)
{
    if (ref->eState != 0) return ref->eState == 5 ? 0 : -1;
    const char* pEncoded = std::strchr(ref->strPeerAddress, '^');
    if (!pEncoded)
    {
        NetPrintfCode("protomanglexenon: no xnaddr specified with peer address - cannot connect\n");
        return -1;
    }
    XNADDR peer;
    DecodeAddress(&peer, 36, reinterpret_cast<const u8*>(pEncoded + 1));
    NetPrintfCode("protomanglexenon: XnAddr.ina=%s\n", SocketInAddrGetText(peer.ina.s_addr));
    NetPrintfCode("protomanglexenon: XnAddr.inaOnline=%s\n", SocketInAddrGetText(peer.inaOnline.s_addr));
    NetPrintfCode("protomanglexenon: XnAddr.wPortOnline=%d\n", peer.wPortOnline);
    IN_ADDR secure;
    const s32 result = XNetXnAddrToInAddr(&peer, &ref->SessionInfo.sessionID, &secure);
    if (result)
    {
        NetPrintfCode("protomanglexenon: XNetXnAddrToInAddr() = 0x%08x\n", result);
        return -1;
    }
    NetPrintfCode("protomanglexenon: secure addr=%s\n", SocketInAddrGetText(secure.s_addr));
    *address = static_cast<s32>(SocketHtonl(secure.s_addr));
    if (port) *port = ref->iPeerPort;
    return 1;
}

extern "C" void ProtoMangleConnect(ProtoMangleRefT* ref, s32 port, const char* peer)
{
    ProtoMangleControl(ref, 0x636f6e6e, port, 0, peer);
}
