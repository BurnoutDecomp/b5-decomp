#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribute.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribarray.h"        // Attrib::Array (element count / element data)
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribclassprivate.h" // Attrib::ClassPrivate (inherited static data area)

#include <cstdint> // uintptr_t
#include <cstring> // memset
#include <new>     // placement new (Collection_Get builds the cursor in the caller's buffer)

// Reconstructed from BURNOUT_X360_ARTIST.XEX (AttribSys v1.2.1.2).
//   Attrib::Attribute::Attribute   @ 0x82805AF0
//   Attrib::Attribute::IsInherited @ 0x82803600
//   Attrib::Attribute::GetLength   @ 0x82805B60   (landed 2026-07-31)
//   Attrib::Node::GetPointer       @ 0x828045B0   (landed 2026-07-31)
//   Attrib::Node::GetCount         @ 0x82804610   (landed 2026-07-31)
//   Attrib::Collection::Get        @ 0x82807E30   (landed 2026-07-31, as Collection_Get)
//
// Node::GetPointer and Node::GetCount are the two primitives the whole generated-accessor
// surface stands on -- every Num_<array>() and every indexed element read bottoms out in
// one of them. GetPointer was a __debugbreak() trap with a one-argument signature until
// 2026-07-31; it is neither. Both were recovered from the IDA database rather than the
// JSON export set (GetPointer @0x828045B0 has no export entry at all, GetCount does).

namespace CgsSceneManager
{
namespace CgsCollision
{
    // ========================================================================
    // The Attrib::Attribute cursor teardown -- Attrib::Attribute::~Attribute
    // ========================================================================
    // Every generated Num_<array>() accessor builds an Attribute on the stack and ends by
    // calling this; IDA resolves the call to
    // CgsSceneManager::CgsCollision::BaseCollisionGenerator::Destruct @0x8284CB38, which is
    // an IDENTICAL-CODE-FOLDING artifact -- that address is the one shared `blr` every
    // empty destructor in the image collapsed onto, and the collision generator merely won
    // the naming race. The real callee is ~Attribute, and it is empty: an Attribute is four
    // borrowed pointers ({instance, collection, node, data}) and owns none of them.
    //
    // Homed here, beside Attribute itself, because this is the only thing the symbol is.
    // Four generated headers (surfacelist / speechdata / languagestreamcollection /
    // shotgroup) declare it as this free-function seam; they had no definition to link
    // against until 2026-07-31, which went unnoticed only because every one of those
    // accessors was inline and uninstantiated.
    void BaseCollisionGenerator_Destruct(void* /*lpThis*/)
    {
    }
}
}

namespace Attrib
{
    // ========================================================================
    // Attrib::Node::GetPointer @ 0x828045B0
    // ========================================================================
    // Where this node's value actually lives, in the order the X360 tests:
    //   0x40 -> stored INLINE in the node, at `this + 8` (the muValue word onwards);
    //   0x10 -> laid out, at lpLayout + muValue;
    //   0x20 -> inherited, in the owning class's static data area + muValue;
    //   else -> muValue is itself the pointer (the X360 32-bit pointer image).
    void* Node::GetPointer(void* lpLayout, const Collection* lpCollection)
    {
        if ((muFlags & 0x40u) != 0)
            return &muValue;

        if ((muFlags & 0x10u) != 0)
            return static_cast<u8*>(lpLayout) + muValue;

        if ((muFlags & 0x20u) != 0)
        {
            // X360 `lwz r11,0x18(r5); lwz r11,8(r11); lwz r11,0x34(r11)` ==
            // collection->mpClass->mpPrivates->mStaticData -- by name here.
            const ClassPrivate* lpPrivate =
                reinterpret_cast<const ClassPrivate*>(lpCollection->mpClass)->mpPrivates;
            return static_cast<u8*>(lpPrivate->mStaticData) + muValue;
        }

        // Plain-pointer case: the payload slot IS the pointer. Console 4 bytes, host 8 --
        // read the FULL-WIDTH member (muValue would truncate a host pointer to its low half).
        return mpValue;
    }

    // ========================================================================
    // Attrib::Node::GetCount @ 0x82804610
    // ========================================================================
    // How many elements this node's value has. An unoccupied bucket (no 0x80 bit) has
    // none; a non-array node is a single value; an array node reports its Array header's
    // muNumElements, and the header is located exactly as GetPointer locates the value
    // (laid out / inherited / plain -- note there is NO 0x40 inline case here, an array
    // is never stored inside the node).
    unsigned int Node::GetCount(void* lpLayout, const Collection* lpCollection)
    {
        if ((muFlags & 0x80u) == 0)
            return 0;
        if ((muFlags & 0x2u) == 0)
            return 1;

        const Array* lpArray;
        if ((muFlags & 0x10u) != 0)
        {
            lpArray = reinterpret_cast<const Array*>(static_cast<u8*>(lpLayout) + muValue);
        }
        else if ((muFlags & 0x20u) != 0)
        {
            const ClassPrivate* lpPrivate =
                reinterpret_cast<const ClassPrivate*>(lpCollection->mpClass)->mpPrivates;
            lpArray = reinterpret_cast<const Array*>(
                static_cast<u8*>(lpPrivate->mStaticData) + muValue);
        }
        else
        {
            lpArray = reinterpret_cast<const Array*>(mpValue);   // full-width payload
        }
        return lpArray->muNumElements;
    }

    // Bind a cursor to (instance, collection, node). Cache the value pointer eagerly
    // unless the node carries the no-pointer flag (0x2). The cached pointer is resolved
    // from the instance's layout block (instance.mpAttributeData, +4) -- and, for an
    // inherited node, from the COLLECTION the value was found in, which is why the X360
    // leaves that collection in r5 across the call (`stw r5,4(r31)` does not clobber it).
    Attribute::Attribute(const Instance& lrInstance, const Collection* lpCollection, Node* lpNode)
        : mpInstance(&lrInstance), mpCollection(lpCollection), mpNode(lpNode), mpData(nullptr)
    {
        if (lpNode && (lpNode->muFlags & 0x2u) == 0)
            mpData = lpNode->GetPointer(lrInstance.GetLayoutPointer(), lpCollection);
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

    // ========================================================================
    // Attrib::Attribute::GetLength @ 0x82805B60
    // ========================================================================
    //   lwz r3,8(this)     ; mpNode -- a null node has no elements
    //   lwz r11,0(this)    ; mpInstance
    //   lwz r5,0(r11)      ; instance->mpCollection    -> GetCount's collection arg
    //   lwz r4,4(r11)      ; instance->mpAttributeData -> GetCount's layout arg
    //   b   Attrib__Node__GetCount
    // ⚠️ Note it asks the INSTANCE for the collection, not this cursor's own mpCollection
    // (the container the value was found in). For an inherited attribute those differ --
    // that is exactly what IsInherited above compares. Reproduce it as written.
    int Attribute::GetLength()
    {
        if (!mpNode)
            return 0;
        return static_cast<int>(
            mpNode->GetCount(mpInstance->GetLayoutPointer(),
                             mpInstance->GetResolvedCollection()));
    }

    // ========================================================================
    // Attrib::Collection::Get @ 0x82807E30   (as the Collection_Get seam)
    // ========================================================================
    // Resolve luKey through lpCollection's parent/layout chain and hand back a cursor bound
    // to (lpInstance, the container that actually held the key, the node). A miss yields a
    // zeroed cursor. The X360 returns the 16-byte Attribute by value through the hidden
    // sret pointer -- it builds it in a stack local and copies the four words out; building
    // it straight into the caller's buffer is the same thing without the console's
    // 4-byte-word copy loop, which does not survive the x64 widening anyway.
    //
    // Homed here rather than in attribcollection.cpp because Attribute is only complete in
    // attribute.h, which includes attribinstance.h -- see the seam's declaration there.
    void* Collection_Get(void* lpOut, Collection* lpCollection,
                         const Instance* lpInstance, u64 luKey)
    {
        const Collection* lpContainer = 0;
        Node* lpNode = lpCollection->GetNode(luKey, lpContainer);
        if (lpNode)
            new (lpOut) Attribute(*lpInstance, lpContainer, lpNode);
        else
            std::memset(lpOut, 0, sizeof(Attribute));
        return lpOut;
    }
}
