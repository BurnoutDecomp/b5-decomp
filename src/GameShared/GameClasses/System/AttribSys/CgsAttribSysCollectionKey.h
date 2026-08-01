#pragma once

#include "types.hpp"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h"   // Attribute::Key / Attrib::StringToKey

// CgsAttribSys::AttribSysCollectionKey - a serialised asset GUID that resolves to
// an attribsys collection key: the 64-bit GUID is printed as decimal text and
// hashed with the attribsys string hash. DWARF home
// CgsAttribSysCollectionKey.h:52 ({s64 miAssetGuid}); gated on the X360 ledger.
// This TU bodies GetHashKey; Construct/Destruct/Set/FixUp are their own ledger
// functions (declaration-only here).
namespace CgsAttribSys
{
    struct AttribSysCollectionKey
    {
        // DWARF h:57/h:60/h:65/h:73 -- declaration-only.
        void Construct();
        void Destruct();
        void Set(s64 liAssetGuid);
        void FixUp();

        // @0x82805C20 (this TU, DWARF cpp:79) -- decimal-print the GUID and hash it.
        // RETURNS 64 BITS (widened 2026-08-01, physics wave 1): the X360 body tail-calls
        // Attrib::StringToKey and returns r3 whole, and every consumer of the value stores it
        // with `std` (see the .cpp banner). It is the collection key
        // VehicleManager::ProcessCreateEvents hands to Attrib::FindCollection, which hashes
        // the full doubleword -- a 32-bit key can never match.
        u64 GetHashKey() const;

    private:
        s64 miAssetGuid;   // +0x00 (DWARF h:94; GetHashKey `ld r3, 0(this)`)
    };
}
