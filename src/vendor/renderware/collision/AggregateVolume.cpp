// ===========================================================================
// rw::collision::AggregateVolume -- bounding-box queries. Reconstructed from
// BURNOUT_X360_ARTIST.XEX:
//     GetBBox      @ 0x82BBBA10
//     GetBBoxDiag  @ 0x82BBBB58
// The PowerPC asm is authoritative. Both bodies read the aggregate's cached box
// (Aggregate::m_AABB @ +0x00) and transform it via the committed AABBox::Transform
// (rw::collision::AAB @ 0x828B6788). See AggregateVolume.hpp for layout.
// ===========================================================================

#include "vendor/renderware/collision/AggregateVolume.hpp"
#include "vendor/renderware/collision/AABBox.hpp"
#include "vendor/renderware/collision/Aggregate.hpp"

namespace rw
{
namespace collision
{

// ---------------------------------------------------------------------------
// rw::collision::AggregateVolume::GetBBox @ 0x82BBBA10
//
// Canonical signature (DWARF aggregatevolume.h:122):
//   RwBool GetBBox(const Matrix44Affine* lpTransform, RwBool, AABBox& arBBox) const
// The RwBool second arg is part of the ABI shape but is NOT consumed by this
// body -- the asm tests r4 (the matrix pointer), not r5 (the RwBool).
//
//   r11 = *(this+0x44)              -> GetAggregate() (Volume-base union slot)
//   var_90 = aggregate->m_AABB      (4x ld/std: 32-byte cached AABBox copy)
//   if (lpTransform != NULL):
//       var_50 = Mult(this->mTransform, *lpTransform)   (committed affine Mult;
//                this transform rows @+0/0x10/0x20/0x30 broadcast against arg rows)
//       result = var_90.Transform(&var_50)
//   else:
//       result = var_90.Transform(&this->mTransform)    (AAB r5 = this =
//                &mTransform @ +0x00, since Volume::transform sits at +0x00)
//   arBBox = result                 (4x ld/std, 32-byte block copy)
//   return 1                        (li r3,1)
// ---------------------------------------------------------------------------
RwBool AggregateVolume::GetBBox(const math::vpu::Matrix44Affine* lpTransform,
                                RwBool /*abInstanced*/,
                                AABBox& arBBox) const
{
    // r11 = *(this+0x44); the cached AABBox lives at Aggregate+0x00 (m_AABB).
    const AABBox lAggBox = GetAggregate()->GetBBox();

    AABBox lResult;
    if (lpTransform != 0)
    {
        // Mult(Matrix44Affine this-transform, Matrix44 arg): volume transform
        // composed with the caller matrix, staged into the var_50 stack matrix.
        math::vpu::Matrix44Affine lCombined;
        const __vector4 lx = lpTransform->xAxis.mV;
        const __vector4 ly = lpTransform->yAxis.mV;
        const __vector4 lz = lpTransform->zAxis.mV;
        const __vector4 lw = lpTransform->wAxis.mV;
        const __vector4 ax = mTransform.xAxis.mV;
        const __vector4 ay = mTransform.yAxis.mV;
        const __vector4 az = mTransform.zAxis.mV;
        const __vector4 aw = mTransform.wAxis.mV;
        __vector4 vx = __vmulfp(__vspltw(ax, 0), lx);
        __vector4 vy = __vmulfp(__vspltw(ay, 0), lx);
        __vector4 vz = __vmulfp(__vspltw(az, 0), lx);
        __vector4 vw = __vmaddfp(__vspltw(aw, 0), lx, lw);
        vx = __vmaddfp(__vspltw(ax, 1), ly, vx);
        vy = __vmaddfp(__vspltw(ay, 1), ly, vy);
        vz = __vmaddfp(__vspltw(az, 1), ly, vz);
        vw = __vmaddfp(__vspltw(aw, 1), ly, vw);
        lCombined.xAxis.mV = __vmaddfp(__vspltw(ax, 2), lz, vx);
        lCombined.yAxis.mV = __vmaddfp(__vspltw(ay, 2), lz, vy);
        lCombined.zAxis.mV = __vmaddfp(__vspltw(az, 2), lz, vz);
        lCombined.wAxis.mV = __vmaddfp(__vspltw(aw, 2), lz, vw);
        lResult = lAggBox.Transform(&lCombined);
    }
    else
    {
        // Non-transform path: AAB(var_70, var_90, r5=this) -- r5 == &mTransform.
        lResult = lAggBox.Transform(&mTransform);
    }

    arBBox = lResult;   // 4x ld/std 32-byte copy into the caller's AABBox
    return 1;           // li r3,1
}

// ---------------------------------------------------------------------------
// rw::collision::AggregateVolume::GetBBoxDiag @ 0x82BBBB58
//
// Canonical signature (DWARF aggregatevolume.h:125): Vector3 GetBBoxDiag() const
// (returned through the hidden r3 slot -> r31).
//
//   var_50 = aggregate->m_AABB                    (4x ld/std, 32-byte copy)
//   var_30 = var_50.Transform(&this->mTransform)  (bl AAB, r5 = this =
//                                                  &mTransform; no caller matrix)
//   diag   = var_30.max - var_30.min              (lvx min, lvx max, vsubfp)
//   return diag                                    (stvx128 -> r31)
// ---------------------------------------------------------------------------
math::vpu::Vector3 AggregateVolume::GetBBoxDiag() const
{
    // r11 = *(this+0x44); cached AABBox at Aggregate+0x00 (see GetBBox above on the
    // accessor.
    const AABBox lAggBox = GetAggregate()->GetBBox();

    // AAB(var_30, var_50, r5=this): transform the cached box by the volume's
    // own transform (no caller matrix on this path).
    const AABBox lBox = lAggBox.Transform(&mTransform);

    return math::vpu::Vector3(__vsubfp(lBox.mMax.mV, lBox.mMin.mV));
}

} // namespace collision
} // namespace rw

// ARTIST folds these genuine per-type methods to 82BAC980, 82B0F1B8 or 82AD5078.
namespace rw { namespace collision {
RwBool AggregateVolume::GetMaximumFeature(RwBool, const Vector3&, Feature&) const { return 0; }
RwBool AggregateVolume::CreateGPInstance(GPInstance&, const Matrix44Affine*) const { return 0; }
void AggregateVolume::Release() {}
} }
