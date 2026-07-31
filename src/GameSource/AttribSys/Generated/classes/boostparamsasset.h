#pragma once

// Attrib::Gen::boostparamsasset -- generated AttribSys class (boost tuning
// parameters: BoostBurnout2/3/5 read this asset in Prepare/ApplyUpdate). The
// generated accessor / `using Instance::...` API is inlined away at the call
// sites, so the constructor is the only boostparamsasset entry point the X360
// ledger attests as real (minimal generated-ctor recon, same shape as the
// sibling generated classes sparkeffect / shotgroup / debrisparams / surfacelist).
// Derives from Attrib::Instance.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::boostparamsasset::boostparamsasset @ 0x822B8C88
//
//   * the ctor resolves the boostparamsasset class collection via
//     FindCollection(0x48943FAC) and chains Instance(Collection, owner) over
//     it. The X360 builds the argument as the low 32 bits of a 64-bit immediate
//     register (lis/ori r3=0x48943FAC, then insrdi folds the incidental upper
//     word 0xDA21657C into r3's high half); Hex-Rays collapses the call to the
//     single int FindCollection(1217675180) == FindCollection(0x48943FAC) --
//     the high half is a dead/incidental upper word of the 64-bit immediate
//     load, NOT a second argument. This matches the sibling sparkeffect /
//     surfacelist key-staging pattern exactly (32-bit class key).
//   * chains Instance::Instance(this, Collection, owner) -- the incoming 2nd
//     parameter (r4) is never read in this ctor (dead, same as shotgroup's
//     unused luGroupNameKey); the incoming 3rd parameter (r5, saved in r30) is
//     the owner and is forwarded to Instance::Instance's owner slot.
//   * gives the instance a default data area (0x88 bytes) if Instance::Instance
//     left mpAttributeData (this+4) null. No AssertOnClassCheck in this ctor
//     (like sparkeffect, unlike debrisparams/iceanim/surfacelist).
#include "types.hpp"                                                          // u32
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"   // Attrib::FindCollection (canonical)

namespace Attrib
{
namespace Gen
{
    class boostparamsasset : private Instance
    {
    public:
        // The boostparamsasset class key the ctor resolves its collection
        // against. The X360 stages this as the low 32 bits of a 64-bit immediate
        // register (0xDA21657C_48943FAC); Hex-Rays reports FindCollection(
        // 1217675180) == 0x48943FAC, matching the sibling sparkeffect/surfacelist
        // 32-bit-key shape.
        static const int KI_BOOSTPARAMSASSET_CLASS = 0x48943FAC; // 1217675180

        // The FULL 64-bit class key Attrib::FindCollection resolves against -- the
        // doubleword the X360 ctor stages in r3 with lis/ori + insrdi. KI_KI_BOOSTPARAMSASSET_CLASS
        // above is only its LOW word (which is what Hex-Rays surfaces, and what this
        // header used to pass to the old one-key FindCollection(int)); the class
        // registry is keyed by the whole doubleword, so the low word alone MISSES.
        static const u64 KU_BOOSTPARAMSASSET_CLASS_KEY = 0xDA21657C48943FACULL;

        // Construct over the boostparamsasset collection. luUnused mirrors the
        // incoming 2nd parameter the X360 ctor never reads; lpOwner is the
        // optional owning object threaded through to Instance.
        explicit boostparamsasset(u32 luCollectionKey = 0, void* lpOwner = nullptr);
    };

    // X360 ctor @0x822B8C88: Collection = FindCollection(0x48943FAC); chain the
    // Instance ctor over it; then give the instance a default data area (0x88
    // bytes) if it has none.
    inline boostparamsasset::boostparamsasset(u32 luCollectionKey, void* lpOwner)
        : Instance(FindCollection(KU_BOOSTPARAMSASSET_CLASS_KEY, luCollectionKey), lpOwner)
    {
        if (!GetLayoutPointer())
            mpAttributeData = DefaultDataArea(0x88u);
    }
}
}
