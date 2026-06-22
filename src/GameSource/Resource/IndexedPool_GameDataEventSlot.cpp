#include "GameShared/GameClasses/Containers/CgsIndexedPool.h"
#include "GameSource/Resource/BrnGameDataEventSlot.h"   // BrnResource::GameDataEventSlot (0x30 element)

// Per-instantiation TU for CgsContainers::IndexedPool<BrnResource::GameDataEventSlot, N>.
// The member bodies are inline in CgsIndexedPool.h; this .cpp forces the out-of-line
// emission of the four members the X360 ARTIST build attested under the IDA-truncated
// symbol "BrnResource::GameDataEventSlot,short>":
//
//   operator[](s16)        @ 0x82663010  (baked asserts CgsIndexedPool.h:241 / :250)
//   Clear()                @ 0x826638D0
//   GetObjectIndex(T*)     @ 0x82662F50  (assert CgsIndexedPool.h:222)
//   PushIndex(s16)         @ 0x82663180  (asserts CgsIndexedPool.h:301 / :310)
//
// FLAG: the pool capacity N is a runtime field (msCapacity), not used by any of these
// members, so the instantiation bodies are N-independent -- the chosen N below is a
// placeholder (GameDataModule's real event-slot count is deferred with that module). The
// emitted code is identical for any N. The IDA "short>" tail is the template's second
// (size/capacity) parameter type spelled by the demangler, NOT a field type.
namespace BrnResource { struct GameDataEventSlot; }

template BrnResource::GameDataEventSlot&
    CgsContainers::IndexedPool<BrnResource::GameDataEventSlot, 256>::operator[](s16);
template void
    CgsContainers::IndexedPool<BrnResource::GameDataEventSlot, 256>::Clear();
template s16
    CgsContainers::IndexedPool<BrnResource::GameDataEventSlot, 256>::GetObjectIndex(
        const BrnResource::GameDataEventSlot*) const;
template void
    CgsContainers::IndexedPool<BrnResource::GameDataEventSlot, 256>::PushIndex(s16);
