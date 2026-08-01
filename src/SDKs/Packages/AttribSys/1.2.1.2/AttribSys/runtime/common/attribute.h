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
    // ⚠️⚠️ THIS STRUCT AND Attrib::HashMap::Node ARE THE SAME CONSOLE TYPE, and they must
    // stay byte-identical on the host. Attrib::Collection::GetNode hands a
    // HashMap::Bucket* straight back as an Attrib::Node* (`reinterpret_cast`), which is
    // free on the console (both were 16 bytes) and was WRONG here from 2026-07-27 until
    // 2026-08-01: HashMap::Node's payload slot is a machine word, so on x64 it is 8 bytes
    // and pushes mTypeIndex/mMax/muFlags from +0x0C/+0x0E/+0x0F to +0x10/+0x12/+0x13 --
    // while this struct still declared a u32 payload and read muFlags at +0x0F, i.e. the
    // TOP BYTE OF THE PAYLOAD POINTER. Measured live: HashMap flags byte 0x80 (occupied),
    // this struct's muFlags 0. Every generated Num_<array>() therefore reported ZERO
    // elements -- Attribute::GetLength -> Node::GetCount bails on `(muFlags & 0x80) == 0`.
    // That was invisible until DirectorResourceManager::Prepare landed and asked 37 shot
    // groups for their ShotList length. attribute_embed_check.cpp now cross-asserts the two
    // layouts against each other; do NOT re-pin either to a console byte literal.
    struct Node
    {
        u64 mKey;          // +0x00  attribute key (8 bytes)
        // The payload slot. Console: 4 bytes. Host: a MACHINE WORD, because the collection
        // loader and the class/collection registries store real host pointers here
        // (attribdatabase.cpp `lpNode->mpValue = *lppValue`, attribhashmap.cpp's free-bucket
        // self-pointer sentinel). The u32 view is the byte offset a laid-out / inherited
        // node carries; the pointer view is the plain-pointer case.
        union
        {
            void* mpValue; // +0x08  plain pointer payload / inline value word
            u32   muValue; // +0x08  laid-out / inherited data-area byte offset (low word)
        };
        u16 mTypeIndex;    // console +0x0C / host +0x10  attribute type index
        u8  mMax;          // console +0x0E / host +0x12  cached probe-run length (bucket role)
        u8  muFlags;       // console +0x0F / host +0x13
                           //       : bit1(0x2)=array, bit4(0x10)=laid-out/local,
                           //         bit5(0x20)=inherited, bit6(0x40)=value stored INLINE
                           //         in the node (GetPointer @0x828045B0 tests 0x40 first
                           //         and returns `this + 8`), bit7(0x80)=occupied

        // Node @ 0x82809430 -- placement-construct a node/bucket for (key, type, value).
        // The one DEFINITION lives on the HashMap::Node spelling of this same type
        // (attribhashmap.cpp), which takes the 64-bit TYPE KEY and resolves the index
        // itself. Nothing ever defined this (u16 index) spelling, so it is retired rather
        // than left as a second declaration of a function that does not exist.

        // Default construct -- leaves the node uninitialised (the embed check builds one on
        // the stack).
        Node() {}

        // GetPointer(layout, collection) @ 0x828045B0 — resolve this node's value pointer.
        // Four cases, in the order the X360 tests them: the value stored INLINE in the node
        // (0x40 -> `this + 8`); a laid-out node (0x10 -> lpLayout + muValue); an inherited
        // node (0x20 -> the owning class's static data area + muValue); otherwise muValue is
        // itself the pointer. RECOVERED 2026-07-31 from the IDA database -- the function is
        // absent from the JSON export set (it sits between Array::GetData and Node::GetCount,
        // both of which ARE exported).
        //
        // ⚠️ There used to be a one-argument `GetPointer(void*)` declared beside this, with a
        // __debugbreak() body in attribute.cpp. It was a FORK, not an overload: both of the
        // X360 call sites (Collection::GetData @0x828050A8, Attribute::Attribute @0x82805B38)
        // stage a collection in r5, and the 0x20 branch dereferences it. Retired.
        void* GetPointer(void* lpLayout, const Collection* lpCollection);

        // GetCount(layout, collection) @ 0x82804610 — element count of this node's value.
        // Drives Attribute::GetLength. Unoccupied node (no 0x80 bit) -> 0; non-array -> 1;
        // otherwise the Array header's muNumElements, located exactly as GetPointer locates
        // the value. (DWARF attribhashmap.h:326.)
        unsigned int GetCount(void* lpLayout, const Collection* lpCollection);

        // GetTypeDesc @ own AttribSys TU (todo) — resolve this node's schema TypeDesc from
        // the attribute database by its mTypeIndex. Declared here (TypeDesc from
        // attribarray.h, forward-declared via attribinstance.h) so Collection::Clear's
        // inherited-attribute teardown links against the real out-of-line call.
        const TypeDesc* GetTypeDesc() const;
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

    // The generated Num_<array>() accessors declare their stack cursor as an opaque
    // Attrib::AttributeValue (attribinstance.h, which cannot name Attribute) and let
    // Instance::Get / Collection_Get build an Attribute into it. Keep the two in step.
    static_assert(sizeof(AttributeValue) == sizeof(Attribute),
                  "Attrib::AttributeValue must be a byte-exact stack home for Attrib::Attribute");
    static_assert(alignof(AttributeValue) >= alignof(Attribute),
                  "Attrib::AttributeValue must be at least as aligned as Attrib::Attribute");
}
