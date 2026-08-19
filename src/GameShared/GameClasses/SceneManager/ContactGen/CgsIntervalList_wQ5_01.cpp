// ============================================================================
// CgsSceneManager::IntervalList::AddObject( u16, Vector3, Vector3 )
//   Home: GameShared/GameClasses/SceneManager/ContactGen/CgsIntervalList.cpp
//   Partfile: CgsIntervalList_wQ5_01.cpp   (wave Q5 round 2, cluster "sweeper (i)")
//
// X360: sub_828B49A0 @ 0x828B49A0, 348 instructions.
//
// ---------------------------------------------------------------------------
// WHY THIS IS `IntervalList::AddObject`, AND WHY IT LIVES IN CgsIntervalList.cpp
// (the symbol is UNNAMED in the IDB -- `sub_828B49A0` -- so ownership is derived,
//  and every leg of the derivation is measured, not assumed):
//
//  1. IT IS A MEMBER OF IntervalList. r3 is dereferenced at +0x08 and +0x0C only
//     (0x828B49D0 `lwz r10,0xC(r31)` == muMaxNumIntervals, 0x828B49DC `lwz r11,8(r31)`
//     == muNumIntervals), and at +0x00 / +0x04 for the two backing arrays
//     (0x828B4D90 `lwz r11,0(r31)` == mpaIntervals, 0x828B4EEC `lwz r11,4(r31)` ==
//     mpaObjectToIntervalMap). That is exactly the four-field IntervalList and
//     nothing else -- see the offset table in CgsIntervalList.h.
//  2. ITS ASSERTS BAKE THIS FILE. Every FireAssert in the body passes the string at
//     0x820F35B0 as its `file` argument; dumped verbatim by headless idat on a private
//     .i64 copy it reads
//       "d:\p4\b5_main\burnout\main\code\gameshared\gameclasses\scenemanager\ContactGen/CgsIntervalList.cpp"
//     -- byte-identical to the file string used by IntervalList::Prepare @0x828B4658 and
//     by the sibling overload IntervalList::AddObject @0x828B46E8.
//  3. ITS .pdata ROW SITS BETWEEN TWO NAMED IntervalList METHODS. The exception table at
//     0x821DC490 lists, in address order:
//       IntervalList::Prepare | IntervalList::AddObject | sub_828B49A0 |
//       IntervalList::UpdateObject | IntervalList::AddSentinelInterval
//     so this body is emitted inside the CgsIntervalList.cpp object, between two of its
//     methods, not in a sweeper object.
//  4. ITS SOURCE LINES INTERLEAVE CORRECTLY. The sibling overload's asserts bake lines
//     162/164/167/168/169; this body's bake 202/204/207/208/209/210/211; UpdateObject's
//     are later still. One file, in order.
//  5. THE DWARF DECLARES EXACTLY THIS SECOND OVERLOAD (CgsIntervalList.h:108,
//     `AddObject(uint16_t, Vector3, Vector3)`), it had no body anywhere in the tree, and
//     the two X360 callers -- SceneSweeper::AddObject @0x828B5958 and
//     SceneSweeper::UpdateObject @0x828B6160 -- are precisely the two the DWARF's call
//     graph names for it.
//
// ---------------------------------------------------------------------------
// PARAMETER ABI. The two Vector3s travel in VECTOR registers, not GPRs: the caller loads
// `lvx128 v1, r0, <box+0x00>` and `lvx128 v2, r0, <box+0x10>` (SceneSweeper::AddObject
// 0x828B5CF8/0x828B5D00), and this body's prologue spills v1 -> arg_20 and v2 -> arg_30
// (0x828B49C8/0x828B49D8). The u16 object index rides r4 alone -- the vector arguments do
// NOT consume GPR slots (the PPC vector analogue of the float-skips-a-GPR rule). So the
// argument order really is (index, min, max), matching the DWARF declaration.
//
// LEDGER NOTE (measured, gotcha 10): progress/ledger marks `IntervalList::AddObject`
// `reviewed`. Only the `(u16, const Interval&, const Interval&)` overload had a body; this
// one had none. The row was true of a different function with the same name.
// ============================================================================

#include "GameShared/GameClasses/SceneManager/ContactGen/CgsIntervalList.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/math/fpu/scalar_operation.h"     // rw::math::fpu::IsZero (the box-extent tests)
#include "rw/math/vpu/vector3_operation.h"    // rw::math::vpu::IsValid (Vector3 NaN test)

namespace CgsSceneManager
{
    // ------------------------------------------------------------------------
    // CgsIntervalList.cpp:196-215 @ 0x828B49A0 (348 insns, store-for-store)
    //
    // The interval-record BUILDER: the overload that takes the world box's two corner rows
    // and manufactures the two Interval endpoint records itself, rather than being handed
    // them ready-made like the `(u16, const Interval&, const Interval&)` sibling above it in
    // the file. Both console callers (SceneSweeper::AddObject / ::UpdateObject) use this one,
    // so it is the function that actually puts a car -- or a smash gate -- into the sweep.
    //
    // Two slots are consumed, min-role then max-role, and the SHARED axis bounds are taken
    // from BOTH corners: every field except mfXInterval is identical in the two records
    // (that is what makes the sweep single-axis with per-pair Y/Z rejection). The negations
    // are the console's `vxor` against the 0x80000000 lane mask built by
    // `vspltisw128 v127,-1 ; vslw128 vX,v127,v127` (shift each word left by 31) --
    // a sign flip, not an arithmetic negate of a different quantity:
    //
    //   0x828B4DC0  sth  r20, 0x14(min)   ; mu16ObjectIndex     = luObjectIndex
    //   0x828B4DF4  stfs f0,  0x00(min)   ; mfXInterval         =  lMin.x
    //   0x828B4DF8  stfs f13, 0x0C(min)   ; mfYMinInterval      =  lMin.y
    //   0x828B4E0C  stfs f12, 0x04(min)   ; mfZMinInterval      =  lMin.z
    //   0x828B4E1C  stfs f0,  0x10(min)   ; mfMinusYMaxInterval = -lMax.y   (vspltw 1 + vxor)
    //   0x828B4E3C  stfs f0,  0x08(min)   ; mfMinusZMaxInterval = -lMax.z   (vspltw 2 + vxor)
    //   0x828B4E4C  sth  r23(=0), 0x16(min) ; mu16Flags         = 0   -> min-role endpoint
    //   0x828B4E58  sth  r20, 0x14(max)   ; ... the same record again, except
    //   0x828B4E8C  stfs f0,  0x00(max)   ; mfXInterval         =  lMax.x
    //   0x828B4EE8  sth  r7(=1), 0x16(max) ; mu16Flags          = 1   -> max-role endpoint
    //   0x828B4EF0  sthx r29, map+idx*4   ; map[idx][KI_MIN_INDEX] = luNewObjectMin
    //   0x828B4EFC  sth  r24, 2(map+idx*4); map[idx][KI_MAX_INDEX] = luNewObjectMax
    //
    // ⭐ THE ZERO-EXTENT EPSILON IS NOT A GUESS. The three degenerate-box tests are spelled
    // in VMX as `|lMax.<axis> - lMin.<axis>| > eps` -- `vsubfp` then `vandc` against the same
    // 0x80000000 lane mask (an absolute value) then `vcmpgtfp` against lane 0 of the .rdata
    // constant at 0x820F25D0. That constant reads as 32 opaque bytes in the export
    // (`unk_820F25D0`), which is exactly the shape of the "unrecoverable zero" trap; dumped
    // for real it is { 1.1920928955078125e-07, 0.1, 2.0, ... } and lane 0 is FLT_EPSILON --
    // BIT-IDENTICAL to rw::math::fpu::KF_IS_ZERO_TOLERANCE as this tree already committed it.
    // So `!rw::math::fpu::IsZero(diff)` below is the console's own predicate, not an
    // approximation of it, and it is spelled the same way as the sibling overload's three
    // asserts (CgsIntervalList.cpp:90-96) which bake the identical message strings.
    // (Sub-note on polarity: the console's single `vcmpgtfp` reports FALSE for a NaN extent
    // and would therefore fire, while `!IsZero(NaN)` is TRUE and does not. That case is
    // unreachable here -- the two IsValid asserts immediately above reject NaN corners first,
    // and both are non-gating tripwires either way.)
    //
    // Assert lines/messages, all recovered in full from the private .i64 (IDA's inline
    // listing truncates at 39 chars):
    //   :202/:204  streamed "Too many intervals: " << luNewObject<Min|Max>
    //   :207       "rw::math::IsValid( lMin )"      (per-lane vcmpeqfp self-compare, x/y/z)
    //   :208       "rw::math::IsValid( lMax )"
    //   :209/:210/:211  "Bounding box added with zero width|height|depth"
    // ------------------------------------------------------------------------
    void IntervalList::AddObject(u16 luObjectIndex, Vector3 lMin, Vector3 lMax)
    {
        // Two slots, claimed one at a time; the console bumps muNumIntervals BEFORE each
        // bounds test and re-reads both fields for the second one (0x828B49DC/0x828B49EC,
        // then 0x828B4A74/0x828B4A84), so a failed assert still leaves the count advanced.
        const u32 luNewObjectMin = muNumIntervals;
        muNumIntervals = luNewObjectMin + 1;
        CGS_ASSERT(luNewObjectMin < muMaxNumIntervals, "Too many intervals: ");   // :202

        const u32 luNewObjectMax = muNumIntervals;
        muNumIntervals = luNewObjectMax + 1;
        CGS_ASSERT(luNewObjectMax < muMaxNumIntervals, "Too many intervals: ");   // :204

        CGS_ASSERT(rw::math::vpu::IsValid(lMin), "rw::math::IsValid( lMin )");    // :207
        CGS_ASSERT(rw::math::vpu::IsValid(lMax), "rw::math::IsValid( lMax )");    // :208

        // Per-axis extent, asserted non-degenerate. Operand order is the console's
        // `vsubfp vD, vMax, vMin` (0x828B4C48 / 0x828B4CC8 / 0x828B4D44).
        const f32 lfWidth  = lMax.x - lMin.x;
        CGS_ASSERT(!rw::math::fpu::IsZero(lfWidth),  "Bounding box added with zero width");   // :209

        const f32 lfHeight = lMax.y - lMin.y;
        CGS_ASSERT(!rw::math::fpu::IsZero(lfHeight), "Bounding box added with zero height");  // :210

        const f32 lfDepth  = lMax.z - lMin.z;
        CGS_ASSERT(!rw::math::fpu::IsZero(lfDepth),  "Bounding box added with zero depth");   // :211

        // The two endpoint records. Everything but mfXInterval is shared.
        const f32 lfZMin      =  lMin.z;
        const f32 lfYMin      =  lMin.y;
        const f32 lfMinusZMax = -lMax.z;
        const f32 lfMinusYMax = -lMax.y;

        Interval& lrMinInterval = mpaIntervals[luNewObjectMin];
        lrMinInterval.mu16ObjectIndex     = luObjectIndex;
        lrMinInterval.mfXInterval         = lMin.x;
        lrMinInterval.mfYMinInterval      = lfYMin;
        lrMinInterval.mfZMinInterval      = lfZMin;
        lrMinInterval.mfMinusYMaxInterval = lfMinusYMax;
        lrMinInterval.mfMinusZMaxInterval = lfMinusZMax;
        lrMinInterval.mu16Flags           = 0;   // min-role endpoint

        Interval& lrMaxInterval = mpaIntervals[luNewObjectMax];
        lrMaxInterval.mu16ObjectIndex     = luObjectIndex;
        lrMaxInterval.mfXInterval         = lMax.x;
        lrMaxInterval.mfYMinInterval      = lfYMin;
        lrMaxInterval.mfZMinInterval      = lfZMin;
        lrMaxInterval.mfMinusYMaxInterval = lfMinusYMax;
        lrMaxInterval.mfMinusZMaxInterval = lfMinusZMax;
        lrMaxInterval.mu16Flags           = 1;   // max-role endpoint

        // The object -> slot back-reference. `clrlslwi r10,r20,16,2` at 0x828B4EE0 is
        // (u16)luObjectIndex * 4, i.e. sizeof(ObjectToIntervalMap) -- the map is indexed by
        // OBJECT index, never by interval slot.
        ObjectToIntervalMap& lrMap = mpaObjectToIntervalMap[luObjectIndex];
        lrMap.SetIntervalIndex(KI_MIN_INDEX, static_cast<u16>(luNewObjectMin));
        lrMap.SetIntervalIndex(KI_MAX_INDEX, static_cast<u16>(luNewObjectMax));
    }
}
