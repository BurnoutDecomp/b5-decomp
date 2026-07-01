#pragma once

// Attrib::Gen::passbybin — generated AttribSys class (passby-effect distance/volume
// bin table). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::passbybin::passbybin @ 0x82697688
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) — same generated-ctor
// pattern as surfacelist/debrisparams/iceanim/shotgroup. The X360 build inlines the
// generated accessor / `using Instance::…` API away, so the constructor is the only
// passbybin function in the ledger (minimal, X360-faithful recon). Derives from
// Attrib::Instance. Used by BrnSound::Logic::Passby::PassbyEffect /
// BrnSound::Logic::Passby::PassbyStateManager.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class passbybin : private Instance
    {
    public:
        explicit passbybin(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
    };

    // Chain the Instance ctor, assert the collection's class is ClassName::passbybin,
    // then give the instance a default data area (0x70 bytes) if it has none.
    inline passbybin::passbybin(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_PASSBYBIN_CLASS = 1854150515; // Attrib::ClassName::passbybin (0x6E841773)
        if (GetClass() != KI_PASSBYBIN_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_PASSBYBIN_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x70u);
    }
}
}
