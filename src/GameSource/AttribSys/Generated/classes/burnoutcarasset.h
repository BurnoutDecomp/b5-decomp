#pragma once

// Attrib::Gen::burnoutcarasset -- generated AttribSys class (per-car "burnout car
// asset" attribute schema: the top-level attribute block referencing a car's model,
// physics, sound, and gameplay sub-attributes). Reconstructed from
// BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::burnoutcarasset::burnoutcarasset @ 0x822048F0
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) -- same generated-ctor
// pattern as the sibling generated classes debrisparams / surfacelist / iceanim. The
// X360 build inlines the generated accessor / `using Instance::...` API away, so the
// constructor is the only burnoutcarasset function in the ledger (minimal X360-faithful
// recon). Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"   // Attrib::FindCollection

namespace Attrib
{
namespace Gen
{
    class burnoutcarasset : private Instance
    {
    public:
        // The class key low word, kept for the AssertOnClassCheck site below.
        static const int KI_BURNOUTCARASSET_CLASS_LOW = -206702987;   // 0xF3ADF675

        // ⭐ THE FULL 64-BIT CLASS KEY Attrib::FindCollection resolves against. The X360
        // key-ctor sub_82204998 builds it as `lis r11,-0xC53 / ori r3,r11,0xF675 /
        // lis r11,0x52B8 / ori r11,r11,0x1656 / insrdi r3,r11,32,0` == 0x52B81656F3ADF675,
        // and MainDirector::ProcessNewVehicleEvents @0x8221A7E8 inlines the SAME five
        // instructions. Independently confirmed by name:
        //     hash64("burnoutcarasset") == 0x52B81656F3ADF675
        // (attribhash64, seed 0xABCDEF0011223344), whose low word is exactly the committed
        // KI_BURNOUTCARASSET_CLASS above. The class registry is keyed by the whole
        // doubleword, so the low word alone MISSES.
        static const u64 KU_BURNOUTCARASSET_CLASS_KEY = 0x52B81656F3ADF675ULL;

        // ---- the data-area offsets the director's camera seeding reads --------------------
        // All three are inside the 0x228-byte default data area below and are attested twice,
        // by BrnDirectorVehicleInputInterface::NewVehicle @0x822CBA90 and by
        // MainDirector::ProcessNewVehicleEvents @0x8221A6B0 / UpdateAttribSys @0x8221AFD0,
        // which read them off `Instance::mpAttributeData` (`lwz r11,4(instance)`).
        // FLAG: the ACCESSOR NAMES are ours -- the generated accessor API is inlined away in
        // the X360 build, so only the offsets and their roles are attested.
        static const u32 KU_BUMPER_CAM_REFSPEC_OFFSET   = 0x1B8;  // 440 -> camerabumperbehaviour
        static const u32 KU_EXTERNAL_CAM_REFSPEC_OFFSET = 0x1A0;  // 416 -> cameraexternalbehaviour
        static const u32 KU_ASSET_NAME_OFFSET           = 0x1E8;  // 488 -> const char* (assert text)
        // ⭐ ADDED 2026-08-09 (attribs-setup wave): the handling-asset RefSpec. Attested by
        // SimpleVehiclePhysics::SetAttributes @0x8262064C / VehiclePhysics::SetAttributes
        // @0x8262E04C -- both do `addi r3, dataArea, 0x158 ; bl RefSpec::GetCollection` and
        // feed the result to the physicsvehiclehandling ctor.
        static const u32 KU_PHYSICS_VEHICLE_HANDLING_REFSPEC_OFFSET = 0x158;  // 344 -> physicsvehiclehandling

        // ⭐ THE KEY CTOR -- X360 sub_82204998 (out-of-line here, inlined into
        // ProcessNewVehicleEvents). Resolve this car's burnoutcarasset collection by its
        // COLLECTION key, chain Instance, and fall back to the 0x228-byte default data area
        // when the collection carries none. Same shape as the committed sibling
        // shotgroup(u64, void*).
        // ⚠️ NOT DEFAULTED, for the same reason shotgroup's is not: a defaulted key ctor
        // would make no-argument construction resolve a collection, and generated-class
        // members reached from file-scope statics construct PRE-MAIN, before
        // Attrib::Database exists.
        explicit burnoutcarasset(u64 luCarCollectionKey, void* lpOwner);

        explicit burnoutcarasset(Collection* lpCollection = nullptr, void* lpOwner = nullptr);

        // The base's validity test + layout pointer, re-exported (the generated classes derive
        // Instance PRIVATELY). Both are what the console reads off the stack instance:
        // `lwz r11, 0(inst)` (the collection -- IsValid) and `lwz r11, 4(inst)` (the attribute
        // data area). Same using-declaration precedent as cameradefaults.h / shotgroup.h.
        using Instance::IsValid;
        using Instance::GetLayoutPointer;

        // The two camera RefSpecs and the asset name, reached BY NAME off the data area
        // rather than by a raw offset at each call site.
        RefSpec* GetBumperCamRefSpec() const   { return RefSpecAt(KU_BUMPER_CAM_REFSPEC_OFFSET); }
        RefSpec* GetExternalCamRefSpec() const { return RefSpecAt(KU_EXTERNAL_CAM_REFSPEC_OFFSET); }
        RefSpec* GetPhysicsVehicleHandlingRefSpec() const
        { return RefSpecAt(KU_PHYSICS_VEHICLE_HANDLING_REFSPEC_OFFSET); }
        const char* GetAssetName() const
        {
            const u8* lpData = static_cast<const u8*>(GetLayoutPointer());
            if (lpData == 0) return 0;
            return *reinterpret_cast<const char* const*>(lpData + KU_ASSET_NAME_OFFSET);
        }

    private:
        RefSpec* RefSpecAt(u32 luOffset) const
        {
            u8* lpData = static_cast<u8*>(GetLayoutPointer());
            if (lpData == 0) return 0;
            return reinterpret_cast<RefSpec*>(lpData + luOffset);
        }
    };

    // X360 sub_82204998: FindCollection(class key, collection key) -> Instance -> default
    // data area when the resolved collection has no layout of its own.
    inline burnoutcarasset::burnoutcarasset(u64 luCarCollectionKey, void* lpOwner)
        : Instance(FindCollection(KU_BURNOUTCARASSET_CLASS_KEY, luCarCollectionKey), lpOwner)
    {
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x228u);
    }

    // Chain the Instance ctor, assert the collection's class is ClassName::burnoutcarasset
    // (skipping the assert when the class is unset/0), then give the instance a default
    // data area (0x228 bytes) if it has none.
    inline burnoutcarasset::burnoutcarasset(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        if (GetClass() != KI_BURNOUTCARASSET_CLASS_LOW && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_BURNOUTCARASSET_CLASS_LOW, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x228u);
    }
}
}
