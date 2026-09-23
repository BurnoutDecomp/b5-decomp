#ifndef DIRTYSDK_PROTONAME_H
#define DIRTYSDK_PROTONAME_H

#include "types.hpp"

// DirtySDK 5.5.3 - proto/include/protoname.h (+ the HostentT record of dirtynet.h)
// Asynchronous host-name lookup. The CGS ping-regions component is the only caller.
// PC body: ../src/pc/protopc.cpp.

// Host-name lookup record returned by ProtoNameAsync. The caller polls Done until it is
// non-zero (-1 on failure), reads addr, then releases the record through Free.
struct HostentT
{
    s32  done;
    u32  addr;
    s32  (*Done)(HostentT* pHost);
    void (*Free)(HostentT* pHost);
    char name[64];
    s32  sema;
    s32  thread;
    u32  timeout;
};

#ifdef __cplusplus
extern "C" {
#endif

// Start resolving pName (iTimeout ms). Returns the lookup record, or NULL when no lookup
// could be started.
HostentT* ProtoNameAsync(const char* pName, s32 iTimeout);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_PROTONAME_H
