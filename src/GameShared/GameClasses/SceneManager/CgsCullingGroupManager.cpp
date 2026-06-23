#include "GameShared/GameClasses/SceneManager/CgsCullingGroupManager.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsSceneManager::CullingGroupManager::SetCullingGroupPair @ 0x828AA580
//
// Behaviour-faithful to the X360 asm (store-for-store). The culling table is a
// symmetric NxN adjacency bit grid; muHeight (the backing store's +4 word) is the
// row stride. For each of the two symmetric cells (A,B) and (B,A):
//   index = muHeight * row + col
//   word  = maBits[index >> 5]      (the asm's `(index >> 5) + 3` word offset is
//                                     the 3 header words before maBits)
//   bit   = 1 << (index & 0x1F)
// lbEnabled sets the bit (`word |= bit`); otherwise it clears it (`word &= ~bit`).

namespace CgsSceneManager
{

void CullingGroupManager::SetCullingGroupPair(CullingGroup liGroupA, CullingGroup liGroupB, bool lbEnabled)
{
    rw::BitTable::Storage* lpTable = mpCullingTable;
    const u32 luStride = lpTable->muHeight;

    // First cell: (liGroupA row, liGroupB column).
    const u32 luIndexAB = luStride * liGroupA + liGroupB;
    const u32 luBitAB   = 1u << (luIndexAB & 0x1Fu);
    u32&      lrWordAB  = lpTable->maBits[luIndexAB >> 5];
    if (lbEnabled)
        lrWordAB |= luBitAB;
    else
        lrWordAB &= ~luBitAB;

    // Second (symmetric) cell: (liGroupB row, liGroupA column). The X360 reloads
    // mpCullingTable here, so re-fetch it.
    rw::BitTable::Storage* lpTable2 = mpCullingTable;
    const u32 luStride2 = lpTable2->muHeight;
    const u32 luIndexBA = luStride2 * liGroupB + liGroupA;
    const u32 luBitBA   = 1u << (luIndexBA & 0x1Fu);
    u32&      lrWordBA  = lpTable2->maBits[luIndexBA >> 5];
    if (lbEnabled)
        lrWordBA |= luBitBA;
    else
        lrWordBA &= ~luBitBA;
}

}
