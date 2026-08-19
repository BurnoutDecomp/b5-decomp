#ifndef CGS_CAPSULE_H
#define CGS_CAPSULE_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3, Vector3Plus, VecFloat (rw::math::vpu)

// ============================================================================
// GameShared/GameClasses/Geometric/Primitives/CgsCapsule.h
//
// CgsGeometric::Capsule -- the 32-byte packed CAPSULE primitive: a start position with
// the radius in the w lane, and a direction with the length in the w lane. Structurally
// identical to CgsGeometric::SweptSphere (same two packed lanes, same member names); the
// two are distinct types because the collision vocabulary keeps a swept sphere and a
// capsule apart (PrimitivePairList::EVolumeType has E_VOLUME_TYPE_CAPSULE == 2 and no
// swept-sphere entry, while the sphere/swept-sphere job descriptors are separate TUs).
//
// ⭐ NEW 2026-08-19 (wave Q6, cluster `addprim`). No CgsGeometric::Capsule existed
// anywhere in b5-decomp/src before this file -- which is the whole reason
// PrimitivePairListBuilder::AddPrimitive(const rw::collision::Volume*, ...) @0x82814AB8
// could not be bodied and stood as a loud conductor gate: its CAPSULE arm needs this
// type. Homed at the DWARF's own mirrored path (references/DecFIGS/dwarfdump/GameShared/
// GameClasses/Geometric/Primitives/CgsCapsule.h:42).
//
// LAYOUT -- DWARF-authoritative (:73/:74), corroborated twice by the X360 asm:
//   +0x00  Vector3Plus mPositionAndRadius    xyz = start position, w = radius
//   +0x10  Vector3Plus mDirectionAndLength   xyz = direction,      w = length
//   sizeof == 32 == 0x20
//
//   1. PrimitivePairListBuilder::AddPrimitive(Capsule*) @0x82814600 allocates 0x20 bytes
//      and copies exactly 32 (four ld/std doubleword pairs at 0/8/0x10/0x18).
//   2. PrimitivePairList::KAU16_VOLUME_SIZES[E_VOLUME_TYPE_CAPSULE] == 32 (rodata
//      word_820DA934, recovered from the image in CgsPrimitivePairList.cpp).
//
// ⚠️ NO METHOD OF THIS CLASS HAS A STANDALONE X360 BODY -- `progress/identity.json` has
// no CgsGeometric::Capsule::* entry at all. That is not "missing"; it is what a
// header-only primitive looks like after inlining. The bodies below are therefore
// reconstructed FROM THE ONE CALL SITE THAT INLINES THEM (the CAPSULE arm of
// AddPrimitive(Volume*) @0x82814B88..0x82814BFC), not invented.
// ============================================================================

namespace CgsGeometric
{
    struct alignas(16) Capsule
    {
        // --------------------------------------------------------------------
        // Set(Vector3, Vector3, VecFloat, VecFloat) -- DWARF CgsCapsule.h:49.
        //
        // Reconstructed from the inlined expansion in AddPrimitive(Volume*)'s CAPSULE
        // arm (X360 0x82814B88..0x82814BFC, raw asm in scratchpad/waveQ6/asm_82814AB8.txt).
        // The compiler emitted the four lane writes as THREE stores, which is exactly the
        // signature of two Vector3Plus read-modify-write pairs with the first pair fused:
        //     0x82814BA0  lvx128 v0,[capsule+0x10]         read old mDirectionAndLength
        //     0x82814BA8  lvx128 v11,[capsule+0x00]        read old mPositionAndRadius
        //     0x82814BB0  lvx128 v10,[transform+0x20]      the direction  (the "at" row)
        //     0x82814BB8  vrlimi128 v10,v0,1,0             keep old w     -> SetVector3
        //     0x82814BBC  lvx128 v9,[transform+0x30]       the position   (the centre row)
        //     0x82814BC0  vrlimi128 v9,v11,1,0             keep old w     -> SetVector3
        //     0x82814BD4  vspltw v13,<volume+0x44>,0       the length     (broadcast)
        //     0x82814BDC  vspltw v12,<volume+0x50>,0       the radius     (broadcast)
        //     0x82814BE0  stvx128 v10,[capsule+0x10]       store 1: direction, old w
        //     0x82814BE8  vrlimi128 v9,v12,1,0             w := radius    -> SetPlus
        //     0x82814BF0  stvx128 v9, [capsule+0x00]       store 2: position + radius
        //     0x82814BEC  vrlimi128 v0,v13,1,0             w := length    -> SetPlus
        //     0x82814BF8  stvx128 v0, [capsule+0x10]       store 3: direction + length
        // (`vrlimi128 vD,vB,1,0` inserts word 3 -- the w lane -- of vB into vD, which is
        //  precisely Vector3Plus::SetVector3 / ::SetPlus. The broadcasts are why the two
        //  scalars are typed VecFloat and why the w lane is the one read.)
        //
        // ⚠️ PARAMETER ORDER: the DWARF gives types only (Vector3, Vector3, VecFloat,
        // VecFloat), no names. The pairing is fixed by the member names -- the position
        // pairs with the radius, the direction with the length -- and by the member order
        // (mPositionAndRadius first). The sibling CgsSweptSphere.h, whose DWARF spells the
        // same four values INTERLEAVED (Vector3, VecFloat, Vector3, VecFloat), is already
        // reconstructed in this directory with exactly this pairing.
        // --------------------------------------------------------------------
        void Set(Vector3 lPosition, Vector3 lDirection, VecFloat lvfRadius, VecFloat lvfLength)
        {
            mPositionAndRadius.SetVector3(lPosition);
            mPositionAndRadius.SetPlus(lvfRadius.w);      // vrlimi128 -> word 3
            mDirectionAndLength.SetVector3(lDirection);
            mDirectionAndLength.SetPlus(lvfLength.w);     // vrlimi128 -> word 3
        }

        // --------------------------------------------------------------------
        // Set(Vector3Plus, Vector3Plus) -- DWARF CgsCapsule.h:54.
        //
        // ⛔ DECLARED ONLY -- NO BODY, here or anywhere in the tree. No call site in the
        // reconstructed tree and no inlined expansion of it has been located in the X360
        // image, so there is nothing to reconstruct it FROM; writing the two obvious
        // assignments would be a guess dressed as a decompile. CALLING IT IS AN LNK2019.
        // --------------------------------------------------------------------
        void Set(Vector3Plus lPositionAndRadius, Vector3Plus lDirectionAndLength);

        // --------------------------------------------------------------------
        // The four unpackers -- DWARF CgsCapsule.h:57/:61/:65/:69. No standalone X360
        // body (header inlines the compiler folds). Each body is FORCED by the member it
        // names: there is exactly one lane of the right kind and nothing to choose. The
        // VecFloat pair broadcasts its scalar to all four lanes, matching the console's
        // vspltw idiom and the committed CgsGeometric::Sphere::GetRadius (CgsSphere.cpp).
        // --------------------------------------------------------------------
        Vector3 GetPosition()  const { return mPositionAndRadius.GetVector3(); }
        Vector3 GetDirection() const { return mDirectionAndLength.GetVector3(); }

        VecFloat GetRadius() const
        {
            const f32 lfRadius = mPositionAndRadius.GetPlus();
            VecFloat lvfResult;
            lvfResult.x = lfRadius; lvfResult.y = lfRadius;
            lvfResult.z = lfRadius; lvfResult.w = lfRadius;
            return lvfResult;
        }

        VecFloat GetLength() const
        {
            const f32 lfLength = mDirectionAndLength.GetPlus();
            VecFloat lvfResult;
            lvfResult.x = lfLength; lvfResult.y = lfLength;
            lvfResult.z = lfLength; lvfResult.w = lfLength;
            return lvfResult;
        }

    private:
        Vector3Plus mPositionAndRadius;    // +0x00 (DWARF :73) xyz = position, w = radius
        Vector3Plus mDirectionAndLength;   // +0x10 (DWARF :74) xyz = direction, w = length
    };
}

#endif // CGS_CAPSULE_H
