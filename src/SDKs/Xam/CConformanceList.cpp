// CConformanceList.cpp -- XAM RPC conformance-slot table bodies (Xbox 360).
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX. The object is an
// array of 0x40 16-byte slots (see CConformanceList.h for the ENTRY layout); the
// four bodies below establish / acquire / resolve / write-back one slot while
// CMarshaller::RecursiveMarshal walks a typed message against its schema.
//
//   CConformanceList::EstablishConformanceInfo    @ 0x829802D0
//   CConformanceList::AcquireConformanceInfo       @ 0x82980350
//   CConformanceList::GetConformingScopeAndValue   @ 0x82980400
//   CConformanceList::UpdateConformance            @ 0x82980510

#include "SDKs/Xam/CConformanceList.h"

#include "SDKs/Xam/CBaseEndianBuffer.h"
#include "SDKs/Xam/CSchemaData.h"

// HRESULT-style status constants the bodies return (X360 rodata):
//   S_OK          0x00000000
//   E_INVALIDARG  0x80070057  (lis r3,-0x7FF9 ; ori r3,r3,0x57)
//   E_FAIL        0x80004005  (lis r3,-0x8000 ; ori r3,r3,0x4005)
static const s32 KI_S_OK         = 0;
static const s32 KI_E_INVALIDARG = static_cast<s32>(0x80070057);
static const s32 KI_E_FAIL       = static_cast<s32>(0x80004005);

// Slot status bits.
static const u32 KU_FLAG_ESTABLISHED = 0x80000000u; // slot has been initialised
static const u32 KU_FLAG_ACQUIRED    = 0x40000000u; // slot is in use
static const u32 KU_SIZE_MASK        = 0x00000003u;  // element-size selector (log2)

// ---------------------------------------------------------------------------
// CConformanceList::EstablishConformanceInfo @ 0x829802D0
//
// Initialise slot `luIndex`. Rejects an out-of-range index (>= 0x40) or scope
// (> 3) with E_INVALIDARG, and an already-acquired slot with E_FAIL. When a
// descriptor is supplied and its byte[1] low bit is set, the incoming value is
// divided down by the element byte size (1 << luScope) -- the schema stores an
// element count that must be scaled to the raw element the wire cursor writes.
// The slot's flags are (re)written to established | (scope & 3), its destination
// span pointer to `lpData`, and its value to the (possibly scaled) value.
//
//   luUnused is the r7 argument the caller passes but this body never reads; it
//   keeps lpDescriptor (r8) and lpData (r9) at their asm-exact registers.
// ---------------------------------------------------------------------------
s32 CConformanceList::EstablishConformanceInfo(u32 luIndex, u32 luScope, u64 luValue,
                                               u32 luUnused, const u8* lpDescriptor,
                                               u8* lpData)
{
    (void)luUnused;

    if (luIndex >= KU_MAX_ENTRIES || luScope > 3)
    {
        return KI_E_INVALIDARG;
    }

    Entry& lEntry = maEntries[luIndex];
    if ((lEntry.muFlags & KU_FLAG_ACQUIRED) != 0)
    {
        return KI_E_FAIL;
    }

    // Serialised descriptor byte[1] bit0: value is an element count -> scale it to
    // the raw element size the cursor writes.
    if (lpDescriptor != 0 && (lpDescriptor[1] & 1) != 0)
    {
        luValue /= (static_cast<u64>(1) << luScope);
    }

    lEntry.muFlags = KU_FLAG_ESTABLISHED | (luScope & KU_SIZE_MASK);
    lEntry.mpData  = lpData;
    lEntry.muValue = luValue;
    return KI_S_OK;
}

// ---------------------------------------------------------------------------
// CConformanceList::AcquireConformanceInfo @ 0x82980350
//
// Acquire the established slot `luIndex`, mark it in-use and hand back a pointer
// to it. The slot must be established (0x80000000 set) and not already acquired
// (0x40000000 clear); otherwise E_FAIL. Out-of-range index -> E_INVALIDARG.
// ---------------------------------------------------------------------------
s32 CConformanceList::AcquireConformanceInfo(u32 luIndex, Entry** lppOut)
{
    if (luIndex >= KU_MAX_ENTRIES)
    {
        return KI_E_INVALIDARG;
    }

    Entry& lEntry = maEntries[luIndex];
    if ((lEntry.muFlags & KU_FLAG_ESTABLISHED) == 0 ||
        (lEntry.muFlags & KU_FLAG_ACQUIRED) != 0)
    {
        return KI_E_FAIL;
    }

    lEntry.muFlags |= KU_FLAG_ACQUIRED;
    *lppOut = &lEntry;
    return KI_S_OK;
}

// ---------------------------------------------------------------------------
// CConformanceList::GetConformingScopeAndValue @ 0x82980400
//
// Resolve the conforming field whose descriptor is read from `lpAccess`. The low
// 6 bits of the descriptor's first byte are the slot index; index 0x3F is the
// "no conformance" sentinel (return success, nothing resolved). When bit 0x80 of
// the descriptor is set the field names an explicit schema constant: read the
// selector word, look the constant up in the bound schema's table, and establish
// the slot with it (destination span 0, value = constant). Finally acquire the
// slot and return its value in *lpValue.
// ---------------------------------------------------------------------------
s32 CConformanceList::GetConformingScopeAndValue(u8 lScope, CSchemaAccess* lpAccess,
                                                 u8* lpDescriptor, u32* lpValue)
{
    s32 lhr = lpAccess->GetConformingInfo(lpDescriptor);
    if (lhr < 0)
    {
        return lhr;
    }

    u32 luIndex = lpDescriptor[0] & 0x3F;
    if (luIndex == 0x3F)
    {
        return lhr; // no conformance -- nothing to resolve
    }

    if ((lpDescriptor[0] & 0x80) != 0)
    {
        u16 luSelector;
        lhr = lpAccess->GetWord(&luSelector);
        if (lhr < 0)
        {
            return lhr;
        }

        u32 luConstant;
        lhr = lpAccess->GetSchema()->LookupConstantFromTable(luSelector, &luConstant);
        if (lhr < 0)
        {
            return lhr;
        }

        // Inlined establish: no descriptor scaling on this path, destination span 0.
        u32 luEstIndex = lpDescriptor[0] & 0x3F;
        if (luEstIndex >= KU_MAX_ENTRIES || (lScope & KU_SIZE_MASK) > 3)
        {
            return KI_E_INVALIDARG;
        }

        Entry& lEntry = maEntries[luEstIndex];
        if ((lEntry.muFlags & KU_FLAG_ACQUIRED) != 0)
        {
            return KI_E_FAIL;
        }

        lEntry.muFlags = KU_FLAG_ESTABLISHED | (lScope & KU_SIZE_MASK);
        lEntry.mpData  = 0;
        lEntry.muValue = static_cast<u64>(luConstant);
    }

    Entry* lpEntry;
    lhr = AcquireConformanceInfo(lpDescriptor[0] & 0x3F, &lpEntry);
    if (lhr >= 0)
    {
        *lpValue = static_cast<u32>(lpEntry->muValue);
    }
    return lhr;
}

// ---------------------------------------------------------------------------
// CConformanceList::UpdateConformance @ 0x82980510
//
// Write `luValue` out through slot `luIndex`'s destination span. A local cursor
// is bound over the span for exactly one element (1 << (flags & 3) bytes), then
// the value's low 1/2/4/8 bytes are written per the slot's element-size selector
// (byte / word / dword / qword), byte-swapping per lbSwapEndian. Returns the
// write status, or S_OK for an (impossible) out-of-range size selector.
// ---------------------------------------------------------------------------
s32 CConformanceList::UpdateConformance(u32 luIndex, u64 luValue, u32 lbSwapEndian)
{
    Entry& lEntry = maEntries[luIndex];
    u32 luElemBytes = 1u << (lEntry.muFlags & KU_SIZE_MASK);

    CBaseEndianBuffer lCursor;
    lCursor.Bind(lEntry.mpData, luElemBytes, luElemBytes, lbSwapEndian);

    switch (lEntry.muFlags & KU_SIZE_MASK)
    {
    case 0:
    {
        u8 luByte = static_cast<u8>(luValue);
        return lCursor.WriteBytes(&luByte, 1);
    }
    case 1:
    {
        u16 luWord = static_cast<u16>(luValue);
        return lCursor.WriteWords(&luWord, 1);
    }
    case 2:
    {
        u32 luDword = static_cast<u32>(luValue);
        return lCursor.WriteDwords(&luDword, 1);
    }
    case 3:
        return lCursor.WriteQwords(&luValue, 1);
    }

    return KI_S_OK;
}
