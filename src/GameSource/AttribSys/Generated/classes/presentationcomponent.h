#pragma once

// Attrib::Gen::presentationcomponent -- generated AttribSys class (presentation
// component attributes). The generated accessor / `using` API is inlined away at
// the call sites, so the constructor is the only presentationcomponent function in
// the build (a minimal generated-ctor recon, same shape as the sibling generated
// classes surfacelist / debrisparams / iceanim). Derives from Attrib::Instance.
// Callers: BrnSound::Logic::HUDEffect::HUDEffect (the member) and
//          BrnSound::Logic::HUDEffect::FindEventMapping @0x82686928 (the accessors
//          below, which the X360 inlined into that function).
//
// LAYOUT (DWARF GameSource/AttribSys/Generated/classes/presentationcomponent.h:123
// _LayoutStruct, every offset re-confirmed against the inlined reads in
// FindEventMapping @0x82686928):
//     +0x0000  Private _Array_MessageIds
//     +0x0008  Int64  MessageIds[256]          (X360 `*(v12 + v23 + 8)`, stride 8)
//     +0x0808  Int32  NumMappings              (X360 `*(v23 + 2056)`)
//     +0x080C  Private _Array_SpliceIndices    (X360 GetLength(v25 + 2060))
//     +0x0814  UInt16 SpliceIndices[256]       (X360 `v22 + v25 - 520`, v22 = 2588 + 2i)
//     +0x0A14  Private _Array_LastSpliceIndices(X360 GetLength(v27 + 2580))
//     +0x0A1C  UInt16 LastSpliceIndices[256]   (X360 `v22 + v27`)
//     +0x0C1C  Private _Array_MixerOutputs     (X360 GetLength(v32 + 3100))
//     +0x0C24  UInt8  MixerOutputs[256]        (X360 `v32 + v21 + 3108`)
//     +0x0D24  Private _Array_ChokeGroups      (X360 GetLength(v34 + 3364))
//     +0x0D2C  UInt8  ChokeGroups[256]         (X360 `v34 + v21 + 3372`)
//   total 0xE30 -- the size the ctor already asks DefaultDataArea for.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "GameSource/AttribSys/Generated/attrib_private.h"

namespace Attrib
{
namespace Gen
{
    class presentationcomponent : private Instance
    {
    public:
        // Attrib::ClassName::presentationcomponent (DWARF byte form
        // [0,0,0,0, 208,40,169,116] == 0xD028A974; the ctor's -802641548 is the
        // same 32-bit value sign-extended).
        static const u64 KU_CLASS_KEY = 0xD028A974ull;

        explicit presentationcomponent(Collection* lpCollection = nullptr, void* lpOwner = nullptr);

        // The X360 HUDEffect::Attach @0x8269C3C8 calls
        // Attrib::Instance::ChangeWithDefault(&mHudMessageData, <globaldata>+0x500)
        // -- the burnoutglobaldata `mHudMessages` RefSpec. Exposed here (the base is
        // a PRIVATE base, so the generated class must re-publish it).
        explicit presentationcomponent(const RefSpec& lrRefSpec, void* lpOwner)
            : Instance(lrRefSpec, lpOwner) {}

        void ChangeWithDefault(const RefSpec& lrRefSpec)
        {
            RefSpec& lrMutable = const_cast<RefSpec&>(lrRefSpec);
            Change(const_cast<Collection*>(lrMutable.GetCollectionWithDefault()));
        }

        bool HasData() const { return mpAttributeData != 0; }

        // Number of message->splice mappings this component carries. X360:
        // `*(mpAttributeData + 2056)`, asserted < 256 at BrnHUDEffect.cpp:424.
        u32 NumMappings() const
        {
            return *reinterpret_cast<const u32*>(
                static_cast<const u8*>(mpAttributeData) + 0x808u);
        }

        // The HUD-message CgsID this mapping row matches. X360 reads only the LOW
        // word (`*(v24 + 4)`) of the 8-byte element, which is what the GuiAudioEvent's
        // compressed id compares against.
        u64 MessageIds(u32 luIndex) const
        {
            const u8* lpData = static_cast<const u8*>(mpAttributeData);
            if (luIndex >= reinterpret_cast<const Private*>(lpData)->GetLength())
                return *static_cast<const u64*>(DefaultDataArea(8u));
            return *reinterpret_cast<const u64*>(lpData + 0x8u + luIndex * 8u);
        }

        // First splice index of the row's round-robin range.
        u16 SpliceIndices(u32 luIndex) const
        {
            const u8* lpData = static_cast<const u8*>(mpAttributeData);
            if (luIndex >= reinterpret_cast<const Private*>(lpData + 0x80Cu)->GetLength())
                return *static_cast<const u16*>(DefaultDataArea(2u));
            return *reinterpret_cast<const u16*>(lpData + 0x814u + luIndex * 2u);
        }

        // Last splice index of the row's round-robin range (inclusive).
        u16 LastSpliceIndices(u32 luIndex) const
        {
            const u8* lpData = static_cast<const u8*>(mpAttributeData);
            if (luIndex >= reinterpret_cast<const Private*>(lpData + 0xA14u)->GetLength())
                return *static_cast<const u16*>(DefaultDataArea(2u));
            return *reinterpret_cast<const u16*>(lpData + 0xA1Cu + luIndex * 2u);
        }

        // Dynamic-mixer output slot the row's voice plays on.
        u8 MixerOutputs(u32 luIndex) const
        {
            const u8* lpData = static_cast<const u8*>(mpAttributeData);
            if (luIndex >= reinterpret_cast<const Private*>(lpData + 0xC1Cu)->GetLength())
                return *static_cast<const u8*>(DefaultDataArea(1u));
            return *(lpData + 0xC24u + luIndex);
        }

        // Choke group: a non-zero group steals any voice already playing it.
        u8 ChokeGroups(u32 luIndex) const
        {
            const u8* lpData = static_cast<const u8*>(mpAttributeData);
            if (luIndex >= reinterpret_cast<const Private*>(lpData + 0xD24u)->GetLength())
                return *static_cast<const u8*>(DefaultDataArea(1u));
            return *(lpData + 0xD2Cu + luIndex);
        }
    };

    // Chain the Instance ctor, assert the collection's class is
    // ClassName::presentationcomponent (skipping the assert when the class is
    // unset/0), then give the instance a default data area (0xE30 bytes) if it
    // has none.
    inline presentationcomponent::presentationcomponent(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_PRESENTATIONCOMPONENT_CLASS = -802641548; // Attrib::ClassName::presentationcomponent
        if (GetClass() != KI_PRESENTATIONCOMPONENT_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_PRESENTATIONCOMPONENT_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0xE30u);
    }
}
}
