#ifndef GAMESOURCE_DIRECTOR_UTILS_BRN_VEHICLE_REF_H
#define GAMESOURCE_DIRECTOR_UTILS_BRN_VEHICLE_REF_H

#include "types.hpp"
#include "GameSource/BurnoutConstants.h"   // EActiveRaceCarIndex (VehicleRef::Set race car)
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT (Get's five tripwires)
#include "GameSource/Director/Utils/BrnDirectorAllVehicleData.h" // AllVehicleData -- the "world" Get resolves against

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

        // LAYOUT (X360, pinned by Set @0x8252D7F8 / operator!= @0x821F2A38: word @+0,
        // word @+4 (-1 outside the race-car case), word @+8 (the nearest-player ref
        // slot), BYTE @+0xC (lbz/stb)). Supersedes the earlier "12-byte pad + mpRef
        // @+0x0C" guess -- the +0xC field is a one-byte set-flag, not a pointer.
        EType meType;           // +0x00  which reference kind is live
        s32   miRaceCarIndex;   // +0x04  the bound race car (-1 outside E_RACE_CAR)
        u32   muRef;            // +0x08  the nearest-player ref slot (E_RACE_CAR_NEAREST_PLAYER)
        bool  mbSet;            // +0x0C  reference-populated flag

        VehicleRef* Construct();   // body in BrnVehicleRef.cpp

        // ⭐ Get @0x822335A0 -- resolve the reference to the live vehicle record, BODIED INLINE
        // HERE because that is where the CONSOLE had it: its five asserts all cite
        // BrnVehicleRef.h (:121, :144, :150, :155), exactly like the already-inline IsValid.
        // ⚠️ MOUNT HAZARD (measured): bodying this out of line in BrnVehicleRef.cpp -- which IS
        // on the build list -- emits an unresolved external for every AllVehicleData accessor
        // it reaches, in EVERY link, whether or not anything calls Get. Inline, the body
        // materialises only where a caller needs it, and those accessors are themselves header
        // inlines now (see BrnDirectorAllVehicleData.h).
        // ⚠️ TRANSCRIPTION TRAP: in the asm, case E_PLAYER_CAR branches PAST the :155
        // "this shouldn't happen" assert straight onto the same GetPlayer tail the fall-out
        // path uses -- that is MSVC TAIL-MERGING two returns, NOT a fall-through. Written here
        // as an early return so no assert is skipped that the console does not skip.
        const Camera::VehicleInfo* Get(const void* lpWorld) const
        {
            CGS_ASSERT(mbSet, "mbSet");                                   // BrnVehicleRef.h:121

            const AllVehicleData& lrWorld = *static_cast<const AllVehicleData*>(lpWorld);

            switch (meType)
            {
            case E_PLAYER_CAR:
                return &lrWorld.GetPlayer();

            case E_RACE_CAR:
                return &lrWorld.GetRaceCar(static_cast<EActiveRaceCarIndex>(miRaceCarIndex));

            case E_RACE_CAR_NEAREST_PLAYER:
                return &lrWorld.GetRaceCar(lrWorld.GetNearestRaceCarIndexToPlayer(muRef));

            case E_TRAFFIC_VEHICLE:
                CGS_ASSERT(false, "not implemented yet");                 // BrnVehicleRef.h:144
                break;

            default:
                CGS_ASSERT(false, "unknown type");                        // BrnVehicleRef.h:150
                break;
            }

            CGS_ASSERT(false, "this shouldn't happen");                   // BrnVehicleRef.h:155
            return &lrWorld.GetPlayer();
        }

        // @0x8252D7F8 (class TU; body in BrnVehicleRef.cpp) -- bind the reference.
        // The trailing word is stored only for E_RACE_CAR_NEAREST_PLAYER (the X360
        // ICEWrapper::PlayMovie call site passes 1 there).
        void Set(EType leType, EActiveRaceCarIndex leRaceCar, u32 luRef);

        // Bind to a specific race car. Its own ledger function (declaration-only;
        // the BehaviourIceAnim named-setter note records the de-inlined call shape).
        void SetToRaceCar(EActiveRaceCarIndex leRaceCar);

        // @0x821F2A38 (class TU; body in BrnVehicleRef.cpp) -- memberwise inequality.
        bool operator!=(const VehicleRef& lrOther) const;
    };
}

#endif // GAMESOURCE_DIRECTOR_UTILS_BRN_VEHICLE_REF_H
