#ifndef BRN_VEHICLE_SOA_DATA_H
#define BRN_VEHICLE_SOA_DATA_H

#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"

namespace BrnTraffic
{
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827xxxxx.
// Structure-of-arrays traffic vehicle data; Construct zeroes every bit set.
// Member names/types per burnout.wiki (World Module / Traffic Entity Module ->
// BrnTraffic::VehicleSoaData): eight FastBitArray sets, each 0x50 bytes. Layout
// (8 x 0x50 = 0x280) verified against the X360 pseudocode's quadword clears.
//
// [!] KU_MAX_VEHICLES CORRECTED 600 <- 601, 2026-08-29 (jam-valve wave). The 601 came from
// the wiki page; the wiki is name-authoritative and NEVER count/offset-authoritative, and the
// image disagrees. KillAllTrafficInCylinder @0x82741C58 bounds-checks a vehicle index for
// these very sets and bakes `cmplwi r29, 0x258` (== 600) @0x82741CB4 under the literal assert
// string "luIndex < KU_MAX_TOTAL_TRAFFIC" -- and BrnTrafficConstants.h already derives
// KU_MAX_TOTAL_TRAFFIC == 600 (400 standard + 199 static + 1 trailer) from the same 0x258.
// NukeTrafficJams @0x827353E8 bakes the identical 0x258 four more ways for the param sets
// (see BrnTrafficParam.h's KU_PARAM_MAX_PARAMS banner).
// ⭐ THE COMPILE GATE PROVED THE PAIRING, not just the value: with only the param constant
// corrected, _wT2_02.cpp failed to build because it passes mVehicleSoaData sets straight into
// FastBitArray<KU_PARAM_MAX_PARAMS>::SetInverse / SetAnd. The committed code ALREADY requires
// these two constants to be the same number, so 601-here/600-there could never both be right.
// Field count is unchanged ((600+63)/64 == 10 == (601+63)/64), so the 0x50 stride and every
// committed +164560/+164640/... member offset are untouched.
struct VehicleSoaData
{
    static const u32 KU_MAX_VEHICLES = 600;

    CgsContainers::FastBitArray<KU_MAX_VEHICLES> mAliveVehicles;
    CgsContainers::FastBitArray<KU_MAX_VEHICLES> mVehiclesWithEntities;
    CgsContainers::FastBitArray<KU_MAX_VEHICLES> mCollidableVehicles;
    CgsContainers::FastBitArray<KU_MAX_VEHICLES> mPhysicalVehicles;
    CgsContainers::FastBitArray<KU_MAX_VEHICLES> mArticulatedVehicles;
    CgsContainers::FastBitArray<KU_MAX_VEHICLES> mVehiclesRenderedLastFrame;
    CgsContainers::FastBitArray<KU_MAX_VEHICLES> mPhysicalVehiclesFarFromPlayer;
    CgsContainers::FastBitArray<KU_MAX_VEHICLES> mPhysicalVehiclesTryingToRecover;

    void Construct();
};
}

#endif
