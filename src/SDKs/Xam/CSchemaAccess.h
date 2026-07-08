#ifndef SDKS_XAM_CSCHEMAACCESS_H
#define SDKS_XAM_CSCHEMAACCESS_H

#include "types.hpp"

#include "SDKs/Xam/CBaseEndianBuffer.h"
#include "SDKs/Xam/CSchemaData.h"

// ===========================================================================
// SDKs/Xam/CSchemaAccess.h
//
// CSchemaAccess -- the schema-aware read cursor the Xbox 360 XAM RPC
// marshalling layer uses to walk a wire-format message against its schema.
// It derives from CBaseEndianBuffer (the raw endian-aware byte cursor) and
// adds a bound-schema pointer plus the typed field accessors CMarshaller and
// CConformanceList call while (un)marshalling a message (GetDescriptor,
// GetArgumentInfo, GetConformanceInfo, GetConformingInfo, GetUnionInfo) and
// the recursive scope-skipper SkipScope.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. There is no reference source and
// no DWARF for this TU; the SHAPE below is grounded purely on the X360 asm.
// These are Microsoft-XDK-style C-prefixed marshalling classes, so the
// identifiers (CSchemaAccess, BindToSchema, ...) are kept verbatim per the
// naming convention's external/platform-API carve-out.
//
// LAYOUT
//   CSchemaAccess extends CBaseEndianBuffer (0x14 bytes: mpData@0, +0x04 word,
//   muSize@0x08, muOffset@0x0C, mbSwapEndian@0x10) and stores a single pointer
//   immediately after the base:
//     +0x14  mpSchema  CSchemaData*  the bound schema (BindToSchema: stw r?,0x14)
//
//   NOTE: the leading base-class pointer widens on the 64-bit host, so the
//   +0x14 member offset is NOT assertable here.
// ===========================================================================

class CSchemaAccess : public CBaseEndianBuffer
{
public:
    // The 0x0C-byte record CSchemaData::GetSchemaEntry fills for a schema id.
    // BindToSchema selects one of two (size, offset) pairs from it depending on
    // its `lbUseFirst` flag: the first pair lives at (+0x00 u16, +0x04 u32), the
    // second at (+0x02 u16, +0x08 u32). Field sizes are X360-attested (lhz vs
    // lwz); the naming reflects the first/second selection BindToSchema makes.
    struct SchemaEntry
    {
        u16 muSizeFirst;    // +0x00  (lhz var_40)  size for the lbUseFirst path
        u16 muSizeSecond;   // +0x02  (lhz var_3E)  size for the else path
        u32 muOffsetFirst;  // +0x04  (lwz var_3C)  offset for the lbUseFirst path
        u32 muOffsetSecond; // +0x08  (lwz var_38)  offset for the else path
    };

    // @ 0x8297F380 -- bind this cursor to a field of `lpSchema` identified by
    // `luSchemaId`. Looks the id up (CSchemaData::GetSchemaEntry), then binds the
    // base CBaseEndianBuffer over the schema payload span at the selected offset
    // for the selected size (endian-swap enabled). `lbUseFirst` picks the first
    // (size, offset) pair from the entry, otherwise the second. Returns the
    // GetSchemaEntry status (negative => not bound), else S_OK.
    s32 BindToSchema(CSchemaData* lpSchema, u32 luSchemaId, u32 lbUseFirst);

    // @ 0x8297EDB0 -- read the 1-byte field descriptor at the cursor into *lpDst.
    s32 GetDescriptor(void* lpDst);

    // @ 0x8297EDE0 -- read the 1-byte argument-info descriptor into *lpDst.
    s32 GetArgumentInfo(void* lpDst);

    // @ 0x8297EE10 -- read the 2-byte conformance descriptor into *lpDst, then the
    // trailing conformance word into (lpDst + 2).
    s32 GetConformanceInfo(void* lpDst);

    // @ 0x8297EE68 -- read the 2-byte conforming descriptor at the cursor into
    // *lpDst.
    s32 GetConformingInfo(void* lpDst);

    // @ 0x8297EE98 -- read the 2-byte union descriptor into *lpDst; when its 0x40
    // bit is set, also read the union selector word into *lpSelector.
    s32 GetUnionInfo(u8* lpDst, void* lpSelector);

    // @ 0x8297EEE8 -- recursively skip a whole schema scope, validating each
    // field descriptor against the wire data (bounded to depth 0x10).
    s32 SkipScope(u32 luDepth);

    // The bound schema descriptor (read at +0x14). CConformanceList::GetConforming-
    // ScopeAndValue reads it back off the cursor (lwz r3,0x14(r31)) to resolve a
    // constant through the schema's constant table.
    CSchemaData* GetSchema() const { return mpSchema; }

private:
    CSchemaData* mpSchema; // +0x14  bound schema descriptor
};

#endif // SDKS_XAM_CSCHEMAACCESS_H
