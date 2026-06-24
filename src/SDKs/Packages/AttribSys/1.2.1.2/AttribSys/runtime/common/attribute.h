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
        u8  mPad0[15];   // +0
        u8  muFlags;     // +0xF : bit1(0x2)=no-pointer, bit4(0x10)=local/non-inherited

        // GetPointer @ external — resolve this node's value pointer inside lpLayout
        // (the instance's mpAttributeData block).
        void* GetPointer(void* lpLayout);
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

    private:
        const Instance*   mpInstance;    // +0
        const Collection* mpCollection;  // +4
        Node*             mpNode;        // +8
        void*             mpData;        // +0xC
    };
}
