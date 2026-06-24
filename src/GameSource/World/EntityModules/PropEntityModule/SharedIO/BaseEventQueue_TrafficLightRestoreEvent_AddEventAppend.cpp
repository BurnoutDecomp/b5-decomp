#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                          // BaseEventQueue<T>::AddEvent/Append (inline generic)
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"   // BrnWorld::PropEntityIO::TrafficLightRestoreEvent (4-byte element)

// CgsModule::BaseEventQueue<BrnWorld::PropEntityIO::TrafficLightRestoreEvent>::AddEvent @ 0x822C8EB0
// CgsModule::BaseEventQueue<BrnWorld::PropEntityIO::TrafficLightRestoreEvent>::Append   @ 0x827A8730
//   (ledger id: class:BrnWorld::PropEntityIO::TrafficLightRestoreEvent>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match). The generic
// BaseEventQueue<T>::AddEvent / ::Append bodies are already committed inline in
// CgsBaseEventQueue.h; this TU is the thin explicit instantiation that forces the per-element
// out-of-line emission the X360 build produced. The 4-byte element stride is exactly the one
// the committed TrafficLightRestoreEvent struct documents and EventQueue<...,80>::Construct
// @ 0x822E4D50 asserts (inline maEvents[80] at +0xC, no alignment pad).
//
// X360 store-for-store:
//   AddEvent (@0x822C8EB0): asserts mpEvents != NULL (CgsBaseEventQueue.h:312, `lwz r11,0(this)`)
//     and miLength < miMaxLength ("...Reached Max length", :313 -- the bge-to-assert path falls
//     through and still appends; not a gate). Then appends UNCONDITIONALLY:
//       `lwz idx,8(this); slwi idx,idx,2; lwz buf,0(this); lwz v,0(src); stwx v, idx, buf`
//       -- a single 4-byte (stw) copy to mpEvents[miLength] at stride 4 (`slwi ...,2`), confirming
//       sizeof(TrafficLightRestoreEvent) == 4. Then `++miLength`
//       (`lwz r11,8(this); addi r11,r11,1; stw r11,8(this)`); returns 1 (true).
//   Append (@0x827A8730): asserts mpEvents != NULL (:413), no-overflow
//     (`lwz srcLen,8(src); add srcLen,srcLen,miLength; cmpw vs miMaxLength`, "Base event queue
//     overflow", :414) and source owns a buffer (:486). Then XMemCpy's `4 * srcLen` bytes onto the
//     tail (asm `slwi count,srcLen,2` == 4*count for the byte count, dst at `4*miLength + mpEvents`)
//     and advances `miLength += src.miLength`; returns 1 (true).
//
// The Hex-Rays `int AddEvent(_DWORD*, _DWORD*)` prototype is the ABI shape of
// `bool AddEvent(this, const TrafficLightRestoreEvent&)`; r3 (this) / r4 (event ref). The
// `int Append(_DWORD*, _DWORD*)` is `bool Append(this, const BaseEventQueue<T>&)`.
// Callers (X360): AddEvent <- BrnWorld::PropZoneManager::SendTrafficLightRestoreEvents;
//                 Append   <- WorldModule::BridgePropModuleToTrafficModule_PrePhysics.
template bool
CgsModule::BaseEventQueue<BrnWorld::PropEntityIO::TrafficLightRestoreEvent>::AddEvent(
    const BrnWorld::PropEntityIO::TrafficLightRestoreEvent&);

template bool
CgsModule::BaseEventQueue<BrnWorld::PropEntityIO::TrafficLightRestoreEvent>::Append(
    const CgsModule::BaseEventQueue<BrnWorld::PropEntityIO::TrafficLightRestoreEvent>&);
