#include "GameSource/Sound/BrnResourceRegistrar.h"

// Explicit per-method instantiation of
//   CgsContainers::LinkedListHelper<BrnSound::Logic::ResourceRegistrar::RequestedResource, 124>::Construct().
// Reconstructed from BURNOUT_X360_ARTIST.XEX (0x827E17B0; out-of-line per-instantiation
// emission, the pool ctor the registrar's Construct @0x826B0470 calls for mLoadedResourceList).
// Shared generic body lives in CgsLinkedList.h: it writes miNumNodes = 124, then seeds the two
// sublists -- maFreeList.InternalInit(maNodePool, 124, sizeof(Node)) (the X360 reads the count
// back from *this == miNumNodes and uses byte stride 0x13C/316) and maLiveList.InternalInit(0,0,
// sizeof(Node)) for the empty live list. Per-method so safe siblings not attributed to this
// instance stay un-instantiated (matches the EventQueue/VariableEventQueue instantiation TUs).
//
// FAITHFUL NOTE: the X360 0x827E17B0 also INLINES the per-node seeding of each pooled
// RequestedResource's inner mRequesterList (LinkedListHelper<IResourceRequester*,16>) -- a 124-
// iteration loop writing 16 to each node's miNumNodes then two InternalInit(stride 12) calls. On
// the typed host that inner-list construction belongs to each RequestedResource (its mRequesterList
// member), not to this outer helper's generic Construct(); it is seeded when a node is actually
// built (RequestedResource(const QueuedResource&) calls mRequesterList.Construct()). The outer
// generic Construct() is NOT extended to walk the pool because that body is shared by every
// LinkedListHelper<T,N> instantiation and must stay T-agnostic (ODR). Net behaviour is equivalent;
// only the point at which each inner list is seeded differs.
template void CgsContainers::LinkedListHelper<BrnSound::Logic::ResourceRegistrar::RequestedResource, 124>::Construct();
