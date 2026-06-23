#pragma once

// BrnAI::RouteMapModuleIO -- the IO surface for the AI route-map module. The
// module's input buffer carries route-request queues (RaceRouteRequest,
// ExtrapolatedRouteRequest) and its output buffer carries a RouteResponse queue.
// Each element type derives from the empty CgsModule::Event base (EBO -> 0 bytes)
// and is stored by its byte image in a fixed-capacity CgsModule::EventQueue<T,N>.
//
// X360-attested element strides + queue offsets (all from BURNOUT_X360_ARTIST.XEX):
//   * RaceRouteRequest         stride 128 (AddEventSafe @0x8277AF20: `slwi r,r,7`,
//                              16 x `std` 8-byte stores) -> 8-byte aligned.
//                              EventQueue<...,1>::Construct @0x82789FB8 puts maEvents
//                              at this+0x10 (12-byte base + 4 pad for align-8), N=1.
//   * ExtrapolatedRouteRequest stride 64  (AddEventSafe @0x8277AFD8: `slwi r,r,6`,
//                              8 x `std` 8-byte stores) -> 8-byte aligned.
//                              EventQueue<...,12>::Construct @0x8278A028 puts maEvents
//                              at this+0x10, N=12 (`li r11,0xC`).
//   * RouteResponse            stride 5136 (AddEvent @0x8277B090: memcpy 5136 bytes;
//                              Append @0x8277B668: XMemCpy 5136*count) -> 4-byte
//                              aligned. EventQueue<...,16>::Construct @0x8278A098 puts
//                              maEvents at this+0xC (12-byte base, element align 4),
//                              N=16 (`li r11,0x10`).
//
// FLAG (no DWARF / no leak for these element internals): the field-level layout of
// the three route-request/response records is NOT recoverable from the available
// data -- only their X360-attested byte size and alignment are known (from the
// element-stride arithmetic in the queue Construct/AddEvent/Append bodies). They are
// modelled here as honest opaque byte payloads sized + aligned to match the X360
// strides exactly, so the EventQueue<T,N> instantiations land maEvents at the
// X360 offsets. The internal members must be filled in once a DWARF/decl source for
// these types is homed; that does not change these sizes.

#include "types.hpp"                                            // u8/u32/u64/s32
#include "GameShared/GameClasses/Module/CgsEventQueue.h"        // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h" // CgsModule::Event (empty event base)

namespace BrnAI
{
    namespace RouteMapModuleIO
    {
        // Input request: a full race-route query. X360 stride 128, 8-byte aligned
        // (AddEventSafe @0x8277AF20 copies 16 qwords). Opaque payload pending DWARF.
        struct RaceRouteRequest : public CgsModule::Event
        {
            u64 maOpaque[16];   // 128 bytes, 8-byte aligned (X360-attested size only)
        };

        // Input request: an extrapolated (look-ahead) route query. X360 stride 64,
        // 8-byte aligned (AddEventSafe @0x8277AFD8 copies 8 qwords). Opaque payload.
        struct ExtrapolatedRouteRequest : public CgsModule::Event
        {
            u64 maOpaque[8];    // 64 bytes, 8-byte aligned (X360-attested size only)
        };

        // Output response: a computed route. X360 stride 5136, 4-byte aligned
        // (AddEvent @0x8277B090 / Append @0x8277B668 block-copy 5136 bytes). The
        // u32 element keeps the natural alignment at 4 so maEvents lands at +0xC.
        struct RouteResponse : public CgsModule::Event
        {
            u32 maOpaque[1284]; // 5136 bytes, 4-byte aligned (X360-attested size only)
        };
    }
}
