#pragma once

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
// LAYOUT: a payload-less marker (an empty event whose presence triggers the bulk remove).
// C++ gives an empty struct sizeof 1, exactly the asm-attested 1-byte stride. Construct
// does not read the element and Append block-copies it whole, so only the size/alignment
// is load-bearing.

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image). Distinctly named so this leaf element home never ODR-clashes
    // with the SceneManagerIO::Event defined by the other per-element queue homes.
    struct EventBaseRemoveAllEntities {};

    // EventQueue<InEventRemoveAllEntities, N> element. A payload-less marker event:
    // sizeof 1 (empty struct), 1-byte stride -- exactly the asm-attested Append stride.
    struct InEventRemoveAllEntities : public EventBaseRemoveAllEntities {};
}
}
