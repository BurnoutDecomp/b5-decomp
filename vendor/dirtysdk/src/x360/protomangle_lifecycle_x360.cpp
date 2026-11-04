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
    const char* DirtyErrGetName(u32);
    s32 NetConnQuery(const char*, const void*);
}

struct ProtoMangleRefT
{
    XSESSION_INFO SessionInfo;                   // 000
    u8 pad03c[4];
    union { u8 aNonce[8]; ULONGLONG uNonce; };  // 040
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
    s32 iGameMode;                          // 0a4
    u32 uSessionFlags;                           // 0a8
    u32 uSessionLiveFlags;                       // 0ac
    s32 iPublicSlots;                            // 0b0
    s32 iPrivateSlots;                           // 0b4
    s32 iTimeout;                                // 0b8
    s32 iArbitrationAttempts;                   // 0bc
    s32 iExpectedRegistrants;                   // 0c0
    u32 uNextArbitrationTick;                   // 0c4
    DWORD uArbitrationSize;                       // 0c8
    XSESSION_REGISTRATION_RESULTS* pArbitrationResults; // 0cc
    DWORD uSearchBufferSize;                       // 0d0
    XSESSION_SEARCHRESULT_HEADER* pSearchResults;// 0d4
    XSESSION_VIEW_PROPERTIES SkillView;         // 0d8
    u32 uPad0e4;
    XUSER_PROPERTY SkillProperties[2];          // 0e8
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
CGS_STATIC_ASSERT((offsetof(ProtoMangleRefT, iArbitrationAttempts) == 0xbc), "ARTIST arbitration attempts");
CGS_STATIC_ASSERT((offsetof(ProtoMangleRefT, iExpectedRegistrants) == 0xc0), "ARTIST registrant target");
CGS_STATIC_ASSERT((offsetof(ProtoMangleRefT, uNextArbitrationTick) == 0xc4), "ARTIST retry tick");
CGS_STATIC_ASSERT((offsetof(ProtoMangleRefT, uArbitrationSize) == 0xc8), "ARTIST arbitration size");
CGS_STATIC_ASSERT((offsetof(ProtoMangleRefT, pArbitrationResults) == 0xcc), "ARTIST arbitration results");
CGS_STATIC_ASSERT((offsetof(ProtoMangleRefT, uSearchBufferSize) == 0xd0), "ARTIST search size");
CGS_STATIC_ASSERT((offsetof(ProtoMangleRefT, pSearchResults) == 0xd4), "ARTIST search results");
CGS_STATIC_ASSERT((offsetof(ProtoMangleRefT, SkillView) == 0xd8), "ARTIST skill view");
CGS_STATIC_ASSERT((offsetof(ProtoMangleRefT, SkillProperties) == 0xe8), "ARTIST skill properties");
CGS_STATIC_ASSERT((offsetof(ProtoMangleRefT, strSession) == 0x118), "ARTIST session string");
CGS_STATIC_ASSERT((offsetof(ProtoMangleRefT, strPeerAddress) == 0x318), "ARTIST peer address");
CGS_STATIC_ASSERT((offsetof(ProtoMangleRefT, aFlags) == 0x358), "ARTIST async state bytes");
CGS_STATIC_ASSERT((sizeof(XUSER_PROPERTY) == 24), "ARTIST skill property stride");
CGS_STATIC_ASSERT((offsetof(XUSER_PROPERTY, value) + offsetof(XUSER_DATA, nData) == 16), "ARTIST skill value offset");
CGS_STATIC_ASSERT((sizeof(XUSER_CONTEXT) == 8), "ARTIST search context stride");
CGS_STATIC_ASSERT((sizeof(XSESSION_REGISTRANT) == 24), "ARTIST registrant stride");
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
    const u8* DecodeAddress(void* output, s32 count, const u8* text)
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
        return text;
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
    if (ref->pArbitrationResults) DirtyMemFree(ref->pArbitrationResults, 0x706d676c, ref->iMemGroup);
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

namespace
{
    // Persistent argument storage used by the original asynchronous XSession calls.
    DWORD gLocalUserIndex;
    BOOL gLocalPrivateSlot;
    XUID gLeaveUser;
    XUSER_CONTEXT gSearchContexts[2];
    XUID gInviteUser;
    BOOL gRemotePrivateSlot;

    void DecodeSession(ProtoMangleRefT* ref, const char* session)
    {
        const u8* p = reinterpret_cast<const u8*>(session + 1);
        p = DecodeAddress(&ref->SessionInfo.hostAddress, 36, p);
        p = DecodeAddress(&ref->SessionInfo.keyExchangeKey, 16, p);
        DecodeAddress(&ref->SessionInfo.sessionID, 8, p);
        NetPrintMemCode(&ref->SessionInfo.hostAddress, 36, "hostAddress");
        NetPrintMemCode(&ref->SessionInfo.keyExchangeKey, 16, "keyExchangeKey");
        NetPrintMemCode(&ref->SessionInfo.sessionID, 8, "sessionID");
    }

    // ARTIST 82B16758. Pending is state-specific; 996 and 997 both count as pending.
    void StartResult(ProtoMangleRefT* ref, DWORD result, s32 success, s32 pending, s32 failure, const char* name)
    {
        if (result == 0)
        {
            NetPrintfCode("protomanglexenon: %s successful\n", name);
            ref->eState = success;
        }
        else if (result == 997 || result == 996)
        {
            NetPrintfCode("protomanglexenon: %s pending\n", name);
            ref->eState = pending;
        }
        else
        {
            NetPrintfCode("protomanglexenon: %s failed: %s\n", name, DirtyErrGetName(result));
            ref->eState = failure;
        }
        ref->aFlags[0] = ref->eState == pending;
    }

    // ARTIST 82B16808. InternalLow is checked before querying extended error.
    DWORD PollResult(ProtoMangleRefT* ref, s32 success, s32 failure, const char* name)
    {
        if (ref->Overlapped.InternalLow == 997) return 997;
        DWORD result = XGetOverlappedExtendedError(&ref->Overlapped);
        if (result == 997 || result == 996) return result;
        if (result)
        {
            NetPrintfCode("protomanglexenon: %s overlapped execution failed: %s\n", name, DirtyErrGetName(result));
            ref->eState = failure;
        }
        else
        {
            NetPrintfCode("protomanglexenon: %s successful\n", name);
            ref->eState = success;
        }
        ref->aFlags[0] = 0;
        return result;
    }

    // ARTIST 82B17548. Zero timeout selects the instance timeout.
    void WaitForAsync(ProtoMangleRefT* ref, u32 timeout)
    {
        if (ref->eState && ref->eState != 17)
        {
            if (!timeout) timeout = ref->iTimeout;
            NetPrintfCode("protomanglexenon: blocking on async op %d for up to %dms\n", ref->eState, timeout);
            const u32 deadline = NetTick() + timeout;
            while (ref->eState && ref->eState != 17 && static_cast<s32>(deadline - NetTick()) > 0)
            {
                ProtoMangleUpdate(ref);
                NetConnSleep(1);
            }
            NetPrintfCode("protomanglexenon: blocked %d mSec\n", NetTick() + timeout - deadline);
        }
    }

    s32 CreateSession(ProtoMangleRefT* ref, s32 arbitration, const char* session)
    {
        if (ref->hSession)
        {
            NetPrintfCode("protomanglexenon: attempt to create session when session already exists\n");
            return -1;
        }
        u32 flags = ref->uSessionFlags;
        if (arbitration) { flags |= 0x10; ref->aFlags[3] = 1; }
        else ref->aFlags[3] = 0;
        if (ref->xuidCreate == ref->xuidSelf) { flags |= 1; ref->aFlags[2] = 1; }
        else
        {
            ds_strnzcpy(ref->strSession, session, 256);
            DecodeSession(ref, ref->strSession);
            ref->aFlags[2] = 0;
        }
        const s32 user = NetConnQuery(0, &ref->xuidSelf);
        if (user < 0)
        {
            NetPrintfCode("protomanglexenon: unable to dereference user index for session creation - user not currently logged in?\n");
            ref->eState = 17;
        }
        else
        {
            XUserSetContext(user, 0x800a, arbitration == 0);
            XUserSetContext(user, 0x800b, ref->iGameMode);
            std::memset(&ref->Overlapped, 0, sizeof(ref->Overlapped));
            DWORD result = XSessionCreate(flags, user, ref->iPublicSlots, ref->iPrivateSlots,
                &ref->uNonce, &ref->SessionInfo, &ref->Overlapped, &ref->hSession);
            StartResult(ref, result, 3, 1, 17, "XSessionCreate()");
            ref->uSessionLiveFlags = flags;
        }
        return ref->eState == 17 ? -1 : 0;
    }

    s32 SearchSessions(ProtoMangleRefT* ref, s32 count, s32 procedure)
    {
        const s32 user = NetConnQuery(0, &ref->xuidSelf);
        if (user < 0)
        {
            NetPrintfCode("protomanglexenon: unable to dereference user index for session search - user not currently logged in?\n");
            ref->eState = 17;
            return -1;
        }
        DWORD size = 0;
        DWORD result = XSessionSearch(procedure, user, count, 0, 0, 0, 0, &size, 0, 0);
        if (result != 122 || !size)
        {
            NetPrintfCode("protomanglexenon: XSessionSearch() failed: %s\n", DirtyErrGetName(result));
            ref->eState = 17;
            return -1;
        }
        if (size != ref->uSearchBufferSize)
        {
            if (ref->pSearchResults)
            {
                NetPrintfCode("protomanglexenon: freeing old search buffer (size=%d)\n", ref->uSearchBufferSize);
                DirtyMemFree(ref->pSearchResults, 0x706d676c, ref->iMemGroup);
            }
            NetPrintfCode("protomanglexenon: allocating new search buffer (size=%d)\n", size);
            ref->pSearchResults = static_cast<XSESSION_SEARCHRESULT_HEADER*>(DirtyMemAlloc(size, 0x706d676c, ref->iMemGroup));
            ref->uSearchBufferSize = size;
        }
        std::memset(ref->pSearchResults, 0, ref->uSearchBufferSize);
        gSearchContexts[0].dwContextId = 0x800a;
        gSearchContexts[0].dwValue = ref->aFlags[3] == 0;
        gSearchContexts[1].dwContextId = 0x800b;
        gSearchContexts[1].dwValue = ref->iGameMode;
        result = XSessionSearch(procedure, user, count, 0, 2, 0, gSearchContexts,
                                &ref->uSearchBufferSize, ref->pSearchResults, &ref->Overlapped);
        StartResult(ref, result, 0, 2, 17, "XSessionSearch()");
        return ref->eState == 17 ? -1 : 0;
    }

    void MigrateSession(ProtoMangleRefT* ref, s32 makeHost, const char* session)
    {
        if (!makeHost)
        {
            ds_strnzcpy(ref->strSession, session, 256);
            DecodeSession(ref, ref->strSession);
            const DWORD result = XSessionMigrateHost(ref->hSession, 0xfe, &ref->SessionInfo, &ref->Overlapped);
            StartResult(ref, result, 0, 11, 17, "XSessionMigrateHost()");
            ref->aFlags[2] = 0;
            return;
        }
        const s32 user = NetConnQuery(0, &ref->xuidSelf);
        if (user < 0)
        {
            NetPrintfCode("protomanglexenon: unable to dereference user index for host migration - user not currently logged in?\n");
            ref->eState = 17;
            return;
        }
        NetPrintfCode("protomanglexenon: migrating host - setting parameters to null\n");
        std::memset(&ref->SessionInfo, 0, sizeof(ref->SessionInfo));
        std::memset(&ref->Overlapped, 0, sizeof(ref->Overlapped));
        const DWORD result = XSessionMigrateHost(ref->hSession, user, &ref->SessionInfo, &ref->Overlapped);
        StartResult(ref, result, 13, 12, 17, "XSessionMigrateHost()");
        NetPrintfCode("protomanglexenon: migrating host for session 0x%08x\n", ref->hSession);
        ref->aFlags[2] = 1;
    }
}

namespace
{
    // ARTIST 82B16D38. Arbitration uses XDK result buffers and may retry as host.
    void UpdateArbitration(ProtoMangleRefT* ref)
    {
        if (ref->aFlags[0])
        {
            if (ref->aFlags[0] == 1) PollResult(ref, 0, 17, "XSessionArbitrationRegister()");
        }
        else if (ref->iArbitrationAttempts > 0 && NetTick() >= ref->uNextArbitrationTick)
        {
            DWORD size = 0;
            DWORD result = XSessionArbitrationRegister(ref->hSession, 0, ref->uNonce, &size, 0, 0);
            if (result != 122)
            {
                NetPrintfCode("protomanglexenon: XSessionArbitrationRegister() failed (err=%s)\n", DirtyErrGetName(result));
                ref->eState = 17;
            }
            if (size != ref->uArbitrationSize)
            {
                if (ref->pArbitrationResults)
                {
                    NetPrintfCode("protomanglexenon: freeing old arbitration buffer (size=%d)\n", ref->uArbitrationSize);
                    DirtyMemFree(ref->pArbitrationResults, 0x706d676c, ref->iMemGroup);
                }
                NetPrintfCode("protomanglexenon: allocating new arbitration buffer (size=%d)\n", size);
                ref->pArbitrationResults = static_cast<XSESSION_REGISTRATION_RESULTS*>(DirtyMemAlloc(size, 0x706d676c, ref->iMemGroup));
                ref->uArbitrationSize = size;
            }
            std::memset(&ref->Overlapped, 0, sizeof(ref->Overlapped));
            result = XSessionArbitrationRegister(ref->hSession, 0, ref->uNonce,
                &ref->uArbitrationSize, ref->pArbitrationResults, &ref->Overlapped);
            NetPrintfCode("protomanglexenon: XSessionArbitrationRegister()=%s\n", DirtyErrGetName(result));
            StartResult(ref, result, 0, 8, 17, "XSessionArbitrationRegister()");
            --ref->iArbitrationAttempts;
        }
        if (!ref->eState)
        {
            NetPrintfCode("protomanglexenon: wNumRegistrants==%d\n", ref->pArbitrationResults->wNumRegistrants);
            for (s32 i = 0; i < static_cast<s32>(ref->pArbitrationResults->wNumRegistrants); ++i)
            {
                XSESSION_REGISTRANT* registrant = &ref->pArbitrationResults->rgRegistrants[i];
                DirtyAddrT addr;
                DirtyAddrFromHostAddr(&addr, &registrant->qwMachineID);
                NetPrintfCode("protomanglexenon: client %d: nusers=%d, trust=%d, machineid=%s\n", i,
                    registrant->bNumUsers, registrant->bTrustworthiness, addr.strMachineAddr);
                for (s32 j = 0; j < static_cast<s32>(registrant->bNumUsers); ++j)
                {
                    DirtyAddrFromHostAddr(&addr, &registrant->rgUsers[j]);
                    NetPrintfCode("protomanglexenon: user %d) %s\n", j, addr.strMachineAddr);
                }
            }
            if (ref->aFlags[2] && static_cast<s32>(ref->pArbitrationResults->wNumRegistrants) < ref->iExpectedRegistrants && ref->iArbitrationAttempts > 0)
            {
                ref->eState = 8;
                ref->uNextArbitrationTick = NetTick() + 1000;
                NetPrintfCode("protomanglexenon: host attempting another arbitration\n");
            }
            if (!ref->eState)
            {
                std::memset(&ref->Overlapped, 0, sizeof(ref->Overlapped));
                const DWORD result = XSessionStart(ref->hSession, 0, &ref->Overlapped);
                StartResult(ref, result, 0, 9, 17, "XSessionStart()");
            }
        }
    }

    // ARTIST 82B30398; 4-byte UTF-8 sequences are consumed without output.
    void DecodeInviteText(u16* output, s32 capacity, const char* input)
    {
        const u8* source = reinterpret_cast<const u8*>(input);
        s32 pos = 0, count = 0;
        while (source[pos] && count < capacity - 1)
        {
            const u32 c = source[pos];
            if ((c & 0x80) == 0) { output[count++] = static_cast<u16>(c); ++pos; }
            else if ((c & 0xe0) == 0xc0)
            {
                output[count++] = static_cast<u16>(((c << 6) & 0xc7c0) | (source[pos+1] & 0x3f));
                pos += 2;
            }
            else if ((c & 0xf0) == 0xe0)
            {
                output[count++] = static_cast<u16>((c << 12) | ((source[pos+1] << 6) & 0xcfff) | (source[pos+2] & 0x3f));
                pos += 3;
            }
            else pos += 4;
        }
        output[count] = 0;
    }
}

// ARTIST 82B170F0. Separate checks are intentional: a completion can enter
// another state and have that state processed in the same update.
extern "C" void ProtoMangleUpdate(ProtoMangleRefT* ref)
{
    if (ref->eState == 1) PollResult(ref, 3, 17, "XSessionCreate()");
    if (ref->eState == 11 || ref->eState == 12)
        PollResult(ref, ref->eState == 11 ? 0 : 13, 17, "XSessionMigrateHost()");
    if (ref->eState == 3)
    {
        if (ref->aFlags[2])
        {
            ProtoMangleEncodeSession(ref->strSession, 256, &ref->SessionInfo);
            NetPrintfCode("protomanglexenon: session=%s (len=%d)\n", ref->strSession, std::strlen(ref->strSession));
        }
        gLocalUserIndex = NetConnQuery(0, &ref->xuidSelf);
        std::memset(&ref->Overlapped, 0, sizeof(ref->Overlapped));
        gLocalPrivateSlot = ref->iPrivateSlots != 0;
        const DWORD result = XSessionJoinLocal(ref->hSession, 1, &gLocalUserIndex, &gLocalPrivateSlot, &ref->Overlapped);
        StartResult(ref, result, 0, 5, 17, "XSessionJoinLocal()");
    }
    if (ref->eState == 13)
    {
        if (ref->aFlags[2])
        {
            ProtoMangleEncodeSession(ref->strSession, 256, &ref->SessionInfo);
            NetPrintfCode("protomanglexenon: session=%s (len=%d)\n", ref->strSession, std::strlen(ref->strSession));
        }
        ref->eState = 0;
    }
    if (ref->eState == 4) PollResult(ref, 0, 17, "XSessionModify()");
    if (ref->eState == 2) PollResult(ref, 0, 17, "XSessionSearch()");
    if (ref->eState == 5) PollResult(ref, 0, 17, "XSessionJoin()");
    if (ref->eState == 7) PollResult(ref, 0, 17, "XSessionInvite()");
    if (ref->eState == 6) PollResult(ref, 0, 17, "XSessionLeaveRemote()");
    if (ref->eState == 10) PollResult(ref, 0, 17, "XSessionWriteStats()");
    if (ref->eState == 8) UpdateArbitration(ref);
    if (ref->eState == 9 && PollResult(ref, 0, 17, "XSessionStart()") == 0) ref->aFlags[4] = 1;
    if (ref->eState == 14 && PollResult(ref, 0, 17, "XSessionEnd()") != 997)
    {
        ref->aFlags[4] = 0;
        if (ref->aFlags[1] == 1)
        {
            ProtoMangleControl(ref, 0x64656c65, 0, 0, 0);
            ref->aFlags[1] = 0;
        }
    }
    if (ref->eState == 15 && PollResult(ref, 0, 17, "XSessionDelete()") != 997)
    {
        ref->aFlags[4] = 0;
        ref->hSession = 0;
    }
}

// ARTIST 82B176F0. The synchronous selectors precede the wait; async selectors
// wait for the existing operation before consulting or changing session state.
extern "C" s32 ProtoMangleControl(ProtoMangleRefT* ref, s32 select, s32 value, s32 value2, const void* data)
{
    const DirtyAddrT* addr = static_cast<const DirtyAddrT*>(data);
    switch (select)
    {
    case 0x686f7374: DirtyAddrToHostAddr(&ref->xuidCreate, 8, addr); return 0; // host
    case 0x73656c66: DirtyAddrToHostAddr(&ref->xuidSelf, 8, addr); return 0; // self
    case 0x73756964: DirtyAddrToHostAddr(&ref->xuidSession, 8, addr); return 0; // suid
    case 0x69747874: DecodeInviteText(reinterpret_cast<u16*>(ref->strInviteMessage), 256, static_cast<const char*>(data)); return 0;
    case 0x6d6f6465: ref->iGameMode = value; return 0;
    case 0x6e6f6e63:
        DecodeAddress(ref->aNonce, 8, static_cast<const u8*>(data));
        NetPrintMemCode(ref->aNonce, 8, "sessionNonce");
        return 0;
    case 0x736c6f74:
        NetPrintfCode("protomanglexenon: setting slot count (pub=%d, prv=%d)\n", value, value2);
        ref->iPublicSlots = value; ref->iPrivateSlots = value2; return 0;
    case 0x74696d65: ref->iTimeout = value; return 0;
    case 0x73666c67:
        if (!ref->hSession)
        {
            NetPrintfCode("protomanglexenon: setting session flags to 0x%08x\n", value);
            ref->uSessionFlags = value;
            return 0;
        }
        break;
    }
    WaitForAsync(ref, 0);
    if (select == 0x73657373) return CreateSession(ref, value, static_cast<const char*>(data));
    if (select == 0x73726368) return SearchSessions(ref, value, value2);
    if (!ref->hSession)
    {
        NetPrintfCode("protomanglexenon: can't initiate async op without a session\n");
        return -1;
    }
    DWORD result;
    switch (select)
    {
    case 0x636f6e6e: // conn
        ds_strnzcpy(ref->strPeerAddress, static_cast<const char*>(data), 64);
        DirtyAddrToHostAddr(&ref->xuidPeer, 8, addr);
        ref->iPeerPort = value;
        NetPrintfCode("protomanglexenon: adding remote player %s\n", static_cast<const char*>(data));
        std::memset(&ref->Overlapped, 0, sizeof(ref->Overlapped));
        gRemotePrivateSlot = ref->iPrivateSlots != 0;
        result = XSessionJoinRemote(ref->hSession, 1, &ref->xuidPeer, &gRemotePrivateSlot, &ref->Overlapped);
        StartResult(ref, result, 0, 5, 17, "XSessionJoinRemote()");
        return 0;
    case 0x64656c65: // dele
        if (ref->aFlags[4] == 1)
        {
            ProtoMangleControl(ref, 0x73746f70, 0, 0, 0);
            if (ref->eState == 14) ref->aFlags[1] = 1;
            return 0;
        }
        std::memset(&ref->Overlapped, 0, sizeof(ref->Overlapped));
        result = XSessionDelete(ref->hSession, &ref->Overlapped);
        StartResult(ref, result, 0, 15, 17, "XSessionDelete()");
        return 0;
    case 0x696e7674: // invt
        if (ref->aFlags[2])
        {
            const s32 user = NetConnQuery(0, &ref->xuidSelf);
            if (user < 0)
            {
                NetPrintfCode("protomanglexenon: unable to dereference user index for invite - user not currently logged in?\n");
                ref->eState = 17;
                return 0;
            }
            DirtyAddrToHostAddr(&gInviteUser, 8, addr);
            std::memset(&ref->Overlapped, 0, sizeof(ref->Overlapped));
            result = XInviteSend(user, 1, &gInviteUser, ref->strInviteMessage, &ref->Overlapped);
            StartResult(ref, result, 0, 7, 17, "XSessionInvite()");
            return 0;
        }
        NetPrintfCode("protomanglexenon: can't invite when not hosting\n");
        return -1;
    case 0x6d696772: // migr
        MigrateSession(ref, value, static_cast<const char*>(data));
        return 0;
    case 0x72656d76: // remv: native path does not clear Overlapped here.
        DirtyAddrToHostAddr(&gLeaveUser, 8, addr);
        result = XSessionLeaveRemote(ref->hSession, 1, &gLeaveUser, &ref->Overlapped);
        StartResult(ref, result, 0, 6, 17, "XSessionLeaveRemote()");
        return 0;
    case 0x73666c67: // sflg, modify an existing session
        result = XSessionModify(ref->hSession, (value & 0xf00) | (ref->uSessionLiveFlags & ~0xf00u),
            ref->iPublicSlots, ref->iPrivateSlots, &ref->Overlapped);
        StartResult(ref, result, 0, 4, 17, "XSessionModify()");
        return 0;
    case 0x736b696c: // skil, build the exact XDK view/property records
        ref->SkillProperties[0].dwPropertyId = 0x1000800a;
        ref->SkillProperties[0].value.type = 1;
        ref->SkillProperties[0].value.nData = value2;
        ref->SkillProperties[1].dwPropertyId = 0x1000800b;
        ref->SkillProperties[1].value.type = 1;
        ref->SkillProperties[1].value.nData = value;
        ref->SkillView.dwViewId = 0xffff0000;
        ref->SkillView.dwNumProperties = 2;
        ref->SkillView.pProperties = ref->SkillProperties;
        NetPrintfCode("protomanglexenon: skill leaderboard: user %d score %d\n", value, value2);
        value = 1;
        data = &ref->SkillView;
        // fall through to the common stat write
    case 0x73746174:
        std::memset(&ref->Overlapped, 0, sizeof(ref->Overlapped));
        result = XSessionWriteStats(ref->hSession, ref->xuidSession, value,
            static_cast<const XSESSION_VIEW_PROPERTIES*>(data), &ref->Overlapped);
        StartResult(ref, result, 0, 10, 17, "XSessionWriteStats()");
        return 0;
    case 0x73746f70: // stop
        if (ref->aFlags[4] != 1) return 0;
        if (NetConnStatus(0x6f6e6c6e, 0, 0, 0) != 1)
        {
            NetPrintfCode("protomangle: offline -- skipping XSessionEnd()\n");
            ref->aFlags[4] = 0;
            return 0;
        }
        std::memset(&ref->Overlapped, 0, sizeof(ref->Overlapped));
        result = XSessionEnd(ref->hSession, &ref->Overlapped);
        StartResult(ref, result, 14, 14, 17, "XSessionEnd()");
        return 0;
    case 0x73747274: // strt
        if (!ref->aFlags[3])
        {
            std::memset(&ref->Overlapped, 0, sizeof(ref->Overlapped));
            result = XSessionStart(ref->hSession, 0, &ref->Overlapped);
            StartResult(ref, result, 0, 9, 17, "XSessionStart()");
        }
        else
        {
            ref->iExpectedRegistrants = value;
            ref->iArbitrationAttempts = ref->aFlags[2] ? 5 : 1;
            ref->uNextArbitrationTick = NetTick();
            ref->eState = 8;
        }
        return 0;
    default: return -1;
    }
}
