// ARTIST native Csis ABI; DecFIGS corroborates member/signature names.
#pragma once

#include "SDKs/Csis/CsisGlobalVariableHandle.h" // Csis::GlobalVariableHandle, Csis::Result, CsisDef::GlobalVariableDesc (fwd)

#include "SDKs/Csis/CsisDefinition.h"

namespace Csis
{

struct GlobalVariableSubscriber
{
    typedef void (*Callback)(CsisDef::Parameter* pValue, void* pUserData);

    GlobalVariableSubscriber* pNext;        // +0x00
    GlobalVariableSubscriber* pPrev;        // +0x04
    Callback                  pfnCallback;  // +0x08
    void*                     pUserData;    // +0x0C
};

} // namespace Csis

namespace Csis
{

inline CsisDef::GlobalVariableDesc* ResolveDesc(GlobalVariableHandle* pHandle)
{
    return *reinterpret_cast<CsisDef::GlobalVariableDesc**>(pHandle);
}

class GlobalVariable
{
public:
    static Result SetFast(GlobalVariableHandle* pHandle, const CsisDef::Parameter* pValue);

    static Result SubscribeFast(GlobalVariableHandle* pHandle, GlobalVariableSubscriber* pNode);

    static Result UnsubscribeFast(GlobalVariableHandle* pHandle, GlobalVariableSubscriber* pNode);
};

} // namespace Csis
