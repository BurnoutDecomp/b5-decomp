#pragma once

// Attrib's public key type, spelled Attribute::Key in the generated/serialised event
// code (distinct namespace from the Attrib runtime). 32-bit on the X360 spine.
#include "types.hpp"

namespace Attribute
{
    typedef u32 Key;
}
