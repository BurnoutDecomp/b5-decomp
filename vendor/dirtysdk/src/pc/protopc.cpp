// ============================================================================
// protopc.cpp -- ProtoHttp and ProtoNameAsync for the PC build.
//
// [PC platform layer] The game uses HTTP only for the terms-of-service download and host-name
// lookups only for the ping-region servers, both on the online service. The PC build reaches
// no such server in either mode, so:
//   - ProtoHttp: a GET cannot be started; the ref goes to the failed state, which answers as
//     the console module's failed request does ('done' -1, 'body' -1, 'time' 0, 'code' -1,
//     RecvAll PROTOHTTP_RECVFAIL). The CGS HTTP component turns that into its download-error
//     status on its next update.
//   - ProtoNameAsync: no lookup is started (NULL); the ping-regions component then records no
//     region and ends its action without an error.
// State lives in the ref ProtoHttpCreate returns (DirtyMemAlloc / DirtyMemFree).
// ============================================================================

#include "protohttp.h"
#include "protoname.h"
#include "dirtymem.h"   // DirtyMemAlloc / DirtyMemFree / DirtyMemGroupQuery

#include <cstring>      // memset

namespace
{
    // DirtyMem module id.
    const s32 KI_MEMID_PROTOHTTP = ('p' << 24) | ('h' << 16) | ('t' << 8) | 'p';

    // Smallest receive buffer ProtoHttpCreate accepts.
    const s32 KI_PROTOHTTP_MIN_BUFFER = 4096;

    // Request states the status queries distinguish.
    enum EHttpState
    {
        E_HTTP_STATE_IDLE   = 0,   // no request
        E_HTTP_STATE_FAILED = 7    // the request could not be completed
    };

    // ProtoHttpStatus answer while the response header has not arrived.
    const s32 KI_PROTOHTTP_HEADER_PENDING = -2;
}

struct ProtoHttpRefT
{
    s32        iMemGroup;
    s32        iBufSize;
    EHttpState eState;
};

// ============================================================================
// ProtoHttp
// ============================================================================

extern "C" ProtoHttpRefT* ProtoHttpCreate(s32 iBufSize)
{
    const s32 iMemGroup = DirtyMemGroupQuery();
    ProtoHttpRefT* pState =
        static_cast<ProtoHttpRefT*>(DirtyMemAlloc(sizeof(ProtoHttpRefT), KI_MEMID_PROTOHTTP, iMemGroup));
    if (pState == NULL)
    {
        return NULL;
    }
    memset(pState, 0, sizeof(*pState));
    pState->iMemGroup = iMemGroup;
    pState->iBufSize  = (iBufSize < KI_PROTOHTTP_MIN_BUFFER) ? KI_PROTOHTTP_MIN_BUFFER : iBufSize;
    pState->eState    = E_HTTP_STATE_IDLE;
    return pState;
}

extern "C" void ProtoHttpDestroy(ProtoHttpRefT* pState)
{
    DirtyMemFree(pState, KI_MEMID_PROTOHTTP, pState->iMemGroup);
}

extern "C" s32 ProtoHttpGet(ProtoHttpRefT* pState, const char* /*pUrl*/, u32 /*bHeadOnly*/)
{
    // No server is reachable: the request fails before a connection is made.
    pState->eState = E_HTTP_STATE_FAILED;
    return -1;
}

extern "C" void ProtoHttpAbort(ProtoHttpRefT* pState)
{
    pState->eState = E_HTTP_STATE_IDLE;
}

extern "C" void ProtoHttpUpdate(ProtoHttpRefT* /*pState*/)
{
    // Nothing is ever in flight.
}

extern "C" s32 ProtoHttpStatus(ProtoHttpRefT* pState, s32 iSelect, void* /*pBuffer*/, s32 /*iBufSize*/)
{
    switch (iSelect)
    {
    case PROTOHTTP_STATUS_TIME:
        // A request fails at once; it never runs long enough to time out.
        return 0;

    case PROTOHTTP_STATUS_DONE:
        return (pState->eState == E_HTTP_STATE_FAILED) ? -1 : 0;

    case PROTOHTTP_STATUS_CODE:
        return -1;

    case PROTOHTTP_STATUS_DATA:
        return (pState->eState == E_HTTP_STATE_FAILED) ? -1 : 0;

    default:
        return (pState->eState == E_HTTP_STATE_FAILED) ? -1 : KI_PROTOHTTP_HEADER_PENDING;
    }
}

extern "C" s32 ProtoHttpRecvAll(ProtoHttpRefT* pState, char* /*pBuffer*/, s32 /*iBufSize*/)
{
    return (pState->eState == E_HTTP_STATE_FAILED) ? PROTOHTTP_RECVFAIL : 0;
}

// ============================================================================
// ProtoNameAsync
// ============================================================================

extern "C" HostentT* ProtoNameAsync(const char* /*pName*/, s32 /*iTimeout*/)
{
    return NULL;
}
