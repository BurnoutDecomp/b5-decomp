#ifndef DIRTYSDK_PROTOSSL_H
#define DIRTYSDK_PROTOSSL_H
#include "types.hpp"
struct ProtoSSLRefT;
struct sockaddr;
#ifdef __cplusplus
extern "C" {
#endif
ProtoSSLRefT* ProtoSSLCreate(void);
void ProtoSSLDestroy(ProtoSSLRefT* pState);
ProtoSSLRefT* ProtoSSLAccept(ProtoSSLRefT* pState, s32 iSecure, sockaddr* pAddr, s32* pAddrLen);
s32 ProtoSSLConnect(ProtoSSLRefT* pState, s32 iSecure, const char* pAddr, u32 uAddr, s32 iPort);
s32 ProtoSSLDisconnect(ProtoSSLRefT* pState);
void ProtoSSLUpdate(ProtoSSLRefT* pState);
s32 ProtoSSLSend(ProtoSSLRefT* pState, const char* pBuffer, s32 iLength);
s32 ProtoSSLRecv(ProtoSSLRefT* pState, char* pBuffer, s32 iLength);
s32 ProtoSSLStat(ProtoSSLRefT* pState, s32 iSelect, void* pBuffer, s32 iLength);
#ifdef __cplusplus
}
#endif
#endif
