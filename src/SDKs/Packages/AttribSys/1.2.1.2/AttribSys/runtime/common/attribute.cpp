#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribute.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX (AttribSys v1.2.1.2).
//   Attrib::Attribute::Attribute  @ 0x82805AF0
//   Attrib::Attribute::IsInherited @ 0x82803600
//
// Node::GetPointer resolves an attribute node's value pointer inside an instance's
// attribute-data block; its body lives in its own slice (declared in attribute.h, trap
// stub here until that TU lands).

namespace Attrib
{
    // Node::GetPointer — own TU; trap stub until it lands.
    void* Node::GetPointer(void*) { __debugbreak(); return nullptr; }

    // Bind a cursor to (instance, collection, node). Cache the value pointer eagerly
    // unless the node carries the no-pointer flag (0x2). The cached pointer is resolved
    // from the instance's layout block (instance.mpAttributeData, +4).
    Attribute::Attribute(const Instance& lrInstance, const Collection* lpCollection, Node* lpNode)
        : mpInstance(&lrInstance), mpCollection(lpCollection), mpNode(lpNode), mpData(nullptr)
    {
        if (lpNode && (lpNode->muFlags & 0x2u) == 0)
            mpData = lpNode->GetPointer(lrInstance.GetLayoutPointer());
    }

    // An attribute is inherited when its value is not anchored to the instance's own
    // resolved collection. A missing node or missing recorded collection counts as
    // inherited (a default); the 0x10 ("local") node flag forces non-inherited; otherwise
    // it is inherited iff the instance's currently-resolved collection differs from the
    // collection this attribute was recorded against. (The X360 body branches on the
    // instance's modified flag (+0xC bit0) but both branches compute the same comparison,
    // so the result collapses to a single inequality.)
    bool Attribute::IsInherited()
    {
        if (!mpNode)
            return true;
        if (!mpCollection)
            return true;
        if ((mpNode->muFlags & 0x10u) != 0)
            return false;
        return mpInstance->GetResolvedCollection() != mpCollection;
    }
}
