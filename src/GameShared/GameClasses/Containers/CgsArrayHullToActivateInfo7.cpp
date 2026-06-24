// Per-instantiation .cpp for Array<BrnNetwork::HullToActivateInfo, 7>. The generic
// Array<T,N> body (Ge / Grow + siblings) is fully inline in CgsArray.h, so this TU is
// just the explicit class instantiation (the X360 emits one out-of-line copy per
// using-TU). The two methods the X360 emitted for this instance:
//   Array<HullToActivateInfo,7>::Ge   @ 0x8254E2D8 (const checked accessor)
//       (BrnNetwork::TrafficManager::_HullSyncMessageArrivedCallback, HullSyncMessage::PackOrUnpack)
//   Array<HullToActivateInfo,7>::Grow @ 0x8254DB78 (reserve-next-free-slot)
//       (BrnNetwork::TrafficManager::HandleHullSyncMessage, HullSyncMessage::GetPackedMessageSize/PackOrUnpack)
//
// Layout: maElements[7] (7 * 6 = 42B; HullToActivateInfo is three u16 lanes -- muHull,
// muFramesSinceStart, muTrafUpdateToActivate) + miCount @ +0x2C, matching the X360 count
// word read/written at *(this+0x2C) (`lwz 0x2C(rN)`) and the 6-byte element addressing
// (`slwi r11,r27,1; add r11,r27,r11; slwi r11,r11,1` == 6*index, then + base).
//
//   * Ge   @ 0x8254E2D8: const operator[]; asserts constructed (count != -1, CgsArray.h:556)
//     then bounds-checks the index against count (CgsArray.h:557, "Array index out of bounds.
//     Index: <i>, length: <n>" streamed dynamic message, kept as the static string in the
//     generic CGS_ASSERT body), returns &maElements[index] (6*index + base). Does NOT touch
//     the count.
//   * Grow @ 0x8254DB78: asserts constructed (CgsArray.h:289) then capacity (count < 7u,
//     CgsArray.h:290, "Array container out of space, Length: <n>, Capacity: 7" streamed
//     message), reserves &maElements[count] (6*count + base) and post-increments the count
//     word (`stw count+1, 0x2C(rN)`).
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h);
// the DWARF spells the type CgsContainers::Array<BrnNetwork::HullToActivateInfo,7u>. The
// HullToActivateInfo type and the Array<HullToActivateInfo,7> typedef are committed in
// BrnHullSyncMessage.h; this file only forces the explicit instantiation.
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/Network/Messages/BrnHullSyncMessage.h"

template class Array<BrnNetwork::HullToActivateInfo, 7>;
