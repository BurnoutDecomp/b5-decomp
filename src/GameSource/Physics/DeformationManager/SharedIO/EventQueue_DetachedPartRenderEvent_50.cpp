#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationEvents.h"  // DetachedPartRenderEvent

// CgsModule::BaseEventQueue<BrnPhysics::Deformation::DetachedPartRenderEvent> methods
// (bodies inline in CgsBaseEventQueue.h; this TU is the per-instantiation ledger .cpp).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The deformation system fills a fixed-capacity
// EventQueue<DetachedPartRenderEvent, 50> with per-part render transforms each frame; the
// render side merges them. The two attested out-of-line bodies for this instantiation match
// the committed generic template bodies store-for-store (stride 80 / sizeof(DetachedPartRenderEvent)):
//
//   AddEvent @ 0x825E5C78 — called by PhysicalBodyPartPool::OutputEvents. Asserts mpEvents != NULL
//     (CgsBaseEventQueue.h:312) and miLength < miMaxLength ("Reached Max length", :313), both
//     NON-GATING tripwires, then appends a copy of the event (the four 16-byte SIMD stores of the
//     Matrix44Affine at +0/+16/+32/+48, the two dwords at +64/+68 and the bool at +72), bumps
//     miLength and returns true.
//   Append   @ 0x827A7C80 — called by DeformationOutputInterfaceForEntityModules::Append. Asserts
//     mpEvents != NULL (:413) and lSource.miLength + miLength <= miMaxLength ("Base event queue
//     overflow", :414) — non-gating tripwires — then XMemCpy's (std::memcpy here) the source's
//     live events onto the tail (80 * miLength + mpEvents <- src, 80 * srcLength bytes) and
//     advances miLength. Returns true.
template bool CgsModule::BaseEventQueue<BrnPhysics::Deformation::DetachedPartRenderEvent>::AddEvent(
    const BrnPhysics::Deformation::DetachedPartRenderEvent&);
template bool CgsModule::BaseEventQueue<BrnPhysics::Deformation::DetachedPartRenderEvent>::Append(
    const CgsModule::BaseEventQueue<BrnPhysics::Deformation::DetachedPartRenderEvent>&);

// The owning EventQueue<DetachedPartRenderEvent, 50>::Construct points the base queue at its
// inline maEvents buffer and sets the capacity; emitted here for completeness of this ledger TU.
template void CgsModule::EventQueue<BrnPhysics::Deformation::DetachedPartRenderEvent, 50>::Construct();
