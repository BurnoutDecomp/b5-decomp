#ifndef CGS_BOX_H
#define CGS_BOX_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Matrix44Affine, Vector3, Vector3Plus, VecFloat (rw::math::vpu)

// ============================================================================
// GameShared/GameClasses/Geometric/Primitives/CgsBox.h
//
// CgsGeometric::Box -- the 80-byte packed ORIENTED box primitive: a 64-byte affine
// frame (right/up/at basis rows + the centre row) followed by one Vector3Plus that
// carries the half-dimensions in xyz and the "fatness" (collision skin) in w.
//
// ⭐ HOMED HERE 2026-08-19 (wave Q6, cluster `addprim`). Until now the ONLY complete
// definition of this type in the tree was a LOCAL FORK inside
// GameShared/GameClasses/SceneManager/Collision/Primitives/CgsPrimitivePairListBuilder.h
// (`struct alignas(16) Box { Vector4 maRows[5]; }`, explicitly labelled "provisional
// layout home"), while five other headers only forward-declared `struct Box;`
// (BrnDeformationManager.h:125, BrnDeformableObject.h:67, BrnPhysicalBodyPart.h:41).
// The fork is DELETED in the same commit as this file lands; the builder header now
// includes this one.
//
// THIS FILE IS THE CONSOLE'S OWN HOME, MEASURED -- not a placement choice. The assert
// inside Box::Set @0x825E6918 passes the file string aGamesharedGame_46 @0x82096CA0,
// dumped verbatim from a private .i64 copy this wave
// (scratchpad/waveQ6/ida_addprim/out.json):
//     "..\..\..\GameShared\GameClasses\Geometric/Primitives/CgsBox.h"   line 0x5F == 95
// i.e. the BODY of Set is in THIS HEADER, at roughly line 95 of the original. That is
// why Set is defined here as an inline rather than in a CgsBox.cpp -- there is no
// CgsBox.cpp on the console, and homing it here means NO new translation unit and NO
// new mount line in tools/build/build_game_exe.bat.
// (`progress/identity.json` attributes Box::Set to Development/CgsStrStream.h. That is
//  the well-known catch-all bucket -- the attribution follows the INLINED assert-stream
//  code, not the function. The __FILE__ string above outranks it.)
//
// LAYOUT -- DWARF-authoritative (references/DecFIGS/dwarfdump/GameShared/GameClasses/
// Geometric/Primitives/CgsBox.h:29, members at :56/:57), corroborated three ways by the
// X360 asm:
//   +0x00  Matrix44Affine mTransform             (rows: right/up/at/centre)
//   +0x40  Vector3Plus    mDimensionsAndFatness  (xyz = half-dimensions, w = fatness)
//   sizeof == 80 == 0x50
//
//   1. PrimitivePairListBuilder::AddPrimitive(Box*) @0x82814570 allocates 0x50 bytes and
//      copies FIVE 16-byte rows (lvx128/stvx128 at 0/0x10/0x20/0x30/0x40).
//   2. PrimitivePairList::KAU16_VOLUME_SIZES[E_VOLUME_TYPE_BOX] == 80 (rodata
//      word_820DA934, already recovered from the image in CgsPrimitivePairList.cpp).
//   3. Box::Set @0x825E6918 stores the caller's four transform rows to +0x00..+0x30 and
//      then read-modify-writes ONLY the +0x40 row twice (xyz from the Vector3, then the
//      w lane from the VecFloat) -- the Vector3Plus SetVector3/SetPlus pair.
//
// ⚠️ TWO COMMITTED FILES STILL MODEL Box::Set BY RAW OFFSET AND SHOULD BE DE-DUPLICATED
// AGAINST THIS HEADER BY THEIR OWNERS (reported, NOT edited by this cluster -- they are
// out of its ownership):
//     GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject_BBox.cpp
//         :269-274 (GetBoundingBox) and :331-336 (GetAlignedDeformedBoundingBox)
//     GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPart.cpp
//         (GetBoundingBox, the precedent those two cite)
// Both write `*(Vector3*)(lpBox + 0/16/32/48/64)` through a char* and both carry a
// "FLAG: Box layout provisional" note. The layout they guessed is CORRECT -- this header
// confirms it -- so the de-duplication is a mechanical swap to
// `lpBoxOut->Set(lTransform, lvDims, lvfFatness)`, not a behaviour change.
// ============================================================================

namespace CgsGeometric
{
    struct alignas(16) Box
    {
        // --------------------------------------------------------------------
        // Set(Matrix44Affine, Vector3Plus) -- DWARF CgsBox.h:35.
        //
        // ⛔ DECLARED ONLY -- NO BODY HERE AND NO BODY ANYWHERE IN THE TREE.
        // The X360 image exports exactly ONE Box::Set (0x825E6918) and its argument
        // registers are r4 = the Matrix44Affine, v1 = a Vector3, v2 = a VecFloat, i.e.
        // it is the THREE-argument overload below. This two-argument sibling is either
        // never instantiated in the ARTIST build or folded into a caller; nothing in the
        // reconstructed tree calls it. Declared so the DWARF's declaration set stays
        // honest -- calling it is an LNK2019.
        // --------------------------------------------------------------------
        void Set(Matrix44Affine lTransform, Vector3Plus lDimensionsAndFatness);

        // --------------------------------------------------------------------
        // Set(Matrix44Affine, Vector3, VecFloat) @0x825E6918 -- DWARF CgsBox.h:41.
        // 269 instructions, 0x825E6918..0x825E6D4C (boundaries MEASURED this wave on a
        // private .i64 copy, scratchpad/waveQ6/ida_addprim/out.json `leaves`).
        //
        // REGISTER MAP, read off the prologue (raw `assembly`,
        // .ida-exports/BURNOUT_X360_ARTIST.XEX/0x825E6918.json):
        //     r3 = this                       (`mr r30, r3`   @0x825E692C)
        //     r4 = the Matrix44Affine         (`mr r28, r4`   @0x825E6934; a 64-byte
        //          by-value struct rides a hidden pointer on this ABI, and the body
        //          reads it as four rows: lvx128 r28+0/0x10/0x20/0x30)
        //     v1 = the Vector3 dimensions     (`vmr128 v127, v1` @0x825E6930 -- saved
        //          across the assert call, then `vmr128 v0, v127` @0x825E6964)
        //     v2 = the VecFloat fatness       (spilled to the stack @0x825E6958 so the
        //          assert's message printer can re-read it as a scalar, 0x825E6C20)
        //
        // THE SEVEN STORES, verbatim:
        //     0x825E694C/54  lvx128 v0,[r28+0x00]  ; stvx128 v0,[r30+0x00]   row 0
        //     0x825E6960/68  lvx128 v13,[r28+0x10] ; stvx128 v13,[r30+0x10]  row 1
        //     0x825E696C/70  lvx128 v13,[r28+0x20] ; stvx128 v13,[r30+0x20]  row 2
        //     0x825E6974/78  lvx128 v13,[r28+0x30] ; stvx128 v13,[r30+0x30]  row 3
        //     0x825E697C     lvx128 v13,[r30+0x40]                 (read-modify-write)
        //     0x825E6980/84  vrlimi128 v0,v13,1,0 ; stvx128 v0,[r30+0x40]
        //                       -> +0x40 = {dims.xyz, OLD w}   == SetVector3(dims)
        //     0x825E6988/8C  vrlimi128 v0,v2,1,0  ; stvx128 v0,[r30+0x40]
        //                       -> +0x40 = {dims.xyz, fatness} == SetPlus(fatness)
        // `vrlimi128 vD,vB,1,0` inserts WORD 3 (the w lane) of vB into vD, which is why
        // the fatness is taken from the VecFloat's w lane below. A VecFloat is one float
        // broadcast to all four lanes, so any lane would do; w is the one the asm reads.
        //
        // ⛔⛔ ONE THING THE CONSOLE DOES AND THIS BODY DOES NOT -- READ THIS BEFORE
        // BELIEVING A BOX IS VALIDATED. After the seven stores the console calls
        //     bl CgsGeometric::Box::IsValid   @0x825E6990
        // and, when it returns false, runs a ~200-instruction DEBUG-ONLY assert that
        // formats "Transform: <m> Dimensions: <v> Fatness: <f> Orthogonal: <b>
        // Normal:<b> Righthanded: <b>\n" into the assert message buffer through the
        // CgsDev StrStream and fires FireAssert(buffer, "..\\..\\..\\GameShared\\
        // GameClasses\\Geometric/Primitives/CgsBox.h", 95). It has NO effect on the
        // box or on any program state -- FireAssert only reports.
        //
        // CgsGeometric::Box::IsValid @0x825BEB80 is **264 instructions** (0x825BEB80..
        // 0x825BEF98, measured; raw dump kept at scratchpad/waveQ6/asm_825BEB80.txt) of
        // dense VMX: a per-lane NaN screen over all five rows (vcmpeqfp x,x), then a
        // basis orthogonality + normality test against tolerance flt_82004014
        // (image bytes 3DCCCCCD == 0.1f, re-dumped this wave) using three un-recovered
        // rodata vectors (unk_82CDA350, unk_82181510, unk_82181520) and
        // rw::math::vpu::detail::gIVector, and finally a right-handedness sign test.
        // IT HAS NO BODY ANYWHERE IN THE TREE and its only caller in the whole image is
        // this function (xrefs MEASURED this wave: exactly one, 0x825E6990).
        // Reconstructing it is its own cluster; guessing the permute semantics of those
        // four un-dumped constants would be fabrication.
        //
        // SO: the call is OMITTED, not stubbed, and the omission is recorded here rather
        // than hidden. It is NOT called with a trap/loud body either, because a
        // referenced-but-bodyless callee is an LNK2019 in a mounted TU (the build's own
        // doctrine, build_game_exe.bat:544) and Box::Set is reached from
        // CgsPrimitivePairListBuilder.cpp, which IS mounted (bat:867).
        // CONSEQUENCE, stated plainly: a degenerate box (NaN lane, non-orthonormal or
        // left-handed basis) that the console would have SHOUTED about is accepted
        // silently on this build. Nothing else changes.
        // --------------------------------------------------------------------
        void Set(Matrix44Affine lTransform, Vector3 lDimensions, VecFloat lvfFatness)
        {
            mTransform = lTransform;                        // rows 0..3, verbatim
            mDimensionsAndFatness.SetVector3(lDimensions);  // vrlimi128 v0,v13,1,0 -> xyz
            mDimensionsAndFatness.SetPlus(lvfFatness.w);    // vrlimi128 v0,v2,1,0  -> w

            // ⛔ OMITTED (see the banner): CGS_ASSERT(IsValid(), <streamed diagnostic>)
            //    -- CgsGeometric::Box::IsValid @0x825BEB80 (264) has no body in the tree.
        }

        // --------------------------------------------------------------------
        // Trivial accessors -- DWARF CgsBox.h:44/:47/:50. None has a standalone X360
        // body (the compiler folds them at every call site), so their bodies live here,
        // and each is FORCED by the member it names: there is exactly one member of the
        // right type and nothing to choose. Same treatment the tree already gives
        // CgsGeometric::Sphere::GetPosition / ::GetRadius (CgsSphere.cpp).
        // --------------------------------------------------------------------
        Matrix44Affine GetTransform() const { return mTransform; }

        Vector3 GetDimensions() const { return mDimensionsAndFatness.GetVector3(); }

        VecFloat GetFatness() const
        {
            // A VecFloat is one scalar in every lane (the console's vspltw); the scalar
            // is the +0x40 row's w lane.
            const f32 lfFatness = mDimensionsAndFatness.GetPlus();
            VecFloat lvfResult;
            lvfResult.x = lfFatness;
            lvfResult.y = lfFatness;
            lvfResult.z = lfFatness;
            lvfResult.w = lfFatness;
            return lvfResult;
        }

        // --------------------------------------------------------------------
        // IsValid @0x825BEB80 -- DWARF CgsBox.h:53.
        //
        // ⛔ DECLARED ONLY -- NO BODY ANYWHERE IN THE TREE, and this is a REPORTED,
        // SIZED park, not an oversight: 264 instructions of VMX plus four un-dumped
        // rodata constants (see the long note on Set above). CALLING IT IS AN LNK2019.
        // Its one console caller (Box::Set) deliberately does not call it here.
        // --------------------------------------------------------------------
        bool IsValid() const;

    private:
        Matrix44Affine mTransform;             // +0x00 (DWARF :56) right/up/at/centre rows
        Vector3Plus    mDimensionsAndFatness;  // +0x40 (DWARF :57) xyz = half-dims, w = fatness
    };
}

#endif // CGS_BOX_H
