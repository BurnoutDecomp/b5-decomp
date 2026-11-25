#pragma once

// Attrib::Gen::globalenginedata — generated AttribSys class (global engine-data
// attribute schema). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::globalenginedata::globalenginedata @ 0x82696330
//
// class-sourced (no LegacySrc partial source / DWARF for this TU) — same generated-ctor
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
        explicit globalenginedata(Collection* lpCollection = 0, void* lpOwner = 0);

        void ChangeWithDefault(const RefSpec& lrRefSpec)
        {
            RefSpec& lrMutable = const_cast<RefSpec&>(lrRefSpec);
            Change(const_cast<Collection*>(lrMutable.GetCollectionWithDefault()));
        }
    };

    // ARTIST 0x8269635C constructs the full class key 0x6DC9B98F827BE55F.
    inline globalenginedata::globalenginedata(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const u64 KU_GLOBALENGINEDATA_CLASS = 0x6DC9B98F827BE55FULL; // ARTIST 0x8269635C
        if (GetClass() != KU_GLOBALENGINEDATA_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KU_GLOBALENGINEDATA_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0xB0u);
    }
}
}
