// ARTIST native Csis ABI; DecFIGS corroborates member/signature names.
#pragma once

#include "SDKs/Csis/CsisDefinition.h" // Csis::Result, Csis::ClassHandle, CsisDef::FunctionDesc

namespace Csis
{

class Function : public ClassHandle
{
public:
    struct Subscriber
    {
        Subscriber* mpNext;                       // +0x00  next in list
        Subscriber* mpPrev;                       // +0x04  prev in list
        void (*mpfnCallback)(Parameter* pParameter, void* pContext); // +0x08
        void*       mpContext;                    // +0x0C  opaque callback context
    };

    Result CallFast(Parameter* pParameter);

    Result SubscribeFast(Subscriber* pNode);

    Result UnsubscribeFast(Subscriber* pNode);
};

} // namespace Csis
