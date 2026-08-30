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
        unsigned int Num_mWorldEmitters() const
        {
            return static_cast<const Private*>(GetLayoutPointer())->GetLength();
        }
    };

    // Bounds-checked flat-array accessor into the RefSpec[50] payload. Reads the Private
    // header count (mpAttributeData points at it); returns the in-range element at
    // mpAttributeData + 8 + 24*uiIndex, else the shared 24-byte zeroed default block.
    inline const RefSpec& worldemitterlist::mWorldEmitters(unsigned int uiIndex) const
    {
        const u8* lpLayout = static_cast<const u8*>(GetLayoutPointer());     // Instance::mpAttributeData (+4)
        const Private* lpArrayHeader = reinterpret_cast<const Private*>(lpLayout);

        if (uiIndex >= lpArrayHeader->GetLength())
            return *static_cast<const RefSpec*>(DefaultDataArea(0x18u));

        return *reinterpret_cast<const RefSpec*>(lpLayout + 8 + 24 * uiIndex);
    }
}
}
