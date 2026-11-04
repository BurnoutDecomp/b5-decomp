#ifndef DIRTYSDK_DIRTYLSP_H
#define DIRTYSDK_DIRTYLSP_H
#include "protoname.h"

struct DirtyLSPRefT;
#ifdef __cplusplus
extern "C" {
#endif
DirtyLSPRefT* DirtyLSPCreate(void);
void DirtyLSPDestroy(DirtyLSPRefT* pRef);
s32 DirtyLSPQueryStart(DirtyLSPRefT* pRef, HostentT* pHost);
s32 DirtyLSPQueryDone(DirtyLSPRefT* pRef, HostentT* pHost);
void DirtyLSPQueryFree(DirtyLSPRefT* pRef, HostentT* pHost);
#ifdef __cplusplus
}
#endif
#endif
