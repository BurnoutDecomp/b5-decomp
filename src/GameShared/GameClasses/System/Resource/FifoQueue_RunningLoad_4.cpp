// Per-instantiation .cpp for FifoQueue<CgsResource::RunningLoad, 4>. The generic
// FifoQueue<T,N> body (Construct/Push/Pop/Peek/...) is fully inline in CgsFifoQueue.h, so
// this TU is just the explicit class instantiation (the X360 emits one out-of-line copy
// per using-TU; RunningLoad,4>::Pop @ 0x828DF7C8, called by
// CgsResource::BundleLoaderModule::StreamIdleFunc).
//
// Byte-parity check against the X360 Pop pseudocode for this instantiation:
//   element stride 148 (RunningLoad == {LoadBundleRequest}), maData[4] @ +0 (592B),
//   miReadPos@+592 (0x250), miWritePos@+596 (0x254), miLength@+600 (0x258). The
//   pseudocode's `if (*(a1+600) <= 0) return 0;` == `if (miLength <= 0) return false;`,
//   `memcpy(a2, 148 * *(a1+592) + a1, 148)` == `*lpItem = maData[miReadPos]`, then
//   ++miReadPos/--miLength and wrap miReadPos at 4 -- all matched by the generic.
//
// Spelled unqualified FifoQueue<...> to match the committed Array<T,N>/FifoQueue<f32,10>
// container convention (CgsFifoQueueFloat10.cpp); the DWARF spells the type
// CgsContainers::FifoQueue<CgsResource::RunningLoad,4> (CgsBundleLoaderModule.h:51,
// typedef RunningLoadQueue).
#include "GameShared/GameClasses/Containers/CgsFifoQueue.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoaderModule.h"  // CgsResource::RunningLoad (148B)

template class FifoQueue<CgsResource::RunningLoad, 4>;
