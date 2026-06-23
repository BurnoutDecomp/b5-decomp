// Per-instantiation .cpp for FifoQueue<CgsResource::Events::AllocateResourceListRequest, 4>.
// The generic FifoQueue<T,N> body (Construct/Push/Pop/Peek/...) is fully inline in
// CgsFifoQueue.h, so this TU is just the explicit class instantiation (the X360 emits one
// out-of-line copy per using-TU; AllocateResourceListRequest,4>::Pop @ 0x828DFCC0 called by
// CgsResource::PoolModule::Update, Push @ 0x828DFC48 called by
// CgsResource::PoolModule::ProcessInputBuffer).
//
// Byte-parity check against the X360 Push/Pop pseudocode for this instantiation:
//   element stride 48 (AllocateResourceListRequest, 6 qwords), maData[4] @ +0 (192B),
//   miReadPos@+192 (0xC0), miWritePos@+196 (0xC4), miLength@+200 (0xC8). Pop's
//   `if (*(a1+200) <= 0) return 0;` == `if (miLength <= 0) return false;`, the 6x qword
//   copy from `48 * *(a1+192) + a1` == `*lpItem = maData[miReadPos]`, then ++miReadPos/
//   --miLength wrapping miReadPos at 4. Push mirrors with miWritePos@+196 and the full
//   check `if (*(a1+200) >= 4) return 0;` -- all matched by the generic.
//
// Spelled unqualified FifoQueue<...> to match the committed FifoQueue<f32,10> container
// convention (CgsFifoQueueFloat10.cpp); the DWARF spells the type
// CgsContainers::FifoQueue<CgsResource::Events::AllocateResourceListRequest,4>.
#include "GameShared/GameClasses/Containers/CgsFifoQueue.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"  // AllocateResourceListRequest (48B)

template class FifoQueue<CgsResource::Events::AllocateResourceListRequest, 4>;
