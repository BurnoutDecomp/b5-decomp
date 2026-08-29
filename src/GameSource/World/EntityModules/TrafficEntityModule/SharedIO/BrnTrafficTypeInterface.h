#pragma once

// Traffic-type query response payload. Reconstructed from the DecFIGS DWARF.
#include "BrnCommonTypes.h"                          // CgsID
#include "SharedClasses/Traffic/BrnTrafficVehicleType.h"  // BrnTraffic::VehicleClass

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    // Response carrying a traffic vehicle's resolved class + type id.
    //
    // ⭐ LAYOUT PINNED 2026-08-29 (showtime score wave), from BOTH ends of the wire, because
    // this record is the last un-landed hop of the showtime "Cars Crashed" chain and the next
    // wave will need it exact:
    //   * PRODUCER -- BrnTraffic::TrafficEntityModule::ProcessTrafficTypeRequests. Absent from
    //     the ARTIST export set (an export hole; the traffic module's own LogMissingLeg already
    //     names it as @0x8272B880), but present in Burnout_External_PS3 @0x4C4C1C with its full
    //     mangled signature -- `(const CgsModule::EventQueue<u16,32>* lpRequestQueue,
    //     CgsModule::EventQueue<TrafficTypeResponse,32>* lpResponseQueue)`. Its body writes
    //     `*(u16*)(e+0) = index`, `*(u32*)(e+4) = class`, `*(u64*)(e+8) = typeId`, stride 16.
    //   * CONSUMER -- BrnGameState::GameStateModule::UpdateShowtimeMode @0x82380EF8 reads the
    //     matched element back with `lhz r4, 0(r31)` / `lwz r5, 4(r31)` / `ld r6, 8(r31)`
    //     (@0x82380FDC..0x82380FEC) straight into
    //     CrashModeScoring::DealWithScoreForVehicleClass(u16, VehicleClass, CgsID, ...).
    // Both agree: u16 @0, two bytes of padding, a 4-byte VehicleClass @4, an 8-byte CgsID @8,
    // sizeof 16 -- which is what natural alignment already gives the declaration below.
    //
    // ⛔ DO NOT TAKE THIS FROM THE PSEUDOCODE. IDA types UpdateShowtimeMode's cursor as
    // `unsigned __int16 *` and therefore prints the two later reads as `*(v12 + 1)` and
    // `*(v12 + 3)` -- i.e. +2 and +6, the padding and the middle of the class word. Transcribing
    // that would hand the scorer a garbage VehicleClass (which indexes maiNumCarsCrashed[4]) and
    // a truncated type id. The asm above is unambiguous; the pseudocode is not.
    struct TrafficTypeResponse
    {
        u16          muVehicleIndex;   // +0   (lhz)
        VehicleClass meType;           // +4   (lwz)  -- 2 bytes of padding precede it
        CgsID        mTypeId;          // +8   (ld)
    };
}
}
