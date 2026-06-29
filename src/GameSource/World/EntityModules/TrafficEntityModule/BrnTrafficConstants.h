#pragma once

// =============================================================================
// BrnTrafficConstants.h  (NEW OWNING HEADER -- partial: traffic entity-id helper)
//
// DWARF home (references/DecFIGS/dwarfdump/GameSource/World/EntityModules/
// TrafficEntityModule/BrnTrafficConstants.h) of the BrnTraffic subsystem's shared
// constants and the small id-packing free functions. This slice owns ONLY the
// traffic entity-id factory:
//
//   BrnTraffic::MakeTrafficEntityId(u32)  (DWARF :147, body @ 0x827048C0)
//
// The (large) remainder of BrnTrafficConstants.h -- the KU_MAX_* traffic-count
// constants, KF_JUNCTION_* tuning floats, MakeTrafficVolumeInstanceId, etc. --
// belongs to not-yet-reconstructed slices; when they land they should GROW this
// header additively, never redefine MakeTrafficEntityId.
// =============================================================================

#include "types.hpp"        // u32
#include "BrnCommonTypes.h" // EntityId (32-bit packed scene-entity handle)

namespace BrnTraffic
{
    // Pack a traffic vehicle's index into a scene EntityId.
    //
    // GROUND TRUTH (X360 asm @ 0x827048C0, store-for-store):
    //   slwi  r11, entityIndex, 10     ; entityIndex << 10
    //   oris  r11, r11, 0x200          ; OR in (0x200 << 16) == 0x02000000 == owner(2) << 24
    //   stw   r11, 0(result)
    // guarded by  cmplwi entityIndex, 0x4000 (assert luEntityIndex < (1U << 14)).
    //
    // So a traffic EntityId is laid out owner:[31..24]=2, entityIndex:[23..10] (14 bits),
    // partIndex:[9..0]=0. NOTE: this is the traffic subsystem's own 14/10 entity/part split
    // (asm-authoritative); it is intentionally distinct from CgsSceneManager::EntityId's
    // committed 8/12/12 split (the helper packs the raw u32 directly, it does not call
    // EntityId::Set). The constants below are grounded entirely by the asm immediates.
    EntityId MakeTrafficEntityId(u32 luEntityIndex);
}
