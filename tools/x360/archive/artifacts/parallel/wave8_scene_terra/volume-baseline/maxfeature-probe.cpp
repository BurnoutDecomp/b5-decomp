#include "SDKs/EATech/rwcollision/volume_debug_access.h"
#include "vendor/renderware/collision/Feature.hpp"
namespace rw { namespace collision { struct SphereVolume : public Volume { RwBool GetMaximumFeature(RwBool, const Vector3&, Feature&) const; }; struct BoxVolume : public Volume { RwBool GetMaximumFeature(RwBool, Vector3, Feature&) const; };
RwBool SphereVolume::GetMaximumFeature(RwBool /*abCcw*/, const Vector3& /*arDir*/,
                                       Feature& arFeature) const
{
    arFeature.numedges = 0;
    arFeature.pt       = mTransform.wAxis;
    return 1;
}
RwBool BoxVolume::GetMaximumFeature(RwBool /*abCcw*/, Vector3 /*aDir*/,
                                    Feature& arFeature) const
{
    arFeature.numedges = 0;
    arFeature.pt       = mTransform.wAxis;
    return 1;
}
} }
