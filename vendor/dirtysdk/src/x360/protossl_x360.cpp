// DirtySDK ProtoSSL native object lifecycle and status, ARTIST 82B35758,
// 82B36E58..82B36F9C, 82B37150 and 82B38108. Secure protocol functions
// require the native handshake and crypto providers and are not substituted here.
#include "protossl.h"
#include "protoname.h"
#include "cryptarc4.h"
#include "cryptsha1.h"
#include "cryptrsa.h"
#include "dirtymem.h"
#include "dirtylib.h"
#include <xtl.h>
#include <cstddef>
#include <cstring>
#include <cstdlib>

struct SocketT;
extern "C" SocketT* SocketAccept(SocketT*, sockaddr*, s32*);
extern "C" s32 SocketClose(SocketT*);
extern "C" s32 SocketInfo(SocketT*, s32, s32, void*, s32);
extern "C" SocketT* SocketOpen(s32, s32, s32);
extern "C" HostentT* SocketLookup(const char*, s32);
extern "C" s32 SockaddrInSetAddrText(sockaddr*, const char*);

// Native type shape from DirtySDK 5.5.3 DWARF, gated by ARTIST offsets.
struct CryptMD5T { u8 strData[72]; u32 uCount; u32 uRegs[4]; };
struct CipherSuiteT { u8 uIdent[2], uKey, uEnc, uMac; };
struct X509NameT { char strCountry[32], strState[32], strCity[32], strOrg[32], strUnit[32], strCommon[32]; };
struct X509CertificateT
{
    s32 version;
    X509NameT Issuer, Subject;
    char strGoodFrom[32], strGoodTill[32];
    s32 iSerialSize; u8 SerialData[32];
    s32 iSigType, iSigSize; u8 SigData[128];
    s32 iKeyType; u8 KeyParm[64]; s32 iKeySize; u8 KeyData[256];
    s32 iKeyModSize; u8 KeyModData[129]; s32 iKeyExpSize; u8 KeyExpData[129];
};
struct SecureStateT
{
    u32 uTimer, uSendSeqn;
    s32 iSendProg, iSendSize;
    u32 uRecvSeqn;
    s32 iRecvProg, iRecvSize, iRecvBase;
    const CipherSuiteT* pCipher;
    u8 ClientRandom[32], ServerRandom[32], PreMasterKey[48], MasterKey[48], KeyBlock[128];
    u8 *pServerMAC, *pClientMAC, *pServerKey, *pClientKey;
    CryptMD5T HandshakeMD5;
    CryptSha1T HandshakeSHA;
    CryptArc4T ReadArc4, WriteArc4;
    X509CertificateT Cert;
    u8 SendData[16384], RecvData[16384];
};
CGS_STATIC_ASSERT((offsetof(SecureStateT, HandshakeMD5) == 0x154), "ARTIST MD5 context");
CGS_STATIC_ASSERT((offsetof(SecureStateT, HandshakeSHA) == 0x1b0), "ARTIST SHA1 context");
CGS_STATIC_ASSERT((offsetof(SecureStateT, ReadArc4) == 0x20c), "ARTIST read cipher");
CGS_STATIC_ASSERT((offsetof(SecureStateT, WriteArc4) == 0x30e), "ARTIST write cipher");
CGS_STATIC_ASSERT((offsetof(SecureStateT, SendData) == 0x8d8), "ARTIST send record");
CGS_STATIC_ASSERT((offsetof(SecureStateT, RecvData) == 0x48d8), "ARTIST receive record");
CGS_STATIC_ASSERT((sizeof(SecureStateT) == 0x88d8), "ARTIST secure allocation");
extern "C" void CryptMD5Init(CryptMD5T*);
extern "C" void CryptMD5Update(CryptMD5T*, const void*, s32);
extern "C" void CryptMD5Final(CryptMD5T*, void*, s32);
extern "C" s32 SocketSendto(SocketT*, const char*, s32, s32, const sockaddr*, s32);
extern "C" s32 SocketRecvfrom(SocketT*, char*, s32, s32, sockaddr*, s32*);
// ARTIST 82B356A0: genuine entropy routine; provider remains separately unresolved
// until its stack-sampling contract is reconstructed and verified on PPC.
extern "C" void ProtoSSLGenerateRandom(u8*, s32, CryptArc4T*);

struct ProtoSSLRefT
{
    SocketT* pSock;
    HostentT* pHost;
    s32 iMemGroup;
    char strHost[256];
    sockaddr PeerAddr;
    s32 iState;
    s32 iClosed;
    SecureStateT* pSecure;
};
CGS_STATIC_ASSERT((sizeof(ProtoSSLRefT) == 0x128), "ARTIST ProtoSSL state size");
CGS_STATIC_ASSERT((offsetof(ProtoSSLRefT, PeerAddr) == 0x10c), "ARTIST SSL peer");
CGS_STATIC_ASSERT((offsetof(ProtoSSLRefT, iState) == 0x11c), "ARTIST SSL state");
CGS_STATIC_ASSERT((offsetof(ProtoSSLRefT, pSecure) == 0x124), "ARTIST secure state pointer");
static const s32 kProtoSSLMemId = 0x7073736c;

// ARTIST 82B35758: reset transport, then preserve or allocate real secure state.
static s32 ResetState(ProtoSSLRefT* state, s32 secure)
{
    if (state->pSock)
    {
        SocketClose(state->pSock);
        state->pSock = 0;
    }
    if (state->pHost)
    {
        state->pHost->Free(state->pHost);
        state->pHost = 0;
    }
    state->iState = 0;
    state->iClosed = 1;
    if (secure)
    {
        if (!state->pSecure)
        {
            state->pSecure = static_cast<SecureStateT*>(DirtyMemAlloc(0x88d8, kProtoSSLMemId, state->iMemGroup));
            if (state->pSecure)
                std::memset(state->pSecure, 0, 0x88d8);
        }
    }
    else if (state->pSecure)
    {
        DirtyMemFree(state->pSecure, kProtoSSLMemId, state->iMemGroup);
        state->pSecure = 0;
    }
    SecureStateT* const data = state->pSecure;
    if (data)
    {
        ProtoSSLGenerateRandom(0, 0, &data->ReadArc4);
        data->iSendProg = 0;
        data->iSendSize = 0;
        data->uSendSeqn = 0;
        data->iRecvProg = 0;
        data->iRecvSize = 0;
        data->uRecvSeqn = 0;
        CryptMD5Init(&data->HandshakeMD5);
        CryptSha1Init(&data->HandshakeSHA);
    }
    return secure && !data ? -8 : 0;
}

extern "C" ProtoSSLRefT* ProtoSSLCreate(void)
{
    const s32 group = DirtyMemGroupQuery();
    ProtoSSLRefT* const state = static_cast<ProtoSSLRefT*>(DirtyMemAlloc(0x128, kProtoSSLMemId, group));
    if (!state)
    {
        NetPrintfCode("protossl: could not allocate module state\n");
        return 0;
    }
    std::memset(state, 0, 0x128);
    state->iMemGroup = group;
    return state;
}

extern "C" void ProtoSSLDestroy(ProtoSSLRefT* state)
{
    ResetState(state, 0);
    DirtyMemFree(state, kProtoSSLMemId, state->iMemGroup);
}

extern "C" ProtoSSLRefT* ProtoSSLAccept(ProtoSSLRefT* state, s32 secure, sockaddr* address, s32* addressLength)
{
    if (secure)
        return 0;
    SocketT* const socket = SocketAccept(state->pSock, address, addressLength);
    if (!socket)
        return 0;
    ProtoSSLRefT* const client = ProtoSSLCreate();
    if (!client)
    {
        SocketClose(socket);
        return 0;
    }
    client->pSock = socket;
    std::memcpy(&client->PeerAddr, address, *addressLength);
    client->iState = 31;
    return client;
}

// ARTIST 82B36FA0: start the native socket/resolver transition.
extern "C" s32 ProtoSSLConnect(ProtoSSLRefT* state, s32 secure, const char* address, u32 numericAddress, s32 port)
{
    const s32 reset = ResetState(state, secure);
    if (reset != 0)
        return reset;
    state->pSock = SocketOpen(2, 1, 0);
    if (!state->pSock)
        return -9;
    state->PeerAddr.sa_family = 2;
    std::memset(state->PeerAddr.sa_data, 0, sizeof(state->PeerAddr.sa_data));
    if (!address)
        address = "";
    u32 i = 0;
    while (address[i] && address[i] != ':' && i < 255)
    {
        state->strHost[i] = address[i];
        ++i;
    }
    state->strHost[i] = 0;
    SockaddrInSetAddrText(&state->PeerAddr, state->strHost);
    u8* const bytes = reinterpret_cast<u8*>(state->PeerAddr.sa_data);
    if ((u32(bytes[2]) | u32(bytes[3]) | u32(bytes[4]) | u32(bytes[5])) == 0)
    {
        bytes[2] = static_cast<u8>(numericAddress >> 24);
        bytes[3] = static_cast<u8>(numericAddress >> 16);
        bytes[4] = static_cast<u8>(numericAddress >> 8);
        bytes[5] = static_cast<u8>(numericAddress);
    }
    if (address[i] == ':')
    {
        bytes[0] = static_cast<u8>(std::atoi(address + i + 1) >> 8);
        bytes[1] = static_cast<u8>(std::atoi(address + i + 1));
    }
    else
    {
        bytes[0] = static_cast<u8>(port >> 8);
        bytes[1] = static_cast<u8>(port);
    }
    if ((u32(bytes[2]) | u32(bytes[3]) | u32(bytes[4]) | u32(bytes[5])) != 0)
        state->iState = 2;
    else
    {
        state->pHost = SocketLookup(state->strHost, 30000);
        state->iState = 1;
    }
    return 0;
}

extern "C" s32 ProtoSSLDisconnect(ProtoSSLRefT* state)
{
    ResetState(state, state->pSecure != 0);
    return 0;
}

extern "C" s32 ProtoSSLStat(ProtoSSLRefT* state, s32 selector, void* buffer, s32 length)
{
    if (selector == 0x736f636b)
    {
        if (!buffer || length != 4)
            return -1;
        std::memcpy(buffer, &state->pSock, 4);
        return 0;
    }
    if (!state->pSock)
        return -1;
    s32 result = SocketInfo(state->pSock, selector, 0, buffer, length);
    if (selector == 0x73746174)
    {
        if (state->iState >= 4096)
            result = -1;
        if (state->iState == 1)
            result = 0;
        if (result > 0 && state->iState != 31 && state->iState != 30)
            return 0;
    }
    return result;
}

// ARTIST 82B35880: construct one SSLv3 record and its negotiated MAC/cipher.
static s32 SendPacket(ProtoSSLRefT* state, u8 type, const void* header, s32 headerSize,
                       const void* body, s32 bodySize)
{
    SecureStateT* const secure = state->pSecure;
    secure->SendData[0] = type;
    secure->SendData[1] = 3;
    secure->SendData[2] = 0;
    std::memcpy(secure->SendData + 5, header, headerSize);
    std::memcpy(secure->SendData + 5 + headerSize, body, bodySize);
    s32 length = headerSize + bodySize;
    u8* const payload = secure->SendData + 5;
    if (type == 22)
    {
        CryptMD5Update(&secure->HandshakeMD5, payload, length);
        CryptSha1Update(&secure->HandshakeSHA, payload, length);
    }
    if (state->iState >= 24 && state->iState <= 30 && secure->pCipher)
    {
        const u32 sequence = secure->uSendSeqn;
        const u8 macHeader[11] = { 0, 0, 0, 0, static_cast<u8>(sequence >> 24),
            static_cast<u8>(sequence >> 16), static_cast<u8>(sequence >> 8),
            static_cast<u8>(sequence), type, static_cast<u8>(length >> 8), static_cast<u8>(length) };
        u8 innerPad[48], outerPad[48], digest[20];
        std::memset(innerPad, 0x36, sizeof(innerPad));
        std::memset(outerPad, 0x5c, sizeof(outerPad));
        if (secure->pCipher->uMac == 16)
        {
            CryptMD5T context;
            CryptMD5Init(&context);
            CryptMD5Update(&context, secure->pClientMAC, 16);
            CryptMD5Update(&context, innerPad, 48);
            CryptMD5Update(&context, macHeader, 11);
            CryptMD5Update(&context, payload, length);
            CryptMD5Final(&context, digest, 16);
            CryptMD5Init(&context);
            CryptMD5Update(&context, secure->pClientMAC, 16);
            CryptMD5Update(&context, outerPad, 48);
            CryptMD5Update(&context, digest, 16);
            CryptMD5Final(&context, payload + length, 16);
            length += 16;
        }
        if (secure->pCipher->uMac == 20)
        {
            CryptSha1T context;
            CryptSha1Init(&context);
            CryptSha1Update(&context, secure->pClientMAC, 20);
            CryptSha1Update(&context, innerPad, 40);
            CryptSha1Update(&context, macHeader, 11);
            CryptSha1Update(&context, payload, length);
            CryptSha1Final(&context, digest, 20);
            CryptSha1Init(&context);
            CryptSha1Update(&context, secure->pClientMAC, 20);
            CryptSha1Update(&context, outerPad, 40);
            CryptSha1Update(&context, digest, 20);
            CryptSha1Final(&context, payload + length, 20);
            length += 20;
        }
        if (secure->pCipher->uEnc == 1)
            CryptArc4Apply(&secure->WriteArc4, payload, length);
    }
    secure->SendData[3] = static_cast<u8>(length >> 8);
    secure->SendData[4] = static_cast<u8>(length);
    secure->iSendProg = 0;
    secure->iSendSize = length + 5;
    ++secure->uSendSeqn;
    return 0;
}

// ARTIST 82B37F00. State 30 uses the genuine record path; 31 is native raw mode.
extern "C" s32 ProtoSSLSend(ProtoSSLRefT* state, const char* data, s32 length)
{
    s32 result = -1;
    SecureStateT* const secure = state->pSecure;
    if (length < 0)
        length = static_cast<s32>(std::strlen(data));
    if (state->iState == 30)
    {
        result = 0;
        if (secure->iSendSize == 0)
        {
            if (length > 16000)
                length = 16000;
            SendPacket(state, 23, 0, 0, data, length);
            result = length;
            ProtoSSLUpdate(state);
        }
    }
    if (state->iState == 31)
        result = SocketSendto(state->pSock, data, length, 0, 0, 0);
    return result;
}

// ARTIST 82B37FD8: consume only completed application-data records.
extern "C" s32 ProtoSSLRecv(ProtoSSLRefT* state, char* buffer, s32 length)
{
    s32 result = -1;
    SecureStateT* const secure = state->pSecure;
    if (state->iState == 30)
    {
        result = 0;
        if (secure->iRecvProg != secure->iRecvSize)
            ProtoSSLUpdate(state);
        const s32 size = secure->iRecvSize;
        if ((size < 5 || secure->iRecvProg < size) && state->iClosed)
            result = -1;
        else if (secure->iRecvProg == size && secure->iRecvBase < size && secure->RecvData[0] == 23)
        {
            result = size - secure->iRecvBase;
            if (result > length)
                result = length;
            std::memcpy(buffer, secure->RecvData + secure->iRecvBase, result);
            secure->iRecvBase += result;
            if (secure->iRecvBase >= secure->iRecvSize)
            {
                secure->iRecvBase = 0;
                secure->iRecvSize = 0;
                secure->iRecvProg = 0;
            }
        }
    }
    if (state->iState == 31)
        result = SocketRecvfrom(state->pSock, buffer, length, 0, 0, 0);
    if (result > 0 && result < length)
        buffer[result] = 0;
    return result;
}

// ARTIST 82B35B30: decrypt and authenticate a complete SSLv3 record.
static s32 RecvPacket(ProtoSSLRefT* state)
{
    SecureStateT* const secure = state->pSecure;
    secure->iRecvBase = secure->iRecvSize;
    if (secure->iRecvProg != secure->iRecvSize) return -1;
    const u8 type = secure->RecvData[0];
    if (type < 20 || type > 23) return -2;
    if (secure->RecvData[1] != 3 || secure->RecvData[2] != 0) return -3;
    if (secure->iRecvSize != ((s32(secure->RecvData[3]) << 8) | secure->RecvData[4]) + 5) return -4;
    secure->iRecvBase = 5;
    if (state->iState >= 26 && state->iState <= 30 && secure->pCipher)
    {
        if (secure->pCipher->uEnc == 1)
            CryptArc4Apply(&secure->ReadArc4, secure->RecvData + 5, secure->iRecvSize - 5);
        s32 length = secure->iRecvSize - secure->iRecvBase;
        if (length < secure->pCipher->uMac) return -4;
        length -= secure->pCipher->uMac;
        const u32 sequence = secure->uRecvSeqn;
        const u8 macHeader[11] = { 0, 0, 0, 0, static_cast<u8>(sequence >> 24),
            static_cast<u8>(sequence >> 16), static_cast<u8>(sequence >> 8),
            static_cast<u8>(sequence), type, static_cast<u8>(length >> 8), static_cast<u8>(length) };
        secure->iRecvSize = secure->iRecvBase + length;
        secure->iRecvProg = secure->iRecvSize;
        u8 innerPad[48], outerPad[48], digest[20];
        std::memset(innerPad, 0x36, sizeof(innerPad));
        std::memset(outerPad, 0x5c, sizeof(outerPad));
        if (secure->pCipher->uMac == 16)
        {
            CryptMD5T context;
            CryptMD5Init(&context);
            CryptMD5Update(&context, secure->pServerMAC, 16);
            CryptMD5Update(&context, innerPad, 48);
            CryptMD5Update(&context, macHeader, 11);
            CryptMD5Update(&context, secure->RecvData + secure->iRecvBase, length);
            CryptMD5Final(&context, digest, 16);
            CryptMD5Init(&context);
            CryptMD5Update(&context, secure->pServerMAC, 16);
            CryptMD5Update(&context, outerPad, 48);
            CryptMD5Update(&context, digest, 16);
            CryptMD5Final(&context, digest, 16);
            if (std::memcmp(digest, secure->RecvData + secure->iRecvSize, 16) != 0)
            {
                NetPrintfCode("protossl: _RecvPacket: bad MD5 MAC\n");
                return -5;
            }
        }
        if (secure->pCipher->uMac == 20)
        {
            CryptSha1T context;
            CryptSha1Init(&context);
            CryptSha1Update(&context, secure->pServerMAC, 20);
            CryptSha1Update(&context, innerPad, 40);
            CryptSha1Update(&context, macHeader, 11);
            CryptSha1Update(&context, secure->RecvData + secure->iRecvBase, length);
            CryptSha1Final(&context, digest, 20);
            CryptSha1Init(&context);
            CryptSha1Update(&context, secure->pServerMAC, 20);
            CryptSha1Update(&context, outerPad, 40);
            CryptSha1Update(&context, digest, 20);
            CryptSha1Final(&context, digest, 20);
            if (std::memcmp(digest, secure->RecvData + secure->iRecvSize, 20) != 0)
            {
                NetPrintfCode("protossl: _RecvPacket: bad SHA MAC\n");
                return -6;
            }
        }
    }
    if (type == 22 && state->iState < 26)
    {
        const s32 length = secure->iRecvSize - secure->iRecvBase;
        CryptMD5Update(&secure->HandshakeMD5, secure->RecvData + secure->iRecvBase, length);
        CryptSha1Update(&secure->HandshakeSHA, secure->RecvData + secure->iRecvBase, length);
    }
    ++secure->uRecvSeqn;
    return 0;
}

// ARTIST 82B35F08: consume one matching handshake message from the record.
static const u8* RecvHandshake(ProtoSSLRefT* state, u8 type)
{
    SecureStateT* const secure = state->pSecure;
    const s32 base = secure->iRecvBase;
    const u8* const record = secure->RecvData + base;
    if (record[0] != type) return 0;
    const s32 length = (s32(record[1]) << 16) | (s32(record[2]) << 8) | record[3];
    const s32 end = base + length + 4;
    if (end > secure->iRecvSize)
    {
        NetPrintfCode("protossl: _RecvHandshake: packet at base %d is too long (%d vs %d)\n",
            base, length, secure->iRecvSize - base - 4);
        return 0;
    }
    secure->iRecvBase = end;
    return record + 4;
}

// Original cipher suite records at ARTIST 82146F10, each five bytes.
static const CipherSuiteT kCipherSuites[2] = {
    { { 0, 5 }, 1, 1, 20 }, { { 0, 4 }, 1, 1, 16 }
};

// These are real remaining ARTIST handshake providers, not fallback behavior.
extern "C" s32 ProtoSSLParseCertificate(ProtoSSLRefT*, X509CertificateT*, const u8*, s32); // 82B360D8
extern "C" s32 ProtoSSLUpdateSendClientKey(ProtoSSLRefT*); // 82B37318
extern "C" s32 ProtoSSLUpdateSendClientFinish(ProtoSSLRefT*); // 82B37580
extern "C" s32 ProtoSSLUpdateRecvServerFinish(ProtoSSLRefT*, const u8*); // 82B37720
extern "C" s32 SocketConnect(SocketT*, const sockaddr*, s32);

// ARTIST 82B37188.
static s32 RecvServerHello(ProtoSSLRefT* state, const u8* data)
{
    if (data[0] != 3 || data[1] != 0)
    {
        NetPrintfCode("protossl: _ServerHello: version %d,%d\n", data[0], data[1]);
        return 4100;
    }
    SecureStateT* const secure = state->pSecure;
    std::memcpy(secure->ServerRandom, data + 2, 32);
    const u8* const cipher = data + 35 + data[34];
    secure->pCipher = 0;
    for (u32 i = 0; i < 2; ++i)
    {
        if (cipher[0] == kCipherSuites[i].uIdent[0] && cipher[1] == kCipherSuites[i].uIdent[1])
        {
            secure->pCipher = &kCipherSuites[i];
            break;
        }
    }
    return 21;
}

// ARTIST 82B37268; certificate verification itself remains a genuine edge.
static s32 RecvServerCert(ProtoSSLRefT* state, const u8* data)
{
    const s32 envelope = (s32(data[0]) << 16) | (s32(data[1]) << 8) | data[2];
    const s32 certSize = (s32(data[3]) << 16) | (s32(data[4]) << 8) | data[5];
    if (certSize > envelope - 3)
    {
        NetPrintfCode("protossl: _ServerCert: first certificate is larger than envelope (%d/%d)\n", certSize, envelope - 3);
        return 4100;
    }
    const s32 result = ProtoSSLParseCertificate(state, &state->pSecure->Cert, data + 6, certSize);
    if (result < 0)
    {
        NetPrintfCode("protossl: _ServerCert: x509 cert invalid (error=%d)\n", result);
        return 4099;
    }
    return 21;
}

// ARTIST 82B37938: resolver and socket transitions followed by record dispatch.
extern "C" void ProtoSSLUpdate(ProtoSSLRefT* state)
{
    SecureStateT* const secure = state->pSecure;
    if (state->iState == 1)
    {
        HostentT* const host = state->pHost;
        if (host->Done(host))
        {
            if (host->addr == 0)
                state->iState = 4097;
            else
            {
                state->iState = 2;
                u8* const bytes = reinterpret_cast<u8*>(state->PeerAddr.sa_data);
                bytes[2] = static_cast<u8>(host->addr >> 24);
                bytes[3] = static_cast<u8>(host->addr >> 16);
                bytes[4] = static_cast<u8>(host->addr >> 8);
                bytes[5] = static_cast<u8>(host->addr);
            }
            host->Free(host);
            state->pHost = 0;
        }
    }
    if (state->iState == 2)
    {
        SocketConnect(state->pSock, &state->PeerAddr, 16);
        state->iState = 3;
    }
    if (state->iState == 3)
    {
        const s32 result = SocketInfo(state->pSock, 0x73746174, 0, 0, 0);
        if (result > 0)
        {
            state->iClosed = 0;
            state->iState = secure ? 20 : 31;
        }
        if (result < 0)
        {
            state->iClosed = 1;
            state->iState = 4098;
        }
    }
    if (!state->pSock)
        return;
    do
    {
        const s32 current = state->iState;
        if (current < 20 || current > 30)
            break;
        bool progress = false;
        if (secure->iSendProg == secure->iSendSize)
        {
            if (current == 20)
            {
                u8 body[43];
                body[0] = 3; body[1] = 0;
                ProtoSSLGenerateRandom(secure->ClientRandom, 32, &secure->ReadArc4);
                std::memcpy(body + 2, secure->ClientRandom, 32);
                body[34] = 0; body[35] = 0; body[36] = 4;
                body[37] = kCipherSuites[0].uIdent[0]; body[38] = kCipherSuites[0].uIdent[1];
                body[39] = kCipherSuites[1].uIdent[0]; body[40] = kCipherSuites[1].uIdent[1];
                body[41] = 1; body[42] = 0;
                const u8 header[4] = { 1, 0, 0, 43 };
                SendPacket(state, 22, header, 4, body, 43);
                state->iState = 21;
            }
            else if (current == 22)
                state->iState = ProtoSSLUpdateSendClientKey(state);
            else if (current == 23)
            {
                CryptArc4Init(&secure->WriteArc4, secure->pClientKey, 16, 1);
                const u8 change = 1;
                SendPacket(state, 20, &change, 1, 0, 0);
                secure->uSendSeqn = 0;
                state->iState = 24;
            }
            else if (current == 24)
                state->iState = ProtoSSLUpdateSendClientFinish(state);
        }
        if (secure->iSendProg < secure->iSendSize)
        {
            const s32 sent = SocketSendto(state->pSock,
                reinterpret_cast<const char*>(secure->SendData + secure->iSendProg),
                secure->iSendSize - secure->iSendProg, 0, 0, 0);
            if (sent > 0)
            {
                progress = true;
                secure->iSendProg += sent;
            }
            if (secure->iSendProg == secure->iSendSize)
            {
                secure->iSendSize = 0;
                secure->iSendProg = 0;
            }
        }
        if (secure->iRecvSize < 5)
        {
            const s32 received = SocketRecvfrom(state->pSock,
                reinterpret_cast<char*>(secure->RecvData + secure->iRecvSize),
                5 - secure->iRecvSize, 0, 0, 0);
            if (received > 0)
            {
                secure->iRecvSize += received;
                secure->iRecvProg = secure->iRecvSize;
            }
            if (received < 0) state->iClosed = 1;
        }
        if (secure->iRecvSize == 5)
        {
            if (secure->RecvData[1] != 3 || secure->RecvData[2] != 0)
                secure->RecvData[3] = secure->RecvData[4] = 0;
            secure->iRecvSize = (s32(secure->RecvData[3]) << 8) | secure->RecvData[4];
            secure->iRecvSize += 5;
        }
        if (secure->iRecvProg < secure->iRecvSize)
        {
            const s32 received = SocketRecvfrom(state->pSock,
                reinterpret_cast<char*>(secure->RecvData + secure->iRecvProg),
                secure->iRecvSize - secure->iRecvProg, 0, 0, 0);
            if (received > 0)
            {
                progress = true;
                secure->iRecvProg += received;
                if (secure->iRecvProg == secure->iRecvSize)
                    RecvPacket(state);
            }
            if (received < 0) state->iClosed = 1;
        }
        if (secure->iRecvProg == secure->iRecvSize && secure->iRecvBase < secure->iRecvSize)
        {
            const u8 type = secure->RecvData[0];
            if (type == 21)
            {
                if (secure->RecvData[5] != 1 || secure->RecvData[6] != 0)
                {
                    NetPrintfCode("protossl: ALERT: level=%d, type=%d\n", secure->RecvData[5], secure->RecvData[6]);
                    state->iState = 4100;
                }
                else
                {
                    state->iClosed = 1;
                    secure->iRecvBase = 0;
                    secure->iRecvSize = 0;
                    secure->iRecvProg = 0;
                }
            }
            else if (state->iState == 21)
            {
                const u8* message = RecvHandshake(state, 2);
                if (message)
                    state->iState = RecvServerHello(state, message);
                else if ((message = RecvHandshake(state, 11)) != 0)
                    state->iState = RecvServerCert(state, message);
                else if ((message = RecvHandshake(state, 12)) != 0)
                {
                    if (message[0] == 1)
                        state->iState = 21;
                    else
                    {
                        NetPrintfCode("protossl: _ServerKey: wrong key alg %d\n", message[0]);
                        state->iState = 4100;
                    }
                }
                else if (RecvHandshake(state, 13))
                {
                    NetPrintfCode("protossl: server wants our cert\n");
                    state->iState = 4100;
                }
                else if (RecvHandshake(state, 14))
                    state->iState = 22;
            }
            else if (state->iState == 25 && type == 20)
            {
                CryptArc4Init(&secure->ReadArc4, secure->pServerKey, 16, 1);
                secure->uRecvSeqn = 0;
                state->iState = 26;
                secure->iRecvBase = secure->iRecvSize;
            }
            else if (state->iState == 26)
            {
                const u8* const message = RecvHandshake(state, 20);
                if (message)
                    state->iState = ProtoSSLUpdateRecvServerFinish(state, message);
            }
            if (secure->iRecvBase >= secure->iRecvSize)
            {
                secure->iRecvBase = 0;
                secure->iRecvSize = 0;
                secure->iRecvProg = 0;
            }
        }
        if (!progress || !state->pSock)
            break;
    } while (true);
}

// ARTIST 82B37580/82B37720: SSLv3 finished hash over a copy of the transcript.
static void MakeFinished(SecureStateT* secure, const char* sender, u8 output[36])
{
    u8 innerPad[48], outerPad[48], digest[20];
    std::memset(innerPad, 0x36, sizeof(innerPad));
    std::memset(outerPad, 0x5c, sizeof(outerPad));
    CryptMD5T md5 = secure->HandshakeMD5;
    CryptMD5Update(&md5, sender, 4);
    CryptMD5Update(&md5, secure->MasterKey, 48);
    CryptMD5Update(&md5, innerPad, 48);
    CryptMD5Final(&md5, digest, 16);
    CryptMD5Init(&md5);
    CryptMD5Update(&md5, secure->MasterKey, 48);
    CryptMD5Update(&md5, outerPad, 48);
    CryptMD5Update(&md5, digest, 16);
    CryptMD5Final(&md5, output, 16);
    CryptSha1T sha = secure->HandshakeSHA;
    CryptSha1Update(&sha, reinterpret_cast<const u8*>(sender), 4);
    CryptSha1Update(&sha, secure->MasterKey, 48);
    CryptSha1Update(&sha, innerPad, 40);
    CryptSha1Final(&sha, digest, 20);
    CryptSha1Init(&sha);
    CryptSha1Update(&sha, secure->MasterKey, 48);
    CryptSha1Update(&sha, outerPad, 40);
    CryptSha1Update(&sha, digest, 20);
    CryptSha1Final(&sha, output + 16, 20);
}

extern "C" s32 ProtoSSLUpdateSendClientFinish(ProtoSSLRefT* state)
{
    u8 body[36];
    MakeFinished(state->pSecure, "CLNT", body);
    const u8 header[4] = { 20, 0, 0, 36 };
    SendPacket(state, 22, header, 4, body, 36);
    return 25;
}

extern "C" void NetPrintMemCode(const void*, s32, const char*);
extern "C" s32 ProtoSSLUpdateRecvServerFinish(ProtoSSLRefT* state, const u8* data)
{
    u8 expected[36];
    MakeFinished(state->pSecure, "SRVR", expected);
    if (std::memcmp(expected, data, 16) != 0 || std::memcmp(expected + 16, data + 16, 20) != 0)
    {
        NetPrintMemCode(expected, 16, "Server Final MD5 Calc");
        NetPrintMemCode(data, 16, "Server Final MD5 Recv");
        NetPrintMemCode(expected + 16, 20, "Server Final SHA Calc");
        NetPrintMemCode(data + 16, 20, "Server Final SHA Recv");
        return 4100;
    }
    return 30;
}

CGS_STATIC_ASSERT((offsetof(SecureStateT, Cert) + offsetof(X509CertificateT, iKeyModSize) == 1992), "ARTIST certificate modulus size");
CGS_STATIC_ASSERT((offsetof(SecureStateT, Cert) + offsetof(X509CertificateT, KeyModData) == 1996), "ARTIST certificate modulus");
CGS_STATIC_ASSERT((offsetof(SecureStateT, Cert) + offsetof(X509CertificateT, iKeyExpSize) == 2128), "ARTIST certificate exponent size");
CGS_STATIC_ASSERT((offsetof(SecureStateT, Cert) + offsetof(X509CertificateT, KeyExpData) == 2132), "ARTIST certificate exponent");

// ARTIST 82B37318: RSA-encrypt the real pre-master secret and derive SSLv3 keys.
extern "C" s32 ProtoSSLUpdateSendClientKey(ProtoSSLRefT* state)
{
    SecureStateT* const secure = state->pSecure;
    ProtoSSLGenerateRandom(secure->PreMasterKey, 48, &secure->ReadArc4);
    secure->PreMasterKey[0] = 3;
    secure->PreMasterKey[1] = 0;
    CryptRSAT rsa;
    CryptRSAInit(&rsa, secure->Cert.KeyModData, secure->Cert.iKeyModSize,
        secure->Cert.KeyExpData, secure->Cert.iKeyExpSize);
    CryptRSAInitMaster(&rsa, secure->PreMasterKey, 48);
    CryptRSAEncrypt(&rsa);
    u8 digest[20];
    u8 pad[8];
    for (s32 i = 1; i <= 3; ++i)
    {
        CryptMD5T md5;
        CryptSha1T sha;
        CryptMD5Init(&md5);
        CryptMD5Update(&md5, secure->PreMasterKey, 48);
        CryptSha1Init(&sha);
        std::memset(pad, 'A' + i - 1, i);
        CryptSha1Update(&sha, pad, i);
        CryptSha1Update(&sha, secure->PreMasterKey, 48);
        CryptSha1Update(&sha, secure->ClientRandom, 32);
        CryptSha1Update(&sha, secure->ServerRandom, 32);
        CryptSha1Final(&sha, digest, 20);
        CryptMD5Update(&md5, digest, 20);
        CryptMD5Final(&md5, secure->MasterKey + (i - 1) * 16, 16);
    }
    std::memset(secure->PreMasterKey, 0, 48);
    for (s32 i = 1; i <= 8; ++i)
    {
        CryptMD5T md5;
        CryptSha1T sha;
        CryptMD5Init(&md5);
        CryptMD5Update(&md5, secure->MasterKey, 48);
        CryptSha1Init(&sha);
        std::memset(pad, 'A' + i - 1, i);
        CryptSha1Update(&sha, pad, i);
        CryptSha1Update(&sha, secure->MasterKey, 48);
        CryptSha1Update(&sha, secure->ServerRandom, 32);
        CryptSha1Update(&sha, secure->ClientRandom, 32);
        CryptSha1Final(&sha, digest, 20);
        CryptMD5Update(&md5, digest, 20);
        CryptMD5Final(&md5, secure->KeyBlock + (i - 1) * 16, 16);
    }
    secure->pClientMAC = secure->KeyBlock;
    secure->pServerMAC = secure->pClientMAC + secure->pCipher->uMac;
    secure->pClientKey = secure->pServerMAC + secure->pCipher->uMac;
    secure->pServerKey = secure->pClientKey + 16;
    const s32 length = secure->Cert.iKeyModSize;
    const u8 header[4] = { 16, 0, static_cast<u8>(length >> 8), static_cast<u8>(length) };
    SendPacket(state, 22, header, 4, rsa.EncryptBlock, length);
    return 23;
}
