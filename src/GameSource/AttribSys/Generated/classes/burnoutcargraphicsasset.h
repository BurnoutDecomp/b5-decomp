#pragma once

// Attrib::Gen::burnoutcargraphicsasset -- generated AttribSys class (car graphics
// asset attributes). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::burnoutcargraphicsasset::burnoutcargraphicsasset @ 0x822BA0D8
//
// The X360 build inlines the generated accessor / `using Instance::...` API away, so
// the constructor is the only burnoutcargraphicsasset function in the ledger -- this is
// therefore a minimal, X360-faithful recon (class identity + ctor), same shape as the
// sibling generated classes surfacelist / debrisparams / iceanim. Derives from
// Attrib::Instance. Called by BrnWorld::ActiveRaceCar::OnResourcesLoaded and
// BrnTraffic::VehicleTypeRuntime::Prepare.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class burnoutcargraphicsasset : private Instance
    {
    public:
        explicit burnoutcargraphicsasset(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
    };

    // Chain the Instance ctor, assert the collection's class is
    // ClassName::burnoutcargraphicsasset (skipping the assert when the class is
    // unset/0), then give the instance a default data area (8 bytes) if it has none.
    inline burnoutcargraphicsasset::burnoutcargraphicsasset(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_BURNOUTCARGRAPHICSASSET_CLASS = 1712282196; // Attrib::ClassName::burnoutcargraphicsasset (0x660F5A54)
        if (GetClass() != KI_BURNOUTCARGRAPHICSASSET_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_BURNOUTCARGRAPHICSASSET_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(8u);
    }
}
}
