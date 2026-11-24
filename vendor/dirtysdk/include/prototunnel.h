#pragma once
#include "types.hpp"
typedef struct ProtoTunnelRefT ProtoTunnelRefT;   // DirtySDK is C
struct ProtoTunnelRefT;
typedef struct ProtoTunnelInfoT ProtoTunnelInfoT;   // DirtySDK is C
struct ProtoTunnelInfoT
{
    u32 uRemoteClientId;
    u32 uRemoteAddr;
    u16 uRemotePort;
    u16 uPadding;
    u16 aRemotePortList[8];
    u8 aPortFlags[8];
};
#ifdef __cplusplus
extern "C" {
#endif
ProtoTunnelRefT* ProtoTunnelCreate(s32 iMaxTunnels, s32 iTunnelPort);
void ProtoTunnelDestroy(ProtoTunnelRefT* pRef);
s32 ProtoTunnelAlloc(ProtoTunnelRefT* pRef, ProtoTunnelInfoT* pInfo, const char* pKey);
void ProtoTunnelFree(ProtoTunnelRefT* pRef, u32 uVirtualAddr, const char* pKey);
s32 ProtoTunnelControl(ProtoTunnelRefT* pRef, s32 iSelect, s32 iValue, s32 iValue2, const void* pValue);
void ProtoTunnelUpdate(ProtoTunnelRefT* pRef);
#ifdef __cplusplus
}
#endif
