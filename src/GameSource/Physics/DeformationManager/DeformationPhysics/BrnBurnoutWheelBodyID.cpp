#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnBurnoutBodyPartID.h"

// BrnPhysics::Deformation::BurnoutWheelBodyID::Set @ 0x825C1D40
// Reconstructed from BURNOUT_X360_ARTIST.XEX (asm authoritative). Same packed layout as
// BurnoutBodyPartID::Set, but the deformation owner tag is the wheel tag:
//   player vehicle (owner byte 1) -> 9, otherwise (traffic, owner byte 2) -> 10.
// In the non-player branch the X360 fires an owner tripwire that the owning vehicle is a
// traffic vehicle BEFORE the entity-index / part-index tripwires (asm order):
//   "lOwningVehicleID.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE"
//   (baked ..\..\..\GameSource\Physics/DeformationManager/DeformationPhysics/BrnPhysicalWheel.h:244).
// Store order / widths match the asm exactly (see BurnoutBodyPartID::Set for the field
// derivation). All asserts are non-gating tripwires.

namespace BrnPhysics
{
namespace Deformation
{
    void BurnoutWheelBodyID::Set(u32 luOwningVehicleID, u16 luPartIndex, u16 luSubA, u16 luSubB)
    {
        const u32 luEntityIndex = (luOwningVehicleID >> BurnoutBodyPartIDLayout::KU_ENTITY_INDEX_BASE)
                                & 0x3FFFu;
        const u32 luVehicleOwner = luOwningVehicleID >> BurnoutBodyPartIDLayout::KU_OWNER_BASE;

        u32 luOwnerTag;
        if (luVehicleOwner == BurnoutBodyPartIDLayout::KU_OWNER_PLAYER_VEHICLE)
        {
            luOwnerTag = KU_OWNER_PLAYER_WHEEL;
        }
        else
        {
            CGS_ASSERT(luVehicleOwner == BurnoutBodyPartIDLayout::KU_OWNER_TRAFFIC_VEHICLE,
                       "lOwningVehicleID.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");
            luOwnerTag = KU_OWNER_TRAFFIC_WHEEL;
        }

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
