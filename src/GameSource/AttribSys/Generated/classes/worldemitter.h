#pragma once

// Attrib::Gen::worldemitter — generated AttribSys class (world sound-emitter attribute
// schema; instantiated by BrnSound::Logic::World::EmitterEffect::Attach). Reconstructed
// from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::worldemitter::worldemitter @ 0x8269C0B0
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) — same generated-ctor
// pattern as debrisparams/surfacelist. The X360 build inlines the generated accessor /
// `using Instance::…` API away, so the constructor is the only worldemitter function in
// the ledger (minimal X360-faithful recon). Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class worldemitter : public Instance
    {
    public:
        explicit worldemitter(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
        explicit worldemitter(const RefSpec& lrRefSpec, void* lpOwner = nullptr)
            : Instance(lrRefSpec, lpOwner) {}

        void ChangeWithDefault(const RefSpec& lrRefSpec)
        {
            RefSpec& lrMutable = const_cast<RefSpec&>(lrRefSpec);
            Change(const_cast<Collection*>(lrMutable.GetCollectionWithDefault()));
        }

        const char* EmitterName() const
        {
            return static_cast<const Layout*>(mpAttributeData)->mpEmitterName;
        }
        bool IsStream() const
        {
            return static_cast<const Layout*>(mpAttributeData)->mbIsStream;
        }
        bool AffectedByDoppler() const
        {
            return static_cast<const Layout*>(mpAttributeData)->mbAffectedByDoppler;
        }

    private:
        // libapt2/AttribSys native-8 data keeps Text as a native pointer.  The
        // two generated Bool fields follow it in source declaration order.
        struct Layout
        {
            const char* mpEmitterName;
            bool mbIsStream;
            bool mbAffectedByDoppler;
        };
    };

    // Chain the Instance ctor, assert the collection's class is ClassName::worldemitter,
    // then give the instance a default data area (8 bytes) if it has none.
    inline worldemitter::worldemitter(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_WORLDEMITTER_CLASS = -1718129039; // Attrib::ClassName::worldemitter
        if (GetClass() != KI_WORLDEMITTER_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_WORLDEMITTER_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(sizeof(Layout));
    }
}
}
