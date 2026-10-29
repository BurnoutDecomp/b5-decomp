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

// ---------------------------------------------------------------------------
// GlobalVariable::SetFast @ 0x82B0FEB0
// ---------------------------------------------------------------------------
Result GlobalVariable::SetFast(GlobalVariableHandle* pHandle, const CsisDef::Parameter* pValue)
{
    Result status = ValidHandle<GlobalVariableHandle, CsisDef::GlobalVariableDesc>(pHandle, 0);
    if (static_cast<int>(status) < 0)
        return status;

    CsisDef::GlobalVariableDesc* pDesc = ResolveDesc(pHandle);
    if (pValue->intVal == pDesc->curVal.intVal)
        return static_cast<Result>(0);

    GlobalVariableSubscriber* pNode = static_cast<GlobalVariableSubscriber*>(pDesc->clients);
    pDesc->curVal.intVal = pValue->intVal;
    while (pNode != 0)
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
    Result status = ValidHandle<GlobalVariableHandle, CsisDef::GlobalVariableDesc>(pHandle, 0);
    if (static_cast<int>(status) < 0)
        return status;

    CsisDef::GlobalVariableDesc* pDesc = ResolveDesc(pHandle);
    pNode->pPrev = 0;
    pNode->pNext = static_cast<GlobalVariableSubscriber*>(pDesc->clients);
    if (pDesc->clients != 0)
        static_cast<GlobalVariableSubscriber*>(pDesc->clients)->pPrev = pNode;
    pDesc->clients = pNode;
    pNode->pfnCallback(&pDesc->curVal, pNode->pUserData);
    return static_cast<Result>(0);
}

// ---------------------------------------------------------------------------
// GlobalVariable::UnsubscribeFast @ 0x82B0FFB8 -- doubly-linked unlink + head fixup.
// ---------------------------------------------------------------------------
Result GlobalVariable::UnsubscribeFast(GlobalVariableHandle* pHandle, GlobalVariableSubscriber* pNode)
{
    Result status = ValidHandle<GlobalVariableHandle, CsisDef::GlobalVariableDesc>(pHandle, 0);
    if (static_cast<int>(status) < 0)
        return status;

    CsisDef::GlobalVariableDesc* pDesc = ResolveDesc(pHandle);
    if (pNode == pDesc->clients)
        pDesc->clients = pNode->pNext;
    if (pNode->pPrev != 0)
        pNode->pPrev->pNext = pNode->pNext;
    if (pNode->pNext != 0)
        pNode->pNext->pPrev = pNode->pPrev;
    return static_cast<Result>(0);
}

} // namespace Csis
