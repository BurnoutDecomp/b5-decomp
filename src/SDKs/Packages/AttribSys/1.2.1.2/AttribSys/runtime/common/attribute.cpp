#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribute.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribarray.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/attribsys.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX (AttribSys v1.2.1.2).
//   Attrib::Attribute::Attribute          @ 0x82805AF0
//   Attrib::Attribute::IsInherited        @ 0x82803600
//   Attrib::Attribute::GetLength          @ 0x82805B60
//   Attrib::Attribute::GetInternalPointer @ 0x82805B88
//   Attrib::Node::Node                    @ 0x82809430
//   Attrib::Node::GetCount                @ 0x82804610
//   Attrib::Node::GetTypeDesc             @ 0x828079E8
//
// Node::GetPointer resolves an attribute node's value pointer inside an instance's
// attribute-data block; its body lives in its own slice (declared in attribute.h, trap
// stub here until that TU lands).

namespace Attrib
{
    // Minimal EASTL vector<TypeDesc*> face -- only the begin/end pointers the committed
    // operator[] (@0x82803D48, bodied in AttribVector_TypeDescPtr_operator_index.cpp) reads.
    // GetTypeDesc dispatches through that same bounds-checked operator[].
    struct AttribTypeDescPtrVector
    {
        TypeDesc** mpBegin;   // +0x00
        TypeDesc** mpEnd;     // +0x04
        TypeDesc*& operator[](unsigned int luIndex);
    };

    // Node value resolvers — own TUs; trap stubs until they land.
    void*        Node::GetPointer(void*) { __debugbreak(); return nullptr; }
    void*        Node::GetPointer(void*, const Collection*) { __debugbreak(); return nullptr; }

    // Attrib::Node::Node @ 0x82809430
    // Placement-construct a node for (key, type, value). Stash the key and the value word,
    // resolve the schema TypeDesc for luTypeIndex from the attribute database and take its
    // indexed-type id (TypeDesc+0x10) as this node's type index, then set the occupied bit
    // (0x80) into the caller's flags byte. When the caller asks to lay the node out
    // (lbComputeOffset) and it is a laid-out (0x10) or inherited (0x20) node, rewrite the
    // value word as the byte offset of lpValue within its data-area base (lpValue - lpBase).
    // The X360 inlines the 'Attribute database not initialized.' assert that Database::Get()
    // fires before dereferencing the singleton.
    Node::Node(u64 luKey, u16 luTypeIndex, void* lpValue, bool lbComputeOffset,
               u8 lu8Flags, void* lpBase)
    {
        mKey    = luKey;
        muValue = static_cast<u32>(reinterpret_cast<uintptr_t>(lpValue));

        const TypeDesc& lrTypeDesc = Database::Get().GetTypeDesc(luTypeIndex);
        mTypeIndex = static_cast<u16>(lrTypeDesc.mReserved10); // indexed-type id @ TypeDesc+0x10
        muFlags    = static_cast<u8>(lu8Flags | 0x80u);        // set the occupied bit

        if (lbComputeOffset && ((muFlags & 0x10u) != 0 || (muFlags & 0x20u) != 0))
            muValue = static_cast<u32>(reinterpret_cast<u8*>(lpValue)
                                       - reinterpret_cast<u8*>(lpBase));
    }

    // Attrib::Node::GetCount @ 0x82804610
    // Number of elements this node's attribute holds, resolved from its Array header. A free
    // node (occupied bit 0x80 clear) has none; a non-array node (0x2 clear) counts as one.
    // For an array-typed node the Array header is located per its storage flags -- laid out
    // (0x10) at the instance layout base + node offset, inherited (0x20) in the resolved
    // class's inherited data area, else the node's own value word as the Array pointer image
    // -- and its live element count (muNumElements @ +0x2) returned.
    unsigned int Node::GetCount(void* lpLayout, const Collection* lpCollection)
    {
        const u8 lu8Flags = muFlags;

        // Unoccupied slot: no elements.
        if ((lu8Flags & 0x80u) == 0)
            return 0;

        // Non-array (scalar) attribute: exactly one element.
        if ((lu8Flags & 0x2u) == 0)
            return 1;

        Array* lpArray;
        if ((lu8Flags & 0x10u) != 0)
        {
            // Laid out: header at (instance layout base + node byte offset).
            lpArray = reinterpret_cast<Array*>(static_cast<u8*>(lpLayout) + muValue);
        }
        else if ((lu8Flags & 0x20u) != 0)
        {
            // Inherited: header in the resolved class's inherited data area
            // (collection->mpClass -> [+0x08] -> [+0x34]), mirroring GetInternalPointer.
            u8* lpClass      = reinterpret_cast<u8*>(lpCollection->mpClass);
            u8* lpClassBlock = *reinterpret_cast<u8**>(lpClass + 0x08);
            u8* lpDataBase   = *reinterpret_cast<u8**>(lpClassBlock + 0x34);
            lpArray = reinterpret_cast<Array*>(lpDataBase + muValue);
        }
        else
        {
            // Plain: the node's own value word is the (X360 32-bit) Array pointer image.
            lpArray = reinterpret_cast<Array*>(static_cast<uintptr_t>(muValue));
        }
        return lpArray->muNumElements;
    }

    // Attrib::Node::GetTypeDesc @ 0x828079E8
    // Resolve this node's schema TypeDesc from the process attribute database by its
    // mTypeIndex. The index is clamped to zero when it is out of range, then looked up in the
    // DatabasePrivate impl (indexed-type count @ +0x14, vector<TypeDesc*> @ +0x18) through the
    // committed bounds-checked operator[]. The X360 inlines the same 'Attribute database not
    // initialized.' assert + singleton read Database::Get() emits (mirrors Array::GetTypeDesc).
    const TypeDesc* Node::GetTypeDesc() const
    {
        u8* lpDatabase = reinterpret_cast<u8*>(&Database::Get());
        // sThis[1] == mPrivates (DatabasePrivate*) at Database+0x04.
        u8* lpPrivates = *reinterpret_cast<u8**>(lpDatabase + 4);

        const u32 luNumIndexedTypes = *reinterpret_cast<u32*>(lpPrivates + 0x14);
        u32 luIndex = mTypeIndex;
        if (luIndex >= luNumIndexedTypes)
            luIndex = 0;

        AttribTypeDescPtrVector* lpVector =
            reinterpret_cast<AttribTypeDescPtrVector*>(lpPrivates + 0x18);
        return (*lpVector)[luIndex];
    }

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
