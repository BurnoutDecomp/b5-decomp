#pragma once

// Attrib::Gen::physicsvehiclehandling -- generated AttribSys class (the per-vehicle
// handling HUB: eight Attrib::RefSpec members, one per physics sub-attribute class).
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::physicsvehiclehandling::physicsvehiclehandling @ 0x825BDAE0
//       (Collection*, owner) -- named in the IDA database; MISSING from the JSON export
//       set (a hole: Wheel::SetPosition @0x825BDA10 ends at 0x825BDAD8, the next export
//       is 0x825BDB88, and this 0xA8-byte body sits in between). 10 named call sites.
//   Attrib::Gen::physicsvehiclehandling::physicsvehiclehandling @ 0x825BDB88
//       (const physicsvehiclehandling&) -- exported as `sub_825BDB88`. Every one of its
//       five call sites is `copy(&temp, &justBuiltHandling)` feeding a BY-VALUE argument;
//       the DecFIGS DWARF spells the parameter as `void SetupAttribs(physicsvehiclehandling)`
//       (VehicleAttribs.h:952/:1885), which is exactly why the copy ctor is out of line.
//
// Same generated-ctor pattern as the eight sibling classes physicsvehiclebaseattribs /
// physicsvehiclebodyrollattribs / physicsvehicleboostattribs / physicsvehiclecollisionattribs /
// physicsvehicledriftattribs / physicsvehicleengineattribs / physicsvehiclesteeringattribs /
// physicsvehiclesuspensionattribs. Derives from Attrib::Instance.
//
// The X360 build inlines the generated accessor API away, so the two constructors are the
// only physicsvehiclehandling functions in the ledger; the eight member accessors below are
// modelled as the inlined layout-block reads the call sites actually emit (the iceanim
// SuitableFor()/ShotProperties() convention), with the DWARF's own accessor names.
//
// EVERY constant in this file is data-proven, not guessed:
//   * the class key is hash64("physicsvehiclehandling") == 0x966121397B502EED, which is the
//     doubleword both ctors materialise (lis 0x7B50 / ori 0x2EED / lis 0x9661 / ori 0x2139 /
//     insrdi / cmpld) and the export key of the class record in build/game/schema.vlt.
//   * every attribute key is hash64(<the member name spelled below>), and each one equals
//     the corresponding Attrib::Hash::physicsvehiclehandling::* constant in the DecFIGS DWARF
//     AND the mKey of that member's Attrib::Definition in build/game/schema.vlt.
//   * every layout offset is the mOffset of that Definition, and is the literal `addi rN, r11, off`
//     the console emits at VehicleAttribs::SetupAttribs @0x825F4CD8 before RefSpec::GetCollection.
//   * 0xC0 is both the ctor's DefaultDataArea argument and the class record's mLayoutSize,
//     and equals 8 * sizeof(Attrib::RefSpec).
#include "types.hpp"                                                          // u8 / u32 / u64
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class physicsvehiclehandling : private Instance
    {
    public:
        // The class id, LOW word (0x7B502EED == 2068852461). This is the operand the
        // AssertOnClassCheck sites quote: Attrib::Instance::GetClass() is reconstructed as a
        // 32-bit int (attribinstance.h), so the check narrows to the low word -- the same
        // modelling choice the physicsvehiclecollisionattribs sibling documents.
        // (The console truth is wider: GetClass @0x82802F18 ends `ld r3,0(r11)` and both
        // ctors compare with `cmpld` against the whole doubleword below.)
        static const int KI_PHYSICSVEHICLEHANDLING_CLASS = 2068852461;         // 0x7B502EED

        // The FULL 64-bit class key -- hash64("physicsvehiclehandling"). Anything that goes
        // through the class registry (Attrib::FindCollection, RefSpec::GetClass) hashes the
        // whole doubleword, so the low word alone MISSES.
        static const u64 KU_PHYSICSVEHICLEHANDLING_CLASS_KEY = 0x966121397B502EEDull;

        // The class layout size (schema.vlt ClassLoadData::mLayoutSize == 192 == the ctor's
        // Attrib::DefaultDataArea argument == 8 * sizeof(Attrib::RefSpec)).
        static const u32 KU_LAYOUT_SIZE = 0xC0u;

        // ---- the eight members -------------------------------------------------------
        // For each: the 32-bit Attrib::Hash::physicsvehiclehandling::<Name> constant the DWARF
        // spells, the FULL 64-bit attribute key (Collection::GetNode hashes the whole
        // doubleword), and the member's offset in the class layout block.
        static const u32 KU_PHYSICSVEHICLESUSPENSIONATTRIBS_KEY           = 1360331405u;   // 0x5115028D
        static const u64 KU_PHYSICSVEHICLESUSPENSIONATTRIBS_ATTRIBUTE_KEY = 0x77E725F65115028Dull;
        static const u32 KU_PHYSICSVEHICLESUSPENSIONATTRIBS_OFFSET        = 0x00u;

        static const u32 KU_PHYSICSVEHICLESTEERINGATTRIBS_KEY             = 3366941343u;   // 0xC8AF729F
        static const u64 KU_PHYSICSVEHICLESTEERINGATTRIBS_ATTRIBUTE_KEY   = 0xC94C5AA0C8AF729Full;
        static const u32 KU_PHYSICSVEHICLESTEERINGATTRIBS_OFFSET          = 0x18u;

        static const u32 KU_PHYSICSVEHICLEENGINEATTRIBS_KEY               = 4157574291u;   // 0xF7CF8C93
        static const u64 KU_PHYSICSVEHICLEENGINEATTRIBS_ATTRIBUTE_KEY     = 0xCDE0C93EF7CF8C93ull;
        static const u32 KU_PHYSICSVEHICLEENGINEATTRIBS_OFFSET            = 0x30u;

        static const u32 KU_PHYSICSVEHICLEDRIFTATTRIBS_KEY                = 2976508990u;   // 0xB169EC3E
        static const u64 KU_PHYSICSVEHICLEDRIFTATTRIBS_ATTRIBUTE_KEY      = 0xE7D9D500B169EC3Eull;
        static const u32 KU_PHYSICSVEHICLEDRIFTATTRIBS_OFFSET             = 0x48u;

        static const u32 KU_PHYSICSVEHICLECOLLISIONATTRIBS_KEY            = 1594976127u;   // 0x5F11677F
        static const u64 KU_PHYSICSVEHICLECOLLISIONATTRIBS_ATTRIBUTE_KEY  = 0x15A0EBF95F11677Full;
        static const u32 KU_PHYSICSVEHICLECOLLISIONATTRIBS_OFFSET         = 0x60u;

        static const u32 KU_PHYSICSVEHICLEBOOSTATTRIBS_KEY                = 2255794068u;   // 0x8674AF94
        static const u64 KU_PHYSICSVEHICLEBOOSTATTRIBS_ATTRIBUTE_KEY      = 0x380862108674AF94ull;
        static const u32 KU_PHYSICSVEHICLEBOOSTATTRIBS_OFFSET             = 0x78u;

        static const u32 KU_PHYSICSVEHICLEBODYROLLATTRIBS_KEY             = 672897586u;    // 0x281B9A32
        static const u64 KU_PHYSICSVEHICLEBODYROLLATTRIBS_ATTRIBUTE_KEY   = 0xBE91B985281B9A32ull;
        static const u32 KU_PHYSICSVEHICLEBODYROLLATTRIBS_OFFSET          = 0x90u;

        static const u32 KU_PHYSICSVEHICLEBASEATTRIBS_KEY                 = 2948255455u;   // 0xAFBACEDF
        static const u64 KU_PHYSICSVEHICLEBASEATTRIBS_ATTRIBUTE_KEY       = 0xD3089AA5AFBACEDFull;
        static const u32 KU_PHYSICSVEHICLEBASEATTRIBS_OFFSET              = 0xA8u;

        // The generated class-key accessor (DWARF physicsvehiclehandling.h:15).
        static u64 ClassKey() { return KU_PHYSICSVEHICLEHANDLING_CLASS_KEY; }

        // @0x825BDAE0 -- the (Collection*, owner) ctor every consumer calls, always as
        // `physicsvehiclehandling(carAsset.PhysicsVehicleHandlingAsset().GetCollection(), 0)`.
        explicit physicsvehiclehandling(Collection* lpCollection = nullptr, void* lpOwner = nullptr);

        // @0x825BDB88 (`sub_825BDB88`) -- the generated COPY ctor. It exists out of line
        // because VehicleAttribs::SetupAttribs takes a physicsvehiclehandling BY VALUE, so the
        // caller must copy-construct the argument; the body chains the out-of-line
        // Attrib::Instance::Instance(const Instance&) @0x82802E60 (an unexported function
        // immediately after Instance::Instance @0x82802DB8, which ends at 0x82802E5C) and
        // then repeats the class check and the DefaultDataArea fallback.
        physicsvehiclehandling(const physicsvehiclehandling& lrOther);

        // The base's validity test, re-exported (the generated classes derive Instance
        // PRIVATELY). Same re-export the shotgroup sibling carries.
        using Instance::IsValid;

        // ---- the eight generated member accessors ------------------------------------
        // DWARF physicsvehiclehandling.h:75/:82/:89/:96/:103/:110/:117/:124 spell each of
        // these as `const Attrib::RefSpec& <Name>() const`. The console inlines them to a
        // bare `mpAttributeData + <offset>`; e.g. VehicleAttribs::SetupAttribs @0x825F4CD8
        // opens with
        //     lwz  r11, 4(r28)          ; the instance's mpAttributeData (the layout block)
        //     addi r3,  r11, 0xA8       ; &PhysicsVehicleBaseAttribs
        //     bl   Attrib::RefSpec::GetCollection
        //     bl   Attrib::Gen::physicsvehiclebaseattribs::physicsvehiclebaseattribs
        // and repeats it for 0x60 / 0x78 / 0x90 / 0x00 / 0x18 / 0x48 / 0x30.
        //
        // The non-const overloads exist because the resolve the call sites perform,
        // Attrib::RefSpec::GetCollection @0x82808530, caches into the ref and is declared
        // non-const in attribinstance.h.
        const Attrib::RefSpec& PhysicsVehicleSuspensionAttribs() const;
        Attrib::RefSpec&       PhysicsVehicleSuspensionAttribs();
        const Attrib::RefSpec& PhysicsVehicleSteeringAttribs() const;
        Attrib::RefSpec&       PhysicsVehicleSteeringAttribs();
        const Attrib::RefSpec& PhysicsVehicleEngineAttribs() const;
        Attrib::RefSpec&       PhysicsVehicleEngineAttribs();
        const Attrib::RefSpec& PhysicsVehicleDriftAttribs() const;
        Attrib::RefSpec&       PhysicsVehicleDriftAttribs();
        const Attrib::RefSpec& PhysicsVehicleCollisionAttribs() const;
        Attrib::RefSpec&       PhysicsVehicleCollisionAttribs();
        const Attrib::RefSpec& PhysicsVehicleBoostAttribs() const;
        Attrib::RefSpec&       PhysicsVehicleBoostAttribs();
        const Attrib::RefSpec& PhysicsVehicleBodyRollAttribs() const;
        Attrib::RefSpec&       PhysicsVehicleBodyRollAttribs();
        const Attrib::RefSpec& PhysicsVehicleBaseAttribs() const;
        Attrib::RefSpec&       PhysicsVehicleBaseAttribs();

    private:
        // The layout block is a packed array of Attrib::RefSpec (schema Definition:
        // mSize 24, mMaxCount 1, mAlignment log2 3). sizeof(Attrib::RefSpec) is 24 on the
        // console ({u64, u64, 4-byte collection SLOT} padded to 8) and 24 on x64
        // ({u64, u64, Collection*}) -- the stride does NOT change, so this is a plain
        // offset read on both, and the serialised 4-byte slot lands inside the host
        // pointer's 8 bytes (it is always 0 in the file; RefSpec::GetCollection fills it).
        Attrib::RefSpec* MemberAt(u32 luOffset) const
        {
            return reinterpret_cast<Attrib::RefSpec*>(
                static_cast<u8*>(GetLayoutPointer()) + luOffset);
        }
    };

    // X360 ctor @0x825BDAE0 (byte-identical in shape to the copy ctor at 0x825BDB88 that the
    // export set does carry, differing only in which Attrib::Instance ctor it chains):
    //     Attrib::Instance::Instance(this, collection, owner)
    //     if (GetClass() != 0x966121397B502EED && GetClass() != 0)
    //         AssertOnClassCheck(GetClass(), <the key>, GetCollection());
    //     if (!mpAttributeData) mpAttributeData = DefaultDataArea(0xC0);
    // The `GetClass() != 0` arm keeps a null-collection default construction quiet.
    inline physicsvehiclehandling::physicsvehiclehandling(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        if (GetClass() != KI_PHYSICSVEHICLEHANDLING_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_PHYSICSVEHICLEHANDLING_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(KU_LAYOUT_SIZE);
    }

    // X360 copy ctor @0x825BDB88 -- full asm:
    //     mr   r31, r3
    //     bl   sub_82802E60                 ; Attrib::Instance::Instance(const Instance&)
    //     bl   Attrib::Instance::GetClass   ; cmpld against 0x966121397B502EED
    //     ...  AssertOnClassCheck(GetClass(), key, GetCollection())
    //     lwz  r11, 4(r31) ; if zero -> li r3,0xC0 ; bl Attrib::DefaultDataArea ; stw r3,4(r31)
    //     return r31
    inline physicsvehiclehandling::physicsvehiclehandling(const physicsvehiclehandling& lrOther)
        : Instance(static_cast<const Instance&>(lrOther))
    {
        if (GetClass() != KI_PHYSICSVEHICLEHANDLING_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_PHYSICSVEHICLEHANDLING_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(KU_LAYOUT_SIZE);
    }

    inline const Attrib::RefSpec& physicsvehiclehandling::PhysicsVehicleSuspensionAttribs() const
    { return *MemberAt(KU_PHYSICSVEHICLESUSPENSIONATTRIBS_OFFSET); }
    inline Attrib::RefSpec& physicsvehiclehandling::PhysicsVehicleSuspensionAttribs()
    { return *MemberAt(KU_PHYSICSVEHICLESUSPENSIONATTRIBS_OFFSET); }

    inline const Attrib::RefSpec& physicsvehiclehandling::PhysicsVehicleSteeringAttribs() const
    { return *MemberAt(KU_PHYSICSVEHICLESTEERINGATTRIBS_OFFSET); }
    inline Attrib::RefSpec& physicsvehiclehandling::PhysicsVehicleSteeringAttribs()
    { return *MemberAt(KU_PHYSICSVEHICLESTEERINGATTRIBS_OFFSET); }

    inline const Attrib::RefSpec& physicsvehiclehandling::PhysicsVehicleEngineAttribs() const
    { return *MemberAt(KU_PHYSICSVEHICLEENGINEATTRIBS_OFFSET); }
    inline Attrib::RefSpec& physicsvehiclehandling::PhysicsVehicleEngineAttribs()
    { return *MemberAt(KU_PHYSICSVEHICLEENGINEATTRIBS_OFFSET); }

    inline const Attrib::RefSpec& physicsvehiclehandling::PhysicsVehicleDriftAttribs() const
    { return *MemberAt(KU_PHYSICSVEHICLEDRIFTATTRIBS_OFFSET); }
    inline Attrib::RefSpec& physicsvehiclehandling::PhysicsVehicleDriftAttribs()
    { return *MemberAt(KU_PHYSICSVEHICLEDRIFTATTRIBS_OFFSET); }

    inline const Attrib::RefSpec& physicsvehiclehandling::PhysicsVehicleCollisionAttribs() const
    { return *MemberAt(KU_PHYSICSVEHICLECOLLISIONATTRIBS_OFFSET); }
    inline Attrib::RefSpec& physicsvehiclehandling::PhysicsVehicleCollisionAttribs()
    { return *MemberAt(KU_PHYSICSVEHICLECOLLISIONATTRIBS_OFFSET); }

    inline const Attrib::RefSpec& physicsvehiclehandling::PhysicsVehicleBoostAttribs() const
    { return *MemberAt(KU_PHYSICSVEHICLEBOOSTATTRIBS_OFFSET); }
    inline Attrib::RefSpec& physicsvehiclehandling::PhysicsVehicleBoostAttribs()
    { return *MemberAt(KU_PHYSICSVEHICLEBOOSTATTRIBS_OFFSET); }

    inline const Attrib::RefSpec& physicsvehiclehandling::PhysicsVehicleBodyRollAttribs() const
    { return *MemberAt(KU_PHYSICSVEHICLEBODYROLLATTRIBS_OFFSET); }
    inline Attrib::RefSpec& physicsvehiclehandling::PhysicsVehicleBodyRollAttribs()
    { return *MemberAt(KU_PHYSICSVEHICLEBODYROLLATTRIBS_OFFSET); }

    inline const Attrib::RefSpec& physicsvehiclehandling::PhysicsVehicleBaseAttribs() const
    { return *MemberAt(KU_PHYSICSVEHICLEBASEATTRIBS_OFFSET); }
    inline Attrib::RefSpec& physicsvehiclehandling::PhysicsVehicleBaseAttribs()
    { return *MemberAt(KU_PHYSICSVEHICLEBASEATTRIBS_OFFSET); }
}
}
