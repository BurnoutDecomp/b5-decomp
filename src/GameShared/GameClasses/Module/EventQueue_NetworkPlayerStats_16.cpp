// Per-instantiation .cpp for CgsModule::EventQueue<BrnNetwork::NetworkPlayerStats, 16>::Construct.
//
// The generic EventQueue<T, N> body (Construct -> BaseEventQueue<T>::Construct(maEvents, N))
// is fully inline in CgsEventQueue.h, so this TU is just the explicit member instantiation
// (the X360 emits one out-of-line copy per using-TU). The element type is the committed
// BrnNetwork::NetworkPlayerStats (136-byte record) reused by name.
//
// Byte-parity check against the X360 Construct for this instantiation:
//   CgsModule::EventQueue<BrnNetwork::NetworkPlayerStats,16>::Construct @ 0x825924A8
//     (called by BrnNetwork::BrnNetworkModuleIO::OutputBuffer::Construct).
//   Layout a1[0]=mpEvents, a1[1]=miMaxLength, a1[2]=miLength; the inline maEvents[16] buffer
//   starts at this+0xC. The body computes lpEventBuffer = this+0xC, asserts it != NULL
//   ("lpEventBuffer != NULL", CgsBaseEventQueue.h:160), then stores:
//     stw (this+0xC), 0(this)   -> mpEvents     = &maEvents[0]
//     stw 16,         4(this)   -> miMaxLength  = 16
//     stw 0,          8(this)   -> miLength     = 0
//   This is exactly EventQueue<T,16>::Construct() == BaseEventQueue<T>::Construct(maEvents, 16),
//   with N=16 supplying maEvents (at +0xC) and the KI_LENGTH (16) capacity. The DWARF confirms
//   CgsModule::EventQueue<BrnNetwork::NetworkPlayerStats,16> : BaseEventQueue<...> with maEvents[16]
//   (CgsEventQueue.h dwarfdump).

#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Network/Managers/BrnNetworkPlayerStats.h"  // BrnNetwork::NetworkPlayerStats (136B element)

template void CgsModule::EventQueue<BrnNetwork::NetworkPlayerStats, 16>::Construct();
