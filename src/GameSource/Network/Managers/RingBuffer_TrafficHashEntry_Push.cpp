#include "GameShared/GameClasses/Containers/CgsRingBuffer.h"               // CgsContainers::RingBuffer<T>::Push (inline generic)
#include "GameSource/Network/Managers/BrnNetworkTrafficManager.h"          // BrnNetwork::TrafficManager::TrafficHash (4-byte element)

// CgsContainers::RingBuffer<BrnNetwork::TrafficManager::TrafficHash>::Push
// This is the generic CgsContainers::RingBuffer<Type>::Push (inline in CgsRingBuffer.h); the
// file is the thin explicit instantiation for the traffic-hash ring's 4-byte element. The
// console body matches the generic store-for-store at a 4-byte stride: the element copy is two
// halfwords (TrafficHash::muTrafficHash @ +0, muUpdate10HzFrame @ +2), then the write position
// and live length advance with wrap, and a full buffer advances the read position and asserts
// "Read pos should equal write pos if buffer is full". Member offsets are exactly
// RingBuffer<Type>: mpData +0x0, miMaxLength +0x4, miReadPos +0x8, miWritePos +0xC, miLength +0x10.
template void
CgsContainers::RingBuffer<BrnNetwork::TrafficManager::TrafficHash>::Push(
    const BrnNetwork::TrafficManager::TrafficHash*);
