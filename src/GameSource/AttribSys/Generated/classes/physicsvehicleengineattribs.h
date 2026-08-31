#pragma once

// Attrib::Gen::physicsvehicleengineattribs — generated AttribSys class (physics vehicle
// engine tuning attributes). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::physicsvehicleengineattribs::physicsvehicleengineattribs @ 0x825BDD80
//
// The X360 build inlines the generated accessor / `using Instance::…` API away, so the
// constructor is the only physicsvehicleengineattribs function in the ledger — this is
// therefore a minimal, X360-faithful recon (class identity + ctor), same generated-ctor
// pattern as physicsvehiclebaseattribs/debrisparams/surfacelist. Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

#include <cstring>

namespace Attrib
{
namespace Gen
{
    class physicsvehicleengineattribs : private Instance
    {
    public:
        explicit physicsvehicleengineattribs(Collection* lpCollection = nullptr, void* lpOwner = nullptr);

        // Re-exposed from the private Instance base (attribs-data wave, 2026-08-09): the X360
        // consumer (VehicleAttribs::SetupAttribs @0x825F4CD8) reads the record through
        // `lwz +4` == mpAttributeData, which is what GetLayoutPointer returns.
        using Instance::GetLayoutPointer;

        // The two authored GearUpRPM vec3s occupy +0x20/+0x30 in the generated
        // layout (three gears apiece). VehicleState::UpdateParams reduces these
        // six values to the audio graph's maximum RPM, exactly as ARTIST does.
        f32 GetGearUpRPM(u32 auGear) const
        {
            if (auGear >= 6 || !GetLayoutPointer())
                return 0.0f;
            const u32 luGroup = auGear / 3;
            const u32 luLane = auGear % 3;
            f32 lfValue = 0.0f;
            const u8* lpData = static_cast<const u8*>(GetLayoutPointer());
            std::memcpy(&lfValue, lpData + 0x20 + luGroup * 0x10 + luLane * 4,
                        sizeof(lfValue));
            return lfValue;
        }
    };

    // Chain the Instance ctor, assert the collection's class is
    // ClassName::physicsvehicleengineattribs, then give the instance a default data area
    // (0x90 bytes) if construction left it without one.
    inline physicsvehicleengineattribs::physicsvehicleengineattribs(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_PHYSICSVEHICLEENGINEATTRIBS_CLASS = static_cast<int>(0xA54C9B92u); // Attrib::ClassName::physicsvehicleengineattribs
        if (GetClass() != KI_PHYSICSVEHICLEENGINEATTRIBS_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_PHYSICSVEHICLEENGINEATTRIBS_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x90u);
    }
}
}
