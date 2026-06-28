// ===========================================================================
// EATech Apt -- AptActionInterpreter operand stack.   DECOMPILED from the PS3
// EXTERNAL ELF:
//     stackPush       @0x7F1790    stackPushNoInc  @0x7E990C
//     stackPop()      @0x7F3248    stackPop(int)   @0x7FDB68
//     stackSafePop    @0x7DF7F8    stackGetPop     @0x7DF7B4
//     stackPopNoDec   @0x7DF7DC    stackPopAndPush @0x7FB288
//
// The interpreter's AptValue* evaluation stack: mpStack[0..mnStackTop). Pushes
// take a counted reference (AddRef), pops release it (Release) -- except the
// *NoInc/*NoDec/GetPop variants, which transfer ownership instead of touching the
// refcount. The console's `4 * top + base` byte addressing is reproduced here as
// element indexing (mpStack[top]); on x64 AptValue* is 8 bytes, so the indexing
// stays correct without a pointer-width transcode.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"

// ---------------------------------------------------------------------------
// stackPush @0x7F1790 -- store at top, advance, AddRef.
// ---------------------------------------------------------------------------
void AptActionInterpreter::stackPush(AptValue* pValue)
{
    mpStack[mnStackTop] = pValue;
    ++mnStackTop;
    pValue->AddRef();
}

// ---------------------------------------------------------------------------
// stackPushNoInc @0x7E990C -- store at top, advance, but DO NOT AddRef (the
// caller's reference is handed to the stack).
// ---------------------------------------------------------------------------
void AptActionInterpreter::stackPushNoInc(AptValue* pValue)
{
    mpStack[mnStackTop] = pValue;
    ++mnStackTop;
}

// ---------------------------------------------------------------------------
// stackPop @0x7F3248 -- release + pop the top value, returning it. The returned
// pointer has already been Release()'d (the console returns it regardless); null
// on an empty stack.
// ---------------------------------------------------------------------------
AptValue* AptActionInterpreter::stackPop()
{
    AptValue* pValue = 0;
    if (mnStackTop > 0)
    {
        pValue = mpStack[mnStackTop - 1];
        pValue->Release();
        --mnStackTop;
    }
    return pValue;
}

// ---------------------------------------------------------------------------
// stackPop @0x7FDB68 -- pop nCount values (releasing each), only if the stack
// holds at least that many.
// ---------------------------------------------------------------------------
void AptActionInterpreter::stackPop(int nCount)
{
    if (mnStackTop >= nCount)
    {
        for (int i = 1; i <= nCount; ++i)
            mpStack[mnStackTop - i]->Release();
        mnStackTop -= nCount;
    }
}

// ---------------------------------------------------------------------------
// stackSafePop @0x7DF7F8 -- stackPop(nCount) with the additional nCount>0 guard.
// ---------------------------------------------------------------------------
void AptActionInterpreter::stackSafePop(int nCount)
{
    if (nCount > 0 && mnStackTop >= nCount)
    {
        for (int i = 1; i <= nCount; ++i)
            mpStack[mnStackTop - i]->Release();
        mnStackTop -= nCount;
    }
}

// ---------------------------------------------------------------------------
// stackGetPop @0x7DF7B4 -- pop + return the top value WITHOUT releasing it (the
// caller takes over the reference). No bounds check (matches the console).
// ---------------------------------------------------------------------------
AptValue* AptActionInterpreter::stackGetPop()
{
    --mnStackTop;
    return mpStack[mnStackTop];
}

// ---------------------------------------------------------------------------
// stackPopNoDec @0x7DF7DC -- lower the top slot without releasing the value (its
// reference is kept alive by another owner).
// ---------------------------------------------------------------------------
void AptActionInterpreter::stackPopNoDec()
{
    if (mnStackTop > 0)
        --mnStackTop;
}

// ---------------------------------------------------------------------------
// stackPopAndPush @0x7FB288 -- collapse the top nCount operands into one result:
// AddRef the result, Release the nCount popped, store the result at the collapsed
// slot, set top = top - nCount + 1. No-op unless the stack holds nCount values.
// ---------------------------------------------------------------------------
void AptActionInterpreter::stackPopAndPush(int nCount, AptValue* pValue)
{
    if (mnStackTop >= nCount)
    {
        pValue->AddRef();
        for (int i = 1; i <= nCount; ++i)
            mpStack[mnStackTop - i]->Release();
        mpStack[mnStackTop - nCount] = pValue;
        mnStackTop = mnStackTop - nCount + 1;
    }
}
