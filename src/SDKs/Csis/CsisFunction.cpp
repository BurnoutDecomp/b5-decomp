#include "SDKs/Csis/CsisFunction.h"

// Csis::Function -- reconstructed from BURNOUT_X360_ARTIST.XEX (vendor Csis boundary).
//   CallFast        @ 0x82B0FB40
//   SubscribeFast   @ 0x82B0FBA8
//   UnsubscribeFast @ 0x82B0FC18
// Each validates the handle via ValidHandle<ClassHandle, FunctionDesc>(this, 0), then
// walks the descriptor's doubly-linked subscriber list rooted at *this (miPayload @+0).

namespace Csis
{

namespace
{
Result ValidateFunctionHandle(ClassHandle* apHandle)
{
    if (!apHandle || apHandle->miIndex < 0)
        return static_cast<Result>(apHandle ? apHandle->miIndex : -3);
    if (!apHandle->mpDescriptor)
        return static_cast<Result>(-6);
    if (apHandle->miIndex != *reinterpret_cast<s32*>(
            reinterpret_cast<u8*>(apHandle->mpDescriptor) + 0x10))
    {
        apHandle->mpDescriptor = 0;
        apHandle->miIndex = -3;
        return static_cast<Result>(-3);
    }
    return static_cast<Result>(0);
}
}

// ---------------------------------------------------------------------------
// Function::CallFast @ 0x82B0FB40
// ---------------------------------------------------------------------------
Result Function::CallFast(uintptr_t iParam)
{
    Result eValid = ValidateFunctionHandle(this);
    if (static_cast<int>(eValid) < 0)
        return eValid;

    Subscriber* pNode = *reinterpret_cast<Subscriber**>(miPayload);
    if (!pNode)
        return static_cast<Result>(-4);

    do
    {
        pNode->mpfnCallback(iParam, pNode->mpContext);
        pNode = pNode->mpNext;
    } while (pNode);

    return eValid;
}

// ---------------------------------------------------------------------------
// Function::SubscribeFast @ 0x82B0FBA8 -- head-insert into the subscriber list.
// ---------------------------------------------------------------------------
Result Function::SubscribeFast(Subscriber* pNode)
{
    Result eValid = ValidateFunctionHandle(this);
    if (static_cast<int>(eValid) < 0)
        return eValid;

    Subscriber** ppHead = reinterpret_cast<Subscriber**>(miPayload);
    Subscriber* pHead = *ppHead;

    pNode->mpPrev = nullptr;
    pNode->mpNext = pHead;
    if (pHead)
        pHead->mpPrev = pNode;
    *ppHead = pNode;

    return static_cast<Result>(0);
}

// ---------------------------------------------------------------------------
// Function::UnsubscribeFast @ 0x82B0FC18 -- doubly-linked unlink + head fixup.
// ---------------------------------------------------------------------------
Result Function::UnsubscribeFast(Subscriber* pNode)
{
    Result eValid = ValidateFunctionHandle(this);
    if (static_cast<int>(eValid) < 0)
        return eValid;

    Subscriber** ppHead = reinterpret_cast<Subscriber**>(miPayload);
    if (pNode == *ppHead)
        *ppHead = pNode->mpNext;

    if (pNode->mpPrev)
        pNode->mpPrev->mpNext = pNode->mpNext;
    if (pNode->mpNext)
        pNode->mpNext->mpPrev = pNode->mpPrev;

    return static_cast<Result>(0);
}

} // namespace Csis
