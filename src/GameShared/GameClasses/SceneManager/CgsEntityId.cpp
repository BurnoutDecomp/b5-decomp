#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

// CgsSceneManager::EntityId::SetEntityIndex, kept out of line here although the original had it
// header-inline (DecFIGS has no CgsEntityId.cpp; ARTIST bakes CgsEntityId.h:160 into its assert and
// inlines it into VolumeInstanceId::SetEntityIDEntityIndex 0x822B0E70, PropEntityID::SetEntityIndex
// 0x822B79D0 and TrafficEntityModule::CleanUpCrashedVehiclePhysics 0x82720960). Its out-of-line
// copy 0x8229FF58 exists because VehicleManager::ProcessShowtimeShunts (0x82629F20) and
// ::HandleRaceCarTrafficCarPotentialContact (0x8263FA50) call it without inlining, and the x360
// tree's versions of those two do not call it yet: back in the header, no object would emit the
// verified copy. Move it next to SetOwner (CgsEntityId.h:157..162) once one of them does.
// Store order and bit-fields are reproduced exactly from the X360 asm.

namespace CgsSceneManager
{
    // 0x8229FF58: SetEntityIndex. cmplwi 0x4000 + blt asserts luEntityIndex < 1<<14.
    // Assert baked at CgsEntityId.h:160. Pack:
    // slwi r10,r30,10 (idx<<KU_ENTITY_INDEX_BASE); rlwinm r11,r11,0,22,7 clears
    // bits [10..23] leaving mask 0xFF0003FF (== ~KU_ENTITY_INDEX_MASK); or; stw ->
    // mId = (mId & ~KU_ENTITY_INDEX_MASK) | (idx << KU_ENTITY_INDEX_BASE).
    void
    EntityId::SetEntityIndex( u32 luEntityIndex )
    {
        CGS_ASSERT( luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM),
            CGS_ASSERT_MESSAGE_AT(
                "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\scenemanager\\CgsEntityId.h", 160 /* ARTIST 0x8229FF84 */));
        mId = ( mId & ~KU_ENTITY_INDEX_MASK )
            | ( (u32)luEntityIndex << KU_ENTITY_INDEX_BASE );
    }
}
