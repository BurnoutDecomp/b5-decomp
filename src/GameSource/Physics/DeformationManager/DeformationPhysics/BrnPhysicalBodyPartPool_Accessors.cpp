// ============================================================================
// GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPartPool_Accessors.cpp
//
// BrnPhysics::Deformation::PhysicalBodyPartPool -- the three pool accessors the contact-spy
// bridge slice links against (via DetachedPartManager's inline wrappers):
//     GetPart (mutable) @ 0x825A0858
//     GetPart (const)   @ 0x825C1BE0   (a DISTINCT X360 body with its own assert set)
//     IsPartIndexUsed   @ 0x825A0758
// MOVED VERBATIM 2026-08-06 (bridge de-facade wave) out of the still-unmounted
// BrnPhysicalBodyPartPool.cpp (whose UpdateRWBodies/UpdateJoinedParts tail carries its own open
// closure) so these three can be on the mounted list on their own -- the established slice-TU
// pattern (BrnPhysicalBodyPartPool_Construct.cpp is the sibling precedent). Fold back into the
// home TU when it mounts.
// ============================================================================

#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPartPool.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnPhysics
{
namespace Deformation
{
    // ------------------------------------------------------------------------------------------
    // GetPart (mutable) @ 0x825A0858
    //   Two bounds asserts (index < 50, "Part index out of range" + the inlined CgsBitArray
    //   "invalid index") and the used-bit assert, all non-gating, then return &maParts[index].
    // ------------------------------------------------------------------------------------------
    PhysicalBodyPart* PhysicalBodyPartPool::GetPart(s16 li16Index)
    {
        const u32 luIndex = static_cast<u32>(li16Index);
        CGS_ASSERT(luIndex < KU_MAX_DETACHED_PARTS, "Part index out of range: ");
        CGS_ASSERT(luIndex < KU_MAX_DETACHED_PARTS, "invalid index : ");   // inlined CgsBitArray bounds tripwire (< 50)
        CGS_ASSERT(mUsedParts.IsBitSet(luIndex), "mUsedParts.IsBitSet( liPartIndex )");
        return &maParts[luIndex];
    }

    // ------------------------------------------------------------------------------------------
    // GetPart (const) @ 0x825C1BE0 -- the const overload is a DISTINCT body (NOT the mutable one).
    //   Its bounds assert message is "liPartIndex < (int32_t)KU_MAX_DETACHED_PARTS"
    //   (BrnPhysicalBodyPartPool.h:176) -- NOT the mutable overload's "Part index out of range" --
    //   followed by the inlined CgsBitArray "invalid index : N < 50" bounds tripwire, then the
    //   used-bit assert "mUsedParts.IsBitSet( liPartIndex )" (BrnPhysicalBodyPartPool.h:177). All
    //   non-gating; returns &maParts[index].
    // ------------------------------------------------------------------------------------------
    const PhysicalBodyPart* PhysicalBodyPartPool::GetPart(s16 li16Index) const
    {
        const u32 luIndex = static_cast<u32>(li16Index);
        CGS_ASSERT(static_cast<s32>(li16Index) < static_cast<s32>(KU_MAX_DETACHED_PARTS),
                   "liPartIndex < (int32_t)KU_MAX_DETACHED_PARTS");
        CGS_ASSERT(luIndex < KU_MAX_DETACHED_PARTS, "invalid index : ");
        CGS_ASSERT(mUsedParts.IsBitSet(luIndex), "mUsedParts.IsBitSet( liPartIndex )");
        return &maParts[luIndex];
    }

    // ------------------------------------------------------------------------------------------
    // IsPartIndexUsed @ 0x825A0758
    //   Single bounds assert (the inlined CgsBitArray "invalid index : N < 50", non-gating), then
    //   return the used-mask bit. The Hex-Rays `BOOL ... return ((a1 << SBYTE3(v14)) & v14) != 0`
    //   is the inlined IsBitSet bit-test.
    // ------------------------------------------------------------------------------------------
    bool PhysicalBodyPartPool::IsPartIndexUsed(s32 liIndex)
    {
        const u32 luIndex = static_cast<u32>(liIndex);
        CGS_ASSERT(luIndex < KU_MAX_DETACHED_PARTS, "invalid index : ");
        return mUsedParts.IsBitSet(luIndex);
    }

}
}
