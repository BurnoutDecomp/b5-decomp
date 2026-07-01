#pragma once

// Attrib::Gen::bloomasset -- generated AttribSys class (the bloom post-fx asset:
// BrnEffects::BloomData's backing attribute schema). The generated accessor / `using
// Instance::...` API is inlined away at the call site, so the constructor is the only
// bloomasset function in the X360 ledger -- a minimal generated-ctor recon, same shape
// as the sibling generated classes sparkeffect / shotgroup / iceanim / surfacelist /
// debrisparams. Derives from Attrib::Instance.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::bloomasset::bloomasset @ 0x82677E70
//
//   * the ctor resolves the bloomasset class collection via FindCollection(-1929517549)
//     and chains Instance(Collection, owner) over it -- same shape as the sparkeffect
//     twin (FindCollection resolves the collection, NO AssertOnClassCheck class-check).
//     The class key is staged as the low 32 bits of a 64-bit immediate: r3 is loaded
//     0x8CFDE613 (lis r11,-0x7303 / ori r3,r11,0xE613), then insrdi r3,r11,32,0 inserts
//     0xB632EC17 into the HIGH 32 bits, leaving the low word 0x8CFDE613 = -1929517549 as
//     the key -- matching the resolved pseudocode FindCollection(-1929517549). (Only r3
//     is set before the call; owner is NOT threaded to FindCollection, so this is a
//     single-arg call, mirroring the sparkeffect twin.)
//   * if construction left the instance without a data area, gives it a default 0x20-byte
//     one (asm: li r3,0x20; bl DefaultDataArea; stw r11,4(r31) -> mpAttributeData@+4).
//   * called by BrnEffects::BloomData::Construct.
#include "types.hpp"                                                          // (u32 for DefaultDataArea arg)
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"   // Attrib::FindCollection (canonical)

namespace Attrib
{
namespace Gen
{
    class bloomasset : private Instance
    {
    public:
        // The bloomasset class key the ctor resolves its collection against. The X360 ctor
        // builds this as the low 32 bits of a 64-bit immediate (0xB632EC17_8CFDE613);
        // Hex-Rays collapses the call to the single int FindCollection(-1929517549) -- the
        // high half (0xB632EC17) is a dead/incidental upper word of the 64-bit immediate
        // load, not a second argument (FindCollection reads only a 32-bit key here,
        // matching the sibling sparkeffect ctor's shape).
        static const int KI_BLOOMASSET_CLASS = -1929517549; // 0x8CFDE613

        // Construct over the bloomasset collection, optionally owned by lpOwner.
        explicit bloomasset(void* lpOwner = nullptr);
    };

    // X360 ctor @0x82677E70: Collection = FindCollection(KI_BLOOMASSET_CLASS); chain the
    // Instance ctor over it; then give the instance a default data area (0x20 bytes) if it
    // has none. No class-check assert in this ctor (unlike debrisparams/iceanim).
    inline bloomasset::bloomasset(void* lpOwner)
        : Instance(FindCollection(KI_BLOOMASSET_CLASS), lpOwner)
    {
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x20u);
    }
}
}
