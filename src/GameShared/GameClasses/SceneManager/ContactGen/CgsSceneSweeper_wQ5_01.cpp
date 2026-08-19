// ============================================================================
// CgsSceneManager::SceneSweeper::AddObject
//   Home: GameShared/GameClasses/SceneManager/ContactGen/CgsSceneSweeper.cpp
//   Partfile: CgsSceneSweeper_wQ5_01.cpp   (wave Q5 round 2, cluster "sweeper (i)")
//
// X360: @ 0x828B5958, 316 instructions, store-for-store.
//
// This is the sweeper's ENTRY DOOR. Everything the broadphase later does -- SortLists,
// SweepIntervals, SweepAgainstList, BuildCollidingPairs -- operates on the endpoint
// records this function manufactures, so nothing collides with anything until a body
// gets in here. The console reaches it from
// OverlapGenerationModule::ProcessAddBodyQueue @0x828C1D18, one call per queued
// InAddBodyEvent.
//
// LEDGER NOTE (measured, gotcha 10): progress/ledger marks this `reviewed` with its TU
// set to the `CgsStrStream.h` catch-all bucket. There was NO body anywhere in the tree.
// ============================================================================

#include "GameShared/GameClasses/SceneManager/ContactGen/CgsSceneSweeper.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT

namespace CgsSceneManager
{
namespace
{
    // ------------------------------------------------------------------------
    // rw::collision::AABBox is only FORWARD-DECLARED in CgsSceneSweeper.h, deliberately:
    // its real home (vendor/renderware/collision/AABBox.hpp) pulls the SDKs/EATech
    // rw::math::vpu::Vector3 CLASS into a TU that already has the vendor 4-lane POD via
    // BrnCommonTypes.h, and the two cannot coexist -- MEASURED here, not inherited:
    // scratchpad/waveQ5/probe_sweep1/p_aabb.cpp includes exactly this header plus
    // AABBox.hpp and dies with `C2011: "rw::math::vpu::Vector3": struct redefinition`
    // (SDKs/EATech/include/rw/math/vpu/vector3.h:26 vs
    // vendor/renderware/include/rw/math/vpu/types.h:24) followed by a >100-error cascade.
    // That double definition is a PRE-EXISTING FORK owned by neither this cluster nor the
    // sweeper keystone; it is REPORTED, not worked around (see sweep1.owner.md).
    //
    // What this body needs from the box is only its byte image: two 16-byte corner rows,
    // mMin @+0x00 and mMax @+0x10 (the console reads them as `lvx128 v, r0, box` and
    // `lvx128 v, r0, box+0x10` -- 0x828B5B3C/0x828B5B48 and again at 0x828B5BD0/0x828B5BD8
    // and 0x828B5C5C/0x828B5C64). That image is PINNED by the already-mounted layout oracle
    // GameSource/Physics/PropManager/PropManager_wQ4_03_embed_check.cpp (bat line 1748),
    // which CAN include AABBox.hpp and static_asserts sizeof==32 / offsetof(mMin)==0 /
    // offsetof(mMax)==16 -- so a change to AABBox breaks THAT gate rather than this reader.
    // Same solution, same wording, as the two committed precedents that hit the identical
    // wall: CgsVolumeManager.cpp:54-77 and PropManager_wQ4_03.cpp:289-301.
    // ------------------------------------------------------------------------
    struct alignas(16) AABBoxRows
    {
        Vector3 mMin;   // AABBox +0x00
        Vector3 mMax;   // AABBox +0x10
    };
    static_assert(sizeof(AABBoxRows) == 32, "AABBox image is two 16-byte rows");
}

    // ------------------------------------------------------------------------
    // CgsSceneSweeper.cpp:270-306 @ 0x828B5958
    //
    // Registers one swept body. The three-way dispatch on the RenderWare body state is the
    // whole point of the function -- it decides WHICH of the sweeper's three interval lists
    // owns the object, and therefore which sweep leg will ever see it:
    //
    //   leBodyState & rw::physics::ACTIVE_BODY (4)  -> mDynamicIntervalList,  E_DYNAMIC_BODY
    //     (`rlwinm r11,r14,0,29,29` @0x828B5CE8 -- PPC bit 29 == mask 0x4)
    //   leBodyState & rw::physics::FROZEN_BODY (2)  -> mInactiveIntervalList, E_INACTIVE_BODY
    //     (`rlwinm r11,r14,0,30,30` @0x828B5D14 -- mask 0x2) ... and mbSortFrozenObjects = true
    //     (`lis 0xD ; ori 0xC4B9 ; stbx` @0x828B5D38-0x828B5D44 == this+0xDC4B8+1)
    //   otherwise                                   -> assert STATIC_BODY (1) and RETURN
    //     (`clrlwi r11,r14,31` @0x828B5E18 -- mask 0x1)
    //
    // ⚠️ FAITHFUL ODDITY -- REPRODUCED, NOT "FIXED". A STATIC_BODY is added to NO list, gets
    // NO maObjectData state (it stays E_INVALID_BODY), does NOT get its mCollidingBodies bit
    // and does NOT bump muNumObjects. The console's static arm is `assert; return` and
    // nothing else -- 0x828B5E18..0x828B5E44 contains exactly the assert block and the
    // epilogue. mStaticIntervalList is bound by Prepare and then swept by nobody (SweepLists
    // @0x828C20F0 touches only the dynamic and inactive lists). Do not "complete" this arm.
    //
    // ⭐ WHICH BIT ARRAY. The duplicate-add test and the tail SetBit both address
    // `this + 0xDABF8 + 8*(idx>>6)` (read: 0x828B5A68 `addis r11,r11,2 ; addi r11,r11,-0x4A81
    // ; slwi 3 ; ldx r11,r11,r15`; write: 0x828B5D48 `addis r24,r15,0xE ; addi r24,r24,-0x5408`
    // then `ldx/or/stdx` at 0x828B5DF8-0x828B5E00). 0xDABF8 is mCollidingBodies -- the SECOND
    // of the two BitArray<5051> members -- once maObjectData's 5051-byte span and
    // mUseInternalCollision's 632 are walked forward from the asm-attested maObjectData base
    // 0xD95C0 (`add r11,r21,r15 ; addis r18,r11,0xE ; addi r18,r18,-0x6A40` @0x828B5B04).
    // It is NOT mUseInternalCollision, and it is NOT mabForceNoPadding (0xDC230, attested
    // separately by ForceNoPadding @0x828B0578). See sweep1.owner.md s5 for the three header
    // COMMENT offsets this measurement corrects; the member ORDER and every host-side
    // static_assert span in CgsSceneSweeper.cpp are unaffected, and this body addresses
    // everything by name.
    //
    // The 64-bit-word bit math (`srwi r11,r21,6` field, `clrldi r10,r21,58` bit) is exactly
    // CgsContainers::BitArray<5051>'s inlined IsBitSet/SetBit, which is why the two range
    // tripwires below bake CgsBitArray.h rather than CgsSceneSweeper.cpp.
    //
    // ASSERTS -- lines and messages recovered in full from a private .i64 copy (IDA's inline
    // listing truncates at 39 chars), file strings dumped byte-for-byte:
    //   CgsSceneSweeper.cpp:274  "luObjectIndex < KU_MAX_NUM_OBJECTS"
    //   CgsBitArray.h:203        streamed "invalid index : " << idx << " < " << 5051
    //                            (IsBitSet's own bounds check; the console CSEs it into the
    //                             SAME `blt` as :274 because the condition is identical --
    //                             0x828B59A8 skips both blocks)
    //   CgsSceneSweeper.cpp:275  streamed "object being added to the scene sweeper twice ID="
    //                            << lVolumeInstanceId
    //   CgsSceneSweeper.cpp:276  "maObjectData[luObjectIndex].mu8BodyState == E_INVALID_BODY"
    //   CgsSceneSweeper.cpp:278/:279/:280  streamed "Bad AABBox " << lVolumeInstanceId,
    //                            one per axis, tested `vcmpgefp max >= min` on lanes
    //                            0 / 2 / 1 -- i.e. X, then Z, then Y, in that order
    //   CgsSceneSweeper.cpp:304  "(leBodyState & rw::physics::STATIC_BODY) != 0"
    //   CgsBitArray.h:222        streamed "Index: " << idx << ", Number of bits: " << 5051
    //                            (SetBit's own bounds check)
    // The four streamed messages are reduced to their literal prefix, the tree's standing
    // convention for CgsDev StrStream asserts (CgsVolumeManager.cpp, CgsTriangleCacheManager_
    // Events.cpp), except the CgsBitArray.h:222 one which is spelled the way this same class
    // already spells it in ForceNoPadding (CgsSceneSweeper.cpp:200).
    //
    // ⚠️ lVolumeInstanceId IS CONSUMED ONLY BY THOSE STREAMED MESSAGES on the console -- its
    // four uses are all `mr r4,r20 ; bl sub_82203EE8` feeding the StrStream (0x828B5AE0,
    // 0x828B5BAC, 0x828B5C38, 0x828B5CC4). It reaches no list, no map and no member. The
    // parameter is kept (the DWARF declares it and a future StrStream restoration needs it);
    // it is genuinely unreferenced in the reduced form, which is not a defect.
    // ------------------------------------------------------------------------
    void SceneSweeper::AddObject(u32 luObjectIndex, const rw::collision::AABBox* lpAABBox,
                                 rw::physics::BodyState leBodyState,
                                 VolumeInstanceId lVolumeInstanceId)
    {
        CGS_ASSERT(luObjectIndex < KU_MAX_NUM_OBJECTS,
                   "luObjectIndex < KU_MAX_NUM_OBJECTS");                    // :274
        CGS_ASSERT(luObjectIndex < KU_MAX_NUM_OBJECTS, "invalid index : ");  // CgsBitArray.h:203

        CGS_ASSERT(!mCollidingBodies.IsBitSet(luObjectIndex),
                   "object being added to the scene sweeper twice ID=");     // :275

        CGS_ASSERT(maObjectData[luObjectIndex].mu8BodyState == E_INVALID_BODY,
                   "maObjectData[luObjectIndex].mu8BodyState == E_INVALID_BODY");  // :276

        // The world box, read as its two corner rows (see the AABBoxRows note above).
        const AABBoxRows& lrBox = *reinterpret_cast<const AABBoxRows*>(lpAABBox);

        CGS_ASSERT(lrBox.mMax.x >= lrBox.mMin.x, "Bad AABBox ");   // :278  (lane 0)
        CGS_ASSERT(lrBox.mMax.z >= lrBox.mMin.z, "Bad AABBox ");   // :279  (lane 2)
        CGS_ASSERT(lrBox.mMax.y >= lrBox.mMin.y, "Bad AABBox ");   // :280  (lane 1)

        if ((leBodyState & rw::physics::ACTIVE_BODY) != 0)
        {
            mDynamicIntervalList.AddObject(static_cast<u16>(luObjectIndex),
                                           lrBox.mMin, lrBox.mMax);
            maObjectData[luObjectIndex].mu8BodyState = E_DYNAMIC_BODY;
        }
        else if ((leBodyState & rw::physics::FROZEN_BODY) != 0)
        {
            mInactiveIntervalList.AddObject(static_cast<u16>(luObjectIndex),
                                            lrBox.mMin, lrBox.mMax);
            maObjectData[luObjectIndex].mu8BodyState = E_INACTIVE_BODY;

            // The inactive list is only re-sorted on demand; adding to it dirties it.
            mbSortFrozenObjects = true;
        }
        else
        {
            CGS_ASSERT((leBodyState & rw::physics::STATIC_BODY) != 0,
                       "(leBodyState & rw::physics::STATIC_BODY) != 0");     // :304
            return;   // faithful: a static body joins no list and is not counted
        }

        CGS_ASSERT(luObjectIndex < KU_MAX_NUM_OBJECTS, "Index out of range\n");  // CgsBitArray.h:222
        mCollidingBodies.SetBit(luObjectIndex);
        ++muNumObjects;
    }
}
