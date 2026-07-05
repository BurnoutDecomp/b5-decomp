// SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribarray.h
#pragma once

// AttribSys runtime -- Attrib::Array, the variable-length attribute-array header that
// prefixes an inline element block inside a live attribute-data layout.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (AttribSys v1.2.1.2). The Array header is a
// small (8-byte) descriptor immediately followed by its element data in the same
// allocation. Its four u16 fields were recovered store-for-store from the four ledger-
// attested bodies:
//   Destroy      @ 0x828093A0  (block-size + census free of the whole Array allocation)
//   GetData      @ 0x82804558  (element accessor; inline value vs dereferenced pointer)
//   GetTypeDesc  @ 0x828078B8  (schema TypeDesc lookup from the attribute database)
//   ~Array       @ 0x82807948  (per-element release via the schema type handler)
//
// Field roles (u16 each, byte offsets literal -- base is byte-addressed in the asm):
//   +0x0 muNumElementsHeader : element count used to size the allocation
//                              (muNumElementsHeader * elementSize inline data bytes).
//   +0x2 muNumElements       : live element count for bounds/iteration.
//   +0x4 muElementSize       : bytes per element; 0 marks a reference/pointer array
//                              (4-byte pointer slots) that needs type-handler teardown.
//   +0x6 muTypeInfo          : packed schema info -- low 15 bits (& 0x7FFF) are the
//                              indexed-type id; ((v >> 12) & 0xFFFF8) is the aligned
//                              byte offset from the header to the inline data region.
//
// The data region begins at this + ((muTypeInfo >> 12) & 0xFFFF8) + 8.
#include "types.hpp"

namespace Attrib
{
    // The per-element handler AttribSys reference arrays dispatch through. DWARF
    // (references/DecFIGS/dwarfdump/.../attribsys.h:3689) + Feb-2007 v1.0.8.5 source
    // (attribsys.h:256) attest the vtable. In v1.2.1.2 a virtual destructor precedes the
    // four handler virtuals; ~Array invokes the slot at byte offset 0x10, which with the
    // destructor at slot 0 is Release(void*), the reference-element releaser.
    class ITypeHandler
    {
    public:
        virtual ~ITypeHandler();                // vtbl +0x00
        virtual void* Retain(void* lpObj) = 0;  // vtbl +0x04
        virtual void* Clone(void* lpObj) = 0;   // vtbl +0x08
        virtual void  Clean(void* lpObj) = 0;   // vtbl +0x0C
        virtual void  Release(void* lpObj) = 0; // vtbl +0x10  (~Array's per-element call)
    };

    // AttribSys schema type descriptor. DWARF (attribsys.h:521) + Feb-2007 v1.0.8.5 source
    // (attribsys.h:758) name the fields mType/mName/mSize/mIndex/mHandler. ~Array (v1.2.1.2)
    // reads the handler at TypeDesc+0x14 (asm lwz r28,0x14) -- one dword past where the five
    // 4-byte fields sit in the older source, so a 4-byte reserved slot precedes mHandler in
    // this XEX's layout. The handler offset is the X360-attested value; only mHandler is
    // touched by this batch (the rest is recorded for future TypeDesc TUs).
    struct TypeDesc
    {
        u32           mType;        // +0x00
        const char*   mName;        // +0x04
        u32           mSize;        // +0x08
        u32           mIndex;       // +0x0C
        u32           mReserved10;  // +0x10 (present in the v1.2.1.2 XEX layout)
        ITypeHandler* mHandler;     // +0x14 : per-element release handler (may be null)
    };

    // The variable-length attribute-array header. 8-byte descriptor followed inline by
    // its element data in the same allocation.
    class Array
    {
    public:
        // Destroy @ 0x828093A0 -- element teardown + census free of the whole block.
        // Static (the X360 body takes the Array* by register and frees it wholesale).
        static int Destroy(Array* lpArray);

        // GetData @ 0x82804558 -- pointer to element luIndex's value (null if OOR).
        void* GetData(unsigned int luIndex);

        // GetTypeDesc @ 0x828078B8 -- resolve this Array's schema TypeDesc.
        const TypeDesc* GetTypeDesc() const;

        // ~Array @ 0x82807948 -- release reference-type elements via the type handler.
        ~Array();

        u16 muNumElementsHeader;   // +0x0
        u16 muNumElements;         // +0x2
        u16 muElementSize;         // +0x4
        u16 muTypeInfo;            // +0x6
    };
    static_assert(sizeof(Array) == 8, "Attrib::Array header must be 8 bytes");
}
