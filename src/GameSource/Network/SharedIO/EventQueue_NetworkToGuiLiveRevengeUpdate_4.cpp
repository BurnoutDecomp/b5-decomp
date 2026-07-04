#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"   // BaseEventQueue<T>::AddEvent (inline generic)
#include "GameSource/Network/SharedIO/BrnNetworkToGuiEvents.h"   // BrnNetwork::NetworkToGuiLiveRevengeUpdate (16-byte element)

// CgsModule::EventQueue<BrnNetwork::NetworkToGuiLiveRevengeUpdate, 4>::Construct  @ 0x82592518
//   (called by BrnNetwork::BrnNetworkModuleIO::OutputBuffer::Construct)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic EventQueue<T,N>::Construct body
// (== BaseEventQueue<T>::Construct(maEvents, N)) is fully inline in CgsEventQueue.h, so this
// TU is just the explicit member instantiation (the X360 emits one out-of-line copy per
// using-TU).
//
// Byte-parity check against the X360 body:
//   r31 = this; r30 = this + 0xC                  (lpEventBuffer = &maEvents[0])
//   assert r30 != 0   ("lpEventBuffer != NULL", CgsBaseEventQueue.h:160)
//   stw r30, 0(this)  -> mpEvents    = &maEvents[0]   (base queue's three words at +0/+4/+8)
//   stw 4,   4(this)  -> miMaxLength = 4              (== KI_LENGTH == N)
//   stw 0,   8(this)  -> miLength    = 0
// The inline maEvents[4] buffer therefore begins at this+0xC, immediately after the three
// BaseEventQueue<T> words, matching the X360 stores exactly. The element is the committed
// BrnNetwork::NetworkToGuiLiveRevengeUpdate (four int32 == 16 bytes); Construct does not read
// element fields, only takes the buffer base address.
template void CgsModule::EventQueue<BrnNetwork::NetworkToGuiLiveRevengeUpdate, 4>::Construct();

// CgsModule::BaseEventQueue<BrnNetwork::NetworkToGuiLiveRevengeUpdate>::GetEvent(s32) const  @ 0x823AC498
//   (called by BrnGame::BrnGameModule::TranslateNetworkInterfaceToGuiEvents, which drains the
//   NetworkToGui queue). The generic const GetEvent body is inline in CgsBaseEventQueue.h:
//   asserts mpEvents != NULL (:272), liIndex < GetLength() (:274), liIndex >= 0 (:275), then
//   returns mpEvents[liIndex]. The X360 return `slwi r11,r29,4` == index*sizeof(T) with
//   sizeof(NetworkToGuiLiveRevengeUpdate) == 16. Thin out-of-line instantiation.
template const BrnNetwork::NetworkToGuiLiveRevengeUpdate&
CgsModule::BaseEventQueue<BrnNetwork::NetworkToGuiLiveRevengeUpdate>::GetEvent(s32) const;

// CgsModule::BaseEventQueue<BrnNetwork::NetworkToGuiLiveRevengeUpdate>::AddEvent  @ 0x82557F28
//   (called by BrnNetwork::LiveRevengeManager::DisplayPlayerTakedownMessage and
//   ::DisplayRivalTakedownMessage, which push live-revenge status updates into the
//   NetworkToGui OutputBuffer's per-event queue).
//
// Thin explicit instantiation of the generic AddEvent(const T&) already inline in
// CgsBaseEventQueue.h. Store-for-store match against the X360 body:
//   * lwz r11,0(r29); bne -> non-gating 'mpEvents != NULL' tripwire (CgsBaseEventQueue.h:312);
//   * lwz r11,8(r29) (miLength@+8) vs lwz r10,4(r29) (miMaxLength@+4); blt skips the de-inlined
//     'CgsModule::BaseEventQueue<...>::AddEvent\nReached Max length ' builder -- non-gating (:313);
//   * 16-byte stride: lwz r11,8(r29) (miLength), slwi r11,r11,4 (miLength*16), lwz r10,0(r29)
//     (+mpEvents), add, then FOUR 32-bit word copies from r26 (a2) at +0/+4/+8/+0xC ==
//     sizeof(NetworkToGuiLiveRevengeUpdate) == 16 (four int32);
//   * bump miLength: lwz r11,8(r29)/addi r11,r11,1/stw r11,8(r29); return 1 (li r3,1).
// Member offsets: mpEvents@+0, miMaxLength@+4, miLength@+8. Element already committed (16 bytes).
template bool
CgsModule::BaseEventQueue<BrnNetwork::NetworkToGuiLiveRevengeUpdate>::AddEvent(
    const BrnNetwork::NetworkToGuiLiveRevengeUpdate& lEvent);
