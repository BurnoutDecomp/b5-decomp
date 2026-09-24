#pragma once

// Attrib::Gen::worldemitterlist — generated AttribSys class (the world sound-emitter
// list schema; the array of RefSpecs each worldemitterlist instance owns). Reconstructed
// from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::worldemitterlist::mWorldEmitters @ 0x82686600  (called by
//     BrnSound::Logic::World::EmitterEffect::Attach)
//
// DWARF (references/DecFIGS/dwarfdump/.../classes/worldemitterlist.h) attests the
// generated signature `const Attrib::RefSpec & mWorldEmitters(unsigned int) const;`
// (worldemitterlist.h:82) and the layout struct
//   _LayoutStruct { Private _Array_mWorldEmitters; RefSpec[50] mWorldEmitters; Int32 mNumWorldEmitters; }
// so mpAttributeData points at the 8-byte Attrib::Private array header and the RefSpec[50]
// payload begins at +8. The X360 body reads Instance::mpAttributeData (@+4), asks the
// Private header for the live element count (Attrib::Private::GetLength), and either
// returns the element at mpAttributeData + 8 + 24*uiIndex (RefSpec stride = 24, attested by
// the asm's a2*24 index math) or, out of range, the shared 0x18-byte zeroed default block
// from Attrib::DefaultDataArea(0x18). class-sourced ctor pattern matches
// surfacelist/propscrashbinlist; the worldemitterlist ctor itself is a separate TU (owned
// by another wave) — only the assigned mWorldEmitters accessor is authored here.
//
// asm (0x82686600):
//   lwz  r30,4(r3)            ; v2 = mpAttributeData (Instance +4)
//   mr   r31,r4              ; uiIndex
//   mr   r3,r30 ; bl GetLength ; length = Attrib::Private::GetLength(v2)
//   cmplw cr6,r31,r3 ; bge -> out-of-range
//   slwi r11,r31,1 ; add r11,r31,r11 ; slwi r11,r11,3   ; r11 = uiIndex*24
//   add  r11,r11,r30 ; addi r3,r11,8                     ; -> v2 + 8 + 24*uiIndex
//   out-of-range: li r3,0x18 ; bl DefaultDataArea        ; -> DefaultDataArea(0x18)
//
// ⚠️ TWO COUNTS, TWO ACCESSORS (FX-EMITTER, 2026-09-24). The DWARF declares both
//   :163 `const Int32 & mNumWorldEmitters() const;` -- the SCALAR attribute mNumWorldEmitters
//   :178 `unsigned int Num_mWorldEmitters() const;` -- the fixed array's Attrib::Array length
// and they are different numbers. The ARTIST image's embedded AttribSys schema
// (worldemitterlist ClassLoadData, layout 1216 bytes) places mWorldEmitters (RefSpec,
// 24-byte element, capacity 50, fixed-array flags 7) at +0 and mNumWorldEmitters
// (EA::Reflection::Int32, scalar flags 6) at +1208 = 0x4B8, and the one worldemitterlist
// collection in SOUND/BURNOUTGLOBALDATA.BIN (23D8BE2C59CFEBB0) carries the array header
// {alloc 50, count 50} but mNumWorldEmitters = 38: slots 0..37 are the named emitters,
// 38..49 are empty RefSpecs. EmitterEffect::Attach @0x826F5740 gates on the SCALAR
// (`lwz r11,0x4B8(r11)` at 0x826F5804 and 0x826F5838); reading Num_mWorldEmitters there
// admitted twelve empty slots.
#include <cstddef>   // offsetof (layout pins)

#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "GameSource/AttribSys/Generated/attrib_private.h"   // Attrib::Private (canonical)

namespace Attrib
{
namespace Gen
{
    class worldemitterlist : public Instance
    {
    public:
        explicit worldemitterlist(const RefSpec& lrRefSpec, void* lpOwner = nullptr)
            : Instance(lrRefSpec, lpOwner) {}

        void ChangeWithDefault(const RefSpec& lrRefSpec)
        {
            RefSpec& lrMutable = const_cast<RefSpec&>(lrRefSpec);
            Change(const_cast<Collection*>(lrMutable.GetCollectionWithDefault()));
        }

        // DWARF worldemitterlist.h:82: `const Attrib::RefSpec & mWorldEmitters(unsigned int) const;`
        const RefSpec& mWorldEmitters(unsigned int uiIndex) const;
        // DWARF worldemitterlist.h:178 -- the Attrib::Array header's element count (50 in the
        // shipped data), NOT the number of live emitters; see mNumWorldEmitters.
        unsigned int Num_mWorldEmitters() const
        {
            return static_cast<const Private*>(GetLayoutPointer())->GetLength();
        }
        // DWARF worldemitterlist.h:163 -- the scalar attribute at layout +0x4B8 (38 in the
        // shipped data): the count EmitterEffect::Attach asserts and gates the entity type on.
        const s32& mNumWorldEmitters() const
        {
            return static_cast<const _LayoutStruct*>(GetLayoutPointer())->mNumWorldEmitters;
        }

    private:
        // The generated layout block mpAttributeData points at (DWARF worldemitterlist.h:91-94).
        // RefSpec is 24 bytes on the host as on the console ({u64 class key, u64 collection
        // key, collection pointer}), so the console offsets hold unchanged -- pinned below.
        struct _LayoutStruct
        {
            Private _Array_mWorldEmitters;   // +0x000 (:92) the Attrib::Array header
            RefSpec mWorldEmitters[50];      // +0x008 (:93)
            s32     mNumWorldEmitters;       // +0x4B8 (:94)
        };
        static_assert(offsetof(_LayoutStruct, mWorldEmitters) == 0x8,
                      "worldemitterlist::mWorldEmitters @+8 (ARTIST schema, mWorldEmitters(i) @0x82686600)");
        static_assert(offsetof(_LayoutStruct, mNumWorldEmitters) == 0x4B8,
                      "worldemitterlist::mNumWorldEmitters @+0x4B8 (ARTIST schema; Attach @0x826F5804)");
        static_assert(sizeof(_LayoutStruct) == 1216,
                      "worldemitterlist layout 1216 bytes (ARTIST schema ClassLoadData layout size)");
    };

    // Bounds-checked flat-array accessor into the RefSpec[50] payload. Reads the Private
    // header count (mpAttributeData points at it); returns the in-range element at
    // mpAttributeData + 8 + 24*uiIndex, else the shared 24-byte zeroed default block.
    inline const RefSpec& worldemitterlist::mWorldEmitters(unsigned int uiIndex) const
    {
        const _LayoutStruct* lpLayout = static_cast<const _LayoutStruct*>(GetLayoutPointer());   // Instance::mpAttributeData (+4)

        if (uiIndex >= lpLayout->_Array_mWorldEmitters.GetLength())
            return *static_cast<const RefSpec*>(DefaultDataArea(0x18u));

        return lpLayout->mWorldEmitters[uiIndex];
    }
}
}
