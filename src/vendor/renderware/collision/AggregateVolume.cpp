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
    // GetAggregate() is the +0x44 accessor -- a bare `Aggregate*` member gets pushed to
    // +0x48 by x64 alignment, which is the defect fixed 2026-08-19 (see the header).
    const AABBox lAggBox = *reinterpret_cast<const AABBox*>(GetAggregate());

    AABBox lResult;
    if (lpTransform != 0)
    {
        // Mult(Matrix44Affine this-transform, Matrix44 arg): volume transform
        // composed with the caller matrix, staged into the var_50 stack matrix.
        const math::vpu::Matrix44 lCombined =
            math::vpu::Mult(mTransform, *reinterpret_cast<const math::vpu::Matrix44*>(lpTransform));

        lResult = lAggBox.Transform(
            reinterpret_cast<const math::vpu::Matrix44Affine*>(&lCombined));
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
    // accessor -- this offset was measurably wrong until 2026-08-19).
    const AABBox lAggBox = *reinterpret_cast<const AABBox*>(GetAggregate());

    // AAB(var_30, var_50, r5=this): transform the cached box by the volume's
    // own transform (no caller matrix on this path).
    const AABBox lBox = lAggBox.Transform(&mTransform);

    // vsubfp: per-lane (max - min) of the transformed box, all four lanes.
    const f32* lafMin = lBox.mMin.mV.mafLane;
    const f32* lafMax = lBox.mMax.mV.mafLane;

    math::vpu::Vector3 lDiag;
    for (int i = 0; i < 4; ++i)
    {
        lDiag.mV.mafLane[i] = lafMax[i] - lafMin[i];
    }
    return lDiag;
}

} // namespace collision
} // namespace rw
