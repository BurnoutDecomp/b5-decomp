#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                              // BaseEventQueue<T>::Append (inline generic)
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h"   // BrnPhysics::Deformation::DetachedPartCurrentPositionEvent (80-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Deformation::DetachedPartCurrentPositionEvent>::Append @ 0x823C3BC8
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::Append body is
// already inline in CgsBaseEventQueue.h; this is the thin explicit instantiation. The X360 body
// matches the generic store-for-store (header offsets: mpEvents @ 0, miMaxLength @ 4, miLength @ 8):
//   * asserts mpEvents != NULL (CgsBaseEventQueue.h:413 "mpEvents != NULL", li r5,0x19D==413;
//     `lwz r11,0(this)`; cmplwi; bne -> skip);
//   * asserts no overflow (CgsBaseEventQueue.h:414 "Base event queue overflow", li r5,0x19E==414;
//     `lwz r11,8(lSource)`(lSource.miLength) + `lwz r10,8(this)`(this.miLength), add, cmpw vs
//     `lwz r9,4(this)`(this.miMaxLength); ble -> skip -- NON-gating tripwire, copy always runs);
//   * asserts lSource.mpEvents != NULL via GetQueueStartPointer (CgsBaseEventQueue.h:486
//     "mpEvents != NULL", li r5,0x1E6==486; `lwz r11,0(lSource)`; cmplwi; bne -> skip);
//   * XMemCpy(this->mpEvents + this->miLength*80, lSource.mpEvents, lSource.miLength*80) at an
//     80-byte element stride -- count `slwi r9,r29,2`(len*4) `add r9,r29,r9`(len*5)
//     `slwi r5,r9,4`(*16 => len*80); modelled by the generic std::memcpy(...sizeof(T)*len);
//   * bumps this->miLength by lSource.miLength and returns 1 (li r3,1).
// The 80-byte stride matches sizeof(DetachedPartCurrentPositionEvent) == alignas(16){
// Matrix44Affine(64) + EntityId(4) + EBodyParts(4) } rounded to 80. This is the Append member of the
// EventQueue<DetachedPartCurrentPositionEvent,50> family whose Construct instantiation lives in
// EventQueue_DetachedPartCurrentPositionEvent_50.cpp; called from Deformation::DeformationOutputInterface::operator=.
// X360-attested element stride (len*80 via *4/*5/*16).
static_assert(sizeof(BrnPhysics::Deformation::DetachedPartCurrentPositionEvent) == 80,
              "DetachedPartCurrentPositionEvent stride 80");

template bool
CgsModule::BaseEventQueue<BrnPhysics::Deformation::DetachedPartCurrentPositionEvent>::Append(
    const CgsModule::BaseEventQueue<BrnPhysics::Deformation::DetachedPartCurrentPositionEvent>&);
