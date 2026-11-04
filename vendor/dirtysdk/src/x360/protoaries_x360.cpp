// DirtySDK 5.5.3 ProtoAries native X360 owner. ARTIST 82B29DC8..
// 82B2AAxx; Update 82B2A0A0 recovered directly from image instructions.
#include "types.hpp"
#include "dirtymem.h"
#include "dirtylib.h"
#include "cryptstp1.h"
#include <xtl.h>
#include <stddef.h>
#include <string.h>

struct ProtoSSLRefT;
struct ProtoAriesRefT
{
    ProtoSSLRefT* ssl; s32 memGroup; sockaddr_in peer;
    u32 localAddr; s32 localPort; char host[64];
    s32 state; u32 retryTick; s32 inputMax, outputLen, outputOffset, outputMax;
    u8* output; s32 inputLen, inputOffset; u8* input; u8 header[12];
    s16 secure, haveWallet; u8 wallet[84]; CryptStp1T crypt;
    u8 buffers[4]; // first input byte at 0x2F8; allocation contains two variable buffers
};
CGS_STATIC_ASSERT((sizeof(CryptStp1T) == 524), "ARTIST STP1 state");
CGS_STATIC_ASSERT((offsetof(ProtoAriesRefT, state) == 96), "ARTIST Aries state");
CGS_STATIC_ASSERT((offsetof(ProtoAriesRefT, crypt) == 236), "ARTIST Aries cipher");
CGS_STATIC_ASSERT((offsetof(ProtoAriesRefT, buffers) == 760), "ARTIST Aries buffers");
CGS_STATIC_ASSERT((sizeof(ProtoAriesRefT) == 764), "ARTIST Aries allocation header");

extern "C" {
ProtoSSLRefT* ProtoSSLCreate(void);
void ProtoSSLDestroy(ProtoSSLRefT*);
ProtoSSLRefT* ProtoSSLAccept(ProtoSSLRefT*, s32, sockaddr*, s32*);
s32 ProtoSSLConnect(ProtoSSLRefT*, s32, const char*, u32, s32);
s32 ProtoSSLStat(ProtoSSLRefT*, s32, void*, s32);
void ProtoSSLUpdate(ProtoSSLRefT*);
s32 ProtoSSLSend(ProtoSSLRefT*, const char*, s32);
s32 ProtoSSLRecv(ProtoSSLRefT*, char*, s32);
u16 SocketHtons(u16); u32 SocketHtonl(u32); u16 SocketNtohs(u16); u32 SocketNtohl(u32);
u32 NetTick(void);
char* ds_strnzcpy(char*, const char*, s32);
void ProtoAriesUpdate(ProtoAriesRefT*);
s32 ProtoAriesSend(ProtoAriesRefT*, s32, s32, const char*, s32);
}
static const s32 ARIES_MEM_ID = 0x70617269;
static u32 Read32(const u8* p) { return (u32(p[0])<<24) | (u32(p[1])<<16) | (u32(p[2])<<8) | p[3]; }
static void Write32(u8* p, u32 v) { p[0]=u8(v>>24); p[1]=u8(v>>16); p[2]=u8(v>>8); p[3]=u8(v); }

extern "C" ProtoAriesRefT* ProtoAriesCreate(s32 size)
{
    s32 group = DirtyMemGroupQuery();
    ProtoAriesRefT* ref = static_cast<ProtoAriesRefT*>(DirtyMemAlloc(2 * (size + 382), ARIES_MEM_ID, group));
    if (!ref) { NetPrintfCode("protoaries: could not allocate module state\n"); return 0; }
    memset(ref, 0, 764);
    ref->memGroup = group; ref->inputMax = size; ref->outputMax = size;
    ref->input = ref->buffers; ref->output = ref->buffers + size;
    ref->ssl = 0; ref->state = 0; ref->haveWallet = 0;
    return ref;
}
extern "C" void ProtoAriesDestroy(ProtoAriesRefT* ref)
{
    if (ref->ssl) ProtoSSLDestroy(ref->ssl);
    DirtyMemFree(ref, ARIES_MEM_ID, ref->memGroup);
}
extern "C" s32 ProtoAriesUnconnect(ProtoAriesRefT* ref)
{
    if (ref->ssl) { ProtoSSLDestroy(ref->ssl); ref->ssl = 0; }
    ref->outputLen = ref->outputOffset = 0; ref->state = 4; return 0;
}
extern "C" void ProtoAriesSecure(ProtoAriesRefT* ref, s32 secure) { ref->secure = secure == 1; }
extern "C" void ProtoAriesSetKey(ProtoAriesRefT* ref, const void* key, u32 size)
{
    if (key && size == 84) { ref->haveWallet = 1; memcpy(ref->wallet, key, 84); }
    else { ref->haveWallet = 0; CryptStp1UseSecret(&ref->crypt, 0); }
}
extern "C" s32 ProtoAriesStatus(ProtoAriesRefT* ref, s32 select, void* buffer, s32 length)
{
    switch (select)
    {
    case 0x61646472: return SocketNtohl(ref->peer.sin_addr.s_addr);
    case 0x63727970: return ref->crypt.enabled;
    case 0x69627566: return ref->input ? ref->inputLen - ref->inputOffset : 0;
    case 0x6c616472: return ref->localAddr;
    case 0x6c707274: return ref->localPort;
    case 0x6f627566: return ref->outputLen - ref->outputOffset;
    case 0x706f7274: return SocketNtohs(ref->peer.sin_port);
    case 0x73656375: return ref->secure;
    case 0x73746174: return ref->state;
    default: return ref->ssl ? ProtoSSLStat(ref->ssl, select, buffer, length) : -1;
    }
}

static void AriesConnectedRecord(ProtoAriesRefT* ref, u8 code)
{
    ref->inputLen = ref->inputOffset = 12;
    Write32(ref->input, 0xffffffff); Write32(ref->input + 4, code == 0xfe ? 0xfefefefe : 0xffffffff);
    Write32(ref->input + 8, 12);
}
extern "C" void ProtoAriesUpdate(ProtoAriesRefT* ref)
{
    if (ref->state == 3 && !ref->ssl) { AriesConnectedRecord(ref, 0xfe); ref->state = 4; }
    if (ref->state == 4 && ref->outputLen > 0) { ref->retryTick = 0; ref->state = 2; }
    if (ref->state == 1)
    {
        sockaddr address; s32 size = 16;
        ProtoSSLRefT* accepted = ProtoSSLAccept(ref->ssl, 0, &address, &size);
        if (!accepted) return;
        ProtoSSLDestroy(ref->ssl); ref->ssl = accepted; ref->state = 3; ref->outputOffset = 0;
        AriesConnectedRecord(ref, 0xff);
    }
    if (ref->state == 2)
    {
        if (NetTick() > ref->retryTick)
        {
            if (ref->ssl) ProtoSSLDestroy(ref->ssl);
            DirtyMemGroupEnter(ref->memGroup); ref->ssl = ProtoSSLCreate(); DirtyMemGroupLeave();
            if (!ref->ssl) return;
            if (ProtoSSLConnect(ref->ssl, ref->secure != 0, ref->host,
                SocketNtohl(ref->peer.sin_addr.s_addr), SocketNtohs(ref->peer.sin_port)) < 0) return;
            ref->retryTick = NetTick() + 30000;
            CryptStp1UseSecret(&ref->crypt, 0);
        }
        if (!ref->ssl) return;
        ProtoSSLUpdate(ref->ssl);
        s32 status = ProtoSSLStat(ref->ssl, 0x73746174, 0, 0);
        if (status < 0)
        {
            NetPrintfCode("protoaries: socket disconnection during connect\n");
            ProtoAriesUnconnect(ref); return;
        }
        if (!status) return;
        if (!ref->peer.sin_addr.s_addr) ProtoSSLStat(ref->ssl, 0x70656572, &ref->peer, 16);
        sockaddr_in local;
        if (!ProtoSSLStat(ref->ssl, 0x62696e64, &local, 16))
        {
            ref->localAddr = SocketNtohl(local.sin_addr.s_addr);
            ref->localPort = SocketNtohs(local.sin_port);
        }
        ref->state = 3; ref->outputOffset = 0; AriesConnectedRecord(ref, 0xff);
    }
    if (!ref->ssl) return;
    ProtoSSLUpdate(ref->ssl);
    if (ref->outputLen > 0 && ref->outputOffset != ref->outputLen)
    {
        s32 sent = ProtoSSLSend(ref->ssl, reinterpret_cast<char*>(ref->output + ref->outputOffset), ref->outputLen - ref->outputOffset);
        if (sent < 0) { ProtoSSLDestroy(ref->ssl); ref->ssl = 0; return; }
        if (sent > 0) ref->outputOffset += sent;
        if (ref->outputOffset == ref->outputLen) ref->outputOffset = ref->outputLen = 0;
    }
    if (!ref->inputLen)
    {
        s32 received = ProtoSSLRecv(ref->ssl, reinterpret_cast<char*>(ref->header + ref->inputOffset), 12 - ref->inputOffset);
        if (received < 0) { ProtoSSLDestroy(ref->ssl); ref->ssl = 0; return; }
        if (received > 0) ref->inputOffset += received;
        if (ref->inputOffset == 12)
        {
            CryptStp1DecryptData(&ref->crypt, ref->header, 12);
            ref->inputLen = static_cast<s32>(Read32(ref->header + 8));
            if (ref->inputLen < 12 || ref->inputLen > ref->inputMax)
            {
                NetPrintfCode("protoaries: recv error -- invalid header\n");
                ProtoSSLDestroy(ref->ssl); ref->ssl = 0; return;
            }
            memcpy(ref->input, ref->header, ref->inputOffset);
            if (ref->inputOffset == ref->inputLen && ref->outputLen > 0 && ref->outputOffset == ref->outputLen)
                ref->outputOffset = ref->outputLen = 0;
        }
    }
    if (ref->inputLen > 0 && ref->inputOffset < ref->inputLen)
    {
        s32 received = ProtoSSLRecv(ref->ssl, reinterpret_cast<char*>(ref->input + ref->inputOffset), ref->inputLen - ref->inputOffset);
        if (received < 0) { ProtoSSLDestroy(ref->ssl); ref->ssl = 0; return; }
        if (received > 0) ref->inputOffset += received;
        s32 compareResult = received;
        if (ref->inputOffset == ref->inputLen)
        {
            CryptStp1DecryptData(&ref->crypt, ref->input + 12, ref->inputOffset - 12);
            if (CryptStp1DecryptHash(&ref->crypt, ref->input, ref->inputLen) < 0)
            { ProtoSSLDestroy(ref->ssl); ref->ssl = 0; return; }
            compareResult = CryptStp1DecryptSize(&ref->crypt, ref->inputLen);
            ref->inputLen = ref->inputOffset = compareResult;
        }
        if (compareResult == ref->inputLen && ref->outputLen > 0 && ref->outputOffset == ref->outputLen)
            ref->outputOffset = ref->outputLen = 0;
    }
}extern "C" s32 ProtoAriesSend(ProtoAriesRefT* ref, s32 kind, s32 code, const char* data, s32 length)
{
    ProtoAriesUpdate(ref);
    if (length < 0) length = static_cast<s32>(strlen(data)) + 1;
    const s32 packetSize = CryptStp1EncryptSize(&ref->crypt, length + 12);
    if (ref->outputLen)
    {
        s32 position = 0, packet = 0;
        while (position < ref->outputOffset)
        {
            packet = static_cast<s32>(Read32(ref->output + position + 8));
            position += packet;
        }
        if (position > ref->outputOffset) position -= packet;
        if (position > 0)
        {
            memcpy(ref->output, ref->output + position, ref->outputLen - position);
            ref->outputLen -= position; ref->outputOffset -= position;
        }
        if (ref->outputMax - ref->outputLen < packetSize)
        {
            NetPrintfCode("protoaries: output buffer overflow\n");
            NetPrintfCode("protoaries: send buffer overflow\n");
            return -1;
        }
    }
    else
    {
        ref->outputOffset = 0;
        if (ref->outputMax < packetSize) { NetPrintfCode("protoaries: send buffer overflow\n"); return -1; }
    }
    u8* packet = ref->output + ref->outputLen;
    if (length > 0) memcpy(packet + 12, data, length);
    ref->outputLen += packetSize;
    Write32(packet, kind); Write32(packet + 4, code); Write32(packet + 8, packetSize);
    CryptStp1EncryptHash(&ref->crypt, packet, packetSize);
    CryptStp1EncryptData(&ref->crypt, packet, packetSize);
    if (kind == 0x40746963) ref->haveWallet = 0;
    ProtoAriesUpdate(ref);
    return 0;
}

extern "C" s32 ProtoAriesPeek(ProtoAriesRefT* ref, s32* kind, s32* code, char** body)
{
    if (!(ref->inputLen && ref->inputOffset == ref->inputLen))
    {
        if (!(kind || code || body)) return -1;
        ProtoAriesUpdate(ref);
        if (!ref->inputLen || ref->inputOffset != ref->inputLen) return -1;
    }
    const s32 size = ref->inputLen - 12;
    char* data = reinterpret_cast<char*>(ref->input + 12);
    if (size > 0) data[size] = 0;
    else { static char empty[1]; data = empty; }
    if (kind) *kind = Read32(ref->input);
    if (code) *code = Read32(ref->input + 4);
    if (body) *body = data;
    return size;
}
extern "C" s32 ProtoAriesRecv(ProtoAriesRefT* ref, s32* kind, s32* code, char* data, s32 length)
{
    s32 received = ProtoAriesPeek(ref, kind, code, 0);
    if (received >= 0)
    {
        if (length > received) length = received;
        if (data) memcpy(data, ref->input + 12, length);
        ref->inputLen = ref->inputOffset = 0;
    }
    return received;
}
extern "C" s32 ProtoAriesConnect(ProtoAriesRefT* ref, const char* host, u32 address, s32 port)
{
    if (ref->ssl) return -1;
    ref->inputLen = ref->inputOffset = 0;
    memset(&ref->peer, 0, 16);
    ref->peer.sin_family = AF_INET;
    ref->peer.sin_addr.s_addr = SocketHtonl(address);
    ref->peer.sin_port = SocketHtons(static_cast<u16>(port));
    if (host) ds_strnzcpy(ref->host, host, sizeof(ref->host));
    else memset(ref->host, 0, sizeof(ref->host));
    ref->state = 2; ref->retryTick = 0; ref->secure = 0;
    if (ref->haveWallet)
    {
        u8 secret[32], ticket[52];
        if (CryptStp1OpenWallet(ref->wallet, secret, ticket) <= 0) ref->haveWallet = 0;
        else
        {
            ProtoAriesSend(ref, 0x3f746963, 0, reinterpret_cast<const char*>(ticket), 52);
            CryptStp1UseSecret(&ref->crypt, secret);
        }
    }
    return 0;
}
