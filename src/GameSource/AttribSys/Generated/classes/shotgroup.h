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
        // The shotgroup class key the ctor resolves its collection against (0x388C8F47).
        static const int KI_SHOTGROUP_CLASS = 948423623;

        // The "ShotList" array attribute key (the shot list this group plays through).
        // 0x15246B49 == 354708297; used by Num_ShotList and the per-shot accessor.
        static const u32 KU_SHOTLIST_KEY = 354708297u;

        // Construct over the shotgroup collection named by the keyed group name. lpOwner is
        // the optional owning object the AttribSys collection resolve threads through.
        explicit shotgroup(Attrib::Key luGroupNameKey = 0, void* lpOwner = nullptr);

        // The number of shots in this group's ShotList (the length of the ShotList array
        // attribute). The intro state asserts this is > 0 before driving the group. REAL
        // X360 function @0x821F5948; declaration-only under the cl /c gate.
        u32 Num_ShotList() const;

        // The data block for this group's ShotList attribute (the per-shot ICE-anim
        // parameter block fed to BehaviourIceAnim::SetParameters). The X360 inlines the
        // generated Get_ShotList accessor down to Instance::GetAttributePointer, so this
        // forwards to that. FLAG: the X360 call also stages a 64-bit attribute key
        // (0x7533C0E2_15246B49) and a 0/1 selector in the argument registers, but the
        // resolved body (Attrib::Instance::GetAttributePointer @0x82805880) reads only
        // `this` and returns the collection data block -- the staged key/selector are
        // dead in the called body. lbUseSecond mirrors that 0/1 selector for provenance.
        const void* GetShotListData(bool lbUseSecond) const;

        // The per-shot data block at a given index in this group's ShotList. The rank-up
        // arbitrator state (BrnArbStateRankUp) walks the shots one rival at a time, indexing
        // by (rivalIndex % Num_ShotList()). The X360 stages the same 64-bit ShotList key with
        // the shot index in the low word (0x7533C0E2_15246B49 + the index selector), but the
        // resolved body (Attrib::Instance::GetAttributePointer @0x82805880) reads only `this`
        // and hands back the collection data block -- the staged key/index are dead in the
        // called body. liShotIndex mirrors that index selector for provenance.
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
            const void* lpElement =
                const_cast<shotgroup*>(this)->GetAttributePointer(0x7533C0E215246B49ULL, luIndex);
            if (lpElement == 0)
                lpElement = DefaultDataArea(0x18u);
            return lpElement;
        }
    };

    // X360 ctor @0x82208620: Collection = FindCollection(948423623, owner); chain the
    // Instance ctor over it. (The keyed group name the caller passes selects which named
    // shotgroup collection the AttribSys resolve binds.)
    inline shotgroup::shotgroup(Attrib::Key /*luGroupNameKey*/, void* lpOwner)
        : Instance(FindCollection(KI_SHOTGROUP_CLASS, lpOwner), lpOwner)
    {
    }

    inline const void* shotgroup::GetShotListData(bool /*lbUseSecond*/) const
    {
        return const_cast<shotgroup*>(this)->GetAttributePointer();
    }

    inline const void* shotgroup::GetShotListData(s32 /*liShotIndex*/) const
    {
        return const_cast<shotgroup*>(this)->GetAttributePointer();
    }
}
}
