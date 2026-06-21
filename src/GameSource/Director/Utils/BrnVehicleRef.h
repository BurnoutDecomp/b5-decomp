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
// FLAG: the EType enumerator set is a minimal slice -- the only values the ICE
//   movie-player family needs are the two it seeds (0 and 1). The remaining enumerators
//   (and the rest of VehicleRef's method/member set) land when VehicleRef's own type TU
//   recovers them; this header is the place to GROW them. The enumerator names below are
//   placeholders pending that recovery (the two seeded values are E_TYPE_0 / E_TYPE_1).
// ============================================================================

namespace BrnDirector
{
    struct VehicleRef
    {
        // FLAG: minimal/placeholder enumerator set (see header note). Two values are
        // evidenced by the SharedPlaylists seed data; INVALID/COUNT sentinels added per
        // convention. Grow with the real names when VehicleRef's type TU is recovered.
        enum EType
        {
            E_TYPE_INVALID = -1,
            E_TYPE_0       = 0,
            E_TYPE_1       = 1,
            E_TYPE_COUNT   = 2
        };

        u8    mPad0[12];   // +0x00  opaque
        void* mpRef;       // +0x0C  cleared reference

        VehicleRef* Construct();   // body in BrnVehicleRef.cpp

        // Bind this reference to a specific active race car of the given ref type.
        // BrnDirector::ICEWrapper::PlayMovie calls it as Set(refType, raceCar, true).
        // FLAG: declaration-only here -- the body lands with VehicleRef's own TU (the
        // per-TU `cl /c` gate does not link). The trailing bool carries its literal role
        // (the recorded call passes 1). Grow with the real signature when that TU lands.
        void Set(EType leType, EActiveRaceCarIndex leRaceCar, bool lbA);
    };
}

#endif // GAMESOURCE_DIRECTOR_UTILS_BRN_VEHICLE_REF_H
