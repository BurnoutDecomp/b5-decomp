#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribute.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribarray.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX (AttribSys v1.2.1.2).
//   Attrib::Attribute::Attribute          @ 0x82805AF0
//   Attrib::Attribute::IsInherited        @ 0x82803600
//   Attrib::Attribute::GetLength          @ 0x82805B60
//   Attrib::Attribute::GetInternalPointer @ 0x82805B88
//
// Node::GetPointer resolves an attribute node's value pointer inside an instance's
// attribute-data block; its body lives in its own slice (declared in attribute.h, trap
// stub here until that TU lands).

namespace Attrib
{
    // Node value resolvers — own TUs; trap stubs until they land.
    void*        Node::GetPointer(void*) { __debugbreak(); return nullptr; }
    void*        Node::GetPointer(void*, const Collection*) { __debugbreak(); return nullptr; }
    unsigned int Node::GetCount(void*, const Collection*) { __debugbreak(); return 0; }

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

    // Attrib::Attribute::GetLength @ 0x82805B60
    // Element count of the array this cursor resolved to: forward to the node's GetCount
    // with the instance's layout block and resolved collection, or 0 when unbound.
    int Attribute::GetLength()
    {
        if (mpNode)
            return static_cast<int>(mpNode->GetCount(mpInstance->GetLayoutPointer(),
                                                     mpInstance->GetResolvedCollection()));
        return 0;
    }

    // Attrib::Attribute::GetInternalPointer @ 0x82805B88
    // Resolve the raw pointer to element luIndex of this cursor's value.
    void* Attribute::GetInternalPointer(u32 luIndex)
    {
        Node* lpNode = mpNode;
        if (!lpNode)
            return nullptr;

        const u8 lu8Flags = lpNode->muFlags;
        if ((lu8Flags & 0x2u) == 0)
        {
            // Non-array node: only element 0 is meaningful; resolve its instance pointer.
            if (luIndex == 0)
                return lpNode->GetPointer(mpInstance->GetLayoutPointer(),
                                          mpInstance->GetResolvedCollection());
            return nullptr;
        }

        // Array-typed node: locate the Array header, then index element luIndex.
        if ((lu8Flags & 0x10u) != 0)
        {
            // Laid out: header at (instance layout base + node byte offset).
            u8* lpBase = static_cast<u8*>(mpInstance->GetLayoutPointer());
            Array* lpArray = reinterpret_cast<Array*>(lpBase + lpNode->muValue);
            return lpArray->GetData(luIndex);
        }
        if ((lu8Flags & 0x20u) != 0)
        {
            // Inherited: header lives in the resolved class's inherited data area. The
            // class/database internals are owned by the AttribSys SDK (un-homed here);
            // walked by the X360-attested byte offsets exactly as the asm does
            // (collection->mpClass -> [+0x08] -> [+0x34] == the inherited data base),
            // mirroring the committed HashMap::Remove inherited branch.
            const Collection* lpCollection = mpInstance->GetResolvedCollection();
            u8* lpClass       = reinterpret_cast<u8*>(lpCollection->mpClass);
            u8* lpClassBlock  = *reinterpret_cast<u8**>(lpClass + 0x08);
            u8* lpDataBase    = *reinterpret_cast<u8**>(lpClassBlock + 0x34);
            Array* lpArray = reinterpret_cast<Array*>(lpDataBase + lpNode->muValue);
            return lpArray->GetData(luIndex);
        }

        // Plain: the node's own value word is the (X360 32-bit) Array pointer image.
        Array* lpArray = reinterpret_cast<Array*>(static_cast<uintptr_t>(lpNode->muValue));
        return lpArray->GetData(luIndex);
    }
}
