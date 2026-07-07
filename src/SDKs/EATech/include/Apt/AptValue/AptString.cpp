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
// cat (@0x82AECD10) is a follow-on (Hex-Rays dropped its second operand).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptValue/AptString.h"

#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"   // AptInteger::Create (boxed indices)
#include "SDKs/EATech/include/Apt/AptArray.h"              // sMethod_split returns an Array
#include "SDKs/EATech/include/Apt/AptNativeFunction.h"     // objectMemberLookup builds the cached method values
#include "SDKs/EATech/include/Apt/AptValue/AptGCReleaseVector.h"  // gValuesToRelease (the deferred-release vector)
#include "SDKs/EATech/include/Apt/AptDefine.h"             // gpNonGCPoolManager (the non-GC value pool)
#include "SDKs/Packages/Apt/2.00.00/source/AptValue/AptString.h" // StringMembersIndex::in_word_set recognizer

#include <intrin.h>  // _InterlockedExchange (the Apt non-GC value-pool spin lock)
#include <new>       // placement new (construct an AptString into pooled storage)
#include <stdio.h>   // sprintf (charCodeAt renders the code as decimal text)
#include <cstdint>   // uintptr_t (the StringMembersIndex wordlist member-id payload)

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

// ---------------------------------------------------------------------------
// sMethod_substr @0x82AFCE5C (PS3 EXTERNAL DecFIGS 0xF41074) -- the legacy AS
// String.substr(start, length): the COUNT-based cousin of slice/substring. Return
// `length` characters starting at character `start`. A negative `start` counts back
// from the end (start += UTF8 length). With no `length` argument the slice runs to
// the end of the string. With no arguments at all the call is malformed -> undefined.
// The args sit on the native-arg stack (top == start, next == length).
//
// DECOMPILED from the PS3 body: a2 is the script arg count (undefined when 0); start
// is toInteger(arg-stack top), defaulting -1; the second arg's toInteger is the
// length; the negative-start fold is `if (start < 0) start += UTF8_Size`; the extract
// is UTF8_Mid(start, length) via the shared AptUTF8_SubString helper (a missing/large
// length runs to the end, matching the PS3 one-arg UTF8_Mid tail).
// ---------------------------------------------------------------------------
AptValue* AptString::sMethod_substr(AptString* pThis, int nArgCount)
{
    if (nArgCount == 0)
        return gpUndefinedValue;

    int nStart  = -1;
    int nLength = 9999999;   // no length arg -> run to the end (PS3 one-arg UTF8_Mid)
    if (nArgCount >= 1)
        nStart  = gppAptNativeArgStack[gnAptNativeArgCount - 1]->toInteger();
    if (nArgCount >= 2)
        nLength = gppAptNativeArgStack[gnAptNativeArgCount - 2]->toInteger();

    EAStringC strThis;
    pThis->toString(&strThis);
    const int nUtf8Size = strThis.UTF8_Size();

    // PS3 (0xF41074): v10 = start + UTF8_Size; if (start < 0) start = v10 (negative -> from
    // end). NB there is NO clamp-to-0 here: a still-negative start is passed straight into
    // AptUTF8_SubString, whose own `iCount -= iStart; iStart = 0` GROWS the count to preserve
    // the end. Pre-clamping start to 0 (without growing nLength) would drop |start+size| chars.
    if (nStart < 0)
        nStart += nUtf8Size;

    EAStringC strSlice;
    AptUTF8_SubString(&strSlice, &strThis, nStart, nLength);   // start + COUNT

    AptString* pResult = AptString::Create("");
    *pResult->GetInternalString() = strSlice;
    return pResult;
}

// sMethod_split @0x82AFC518 -- AS String.split(separator, limit). Always returns
// a freshly-allocated AptArray.
//
//  * No args            -> a one-element array holding the whole string value.
//  * separator present  -> the source is split on the separator (limit caps the
//                          number of pieces; default 999999):
//        - a non-empty separator splits on each occurrence, the trailing
//          remainder forming the final piece;
//        - an empty separator splits into individual UTF8 characters.
//
// The source string is reached through c_string() (which unwraps a boxed
// StringObject to its primitive String), then its embedded EAStringC.
AptValue* AptString::sMethod_split(AptString* pThis, int nArgCount)
{
    AptArray* pArray = new AptArray();

    if (nArgCount == 0)
    {
        // split() with no separator: the whole value as the single element.
        pArray->set(0, pThis);
        return pArray;
    }

    if (nArgCount < 1)
        return pArray;

    int nLimit = 999999;
    EAStringC strSeparator;
    gppAptNativeArgStack[gnAptNativeArgCount - 1]->toString(&strSeparator);
    if (nArgCount >= 2)
        nLimit = gppAptNativeArgStack[gnAptNativeArgCount - 2]->toInteger();

    const EAStringC& strSource = *pThis->c_string()->GetInternalString();
    const int nSeparatorLength = static_cast<int>(strSeparator.GetLength());

    if (nSeparatorLength != 0)
    {
        // Non-empty separator: find each occurrence, emit the slice between.
        if (nLimit > 0)
        {
            int nSearchPos = 0;
            int nOutIndex  = 0;
            for (;;)
            {
                const int nFound = strSource.Find(strSeparator.GetBuffer(), nSearchPos);
                if (nFound == -1)
                {
                    // Last piece: the remainder of the source.
                    AptString* pPiece = AptString::Create("");
                    if (nSearchPos != -1)
                        *pPiece->GetInternalString() = strSource.Mid(nSearchPos);
                    pArray->set(nOutIndex, pPiece);
                    break;
                }

                AptString* pPiece = AptString::Create("");
                *pPiece->GetInternalString() = strSource.Mid(nSearchPos, nFound - nSearchPos);
                pArray->set(nOutIndex++, pPiece);

                nSearchPos = nFound + nSeparatorLength;
                if (nOutIndex >= nLimit)
                    break;
            }
        }
    }
    else
    {
        // Empty separator: one element per UTF8 character (capped at limit).
        if (nLimit > 0)
        {
            const int nCharCount = strSource.UTF8_Size();
            int nOutIndex = 0;
            while (nOutIndex < nLimit && nOutIndex < nCharCount)
            {
                AptString* pPiece = AptString::Create("");
                *pPiece->GetInternalString() = strSource.UTF8_Mid(nOutIndex, 1);
                pArray->set(nOutIndex, pPiece);
                ++nOutIndex;
            }
        }
    }

    return pArray;
}

// ===========================================================================
// Pooled lifecycle: Create / Destroy   (the StringPool recycling spine).
//
// AptString values are recycled through a singly-linked free list whose head is
// the X360 global off_8324E4FC. Create pops a node (or, when the list is empty,
// allocates a fresh one from the non-GC value pool and constructs it); Destroy
// pushes a node back onto the list (releasing an over-large buffer first). Both
// mutate the free list under the Apt non-GC value-pool spin lock (X360
// unk_8324E8E8 -- a separate lock from the GC flag lock in AptValue.cpp).
// ===========================================================================

// FLAG (homed by the StringPool TU): the AptString recycle free-list head
// (X360 off_8324E4FC). Null until a node is ever freed; nodes are chained
// through AptString::mpNext (the +0xC link). Declared here so Create/Destroy can
// pop/push by name; the StringPool that also manages it is the follow-on.
extern AptString* gpStringPoolFreeList;   // off_8324E4FC

// FLAG: the non-GC value-pool free-list spin lock (X360 unk_8324E8E8). The
// console brackets the free-list mutation with the lwarx/stwcx. interrupt-masked
// test-and-set idiom; modelled as a host-portable interlocked TAS (uncontended on
// the single-thread bring-up path).
namespace
{
    volatile long gNonGCPoolFreeListLock = 0;
    inline void NonGCPoolFreeListLock_Acquire()
    {
        while (_InterlockedExchange(&gNonGCPoolFreeListLock, 1) != 0) {}
    }
    inline void NonGCPoolFreeListLock_Release()
    {
        _InterlockedExchange(&gNonGCPoolFreeListLock, 0);
    }
}

// ---------------------------------------------------------------------------
// Create @0x82AEDC18 -- hand back an AptString holding szValue.
//
// X360 control flow:
//   * Free list empty (off_8324E4FC == 0): allocate 16 bytes from the non-GC
//     pool (off_8324D808 == gpNonGCPoolManager) and run the ctor; return it (or
//     null if the pool is dry).
//   * Otherwise (under the pool lock): take the head node, mark it queued in the
//     GC deferred-release vector (SetReleaseAtEnd); if that vector has room push
//     the node, else undo (ClearReleaseAtEnd). Reset the node's embedded string
//     buffer when it still owns a real (non-empty-sentinel) one, unlink the node
//     (head = node->mpNext), copy szValue in when non-empty, and return it.
// ---------------------------------------------------------------------------
AptString* AptString::Create(const char* szValue)
{
    // Fast path duplicated by the X360: if the free list is empty there is no
    // lock/pop work to do -- go straight to a fresh pool allocation.
    if (gpStringPoolFreeList == 0)
    {
        // FLAG: the X360 calls DOGMA_PoolManager::Allocate(off_8324D808, 16)
        // directly; off_8324D808 is gpNonGCPoolManager (same global the header's
        // operator new/delete route through). sizeof(AptString) == 16 on X360.
        void* pMem = gpNonGCPoolManager->Allocate(sizeof(AptString));
        if (pMem == 0)
            return 0;
        return ::new (pMem) AptString(szValue);
    }

    NonGCPoolFreeListLock_Acquire();

    AptString* pNode = gpStringPoolFreeList;
    if (pNode == 0)
    {
        // The list emptied between the unlocked test and the lock: fall back to a
        // fresh pool allocation (the X360 re-checks under the lock and jumps to
        // the same allocate-and-construct tail).
        NonGCPoolFreeListLock_Release();
        void* pMem = gpNonGCPoolManager->Allocate(sizeof(AptString));
        if (pMem == 0)
            return 0;
        return ::new (pMem) AptString(szValue);
    }

    // Queue the recycled node in the GC deferred-release vector (or back it out
    // when the vector is full) -- the X360's SetReleaseAtEnd / push-or-clear.
    pNode->SetReleaseAtEnd();
    if (gValuesToRelease.mnTop < gValuesToRelease.mnCapacity)
        gValuesToRelease.mppItems[gValuesToRelease.mnTop++] = pNode;
    else
        pNode->ClearReleaseAtEnd();

    // Reset the embedded buffer when the node still owns a real StringDataC (the
    // X360 test is `str.m_pData != s_EmptyInternalData`). FLAG: ChangeBuffer is an
    // EAStringC-private helper that clears the buffer IN PLACE keeping its capacity;
    // its private members (m_pData / the empty sentinel) are not reachable from
    // AptString, so this drops back to a fresh empty value -- same observable result
    // (the node holds an empty string before the szValue copy), at the cost of not
    // recycling the buffer capacity. IsEmpty() stands in for the sentinel compare.
    if (!pNode->str.IsEmpty())
        pNode->str = EAStringC();

    // Unlink the head (head = node->mpNext).
    gpStringPoolFreeList = pNode->mpNext;

    if (szValue && szValue[0] != '\0')
    {
        // X360: build a temp EAStringC from szValue and assign it into the node's
        // string (operator=), then drop the temp.
        EAStringC strInit(szValue);
        pNode->str = strInit;
    }

    NonGCPoolFreeListLock_Release();
    return pNode;
}

// ---------------------------------------------------------------------------
// Destroy @0x82AE8218 -- return this node to the recycle free list.
//
// Under the pool lock: push the node onto the free list (node->mpNext = head;
// head = node). If the node's buffer is over the kept-temp size (its StringDataC
// m_uMaxSize > MAX_SIZE_KEPT_TEMP_STRING, i.e. > 0x21-1 -- the X360 gate is
// `m_uMaxSize > 0x21`) release the buffer and reset the string to the empty
// sentinel, so an over-large allocation is not held by the pool.
// ---------------------------------------------------------------------------
void AptString::Destroy()
{
    NonGCPoolFreeListLock_Acquire();

    // Push onto the free list head.
    mpNext = gpStringPoolFreeList;
    gpStringPoolFreeList = this;

    // X360: if (str.m_pData->m_uMaxSize > 0x21) { release the buffer; reset to the
    // empty sentinel }. GetInternalMaxSize() reads that same m_uMaxSize field.
    // FLAG: the X360 does a raw m_pData swap to the empty sentinel after a private
    // DecreaseInternalRefCount; the public equivalent (assigning an empty value)
    // performs the same buffer release + empty-reset.
    if (str.GetInternalMaxSize() > 0x21u)
        str = EAStringC();

    NonGCPoolFreeListLock_Release();
}

// ===========================================================================
// objectMemberLookup @0x82AFCAB0 -- resolve an AS member/method name on a String.
//
// Run the gperf recognizer (StringMembersIndex::in_word_set) over the requested
// name; on a hit, dispatch on its member id. The "length" member computes a fresh
// boxed integer (the UTF-8 character count of the rendered value). Each method
// member returns a process-wide cached AptNativeFunction wrapping the matching
// sMethod_* native, lazily built on first use (GC-rooted + AddRef'd so the GC
// keeps it alive). A miss (or no name) returns 0.
// ===========================================================================

// FLAG (homed by the StringPool/Apt-globals TU): the 12 cached AS String method
// values (X360 off_8324E4B8 .. off_8324E4E4, one per method). Null until first
// lazily built by objectMemberLookup; Released + nulled by CleanNativeFunctions.
// Indexed by E_StringMethodCache below (declaration order == the X360 address
// order of the cache globals).
enum E_StringMethodCache
{
    SMC_charAt = 0, SMC_charCodeAt, SMC_concat,  SMC_fromCharCode,
    SMC_indexOf,    SMC_lastIndexOf, SMC_slice,  SMC_split,
    SMC_substr,     SMC_substring,   SMC_toLowerCase, SMC_toUpperCase,
    SMC_Count
};
AptNativeFunction* gapAptStringMethodCache[SMC_Count] = { 0 };

namespace
{
    // The repeated X360 block: lazily build the cached native-function for one
    // String method (operator new -> ctor wrapping the sMethod), GC-root it, take
    // a reference, and return the cache. Already-built caches return immediately
    // without re-referencing (the X360 `if (cache) return cache`).
    AptValue* GetCachedStringMethod(E_StringMethodCache eSlot, AptExtFunctionPtr pFn)
    {
        AptNativeFunction*& rpCache = gapAptStringMethodCache[eSlot];
        if (rpCache != 0)
            return rpCache;

        // FLAG: the X360 does not null-guard the operator-new result (it would
        // setGCRoot/AddRef through a null `this`); guarded here so a dry pool
        // cannot crash the bring-up. The success path is byte-faithful.
        rpCache = new AptNativeFunction(pFn);
        if (rpCache != 0)
        {
            rpCache->setGCRoot(1);   // AptValue::setGCRoot(this, 1)
            rpCache->AddRef();       // vtable slot 0
        }
        return rpCache;
    }
}

AptValue* AptString::objectMemberLookup(AptValue* const pContext,
                                        const AptNativeString* const pName) const
{
    const StringMembersIndex::Entry* pEntry = 0;
    if (pContext != 0)
    {
        // X360: in_word_set(pName->str.m_pData buffer, pName m_uSize). GetBuffer()
        // is that +8 buffer pointer; GetLength() the m_uSize the asm loads (lhz 2).
        pEntry = StringMembersIndex::in_word_set(pName->GetBuffer(), pName->GetLength());
    }

    if (pEntry == 0)
        return 0;

    // The Entry's second word carries a 1-based member id; (id - 1) indexes the
    // X360 jump table word_82145380. The jump-table CONTENTS are un-recovered, but
    // the physical case-block order @loc_82AFCB24+ pins the mapping unambiguously:
    // the first block (jump base, offset 0) is the length branch, then charAt,
    // charCodeAt, concat, fromCharCode, indexOf, lastIndexOf, slice, split, substr,
    // substring, toLowerCase, toUpperCase (asm sMethod_* refs @0x82AFCB7C..0x82AFCF70).
    // So uMember 0 == length (payload id 1) and 1..12 == charAt..toUpperCase.
    const unsigned int uMember = static_cast<unsigned int>(
        reinterpret_cast<uintptr_t>(pEntry->mpPayload)) - 1u;
    if (uMember > 0xCu)   // only 13 valid members (the X360 `> 0xC` reject)
        return 0;

    switch (uMember)
    {
    case 0:
    {
        // "length": render the value and box its UTF-8 character count
        // (jump-base block @loc_82AFCB24 -> toString/UTF8_Size/AptInteger::Create).
        EAStringC strValue;
        pContext->toString(&strValue);
        return AptInteger::Create(strValue.UTF8_Size());
    }

    case 1:  return GetCachedStringMethod(SMC_charAt,      reinterpret_cast<AptExtFunctionPtr>(&AptString::sMethod_charAt));
    case 2:  return GetCachedStringMethod(SMC_charCodeAt,  reinterpret_cast<AptExtFunctionPtr>(&AptString::sMethod_charCodeAt));
    case 3:  return GetCachedStringMethod(SMC_concat,      reinterpret_cast<AptExtFunctionPtr>(&AptString::sMethod_concat));
    case 4:  return GetCachedStringMethod(SMC_fromCharCode,reinterpret_cast<AptExtFunctionPtr>(&AptString::sMethod_fromCharCode));
    case 5:  return GetCachedStringMethod(SMC_indexOf,     reinterpret_cast<AptExtFunctionPtr>(&AptString::sMethod_indexOf));
    case 6:  return GetCachedStringMethod(SMC_lastIndexOf, reinterpret_cast<AptExtFunctionPtr>(&AptString::sMethod_lastIndexOf));
    case 7:  return GetCachedStringMethod(SMC_slice,       reinterpret_cast<AptExtFunctionPtr>(&AptString::sMethod_slice));
    case 8:  return GetCachedStringMethod(SMC_split,       reinterpret_cast<AptExtFunctionPtr>(&AptString::sMethod_split));
    case 9:  return GetCachedStringMethod(SMC_substr,      reinterpret_cast<AptExtFunctionPtr>(&AptString::sMethod_substr));
    case 10: return GetCachedStringMethod(SMC_substring,   reinterpret_cast<AptExtFunctionPtr>(&AptString::sMethod_substring));
    case 11: return GetCachedStringMethod(SMC_toLowerCase, reinterpret_cast<AptExtFunctionPtr>(&AptString::sMethod_toLowerCase));
    case 12: return GetCachedStringMethod(SMC_toUpperCase, reinterpret_cast<AptExtFunctionPtr>(&AptString::sMethod_toUpperCase));

    default:
        return 0;
    }
}

// ---------------------------------------------------------------------------
// CleanNativeFunctions @0x82AD7A78 -- Apt shutdown teardown: Release every cached
// String method value (vtable slot +4 == Release) and null its cache slot, in the
// X360's order (off_8324E4B8 .. off_8324E4E4). Called by AptUpdateShutdown.
// ---------------------------------------------------------------------------
void AptString::CleanNativeFunctions()
{
    for (int i = 0; i < SMC_Count; ++i)
    {
        if (gapAptStringMethodCache[i] != 0)
        {
            gapAptStringMethodCache[i]->Release();
            gapAptStringMethodCache[i] = 0;
        }
    }
}

// ===========================================================================
// StringMembersIndex -- the gperf perfect-hash recognizer for the AS String
// member/method names (in_word_set + hash + the gperf static tables). X360
// in_word_set @0x82AD7918 / hash @ (inlined). MOVED here from the UNBUILT TU
// SDKs/Packages/Apt/2.00.00/source/AptValue/AptString.cpp into this BUILT TU so
// the recognizer this file already calls (objectMemberLookup above) actually
// links. The table bytes are the X360 .rdata extraction (asso_values[256]
// @byte_82F72DF8, lookup[23] @byte_82F79C08, wordlist[13] @off_82F79BA0). The
// class itself is declared by the SDKs/Packages .../AptString.h already included
// at the top of this file.
// ===========================================================================
const unsigned char StringMembersIndex::asso_values[256] =
{
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23,  0, 23, 10,  0,  0, 10,  0, 23, 23,  0, 23, 23, 23,
    23, 23, 14,  0,  0, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23
};

const signed char StringMembersIndex::lookup[23] =
{
      -1,   -1,   -1,   -1,   -1,    0,  -26,    3,   -1,    4,    5,    6,
     -12,   -2,   -1,    7,    8,   -1,   -3,   -2,    9,  -32,   12
};

const struct StringMembersIndex::Entry StringMembersIndex::wordlist[13] =
{
    /*  0 */ { "split",         reinterpret_cast<void*>(static_cast<uintptr_t>(9)) },
    /*  1 */ { "charAt",        reinterpret_cast<void*>(static_cast<uintptr_t>(2)) },
    /*  2 */ { "concat",        reinterpret_cast<void*>(static_cast<uintptr_t>(4)) },
    /*  3 */ { "indexOf",       reinterpret_cast<void*>(static_cast<uintptr_t>(6)) },
    /*  4 */ { "substring",     reinterpret_cast<void*>(static_cast<uintptr_t>(11)) },
    /*  5 */ { "charCodeAt",    reinterpret_cast<void*>(static_cast<uintptr_t>(3)) },
    /*  6 */ { "lastIndexOf",   reinterpret_cast<void*>(static_cast<uintptr_t>(7)) },
    /*  7 */ { "slice",         reinterpret_cast<void*>(static_cast<uintptr_t>(8)) },
    /*  8 */ { "length",        reinterpret_cast<void*>(static_cast<uintptr_t>(1)) },
    /*  9 */ { "substr",        reinterpret_cast<void*>(static_cast<uintptr_t>(10)) },
    /* 10 */ { "toLowerCase",   reinterpret_cast<void*>(static_cast<uintptr_t>(12)) },
    /* 11 */ { "toUpperCase",   reinterpret_cast<void*>(static_cast<uintptr_t>(13)) },
    /* 12 */ { "fromCharCode",  reinterpret_cast<void*>(static_cast<uintptr_t>(5)) },
};

// --- gperf hash (asso_values[] @0x82F72DF8) ------------------------------
// hash = asso_values[(unsigned char)str[len-1]] + asso_values[(unsigned char)str[0]] + len
unsigned int
StringMembersIndex::hash(const char* lpcStr, unsigned int luLen)
{
    return asso_values[(unsigned char)lpcStr[luLen - 1]]
         + asso_values[(unsigned char)lpcStr[0]]
         + luLen;
}

// gperf perfect-hash lookup with duplicate-key resolution. Faithful to the X360
// pseudocode @0x82AD7918 (length gate 5..12, first/last-char + length hash, hash
// gate 0x16, direct slot vs duplicate-range walk; the run COUNT is stored NEGATED
// -- the asm forms the run end as start - count*8).
const struct StringMembersIndex::Entry*
StringMembersIndex::in_word_set(const char* lpcStr, unsigned int luLen)
{
    if (luLen - MIN_WORD_LENGTH > MAX_WORD_LENGTH - MIN_WORD_LENGTH)  // (luLen - 5) > 7
        return 0;

    const unsigned int luKey = hash(lpcStr, luLen);
    if (luKey > MAX_HASH_VALUE)
        return 0;

    const int liIndex = lookup[luKey];

    if (liIndex >= 0)
    {
        // Single (non-duplicate) keyword slot.
        const struct Entry* lpEntry = &wordlist[liIndex];
        const char* lpcName = lpEntry->name;
        if (lpcStr[0] == lpcName[0])
        {
            const char* lpcS = lpcStr  + 1;
            const char* lpcN = lpcName + 1;
            int liDiff;
            do
            {
                liDiff = (unsigned char)*lpcS - (unsigned char)*lpcN;
                if (*lpcS == '\0')
                    break;
                ++lpcS;
                ++lpcN;
            }
            while (liDiff == 0);

            if (liDiff == 0)
                return lpEntry;
        }
        return 0;
    }

    // liIndex < 0 : either an empty slot or a duplicate list.
    if (liIndex >= -TOTAL_KEYWORDS)   // (-13..-1) -> no duplicate bucket here
        return 0;

    // Duplicate list: gperf encodes lookup[hash] == -(TOTAL_KEYWORDS + 1) - offset.
    const int liOffset = -(TOTAL_KEYWORDS + 1) - liIndex;     // == -14 - liIndex
    const int liBase  = lookup[liOffset];                     // base (sign-extended)
    const int liCount = -(int)lookup[liOffset + 1];           // count (stored negated)

    const struct Entry* lpEntry = &wordlist[TOTAL_KEYWORDS + liBase];
    const struct Entry* lpEnd   = lpEntry + liCount;

    for (; lpEntry < lpEnd; ++lpEntry)
    {
        const char* lpcName = lpEntry->name;
        if (lpcStr[0] == lpcName[0])
        {
            const char* lpcS = lpcStr  + 1;
            const char* lpcN = lpcName + 1;
            int liDiff;
            do
            {
                liDiff = (unsigned char)*lpcS - (unsigned char)*lpcN;
                if (*lpcS == '\0')
                    break;
                ++lpcS;
                ++lpcN;
            }
            while (liDiff == 0);

            if (liDiff == 0)
                return lpEntry;
        }
    }
    return 0;
}
