// DirtySDK 5.5.3 core/source/proto/protoaries.c native X360 owner. ARTIST 82B29DC8..
// 82B2AAxx; Update 82B2A0A0 recovered directly from image instructions.
// XDK size optimization matches ARTIST at 0x82B29DC8 and 0x82B29E60.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

#include "types.hpp"
#include "dirtymem.h"
#include "dirtylib.h"
#include "cryptstp1.h"
// ProtoAries only needs the XDK socket types.
#define NOD3D
#include <xtl.h>
#undef NOD3D
#include <stddef.h>
#include <string.h>

typedef struct ProtoSSLRefT ProtoSSLRefT;
typedef struct ProtoAriesRefT ProtoAriesRefT;
// The on-wire header consists of three byte arrays (DirtySDK protoaries.h:68).
typedef struct ProtoAriesMsgHdrT
{
    u8 kind[4];
    u8 code[4];
    u8 size[4];
} ProtoAriesMsgHdrT;
CGS_STATIC_ASSERT((sizeof(ProtoAriesMsgHdrT) == 12), "ARTIST Aries message header");

struct ProtoAriesRefT
{
    ProtoSSLRefT* ssl; s32 memGroup; struct sockaddr_in peer;
    u32 localAddr, localPort; char host[64];
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

ProtoSSLRefT* ProtoSSLCreate(void);
void ProtoSSLDestroy(ProtoSSLRefT*);
ProtoSSLRefT* ProtoSSLAccept(ProtoSSLRefT*, s32, struct sockaddr*, s32*);
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
static const s32 ARIES_MEM_ID = 0x70617269;
static u32 Read32(const u8* bytes)
{
    return (((((u32)bytes[0] << 8) | bytes[1]) << 8 | bytes[2]) << 8) | bytes[3];
}
static void Write32(u8* p, u32 v) { p[0]=(u8)(v>>24); p[1]=(u8)(v>>16); p[2]=(u8)(v>>8); p[3]=(u8)(v); }

static s32 _ProtoAriesBuff(ProtoAriesRefT* ref, s32 iSize)
{
    ProtoAriesMsgHdrT* pHdr;
    s32 iDone, iStep;

    if (!ref->outputLen)
    {
        ref->outputOffset = 0;
        return ref->outputMax;
    }
    iDone = iStep = 0;
    while (iDone < ref->outputOffset)
    {
        pHdr = (ProtoAriesMsgHdrT*)(ref->output + iDone);
        iStep = (s32)Read32(pHdr->size);
        iDone += iStep;
    }
    if (iDone > ref->outputOffset)
        iDone -= iStep;
    if (iDone > 0)
    {
        memcpy(ref->output, ref->output + iDone, ref->outputLen - iDone);
        ref->outputLen -= iDone;
        ref->outputOffset -= iDone;
    }
    if (ref->outputMax - ref->outputLen < iSize)
    {
        NetPrintfCode("protoaries: output buffer overflow\n");
        return -1;
    }
    return ref->outputMax - ref->outputLen;
}

ProtoAriesRefT* ProtoAriesCreate(s32 size)
{
    s32 group = DirtyMemGroupQuery();
    ProtoAriesRefT* ref = (ProtoAriesRefT*)(DirtyMemAlloc(2 * (size + 382), ARIES_MEM_ID, group));
    if (!ref) { NetPrintfCode("protoaries: could not allocate module state\n"); return 0; }
    memset(ref, 0, 764);
    ref->memGroup = group; ref->inputMax = size; ref->outputMax = size;
    ref->input = ref->buffers; ref->output = ref->buffers + size;
    ref->ssl = 0; ref->state = 0; ref->haveWallet = 0;
    return ref;
}
void ProtoAriesDestroy(ProtoAriesRefT* ref)
{
    if (ref->ssl) ProtoSSLDestroy(ref->ssl);
    DirtyMemFree(ref, ARIES_MEM_ID, ref->memGroup);
}
s32 ProtoAriesUnconnect(ProtoAriesRefT* ref)
{
    if (ref->ssl) { ProtoSSLDestroy(ref->ssl); ref->ssl = 0; }
    ref->outputLen = 0; ref->outputOffset = 0; ref->state = 4; return 0;   // ARTIST stores +0x6c, +0x70, +0x60
}
void ProtoAriesSecure(ProtoAriesRefT* ref, s32 secure) { ref->secure = secure == 1; }
void ProtoAriesSetKey(ProtoAriesRefT* ref, const void* key, u32 size)
{
    // ARTIST 0x82B2A064..0x82B2A074 tests the size for zero before comparing it with the wallet size.
    if (key && size > 0 && size == 84) { ref->haveWallet = 1; memcpy(ref->wallet, key, 84); }
    else { ref->haveWallet = 0; CryptStp1UseSecret(&ref->crypt, 0); }
}
s32 ProtoAriesStatus(ProtoAriesRefT* ref, s32 select, void* buffer, s32 length)
{
    s32 size, offset;

    if (select == 0x61646472) return SocketNtohl(ref->peer.sin_addr.s_addr);
    if (select == 0x63727970) return ref->crypt.enabled;
    if (select == 0x69627566)
    {
        if (ref->input)
        {
            size = ref->inputLen;
            offset = ref->inputOffset;
            goto bufferSize;
        }
        return 0;
    }
    if (select == 0x6c616472) return ref->localAddr;
    if (select == 0x6c707274) return ref->localPort;
    if (select == 0x6f627566)
    {
        size = ref->outputLen;
        offset = ref->outputOffset;
    bufferSize:
        return size - offset;
    }
    if (select == 0x706f7274) return SocketNtohs(ref->peer.sin_port);
    if (select == 0x73656375) return ref->secure;
    if (select == 0x73746174) return ref->state;
    if (ref->ssl) return ProtoSSLStat(ref->ssl, select, buffer, length);
    return -1;
}

static void AriesConnectedRecord(ProtoAriesRefT* ref, u8 code)
{
    ref->inputLen = ref->inputOffset = 12;
    Write32(ref->input, 0xffffffff); Write32(ref->input + 4, code == 0xfe ? 0xfefefefe : 0xffffffff);
    Write32(ref->input + 8, 12);
}
void ProtoAriesUpdate(ProtoAriesRefT* ref)
{
    if (ref->state == 3 && !ref->ssl) { AriesConnectedRecord(ref, 0xfe); ref->state = 4; }
    if (ref->state == 4 && ref->outputLen > 0) { ref->retryTick = 0; ref->state = 2; }
    if (ref->state == 1)
    {
        struct sockaddr address; s32 size = 16;
        ProtoSSLRefT* accepted = ProtoSSLAccept(ref->ssl, 0, &address, &size);
        if (!accepted) return;
        ProtoSSLDestroy(ref->ssl); ref->ssl = accepted; ref->state = 3; ref->outputOffset = 0;
        AriesConnectedRecord(ref, 0xff);
    }
    if (ref->state == 2)
    {
        s32 status;
        struct sockaddr_in local;
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
        status = ProtoSSLStat(ref->ssl, 0x73746174, 0, 0);
        if (status < 0)
        {
            NetPrintfCode("protoaries: socket disconnection during connect\n");
            ProtoAriesUnconnect(ref); return;
        }
        if (!status) return;
        if (!ref->peer.sin_addr.s_addr) ProtoSSLStat(ref->ssl, 0x70656572, &ref->peer, 16);
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
        s32 sent = ProtoSSLSend(ref->ssl, (char*)(ref->output + ref->outputOffset), ref->outputLen - ref->outputOffset);
        if (sent < 0) { ProtoSSLDestroy(ref->ssl); ref->ssl = 0; return; }
        if (sent > 0) ref->outputOffset += sent;
        if (ref->outputOffset == ref->outputLen) ref->outputOffset = ref->outputLen = 0;
    }
    if (!ref->inputLen)
    {
        s32 received = ProtoSSLRecv(ref->ssl, (char*)(ref->header + ref->inputOffset), 12 - ref->inputOffset);
        if (received < 0) { ProtoSSLDestroy(ref->ssl); ref->ssl = 0; return; }
        if (received > 0) ref->inputOffset += received;
        if (ref->inputOffset == 12)
        {
            CryptStp1DecryptData(&ref->crypt, ref->header, 12);
            ref->inputLen = (s32)(Read32(ref->header + 8));
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
        s32 received = ProtoSSLRecv(ref->ssl, (char*)(ref->input + ref->inputOffset), ref->inputLen - ref->inputOffset);
        s32 compareResult;
        if (received < 0) { ProtoSSLDestroy(ref->ssl); ref->ssl = 0; return; }
        if (received > 0) ref->inputOffset += received;
        compareResult = received;
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
}

s32 ProtoAriesSend(ProtoAriesRefT* ref, s32 kind, s32 code, const char* data, s32 length)
{
    ProtoAriesMsgHdrT* pHdr;
    u8* pStart;
    s32 iSize;

    ProtoAriesUpdate(ref);
    if (length < 0)
        length = (s32)strlen(data) + 1;
    iSize = CryptStp1EncryptSize(&ref->crypt, length + (s32)sizeof(*pHdr));
    if (_ProtoAriesBuff(ref, iSize) < iSize)
    {
        NetPrintfCode("protoaries: send buffer overflow\n");
        return -1;
    }

    pStart = ref->output + ref->outputLen;
    pHdr = (ProtoAriesMsgHdrT*)pStart;
    if (length > 0)
        memcpy(pHdr + 1, data, length);
    ref->outputLen += iSize;
    pHdr->size[0] = (u8)(iSize >> 24);
    pHdr->size[1] = (u8)(iSize >> 16);
    pHdr->size[2] = (u8)(iSize >> 8);
    pHdr->size[3] = (u8)iSize;
    pHdr->kind[0] = (u8)(kind >> 24);
    pHdr->kind[1] = (u8)(kind >> 16);
    pHdr->kind[2] = (u8)(kind >> 8);
    pHdr->kind[3] = (u8)kind;
    pHdr->code[0] = (u8)(code >> 24);
    pHdr->code[1] = (u8)(code >> 16);
    pHdr->code[2] = (u8)(code >> 8);
    pHdr->code[3] = (u8)code;
    CryptStp1EncryptHash(&ref->crypt, pStart, iSize);
    CryptStp1EncryptData(&ref->crypt, pStart, iSize);
    if (kind == 0x40746963)
        ref->haveWallet = 0;
    ProtoAriesUpdate(ref);
    return 0;
}

s32 ProtoAriesPeek(ProtoAriesRefT* ref, s32* kind, s32* code, char** body)
{
    ProtoAriesMsgHdrT* pHdr;
    char* pData;
    s32 iLen;

    if (!(ref->inputLen && ref->inputOffset == ref->inputLen))
    {
        if (!(kind || code || body))
            return -1;
        ProtoAriesUpdate(ref);
        if (!ref->inputLen || ref->inputOffset != ref->inputLen)
            return -1;
    }
    pHdr = (ProtoAriesMsgHdrT*)ref->input;
    iLen = ref->inputLen - (s32)sizeof(*pHdr);
    pData = (char*)(pHdr + 1);
    if (iLen < 1)
        pData = "";
    else
        pData[iLen] = 0;
    if (kind)
        *kind = Read32(pHdr->kind);
    if (code)
        *code = (((pHdr->code[1] | ((u32)pHdr->code[0] << 8)) << 8 | pHdr->code[2]) << 8) | pHdr->code[3];
    if (body)
        *body = pData;
    return iLen;
}

s32 ProtoAriesRecv(ProtoAriesRefT* ref, s32* kind, s32* code, char* data, s32 length)
{
    s32 received = ProtoAriesPeek(ref, kind, code, 0);
    if (received >= 0)
    {
        if (length > received) length = received;
        if (data) memcpy(data, ref->input + 12, length);
        ref->inputLen = 0; ref->inputOffset = 0;
    }
    return received;
}
s32 ProtoAriesConnect(ProtoAriesRefT* ref, const char* host, u32 address, s32 port)
{
    if (ref->ssl) return -1;
    ref->inputLen = 0; ref->inputOffset = 0;
    memset(&ref->peer, 0, 16);
    ref->peer.sin_family = AF_INET;
    ref->peer.sin_addr.s_addr = SocketHtonl(address);
    ref->peer.sin_port = SocketHtons((u16)(port));
    if (host) ds_strnzcpy(ref->host, host, sizeof(ref->host));
    else memset(ref->host, 0, sizeof(ref->host));
    ref->state = 2; ref->retryTick = 0; ref->secure = 0;
    if (ref->haveWallet)
    {
        u8 secret[32], ticket[52];
        if (CryptStp1OpenWallet(ref->wallet, secret, ticket) > 0)
        {
            ProtoAriesSend(ref, 0x3f746963, 0, (const char*)(ticket), 52);
            CryptStp1UseSecret(&ref->crypt, secret);
        }
        else
        {
            ref->haveWallet = 0;
        }
    }
    return 0;
}
