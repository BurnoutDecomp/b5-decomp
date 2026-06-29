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

namespace Attrib
{
    // The string-keyed lookup the generated ctor's caller uses to turn a shot-group name
    // into the key it constructs over (codegen.cpp owns the body). Declared here so the
    // consumer (BrnArbStateRaceIntro::Prepare) can name it; declaration-only.
    u32 StringToKey(const char* pcName);

    // Resolve the collection for a class/group key (the shotgroup ctor uses it; the
    // AttribSys runtime owns the body). Declaration-only under the cl /c gate.
    Collection* FindCollection(int liKey, void* lpOwner = nullptr);

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
}
}
