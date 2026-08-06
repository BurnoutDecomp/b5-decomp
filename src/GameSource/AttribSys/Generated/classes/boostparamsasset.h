#pragma once

// Attrib::Gen::boostparamsasset -- generated AttribSys class (the 34 boost
// tuning parameters; BoostBurnout2/3/5 each build one on the stack in their
// Prepare AND re-build it in their ApplyUpdate). Derives from Attrib::Instance.
//
// The generated per-attribute accessor API is fully INLINED at every X360 call
// site (one `lwz r11, 4(instance)` for the layout block, then fixed-offset
// lfs/lwz), so the CONSTRUCTOR is the only boostparamsasset symbol the X360
// ledger carries. The 34 accessors below are that inlined API modelled back
// out -- the same minimal-recon convention the sibling generated classes
// iceanim (SuitableFor/ShotProperties/GetAnimGuid) and physicsvehiclehandling
// (its eight named RefSpec members) already use.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::boostparamsasset::boostparamsasset @ 0x822B8C88
//
//   * The ctor resolves the boostparamsasset collection with
//     FindCollection(<class key>, <caller's key>) and chains
//     Instance(collection, owner) over it. Full body, all 21 instructions:
//         mr    r31, r3                  ; this
//         lis   r11, 0x4894
//         ori   r3,  r11, 0x3FAC         ; r3 = 0x48943FAC
//         lis   r11, -0x25DF
//         mr    r30, r5                  ; owner
//         ori   r11, r11, 0x657C         ; r11 = 0xDA21657C
//         insrdi r3, r11, 32,0           ; r3[0:31] = r11[32:63]
//   @0x822B8CB8  bl    Attrib__FindCollection    ; r3 = class key, r4 = UNTOUCHED
//         mr    r4, r3 / mr r3, r31 / mr r5, r30
//         bl    Attrib__Instance__Instance
//         lwz   r11, 4(r31) ; if 0 -> li r3,0x88 ; bl DefaultDataArea ; stw 4(r31)
//
//   * `insrdi r3,r11,32,0` inserts 0xDA21657C into the HIGH half of r3, so the
//     class key handed to FindCollection is the WHOLE doubleword
//     0xDA21657C_48943FAC == hash64("boostparamsasset"). Hex-Rays renders the
//     call as the single int FindCollection(1217675180) because the body never
//     writes r4 -- that is a decompiler artifact, not a one-argument call.
//
//   * No AssertOnClassCheck in this ctor (like the sparkeffect / cameradefaults
//     siblings, unlike debrisparams / iceanim / surfacelist).
//
// ============================================================================
// FALSE COMMENT REMOVED, 2026-08-06 -- this file used to state that "the
// incoming 2nd parameter (r4) is never read in this ctor (dead, same as
// shotgroup's unused luGroupNameKey)". THAT IS BACKWARDS, and it is exactly the
// note that justified the u32 truncation on the declaration below.
//
// The ctor does not WRITE r4; that is precisely why r4 is LIVE. Between the
// entry at 0x822B8C88 and `bl Attrib__FindCollection` @0x822B8CB8 the only
// registers written are r31 (this), r30 (owner) and r3 (the class key). r4
// therefore arrives at FindCollection holding whatever the CALLER put there --
// i.e. the caller's key IS FindCollection's COLLECTION key. Same shape, same
// reasoning, and the same 2026-07-31 correction as
// attrib_findcollection.h:33-39 / shotgroup.h:149-152 / cameradefaults.h:38-44.
//
// All six call sites prove it directly; BoostBurnout2::ApplyUpdate is verbatim:
//     0x822C1450  lis   r3, 8                         ; with the ori below,
//     0x822C1454  li    r5, 0xA                       ;   r3 = 0x0008CA05
//     0x822C1458  addi  r4, r1, <text buf>            ;   == 576005, radix 10
//     0x822C145C  ori   r3, r3, 0xCA05
//     0x822C1460  bl    rw__core__stdc__ConvertI64ToA ; -> "576005"
//     0x822C1468  bl    Attrib__StringToKey           ; r3 = the FULL u64 hash
//     0x822C146C  mr    r4, r3                        ; 64-bit move, no clrldi
//     0x822C1470  li    r5, 0                         ; owner
//     0x822C1478  bl    Attrib__Gen__boostparamsasset__boostparamsasset
// (BoostBurnout5::ApplyUpdate @0x822C2274-0x822C2284 is instruction-identical;
// BoostBurnout2/3/5 Prepare and BoostBurnout3::ApplyUpdate all repeat it with
// their own GUID -- 576005 / 576011 / and the BoostBurnout5 stunt GUID.)
//
// Attrib::StringToKey returns u64 (AttributeKey.h:43,47 -- @0x82805828 tail-
// calls the lookup8 hash and returns r3 whole, no narrowing), and
// Attrib::FindCollection hashes the collection key as a whole doubleword. With
// a u32 parameter here the high word was dropped at the call boundary, every
// lookup missed, the ctor fell through to DefaultDataArea(0x88), and all 34
// tuning parameters read back as a ZEROED default record.
// ============================================================================
#include "types.hpp"                                                          // f32 / s32 / u8 / u32 / u64
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"   // Attrib::FindCollection (canonical)

namespace Attrib
{
namespace Gen
{
    class boostparamsasset : private Instance
    {
    public:
        // The boostparamsasset class key, LOW word (0x48943FAC == 1217675180).
        // Kept because Attrib::Instance::GetClass() is modelled 32-bit, and
        // because it is what Hex-Rays surfaces for the FindCollection call.
        static const int KI_BOOSTPARAMSASSET_CLASS = 0x48943FAC; // 1217675180

        // The FULL 64-bit class key -- the doubleword the ctor stages in r3 with
        // lis/ori + insrdi, == hash64("boostparamsasset") (independently attested
        // in attribhash64.cpp:22 and AttributeKey.h:58). The class registry is
        // keyed by the whole doubleword, so the low word alone MISSES.
        static const u64 KU_BOOSTPARAMSASSET_CLASS_KEY = 0xDA21657C48943FACULL;

        // The generated class-key accessor (DecFIGS DWARF boostparamsasset.h:15),
        // same spelling the physicsvehiclehandling sibling carries.
        static u64 ClassKey() { return KU_BOOSTPARAMSASSET_CLASS_KEY; }

        // The class layout size: the ctor's own DefaultDataArea argument
        // (`li r3, 0x88` @0x822B8CD8), and exactly 34 * 4 bytes -- the 34
        // attributes the DWARF declares, at a 4-byte stride, with no padding.
        static const u32 KU_LAYOUT_SIZE = 0x88u;

        // Construct over the boostparamsasset collection named by luCollectionKey
        // (the callers pass Attrib::StringToKey("<asset GUID>")). lpOwner is the
        // optional owning object threaded through to Attrib::Instance.
        //
        // WIDENED to u64 2026-08-06. See the FALSE COMMENT REMOVED block above:
        // the key is LIVE, it is the collection key, and Attrib::StringToKey
        // produces a full 64-bit hash. This is the identical defect (and the
        // identical fix) shotgroup.h:72 and cameradefaults.h:51 took on
        // 2026-07-31; boostparamsasset was missed in that sweep.
        //
        // The key KEEPS its default here, unlike those two siblings. Their
        // defaults were removed because each has a second (Collection*, owner)
        // element ctor that a PRE-MAIN by-value member would otherwise bind to
        // the key ctor -- boostparamsasset has no such overload and no by-value
        // holder anywhere in the tree (all six console sites build it on the
        // stack inside Prepare/ApplyUpdate), so no pre-main FindCollection can
        // happen. Do not add a (Collection*, owner) overload without also
        // dropping this default.
        explicit boostparamsasset(u64 luCollectionKey = 0, void* lpOwner = nullptr);

        // ------------------------------------------------------------------
        // THE 34 TUNING PARAMETERS.
        //
        // NAMES: every one is a DecFIGS DWARF accessor, declared verbatim at
        //   boostparamsasset.h:73-307 as `const _LayoutStruct::Float& <Name>()
        //   const` (or `_LayoutStruct::Int32&` for the two marked Int32).
        //   Nothing here is invented. They are returned BY VALUE rather than by
        //   const reference -- the committed minimal-recon convention for
        //   inlined generated accessors (iceanim.h:43-44, iceanim.cpp:71).
        //
        // OFFSETS: the AttribSys layout block is the 34 attributes in exact
        //   REVERSE-ALPHABETICAL order at a 4-byte stride. That ordering is not
        //   assumed -- it is the rule the committed siblings already witness:
        //     physicsvehiclehandling  8/8 members (0x00 Suspension .. 0xA8 Base)
        //     proceduralshot          SuitableFor [0], ShotProperties [2]
        //     iceanim                 SuitableFor [0], ShotProperties [1],
        //                             Guid [3]  (iceanim.cpp:73)
        //   and it is pinned INDEPENDENTLY for this class by the two Int32
        //   attributes: SpeedForMin/MaxEarning are the only non-Float members
        //   the DWARF declares, reverse-alphabetical puts them at 0x1C/0x20,
        //   and 0x1C/0x20 are exactly the only two slots every console reader
        //   loads with lwz+extsw+fcfid instead of lfs (BoostBurnout2::ApplyUpdate
        //   @0x822C1498-0x822C14CC, BoostBurnout5::ApplyUpdate @0x822C22A4).
        //   34 * 4 == 0x88 == the ctor's DefaultDataArea size, with every slot
        //   0x00..0x84 read, so the block is fully accounted for.
        //
        // The same 34 record-offset -> member mapping is carried independently
        // on the consumer side in BrnBoostStrategy.h:387-420, derived there from
        // the store order of BoostBurnout2::Prepare @0x822C0FA0; the two agree
        // 34-for-34. Semantic spot-checks in ApplyUpdate @0x822C1128 corroborate
        // it: +0x18 AirEarning is scaled by dt only under the "is airborne"
        // virtual (0x822C1170), +0x14 DriftEarning only under the "is drifting"
        // virtual (0x822C11DC), +0x44 TailgatingEarning only under the
        // "is tailgating" virtual (0x822C1280), and +0x70 BurnRateBoost is the
        // per-second drain in `fnmsubs f0, f0, f31, f13` (loaded 0x822C13EC,
        // consumed 0x822C13F0).
        //
        // NOT MODELLED: the 64-bit Attrib::Hash::boostparamsasset::* attribute
        // keys. The DWARF carries only their LOW words and the schema.vlt that
        // holds the full doublewords is not extracted in this tree -- and the
        // console never needs them here, because it reads the layout block by
        // offset. Do not fabricate them.
        // ------------------------------------------------------------------
        f32 TrafficCheck()             const { return FloatAt(0x00); }
        f32 TradingPaintEarning()      const { return FloatAt(0x04); }
        f32 TakedownEarning()          const { return FloatAt(0x08); }
        f32 TailgatingEarning()        const { return FloatAt(0x0C); }
        f32 StuntSmashEarning()        const { return FloatAt(0x10); }
        f32 StuntJumpEarning()         const { return FloatAt(0x14); }
        f32 StuntBillBoardEarning()    const { return FloatAt(0x18); }
        s32 SpeedForMinEarning()       const { return IntAt(0x1C); }   // attrib Int32
        s32 SpeedForMaxEarning()       const { return IntAt(0x20); }   // attrib Int32
        f32 SlamEarning()              const { return FloatAt(0x24); }
        f32 ShuntEarning()             const { return FloatAt(0x28); }
        f32 RubbingEarning()           const { return FloatAt(0x2C); }
        f32 OnWrecked()                const { return FloatAt(0x30); }
        f32 OnComing()                 const { return FloatAt(0x34); }
        f32 NudgeEarning()             const { return FloatAt(0x38); }
        f32 NearMissBoostEarning()     const { return FloatAt(0x3C); }
        f32 MaxSpeedBoostModifier()    const { return FloatAt(0x40); }
        f32 Handbrake360Earning()      const { return FloatAt(0x44); }
        f32 Handbrake180Earning()      const { return FloatAt(0x48); }
        f32 GrindingEarning()          const { return FloatAt(0x4C); }
        f32 FakieLanding()             const { return FloatAt(0x50); }
        f32 DriftEarning()             const { return FloatAt(0x54); }
        f32 CrashEscapeBoostEarning()  const { return FloatAt(0x58); }
        f32 ComboModifier()            const { return FloatAt(0x5C); }
        f32 CleanLanding()             const { return FloatAt(0x60); }
        f32 BurnRateBoost()            const { return FloatAt(0x64); }
        f32 BoostSpinIncrease()        const { return FloatAt(0x68); }
        f32 BoostSlamStrength()        const { return FloatAt(0x6C); }
        f32 BoostChainMin()            const { return FloatAt(0x70); }
        f32 BoostChainBonus()          const { return FloatAt(0x74); }
        f32 BeingSlammed()             const { return FloatAt(0x78); }
        f32 BarrelRollEarning()        const { return FloatAt(0x7C); }
        f32 AirSpinEarning()           const { return FloatAt(0x80); }
        f32 AirEarning()               const { return FloatAt(0x84); }

        // The base's validity test, re-exported past the PRIVATE inheritance
        // (same using-declaration precedent as shotgroup.h:96 /
        // cameradefaults.h:71). The console does not test it at these six sites
        // -- it reads the layout block unconditionally, because the ctor's
        // DefaultDataArea tail guarantees one exists -- but a consumer that
        // wants to know whether the collection actually resolved needs it, and
        // with the key truncation fixed that distinction finally means something.
        using Instance::IsValid;

    private:
        // The layout block is a packed array of 4-byte attribute slots. Its
        // stride does NOT change between console and host (Float/Int32 are 4
        // bytes on both), so the console offsets are the host offsets -- this is
        // the same reasoning physicsvehiclehandling.h:150-161 records for its
        // own MemberAt(), and the same reason BrnBoostStrategy.h can assert the
        // console offsets of the 34 members these feed.
        //
        // GetLayoutPointer() is Attrib::Instance::mpAttributeData -- console
        // instance+0x04 (the `lwz r11, 4(...)` every reader opens with), host
        // instance+0x08. Going through the accessor is what keeps that widening
        // correct; DO NOT re-export GetLayoutPointer and poke the block from a
        // consumer TU (see the note in the class banner).
        const void* SlotAt(u32 luOffset) const
        {
            return static_cast<const u8*>(GetLayoutPointer()) + luOffset;
        }
        f32 FloatAt(u32 luOffset) const
        {
            return *static_cast<const f32*>(SlotAt(luOffset));
        }
        s32 IntAt(u32 luOffset) const
        {
            return *static_cast<const s32*>(SlotAt(luOffset));
        }
    };

    // X360 ctor @0x822B8C88: collection = FindCollection(0xDA21657C_48943FAC,
    // <the caller's key, arriving untouched in r4>); chain the Instance ctor
    // over it; then give the instance a default data area (0x88 bytes) if it has
    // none. No class-check assert appears in this ctor's asm.
    inline boostparamsasset::boostparamsasset(u64 luCollectionKey, void* lpOwner)
        : Instance(FindCollection(KU_BOOSTPARAMSASSET_CLASS_KEY, luCollectionKey), lpOwner)
    {
        if (!GetLayoutPointer())
            mpAttributeData = DefaultDataArea(KU_LAYOUT_SIZE);
    }
}
}
