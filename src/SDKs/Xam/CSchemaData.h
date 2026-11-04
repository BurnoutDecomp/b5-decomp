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
// Layout from the native constant lookup, entry lookup and payload binding:
//     +0x18  muConstantCount  -- highest valid constant index, inclusive
//     +0x28  muEntryCount     -- number of schema entries
//     +0x2A  muEntryStride    -- source table stride in bytes
//     +0x30  mpEntryTable     -- schema entry table
//     +0x34  mpPayloadBase    -- payload span
//     +0x40  mpConstantTable  -- constant value table
// ===========================================================================

class CSchemaData
{
public:
    // @ 0x8297F280 -- read constant value `luIndex` from the schema's constant
    // table into *lpOut. Returns S_OK (0) on success, or E_FAIL (0x80004005) when
    // the index is past the table (luIndex > muConstantCount).
    s32 LookupConstantFromTable(u16 luIndex, u32* lpOut);

    enum { KU_SCHEMA_ENTRY_SIZE = 0x18 };

    // @ 0x8297F228 -- copy the complete native entry, or return E_FAIL.
    s32 GetSchemaEntry(u32 luSchemaId, void* lpEntry);

    // +0x34 -- base of the schema payload span BindToSchema binds a cursor over
    // (lwz r11,0x34(r31)). Read by CSchemaAccess::BindToSchema.
    u8* GetPayloadBase() const { return mpPayloadBase; }

private:
    u8        mPad0[0x18];          // +0x00 .. +0x17 (opaque)
    u16       muConstantCount;      // +0x18  highest valid index (inclusive)
    u8        mPad1[0x28 - 0x1A];   // +0x1A .. +0x27 (opaque)
    u16       muEntryCount;         // +0x28
    u16       muEntryStride;        // +0x2A
    u8        mPadEntry[0x30 - 0x2C];
    const u8* mpEntryTable;         // +0x30
    u8*       mpPayloadBase;        // +0x34  schema payload span base
    u8        mPad2[0x40 - 0x38];   // +0x38 .. +0x3F (opaque)
    const u32* mpConstantTable;     // +0x40  constant value table base
};

#endif // SDKS_XAM_CSCHEMADATA_H
