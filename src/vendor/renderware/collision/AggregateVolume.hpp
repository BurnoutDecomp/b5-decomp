#pragma once

// ===========================================================================
// rw::collision::AggregateVolume -- a collision Volume whose shape is delegated
// to a collision Aggregate (a ClusteredMesh / KDTree procedural). The volume
// carries its own affine transform in the Volume base and a pointer to the
// aggregate in the Volume-base union slot; its bounding-box queries read the
// aggregate's cached AABBox (Aggregate::m_AABB @ +0x00) and transform it.
//
// OWNING HOME for the two AggregateVolume functions the X360 binary defines:
//     rw::collision::AggregateVolume::GetBBox      @ 0x82BBBA10
//     rw::collision::AggregateVolume::GetBBoxDiag  @ 0x82BBBB58
//
// LAYOUT (attested by the asm; DWARF aggregatevolume.h names):
//   Matrix44Affine mTransform  @ +0x00  Volume-base transform (64B; Volume::transform
//                                        sits at +0x00, so &mTransform == this)
//   Volume::VTable* (opaque)   @ +0x40  (modelled as opaque storage)
//   Aggregate*     mpAggregate @ +0x44  Volume-base union slot (Set/GetAggregate)
//
// The cached bounding box read by both bodies is Aggregate::m_AABB @ +0x00 (the
// aggregate's first member, aggregate.h:341), reached by reinterpreting the
// Aggregate* as a const AABBox*.
//
// FLAG: the full RenderWare Volume base + its VolumeMethods vtable are not
// reconstructed here; the +0x40 vtable slot is modelled as opaque storage so the
// transform (+0x00) and aggregate pointer (+0x44) land at their X360 offsets. This
// is the minimal home for the two GetBBox* bodies; promote to the full Volume
// hierarchy when that TU lands. This type is a distinct home from the minimal
// placeholder rw::collision::Volume in CollisionVolume.hpp and is not included
// alongside it.
// ===========================================================================

#include "types.hpp"
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

class AggregateVolume
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

    math::vpu::Matrix44Affine mTransform;     // +0x00 Volume-base transform
    unsigned char             maVTablePad[4]; // +0x40 opaque Volume::VTable* slot
    Aggregate*                mpAggregate;    // +0x44 Volume-base union slot
};

} // namespace collision
} // namespace rw
