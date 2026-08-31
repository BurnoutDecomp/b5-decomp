#pragma once

// Attrib::Gen::audiosurface — generated AttribSys class (audio surface-type attribute
// schema). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::audiosurface::audiosurface @ 0x8269AD10
//
// The X360 build inlines the generated accessor / `using Instance::…` API away, so the
// constructor is the only audiosurface function in the ledger — this is therefore a
// minimal, X360-faithful recon (class identity + ctor), matching the sibling
// debrisparams/surfacelist Attrib::Gen::* generated classes. Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class audiosurface : private Instance
    {
    public:
        // The serialised 0x20-byte layout is pinned by RoadnoiseEffect::UpdateParams
        // @0x826E5CF8..0x826E5D68. These are data-format offsets and therefore do
        // not widen on the x64 host.
        struct _LayoutStruct
        {
            f32 mSurfaceLoopVolume;       // +0x00
            u8  mauUnhomed04[4];
            f32 mEnvelopeVolume;          // +0x08
            f32 mEnvelopeDecayTime;       // +0x0C
            f32 mEnvelopeAttackTime;      // +0x10
            u8  mauUnhomed14[0x0A];
            s16 miRoadnoiseLoop;           // +0x1E
        };

        explicit audiosurface(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
        explicit audiosurface(const RefSpec& lrRefSpec, void* lpOwner = nullptr);

        const f32& SurfaceLoopVolume() const { return Layout().mSurfaceLoopVolume; }
        const f32& EnvelopeVolume() const { return Layout().mEnvelopeVolume; }
        const f32& EnvelopeDecayTime() const { return Layout().mEnvelopeDecayTime; }
        const f32& EnvelopeAttackTime() const { return Layout().mEnvelopeAttackTime; }
        const s16& mRoadnoiseLoop() const { return Layout().miRoadnoiseLoop; }

    private:
        const _LayoutStruct& Layout() const
        {
            return *static_cast<const _LayoutStruct*>(GetLayoutPointer());
        }
    };

    static_assert(sizeof(audiosurface::_LayoutStruct) == 0x20,
                  "audiosurface serialised layout must remain 0x20 bytes");

    // Chain the Instance ctor, assert the collection's class is ClassName::audiosurface,
    // then give the instance a default data area (0x20 bytes) if it has none.
    inline audiosurface::audiosurface(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_AUDIOSURFACE_CLASS = 594563281; // Attrib::ClassName::audiosurface
        if (GetClass() != KI_AUDIOSURFACE_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_AUDIOSURFACE_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x20u);
    }

    inline audiosurface::audiosurface(const RefSpec& lrRefSpec, void* lpOwner)
        : Instance(lrRefSpec, lpOwner)
    {
        static const int KI_AUDIOSURFACE_CLASS = 594563281;
        if (GetClass() != KI_AUDIOSURFACE_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_AUDIOSURFACE_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x20u);
    }
}
}
