#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                             // CgsModule::BaseEventQueue<T>::AddEvent / Append (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventClearCullingTable.h" // InEventClearCullingTable element (1B)

// =============================================================================
// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventClearCullingTable>::AddEvent
//   @ 0x827B4C10   (ledger id: class:CgsSceneManager::SceneManagerIO::InEventClearCullingTable>::AddEve)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// Appends one InEventClearCullingTable to the clear-culling-table input queue and returns true.
// The assert string baked in this body is
//   "CgsModule::BaseEventQueue<class CgsSceneManager::SceneManagerIO::InEventClearCullingTable>::AddEvent"
// (CgsBaseEventQueue.h:312/313), confirming this is the BaseEventQueue<T> -- not the derived
// EventQueue<T,N> -- AddEvent instantiation. Called from
// InSceneUpdateInterface::ClearCullingTable.
//
// X360 store-for-store (asm at 0x827B4C10), offsets are the BaseEventQueue header
// (mpEvents @ 0, miMaxLength @ 4, miLength @ 8):
//   lwz r11, 0(this)               ; assert mpEvents != NULL (line 312, non-gating tripwire)
//   lwz r11, 8(this); lwz r10, 4(this); cmpw r11, r10
//   blt .append                    ; skip the "Reached Max length" assert when miLength < miMaxLength
//   ... (line 313 tripwire) ...
// .append:
//   lwz r11, 8(this)               ; r11 = miLength  (index)
//   lbz r10, 0(a2)                 ; load the 1-byte element image
//   lwz r9,  0(this)               ; r9  = mpEvents  (base)
//   stbx r10, r11, r9              ; mpEvents[miLength] = *src   (single byte move, NO shift)
//   lwz r11, 8(this); addi r11, r11, 1; stw r11, 8(this) ; ++miLength
//   li r3, 1; return true
// == BaseEventQueue<InEventClearCullingTable>::AddEvent: append the 1-byte element
// unconditionally (the two asserts are non-gating tripwires), bump miLength, return true.
// The element is a single bool (mbCullAll); the generic `mpEvents[miLength] = lEvent` copy
// lowers to the single `lbz`/`stbx` above. Stride attested == 1 byte (sizeof
// InEventClearCullingTable, see CgsSceneManagerIO_EventClearCullingTable.h): no `slwi` shift
// precedes the indexed store, unlike the 8-byte siblings which shift the index by 3.
// =============================================================================
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventClearCullingTable>::AddEvent(
    const CgsSceneManager::SceneManagerIO::InEventClearCullingTable& lEvent);

// =============================================================================
// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventClearCullingTable>::Append
//   @ 0x827A6488   (ledger id: class:CgsSceneManager::SceneManagerIO::InEventClearCullingTable>::Append)
//
// The generic BaseEventQueue<T>::Append body is already inline in CgsBaseEventQueue.h; this is
// the thin explicit instantiation. The X360 body matches the generic store-for-store:
//   * asserts mpEvents != NULL (CgsBaseEventQueue.h:413 "mpEvents != NULL" tripwire,
//     `lwz r11,0(r31)`; bne skips the assert);
//   * asserts no overflow (CgsBaseEventQueue.h:414 "Base event queue overflow" tripwire,
//     `lwz r11,8(r30)` (lSource.miLength) + `lwz r10,8(r31)` (this->miLength) vs
//     `lwz r9,4(r31)` (this->miMaxLength); ble skips the assert -- NON-gating, the copy always runs);
//   * reads lSource's backing buffer via GetQueueStartPointer (CgsBaseEventQueue.h:486
//     "mpEvents != NULL" tripwire on lSource, `lwz r11,0(r30)`; bne skips);
//   * XMemCpy(this->mpEvents + this->miLength, lSource.mpEvents, sizeof(T) * lSource.miLength)
//     at a 1-byte element stride -- count == lSource.miLength (`mr r5,r29` straight count, no
//     shift, since stride == 1) and dest offset == this->miLength (`add r3,r11,r10`, no shift);
//     modelled as std::memcpy;
//   * bumps this->miLength by lSource.miLength (`add r11; stw r11,8(r31)`) and returns 1.
// The 1-byte stride matches sizeof(InEventClearCullingTable) == 1 (a single bool mbCullAll,
// the X360-attested stride: no `slwi` shift appears before the XMemCpy call, unlike the 8-byte
// siblings which shift by 3). This is the Append member of the EventQueue<InEventClearCullingTable,64>
// family whose Construct instantiation lives in EventQueue_InEventClearCullingTable_64.cpp;
// called from InSceneUpdateInterface::Append.
// =============================================================================
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventClearCullingTable>::Append(
    const CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventClearCullingTable>&);
