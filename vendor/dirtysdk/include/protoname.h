#ifndef DIRTYSDK_PROTONAME_H
#define DIRTYSDK_PROTONAME_H

#include "types.hpp"

// DirtySDK 5.5.3 - proto/include/protoname.h (+ the HostentT record of dirtynet.h)
// Asynchronous host-name lookup. The CGS ping-regions component is the only caller.
// PC body: ../src/pc/protopc.cpp.

// Host-name lookup record returned by ProtoNameAsync. The caller polls Done until it is
// non-zero (-1 on failure), reads addr, then releases the record through Free.
typedef struct HostentT HostentT;   // DirtySDK is C
struct HostentT
{
    s32  done;
    u32  addr;
    s32  (*Done)(HostentT* pHost);
    void (*Free)(HostentT* pHost);
    char name[64];
    // DWARF dirtynet.h:188-190. The console keeps its lookup state in the two int32_t words,
    // tested signed as such (ARTIST SocketLookup's free 0x82B2CCDC / 0x82B2CCEC cmpwi): sema
    // holds the XNDNS* of a DNS lookup, thread its event or the DirtyLSP query.
    s32  sema;     // +0x50
    s32  thread;   // +0x54
    u32  timeout;  // +0x58
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
