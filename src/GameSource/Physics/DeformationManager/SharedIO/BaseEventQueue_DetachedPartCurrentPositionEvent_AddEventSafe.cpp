#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                              // BaseEventQueue<T>::AddEventSafe (inline generic)
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h"   // BrnPhysics::Deformation::DetachedPartCurrentPositionEvent (80-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Deformation::DetachedPartCurrentPositionEvent>::AddEventSafe @ 0x825E5B00
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEventSafe body is
// already inline in CgsBaseEventQueue.h; this is the thin explicit instantiation. Unlike the
// UNCONDITIONAL AddEvent, this is the BOUNDS-GATED variant:
//   - assert mpEvents != NULL (CgsBaseEventQueue.h:331, `lwz r11,0(this)`; bne skips) -- the single
//     non-gating tripwire (li r5,0x14B == 331);
//   - if miLength >= miMaxLength (`lwz r11,8(this)`(miLength) vs `lwz r10,4(this)`(miMaxLength),
//     `bge`) it returns 0 WITHOUT appending (li r3,0) -- the real bounds gate;
//   - otherwise reserves the tail slot at mpEvents + 80*miLength (`slwi r9,len,2`==len*4,
//     `add r11,len,r9`==len*5, `slwi r11,r11,4`==*16 => len*80, `add`+mpEvents), writes
//     miLength+1 back, copies the 80-byte element (leading 64-byte Matrix44Affine in four
//     16-byte VMX lanes lvx128/stvx128 at +0/+16/+32/+48, then the two scalar dwords
//     mVehicleEntityId/meType at +0x40/+0x44), and returns 1.
// The 80-byte stride matches sizeof(DetachedPartCurrentPositionEvent) == alignas(16){
// Matrix44Affine(64) + EntityId(4) + EBodyParts(4) } rounded to 80. Callers (X360 xrefs):
// Deformation::PhysicalBodyPartPool::OutputEvents.
// X360-attested element stride (len*80 via *4/*5/*16; 4 SIMD lanes + 2 scalar dwords).
static_assert(sizeof(BrnPhysics::Deformation::DetachedPartCurrentPositionEvent) == 80,
              "DetachedPartCurrentPositionEvent stride 80");

template bool
CgsModule::BaseEventQueue<BrnPhysics::Deformation::DetachedPartCurrentPositionEvent>::AddEventSafe(
    const BrnPhysics::Deformation::DetachedPartCurrentPositionEvent&);
