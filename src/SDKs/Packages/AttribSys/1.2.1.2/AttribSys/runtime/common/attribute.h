#pragma once

// AttribSys runtime — Attrib::Attribute, a lightweight cursor onto one attribute of a
// live Attrib::Instance. It records the owning instance, the collection the value was
// resolved from, the schema Node that describes it, and a cached pointer to the value
// data.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (AttribSys v1.2.1.2). Layout recovered from
// the ctor @ 0x82805AF0 (4 words: instance/collection/node/data) and IsInherited
// @ 0x82803600. The rich generated-class accessor API is inlined away on
// X360, so only the ledger-attested out-of-line bodies are declared here; the rest of the
// accessor surface (GetKey/GetType/GetSize/...) lives in its own slices. Reuses the
// committed Attrib::Instance / Attrib::Collection layouts (attribinstance.h).
#include "types.hpp"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
    // Schema node describing a single attribute (key, type, layout slot). Only the bits
    // the recovered bodies touch are modelled: a flags byte at +0xF whose 0x2 bit marks
    // a node with no instance-resolved pointer and whose 0x10 bit marks a forced-local
    // (non-inheritable) attribute. GetPointer resolves the node's value pointer within an
    // instance's attribute-data block (its body lives in the Node slice / SDK).
    struct Node
    {
        u8  mPad0[8];      // +0x00  attribute key (8 bytes)
        u32 muValue;       // +0x08  value word: a byte offset into a data area (laid-out /
                           //         inherited nodes) or the 32-bit Array pointer image
                           //         (plain array node). X360 width -- the byte-exact node
                           //         keeps muFlags at +0xF (see attribute_embed_check.cpp).
        u16 mTypeIndex;    // +0x0C  attribute type index
        u8  mMax;          // +0x0E  cached probe-run length (bucket role; unused by cursors)
        u8  muFlags;       // +0x0F : bit1(0x2)=no-pointer, bit4(0x10)=laid-out/local,
                           //         bit5(0x20)=inherited

        // GetPointer @ external — resolve this node's value pointer inside lpLayout
        // (the instance's mpAttributeData block).
        void* GetPointer(void* lpLayout);

        // GetPointer(layout, collection) @ external — the collection-aware resolve the
        // out-of-line Attribute::GetInternalPointer path calls (DWARF attribhashmap.h:289).
        void* GetPointer(void* lpLayout, const Collection* lpCollection);

        // GetCount(layout, collection) @ external — element count of an array-typed node
        // (DWARF attribhashmap.h:326); drives Attribute::GetLength.
        unsigned int GetCount(void* lpLayout, const Collection* lpCollection);
    };

    // Cursor onto one attribute of a live instance. 16 bytes: instance, collection, node,
    // cached value pointer.
    class Attribute
    {
    public:
        // Attribute @ 0x82805AF0 — bind to (instance, collection, node); cache the value
        // pointer unless the node is flagged no-pointer (0x2).
        Attribute(const Instance& lrInstance, const Collection* lpCollection, Node* lpNode);

        // IsInherited @ 0x82803600 — true when the value comes from the instance's
        // resolved collection rather than the attribute's recorded one (i.e. inherited
        // from a parent/default), accounting for the modified flag.
        bool IsInherited();

        // GetLength — element count of the array-typed value this cursor was resolved to.
        // Used by the generated Num_<array>() accessors (surfacelist::Num_Surfaces,
        // speechdata::Num_*, languagestreamcollection::Num_Items) which resolve an array
        // attribute into a stack cursor and read its length.
        int GetLength();

        // GetInternalPointer @ 0x82805B88 — resolve the raw pointer to element luIndex of
        // this cursor's value. A non-array node yields its instance-resolved pointer for
        // index 0 (null otherwise); an array node locates its Array header (from the
        // instance layout base for a laid-out node, the inherited class data area for an
        // inherited node, or the node's own pointer word) and indexes it.
        void* GetInternalPointer(u32 luIndex);

    private:
        const Instance*   mpInstance;    // +0
        const Collection* mpCollection;  // +4
        Node*             mpNode;        // +8
        void*             mpData;        // +0xC
    };
}
