#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// Out-of-line bodies for the two CgsSceneManager::VolumeInstanceId field setters the
// X360 ARTIST build emitted. Each operates on the 32-bit ENTITY WORD that lives in the
// HIGH dword (bits [32..63]) of the packed 64-bit muId: it reads that word, splices a
// field into it (read-modify-write of just that field), then stores it back to the high
// dword while preserving the low dword. Store order / bit-field masks are reproduced
// exactly from the X360 asm (see CgsVolumeInstanceId.h banner for the per-symbol
// addresses and the 8/14/10-bit owner/entity/part layout).
//
// HOST-vs-X360 NOTE: the asm reaches the entity word via a 64-bit big-endian load and a
// `srdi r,r,32` (the HIGH 32 bits). On a little-endian host the high dword is expressed
// here as `muId >> 32`, so the field math stays endian-independent. The asm's "zero the
// high dword, reload, OR back" idiom (`stw r,0(this); ld; or; std`) preserves the low
// dword unchanged — expressed below as keeping `(muId & 0xFFFFFFFFull)`.

namespace CgsSceneManager
{

// 0x822B0E00 — SetEntityIDOwner.
//   ld r11,0(this); srdi r11,r11,32; clrlwi r30,r11,0        -> entityWord = muId>>32
//   clrlwi r29,r4,24; cmplwi r29,0xC; ble skip               -> owner (u8); assert <= 0xC
//     (assert "Burnout Specfic: Bad entity type set", CgsEntityId.h:153)
//   insrwi r30,r29,8,0                                        -> splice owner into bits [24..31]
//   sldi r10,r30,32; stw 0,0(this); ld r11,0(this); or; std  -> high dword <- entityWord
// The PPC `insrwi r30,r29,8,0` inserts owner's low 8 bits at bit offset 0 of the 32-bit
// word == the high byte (bits [24..31]) == KU_OWNER_BASE.
VolumeInstanceId* VolumeInstanceId::SetEntityIDOwner(u8 lu8Owner)
{
    const u32 luOwner = lu8Owner;
    CGS_ASSERT(luOwner <= KU_MAX_ENTITY_TYPE, "Burnout Specfic: Bad entity type set");

    u32 luEntityWord = static_cast<u32>(muId >> 32);
    luEntityWord = (luEntityWord & ~KU_OWNER_MASK) | (luOwner << KU_OWNER_BASE);

    muId = (static_cast<u64>(luEntityWord) << 32) | (muId & 0xFFFFFFFFull);
    return this;
}

// 0x822B0E70 — SetEntityIDEntityIndex.
//   ld r11,0(this); srdi r11,r11,32; clrlwi r29,r11,0        -> entityWord = muId>>32
//   cmplwi r30,0x4000; blt skip                              -> index (u32); assert < 1<<14
//     (assert "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)", CgsEntityId.h:160)
//   rlwinm r11,r29,0,22,7                                    -> clear bits [10..23] (keep 0xFF0003FF)
//   slwi r9,r30,10; or r11,r11,r9                            -> | (index << 10)
//   sldi r11,r11,32; stw 0,0(this); ld; or; std             -> high dword <- entityWord
VolumeInstanceId* VolumeInstanceId::SetEntityIDEntityIndex(u32 luEntityIndex)
{
    CGS_ASSERT(luEntityIndex < (1u << KU_NUM_BITS_FOR_ENTITY_NUM),
               "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");

    u32 luEntityWord = static_cast<u32>(muId >> 32);
    luEntityWord = (luEntityWord & ~KU_ENTITY_INDEX_MASK)
                 | (luEntityIndex << KU_ENTITY_INDEX_BASE);

    muId = (static_cast<u64>(luEntityWord) << 32) | (muId & 0xFFFFFFFFull);
    return this;
}

} // namespace CgsSceneManager
