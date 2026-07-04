// ============================================================================
// CgsSceneManager::SceneSweeper -- broadphase body-home TU.
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   SceneSweeper::ForceNoPadding()  @ 0x828B0578
//     (called by OverlapGenerationModule::ProcessForceNoPaddingQueue per queued
//      force-no-padding request; NOT executed before the boot-trace milestone.)
//
// It marks one volume instance as "no padding" by setting its bit in a
// CgsContainers::BitArray<5051> that lives inside the sweeper's opaque interval-
// list tail, at the X360 byte offset 0xDC230 from `this` (addis r31,0xE ;
// addi -0x3DD0). As the sibling module homes do, the field is addressed by its
// X360 byte offset through a u8* view of `this` rather than as a named member, so
// the committed sweeper layout / by-value embedding is left untouched. The 5051-bit
// capacity (632-byte BitArray) and the 0xDC230 offset are the only locations this
// body reads/writes.
//
// Two guard asserts, faithful to the X360:
//   1. the caller's KI_MAX_NUM_VOLUME_INSTANCES (5048) range check
//      (CgsSceneSweeper.h), string reproduced verbatim.
//   2. BitArray<5051>::SetBit's internal bounds assert (CgsBitArray.h:222). The
//      X360 streams a dynamic "Index: <i>, Number of bits: 5051" message; per the
//      project convention for this exact streamed BitArray:222 assert (see the
//      committed sibling CgsGui::EventInterpreterModule.cpp), it is reduced to the
//      static CGS_ASSERT string "Index out of range\n".
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsSceneSweeper.h"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"   // CgsContainers::BitArray
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT

namespace CgsSceneManager
{
    // The force-no-padding bit set: one bit per volume instance, capacity 5051
    // (the assert's "Number of bits: 5051"; KI_MAX_NUM_VOLUME_INSTANCES 5048 padded
    // by 3). Lives at this+0xDC230 inside the sweeper's opaque interval-list tail.
    static const u32 KU_NUM_VOLUME_INSTANCE_BITS     = 5051;
    static const u32 KU_FORCE_NO_PADDING_BITS_OFFSET = 0xDC230;

    typedef CgsContainers::BitArray<KU_NUM_VOLUME_INSTANCE_BITS> ForceNoPaddingBitArray;

    void SceneSweeper::ForceNoPadding(u32 luObjectIndex)
    {
        CGS_ASSERT(luObjectIndex < static_cast<u32>(KI_MAX_NUM_VOLUME_INSTANCES),
                   "luObjectIndex < (uint32_t)KI_MAX_NUM_VOLUME_INSTANCES");

        // BitArray<5051>::SetBit's inlined bounds guard (X360 CgsBitArray.h:222). The
        // dynamic "Index: <i>, Number of bits: 5051" streamed message collapses to the
        // project's static string for this assert.
        CGS_ASSERT(luObjectIndex < KU_NUM_VOLUME_INSTANCE_BITS, "Index out of range\n");

        // Address the force-no-padding bit set at its X360 byte offset through a u8*
        // view of `this` (sibling-module-home convention; the sweeper layout stays
        // opaque), then set the object's bit (field = index/64, bit = index%64).
        ForceNoPaddingBitArray* lpBits = reinterpret_cast<ForceNoPaddingBitArray*>(
            reinterpret_cast<u8*>(this) + KU_FORCE_NO_PADDING_BITS_OFFSET);
        lpBits->SetBit(luObjectIndex);
    }
}
