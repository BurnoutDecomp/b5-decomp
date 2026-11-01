#pragma once

// AggregateVolume is a Volume whose shape is supplied by an Aggregate.
// ARTIST 82BBBA10/82BBBB58 read the inherited aggregate arm at +0x44.

#include <cstddef>   // offsetof (layout pins)

#include "types.hpp"
#include "SDKs/EATech/rwcollision/volume_debug_access.h"
#include "vendor/renderware/collision/AABBox.hpp"
#include "vendor/renderware/collision/Aggregate.hpp"
#include "SDKs/EATech/include/rw/math/vpu/matrix44.h"   // math::vpu::Matrix44 / Matrix44Affine + Mult

namespace rw
{
namespace collision
{

// RenderWare boolean (canonical RwBool; the vendor typedef is a 32-bit int). Identical
// to the typedef in FeatureEdge.hpp / provided the same way there -- an identical typedef
// in the same namespace is legal, so co-inclusion with those headers does not clash.
typedef s32 RwBool;

class AggregateVolume : public Volume
{
public:
    // @ 0x82BBBA10 -- return the aggregate's cached box, transformed by (this->mTransform
    // composed with lpTransform) when lpTransform is non-null, else by this->mTransform.
    // The RwBool second arg is part of the ABI shape but is not consumed by the body.
    RwBool GetBBox(const math::vpu::Matrix44Affine* lpTransform,
                   RwBool abInstanced,
                   AABBox& arBBox) const;

    // @ 0x82BBBB58 -- return the (max - min) diagonal of the aggregate's cached box
    // transformed by this->mTransform.
    math::vpu::Vector3 GetBBoxDiag() const;

    RwBool GetMaximumFeature(RwBool abCcw, const Vector3& arDir, Feature& arFeature) const;
    RwBool CreateGPInstance(GPInstance& arInstance, const Matrix44Affine* lpTransform) const;
    void Release();

    Aggregate* GetAggregate() const { return mAggregateData.mpAggregate; }
    void SetAggregate(Aggregate* lpAggregate) { mAggregateData.mpAggregate = lpAggregate; }

};

// --- layout pins (console offsets; the reinterpret in VolumeVTables.cpp rests on these)
CGS_STATIC_ASSERT((offsetof(AggregateVolume, mTransform)   == 0x00), "AggregateVolume::transform");
CGS_STATIC_ASSERT((offsetof(AggregateVolume, mpVTable) == 0x40), "AggregateVolume::vTable slot");
CGS_STATIC_ASSERT((offsetof(AggregateVolume, mfRadius)     == 0x50), "AggregateVolume::radius");
CGS_STATIC_ASSERT((offsetof(AggregateVolume, muFlags)      == 0x5C), "AggregateVolume::m_flags");
CGS_STATIC_ASSERT((sizeof(AggregateVolume) == 96),
              "AggregateVolume models the same 96-byte serialised Volume record");

} // namespace collision
} // namespace rw
