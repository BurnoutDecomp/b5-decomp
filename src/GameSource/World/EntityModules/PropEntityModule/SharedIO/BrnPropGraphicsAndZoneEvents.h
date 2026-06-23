#pragma once

// BrnWorld::PropEntityIO graphics/zone notification event element types.
//
// Three sibling notification events the prop-entity IO publishes to drive prop
// streaming: a prop's graphics became loaded, became unloaded, and the set of prop
// instances a zone needs. Each carries a single 2-byte id payload.
//
// SIZE (X360, authoritative): sizeof == 2 for all three. Pinned by the per-element
// BaseEventQueue<T>::AddEvent bodies, which copy the event with
//   `lhz r10, 0(src); slwi idx,idx,1; sthx r10, idx, mpEvents`
// -- a single halfword at stride 2 (index << 1, no wider scaling):
//   PropGraphicsLoadedEvent>::AddEvent          @ 0x822C9120
//   PropGraphicsUnloadedEvent>::AddEvent        @ 0x822C9258
//   PropInstancesNeededForZoneEvent>::AddEvent  @ 0x822C8FE8
// The matching EventQueue<T,N>::Construct TUs place the inline maEvents[N] at +0xC
// (immediately past the 12-byte BaseEventQueue header {T* @0, s32 @4, s32 @8}, with no
// alignment pad), confirming the element's alignment is 2:
//   PropGraphicsLoadedEvent,25>::Construct          @ 0x822E4E30  (maxLen 25)
//   PropGraphicsUnloadedEvent,25>::Construct        @ 0x822E4EA0  (maxLen 25)
//   PropInstancesNeededForZoneEvent,30>::Construct  @ 0x822E4DC0  (maxLen 30)
//
// The DWARF homes these types in BrnPropEntityModuleIO.h, but that file's current slice
// is scoped to PropVFXLocatorEvent / BrokenPropEvent / OutputBuffer_PreScene (owned by
// other groups). To avoid editing a foreign group's file, the element types are homed
// here in the prop SharedIO area, in their canonical namespace; nothing in
// BrnPropEntityModuleIO.h is touched. Adopt the canonical home additively if/when the
// owning slice lands.
//
// The single 2-byte field is the loaded/unloaded prop's graphics-instance id (for the
// load/unload events) and the zone's id (for the instances-needed event). The exact
// field semantics beyond "a 16-bit id" are not separately recoverable from this slice,
// so each is modelled as one named u16; the type is exactly two bytes wide as the asm
// attests (single lhz/sthx, stride 2).

#include "types.hpp"   // u16

namespace BrnWorld
{
namespace PropEntityIO
{
    // Published when a prop's graphics finish loading; carries the prop graphics id.
    struct PropGraphicsLoadedEvent
    {
        u16 muPropGraphicsId;   // +0x00  (single 2-byte payload; asm lhz/sthx stride 2)
    };

    // Published when a prop's graphics are unloaded; carries the prop graphics id.
    struct PropGraphicsUnloadedEvent
    {
        u16 muPropGraphicsId;   // +0x00  (single 2-byte payload; asm lhz/sthx stride 2)
    };

    // Published to request the prop instances a zone needs; carries the zone id.
    struct PropInstancesNeededForZoneEvent
    {
        u16 muZoneId;           // +0x00  (single 2-byte payload; asm lhz/sthx stride 2)
    };
}
}
