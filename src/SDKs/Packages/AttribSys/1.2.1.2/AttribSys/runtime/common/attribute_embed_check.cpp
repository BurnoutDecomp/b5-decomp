// Translation-unit embed check for Attrib::Attribute.
// Forces the owning header to compile standalone and exercises the bodied ctor and
// IsInherited so their signatures stay wired to their home. Also pins the Node flags
// byte to +0xF (the offset the X360 bodies read: node[0xF] & 0x2 / & 0x10).
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribute.h"
#include <cstddef>

static_assert(offsetof(Attrib::Node, muFlags) == 0xF,
              "Attrib::Node flags byte must sit at +0xF (X360 reads node[0xF])");

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
