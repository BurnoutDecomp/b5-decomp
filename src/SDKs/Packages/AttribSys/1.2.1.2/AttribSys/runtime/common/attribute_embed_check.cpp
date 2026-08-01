// Translation-unit embed check for Attrib::Attribute.
// Forces the owning header to compile standalone and exercises the bodied ctor and
// IsInherited so their signatures stay wired to their home.
//
// ⭐ THE LAYOUT PIN CHANGED 2026-08-01. It used to be
//     static_assert(offsetof(Attrib::Node, muFlags) == 0xF, ...)
// -- a CONSOLE byte literal, and it was actively harmful: it pinned Attrib::Node to the
// X360's 16-byte shape while its alias Attrib::HashMap::Node had already (correctly) grown
// its payload slot to a host machine word. Attrib::Collection::GetNode reinterpret_casts one
// to the other, so the two structs MUST agree, and the console offset is exactly what they
// cannot both honour. The pin is now a CROSS-CHECK between the two spellings -- which is the
// invariant that actually matters and which holds on either target.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribute.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribhashmap.h"
#include <cstddef>

static_assert(sizeof(Attrib::Node) == sizeof(Attrib::HashMap::Node),
              "Attrib::Node and Attrib::HashMap::Node are ONE console type -- "
              "Collection::GetNode casts between them");
static_assert(offsetof(Attrib::Node, mKey) == offsetof(Attrib::HashMap::Node, mKey),
              "Node key slot must agree between the two spellings");
static_assert(offsetof(Attrib::Node, mpValue) == offsetof(Attrib::HashMap::Node, mpValue),
              "Node payload slot must agree between the two spellings");
static_assert(offsetof(Attrib::Node, mTypeIndex) == offsetof(Attrib::HashMap::Node, mTypeIndex),
              "Node type index must agree between the two spellings");
static_assert(offsetof(Attrib::Node, mMax) == offsetof(Attrib::HashMap::Node, mu8SearchLen),
              "Node probe-run byte must agree between the two spellings");
static_assert(offsetof(Attrib::Node, muFlags) == offsetof(Attrib::HashMap::Node, mFlags),
              "Node FLAGS byte must agree between the two spellings -- this is the one that "
              "silently made every generated Num_<array>() report zero elements");

namespace
{
void EmbedCheck()
{
    Attrib::Node lNode;
    lNode.muFlags = 0;

    Attrib::Instance lInstance(nullptr, nullptr);
    Attrib::Attribute lAttr(lInstance, nullptr, &lNode);
    bool lbInherited = lAttr.IsInherited();
    (void)lbInherited;
}
}
