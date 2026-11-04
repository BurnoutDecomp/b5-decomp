#ifndef DIRTYSDK_PROTOADVT_H
#define DIRTYSDK_PROTOADVT_H
#include "types.hpp"

struct ProtoAdvtRefT;
#ifdef __cplusplus
extern "C" {
#endif
ProtoAdvtRefT* ProtoAdvtConstruct(s32 maxRecords);
void ProtoAdvtDestroy(ProtoAdvtRefT* ref);
s32 ProtoAdvtAnnounce(ProtoAdvtRefT* ref, const char* kind, const char* name,
                      const char* note, const char* address, s32 frequency);
s32 ProtoAdvtCancel(ProtoAdvtRefT* ref, const char* kind, const char* name);
u32 ProtoAdvtLocate(ProtoAdvtRefT* ref, const char* kind, const char* name,
                    u32* localAddress, u32 fallback);
#ifdef __cplusplus
}
#endif
#endif