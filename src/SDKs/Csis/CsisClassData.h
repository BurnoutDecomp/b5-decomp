// ARTIST native Csis ABI; DecFIGS corroborates member/signature names.
#ifndef CSIS_CLASSDATA_H
#define CSIS_CLASSDATA_H

#include "SDKs/Csis/CsisDefinition.h"

namespace Csis
{

union Parameter;

struct ClassDataClient
{
    typedef void (*NotifyFn)(Parameter* pParameter, void* pContext);

    ClassDataClient* mpNext;     // +0x0  forward link (0 == end)
    ClassDataClient* mpPrev;     // +0x4  back link
    NotifyFn         mpfnNotify; // +0x8  callback
    void*            mpContext;  // +0xC  callback context
};

struct CListDStack
{
    ClassDataClient* mpHead; // +0x0
};

struct ClassData
{
    void SendParameters(Parameter* pParameter);

    CsisDef::ClassDesc* mpClassDesc;     // +0x0
    s32                    mRefCount;       // +0x4
    CListDStack            mUpdateClients;  // +0x8
    CListDStack            mDestroyClients; // +0xC
};

} // namespace Csis

#endif // CSIS_CLASSDATA_H

CGS_STATIC_ASSERT(sizeof(Csis::ClassData) == 16, "ARTIST class data");
CGS_STATIC_ASSERT(sizeof(Csis::ClassDataClient) == 16, "ARTIST class data client");
CGS_STATIC_ASSERT(offsetof(Csis::ClassDataClient, mpfnNotify) == 8, "ARTIST data callback");
CGS_STATIC_ASSERT(offsetof(Csis::ClassData, mUpdateClients) == 8, "ARTIST update list");
