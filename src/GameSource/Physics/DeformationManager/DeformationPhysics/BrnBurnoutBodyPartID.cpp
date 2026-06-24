#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnBurnoutBodyPartID.h"
#include <cstddef>   // offsetof

// X360-attested storage shape: both packed IDs are a single 64-bit word (entity word in
// the high dword, two u16 sub-fields in the low dword). The two `sth` stores at this+4 / +6
// pin the sub-field offsets; the `std` to this+0 pins the 8-byte size.
static_assert(sizeof(BrnPhysics::Deformation::BurnoutBodyPartID) == 8,  "BurnoutBodyPartID == 8 bytes");
static_assert(sizeof(BrnPhysics::Deformation::BurnoutWheelBodyID) == 8, "BurnoutWheelBodyID == 8 bytes");
static_assert(offsetof(BrnPhysics::Deformation::BurnoutBodyPartID, muSubA) == 4, "muSubA @ +4");
static_assert(offsetof(BrnPhysics::Deformation::BurnoutBodyPartID, muSubB) == 6, "muSubB @ +6");

// BrnPhysics::Deformation::BurnoutBodyPartID::Set @ 0x825C1A10
// Reconstructed from BURNOUT_X360_ARTIST.XEX (asm authoritative). Packs the owning
// vehicle's EntityId (owner byte + 14-bit entity index) plus a 10-bit part index into the
// embedded 32-bit entity word, choosing the deformation owner tag from the vehicle owner:
//   player vehicle (owner byte 1) -> 6, otherwise (traffic) -> 7.
// Then stores the two trailing 16-bit sub-fields. Store order/widths match the asm:
//   - entityIndex = (luOwningVehicleID >> 10) & 0x3FFF        (extrwi r30,r4,14,8)
//   - tripwire entityIndex < (1<<14)                          (CgsEntityId.h:117)
//   - tripwire luPartIndex < (1<<10)                          (CgsEntityId.h:117)
//   - entityWord = (entityIndex << 10) | (tag << 24) | partIndex   (slwi/oris/or)
//   - the 64-bit `std` to this+0 places entityWord in the high dword; the two `sth`+`or`
//     splice subA at bytes +4/+5 (<<16 of the low dword) and subB at bytes +6/+7.
// The two asserts are non-gating tripwires (the X360 fires them but proceeds to pack).

namespace BrnPhysics
{
namespace Deformation
{
    void BurnoutBodyPartID::Set(u32 luOwningVehicleID, u16 luPartIndex, u16 luSubA, u16 luSubB)
    {
        const u32 luEntityIndex = (luOwningVehicleID >> BurnoutBodyPartIDLayout::KU_ENTITY_INDEX_BASE)
                                & 0x3FFFu;
        const u32 luVehicleOwner = luOwningVehicleID >> BurnoutBodyPartIDLayout::KU_OWNER_BASE;

        const u32 luOwnerTag = (luVehicleOwner == BurnoutBodyPartIDLayout::KU_OWNER_PLAYER_VEHICLE)
                             ? KU_OWNER_PLAYER_BODY_PART
                             : KU_OWNER_TRAFFIC_BODY_PART;

        CGS_ASSERT(luEntityIndex < (1u << BurnoutBodyPartIDLayout::KU_NUM_BITS_FOR_ENTITY_NUM),
                   "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");
        CGS_ASSERT(luPartIndex < (1u << BurnoutBodyPartIDLayout::KU_NUM_BITS_FOR_PART_NUM),
                   "luPartIndex < (1U << KU_NUM_BITS_FOR_PART_NUM)");

        muEntityWord = (luEntityIndex << BurnoutBodyPartIDLayout::KU_ENTITY_INDEX_BASE)
                     | (luOwnerTag << BurnoutBodyPartIDLayout::KU_OWNER_BASE)
                     | static_cast<u32>(luPartIndex);
        muSubA = luSubA;
        muSubB = luSubB;
    }
}
}
