// Per-instantiation .cpp for CgsContainers::PriorityQueue<CgsResource::Events::LoadBundleRequest, 128>.
// The generic PriorityQueue<T,N> body (Construct/Push) is fully inline in CgsPriorityQueue.h, so
// this TU is just the explicit class instantiation (the X360 emits one out-of-line copy per
// using-TU).
//
// Byte-parity check against the X360 Push pseudocode for this instantiation:
//   Push @ 0x828DF858, called by CgsResource::BundleLoaderModule::ProcessBundleLoadRequests.
//   element stride 148 (LoadBundleRequest), the full check `a1[1] == *a1` ==
//   `muNumEntries == muMaxEntries`, then `v10 = BasePriorityQueue::AddEntry(a1, priority)` and
//   `memcpy(148 * v10 + a1[5], item, 148)` == `mpData[v10] = *lpItem` (a1[5] @ +0x14 == mpData).
//   The queue-full path logs "Unable to add entry. Queue is full." through the debug print and
//   returns 0 -- modelled as the early `return false` (the log is a non-gating tripwire).
#include "GameShared/GameClasses/Containers/CgsPriorityQueue.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"  // LoadBundleRequest (148B element)

template class CgsContainers::PriorityQueue<CgsResource::Events::LoadBundleRequest, 128>;
