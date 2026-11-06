from pathlib import Path
p=Path('vendor/dirtysdk/src/x360/protossl_x360.cpp');s=p.read_text();s=s.replace('#include <cstring>','#include <cstring>\n#include <cstdlib>');s=s.replace('extern "C" s32 SocketInfo(SocketT*, s32, s32, void*, s32);','''extern "C" s32 SocketInfo(SocketT*, s32, s32, void*, s32);
extern "C" SocketT* SocketOpen(s32, s32, s32);
extern "C" HostentT* SocketLookup(const char*, s32);
extern "C" s32 SockaddrInSetAddrText(sockaddr*, const char*);''');s=s.replace('extern "C" void ProtoSSLUpdate(ProtoSSLRefT*);','''extern "C" void ProtoSSLUpdate(ProtoSSLRefT*);
// ARTIST 82B356A0: genuine entropy routine; provider remains separately unresolved
// until its stack-sampling contract is reconstructed and verified on PPC.
extern "C" void ProtoSSLGenerateRandom(u8*, s32, CryptArc4T*);''')
a=s.index('// _ResetState(pState, 0)'); b=s.index('extern "C" ProtoSSLRefT* ProtoSSLCreate',a)
s=s[:a]+'''// ARTIST 82B35758: reset transport, then preserve or allocate real secure state.
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

'''+s[b:];s=s.replace('    ReleaseTransport(state);','    ResetState(state, 0);');marker='extern "C" s32 ProtoSSLDisconnect(ProtoSSLRefT* state)';a=s.index(marker)
connect='''// ARTIST 82B36FA0: start the native socket/resolver transition.
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

'''
s=s[:a]+connect+s[a:];p.write_text(s)
p=Path('vendor/dirtysdk/include/protossl.h');s=p.read_text();s=s.replace('s32 ProtoSSLDisconnect(ProtoSSLRefT* pState);','s32 ProtoSSLConnect(ProtoSSLRefT* pState, s32 iSecure, const char* pAddr, u32 uAddr, s32 iPort);\ns32 ProtoSSLDisconnect(ProtoSSLRefT* pState);');p.write_text(s)
