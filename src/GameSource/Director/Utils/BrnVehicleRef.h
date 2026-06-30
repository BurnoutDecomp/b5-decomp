#ifndef GAMESOURCE_DIRECTOR_UTILS_BRN_VEHICLE_REF_H
#define GAMESOURCE_DIRECTOR_UTILS_BRN_VEHICLE_REF_H

#include "types.hpp"
#include "GameSource/BurnoutConstants.h"   // EActiveRaceCarIndex (VehicleRef::Set race car)

// ============================================================================
// GameSource/Director/Utils/BrnVehicleRef.h
//
// BrnDirector::VehicleRef -- a director-side reference to a vehicle (the car a camera
// take is anchored to). HOME for the VehicleRef struct (its Construct body stays in
// BrnVehicleRef.cpp, which should now #include this header instead of redeclaring the
// struct) and for its EType enum.
//
// The EType enumerator set is recovered from the PS3 DecFIGS DWARF and independently
//   confirmed by the Feb-2007 BrnEntityModuleUnity reference header (identical names +
//   values). The X360 target seeds slot 0 (player) and 1 (race car); the remaining
//   enumerators (RACE_CAR_NEAREST_PLAYER, TRAFFIC_VEHICLE) complete the set.
// ============================================================================

namespace BrnDirector
{
    struct VehicleRef
    {
        // Enumerator names from PS3 DecFIGS DWARF + Feb-2007 reference header (Get()/Set()
        // switch over these). X360 seeds E_PLAYER_CAR (0) and E_RACE_CAR (1).
        enum EType
        {
            E_PLAYER_CAR              = 0,
            E_RACE_CAR                = 1,
            E_RACE_CAR_NEAREST_PLAYER = 2,
            E_TRAFFIC_VEHICLE         = 3,
            E_NUM_TYPES               = 4
        };

        u8    mPad0[12];   // +0x00  opaque
        void* mpRef;       // +0x0C  cleared reference

        VehicleRef* Construct();   // body in BrnVehicleRef.cpp

        // Resolve the reference to a live vehicle object given the world context.
        // FLAG: declaration-only (body in BrnVehicleRef.cpp); signature from X360 pseudo.
        void* Get(const void* lpWorld) const;

        // Bind this reference to a specific active race car of the given ref type.
        // BrnDirector::ICEWrapper::PlayMovie calls it as Set(refType, raceCar, true).
        // FLAG: declaration-only here -- the body lands with VehicleRef's own TU (the
        // per-TU `cl /c` gate does not link). The trailing bool carries its literal role
        // (the recorded call passes 1). Grow with the real signature when that TU lands.
        void Set(EType leType, EActiveRaceCarIndex leRaceCar, bool lbA);
    };
}

#endif // GAMESOURCE_DIRECTOR_UTILS_BRN_VEHICLE_REF_H
