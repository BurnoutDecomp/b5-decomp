#include "SDKs/Csis/CsisGlobalVariable.h"

// Csis::GlobalVariable -- reconstructed from BURNOUT_X360_ARTIST.XEX (vendor Csis boundary).
//   SetFast         @ 0x82B0FEB0
//   SubscribeFast   @ 0x82B0FF30
//   UnsubscribeFast @ 0x82B0FFB8
// Each validates the handle via ValidHandle, then reads word 0 of the handle as the
// resolved GlobalVariableDesc* (ResolveDesc) whose +0 is the subscriber list head and
// whose +4 is the current scalar value.

namespace Csis
{

namespace
{
Result ValidateGlobalHandle(GlobalVariableHandle* apHandle)
{
    if (!apHandle || apHandle->miIndex < 0)
        return static_cast<Result>(apHandle ? apHandle->miIndex : -3);
    if (!apHandle->mpDescriptor)
        return static_cast<Result>(-6);
    if (apHandle->miIndex != *reinterpret_cast<s32*>(
            reinterpret_cast<u8*>(apHandle->mpDescriptor) + 0x18))
    {
        apHandle->mpDescriptor = 0;
        apHandle->miIndex = -3;
        return static_cast<Result>(-3);
    }
    return static_cast<Result>(0);
}
}

// ---------------------------------------------------------------------------
// GlobalVariable::SetFast @ 0x82B0FEB0
// ---------------------------------------------------------------------------
Result GlobalVariable::SetFast(GlobalVariableHandle* pHandle, const CsisDef::Parameter* pValue)
{
    Result status = ValidateGlobalHandle(pHandle);
    if (static_cast<int>(status) < 0)
        return status;

    CsisDef::GlobalVariableDesc* pDesc = ResolveDesc(pHandle);
    if (pValue->intVal == pDesc->curVal.intVal)
        return static_cast<Result>(0);

    GlobalVariableSubscriber* pNode = pDesc->phead;
    pDesc->curVal.intVal = pValue->intVal;
    while (pNode != nullptr)
    {
        pNode->pfnCallback(&pDesc->curVal, pNode->pUserData);
        pNode = pNode->pNext;
    }
    return status;
}

// ---------------------------------------------------------------------------
// GlobalVariable::SubscribeFast @ 0x82B0FF30 -- push-front + immediate callback fire.
// ---------------------------------------------------------------------------
Result GlobalVariable::SubscribeFast(GlobalVariableHandle* pHandle, GlobalVariableSubscriber* pNode)
{
    Result status = ValidateGlobalHandle(pHandle);
    if (static_cast<int>(status) < 0)
        return status;

    CsisDef::GlobalVariableDesc* pDesc = ResolveDesc(pHandle);
    pNode->pPrev = nullptr;
    pNode->pNext = pDesc->phead;
    if (pDesc->phead != nullptr)
        pDesc->phead->pPrev = pNode;
    pDesc->phead = pNode;
    pNode->pfnCallback(&pDesc->curVal, pNode->pUserData);
    return static_cast<Result>(0);
}

// ---------------------------------------------------------------------------
// GlobalVariable::UnsubscribeFast @ 0x82B0FFB8 -- doubly-linked unlink + head fixup.
// ---------------------------------------------------------------------------
Result GlobalVariable::UnsubscribeFast(GlobalVariableHandle* pHandle, GlobalVariableSubscriber* pNode)
{
    Result status = ValidateGlobalHandle(pHandle);
    if (static_cast<int>(status) < 0)
        return status;

    CsisDef::GlobalVariableDesc* pDesc = ResolveDesc(pHandle);
    if (pNode == pDesc->phead)
        pDesc->phead = pNode->pNext;
    if (pNode->pPrev != nullptr)
        pNode->pPrev->pNext = pNode->pNext;
    if (pNode->pNext != nullptr)
        pNode->pNext->pPrev = pNode->pPrev;
    return static_cast<Result>(0);
}

} // namespace Csis
