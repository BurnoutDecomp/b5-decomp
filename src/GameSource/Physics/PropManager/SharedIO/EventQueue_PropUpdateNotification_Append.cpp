#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"

// CgsModule::BaseEventQueue<BrnPhysics::Props::PropUpdateNotification>::Append
//   @ 0x823C3DA8   (BURNOUT_X360_ARTIST.XEX)
//   called by BrnGame::BrnGameModule::BridgeWorldToSound and
//             BrnWorldIO::UpdateOutputBuffer::AppendPropUpdateNotificationQueue
//
// Explicit instantiation of the committed generic BaseEventQueue<T>::Append
// (body inline in CgsBaseEventQueue.h). The X360 emits a per-instantiation
// out-of-line body; here it is realised by instantiating T =
// BrnPhysics::Props::PropUpdateNotification over the shared generic.
//
// ASM <-> generic mapping (store-for-store):
//   lwz r11,0(r31)            -> mpEvents              (a1[0])  -> assert mpEvents != NULL  (CgsBaseEventQueue.h:413)
//   lwz r11,8(r30)/r10,8(r31) -> lSource.miLength + miLength    -> assert <= miMaxLength (4(r31))  (:414)
//   lwz r11,0(r30)            -> lSource.mpEvents      (a2[0])  -> assert mpEvents != NULL  (:486, via GetQueueStartPointer)
//   slwi r5,r29,6             -> sizeof(T) * lSource.miLength    (stride 64 == sizeof(PropUpdateNotification))
//   slwi r11,r11,6 + 0(r31)   -> mpEvents + miLength*sizeof(T)   (XMemCpy dest)
//   bl XMemCpy                -> std::memcpy(dest, src, bytes)   (Xbox block-copy intrinsic)
//   stw r11,8(r31)            -> miLength += lSource.miLength
//   li r3,1 / b __restgprlr_27-> return true
//
// The 64-byte stride proves sizeof(PropUpdateNotification) == 64; the alignas(16)
// home layout in BrnPropEvents.h sizes the record there, so the generic
// `sizeof(T) * liSourceLength` byte count reproduces the asm's `r29 << 6` exactly.
template bool CgsModule::BaseEventQueue<BrnPhysics::Props::PropUpdateNotification>::Append(
    const CgsModule::BaseEventQueue<BrnPhysics::Props::PropUpdateNotification>&);
