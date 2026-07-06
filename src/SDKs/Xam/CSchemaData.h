#ifndef SDKS_XAM_CSCHEMADATA_H
#define SDKS_XAM_CSCHEMADATA_H

#include "types.hpp"

// ===========================================================================
// SDKs/Xam/CSchemaData.h
//
// CSchemaData -- the per-schema descriptor the Xbox 360 XAM RPC marshalling
// layer consults while (un)marshalling a typed message. CMarshaller::Recursive-
// Marshal and CConformanceList::GetConformingScopeAndValue read constant values
// out of the schema's constant table through LookupConstantFromTable. Sibling of
// CArgumentList / CMarshaller / CConformanceList.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (LookupConstantFromTable @
// 0x8297F280). There is no reference source and no DWARF for this TU; the
// SHAPE below is grounded purely on the X360 asm. These are Microsoft-XDK-style
// C-prefixed marshalling classes, so the identifiers (CSchemaData,
// LookupConstantFromTable) are kept verbatim per the naming convention's
// external/platform-API carve-out.
//
// LAYOUT (from LookupConstantFromTable's asm -- only the two fields it reads are
// pinned; the rest of the object is opaque padding sized to reach them):
//     +0x18  muConstantCount  -- u16: highest valid constant index, inclusive
//                                (lhz r10,0x18(r3); the guard rejects index>count)
//     +0x40  mpConstantTable  -- const u32*: base of the constant value table
//                                (lwz r10,0x40(r3); indexed by `4 * index`)
// ===========================================================================

class CSchemaData
{
public:
    // @ 0x8297F280 -- read constant value `luIndex` from the schema's constant
    // table into *lpOut. Returns S_OK (0) on success, or E_FAIL (0x80004005) when
    // the index is past the table (luIndex > muConstantCount).
    s32 LookupConstantFromTable(u16 luIndex, u32* lpOut);

    // Look a schema field up by id and fill `lpEntry` (a CSchemaAccess::SchemaEntry:
    // two u16 sizes + two u32 offsets). Returns a negative status when the id is not
    // found. Body lives in the CSchemaData/CSchemaAccess marshalling TU; declared
    // here so CSchemaAccess::BindToSchema @ 0x8297F380 links against the real member.
    s32 GetSchemaEntry(u32 luSchemaId, void* lpEntry);

    // +0x34 -- base of the schema payload span BindToSchema binds a cursor over
    // (lwz r11,0x34(r31)). Read by CSchemaAccess::BindToSchema.
    u8* GetPayloadBase() const { return mpPayloadBase; }

private:
    u8        mPad0[0x18];          // +0x00 .. +0x17 (opaque)
    u16       muConstantCount;      // +0x18  highest valid index (inclusive)
    u8        mPad1[0x34 - 0x1A];   // +0x1A .. +0x33 (opaque)
    u8*       mpPayloadBase;        // +0x34  schema payload span base
    u8        mPad2[0x40 - 0x38];   // +0x38 .. +0x3F (opaque)
    const u32* mpConstantTable;     // +0x40  constant value table base
};

#endif // SDKS_XAM_CSCHEMADATA_H
