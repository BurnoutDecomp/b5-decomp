#pragma once

#include "types.hpp"
#include "vendor/renderware/collision/BitTable.hpp"   // rw::BitTable::Storage

// ===========================================================================
// CgsSceneManager::CullingGroupManager
//   Home: GameShared/GameClasses/SceneManager/CgsCullingGroupManager.{h,cpp}
//
// Maps every volume instance to a "culling group" (a small group id stored per
// instance) and holds an NxN symmetric adjacency bit grid telling whether two
// culling groups are allowed to test against each other. Embedded by value in
// CgsSceneManager::SceneManagerModule (mCullingGroupManager).
//
// LAYOUT (CgsSceneManagerModule.h DWARF + X360 asm @ 0x828AA580):
//   +0x0000  maVolumeInstanceCullingGroup[5051]  (uint8_t)  -- per-instance group id
//   +0x13BC  mpCullingTable                      (rw::BitTable::Storage*)
// The 5051-byte array rounds up to 5052 (0x13BC) so the pointer is 4-aligned --
// matching the asm's `lwz r11, 0x13BC(r3)`.
//
// The culling table is a flat packed bit grid (rw::BitTable backing store):
//   header { muWidth, muHeight, muWordCount } then maBits[]. The pair test uses
//   muHeight (+4) as the row stride: bit index = muHeight * groupA + groupB, then
//   word = maBits[index >> 5], bit = 1 << (index & 0x1F).
//
// Only SetCullingGroupPair is this TU's ledger function (reached from
// SceneManagerModule::BridgeInputSceneUpdateInterfaceToSubModules). The remaining
// members are declared for layout fidelity and bodied in their own TUs.
// ===========================================================================

namespace CgsSceneManager
{
    class CullingGroupManager
    {
    public:
        // A culling-group id (InEventAddForCollision::CullingGroup is a small int).
        typedef u32 CullingGroup;

        // @ X360 0x828AA580 -- set or clear the (liGroupA, liGroupB) adjacency bit.
        // The grid is symmetric: both (A,B) and (B,A) bits are written so the pair
        // test is order-independent.
        void SetCullingGroupPair(CullingGroup liGroupA, CullingGroup liGroupB, bool lbEnabled);

    private:
        u8                    maVolumeInstanceCullingGroup[5051];   // +0x0000
        rw::BitTable::Storage* mpCullingTable;                      // +0x13BC
    };
}
