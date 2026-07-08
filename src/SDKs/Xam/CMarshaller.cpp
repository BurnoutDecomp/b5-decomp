// CMarshaller.cpp -- XAM RPC argument marshaller bodies (Xbox 360).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. CMarshaller serialises a typed argument
// list into a raw wire buffer by walking the call's schema descriptor stream (see
// CMarshaller.h for the object layout). The four bodies:
//
//   CMarshaller::Initialize            @ 0x8297F4D8
//   CMarshaller::GetArgumentFromSchema @ 0x8297F598
//   CMarshaller::RecursiveMarshal      @ 0x8297F620
//   CMarshaller::Marshal               @ 0x8297FDE8
//
// The schema read cursor is the embedded mSchemaAccess sub-object throughout
// RecursiveMarshal: the X360 holds &mSchemaAccess (this+0x1C) in one register for the
// whole function, so Hex-Rays' "HIDWORD(v8)" cursor aliasing is noise -- every
// Get*/Seek*/Skip* call below targets mSchemaAccess. The write side targets mBuffer
// (this+0x08); the conformance table is mConformance (this+0x38).

#include "SDKs/Xam/CMarshaller.h"

#include <cstring> // memset
#include <cwchar>  // wcslen

// Win32 UTF-16 -> UTF-8 conversion used on the string-marshalling path. Declared locally
// (the Xam headers pull no <windows.h>); CP_UTF8 == 65001.
extern "C" int __stdcall WideCharToMultiByte(unsigned int CodePage, unsigned long dwFlags,
                                             const wchar_t* lpWideCharStr, int cchWideChar,
                                             char* lpMultiByteStr, int cbMultiByte,
                                             const char* lpDefaultChar, int* lpUsedDefaultChar);

// HRESULT-style status constant the bodies return (X360 rodata:
// lis r3,-0x8000 ; ori r3,r3,0x4005 == 0x80004005).
static const s32          KI_E_FAIL   = static_cast<s32>(0x80004005);
static const unsigned int KU_CP_UTF8  = 65001;
static const int          KI_ARG_TAG  = 4; // argument-slot type tag stamped by the marshaller

// ---------------------------------------------------------------------------
// CMarshaller::Initialize @ 0x8297F4D8
//
// Record the schema + argument-list pointers, bind the schema read cursor to the schema
// (first (size,offset) pair) and the output buffer to the caller's span, then zero the
// 64-slot conformance table. On a schema-bind failure, reset the object (clearing the
// scalar pointers, the schema-cursor + output-buffer spans, and the default conformance)
// and return the failing status. luBufferSize is handed to CBaseEndianBuffer::Bind as
// both the context and the size argument (the X360 passes the same register for r5/r6).
// ---------------------------------------------------------------------------
s32 CMarshaller::Initialize(CSchemaData* lpSchema, u32 luSchemaId, u32 luDefaultConformance,
                            CArgumentList* lpArgList, u8* lpBuffer, u32 luBufferSize,
                            u32 lbSwapEndian)
{
    mpSchema  = lpSchema;
    mpArgList = lpArgList;

    s32 lhr = mSchemaAccess.BindToSchema(lpSchema, luSchemaId, 1);
    if (lhr >= 0)
    {
        muDefaultConformance = luDefaultConformance;
        mBuffer.Bind(lpBuffer, luBufferSize, luBufferSize, lbSwapEndian);
        memset(&mConformance, 0, sizeof(mConformance));
    }
    else
    {
        mpSchema  = 0;
        mpArgList = 0;
        mSchemaAccess.Reset();
        mBuffer.Reset();
        muDefaultConformance = 0;
    }
    return lhr;
}

// ---------------------------------------------------------------------------
// CMarshaller::GetArgumentFromSchema @ 0x8297F598
//
// Read the schema's next 1-byte argument-info descriptor. Bit 0x20 (>>5 & 1) is handed
// back through lpIndirect when non-null; the low 5 bits index the bound argument list.
// Hands the slot back in *lppSlot (null when the index is past the list's count) and
// returns E_FAIL for an out-of-range argument index or a missing slot.
// ---------------------------------------------------------------------------
s32 CMarshaller::GetArgumentFromSchema(u32 luArgIndex, CArgumentList::Element** lppSlot,
                                       u32* lpIndirect)
{
    if (luArgIndex > 1)
    {
        return KI_E_FAIL;
    }

    u8  lDescriptor;
    s32 lhr = mSchemaAccess.GetArgumentInfo(&lDescriptor);
    if (lhr < 0)
    {
        return lhr;
    }

    if (lpIndirect)
    {
        *lpIndirect = (lDescriptor >> 5) & 1;
    }

    u32 luSlot = lDescriptor & 0x1F;
    CArgumentList::Element* lpElement =
        (luSlot >= mpArgList->muCount) ? 0 : &mpArgList->maElements[luSlot];
    *lppSlot = lpElement;
    if (lpElement == 0)
    {
        lhr = KI_E_FAIL;
    }
    return lhr;
}

// ---------------------------------------------------------------------------
// CMarshaller::RecursiveMarshal @ 0x8297F620
//
// Marshal one schema scope at `luDepth` (bounded to 0x10) into the output buffer. On entry
// `lppData` holds the source-data pointer for this scope; on a clean exit it is advanced
// past the scope. The scope header is a field descriptor (lParentDesc) plus a count word;
// the descriptor's bits select an argument source (0x80), a skipped word (0x40), a
// run-time conformance / repeat count (0x20) and a union (0x0C == 8). The body then walks
// each child field `luRepeatCount` times, writing each leaf value (endian-swapped) and
// recursing into nested scopes.
// ---------------------------------------------------------------------------
s32 CMarshaller::RecursiveMarshal(u32 luDepth, u8** lppData)
{
    if (luDepth >= 0x10)
    {
        return KI_E_FAIL;
    }

    u8* lpData              = lppData ? *lppData : 0;      // source-data cursor (v7 low)
    u32 luSavedSchemaOffset = mSchemaAccess.GetOffset();   // scope descriptor position (v54)

    u8  lParentDesc;                                       // v36
    s32 lhr = mSchemaAccess.GetDescriptor(&lParentDesc);
    if (lhr < 0)
    {
        return lhr;
    }

    u32 luParentKind = (lParentDesc >> 2) & 3;             // v10
    if (luParentKind == 0 || luParentKind == 3)
    {
        return KI_E_FAIL;
    }

    // Scope count word: kind 1 uses one slot, kind 2 the other (which doubles as the union
    // element stride, luCountWord2).
    u16 luCountWord1 = 0;                                  // v39
    u16 luCountWord2 = 0;                                  // v41
    if (luParentKind == 1)
    {
        lhr = mSchemaAccess.GetWord(&luCountWord1);
    }
    else
    {
        lhr = mSchemaAccess.GetWord(&luCountWord2);
    }
    if (lhr < 0)
    {
        return lhr;
    }

    CArgumentList::Element* lpArgSlot = 0;                 // v50

    // Argument-sourced scope: read the source pointer out of the argument slot, then stamp
    // the slot with the current output-cursor position (type tag 4).
    if (lParentDesc & 0x80)
    {
        lhr = GetArgumentFromSchema(luDepth, &lpArgSlot, 0);
        if (lhr < 0)
        {
            return lhr;
        }
        u8* lpOutCursor    = mBuffer.GetBase() + mBuffer.GetOffset();
        lpData             = reinterpret_cast<u8*>(lpArgSlot->miValue);
        lpArgSlot->muType  = KI_ARG_TAG;
        lpArgSlot->miValue = reinterpret_cast<s64>(lpOutCursor);
    }

    if (lParentDesc & 0x40)
    {
        u16 luSkip;                                        // v44
        lhr = mSchemaAccess.GetWord(&luSkip);
        if (lhr < 0)
        {
            return lhr;
        }
    }

    // Repeat count: a conforming scope resolves it through the conformance table, else 1.
    u8  lParentConformDesc[2] = { 0, 0 };                  // v43
    u32 luRepeatCount;                                     // v49
    if (lParentDesc & 0x20)
    {
        luRepeatCount = muDefaultConformance;
        lhr = mConformance.GetConformingScopeAndValue(lParentDesc, &mSchemaAccess,
                                                      lParentConformDesc, &luRepeatCount);
        if (lhr < 0)
        {
            return lhr;
        }
    }
    else
    {
        luRepeatCount = 1;
    }

    // Union selector value (else the saved schema offset).
    u8  lUnionInfo[2] = { 0, 0 };                          // v38
    u64 luUnionValue;                                      // v12 (low 32 significant)
    if ((lParentDesc & 0xC) == 8)
    {
        u16 luUnionSelector;                               // v42
        lhr = mSchemaAccess.GetUnionInfo(lUnionInfo, &luUnionSelector);
        if (lhr < 0)
        {
            return lhr;
        }

        CConformanceList::Entry* lpUnionEntry;             // v53
        lhr = mConformance.AcquireConformanceInfo(lUnionInfo[0] & 0x3F, &lpUnionEntry);
        if (lhr < 0)
        {
            return lhr;
        }
        luUnionValue = lpUnionEntry->muValue;

        if (lUnionInfo[0] & 0x40)
        {
            u32 luConst;                                   // v47
            lhr = mSchemaAccess.GetSchema()->LookupConstantFromTable(luUnionSelector, &luConst);
            if (lhr < 0)
            {
                return lhr;
            }
            luUnionValue = luConst & static_cast<u32>(luUnionValue);
        }

        lhr = mConformance.ReleaseConformanceInfo(lUnionInfo[0] & 0x3F);
        if (lhr < 0)
        {
            return lhr;
        }
    }
    else
    {
        luUnionValue = luSavedSchemaOffset;
    }

    u32 luScopeChildStart = mSchemaAccess.GetOffset();     // child-field start (v7 high)

    // Walk the scope's child fields, `luRepeatCount` times.
    if (luRepeatCount != 0)
    {
        u8  lChildDesc = 0;                                // v35
        u32 luRepeat   = 0;                                // v15

        for (;;) // scope-repeat loop
        {
            u16  luMemberIdx   = 0;                        // v16 (union member index)
            s32  lUnionMatched = 0;                        // v17 (any member matched)
            lChildDesc &= 0xF3;                            // clear the end-of-scope bits

            bool lbInvalid      = false;
            bool lbFinalize     = false;
            bool lbRestartScope = false;

            for (;;) // field loop
            {
                bool lbAfterField = false; // true -> jump straight to the post-field step

                if ((lParentDesc & 0xC) == 8)
                {
                    if (luMemberIdx >= lUnionInfo[1])
                    {
                        // Union members exhausted: the next descriptor must end the scope.
                        lhr = mSchemaAccess.GetDescriptor(&lChildDesc);
                        if (lhr < 0)
                        {
                            return lhr;
                        }
                        if ((lChildDesc & 0xC) != 0xC ||
                            (lUnionMatched == 0 && (lUnionInfo[0] & 0x80) != 0))
                        {
                            lbInvalid = true;
                            break;
                        }
                        lbAfterField = true;
                    }
                    else
                    {
                        u8 lMatchCount;                    // v37
                        lhr = mSchemaAccess.GetByte(&lMatchCount);
                        if (lhr < 0)
                        {
                            return lhr;
                        }

                        bool lbMemberSelected = false;     // v18
                        for (u8 i = 0; i < lMatchCount; ++i)
                        {
                            lhr = mSchemaAccess.GetWord(&luCountWord1);
                            if (lhr < 0)
                            {
                                return lhr;
                            }
                            u32 luConst;                   // v47
                            lhr = mSchemaAccess.GetSchema()->LookupConstantFromTable(luCountWord1, &luConst);
                            if (lhr < 0)
                            {
                                return lhr;
                            }
                            if (static_cast<u32>(luUnionValue) == luConst)
                            {
                                lbMemberSelected = true;
                                lUnionMatched    = 1;
                            }
                        }

                        lhr = mSchemaAccess.GetWord(&luCountWord1);
                        if (lhr < 0)
                        {
                            return lhr;
                        }

                        if (!lbMemberSelected)
                        {
                            // Non-selected member: skip its data span.
                            u32   luSize = luCountWord1;
                            void* lpSkip;
                            lhr = mSchemaAccess.GetDataPointer(&lpSkip, &luSize);
                            if (lhr < 0)
                            {
                                return lhr;
                            }
                            lbAfterField = true;
                        }
                        // else: selected -- fall through and marshal the member's field.
                    }
                }

                if (!lbAfterField)
                {
                    u32 luFieldSchemaOffset = mSchemaAccess.GetOffset(); // v21
                    lhr = mSchemaAccess.GetDescriptor(&lChildDesc);
                    if (lhr < 0)
                    {
                        return lhr;
                    }

                    u32 luChildKind = (lChildDesc >> 2) & 3;            // v22
                    if (luChildKind == 0)
                    {
                        // ================= leaf field =================
                        bool lbArgIndirect = false;                     // v23
                        bool lbString      = false;                     // v24
                        u64  luWriteValue  = 0;                         // v8 (establish source)

                        if (lChildDesc & 0x80)
                        {
                            u32 luIndirect = 0;                         // v42
                            lhr = GetArgumentFromSchema(luDepth, &lpArgSlot, &luIndirect);
                            if (lhr < 0)
                            {
                                return lhr;
                            }
                            u8* lpOutCursor    = mBuffer.GetBase() + mBuffer.GetOffset();
                            lbArgIndirect      = (luIndirect != 0);
                            lpData             = reinterpret_cast<u8*>(lpArgSlot->miValue);
                            lpArgSlot->muType  = KI_ARG_TAG;
                            lpArgSlot->miValue = reinterpret_cast<s64>(lpOutCursor);
                        }

                        if (lChildDesc & 0x40)
                        {
                            u16 luSkip;                                 // v45
                            lhr = mSchemaAccess.GetWord(&luSkip);
                            if (lhr < 0)
                            {
                                return lhr;
                            }
                        }

                        u32 luElemSize  = 1u << (lChildDesc & 3);       // v27
                        u8* lpSrcData;                                  // v30 / v46
                        u32 luElemCount;                                // v31 / v48
                        u8  lChildConformDesc[2] = { 0, 0 };            // v40

                        if (lChildDesc & 0x20)
                        {
                            u32 luConform = muDefaultConformance;       // v48
                            lhr = mConformance.GetConformingScopeAndValue(lChildDesc, &mSchemaAccess,
                                                                          lChildConformDesc, &luConform);
                            if (lhr < 0)
                            {
                                return lhr;
                            }

                            bool lbDeref;                               // v28
                            u8*  lpResolved;                            // v29
                            if ((lChildConformDesc[0] & 0x80) != 0 || (lChildDesc & 0x80) != 0)
                            {
                                lbDeref    = false;
                                lpResolved = lpData;
                            }
                            else
                            {
                                lbDeref    = true;
                                lpResolved = lpData ? *reinterpret_cast<u8**>(lpData) : 0;
                            }

                            lpSrcData = lpResolved;

                            if (lChildConformDesc[0] & 0x40)
                            {
                                lbString = true;
                                if (luConform == 0)
                                {
                                    luConform = static_cast<u32>(
                                        wcslen(reinterpret_cast<const wchar_t*>(lpResolved))) + 1;
                                }
                            }

                            lhr = mConformance.ReleaseConformanceInfo(lChildConformDesc[0] & 0x3F);
                            if (lhr < 0)
                            {
                                return lhr;
                            }

                            luElemCount = luConform;
                            if (lbDeref)
                            {
                                luElemSize = 4;
                            }
                            else
                            {
                                luElemSize *= luConform;
                            }
                        }
                        else
                        {
                            luElemCount = 1;
                            lpSrcData   = lpData;
                        }

                        if (lbString)
                        {
                            int liBytes = WideCharToMultiByte(
                                KU_CP_UTF8, 0,
                                reinterpret_cast<const wchar_t*>(lpSrcData),
                                static_cast<int>(luElemCount),
                                reinterpret_cast<char*>(mBuffer.GetBase() + mBuffer.GetOffset()),
                                static_cast<int>(mBuffer.GetSize() - mBuffer.GetOffset()), 0, 0);
                            lhr = mBuffer.AppendBytes(static_cast<u32>(liBytes));
                            if (lhr < 0)
                            {
                                return lhr;
                            }
                            lhr = mConformance.UpdateConformance(lChildConformDesc[0] & 0x3F,
                                                                 static_cast<u64>(static_cast<u32>(liBytes)), 1);
                            if (lhr < 0)
                            {
                                return lhr;
                            }
                        }
                        else if (lpSrcData)
                        {
                            // Load the raw value (all sizes) for a later conformance establish,
                            // then write it out unless the field is argument-indirect.
                            switch (lChildDesc & 3) // v34
                            {
                            case 0:
                                luWriteValue = *lpSrcData;
                                if (!lbArgIndirect)
                                {
                                    lhr = mBuffer.WriteBytes(lpSrcData, luElemCount);
                                    if (lhr < 0)
                                    {
                                        return lhr;
                                    }
                                }
                                break;
                            case 1:
                                luWriteValue = *reinterpret_cast<const u16*>(lpSrcData);
                                if (!lbArgIndirect)
                                {
                                    lhr = mBuffer.WriteWords(reinterpret_cast<const u16*>(lpSrcData), luElemCount);
                                    if (lhr < 0)
                                    {
                                        return lhr;
                                    }
                                }
                                break;
                            case 2:
                                luWriteValue = *reinterpret_cast<const u32*>(lpSrcData);
                                if (!lbArgIndirect)
                                {
                                    lhr = mBuffer.WriteDwords(reinterpret_cast<const u32*>(lpSrcData), luElemCount);
                                    if (lhr < 0)
                                    {
                                        return lhr;
                                    }
                                }
                                break;
                            default: // 3
                                luWriteValue = *reinterpret_cast<const u64*>(lpSrcData);
                                if (!lbArgIndirect)
                                {
                                    lhr = mBuffer.WriteQwords(reinterpret_cast<const u64*>(lpSrcData), luElemCount);
                                    if (lhr < 0)
                                    {
                                        return lhr;
                                    }
                                }
                                break;
                            }
                        }

                        // Advance the source cursor past this leaf.
                        if ((lParentDesc & 0xC) == 8)
                        {
                            luElemSize = luCountWord2;
                        }
                        if (lpSrcData)
                        {
                            lpData += luElemSize;
                        }

                        // A leaf tagged 0x10 also establishes a nested conformance slot from
                        // its just-read value.
                        if (lChildDesc & 0x10)
                        {
                            u8 lConformInfo[4] = { 0, 0, 0, 0 };        // v51 (+ v52 word at +2)
                            lhr = mSchemaAccess.GetConformanceInfo(lConformInfo);
                            if (lhr < 0)
                            {
                                return lhr;
                            }
                            u16 luSel = *reinterpret_cast<const u16*>(lConformInfo + 2); // v52
                            u32 luConst;                                // v47
                            lhr = mSchemaAccess.GetSchema()->LookupConstantFromTable(luSel, &luConst);
                            if (lhr < 0)
                            {
                                return lhr;
                            }
                            u32 luScope = lConformInfo[0] >> 6;
                            u8* lpDest  = (mBuffer.GetBase() + mBuffer.GetOffset()) - (1u << luScope);
                            lhr = mConformance.EstablishConformanceInfo(lConformInfo[0] & 0x3F, luScope,
                                                                        luWriteValue, luConst,
                                                                        lConformInfo, lpDest);
                            if (lhr < 0)
                            {
                                return lhr;
                            }
                        }
                        // ============== end leaf field ==============
                    }
                    else if (luChildKind != 3)
                    {
                        // ============ nested scope (kind 1/2) ============
                        mSchemaAccess.SeekTo(luFieldSchemaOffset);

                        u8* lpChildData;                                // v46
                        if ((lChildDesc & 0x20) == 0 || (lChildDesc & 0x80) != 0)
                        {
                            lpChildData = lpData;
                        }
                        else
                        {
                            lpChildData = lpData ? *reinterpret_cast<u8**>(lpData) : 0;
                        }

                        lhr = RecursiveMarshal(luDepth + 1, &lpChildData);
                        if (lhr < 0)
                        {
                            return lhr;
                        }

                        if (lChildDesc & 0x20)
                        {
                            lpData += 4;
                        }
                        else if ((lParentDesc & 0xC) == 8)
                        {
                            lpData += luCountWord2;
                        }
                        else
                        {
                            lpData = lpChildData;
                        }
                    }
                    // luChildKind == 3: no field body (falls to the post-field step).
                }

                // ---- post-field step ----
                if ((lParentDesc & 0xC) == 8)
                {
                    ++luMemberIdx;
                }

                if ((lChildDesc & 0xC) == 0xC)
                {
                    // End-of-scope marker: either repeat the scope or finalize it.
                    ++luRepeat;
                    if (luRepeat < luRepeatCount)
                    {
                        mSchemaAccess.SeekTo(luScopeChildStart); // rewind for the next element
                        if (luRepeat < luRepeatCount)
                        {
                            lbRestartScope = true;
                            break;
                        }
                    }
                    lbFinalize = true;
                    break;
                }
            } // field loop

            if (lbInvalid)
            {
                return KI_E_FAIL;
            }
            if (lbFinalize)
            {
                break;
            }
            (void)lbRestartScope; // re-enter the scope-repeat loop
        } // scope-repeat loop
    }

    // ---- finalize ----
    if (lParentDesc & 0x20)
    {
        lhr = mConformance.ReleaseConformanceInfo(lParentConformDesc[0] & 0x3F);
        if (lhr < 0)
        {
            return lhr;
        }
    }
    if (luRepeatCount == 0)
    {
        lhr = mSchemaAccess.SeekTo(luSavedSchemaOffset);
        if (lhr < 0)
        {
            return lhr;
        }
        lhr = mSchemaAccess.SkipScope(0);
        if (lhr < 0)
        {
            return lhr;
        }
    }
    *lppData = lpData;
    return lhr;
}

// ---------------------------------------------------------------------------
// CMarshaller::Marshal @ 0x8297FDE8
// Marshal the whole argument list from the schema root: RecursiveMarshal at depth 0 with
// an empty inbound data pointer.
// ---------------------------------------------------------------------------
s32 CMarshaller::Marshal()
{
    u8* lpRoot = 0;
    return RecursiveMarshal(0, &lpRoot);
}
