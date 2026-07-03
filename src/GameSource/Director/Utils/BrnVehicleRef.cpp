#include "GameSource/Director/Utils/BrnVehicleRef.h"

// Reconstructed BrnDirector::VehicleRef::Construct.
//
// Behaviour-faithful:
//     *(result + 12) = 0;
//     return result;
//
// Zeroes the single owned reference at +0x0C and returns `this`.

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (Set's unimplemented/unknown tripwires)

namespace BrnDirector
{
    VehicleRef* VehicleRef::Construct()
    {
        // (member retype note: the +0x0C slot is the one-byte set-flag -- see the
        //  header's layout note; the old model called it mpRef.)
        mbSet = false;
        return this;
    }

    // @ 0x8252D7F8 (class:BrnDirector::VehicleRef TU) -- bind the reference by kind.
    // Store order per the asm within each case.
    void VehicleRef::Set(EType leType, EActiveRaceCarIndex leRaceCar, u32 luRef)
    {
        switch (leType)
        {
        case E_PLAYER_CAR:
            meType         = E_PLAYER_CAR;
            mbSet          = true;
            muRef          = 0;
            miRaceCarIndex = -1;
            break;

        case E_RACE_CAR:
            SetToRaceCar(leRaceCar);
            break;

        case E_RACE_CAR_NEAREST_PLAYER:
            muRef          = luRef;
            mbSet          = true;
            meType         = E_RACE_CAR_NEAREST_PLAYER;
            miRaceCarIndex = -1;
            break;

        case E_TRAFFIC_VEHICLE:
            CGS_ASSERT(false, "not implemented yet");   // BrnVehicleRef.h:189 (non-gating)
            break;

        default:
            CGS_ASSERT(false, "unknown type");          // BrnVehicleRef.h:195 (non-gating)
            break;
        }
    }

    // The E_RACE_CAR case of Set: bind the reference to a specific race car index
    // (mirrors the E_PLAYER_CAR fill -- populate the four fields, ref slot cleared).
    void VehicleRef::SetToRaceCar(EActiveRaceCarIndex leRaceCar)
    {
        meType         = E_RACE_CAR;
        mbSet          = true;
        miRaceCarIndex = static_cast<s32>(leRaceCar);
        muRef          = 0;
    }

    // @ 0x821F2A38 (class:BrnDirector::VehicleRef TU) -- memberwise inequality over
    // the four fields (type, race car, set-flag, ref slot -- the asm's compare order).
    bool VehicleRef::operator!=(const VehicleRef& lrOther) const
    {
        if (meType != lrOther.meType
            || miRaceCarIndex != lrOther.miRaceCarIndex
            || mbSet != lrOther.mbSet
            || muRef != lrOther.muRef)
        {
            return true;
        }
        return false;
    }
}
