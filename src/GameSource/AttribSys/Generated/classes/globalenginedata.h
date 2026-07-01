#pragma once

// Attrib::Gen::globalenginedata — generated AttribSys class (global engine-data
// attribute schema). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::globalenginedata::globalenginedata @ 0x82696330
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) — same generated-ctor
// pattern as debrisparams/surfacelist/iceanim/worldemitter. The X360 build inlines the
// generated accessor / `using Instance::…` API away, so the constructor is the only
// globalenginedata function in the ledger (minimal X360-faithful recon). Derives from
// Attrib::Instance.
//
// Referenced by BrnSound::Logic::Brn3DEffectControl::mEngineDataAtrib
// (GameSource/Sound/Module/LogicModule/Brn3DEffectControl.h), which currently models
// that member as a plain Attrib::Instance slice; this header supplies the full
// generated-class ctor for when that member is upgraded to the real generated type.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class globalenginedata : private Instance
    {
    public:
        explicit globalenginedata(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
    };

    // Chain the Instance ctor, assert the collection's class is
    // ClassName::globalenginedata, then give the instance a default data area (0xB0 bytes)
    // if construction left it without one.
    //
    // The class key is the value the ctor loads into r11 first via lis/ori — the LOW 32
    // bits of the 64-bit `insrdi` compare register — which Hex-Rays reports as the
    // comparison literal `&loc_827BE55C + 3` (= 0x827BE55F). The other half staged into
    // r10 (0x6DC9B98F) is the incidental/dead upper word of the 64-bit immediate load
    // (same key-staging shape as the sibling worldemitter/surfacelist ctors, where the
    // committed KI is the low half — e.g. worldemitter KI 0x99976E71 with dead upper
    // 0x475A643D). Here the low half is an X360 rodata ClassName-tag address rather than a
    // numeric hash; the attested literal is preserved for store-for-store fidelity.
    inline globalenginedata::globalenginedata(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_GLOBALENGINEDATA_CLASS = static_cast<int>(0x827BE55Fu); // Attrib::ClassName::globalenginedata (loc_827BE55C+3)
        if (GetClass() != KI_GLOBALENGINEDATA_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_GLOBALENGINEDATA_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0xB0u);
    }
}
}
