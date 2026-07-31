#pragma once

// Attrib::Gen::shotgroup -- generated AttribSys class (a director "shot group": the
// ordered list of camera shots / ICE-anim takes an event intro plays through). The
// generated accessor / `using Instance::...` API is inlined away at the call sites, so
// only the handful of entry points the X360 ledger attests as real functions are
// modelled here (same minimal-recon convention as the sibling generated classes
// iceanim / surfacelist / debrisparams). Derives from Attrib::Instance.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::shotgroup::shotgroup     @ 0x82208620
//   Attrib::Gen::shotgroup::Num_ShotList  @ 0x821F5948
//
//   * the ctor resolves the shotgroup class collection via FindCollection(948423623,
//     owner) and chains Instance(Collection, owner) over it.
//   * Num_ShotList() reads the length of the "ShotList" array attribute (attribute key
//     354708297 / 0x15246B49) out of the instance -- a REAL X360 function, declared
//     here and bodied in the generated AttribSys layer (declaration-only under the
//     per-TU cl /c gate).
//   * the per-shot accessor the arbitrator intro state uses (GetShotListData below)
//     resolves the same "ShotList" attribute's data block; the X360 inlines it to
//     Attrib::Instance::GetAttributePointer (which hands back the collection data block).
#include "types.hpp"                                                          // u32 / s64
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"   // Attrib::FindCollection (canonical)

namespace Attrib
{
    // The string-keyed lookup the generated ctor's caller uses to turn a shot-group name
    // into the key it constructs over (codegen.cpp owns the body). Declared here so the
    // consumer (BrnArbStateRaceIntro::Prepare) can name it; declaration-only.
    u32 StringToKey(const char* pcName);

namespace Gen
{
    class shotgroup : private Instance
    {
    public:
        // The shotgroup class key the ctor resolves its collection against, LOW word
        // (0x3887CBC7 == 948423623). Kept for the AssertOnClassCheck sites that quote it.
        static const int KI_SHOTGROUP_CLASS = 948423623;

        // The FULL 64-bit class key Attrib::FindCollection resolves against. The ctor
        // @0x82208620 builds it in r3 as `lis 0x3887 / ori 0xCBC7 / lis 0x38ED /
        // ori 0x2D37 / insrdi r3,r11,32,0`, i.e. high word 0x38ED2D37 over
        // KI_SHOTGROUP_CLASS. The class registry is keyed by the whole doubleword, so
        // the low word alone MISSES.
        static const u64 KU_SHOTGROUP_CLASS_KEY = 0x38ED2D373887CBC7ULL;

        // The "ShotList" array attribute key (the shot list this group plays through).
        // 0x15246B49 == 354708297; the DecFIGS DWARF spells the same value as
        // Attrib::Hash::shotgroup::ShotList.
        static const u32 KU_SHOTLIST_KEY = 354708297u;

        // The FULL 64-bit "ShotList" attribute key. Both Num_ShotList @0x821F5948 and the
        // indexed element reads build it as `lis 0x1524 / ori 0x6B49 / lis 0x7533 /
        // ori 0xC0E2 / insrdi r5,r11,32,0`. The attribute table hashes the WHOLE
        // doubleword (Collection::GetNode -> HashMap::FindIndex takes it in one register),
        // so the low word alone MISSES -- the same trap the class key had.
        static const u64 KU_SHOTLIST_ATTRIBUTE_KEY = 0x7533C0E215246B49ULL;

        // Construct over the shotgroup collection named by the keyed group name. lpOwner is
        // the optional owning object the AttribSys collection resolve threads through.
        explicit shotgroup(Attrib::Key luGroupNameKey = 0, void* lpOwner = nullptr);

        // The number of shots in this group's ShotList (the length of the ShotList array
        // attribute). The intro state asserts this is > 0 before driving the group. REAL
        // X360 function @0x821F5948; declaration-only under the cl /c gate.
        u32 Num_ShotList() const;

        // The data block for this group's ShotList attribute (the per-shot ICE-anim
        // parameter block fed to BehaviourIceAnim::SetParameters). The X360 inlines the
        // generated Get_ShotList accessor down to Instance::GetAttributePointer.
        //
        // ⭐ CORRECTED 2026-07-31. Both of these used to call a no-argument
        // GetAttributePointer() and carried a comment saying the staged 64-bit key
        // (0x7533C0E2_15246B49) and the 0/1 selector "are dead in the called body". They
        // are NOT dead: GetAttributePointer @0x82805880 tail-calls Collection::GetData and
        // leaves r4/r5 untouched, so the key and the index are exactly what select the
        // element. Both forms now resolve the real indexed element, which makes them the
        // same call as GetShotListElement below -- kept as named spellings because the
        // call sites read better with them.
        const void* GetShotListData(bool lbUseSecond) const;

        // The per-shot data block at a given index in this group's ShotList. The rank-up
        // arbitrator state (BrnArbStateRankUp) walks the shots one rival at a time, indexing
        // by (rivalIndex % Num_ShotList()).
        const void* GetShotListData(s32 liShotIndex) const;

        // ShotList(index) -- the RefSpec for the index'th shot in this group's ShotList
        // (DWARF BrnShotSelector.cpp:169 names the generated accessor). ADDITIVE GROW
        // (ShotSelector::GetCrashShot @0x82239894, which drives it through the REAL
        // indexed Attrib::Instance::GetAttributePointer(key, index) overload -- unlike
        // the two provenance forwarders above -- with the 24-byte DefaultDataArea
        // null-element fallback @0x822398A0).
        const Attrib::RefSpec* ShotList(u32 luIndex) const
        {
            return reinterpret_cast<const Attrib::RefSpec*>(GetShotListElement(luIndex));
        }

        // ADDITIVE GROW (BrnArbStateCarSelect): the SAME indexed element resolve as
        // ShotList() above, handed back untyped. Every arbitrator state that feeds a shot to
        // BehaviourIceAnim::SetParameters/ChangeMovie needs the element as the generated
        // `iceanim` view of the block, not as a RefSpec -- and the X360 site is literally
        // `Attrib::Instance::GetAttributePointer(group, 0x15246B49, index)` returning void*
        // with the 24-byte DefaultDataArea null-element fallback (@0x822398A0), so the untyped
        // form is the faithful one; ShotList() is the typed convenience over it.
        const void* GetShotListElement(u32 luIndex) const
        {
            const void* lpElement = GetAttributePointer(KU_SHOTLIST_ATTRIBUTE_KEY, luIndex);
            if (lpElement == 0)
                lpElement = DefaultDataArea(0x18u);
            return lpElement;
        }
    };

    // X360 ctor @0x82208620:
    //     r3 = 0x38ED2D37_3887CBC7        (the class key -- the ONLY register it writes)
    //     bl Attrib::FindCollection       (r4 = the caller's key, untouched, = collection key)
    //     Attrib::Instance::Instance(this, collection, owner)   (r5 = owner)
    //
    // ⭐ FIXED 2026-07-31. This ctor used to read
    //     `: Instance(FindCollection(KI_SHOTGROUP_CLASS, lpOwner), lpOwner)`
    // with the group-name key COMMENTED OUT -- so every construction resolved the same
    // collection regardless of which shot group was asked for, and the owner pointer was
    // being passed where the collection key belongs. That is the defect
    // BrnDirectorResourceManager.h:203 flagged: all 65 of the director resource manager's
    // shot-group slots would have bound the wrong collection.
    inline shotgroup::shotgroup(Attrib::Key luGroupNameKey, void* lpOwner)
        : Instance(FindCollection(KU_SHOTGROUP_CLASS_KEY, luGroupNameKey), lpOwner)
    {
    }

    inline const void* shotgroup::GetShotListData(bool lbUseSecond) const
    {
        return GetAttributePointer(KU_SHOTLIST_ATTRIBUTE_KEY, lbUseSecond ? 1u : 0u);
    }

    inline const void* shotgroup::GetShotListData(s32 liShotIndex) const
    {
        return GetAttributePointer(KU_SHOTLIST_ATTRIBUTE_KEY, static_cast<u32>(liShotIndex));
    }
}
}
