#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"

// Out-of-line bodies for the two CgsSceneManager::EntityId setters the X360 ARTIST
// build emitted out-of-line (the remaining accessors stay inline in CgsEntityId.h).
// Store order and bit-fields are reproduced exactly from the X360 asm.

namespace CgsSceneManager
{
    // 0x8229FEF0: SetOwner. clrlwi r31,r4,24 byte-masks the owner (u8); cmplwi 0xC + ble
    // means the assert fires only when luOwner > 12, so the asserted range is
    // luOwner <= 12 (the X360 binary is authoritative -- the earlier inline '< 8' was
    // wrong). Assert string baked at CgsEntityId.h:153 (file/line dropped). Store:
    // insrwi r11,r31,8,0 splices the owner into the top byte ->
    // mId = (owner << KU_OWNER_BASE) | (mId & ~KU_OWNER_MASK).
    void
    EntityId::SetOwner( u8 luOwner )
    {
        CGS_ASSERT( luOwner <= 12, "Burnout Specfic: Bad entity type set" );
        mId = ( (u32)luOwner << KU_OWNER_BASE ) | ( mId & ~KU_OWNER_MASK );
    }

    // 0x8229FF58: SetEntityIndex. cmplwi 0x4000 + blt asserts luEntityIndex < 1<<14.
    // Assert string baked at CgsEntityId.h:160 (file/line dropped). Pack:
    // slwi r10,r30,10 (idx<<KU_ENTITY_INDEX_BASE); rlwinm r11,r11,0,22,7 clears
    // bits [10..23] leaving mask 0xFF0003FF (== ~KU_ENTITY_INDEX_MASK); or; stw ->
    // mId = (mId & ~KU_ENTITY_INDEX_MASK) | (idx << KU_ENTITY_INDEX_BASE).
    void
    EntityId::SetEntityIndex( u32 luEntityIndex )
    {
        CGS_ASSERT( luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM),
                    "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)" );
        mId = ( mId & ~KU_ENTITY_INDEX_MASK )
            | ( (u32)luEntityIndex << KU_ENTITY_INDEX_BASE );
    }
}
