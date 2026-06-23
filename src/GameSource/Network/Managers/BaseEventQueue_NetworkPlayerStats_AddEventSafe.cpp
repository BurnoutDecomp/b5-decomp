#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"          // BaseEventQueue<T>::AddEventSafe (inline generic)
#include "GameSource/Network/Managers/BrnNetworkPlayerStats.h"        // BrnNetwork::NetworkPlayerStats element

// CgsModule::BaseEventQueue<BrnNetwork::NetworkPlayerStats>::AddEventSafe  @ 0x82557E90
//   (called by BrnNetwork::NetworkPlayerStatsManager::CopyResultsToOutputBuffer and
//    BrnNetwork::NetworkPlayerStatsManager::ValidateQueueAndPostGettingStatsEvents)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEventSafe body
// is fully inline in CgsBaseEventQueue.h, so this TU is just the explicit member instantiation
// (the X360 emits one out-of-line copy per using-TU).
//
// Byte-parity check against the X360 body (a1 == this):
//   lwz r11, 0(this); cmplwi 0; bne ...  -> assert mpEvents != NULL ("mpEvents != NULL",
//                                            CgsBaseEventQueue.h:331)
//   lwz miLength, 8(this); lwz miMaxLength, 4(this); cmpw; bge -> return 0 when miLength >= miMaxLength
//   mulli r10, miLength, 0x84            -> slot = miLength * stride + mpEvents
//   stw miLength+1, 8(this)              -> ++miLength
//   BrnNetwork::NetworkPlayerStats::operator=(slot, lEvent)  -> copy-assign the element (@0x82355C50)
//   return 1
// This is exactly BaseEventQueue<T>::AddEventSafe: assert-buffer, bounds-gate-return-false-when-full,
// element copy-assign via T::operator=, ++miLength, return true. The element copy resolves to the
// committed BrnNetwork::NetworkPlayerStats::operator= (the X360 calls it directly), reused by name.
//
// Stride note (semantic parity, NOT byte-match): the X360 instance strides 0x84 (132) per element,
// whereas the committed BrnNetwork::NetworkPlayerStats is sizeof 136 (4-byte aligned; its copy-assign
// writes through +0x81). The generic template indexes with sizeof(T), so this instantiation strides
// 136; the 4-byte tail-padding difference does not change the observable behaviour (assert, bounds
// gate, copy-assign of the same committed member set, count bump). The element type and its layout
// are committed elsewhere and reused unchanged here -- see flagged_type_changes.
template bool
CgsModule::BaseEventQueue<BrnNetwork::NetworkPlayerStats>::AddEventSafe(const BrnNetwork::NetworkPlayerStats&);
