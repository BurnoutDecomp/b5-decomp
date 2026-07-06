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

#include "SDKs/Xam/CSchemaAccess.h"

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
