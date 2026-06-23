#include "GameShared/GameClasses/Module/CgsEventQueue.h"
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
