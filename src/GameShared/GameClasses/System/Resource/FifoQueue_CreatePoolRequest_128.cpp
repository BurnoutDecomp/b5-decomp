// Per-instantiation .cpp for FifoQueue<CgsResource::Events::CreatePoolRequest, 128>.
// The generic FifoQueue<T,N> body (Construct/Push/Pop/Peek/...) is fully inline in
// CgsFifoQueue.h, so this TU is just the explicit class instantiation (the X360 emits one
// out-of-line copy per using-TU; CreatePoolRequest,128>::Pop @ 0x828DFBB8 called by
// CgsResource::PoolModule::DoCreatePoolRequest, Push @ 0x828DFB28 called by
// CgsResource::PoolModule::SendCreatePoolMemoryRequest).
//
// Byte-parity check against the X360 Push/Pop pseudocode for this instantiation:
//   element stride 172 (CreatePoolRequest), maData[128] @ +0 (22016B), miReadPos@+22016
//   (0x5600), miWritePos@+22020 (0x5604), miLength@+22024 (0x5608). Pop's
//   `if (*(a1+22024) <= 0) return 0;` == `if (miLength <= 0) return false;`, the
//   `memcpy(a2, 172 * *(a1+22016) + a1, 172)` == `*lpItem = maData[miReadPos]`, then
//   ++miReadPos/--miLength wrapping miReadPos at 128. Push mirrors with miWritePos@+22020
//   and the full check `if (*(a1+22024) >= 128) return 0;` -- all matched by the generic.
//
// Spelled unqualified FifoQueue<...> to match the committed FifoQueue<f32,10> container
// convention (CgsFifoQueueFloat10.cpp); the DWARF spells the type
// CgsContainers::FifoQueue<CgsResource::Events::CreatePoolRequest,128>.
#include "GameShared/GameClasses/Containers/CgsFifoQueue.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"  // CreatePoolRequest (172B)

template class FifoQueue<CgsResource::Events::CreatePoolRequest, 128>;
