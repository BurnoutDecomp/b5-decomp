#ifndef GAMESOURCE_DIRECTOR_SHAREDIO_BRN_DIRECTOR_EVENTS_H
#define GAMESOURCE_DIRECTOR_SHAREDIO_BRN_DIRECTOR_EVENTS_H

#include "types.hpp"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h"  // Attribute::Key

// ============================================================================
// GameSource/Director/SharedIO/BrnDirectorEvents.h
//
// Director shared-IO event payloads. The boot path embeds the NewVehicleEvent queue
// in BrnDirectorVehicleInputInterface (an EventQueue<NewVehicleEvent,50>); world->director
// bridging pushes/merges these as cars enter the simulation.
//
// NewVehicleEvent member NAMES/ORDER verbatim from the DecFIGS DWARF
// (BrnDirectorEvents.h:37): Attribute::Key mAttribsKey + int32_t miEntityIndex.
// ----------------------------------------------------------------------------
// The X360 BaseEventQueue<NewVehicleEvent>::AddEvent @0x822C7308 copies each element as
// TWO qwords (std 0(dst); std 8(dst)) at a `16 * miLength` stride, and Append @0x823C2CB8
// XMemCpy's `16 * count` bytes -- i.e. the event STRIDE is 16 bytes. The event is therefore
// 16-byte aligned (the queue's inline maEvents buffer is a SIMD-aligned block).
// alignas(16) reproduces that stride.
//
// ⚠️⚠️ mAttribsKey IS 64 BITS, AND THIS HEADER SAID 32 UNTIL 2026-08-02. The old model put
// mAttribsKey (u32) at +0x00 and miEntityIndex at +0x04. Both ends of the console's own
// traffic disagree, in three independent places:
//   * the PRODUCER, BrnDirectorVehicleInputInterface::NewVehicle @0x822CBA90, stages the
//     event in its frame as `std r30, var_A0(r1)` (the key argument, EIGHT bytes) followed
//     by `stw r21, var_98(r1)` -- var_98 == var_A0 + 8, i.e. the index lands at +0x08;
//   * the CONSUMER, MainDirector::ProcessNewVehicleEvents @0x8221A6B0, block-copies the
//     16-byte element and then reads `ld r28, var_150` for the key and `lwz r22, var_148`
//     (+8) for the index -- and its `cmpldi cr6, r28, 0` is the .cpp:1783
//     "lEvent.mAttribsKey!=0" tripwire, a 64-bit compare;
//   * the key then goes straight into `Attrib::FindCollection(classKey64, r4)` and is
//     latched with `std` into the parameter bank.
// The DecFIGS DWARF agrees at the typedef: attribsys.h:77 `typedef uint64_t HashInt;` and
// :73 `typedef Attribute::HashInt Key;` -- ::Attribute::Key is uint64_t there.
//
// It is spelled `u64` rather than ::Attribute::Key because THIS TREE deliberately typedefs
// ::Attribute::Key to u32 (AttributeKey.h -- it is also the type of 4-byte SERIALISED key
// fields), and the split is already documented at CgsAttribSysCollectionKey.cpp:43 with two
// existing precedents that spell an honestly-64-bit key as u64: BrnVehicleEvents.h's
// RaceCarState::mCarAssetAttribKey and BrnRaceCarAIInterfaces.h's AttachAIControlEvent::
// mCarAssetAttribKey. Adopt ::Attribute::Key here when that typedef split is done.
// ⇒ this was a LATENT truncation, not a live bug: nothing has ever driven the queue. Had it
// been driven, FindCollection hashes the whole doubleword, so every lookup would have missed
// -- the identical defect Attrib::StringToKey had until c27208fc.
// ============================================================================

namespace BrnDirector
{
    // DWARF: BrnDirectorEvents.h:37. A car has entered the simulation: which attribute-system
    // asset it was spawned from (mAttribsKey) and which world entity slot it occupies
    // (miEntityIndex). 16-byte aligned so the queue element stride matches the X360's 16-byte
    // per-event copy (see header note above).
    struct alignas(16) NewVehicleEvent
    {
        // The car's `burnoutcarasset` COLLECTION key -- the hashed
        // VehicleListEntry::AttribSysCollectionKey the producer reads off the vehicle list.
        // 64-bit; see the banner.
        u64 mAttribsKey;       // +0x00  (DWARF :39, Attribute::Key == uint64_t)
        // The console's own assert text for this field is ", model index:" -- the producer
        // passes VehicleList::GetVehicleIndex(modelId), i.e. the vehicle-list slot.
        s32 miEntityIndex;     // +0x08  (DWARF :40)
    };
}

#endif // GAMESOURCE_DIRECTOR_SHAREDIO_BRN_DIRECTOR_EVENTS_H
