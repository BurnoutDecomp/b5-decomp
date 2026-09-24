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

        // ⭐ IsValid -- does this reference currently resolve to a LIVE race car? DE-FORKED
        // HERE: this is the console's own VehicleRef::IsValid, a method of THIS struct, and the
        // single copy of it. It used to live only as a slice on the behaviour-side derived
        // Camera::Behaviour::VehicleRef, taking the world as an opaque pointer; that copy is
        // gone and the derived class now pulls this one in with a using-declaration, so there
        // is exactly one body tree-wide.
        //
        // The console shape:
        //     if (!mbSet) return false;
        //     switch (meType) {
        //       case E_PLAYER_CAR:              index = world.mePlayerRaceCarIndex;   // +0xC4
        //       case E_RACE_CAR:                index = miRaceCarIndex;
        //       case E_RACE_CAR_NEAREST_PLAYER: index = world.GetNearestRaceCarIndexToPlayer(muRef);
        //       case E_TRAFFIC_VEHICLE:         assert("not implemented yet"); return false;
        //       default:                        assert("unknown type");        return false;
        //     }
        //     assert(index < 8);                              // the bit-array tripwire
        //     return world.mUsedRaceCars.IsBitSet(index);     // +0xC8
        // The two displacements land exactly on the committed AllVehicleData members
        // (mePlayerRaceCarIndex @+0xC4 == 196, mUsedRaceCars @+0xC8 == 200), which the
        // disassembly reaches as world+196 and world + 8*((index>>6)+25).
        //
        // ⚠ The used-race-car BIT is the console's only guard against a reference to a car
        // that was never spawned -- and it is a guard on the bit, not on the vehicle data, so a
        // car whose bit is set but whose record is zeroed still passes here on the console too.
        //
        // INLINE deliberately, for the same mount hazard Get carries above: an out-of-line body
        // in BrnVehicleRef.cpp -- which IS on the build list -- would emit an unresolved
        // external for every AllVehicleData accessor it reaches in EVERY link, whether or not
        // anything calls IsValid. Inline, the body materialises only where a caller needs it.
        bool IsValid(const AllVehicleData& lrWorld) const
        {
            if (!mbSet)
            {
                return false;
            }

            EActiveRaceCarIndex leIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;

            switch (meType)
            {
            case E_PLAYER_CAR:
                leIndex = lrWorld.GetPlayerRCIndex();
                break;

            case E_RACE_CAR:
                leIndex = static_cast<EActiveRaceCarIndex>(miRaceCarIndex);
                break;

            case E_RACE_CAR_NEAREST_PLAYER:
                leIndex = lrWorld.GetNearestRaceCarIndexToPlayer(muRef);
                break;

            case E_TRAFFIC_VEHICLE:
                CGS_ASSERT(false, "not implemented yet");    // non-gating
                return false;

            default:
                CGS_ASSERT(false, "unknown type");           // non-gating
                return false;
            }

            CGS_ASSERT(static_cast<u32>(leIndex) < 8u, "invalid index");   // non-gating

            return lrWorld.GetUsedRaceCarsBitArray().IsBitSet(static_cast<u32>(leIndex));
        }

        // @0x8252D7F8 (class TU; body in BrnVehicleRef.cpp) -- bind the reference.
        // The trailing word is stored only for E_RACE_CAR_NEAREST_PLAYER (the X360
        // ICEWrapper::PlayMovie call site passes 1 there).
        void Set(EType leType, EActiveRaceCarIndex leRaceCar, u32 luRef);

        // DWARF h:76 -- bind to the player's car. No out-of-line console symbol: inlined at every
        // site as the four stores Set's E_PLAYER_CAR case makes (@0x8252D838..0x8252D850), e.g. the
        // tail of BehaviourBystanderCam::Construct (@0x82243C68..0x82243C74: +0xC = 1, +0 = 0,
        // +8 = 0, +4 = -1).
        void SetToPlayer()
        {
            meType         = E_PLAYER_CAR;
            mbSet          = true;
            muRef          = 0;
            miRaceCarIndex = -1;
        }

        // DWARF h:80 -- bind to a specific race car. @0x821F29D8 (body in BrnVehicleRef.cpp; its
        // index assert cites BrnVehicleRef.h:222).
        void SetToRaceCar(EActiveRaceCarIndex leRaceCar);

        // @0x821F2A38 (class TU; body in BrnVehicleRef.cpp) -- memberwise inequality.
        bool operator!=(const VehicleRef& lrOther) const;
    };
}

#endif // GAMESOURCE_DIRECTOR_UTILS_BRN_VEHICLE_REF_H
