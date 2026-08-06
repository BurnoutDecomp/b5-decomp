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

    // =================================================================================================
    // ⭐ ADDED 2026-08-06 (big-five #2, contact-generation wave).
    //
    // ContactGenList::Construct -- the X360 CreateIOBuffer<ContactGenList> template runs
    // T::Construct after the alloc; the PC template placement-news only, so
    // StartVehicleContactGeneration calls this explicitly (the InputBuffer::Construct precedent).
    // The console inline (@0x8262AFC8..0x8262AFD4): `stw 0, 0xC08(list)` (miNumEntries = 0) +
    // the IOBuffer status raise (`li 1 ; stb 0(list)`).
    // =================================================================================================
    void ContactGenList::Construct()
    {
        CgsModule::IOBuffer::Construct();
        miNumEntries = 0;
    }

    // =================================================================================================
    // ContactGenList::AddEntry(EntityId, EntityId, u8, u8)  @0x825B59B0 -- the 4-byte-entity-id
    // overload. asm-exact:
    //   * assert miNumEntries < 128 BEFORE the writes (non-gating; header line :75);
    //   * mIdA = (u64)lIdA << 32 (`sldi r30,32 ; std +8`), mIdB = (u64)lIdB << 32 (`std +0x10`)
    //     -- an entity-only volume-instance id (volume word zero);
    //   * the two offset bytes at entry+16/+17; then ++miNumEntries.
    // Callers: StartVehicleContactGeneration (the two simple-traffic pair-list markers) + the
    // Do*ContactGeneration family.
    // =================================================================================================
    void ContactGenList::AddEntry(EntityId lIdA, EntityId lIdB,
                                  u8 lu8IdAVolInstOffset, u8 lu8IdBVolInstOffset)
    {
        CGS_ASSERT(miNumEntries < KI_MAX_ENTRIES, "miNumEntries < miMaxEntries");   // :75

        ContactGenEntry& lrEntry = maEntries[miNumEntries];
        lrEntry.mIdA.muId         = static_cast<u64>(lIdA.muValue) << 32;
        lrEntry.mIdB.muId         = static_cast<u64>(lIdB.muValue) << 32;
        lrEntry.mIdAVolInstOffset = lu8IdAVolInstOffset;
        lrEntry.mIdBVolInstOffset = lu8IdBVolInstOffset;

        ++miNumEntries;
    }
}
