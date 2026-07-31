#pragma once

// Attrib's public key type, spelled Attribute::Key in the generated/serialised event
// code (distinct namespace from the Attrib runtime). 32-bit on the X360 spine.
#include "types.hpp"

namespace Attribute
{
    typedef u32 Key;
    typedef u32 Type;   // attribute type id, parallel to Key; 32-bit on the X360 spine.
    typedef u32 HashInt; // hashed asset/string id (Attrib::Vault::AssetID); 32-bit on the X360 spine.
}

namespace Attrib
{
    // The attribsys text hash (a Bob-Jenkins lookup3 over the string) with the
    // explicit 64-bit seed the X360 build stages at every call site
    // (0xABCDEF00_11223344 -- e.g. CgsAttribSys::AttribSysCollectionKey::GetHashKey
    // @0x82805C80, insrdi-built into r5). The PS3 DWARF spells the wrapper
    // Attrib::StringToKey(const char*, unsigned int) with the seed baked in; the
    // X360 symbol @0x82802940 takes it as the third argument -- declared per the
    // X360 ABI. Declaration-only (its own ledger function).
    // Fully qualify ::Attribute -- inside namespace Attrib an unqualified `Attribute::`
    // binds to the Attrib::Attribute cursor class in any TU where attribute.h is also
    // visible (e.g. attribute.cpp). ::Attribute is this file's own Key/Type namespace.
    // ⭐ RETURN WIDTH CORRECTED 2026-07-31, and it was a real defect. Both of these used
    // to be declared returning the 32-bit ::Attribute::Key, and attribhash64.cpp
    // implemented that with an explicit `static_cast<::Attribute::Key>` over the 64-bit
    // hash. The X360 body has NO truncation in it:
    //
    //   Attrib::StringToKey @0x82805828   -- walk the string for its length, stage the
    //     0xABCDEF00_11223344 seed into r5, then `b sub_82802940` -- a TAIL CALL into the
    //     64-bit lookup8 hash, whose r3 is returned whole. No clrldi, no lwz, nothing.
    //
    // The PS3 DWARF agrees: Attrib::StringToKey(const char*) is a thunk whose entire body
    // is `return Attrib::StringHash64(str)`.
    //
    // This mattered: DirectorResourceManager::Prepare builds all 65 of its shot-group slots
    // as `shotgroup(Attrib::StringToKey("606002"), 0)` and the result passes straight
    // through the generated ctor's r4 into Attrib::FindCollection's COLLECTION KEY, which
    // the class table hashes as a full doubleword. Truncated to 32 bits, every one of the
    // 65 lookups misses -- the identical defect the CLASS key had until c27208fc.
    u64 StringToKey(const char* lpcText, u32 luLength, u64 luSeed);

    // The NUL-terminated convenience form (@0x82805828; MomentPlayerStunt hashes
    // its take-guid strings through it). Body: attribhash64.cpp.
    u64 StringToKey(const char* lpcText);

    // The seed every X360 StringToKey site stages (lis/ori/insrdi pairs).
    const u64 KU_ATTRIB_STRING_TO_KEY_SEED = 0xABCDEF0011223344ull;

    // The full 64-bit hash the X360 body @0x82802940 computes (Bob Jenkins lookup8
    // "hash64"). Identical in value to the two StringToKey forms above now that they no
    // longer narrow; kept as the explicit spelling the schema/type-id sites already use.
    // The 64-bit VALUE is what the vault/schema
    // containers store: class keys, dependency ids, export ids, type ids and
    // Definition keys are all full 64-bit hashes of their source strings
    // (attested: hash64('boostparamsasset') == 0xDA21657C48943FAC, the schema
    // DepN ids == hash64('schema.vlt'/'schema.bin'), gDatabaseType ==
    // hash64('Attrib::DatabaseLoadData'), the Definition mType fields ==
    // hash64('EA::Reflection::<type>')). Body: attribhash64.cpp.
    u64 StringToKey64(const char* lpcText, u32 luLength, u64 luSeed);
}
