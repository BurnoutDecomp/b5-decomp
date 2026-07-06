// BrnWorld::NearMissManager -- the per-race-car near-miss / crash / takedown chain tracker that
// drives boost rewards for close passes. DWARF home
// GameSource/World/EntityModules/RaceCarEntityModule/NearMisses/BrnNearMissManager.h.
//
// Owns two NearMissData bookkeeping sub-objects: mTrafficNearMissData (NearMissData<4,8>) and
// mRaceCarNearMissData (NearMissData<4,7>). Layout, member NAMES/types/order and the method shape
// are X360-authoritative from the DWARF dump (references/DecFIGS/dwarfdump/.../BrnNearMissManager.h);
// byte offsets are pinned by the sub-object sizes and the asm:
//   +0x00  mfSpeed               f32
//   +0x04  mfNearMissTimeout     f32
//   +0x08  miNearMissCount       s32
//   +0x0C  mTrafficNearMissData  NearMissData<4,8>  (size 0x13C: maChecked@+0xF8 + 8*8+4)
//   +0x148 mRaceCarNearMissData  NearMissData<4,7>  (size 0x124: maChecked@+0xE8 + 7*8+4)
//   +0x26C mbFailedNearMissChain bool
//   +0x26D mbCrashing            bool
// mTrafficNearMissData@+0x0C is proven by HasThereBeenARecentNearMiss (@0x822CD2E8): it reads that
// sub-object's maNearMiss count word at this+0x54 (0x0C + 0x28 + 0x20 == 0x54) and then calls
// NearMissData<4,7>::HasThereBeenARecentNearMiss on this+0x148 (0x0C + 0x13C == 0x148).
//
// Only HasThereBeenARecentNearMiss() is reconstructed in this batch; the remaining methods
// (Prepare/SetSpeed/Update/the Add* loggers/NearMissEvent) are DWARF-attested but are left out of
// this minimal home because their signatures name types (OutputBuffer_PrePhysics::GameEventQueue,
// ENearMissType, BoostManager) whose homes are not yet reconstructed. They are added additively
// when those payload TUs land.
#pragma once

#include "types.hpp"                                                                    // f32 / s32 / u32 / bool
#include "GameSource/World/EntityModules/RaceCarEntityModule/NearMisses/BrnNearMissData.h" // NearMissData<A,B>

namespace BrnWorld
{
    // Capacities the DWARF attests (BrnNearMissManager.h:45-47).
    const u32 KU_MAX_NEAR_VEHICLES                          = 4;
    const u32 KU_MAX_CRASHING_TRAFFIC_IN_VICINITY_OF_PLAYER = 8;
    const u32 KU_MAX_CRASHING_OTHER_RACE_CARS               = 7;

    struct NearMissManager
    {
    public:
        // True iff either remembered near-miss list is currently non-empty. @0x822CD2E8.
        bool HasThereBeenARecentNearMiss() const;

        // Log a race-car / traffic section-entity id into the matching current-frame near list.
        // Both guard with a manager-level not-already-contained debug assert, then defer to the
        // NearMissData<A,B>::AddNear length<cap append (inlined by the X360). @0x822ED198/@0x822ED250.
        void AddNearRaceCar(u32 luRaceCarId);
        void AddNearTraffic(u32 luEntityId);

    private:
        // DWARF-named typedefs (BrnNearMissManager.h:49-50).
        typedef NearMissData<4, 8> TrafficNearMissData;
        typedef NearMissData<4, 7> RaceCarNearMissData;

        f32                 mfSpeed;               // +0x00
        f32                 mfNearMissTimeout;     // +0x04
        s32                 miNearMissCount;       // +0x08
        TrafficNearMissData mTrafficNearMissData;  // +0x0C  (NearMissData<4,8>, size 0x13C)
        RaceCarNearMissData mRaceCarNearMissData;  // +0x148 (NearMissData<4,7>, size 0x124)
        bool                mbFailedNearMissChain; // +0x26C
        bool                mbCrashing;            // +0x26D
    };
}
