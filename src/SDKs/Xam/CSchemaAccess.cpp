// CSchemaAccess.cpp -- schema-aware read cursor bodies (Xbox 360 XAM marshalling).
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX. CSchemaAccess derives
// from CBaseEndianBuffer; the typed accessors below pin the element size in a stack
// word and forward to the base cursor's GetData / GetWord primitives. See
// CSchemaAccess.h for the layout.
//
//   CSchemaAccess::BindToSchema        @ 0x8297F380
//   CSchemaAccess::GetArgumentInfo     @ 0x8297EDE0
//   CSchemaAccess::GetConformanceInfo  @ 0x8297EE10
//   CSchemaAccess::GetConformingInfo   @ 0x8297EE68
//   CSchemaAccess::SkipScope           @ 0x8297EEE8

#include "SDKs/Xam/CSchemaAccess.h"

// E_FAIL HRESULT the malformed-descriptor guard paths return
// (lis r?,-0x8000 ; ori r?,r?,0x4005 == 0x80004005).
static const s32 KI_E_FAIL = static_cast<s32>(0x80004005);

// ---------------------------------------------------------------------------
// CSchemaAccess::BindToSchema @ 0x8297F380
//
// Stores the schema pointer at +0x14, looks the field up by id, and binds the
// base cursor over the selected (offset, size) slice of the schema payload:
//
//   stw   r31, 0x14(r28)              ; mpSchema = lpSchema
//   bl    CSchemaData::GetSchemaEntry ; entry <- lookup(lpSchema, luSchemaId)
//   blt   done                        ; negative status -> return it, not bound
//   lwz   r11, 0x34(r31)              ; r11 = lpSchema payload base (+0x34)
//   cmpwi r29, 0                      ; lbUseFirst ?
//   ...  size = entry.first/second ; off = entry.first/second
//   Bind(this, base + off, size, size, 1)
//
// GetSchemaEntry hands both its size and its offset words; BindToSchema passes the
// size as BOTH the luContext and luSize arguments of Bind (r5 and r6 are the same
// register here), with the endian-swap flag hard-set to 1.
// ---------------------------------------------------------------------------
s32 CSchemaAccess::BindToSchema(CSchemaData* lpSchema, u32 luSchemaId, u32 lbUseFirst)
{
    mpSchema = lpSchema;

    SchemaEntry lEntry;
    s32 lhr = lpSchema->GetSchemaEntry(luSchemaId, &lEntry);
    if (lhr >= 0)
    {
        u16 luSize;
        u32 luOffset;
        if (lbUseFirst)
        {
            luSize   = lEntry.muSizeFirst;
            luOffset = lEntry.muOffsetFirst;
        }
        else
        {
            luSize   = lEntry.muSizeSecond;
            luOffset = lEntry.muOffsetSecond;
        }

        lhr = 0;
        Bind(lpSchema->GetPayloadBase() + luOffset, luSize, luSize, 1);
    }

    return lhr;
}

// ---------------------------------------------------------------------------
// CSchemaAccess::GetArgumentInfo @ 0x8297EDE0
// Read the 1-byte argument-info descriptor at the cursor into *lpDst.
// ---------------------------------------------------------------------------
s32 CSchemaAccess::GetArgumentInfo(void* lpDst)
{
    u32 luSize = 1;
    return GetData(lpDst, &luSize);
}

// ---------------------------------------------------------------------------
// CSchemaAccess::GetConformanceInfo @ 0x8297EE10
// Read the 2-byte conformance descriptor into *lpDst, then (on success) the
// trailing conformance word into (lpDst + 2).
// ---------------------------------------------------------------------------
s32 CSchemaAccess::GetConformanceInfo(void* lpDst)
{
    u32 luSize = 2;
    s32 lhr = GetData(lpDst, &luSize);
    if (lhr >= 0)
    {
        return GetWord(static_cast<u8*>(lpDst) + 2);
    }
    return lhr;
}

// ---------------------------------------------------------------------------
// CSchemaAccess::GetConformingInfo @ 0x8297EE68
// Read the 2-byte conforming descriptor at the cursor into *lpDst.
// ---------------------------------------------------------------------------
s32 CSchemaAccess::GetConformingInfo(void* lpDst)
{
    u32 luSize = 2;
    return GetData(lpDst, &luSize);
}

// ---------------------------------------------------------------------------
// CSchemaAccess::GetDescriptor @ 0x8297EDB0
// Read the 1-byte field descriptor at the cursor into *lpDst. The size word is
// pinned to 1 and forwarded to the inherited GetData primitive.
// ---------------------------------------------------------------------------
s32 CSchemaAccess::GetDescriptor(void* lpDst)
{
    u32 luSize = 1;
    return GetData(lpDst, &luSize);
}

// ---------------------------------------------------------------------------
// CSchemaAccess::GetUnionInfo @ 0x8297EE98
// Read the 2-byte union descriptor at the cursor into *lpDst. If bit 0x40 of its
// first byte is set, a trailing 16-bit union selector word follows; read it into
// *lpSelector. (rlwinm.,0,25,25 isolates bit 0x40 of the low byte.)
// ---------------------------------------------------------------------------
s32 CSchemaAccess::GetUnionInfo(u8* lpDst, void* lpSelector)
{
    u32 luSize = 2;
    s32 lhr = GetData(lpDst, &luSize);
    if (lhr >= 0 && (lpDst[0] & 0x40) != 0)
    {
        lhr = GetWord(lpSelector);
    }
    return lhr;
}

// ---------------------------------------------------------------------------
// CSchemaAccess::SkipScope @ 0x8297EEE8
//
// Recursively step the cursor past a whole schema scope, validating every field
// descriptor against the wire data as it goes (depth-bounded to 0x10 to bound
// recursion). Each field is a descriptor byte whose low bits select a class:
//   (desc >> 2) & 3 == 0  scalar/leaf field   -> consume its optional trailers
//   (desc >> 2) & 3 == 1  nested scope         -> rewind + recurse (SkipScope)
//   (desc >> 2) & 3 == 3  scope terminator     -> stop
// and whose bit flags 0x80/0x40/0x20/0x10 gate optional trailing words
// (argument byte, count word, 2-byte conformance descriptor + its own 0x80
// selector word, conformance info). A union scope ((desc & 0xC) == 8) first
// reads a member count (GetUnionInfo) and walks that many members, each a byte
// count of index words followed by a data-pointer whose first byte is the next
// descriptor. Malformed class bits return E_FAIL; any buffer read error is
// returned verbatim. Faithful store-for-store translation of the X360 body
// (its branch structure -- including the reads-into-the-loop-tail sharing --
// is preserved with labels rather than restructured).
// ---------------------------------------------------------------------------
s32 CSchemaAccess::SkipScope(u32 luDepth)
{
    s32  lhr;
    s32  luClass;
    u16  luMember;
    bool lbTerminal;
    s32  luWordIdx;
    u32  luSavedOffset;
    s32  luNestedClass;
    u8   lDesc;         // [sp+50h] nested field descriptor
    u8   lTopDesc;      // [sp+51h] this scope's opening field descriptor
    u8   luConfCount;   // [sp+52h] index-word count for a union member
    char lArgByte;      // [sp+53h] argument byte (0x80 trailer)
    u16  luScratchWord; // [sp+54h] scratch 16-bit read sink
    u8   lConfDesc[2];  // [sp+56h] 2-byte conformance descriptor (0x20 trailer)
    u16  luUnionCount;  // [sp+58h] union member count
    u32  luSize;        // [sp+5Ch] element size pinned per GetData call
    u32  luPtrSize;     // [sp+60h] GetDataPointer span size
    u8*  lpData;        // [sp+64h] GetDataPointer span base
    char lConfInfo;     // [sp+68h] conformance-info sink (0x10 trailer)

    if (luDepth >= 0x10)
        goto invalid;

    luSize = 1;
    lhr = GetData(&lTopDesc, &luSize);
    if (lhr < 0)
        goto done;

    luClass = (lTopDesc >> 2) & 3;
    if (luClass == 0 || (luClass != 1 && ((lTopDesc >> 2) & 3) == 3))
    {
invalid:
        lhr = KI_E_FAIL;
        goto done;
    }

    lhr = GetWord(&luScratchWord);
    if (lhr >= 0)
    {
        if ((lTopDesc & 0x80) == 0
            || (luSize = 1, lhr = GetData(&lArgByte, &luSize), lhr >= 0))
        {
            if ((lTopDesc & 0x40) == 0
                || (lhr = GetWord(&luScratchWord), lhr >= 0))
            {
                if ((lTopDesc & 0x20) == 0
                    || (luSize = 2, lhr = GetData(lConfDesc, &luSize), lhr >= 0)
                        && ((lConfDesc[0] & 0x80) == 0
                            || (lhr = GetWord(&luScratchWord), lhr >= 0)))
                {
                    if ((lTopDesc & 0xC) == 8)
                    {
                        lhr = GetUnionInfo(reinterpret_cast<u8*>(&luUnionCount), &luScratchWord);
                        if (lhr >= 0)
                        {
                            luMember = 0;
                            goto unionLoop;
                        }
                        goto done;
                    }
                    luMember = luUnionCount;
unionLoop:
                    while ((lTopDesc & 0xC) == 8)
                    {
                        if (luMember >= luUnionCount)
                        {
                            luSize = 1;
                            lhr = GetData(&lDesc, &luSize);
                            if (lhr < 0)
                                goto done;
                            lbTerminal = (lDesc & 0xC) == 12;
checkTerminal:
                            if (!lbTerminal)
                                goto invalid;
                            goto afterField;
                        }
                        lhr = GetByte(&luConfCount);
                        if (lhr < 0)
                            goto done;
                        luWordIdx = 0;
                        if (luConfCount)
                        {
                            do
                            {
                                lhr = GetWord(&luScratchWord);
                                if (lhr < 0)
                                    goto done;
                            }
                            while (++luWordIdx < luConfCount);
                        }
                        lhr = GetWord(&luScratchWord);
                        if (lhr < 0)
                            goto done;
                        luPtrSize = luScratchWord;
                        lhr = GetDataPointer(reinterpret_cast<void**>(&lpData), &luPtrSize);
                        if (lhr < 0)
                            goto done;
                        lDesc = *lpData;
afterField:
                        if ((lTopDesc & 0xC) == 8)
                            ++luMember;
                        if ((lDesc & 0xC) == 0xC)
                            goto done;
                    }

                    luSavedOffset = GetOffset();
                    luSize = 1;
                    lhr = GetData(&lDesc, &luSize);
                    if (lhr < 0)
                        goto done;
                    luNestedClass = (lDesc >> 2) & 3;
                    if (luNestedClass == 0)
                    {
                        if ((lDesc & 0x80) != 0)
                        {
                            luSize = 1;
                            lhr = GetData(&lArgByte, &luSize);
                            if (lhr < 0)
                                goto done;
                        }
                        if ((lDesc & 0x40) != 0)
                        {
                            lhr = GetWord(&luScratchWord);
                            if (lhr < 0)
                                goto done;
                        }
                        if ((lDesc & 0x20) != 0)
                        {
                            luSize = 2;
                            lhr = GetData(lConfDesc, &luSize);
                            if (lhr < 0)
                                goto done;
                            if ((lConfDesc[0] & 0x80) != 0)
                            {
                                lhr = GetWord(&luScratchWord);
                                if (lhr < 0)
                                    goto done;
                            }
                        }
                        if ((lDesc & 0x10) == 0)
                            goto afterField;
                        lhr = GetConformanceInfo(&lConfInfo);
                        goto checkStatus;
                    }
                    lbTerminal = luNestedClass == 3;
                    if (((lDesc >> 2) & 3) != 3)
                    {
                        SeekTo(luSavedOffset);
                        lhr = SkipScope(luDepth + 1);
checkStatus:
                        if (lhr < 0)
                            goto done;
                        goto afterField;
                    }
                    goto checkTerminal;
                }
            }
        }
    }
done:
    return lhr;
}
