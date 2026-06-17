// Per-instantiation .cpp for FifoQueue<BrnNetwork::BrnNetworkModuleIO::NetworkOutRecvRoadRulesPBEvent, 14>.
// The generic FifoQueue<T,N> body (Construct/Push/Pop/Peek/...) is already fully inline in
// CgsFifoQueue.h, so this TU is just the explicit class instantiation (the X360 emits one
// out-of-line copy per using-TU; FifoQueue<...>::Push @ 0x8254DFB8, called by
// BrnNetwork::NetworkRoadRulesManager::_RoadRulesPersonalBestArrivedCallback).
//
// Byte-parity check against the X360 Push pseudocode for this instantiation:
//   element stride 72 (NetworkOutRecvRoadRulesPBEvent), maData[14] @ +0 (1008B),
//   miReadPos@+1008, miWritePos@+1012, miLength@+1016. The pseudocode's
//   `if (*(a1+1016) >= 14) return 0;` == `if (miLength >= N) return false;`,
//   `v4 = 72 * *(a1+1012) + a1` + element copy == `maData[miWritePos] = *lpItem`,
//   then ++miWritePos/++miLength and wrap miWritePos at 14 -- all matched by the generic.
//
// Spelled unqualified FifoQueue<...> to match the committed Array<T,N>/FifoQueue<f32,10>
// container convention (CgsFifoQueueFloat10.cpp); the DWARF spells the type
// CgsContainers::FifoQueue<BrnNetwork::BrnNetworkModuleIO::NetworkOutRecvRoadRulesPBEvent,14>.
#include "GameShared/GameClasses/Containers/CgsFifoQueue.h"
#include "GameSource/Network/BrnNetworkModuleIO.h"   // NetworkOutRecvRoadRulesPBEvent element (72B)

template class FifoQueue<BrnNetwork::BrnNetworkModuleIO::NetworkOutRecvRoadRulesPBEvent, 14>;
