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
