#pragma once

// Attrib::Gen::burnoutcargraphicsasset -- generated AttribSys class (car graphics
// asset attributes). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::burnoutcargraphicsasset::burnoutcargraphicsasset @ 0x822BA0D8
//
// The X360 build inlines the generated accessor API away, so the ctor is the only ledger
// function; the two RandomTrafficColours accessors below are modelled as the inlined reads
// VehicleTypeRuntime::Prepare @0x82761B10 actually emits (the surfacelist /
// languagestreamcollection convention). Derives from Attrib::Instance. Called by
// BrnWorld::ActiveRaceCar::OnResourcesLoaded and BrnTraffic::VehicleTypeRuntime::Prepare.
#include "types.hpp"                                                          // s32 / u32 / u64
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribute.h"

namespace CgsSceneManager
{
namespace CgsCollision
{
    // Tears down the stack-resident Attrib::Attribute cursor Num_RandomTrafficColours
    // builds. IDA matched the generic 16-byte-object teardown stub to
    // BaseCollisionGenerator::Destruct -- a symbol collision, not collision code.
    // Declaration-only; mirrors committed surfacelist.h / languagestreamcollection.h.
    void BaseCollisionGenerator_Destruct(void* lpThis);
}
}

namespace Attrib
{
namespace Gen
{
    class burnoutcargraphicsasset : private Instance
    {
    public:
        // The FULL 64-bit "RandomTrafficColours" attribute key. Attested three ways:
        // VehicleTypeRuntime::Prepare @0x82761FC4 stages it as
        // `lis r11,-0x4E12 ; ori r5,r11,0x8062 ; lis r11,-0x542E ; ori r11,r11,0x3719 ;
        // insrdi r5,r11,32,0`; DWARF Attrib::Hash::burnoutcargraphicsasset::RandomTrafficColours
        // (burnoutcargraphicsasset.h:131) carries its low word 0xB1EE8062; and
        // hash64("RandomTrafficColours") with the AttribSys seed is exactly this doubleword.
        // The attribute table hashes the WHOLE doubleword, so the low word alone MISSES.
        static const u64 KU_RANDOM_TRAFFIC_COLOURS_ATTRIBUTE_KEY = 0xABD23719B1EE8062ull;

        explicit burnoutcargraphicsasset(Collection* lpCollection = nullptr, void* lpOwner = nullptr);

        // DWARF burnoutcargraphicsasset.h:98 `unsigned int Num_RandomTrafficColours() const`.
        // Inlined at VehicleTypeRuntime::Prepare @0x82761FE0 as the standard generated
        // Num_<array>() shape: Instance::Get into a stack Attribute cursor, Attribute::GetLength,
        // tear the cursor down.
        unsigned int Num_RandomTrafficColours() const;

        // DWARF burnoutcargraphicsasset.h:97 `const <Int32>& RandomTrafficColours(unsigned int) const`
        // (the generated _LayoutStruct::Int32 wrapper, an s32 here). Inlined at Prepare
        // @0x8276203C: GetAttributePointer(key, index), and when that misses the generated
        // accessor falls back to Attrib::DefaultDataArea(4) -- `li r3,4 ; bl DefaultDataArea`
        // @0x82762048 -- so the reference is always readable.
        const s32& RandomTrafficColours(u32 luIndex) const;
    };

    // Chain the Instance ctor, assert the collection's class is
    // ClassName::burnoutcargraphicsasset (skipping the assert when the class is
    // unset/0), then give the instance a default data area (8 bytes) if it has none.
    inline burnoutcargraphicsasset::burnoutcargraphicsasset(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_BURNOUTCARGRAPHICSASSET_CLASS = 1712282196; // Attrib::ClassName::burnoutcargraphicsasset (0x660F5A54)
        if (GetClass() != KI_BURNOUTCARGRAPHICSASSET_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_BURNOUTCARGRAPHICSASSET_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(8u);
    }

    // Inlined at VehicleTypeRuntime::Prepare @0x82761FE0. Get is non-const and this accessor is
    // const per DWARF, so the instance is const_cast -- the languagestreamcollection precedent.
    inline unsigned int burnoutcargraphicsasset::Num_RandomTrafficColours() const
    {
        AttributeValue lScratch;   // stack-resident Attrib::Attribute cursor (attribinstance.h)
        burnoutcargraphicsasset* lpSelf = const_cast<burnoutcargraphicsasset*>(this);
        Attribute* lpCursor = reinterpret_cast<Attribute*>(
            lpSelf->Get(&lScratch, reinterpret_cast<int*>(lpSelf),
                        KU_RANDOM_TRAFFIC_COLOURS_ATTRIBUTE_KEY));
        unsigned int luLength = static_cast<unsigned int>(lpCursor->GetLength());
        CgsSceneManager::CgsCollision::BaseCollisionGenerator_Destruct(&lScratch);
        return luLength;
    }

    // Inlined at VehicleTypeRuntime::Prepare @0x8276203C.
    inline const s32& burnoutcargraphicsasset::RandomTrafficColours(u32 luIndex) const
    {
        void* lpElement = GetAttributePointer(KU_RANDOM_TRAFFIC_COLOURS_ATTRIBUTE_KEY, luIndex);
        if (lpElement == 0)
            lpElement = DefaultDataArea(4u);
        return *static_cast<const s32*>(lpElement);
    }
}
}
