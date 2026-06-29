// ===========================================================================
// EATech Apt -- AptString ActionScript String native methods (sMethod_*).
//
// Reconstructed from the X360 ARTIST.XEX pseudocode/asm via the decompile->
// verify workflow. Each is a static native (AptExtFunctionPtr): the X360 VM
// calls it as f(this, argCount) with r3=the bound String value and r4=the AS
// argument count; the arguments themselves sit on the global native-arg stack
// (gAptActionInterpreter operand stack). They return a fresh boxed value, or
// the shared "undefined" value (off_8324D814) when the call is malformed.
//
// The substring family (slice/substring/split) + cat are a follow-on (they need
// the UTF8 substring-extract helper sub_82AE8ED8 homed first).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptValue/AptString.h"

#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"   // AptInteger::Create (boxed indices)

#include <stdio.h>   // sprintf (charCodeAt renders the code as decimal text)

// FLAG (homed by the apt VM native-call dispatch): the global native-method
// argument stack -- the dispatch layer pushes the AS call args here before
// invoking the native (X360 globals dword_8324E760 = count = the shared
// gAptActionInterpreter.mnStackTop, off_8324E768 = array = its mpStack). The
// i-th argument (i=0 = the last pushed) is gppAptNativeArgStack[gnAptNativeArgCount-1-i].
extern AptValue** gppAptNativeArgStack;   // off_8324E768
extern int        gnAptNativeArgCount;    // dword_8324E760

// FLAG (homed by the AS-globals layer): the shared "undefined" value (off_8324D814).
extern AptValue* gpUndefinedValue;

// ---------------------------------------------------------------------------
// sMethod_charAt @0x82AF2848 -- AS String.charAt(index): return a one-character
// string holding the UTF-8 character at `index`, or undefined when the index is
// negative or out of range. The single argument sits on top of the native-call
// arg stack.
// ---------------------------------------------------------------------------
AptValue* AptString::sMethod_charAt(AptString* pThis)
{
    const int nIndex = gppAptNativeArgStack[gnAptNativeArgCount - 1]->toInteger();

    // The receiver may be a raw StringValue or a boxed StringObject; c_string()
    // resolves both to the underlying AptString (the X360 `a1 = a1[8]` unwrap).
    AptString* pStr = (pThis->getVtblIndex() == AptVFT_StringValue)
                          ? pThis
                          : pThis->c_string();

    if (nIndex < 0)
        return gpUndefinedValue;

    // The UTF-8 byte run that begins the `nIndex`-th character (null past the end).
    const char* pCharRun = pStr->GetInternalString()->UTF8_GetBuffer(nIndex);
    if (!pCharRun)
        return gpUndefinedValue;

    EAStringC strChar;                 // empty (the &unk_82F72FF8 sentinel)
    strChar.UTF8_Append(pCharRun, 1);  // one UTF-8 character unit

    AptString* pResult = AptString::Create("");
    *pResult->GetInternalString() = strChar;
    return pResult;
}

// ---------------------------------------------------------------------------
// sMethod_charCodeAt @0x82AF2910 -- AS String.charCodeAt(index): return the
// UTF-8 character CODE at `index` rendered as a decimal string (the X360
// formats it with "%d" and boxes it as a String), or undefined when the index
// is negative or out of range. The single argument is on top of the arg stack.
// ---------------------------------------------------------------------------
AptValue* AptString::sMethod_charCodeAt(AptString* pThis)
{
    const int nIndex = gppAptNativeArgStack[gnAptNativeArgCount - 1]->toInteger();

    AptString* pStr = (pThis->getVtblIndex() == AptVFT_StringValue)
                          ? pThis
                          : pThis->c_string();

    const char* pCharRun =
        (nIndex >= 0) ? pStr->GetInternalString()->UTF8_GetBuffer(nIndex) : 0;
    if (nIndex < 0 || !pCharRun)
        return gpUndefinedValue;

    const int nCharCode = EAStringC::UTF8_GetCharacter(pCharRun);

    char szCode[8];
    sprintf(szCode, "%d", nCharCode);

    EAStringC strCode(szCode);   // InitFromBuffer

    AptString* pResult = AptString::Create("");
    *pResult->GetInternalString() = strCode;
    return pResult;
}

// ---------------------------------------------------------------------------
// sMethod_concat @0x82AFC168 -- AS String.concat(a, b, ...): render the
// receiver to a string, then append every argument's string form (the args are
// concatenated in pushed order, i.e. arg 0 is the first one appended) and box
// the result as a new String.
// ---------------------------------------------------------------------------
AptValue* AptString::sMethod_concat(AptString* pThis, int nArgCount)
{
    EAStringC strResult;                 // empty
    pThis->toString(&strResult);

    for (int i = 0; i < nArgCount; ++i)
    {
        EAStringC strArg;
        gppAptNativeArgStack[gnAptNativeArgCount - i - 1]->toString(&strArg);
        strResult += strArg;
    }

    AptString* pResult = AptString::Create("");
    *pResult->GetInternalString() = strResult;
    return pResult;
}

// ---------------------------------------------------------------------------
// sMethod_fromCharCode @0x82AF29E8 -- AS String.fromCharCode(c0, c1, ...): build
// a string from the given character codes (each coerced to an int and emitted
// as one UTF-8 character). Static AS method; the receiver is unused. The result
// buffer is pre-reserved for 2 bytes per code.
// ---------------------------------------------------------------------------
AptValue* AptString::sMethod_fromCharCode(AptString* /*pThis*/, int nArgCount)
{
    EAStringC strResult(static_cast<uint32_t>(2 * nArgCount));   // reserve

    for (int i = 0; i < nArgCount; ++i)
    {
        const int nCharCode =
            gppAptNativeArgStack[gnAptNativeArgCount - i - 1]->toInteger();

        EAStringC strChar;
        strChar.UTF8_Initialize(nCharCode);
        strResult += strChar;
    }

    AptString* pResult = AptString::Create("");
    *pResult->GetInternalString() = strResult;
    return pResult;
}

// ---------------------------------------------------------------------------
// sMethod_indexOf @0x82AFC218 -- AS String.indexOf(search [, fromIndex]):
// return the (UTF-8 character) index of the first occurrence of `search` at or
// after `fromIndex` (default 0; a negative fromIndex is clamped to 0), or -1.
// With no arguments it returns undefined.
// ---------------------------------------------------------------------------
AptValue* AptString::sMethod_indexOf(AptString* pThis, int nArgCount)
{
    EAStringC strThis;
    pThis->toString(&strThis);

    if (nArgCount == 0)
        return gpUndefinedValue;

    EAStringC strSearch;
    gppAptNativeArgStack[gnAptNativeArgCount - 1]->toString(&strSearch);

    int nStart = 0;
    if (nArgCount == 2)
    {
        nStart = gppAptNativeArgStack[gnAptNativeArgCount - 2]->toInteger();
        if (nStart < 0)
            nStart = 0;
    }

    const int nFound = strThis.UTF8_Find(strSearch.GetBuffer(), nStart);
    return AptInteger::Create(nFound);
}

// ---------------------------------------------------------------------------
// sMethod_lastIndexOf @0x82AFC2E0 -- AS String.lastIndexOf(search [, fromIndex]):
// return the index of the last occurrence of `search` at or before `fromIndex`
// (default: the end of the string), or -1. With no arguments it returns undefined.
// ---------------------------------------------------------------------------
AptValue* AptString::sMethod_lastIndexOf(AptString* pThis, int nArgCount)
{
    EAStringC strThis;
    pThis->toString(&strThis);

    if (nArgCount == 0)
        return gpUndefinedValue;

    EAStringC strSearch;
    gppAptNativeArgStack[gnAptNativeArgCount - 1]->toString(&strSearch);

    int nStart;
    if (nArgCount <= 1)
        nStart = static_cast<int>(strThis.GetLength());
    else
        nStart = gppAptNativeArgStack[gnAptNativeArgCount - 2]->toInteger();

    const int nFound = strThis.LastIndexOf(strSearch.GetBuffer(), nStart);
    return AptInteger::Create(nFound);
}

// sMethod_toLowerCase @0x82AFC9E0 -- AS String.toLowerCase(): render this value
// to its string form, UTF8-lowercase it in place, and box the result in a fresh
// AptString. (Takes no arguments.)
AptValue* AptString::sMethod_toLowerCase(AptString* pThis)
{
    EAStringC strValue;
    pThis->toString(&strValue);
    strValue.UTF8_MakeLower();

    AptString* pResult = AptString::Create("");
    *pResult->GetInternalString() = strValue;
    return pResult;
}

// sMethod_toUpperCase @0x82AFCA48 -- AS String.toUpperCase(): render this value
// to its string form, UTF8-uppercase it in place, and box the result in a fresh
// AptString. (Takes no arguments.)
AptValue* AptString::sMethod_toUpperCase(AptString* pThis)
{
    EAStringC strValue;
    pThis->toString(&strValue);
    strValue.UTF8_MakeUpper();

    AptString* pResult = AptString::Create("");
    *pResult->GetInternalString() = strValue;
    return pResult;
}

// ---------------------------------------------------------------------------
// AptUTF8_SubString (sub_82AE8ED8) -- the shared UTF-8 substring extractor the
// slice/substring (and the SubString opcode) methods call. Build *pOut from the
// run of iCount UTF-8 chars starting at the iStart-th char of pSrc. (== the body
// of EAStringC::UTF8_Mid; kept as the distinct symbol the VM calls.)
// ---------------------------------------------------------------------------
// sub_82AE8ED8 -- the shared UTF-8 substring extractor the AS String slice/
// substr/substring methods (and the SubString opcode) call. Build *pOut from the
// run of `iCount` UTF-8 characters starting at the `iStart`-th character of pSrc.
// A negative start is pulled up to 0 with the count shrunk by the same amount
// (so e.g. start=-2,count=5 -> start=0,count=7, preserving the end). This is the open-coded body of
// EAStringC::UTF8_Mid(iStart, iCount): walk the buffer to the UTF-8 char
// boundaries, then do a byte-range Mid.
static EAStringC* AptUTF8_SubString(EAStringC* pOut, const EAStringC* pSrc, int iStart, int iCount)
{
    if (iStart < 0)
    {
        iCount -= iStart;   // grow the count to preserve the end position (X360: a4 - a3)
        iStart = 0;
    }

    if (iCount > 0)
    {
        const char* pBuffer = pSrc->GetBuffer();
        const char* pStart  = EAStringC::UTF8_GetBuffer(pBuffer, iStart);
        if (pStart)
        {
            const char* pEnd = EAStringC::UTF8_GetBuffer(pStart, iCount);
            const int   iByteStart = static_cast<int>(pStart - pBuffer);
            if (pEnd)
                *pOut = pSrc->Mid(iByteStart, static_cast<int>(pEnd - pStart));
            else
                *pOut = pSrc->Mid(iByteStart);   // count runs past the end -> to end
            return pOut;
        }
    }

    *pOut = EAStringC();   // empty: negative/zero count, or start past the end
    return pOut;
}

// sMethod_slice @0x82AFC3C0 -- AS String.slice(start, end): return the substring
// from character `start` (default -1, i.e. the last char) up to but not including
// `end` (default a huge sentinel == the whole tail). Negative indices count back
// from the end (idx += length); both are then clamped to [0, length]. With no
// arguments the call is malformed -> undefined. The args sit on the native-arg stack.
AptValue* AptString::sMethod_slice(AptString* pThis, int nArgCount)
{
    if (nArgCount == 0)
        return gpUndefinedValue;

    int nStart = -1;
    int nEnd   = 9999999;
    if (nArgCount >= 1)
        nStart = gppAptNativeArgStack[gnAptNativeArgCount - 1]->toInteger();
    if (nArgCount >= 2)
        nEnd   = gppAptNativeArgStack[gnAptNativeArgCount - 2]->toInteger();

    EAStringC strThis;
    pThis->toString(&strThis);
    const int nLength = strThis.UTF8_Size();

    if (nStart < 0) nStart += nLength;
    if (nEnd   < 0) nEnd   += nLength;
    if (nStart < 0) nStart = 0;
    if (nEnd   < 0) nEnd   = 0;
    if (nStart > nLength) nStart = nLength;
    if (nEnd   > nLength) nEnd   = nLength;

    EAStringC strSlice;
    AptUTF8_SubString(&strSlice, &strThis, nStart, nEnd - nStart);

    AptString* pResult = AptString::Create("");
    *pResult->GetInternalString() = strSlice;
    return pResult;
}

// sMethod_substring @0x82AFC8B0 -- AS String.substring(start, end): like slice but
// it does NOT treat negatives as from-the-end -- instead it swaps start/end so the
// smaller is first, clamps both to >= 0, then swaps again, before extracting the
// [start, end) character range. With no arguments the call is malformed -> undefined.
AptValue* AptString::sMethod_substring(AptString* pThis, int nArgCount)
{
    if (nArgCount == 0)
        return gpUndefinedValue;

    int nStart = -1;
    int nEnd   = 9999999;
    if (nArgCount >= 1)
        nStart = gppAptNativeArgStack[gnAptNativeArgCount - 1]->toInteger();
    if (nArgCount >= 2)
        nEnd   = gppAptNativeArgStack[gnAptNativeArgCount - 2]->toInteger();

    if (nStart > nEnd)
    {
        const int nSwap = nStart;
        nStart = nEnd;
        nEnd   = nSwap;
    }
    if (nStart < 0) nStart = 0;
    if (nEnd   < 0) nEnd   = 0;
    if (nStart > nEnd)
    {
        const int nSwap = nStart;
        nStart = nEnd;
        nEnd   = nSwap;
    }

    EAStringC strThis;
    pThis->toString(&strThis);

    AptString* pResult = AptString::Create("");
    AptUTF8_SubString(pResult->GetInternalString(), &strThis, nStart, nEnd - nStart);
    return pResult;
}
