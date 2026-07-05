#include "SDKs/Csis/CsisFunction.h"

// Csis::Function -- reconstructed from BURNOUT_X360_ARTIST.XEX (vendor Csis boundary).
//   CallFast        @ 0x82B0FB40
//   SubscribeFast   @ 0x82B0FBA8
//   UnsubscribeFast @ 0x82B0FC18
// Each validates the handle via ValidHandle<ClassHandle, FunctionDesc>(this, 0), then
// walks the descriptor's doubly-linked subscriber list rooted at *this (miPayload @+0).

namespace Csis
{

// ---------------------------------------------------------------------------
// Function::CallFast @ 0x82B0FB40
// ---------------------------------------------------------------------------
Result Function::CallFast(int iParam)
{
    Result eValid = ValidHandle<ClassHandle, CsisDef::FunctionDesc>(this, 0);
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
    Result eValid = ValidHandle<ClassHandle, CsisDef::FunctionDesc>(this, 0);
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
    Result eValid = ValidHandle<ClassHandle, CsisDef::FunctionDesc>(this, 0);
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
