// BrnTraffic::MakeTrafficEntityId @ 0x827048C0
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (store-for-store; see BrnTrafficConstants.h for
// the asm trace). Packs a traffic vehicle index into a scene EntityId with the traffic owner
// (2) in the high byte and the index in a 14-bit field above a 10-bit (always-zero) part field.
//
// Called by 9 sites across the traffic entity module (UpdateParams_TryStartSympatheticCrashing,
// UpdateParams_TryAvoidCrashing, CrashVehicleForSympatheticCrashState, KillDyingVehicleEntity,
// HandleExternalResponses, TrafficToRaceCarInterface_PreScene::AddPotentialStompee, ...).

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT

namespace BrnTraffic
{
    // X360-authoritative constants (asm immediates @ 0x827048C0):
    //   entityIndex must fit in 14 bits          (cmplwi 0x4000)
    //   part-index field is 10 bits wide         (slwi ...,10)
    //   the traffic owner byte is 2              (oris ...,0x200 -> 0x02000000 == 2 << 24)
    static const u32 KU_NUM_BITS_FOR_ENTITY_NUM   = 14;
    static const u32 KU_TRAFFIC_PART_INDEX_SHIFT  = 10;
    static const u32 KU_TRAFFIC_OWNER_PACKED      = 0x02000000; // owner(2) << 24

    EntityId
    MakeTrafficEntityId(u32 luEntityIndex)
    {
        CGS_ASSERT(luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM),
                   "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");

        EntityId lResult;
        lResult.muValue = (luEntityIndex << KU_TRAFFIC_PART_INDEX_SHIFT) | KU_TRAFFIC_OWNER_PACKED;
        return lResult;
    }
}
