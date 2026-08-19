// ============================================================================
// CgsSceneManager::SceneSweeper -- round-2 partfile (ii): the two per-frame body
// mutators the broadphase drives every tick.
//
// Home of record: GameShared/GameClasses/SceneManager/ContactGen/CgsSceneSweeper.cpp
// (the console bakes THAT path into every assert in both bodies). This partfile is a
// wave-Q5 ownership split only -- the class, the member set and the layout pins all live
// in CgsSceneSweeper.{h,cpp}; nothing here re-declares anything.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   SceneSweeper::UpdateObject @ 0x828B6160  (318 insns)
//       ⚠️ ABSENT FROM progress/identity.json AND FROM THE PER-ADDRESS EXPORT. Recovered by
//       targeted headless IDA on a private .i64 copy; the dump this body was written from is
//       scratchpad/waveQ5/q5_sweeper_holes.json (entry "0x828b6160").
//   SceneSweeper::RemoveObject @ 0x828B5E48  (197 insns)
//       .ida-exports/BURNOUT_X360_ARTIST.XEX/0x828B5E48.json
//
// Callers: OverlapGenerationModule::ProcessUpdateBodyQueue @0x828C1DE8 and
// ProcessRemoveBodyQueue -- i.e. these two run once per queued body event per frame, and
// UpdateObject is the leg that keeps a MOVING car in the dynamic interval list (a body that
// stops being updated is auto-frozen into the inactive list by SceneSweeper::Update, which is
// how a parked smash gate ends up in the list the car is swept against).
//
// ---------------------------------------------------------------------------------------
// ⭐ THE THIRD PARAMETER OF UpdateObject IS A **PADDING** VECTOR, NOT A POSITION -- and the
// DecFIGS DWARF now settles it by NAME. The header currently spells it `lvPosition` (and the
// producer event field is called mvPosition); both names are DEFECTS, reported by the
// cluster-B1 keystone. FOUR independent witnesses:
//   * DWARF (rung 2, declaration shape -- dwarfdump/.../CgsSceneSweeper.cpp:152) declares
//         SceneSweeper::UpdateObject(uint32_t luObjectIndex, const AABBox* lpBox,
//                                    const rw::math::vpu::Vector3 lPadding,
//                                    VolumeInstanceId lVolumeInstanceID)
//     -- the original source's own parameter names;
//   * the X360 body never uses the vector as a point -- it is a per-lane clamp on the box
//     inflation (`vminfp128` against {-0.1,-0.1,-0.1,0}, `vmaxfp128` against {0.1,0.1,0.1,0});
//   * the body's own assert message prints it as " Padding: [" (0x828B647C aPadding_1);
//   * the producer OverlapGenerationIO::InputBuffer::UpdateBody @0x828BA430 names it lvPadding.
// This definition therefore uses the DWARF's names (lpBox / lPadding / lVolumeInstanceID),
// exactly as the sibling bodies in CgsIntervalList.cpp and CgsVolumeManager.cpp use theirs. A
// definition may name a parameter differently from its declaration; the types and order are
// identical, which is what the ABI and the caller see. The header rename is the fixer's.
// ---------------------------------------------------------------------------------------
// ⚠️ FAITHFUL ODDITY IN UpdateObject -- REPRODUCED, DO NOT "FIX": the final else arm asserts
// `leBodyState == E_STATIC_BODY` and then does the static-list update ANYWAY, so an object in
// E_INVALID_BODY (the state Clear() seeds and RemoveObject() restores) is pushed through
// mStaticIntervalList.UpdateObject with a stale object->interval mapping. The console does
// exactly that: 0x828B65E4 `cmpwi 2 / beq loc_828B6618` falls THROUGH the assert block into
// the same loc_828B6618 the E_STATIC_BODY case branches to.
// ============================================================================

#include "GameShared/GameClasses/SceneManager/ContactGen/CgsSceneSweeper.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"                // Min / Max / operator+ (vminfp/vmaxfp/vaddfp)

// rw::collision::AABBox is only NAMED by the declaration this file defines; the body needs
// its two corner ROWS. Its real home, vendor/renderware/collision/AABBox.hpp, pulls the SDK
// rw::math::vpu::Vector3 CLASS into a TU that already has the vendor 4-lane POD through
// CgsSceneSweeper.h -> BrnCommonTypes.h, and the two cannot coexist (measured:
// scratchpad/waveQ5/probe_sweeper/probe_incl.cpp, and the same clash is solved the same way in
// GameShared/GameClasses/SceneManager/CgsVolumeManager.cpp:54-77 and
// GameSource/Physics/PropManager/PropManager_wQ4_03.cpp:289-301). What this TU needs is only
// AABBox's byte image: two 16-byte float4 rows, mMin @+0x00 and mMax @+0x10 -- which is
// exactly what the console reads (`lvx128 v0,r0,r29` and `lvx128 v12,r0,r29+0x10`).
// That image is PINNED by the mounted oracle TU
// GameSource/Physics/PropManager/PropManager_wQ4_03_embed_check.cpp, which CAN include
// AABBox.hpp and static_asserts sizeof==32 / offsetof(mMin)==0 / offsetof(mMax)==16, so a
// change to AABBox breaks that gate rather than this reader.
namespace rw { namespace collision { class AABBox; } }

namespace CgsSceneManager
{

namespace
{
    // The byte image of rw::collision::AABBox over the vendor POD Vector3 this TU speaks.
    struct alignas(16) AABBoxRows
    {
        Vector3 mMin;   // AABBox +0x00
        Vector3 mMax;   // AABBox +0x10
    };
    static_assert(sizeof(AABBoxRows) == 32, "AABBox image is two 16-byte rows");

    // The two per-lane clamps UpdateObject applies to the caller's padding vector before it
    // inflates the box. Both are .rdata singles the body loads and splats into lanes 0..2 with
    // lane 3 stored as literal zero (0x828B633C/0x828B6374 `stw r21`):
    //   flt_8200D530 == -0.1f  (0x828B6348)  -- value corroborated by three independent users
    //                                           (0x825C7568 `if ( v53 < -0.1 )`, 0x827249F8,
    //                                           0x8253E200), not inferred from this body.
    //   flt_82004014 == +0.1f  (0x828B6364)  -- likewise (0x82715A18, 0x828ABF48).
    // Semantics: the box is inflated by AT LEAST 0.1 in every direction, and by more where the
    // caller's padding asks for more.
    const f32 KF_MIN_PADDING = -0.1f;
    const f32 KF_MAX_PADDING =  0.1f;
}

// ================================================================================
// SceneSweeper::UpdateObject  @ 0x828B6160   (CgsSceneSweeper.cpp:388-431)
//
// Re-fits one already-registered body's interval pair to a fresh world box, inflated by the
// caller's padding vector (clamped to at least 0.1 outward per lane), and migrates the body
// back into the DYNAMIC list if the auto-freeze had moved it to the inactive one. Every arm
// stamps mabMovedThisFrame, which is what stops SceneSweeper::Update re-freezing the body at
// the end of the same frame.
//
// Store-for-store map (console -> here):
//   0x828B61A8  cmplwi 0x13BB / blt          -> the merged bound check; the compiler folded the
//               body's OWN `luObjectIndex < KU_MAX_NUM_OBJECTS` assert (:392, streamed
//               "Object index is invalid: " << i << "\n") and BitArray<5051>::IsBitSet's
//               inlined bounds assert (CgsBitArray.h:203, streamed "invalid index : " << i
//               << " < " << 5051) into ONE branch, because both test the same predicate.
//   0x828B62D4  the inlined IsBitSet + assert :393 "mCollidingBodies.IsBitSet( luObjectIndex )"
//   0x828B6330  lvx128 box.mMin / box.mMax, the two literal clamp vectors, vminfp128/vmaxfp128
//               against the padding, two vaddfp -> the padded min/max
//   0x828B63B0  THREE per-lane `vcmpgtfp.` tests, lanes 0/1/2 == x/y/z, each one SKIPPING the
//               assert when that lane is ordered -- i.e. ONE assert whose condition is the OR
//               of the three lanes (:403). NaN polarity is host `<`/`>`: an unordered lane
//               leaves the CR6 all-true bit clear and falls through to the next lane.
//   0x828B6404  the assert's StrStream message, which reports the RAW box and the RAW padding
//               (not the padded values it just tested):
//                 "\n Bad AABBox \n" " Padding: [" px ", " py ", " pz "] \n "
//                 " Min: [" minx ", " miny ", " minz "] \n"
//                 " Max: [" maxx ", " maxy ", " maxz "] \n" " ID: " <VolumeInstanceId> "\n"
//               CGS_ASSERT in this tree takes a plain string (CgsAssert.h), so as in every
//               other reconstructed TU the message stops at the leading static string and
//               lVolumeInstanceID -- whose ONLY use in this body is that stream -- is unread.
//   0x828B656C  lbz maObjectData[i].mu8BodyState + the three-way dispatch
//   0x828B6634  stbx 1 -> mabMovedThisFrame[i]   (base 0xDAE70, all three arms)
//
// DWARF CORROBORATION (dwarfdump/.../CgsSceneSweeper.cpp:152-252) -- the local names and the
// call list below are the ORIGINAL source's, not this author's:
//   locals   lPaddedMin (:388), lPaddedMax (:389), leBodyState (:390, typed eObjectBodyState)
//   calls    Vector3::Vector3 x2 (the two literal clamp vectors), rw::math::vpu::Min,
//            rw::math::vpu::Max, AABBoxTemplate<...>::Min / ::Max (the box's corner
//            accessors -- our reconstructed AABBox exposes the same two rows as mMin/mMax),
//            rw::math::vpu::operator+ x2, then EXACTLY THREE per-axis comparisons
//            `rw::math::vpu::operator< <VectorAxisX,VectorAxisX>` / Y / Z -- which is why the
//            assert below is spelled min < max per axis, and why it is an OR of three lanes
//            and not three separate asserts.
void SceneSweeper::UpdateObject(u32 luObjectIndex, const rw::collision::AABBox* lpBox,
                                Vector3 lPadding, VolumeInstanceId lVolumeInstanceID)
{
    CGS_ASSERT(luObjectIndex < KU_MAX_NUM_OBJECTS, "Object index is invalid: ");   // :392
    CGS_ASSERT(luObjectIndex < KU_MAX_NUM_OBJECTS, "invalid index : ");            // CgsBitArray.h:203
    CGS_ASSERT(mCollidingBodies.IsBitSet(luObjectIndex),
               "mCollidingBodies.IsBitSet( luObjectIndex )");                      // :393

    (void)lVolumeInstanceID;   // console: streamed into the :403 assert message only

    const AABBoxRows& lrBox = *reinterpret_cast<const AABBoxRows*>(lpBox);

    // The two literal clamp vectors, lane 3 zero exactly as the console builds them
    // (DWARF: the two rw::math::vpu::Vector3::Vector3 constructions).
    const Vector3 lMinPaddingLimit = { KF_MIN_PADDING, KF_MIN_PADDING, KF_MIN_PADDING, 0.0f };
    const Vector3 lMaxPaddingLimit = { KF_MAX_PADDING, KF_MAX_PADDING, KF_MAX_PADDING, 0.0f };

    // vminfp128 v13,v13,v127 / vmaxfp128 v11,v11,v127 -- ARGUMENT ORDER PRESERVED: the clamp
    // vector is the first operand, so an unordered (NaN) padding lane yields the padding lane,
    // which is what `(a < b) ? a : b` gives for a == the limit. Then vaddfp against the box.
    const Vector3 lPaddedMin = lrBox.mMin + rw::math::vpu::Min(lMinPaddingLimit, lPadding);
    const Vector3 lPaddedMax = lrBox.mMax + rw::math::vpu::Max(lMaxPaddingLimit, lPadding);

    CGS_ASSERT(lPaddedMin.x < lPaddedMax.x
               || lPaddedMin.y < lPaddedMax.y
               || lPaddedMin.z < lPaddedMax.z, "\n Bad AABBox \n");                // :403

    const eObjectBodyState leBodyState =
        static_cast<eObjectBodyState>(maObjectData[luObjectIndex].mu8BodyState);
    if (leBodyState == E_DYNAMIC_BODY)
    {
        // Already swept as dynamic: rewrite both endpoints in place, list membership unchanged.
        mDynamicIntervalList.UpdateObject(static_cast<u16>(luObjectIndex), lPaddedMin, lPaddedMax);
    }
    else if (leBodyState == E_INACTIVE_BODY)
    {
        // ⭐ THE THAW. The body was auto-frozen into the inactive list by a previous
        // SceneSweeper::Update; it moved again, so it goes back to the dynamic list. Both
        // lists share one ObjectToIntervalMap array (SceneSweeper::Prepare passes the same
        // third argument to all three IntervalList::Prepare calls), so the remove/add pair
        // needs no map fix-up of its own.
        mInactiveIntervalList.RemoveObject(static_cast<u16>(luObjectIndex));
        mDynamicIntervalList.AddObject(static_cast<u16>(luObjectIndex), lPaddedMin, lPaddedMax);
        maObjectData[luObjectIndex].mu8BodyState = E_DYNAMIC_BODY;   // stb 0
        // The inactive list lost an entry, so it must be re-sorted before the next sweep.
        mbSortFrozenObjects = true;                                  // stbx 1 -> +0xDC4B9
    }
    else
    {
        // E_STATIC_BODY -- and, per the oddity in this file's banner, E_INVALID_BODY too:
        // the console asserts and then takes this arm regardless.
        CGS_ASSERT(leBodyState == E_STATIC_BODY, "leBodyState == E_STATIC_BODY");   // :426
        mStaticIntervalList.UpdateObject(static_cast<u16>(luObjectIndex), lPaddedMin, lPaddedMax);
        mbSortStaticObjects = true;                                  // stbx 1 -> +0xDC4B8
    }

    mabMovedThisFrame[luObjectIndex] = true;
}

// ================================================================================
// SceneSweeper::RemoveObject  @ 0x828B5E48   (CgsSceneSweeper.cpp:338-364)
//
// Unregisters a swept body: drops its two intervals from whichever list owns it, restores the
// per-object state byte to E_INVALID_BODY, clears its colliding-bodies bit and decrements the
// live count.
//
// ⚠️ THE STATIC/INVALID ARM RETURNS EARLY AND TOUCHES NOTHING -- no state reset, no bit clear,
// no counter decrement (0x828B5FC8 `bne cr6, loc_828B6154` jumps straight to the epilogue).
// That is the exact mirror of AddObject's faithful oddity, where a STATIC_BODY is added to no
// list, gets no maObjectData state and does not bump muNumObjects: what was never counted is
// never uncounted. Reproduced, not "fixed".
//
// Store-for-store map (console -> here):
//   0x828B5E60  cmplwi 0x13BB -> assert :340 "luObjectIndex < KU_MAX_NUM_OBJECTS"
//   0x828B5E88  lbz maObjectData[i].mu8BodyState        (base 0xD95C0)
//   0x828B5EA0  E_DYNAMIC arm: CgsBitArray.h:203 bounds assert, inlined IsBitSet, assert :347,
//               IntervalList::RemoveObject on +0xD6590 (mDynamicIntervalList)
//   0x828B5FCC  E_INACTIVE arm: the same pair at :352, RemoveObject on +0xD65A0
//               (mInactiveIntervalList), then stbx 1 -> +0xDC4B9 (mbSortFrozenObjects)
//   0x828B60FC  common tail: stb 3 (E_INVALID_BODY), CgsBitArray.h:241 bounds assert
//               ("luIndex < NUMBITS"), the `andc` UnSetBit on +0xDABF8 (mCollidingBodies),
//               and `lwz/addi -1/stw` on +0xDC4B0 (muNumObjects)
//
// DWARF CORROBORATION (dwarfdump/.../CgsSceneSweeper.cpp:64-87): the body's one local is
// `eObjectBodyState leBodyState` (:341), and its inline-call list is exactly
// BitArray<5051>::IsBitSet, BitArray<5051>::UnSetBit, BitArray<5051>::IsBitSet + TWO
// four-`operator<<` StrStream blocks -- i.e. ONE IsBitSet per swept arm (each carrying the
// "invalid index : " << i << " < " << 5051 bounds message) and ONE UnSetBit in the tail,
// which is precisely the shape below.
void SceneSweeper::RemoveObject(u32 luObjectIndex)
{
    CGS_ASSERT(luObjectIndex < KU_MAX_NUM_OBJECTS, "luObjectIndex < KU_MAX_NUM_OBJECTS");  // :340

    const eObjectBodyState leBodyState =
        static_cast<eObjectBodyState>(maObjectData[luObjectIndex].mu8BodyState);
    if (leBodyState == E_DYNAMIC_BODY)
    {
        CGS_ASSERT(luObjectIndex < KU_MAX_NUM_OBJECTS, "invalid index : ");   // CgsBitArray.h:203
        CGS_ASSERT(mCollidingBodies.IsBitSet(luObjectIndex),
                   "Non existing object being removed from the scene sweeper");            // :347
        mDynamicIntervalList.RemoveObject(static_cast<u16>(luObjectIndex));
    }
    else if (leBodyState == E_INACTIVE_BODY)
    {
        CGS_ASSERT(luObjectIndex < KU_MAX_NUM_OBJECTS, "invalid index : ");   // CgsBitArray.h:203
        CGS_ASSERT(mCollidingBodies.IsBitSet(luObjectIndex),
                   "Non existing object being removed from the scene sweeper");            // :352
        mInactiveIntervalList.RemoveObject(static_cast<u16>(luObjectIndex));
        // The inactive list lost an entry, so it must be re-sorted before the next sweep.
        mbSortFrozenObjects = true;
    }
    else
    {
        // E_STATIC_BODY / E_INVALID_BODY -- never entered a swept list; nothing to undo.
        return;
    }

    maObjectData[luObjectIndex].mu8BodyState = E_INVALID_BODY;

    CGS_ASSERT(luObjectIndex < KU_MAX_NUM_OBJECTS, "luIndex < NUMBITS");      // CgsBitArray.h:241
    mCollidingBodies.UnSetBit(luObjectIndex);

    --muNumObjects;
}

}
