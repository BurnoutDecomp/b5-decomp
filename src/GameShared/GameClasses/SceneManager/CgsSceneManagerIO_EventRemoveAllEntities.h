#pragma once

#include "types.hpp"   // u8 (the mu8Owner payload below)

// Minimal owning home for the SceneManager "remove all entities" event element
//   CgsSceneManager::SceneManagerIO::InEventRemoveAllEntities
// -- the per-event payload stored in the EventQueue<InEventRemoveAllEntities, N>
// remove-all-entities input queue that InSceneUpdateInterface embeds by value. This
// header exists so the explicit-instantiation TUs for that queue's Construct
// (EventQueue_InEventRemoveAllEntities_1.cpp) and the base queue's Append
// (BaseEventQueue_InEventRemoveAllEntities_Append.cpp) can see a COMPLETE element type
// (the queue embeds InEventRemoveAllEntities maEvents[N] inline, and the base queue's
// Append block-copies sizeof(T)-strided records). The full InSceneUpdateInterface
// aggregate keeps its own placeholder slice and its own ledger TU; this header only adds
// the leaf element it queues, by name.
//
// SIZE / ALIGNMENT (X360-attested):
//   * Construct @ 0x822E2720 does `addi r30, this, 0xC` -- the 12-byte BaseEventQueue
//     header needs no padding, so maEvents lives at +0xC (the element is byte aligned).
//   * Append @ 0x827A6E20 strides its block-copy by `count` UNSCALED -- no `slwi` shift
//     (`r5 = lSource.miLength`, `r3 = mpEvents + miLength` with no shift on miLength).
//   => sizeof(InEventRemoveAllEntities) == 1 (a payload-less marker event); the "remove
//      all entities" request carries no data, only its presence in the queue.
//
// LAYOUT: a single-byte payload. ⚠️ CORRECTED 2026-08-12 (prop-spawn link-closure pass) --
// this was previously modelled as a payload-less marker, which is why the one producer
// (PropZoneManager::RemoveAllPropsAndParts) had to carry a FLAG saying its payload byte
// "has nowhere to go". It does have somewhere to go:
//   * DecFIGS DWARF (CgsSceneManagerIO_SceneUpdate.h:63/65) declares
//         struct InEventRemoveAllEntities : public Event { uint8_t mu8Owner; };
//     and the matching producer as `void RemoveAllEntities(uint8_t)`.
//   * The X360 confirms both: RemoveAllPropsAndParts @0x822DEF50 inlines the producer at
//     0x822DF020-38 as
//         GetSceneInputInterface() -> +0xC7E3C -> BaseEventQueue<T>::AddEvent() -> stb 3
//     and 3 is E_ENTITYTYPE_PROP -- i.e. "remove every entity OWNED BY PROPS", which is
//     exactly what that function is for. A payload-less marker could not express that.
// LAYOUT-NEUTRAL: an empty struct and a struct holding one u8 are both sizeof 1 / align 1
// (the empty base is elided), so the asm-attested 1-byte Append stride and the +0xC
// maEvents offset are unchanged, and both explicit-instantiation TUs are unaffected. The
// static_assert below is the tripwire for that.

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image). Distinctly named so this leaf element home never ODR-clashes
    // with the SceneManagerIO::Event defined by the other per-element queue homes.
    struct EventBaseRemoveAllEntities {};

    // EventQueue<InEventRemoveAllEntities, N> element (DWARF CgsSceneManagerIO_SceneUpdate.h:63).
    // sizeof 1, 1-byte stride -- exactly the asm-attested Append stride.
    struct InEventRemoveAllEntities : public EventBaseRemoveAllEntities
    {
        // DWARF :65. The entity-type owner whose entities are to be dropped; the sole
        // observed value is E_ENTITYTYPE_PROP (3), stored by the `stb r10, 0(r11)` at
        // 0x822DF038.
        u8 mu8Owner;
    };
    static_assert(sizeof(InEventRemoveAllEntities) == 1,
                  "InEventRemoveAllEntities stride 1 (asm: Append strides by count UNSCALED)");
}
}
