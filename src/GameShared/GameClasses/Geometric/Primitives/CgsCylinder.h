#ifndef CGS_CYLINDER_H
#define CGS_CYLINDER_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Matrix44Affine, Vector3, VecFloat (rw::math::vpu)

// ============================================================================
// GameShared/GameClasses/Geometric/Primitives/CgsCylinder.h
//
// CgsGeometric::Cylinder -- the 80-byte packed CYLINDER primitive: a 64-byte affine
// frame plus two scalars (radius, length). Unlike Box, whose trailing row packs the
// half-dimensions AND the fatness into one Vector3Plus, the cylinder's two scalars are
// plain float32_t stored back to back at +0x40/+0x44, leaving 8 bytes of tail padding
// inside the 16-byte-aligned 80-byte record.
//
// ⭐ NEW 2026-08-19 (wave Q6, cluster `addprim`). No CgsGeometric::Cylinder existed
// anywhere in b5-decomp/src before this file -- one of the two missing types that kept
// PrimitivePairListBuilder::AddPrimitive(const rw::collision::Volume*, ...) @0x82814AB8
// a loud conductor gate. Homed at the DWARF's own mirrored path
// (references/DecFIGS/dwarfdump/GameShared/GameClasses/Geometric/Primitives/
// CgsCylinder.h:42).
//
// LAYOUT -- DWARF-authoritative (:71/:72/:73), corroborated twice by the X360 asm:
//   +0x00  Matrix44Affine mTransform   right/up/at/centre rows
//   +0x40  float32_t      mfRadius
//   +0x44  float32_t      mfLength
//   sizeof == 80 == 0x50 (16-byte aligned; +0x48..+0x4F is tail padding)
//
//   1. PrimitivePairListBuilder::AddPrimitive(Cylinder*) @0x82814678 allocates 0x50
//      bytes and copies FOUR 16-byte rows (0/0x10/0x20/0x30) plus TWO f32 (lfs/stfs at
//      0x40 and 0x44) -- and nothing else. The 8 padding bytes are deliberately not
//      copied, which is itself the proof that the record ends at +0x48 of live data.
//   2. PrimitivePairList::KAU16_VOLUME_SIZES[E_VOLUME_TYPE_CYLINDER] == 80 (rodata
//      word_820DA934, recovered from the image in CgsPrimitivePairList.cpp).
//
// ⚠️ NO METHOD OF THIS CLASS HAS A STANDALONE X360 BODY -- `progress/identity.json` has
// no CgsGeometric::Cylinder::* entry at all: it is a header-only primitive whose methods
// the compiler folds. Set below is reconstructed FROM THE ONE CALL SITE THAT INLINES IT
// (the CYLINDER arm of AddPrimitive(Volume*) @0x82814C48..0x82814CA4), not invented.
// ============================================================================

namespace CgsGeometric
{
    struct alignas(16) Cylinder
    {
        // --------------------------------------------------------------------
        // Set(Matrix44Affine, float32_t, float32_t) -- DWARF CgsCylinder.h:48.
        //
        // Reconstructed from the inlined expansion in AddPrimitive(Volume*)'s CYLINDER
        // arm (X360 0x82814C48..0x82814CA4, raw asm in scratchpad/waveQ6/asm_82814AB8.txt),
        // where `var_70` is the stack Cylinder and r5 is the caller's transform:
        //     0x82814C48/50  lfs f0,[volume+0x50] ; stfs f0,[cyl+0x40]   mfRadius
        //     0x82814C58/68  lfs f0,[volume+0x44] ; stfs f0,[cyl+0x44]   mfLength
        //     0x82814C60/80  lvx128 v0,[xform+0x00] ; stvx128 v0,[cyl+0x00]   row 0
        //     0x82814C88/8C  lvx128 v0,[xform+0x10] ; stvx128 v0,[cyl+0x10]   row 1
        //     0x82814C78/98  lvx128 v13,[xform+0x20]; stvx128 v13,[cyl+0x20]  row 2
        //     0x82814C94/A0  lvx128 v0,[xform+0x30] ; stvx128 v0,[cyl+0x30]   row 3
        // Four rows and two scalars -- no read-modify-write anywhere, which is why the
        // two scalars are plain float32_t here and Vector3Plus lanes in Box/Capsule.
        //
        // ⚠️ PARAMETER ORDER: the DWARF gives types only (Matrix44Affine, float32_t,
        // float32_t), no names. RADIUS FIRST is fixed by the member order (mfRadius at
        // +0x40 precedes mfLength at +0x44) and by the store order at the one call site.
        // --------------------------------------------------------------------
        void Set(Matrix44Affine lTransform, f32 lfRadius, f32 lfLength)
        {
            mTransform = lTransform;   // rows 0..3, verbatim
            mfRadius   = lfRadius;     // +0x40
            mfLength   = lfLength;     // +0x44
        }

        // --------------------------------------------------------------------
        // The unpackers -- DWARF CgsCylinder.h:51/:55/:59/:63/:67. No standalone X360
        // body (header inlines). The four defined below are FORCED by the member they
        // name plus this tree's committed Matrix44Affine row vocabulary
        // (vendor/renderware/include/rw/math/vpu/types.h: Right/Up/At/Pos == xAxis/
        // yAxis/zAxis/wAxis, where wAxis IS the translation row).
        // --------------------------------------------------------------------
        Matrix44Affine GetTransform() const { return mTransform; }

        Vector3 GetPosition() const { return mTransform.Pos(); }   // the wAxis row

        VecFloat GetRadius() const
        {
            VecFloat lvfResult;
            lvfResult.x = mfRadius; lvfResult.y = mfRadius;
            lvfResult.z = mfRadius; lvfResult.w = mfRadius;
            return lvfResult;
        }

        VecFloat GetLength() const
        {
            VecFloat lvfResult;
            lvfResult.x = mfLength; lvfResult.y = mfLength;
            lvfResult.z = mfLength; lvfResult.w = mfLength;
            return lvfResult;
        }

        // --------------------------------------------------------------------
        // GetDirection -- DWARF CgsCylinder.h:55.
        //
        // ⛔ DECLARED ONLY -- NO BODY, here or anywhere in the tree, AND THAT IS
        // DELIBERATE. Unlike every other accessor on this class it is NOT forced by the
        // layout: the cylinder's axis is one of the three basis rows of mTransform and
        // NOTHING MEASURED SAYS WHICH. The one call site that builds a Cylinder
        // (AddPrimitive(Volume*)'s CYLINDER arm) copies the whole frame verbatim and
        // never asks for an axis, and no other producer or consumer of this type exists
        // in the tree yet. The sibling Capsule takes its direction from the "at" row
        // (mTransform.zAxis) at ITS call site, which makes zAxis the likely answer -- but
        // "likely" is a guess, and a wrong axis here would silently mis-orient every
        // cylinder-vs-world test. Recover it from a real consumer, then define it.
        // CALLING IT IS AN LNK2019.
        // --------------------------------------------------------------------
        Vector3 GetDirection() const;

    private:
        Matrix44Affine mTransform;   // +0x00 (DWARF :71) right/up/at/centre rows
        f32            mfRadius;     // +0x40 (DWARF :72)
        f32            mfLength;     // +0x44 (DWARF :73)
    };
}

#endif // CGS_CYLINDER_H
