// Per-instantiation TU for the ledger id
//   class:CgsResource::Font>::operator class CgsResource::Font *
// i.e. CgsResource::SafeResourceHandle<CgsResource::Font>::operator CgsResource::Font*()  @ 0x8244FDA0
//   (called by BrnGui::InGameMessageRenderer::AddNewMessage).
//
// The generic body IS the inline conversion operator in CgsResourceHandle.h
// (SafeResourceHandle<ResourceType>::operator ResourceType*()); this .cpp only forces the
// out-of-line emission of the one symbol the X360 ARTIST build attested.
//
// X360 @0x8244FDA0 (store-for-store):
//   r28 = this
//   r11 = *(this+0)        ; lwz r11,0(r28)   -> mpResourceMemory (the inherited ResourceHandle
//                          ;                     +0 word, a CgsResource::SmallResource*)
//   r11 = *(r11+0)         ; lwz r11,0(r11)   -> *(void**)mpResourceMemory  (the inner main-memory
//                          ;                     resource ptr; this is the DOUBLE deref the whole
//                          ;                     SafeResourceHandle family uses)
//   if (r11 == 0) ASSERT-fire "Can not get resource pointer - it has no main memory resource"
//                          ; baked CgsResourceHandle.h:490 (discarded per project convention -- the
//                          ;   CGS_ASSERT substitution does not encode the baked file/line)
//   r3 = *(*(this+0))      ; lwz r11,0(r28); lwz r3,0(r11)  -> returns the double-deref Font*
//   -> exactly `*reinterpret_cast<Font* const*>(mpResourceMemory)`, the generic body.
//
// Font is FORWARD-DECLARED (NOT #include "CgsFont.h"). The conversion is a pointer-to-pointer
// reinterpret_cast only -- it never dereferences a Font, so an incomplete type suffices. (Pulling in
// CgsFont.h is no longer an ODR hazard: CgsFont.h has been reconciled to reuse this canonical
// SafeResourceHandle and no longer defines its own; we still forward-declare to keep this TU minimal.)
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"

namespace CgsResource { struct Font; }

template CgsResource::SafeResourceHandle<CgsResource::Font>::operator CgsResource::Font*();
