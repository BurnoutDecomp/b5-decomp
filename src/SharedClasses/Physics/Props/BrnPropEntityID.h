#pragma once

// BrnWorld::PropEntityID — a prop entity handle that packs an entity index + part
// index into a single EntityId word. Reconstructed from the DecFIGS DWARF. Minimal
// recon: the data member only (its accessors are inlined / live in their own TUs).
#include "BrnCommonTypes.h"  // EntityId

namespace BrnWorld
{
    struct PropEntityID
    {
        EntityId mEntityId;
    };
}
