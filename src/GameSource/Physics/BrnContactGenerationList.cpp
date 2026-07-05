#include "GameSource/Physics/BrnContactGenerationList.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// =====================================================================================================
// BrnPhysics::ContactGenList::AddEntry(VolumeInstanceId, VolumeInstanceId, u8, u8)  @0x825B58F0
//
// Append one collision-pair entry (two volume-instance ids + their per-volume-instance offsets) to the
// fixed 128-slot contact-generation list. Producers: DeformableObject::DoBodyPartWorldContactGeneration
// and ::DoDetachedWheelWorldContactGeneration (one call per emitted primitive-vs-triangle contact).
//
// asm-exact store-for-store (0x825B58F0):
//   * assert miNumEntries < miMaxEntries (128) BEFORE the write (non-gating FireAssert; the blt skips
//     only the assert block, then falls through to the stores -- the write runs regardless).
//   * std r30,8(24n+this)  => maEntries[n].mIdA (8B, arg r4)
//   * std r29,0x10(...)    => maEntries[n].mIdB (8B, arg r5)
//   * stbx r28 @24(n+1)... => maEntries[n].mIdAVolInstOffset (arg r6, entry+16)
//   * stb  r27,0x19(24n)   => maEntries[n].mIdBVolInstOffset (arg r7, entry+17)
//   * lwz/addi+1/stw 0xC08 => ++miNumEntries.
// The EntityId (4-byte) overload is a separate console symbol -- declared, not homed by this TU.
// =====================================================================================================
namespace BrnPhysics
{
    void ContactGenList::AddEntry(CgsSceneManager::VolumeInstanceId lIdA,
                                  CgsSceneManager::VolumeInstanceId lIdB,
                                  u8 lu8IdAVolInstOffset,
                                  u8 lu8IdBVolInstOffset)
    {
        CGS_ASSERT(miNumEntries < KI_MAX_ENTRIES, "miNumEntries < miMaxEntries");

        ContactGenEntry& lrEntry = maEntries[miNumEntries];
        lrEntry.mIdA               = lIdA;
        lrEntry.mIdB               = lIdB;
        lrEntry.mIdAVolInstOffset  = lu8IdAVolInstOffset;
        lrEntry.mIdBVolInstOffset  = lu8IdBVolInstOffset;

        ++miNumEntries;
    }
}
