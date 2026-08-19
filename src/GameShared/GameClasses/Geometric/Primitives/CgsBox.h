#ifndef CGS_BOX_H
#define CGS_BOX_H

#include <cmath>              // std::fabs -- the `vandc v,v,0x80000000` sign clear in Set's assert

#include "types.hpp"
#include "BrnCommonTypes.h"   // Matrix44Affine, Vector3, Vector3Plus, VecFloat (rw::math::vpu)
#include "rw/math/vpu/vector3_operation.h"    // Dot / Cross / MagnitudeSquared / GetVector3_?Axis
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CgsDev::Assert Begin/Fire/End + KI_MESSAGEBUFFERSIZE
#include "GameShared/GameClasses/Development/CgsStrStream.h" // CgsDev::StrStream (Set's streamed diagnostic)

// ============================================================================
// GameShared/GameClasses/Geometric/Primitives/CgsBox.h
//
// CgsGeometric::Box -- the 80-byte packed ORIENTED box primitive: a 64-byte affine
// frame (right/up/at basis rows + the centre row) followed by one Vector3Plus that
// carries the half-dimensions in xyz and the "fatness" (collision skin) in w.
//
// ⭐ HOMED HERE 2026-08-19 (wave Q6, cluster `addprim`). Until then the ONLY complete
// definition of this type in the tree was a LOCAL FORK inside
// GameShared/GameClasses/SceneManager/Collision/Primitives/CgsPrimitivePairListBuilder.h
// (`struct alignas(16) Box { Vector4 maRows[5]; }`, explicitly labelled "provisional
// layout home"), while five other headers only forward-declared `struct Box;`
// (BrnDeformationManager.h:125, BrnDeformableObject.h:67, BrnPhysicalBodyPart.h:41).
// The fork is DELETED in the same commit as this file lands; the builder header now
// includes this one.
//
// ⭐⭐ 2026-08-19, WAVE Q7 CLUSTER `box` -- THE THREE MISSING BODIES LANDED, AND THE ONE
// DOCUMENTED OMISSION IS GONE. Box::IsValid @0x825BEB80 and both BoxOverlappingTest
// overloads (@0x825BEFA0 and @0x825BF090) now have real bodies, in the new sibling
// GameShared/GameClasses/Geometric/Primitives/CgsBox.cpp; the four .rdata constants that
// blocked IsValid for a wave were BYTE-DUMPED from a private .i64 copy (the values and
// the vperm decode are in CgsBox.cpp's head banner). Set's `bl IsValid` and its streamed
// assert are RESTORED below -- read the ⚠️ MOUNT note on Set before touching anything.
//
// THIS FILE IS THE CONSOLE'S OWN HOME, MEASURED -- not a placement choice. The assert
// inside Box::Set @0x825E6918 passes the file string aGamesharedGame_46 @0x82096CA0,
// dumped verbatim from a private .i64 copy in wave Q6 and re-dumped in wave Q7
// (scratchpad/waveQ6/ida_addprim/out.json, scratchpad/waveQ7/ida_box/out.json):
//     "..\..\..\GameShared\GameClasses\Geometric/Primitives/CgsBox.h"   line 0x5F == 95
// i.e. the BODY of Set is in THIS HEADER, at roughly line 95 of the original. That is
// why Set is defined here as an inline. The DWARF puts the rest of the file's contents in
// the same header (Box at :29, its members at :56/:57, IsValid at :53 with its body around
// :119, GetBoxExtentsAlongAxis at :143, BoxOverlappingTest at :165 and :190) -- so the
// console has no CgsBox.cpp. Ours exists purely so that a 264-instruction IsValid is not
// compiled into all 55 translation units that include this header; see CgsBox.cpp.
// (`progress/identity.json` attributes Box::Set to Development/CgsStrStream.h and both
//  IsValid and BoxOverlappingTest to SDKs/EATech/include/rw/math/vpu/detail/
//  vector3_operation_inline.h. Those are the well-known catch-all buckets -- the
//  attribution follows the INLINED assert-stream / SDK-math code, not the functions. The
//  __FILE__ string and the DWARF outrank them.)
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
//   4. (wave Q7) Both BoxOverlappingTest bodies index the SAME five rows off a bare box
//      pointer -- +0x00/+0x10/+0x20 as the basis, +0x30 as the centre, +0x40 as the
//      dimensions -- with no other offset touched anywhere in their 110 instructions.
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
    // ------------------------------------------------------------------------
    // The validity tolerance Box::IsValid and Box::Set's diagnostic both load:
    // flt_82004014, image bytes 3D CC CC CD == 0.1f. BYTE-DUMPED this wave from a private
    // .i64 copy (scratchpad/waveQ7/ida_box/out.json, `f32_82004014`), and the SAME constant
    // serves BOTH the orthogonality and the normality test -- the console loads it once
    // (`lfs f0, flt_82004014@l`) and broadcasts it into a VecFloat twice (`stfs` to the
    // stack + `lvlx` + `vspltw v4/v5, ..., 0` at 0x825BEDB8/0x825BEDEC).
    // `const` at namespace scope has internal linkage in C++, so this is header-safe.
    // ------------------------------------------------------------------------
    const f32 KF_BOX_VALIDITY_TOLERANCE = 0.1f;

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
        // 269 instructions, 0x825E6918..0x825E6D4C (boundaries MEASURED in wave Q6 on a
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
        // ⭐⭐ THE VALIDATION IS BACK (wave Q7). The banner that stood here said the
        // console's `bl CgsGeometric::Box::IsValid @0x825E6990` and the ~200-instruction
        // diagnostic behind it were OMITTED because IsValid had no body. IsValid now has
        // one (CgsBox.cpp, from the raw asm, with all four of its .rdata constants dumped),
        // so both are restored and the consequence the old banner recorded -- "a degenerate
        // box that the console would have SHOUTED about is accepted silently on this build"
        // -- NO LONGER APPLIES. The assert still has NO effect on the box or on any program
        // state: FireAssert only reports.
        //
        // ⚠️⚠️ MOUNT REQUIREMENT, and it is a HARD one. Because this inline now calls
        // IsValid, and IsValid's definition lives in CgsBox.cpp, that TU MUST be added to
        // tools/build/build_game_exe.bat (beside CgsSphere.cpp, bat:2773) IN THE SAME
        // CHANGE. This header is reached from CgsPrimitivePairListBuilder.cpp, which IS
        // mounted (bat:867), so without the mount line every one of the 55 including TUs
        // takes an LNK2019 on `CgsGeometric::Box::IsValid(void) const`. `cl /c` cannot see
        // that -- the compile gate stays green either way.
        //
        // ---- THE DIAGNOSTIC, DECODED 0x825E69A0..0x825E6D38 ----------------------------
        // The console builds ONE message in CgsDev::Assert::gpcMessageBuffer through a
        // stack StrStream and fires it at CgsBox.h:95. Every literal below was dumped from
        // the image this wave (scratchpad/waveQ7/ida_box/out.json, `str_*` / `ptrstr_*`):
        //     aTransform_1  @0x82096D28  "Transform: "
        //     aDimensions   @0x82096D18  " Dimensions: "
        //     aFatness      @0x82096D0C  " Fatness: "
        //     aOrthogonal   @0x82096CFC  " Orthogonal: "
        //     aNormal_1     @0x82096CF0  " Normal:"          <- NO trailing space; kept
        //     aRighthanded  @0x82096CE0  " Righthanded: "
        //     asc_82001CC4  @0x82001CC4  "\n"
        //     off_82F31958 -> 0x820489E8 "true"   /  off_82F3195C -> 0x82036800 "false"
        //
        // ⚠️ TWO UNAVOIDABLE SPELLING CHANGES, FLAGGED -- the same two, for the same
        //    reason, that PropManager_wQ2_05.cpp:783 and PropManager_wQ2_04.cpp:685 already
        //    record. `bl sub_821F0FC0` is the free
        //    `operator<<(StrStreamBase&, const Matrix44Affine&)` and `bl sub_82203F70` is
        //    the free `operator<<(StrStreamBase&, Vector3)`. NEITHER has a home anywhere in
        //    this tree and this cluster may not add one, so both go through
        //    StrStreamBase::AppendFormat with the console's OWN format strings -- which is
        //    what those callees do internally. Byte-identical output, one call frame fewer.
        //    Replace with `<< lTransform` / `<< lDimensions` the moment they are homed.
        //    The Fatness leg needs NO such change: `bl sub_821F0F40` IS
        //    CgsDev::StrStreamBase::operator<<(f32), which this tree has.
        //
        // ⚠️ THE DIAGNOSTIC RE-DERIVES WHAT ISVALID JUST COMPUTED, and so does the console:
        //    IsValid returns one bit, so the three named sub-verdicts the message prints are
        //    computed a SECOND time at 0x825E69F0..0x825E6BC0 -- the same Gram-matrix
        //    residual, the same MagnitudeSquared-minus-one triple and the same
        //    cross-then-dot sign test, against the same flt_82004014. That duplication is
        //    the console's, not this port's. The three blocks are annotated with their own
        //    instruction ranges below and are the SAME code CgsBox.cpp's IsValid documents
        //    at length; read that file for the SDK names (IsOrthogonal3x3 / IsNormal3x3 /
        //    CgsNumeric::IsRightHanded) and for why they are spelled inline instead of
        //    called.
        // --------------------------------------------------------------------
        void Set(Matrix44Affine lTransform, Vector3 lDimensions, VecFloat lvfFatness)
        {
            mTransform = lTransform;                        // rows 0..3, verbatim
            mDimensionsAndFatness.SetVector3(lDimensions);  // vrlimi128 v0,v13,1,0 -> xyz
            mDimensionsAndFatness.SetPlus(lvfFatness.w);    // vrlimi128 v0,v2,1,0  -> w

            // 0x825E6990 `bl CgsGeometric::Box::IsValid`, then
            // 0x825E6994 `clrlwi r11,r3,24 ; cmplwi cr6,r11,0 ; bne cr6, <epilogue>`.
            if (!IsValid())
            {
                const Vector3& lrRight = lTransform.Right();
                const Vector3& lrUp    = lTransform.Up();
                const Vector3& lrAt    = lTransform.At();

                // 0x825E6B54..0x825E6BB8 -- the ORTHOGONALITY verdict (printed after
                // " Orthogonal: ", read back from var_70 at 0x825E6C44). Transpose +
                // Mult + SelfSubtract(identity) + three MagnitudeSquared + one more, then
                // `vandc` (Abs) and `vcmpgtfp` against the broadcast tolerance.
                const Vector3 lGramErrorRight =
                    Vector3{ rw::math::vpu::Dot(lrRight, lrRight),
                             rw::math::vpu::Dot(lrRight, lrUp),
                             rw::math::vpu::Dot(lrRight, lrAt), 0.0f }
                    - rw::math::vpu::GetVector3_XAxis();
                const Vector3 lGramErrorUp =
                    Vector3{ rw::math::vpu::Dot(lrUp, lrRight),
                             rw::math::vpu::Dot(lrUp, lrUp),
                             rw::math::vpu::Dot(lrUp, lrAt), 0.0f }
                    - rw::math::vpu::GetVector3_YAxis();
                const Vector3 lGramErrorAt =
                    Vector3{ rw::math::vpu::Dot(lrAt, lrRight),
                             rw::math::vpu::Dot(lrAt, lrUp),
                             rw::math::vpu::Dot(lrAt, lrAt), 0.0f }
                    - rw::math::vpu::GetVector3_ZAxis();
                const f32 lfOrthogonalError = rw::math::vpu::MagnitudeSquared(
                    Vector3{ rw::math::vpu::MagnitudeSquared(lGramErrorRight),
                             rw::math::vpu::MagnitudeSquared(lGramErrorUp),
                             rw::math::vpu::MagnitudeSquared(lGramErrorAt), 0.0f });
                const bool lbOrthogonal =
                    !(std::fabs(lfOrthogonalError) > KF_BOX_VALIDITY_TOLERANCE);

                // 0x825E6A30..0x825E6ADC -- the NORMALITY verdict (printed after
                // " Normal:", read back from var_60 at 0x825E6C94).
                const f32 lfNormalError = rw::math::vpu::MagnitudeSquared(
                    Vector3{ rw::math::vpu::MagnitudeSquared(lrRight) - 1.0f,
                             rw::math::vpu::MagnitudeSquared(lrUp)    - 1.0f,
                             rw::math::vpu::MagnitudeSquared(lrAt)    - 1.0f, 0.0f });
                const bool lbNormal =
                    !(std::fabs(lfNormalError) > KF_BOX_VALIDITY_TOLERANCE);

                // 0x825E69F4..0x825E6A40 -- the HANDEDNESS verdict (printed after
                // " Righthanded: ", held in r26 to 0x825E6CDC). Cross(right, up) via the
                // two `vpermwi128 ..,0x63` YZXW swizzles + `vmulfp128` + `vnmsubfp`, then
                // `vmsum3fp128` with the at row and `vcmpgefp` against zero.
                const bool lbRightHanded =
                    rw::math::vpu::Dot(rw::math::vpu::Cross(lrRight, lrUp), lrAt) >= 0.0f;

                // 0x825E69A0 BeginAssert is called BEFORE the message is built on the
                // console (it takes the assert mutex, then StrStream writes into the
                // shared gpcMessageBuffer). This port builds into a stack buffer, the
                // same shape CgsID.cpp / PropManager_wQ2_05.cpp already use.
                // ⚠️⚠️ FLAG PC-platform guard byte -- ONE spare byte, and a REAL DEFECT REPORT.
                // CgsDev::StrStream::Append (CgsStrStream.cpp:138-150) calls
                //     strncat(mpcBuffer, lpcText, miBufferSize - strlen(mpcBuffer))
                // and its banner records that the console passes exactly that count ("NOT
                // miBufferSize-1; the console passes the full remaining count"). strncat writes up
                // to `n` characters PLUS a terminating NUL, so a message that fills the buffer
                // writes byte [miBufferSize] -- ONE PAST THE END. On the console the destination is
                // the 256-byte GLOBAL gpcMessageBuffer (byte_83018150), so that byte lands in the
                // next global and nothing notices. Here it is a STACK array under /GS, and the
                // stack cookie check turns it into an instant STATUS_STACK_BUFFER_OVERRUN
                // (0xC0000409) -- MEASURED: this probe crashed on it before the +1 was added, and
                // THIS assert is the first caller in the tree whose message is long enough to
                // reach the cap (~310 characters of matrix + dimensions + fatness + three
                // verdicts, against 256).
                // The array is one byte longer than the size handed to the stream, so the
                // one-past-the-end write lands in the spare byte and the VISIBLE message is
                // byte-identical to the console's (same 256-byte cap, same truncation point).
                // THE TRADE, STATED HONESTLY rather than sold as a no-op: `luRoom - 1` in
                // CgsStrStream.cpp's Append would end the overrun, but it is NOT
                // behaviour-preserving -- the 256th byte is a real character on the console, so
                // every MAXED-OUT message would come out one character shorter. Harmless in
                // practice (CgsAssertManager strncpy's only KI_MESSAGEBUFFERSIZE-1 downstream
                // anyway, so that character never reaches a human), but it is a change, not a
                // no-op. The BYTE-EXACT alternative is to keep the count and give every caller a
                // KI_MESSAGEBUFFERSIZE+1 array -- which is exactly what this site does below.
                // Whichever way that shared file eventually goes, this +1 is the correct local
                // answer today; it is also the only one this cluster can make without editing a
                // file it does not own.
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE + 1];
                CgsDev::StrStream lStrStream(lacMessageBuffer,
                                             CgsDev::Assert::KI_MESSAGEBUFFERSIZE);

                lStrStream << "Transform: ";
                lStrStream.AppendFormat(   // sub_821F0FC0's own format string
                    "\n(%f, %f, %f, %f)\n(%f, %f, %f, %f)\n(%f, %f, %f, %f)\n(%f, %f, %f, %f)\n",
                    lTransform.xAxis.x, lTransform.xAxis.y, lTransform.xAxis.z, lTransform.xAxis.w,
                    lTransform.yAxis.x, lTransform.yAxis.y, lTransform.yAxis.z, lTransform.yAxis.w,
                    lTransform.zAxis.x, lTransform.zAxis.y, lTransform.zAxis.z, lTransform.zAxis.w,
                    lTransform.wAxis.x, lTransform.wAxis.y, lTransform.wAxis.z, lTransform.wAxis.w);
                lStrStream << " Dimensions: ";
                lStrStream.AppendFormat(   // sub_82203F70's own format string
                    "(%f, %f, %f)", lDimensions.x, lDimensions.y, lDimensions.z);
                lStrStream << " Fatness: ";
                // 0x825E6C20 `lfs f1, arg_30(r1)` reads lane 0 of the spilled VecFloat
                // while the STORE above takes lane 3; identical value, a VecFloat is one
                // float in every lane. Spelled .w here to match the store.
                lStrStream << lvfFatness.w;                       // sub_821F0F40
                lStrStream << " Orthogonal: ";
                lStrStream << (lbOrthogonal  ? "true" : "false"); // off_82F31958/off_82F3195C
                lStrStream << " Normal:";
                lStrStream << (lbNormal      ? "true" : "false");
                lStrStream << " Righthanded: ";
                lStrStream << (lbRightHanded ? "true" : "false");
                lStrStream << "\n";

                CgsDev::Assert::BeginAssert();
                // console: FireAssert(gpcMessageBuffer,
                //     "..\\..\\..\\GameShared\\GameClasses\\Geometric/Primitives/CgsBox.h", 95)
                CgsDev::Assert::FireAssert(lacMessageBuffer, __FILE__, __LINE__);
                CgsDev::Assert::EndAssert();
            }
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
        // IsValid @0x825BEB80 (264 instructions, 0x825BEB80..0x825BEF98) -- DWARF
        // CgsBox.h:53, body around :119 (DWARF local `bool lbIsValid`).
        //
        // ⭐ BODIED 2026-08-19 (wave Q7, cluster `box`) in CgsBox.cpp. Nine predicates:
        // a per-lane NaN screen over all five rows, orthogonality and normality of the
        // 3x3 against flt_82004014 == 0.1f, and a right-handedness sign test. The four
        // .rdata constants that made this a park last wave (unk_82CDA350, unk_82181510,
        // unk_82181520, rw::math::vpu::detail::gIVector) were all byte-dumped; see
        // CgsBox.cpp for the bytes, the vperm decode and the SDK helper names.
        // Its ONLY caller in the whole image is Box::Set (xrefs MEASURED: exactly one,
        // 0x825E6990) -- which is why the mount note on Set matters.
        // --------------------------------------------------------------------
        bool IsValid() const;

    private:
        Matrix44Affine mTransform;             // +0x00 (DWARF :56) right/up/at/centre rows
        Vector3Plus    mDimensionsAndFatness;  // +0x40 (DWARF :57) xyz = half-dims, w = fatness
    };

    // ========================================================================
    // The three free OBB-overlap functions the DWARF declares in this header --
    // references/DecFIGS/dwarfdump/GameShared/GameClasses/Geometric/Primitives/CgsBox.h
    // :62/:65/:68, verbatim:
    //     extern VecFloat   GetBoxExtentsAlongAxis(const Box &, Vector3);      // :143
    //     extern MaskScalar BoxOverlappingTest(const Box &, const Box &, Vector3);  // :165
    //     extern MaskScalar BoxOverlappingTest(const Box &, const Box &);      // :190
    // (The same DWARF also gives `typedef const Box & BoxArg;` at CgsBox.h:60, which is
    //  how the intersection headers spell the parameter; it is not needed here.)
    //
    // All three are BODIED in CgsBox.cpp. MaskScalar is spelled fully qualified because
    // BrnCommonTypes.h aliases Vector3/Vector4/VecFloat but not MaskScalar; the type
    // itself is the committed vendor one (rw/math/vpu/types.h:115).
    //
    // ⭐ THE TWO-ARGUMENT OVERLOAD IS `sub_825BF090` IN THE EXPORT -- unnamed there and
    // absent from progress/identity.json entirely. It is named from the DWARF; the match
    // is nailed by its callee set (six calls to the three-argument overload and nothing
    // else), its argument registers (r3/r4, no VMX argument) and its only REAL caller,
    // BrnPhysics::Vehicle::VehicleManager::EndVehicleContactGeneration @0x8261AC38, whose
    // own DWARF names CgsGeometric::BoxOverlappingTest.
    // ⚠️ "only REAL caller": the dump lists TWO xrefs to 0x825BF090 -- 0x8261AFF4 (inside
    // EndVehicleContactGeneration) and 0x821CE540, which is a LINKER BRANCH ISLAND rather
    // than a caller. Every function in that dump carries one companion xref in 0x821B..0x821D
    // (Box::Set -> 0x821CEBA0, EndVehicleContactGeneration -> 0x821CF2D8, sub_821F0FC0 ->
    // 0x821BB070), and none of those addresses has a per-address export of its own.
    //
    // ⚠️ FOR THE `carcar` OWNER: the banner in BrnVehicleManagerContactGeneration.cpp that
    // said "CgsGeometric::Box / Box::Set @0x825E6918 / BoxOverlappingTest (PS3 0x7A7B18) are
    // also absent from the tree; reconstruct them WITH this tail" is corrected. All three now
    // exist, so the HIDE_ONLINE unhide tail's remaining blocker is the network path itself,
    // not these primitives.
    // ========================================================================
    VecFloat GetBoxExtentsAlongAxis(const Box& lrBox, Vector3 lAxis);

    rw::math::vpu::MaskScalar BoxOverlappingTest(const Box& lrBox1, const Box& lrBox2,
                                                 Vector3 lAxis);

    rw::math::vpu::MaskScalar BoxOverlappingTest(const Box& lrBox1, const Box& lrBox2);
}

#endif // CGS_BOX_H
