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
    ::Attribute::Key StringToKey(const char* lpcText, u32 luLength, u64 luSeed);

    // The NUL-terminated convenience form (@0x82805828; MomentPlayerStunt hashes
    // its take-guid strings through it). DECLARATION-ONLY.
    ::Attribute::Key StringToKey(const char* lpcText);
}
