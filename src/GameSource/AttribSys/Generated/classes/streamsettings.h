#pragma once

// Attrib::Gen::streamsettings — generated AttribSys class (sound streaming-effect
// attribute schema). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::streamsettings::streamsettings @ 0x82697400
//
// Called (as an embedded tail sub-object ctor, (this+0xB8, 0, 0)) from
// BrnSound::Logic::Streaming::StreamingEffect::StreamingEffect @ 0x826C9D10
// (see GameSource/Sound/Streaming/BrnStreamingEffect.cpp). Same generated-ctor
// pattern as the committed debrisparams/surfacelist siblings: chain Instance's
// ctor, then assert the collection's class matches (or is the "no collection yet"
// sentinel 0). The X360 build inlines the generated accessor / `using Instance::...`
// API away, so the constructor is the only streamsettings function in the ledger —
// this is a minimal, X360-faithful recon (class identity + ctor). Derives from
// Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribute.h"

namespace Attrib
{
namespace Gen
{
    class streamsettings : private Instance
    {
    public:
        explicit streamsettings(Collection* lpCollection = nullptr, void* lpOwner = nullptr);

        unsigned int Num_ContentSpecs() const
        {
            static const u64 KU_KEY = 0x5D2793AB142060E8ull;
            AttributeValue lCursor;
            streamsettings* lpSelf = const_cast<streamsettings*>(this);
            Attribute* lpAttribute = reinterpret_cast<Attribute*>(
                lpSelf->Get(&lCursor, reinterpret_cast<int*>(lpSelf), KU_KEY));
            return lpAttribute ? static_cast<unsigned int>(lpAttribute->GetLength()) : 0u;
        }

        unsigned int Num_Volumes() const
        {
            static const u64 KU_KEY = 0x8E163B6D7B027C7Eull;
            AttributeValue lCursor;
            streamsettings* lpSelf = const_cast<streamsettings*>(this);
            Attribute* lpAttribute = reinterpret_cast<Attribute*>(
                lpSelf->Get(&lCursor, reinterpret_cast<int*>(lpSelf), KU_KEY));
            return lpAttribute ? static_cast<unsigned int>(lpAttribute->GetLength()) : 0u;
        }

        u32 ContentSpecs(unsigned int auIndex) const
        {
            static const u64 KU_KEY = 0x5D2793AB142060E8ull;
            const void* lpValue = GetAttributePointer(KU_KEY, auIndex);
            return lpValue ? *static_cast<const u32*>(lpValue) : 0u;
        }

        f32 Volumes(unsigned int auIndex) const
        {
            static const u64 KU_KEY = 0x8E163B6D7B027C7Eull;
            const void* lpValue = GetAttributePointer(KU_KEY, auIndex);
            return lpValue ? *static_cast<const f32*>(lpValue) : 0.0f;
        }

        using Instance::GetCollection;
    };

    // Chain the Instance ctor, then assert the collection's class is
    // ClassName::streamsettings (0x7DC2E3D9 / 2109924313), UNLESS the class is
    // still the "no collection" sentinel (0) -- matching the asm's two
    // short-circuited GetClass() checks before the AssertOnClassCheck call.
    //
    // NOTE: unlike debrisparams/surfacelist, the X360 asm for THIS ctor does not
    // contain a trailing `DefaultDataArea` call -- both `beq cr6, loc_82697484`
    // branch targets ARE the epilogue (0x82697484 addi r1,r1,0x70 /
    // 0x82697488 b __restgprlr_29). Only Instance::Instance, two Instance::GetClass
    // calls, Instance::GetCollection, and AssertOnClassCheck are invoked. So no
    // `if (!mpAttributeData) mpAttributeData = DefaultDataArea(...)` tail is
    // reproduced here -- doing so would fabricate a call the asm does not make.
    inline streamsettings::streamsettings(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_STREAMSETTINGS_CLASS = 2109924313; // Attrib::ClassName::streamsettings (0x7DC2E3D9)
        if (GetClass() != KI_STREAMSETTINGS_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_STREAMSETTINGS_CLASS, GetCollection());
    }
}
}
