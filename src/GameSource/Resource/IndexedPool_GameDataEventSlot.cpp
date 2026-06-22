#include "GameShared/GameClasses/Containers/CgsIndexedPool.h"
#include "GameSource/Resource/BrnGameDataEventSlot.h"   // BrnResource::GameDataEventSlot (0x30 element)

// Per-instantiation TU for CgsContainers::IndexedPool<BrnResource::GameDataEventSlot, N>.
// The accessor body is inline in CgsIndexedPool.h; this .cpp forces the out-of-line
// emission of the indexed accessor the X360 ARTIST build attested under the
// IDA-truncated symbol "BrnResource::GameDataEventSlot,":
//
//   operator[](s16)  @ 0x82663010  (baked asserts CgsIndexedPool.h:241 / :250)
//
// FLAG: the pool capacity N is a runtime field (msCapacity), not used by operator[],
// so the instantiation body is N-independent -- the chosen N below is a placeholder
// (GameDataModule's real event-slot count is deferred with that module). The emitted
// accessor code is identical for any N.
namespace BrnResource { struct GameDataEventSlot; }

template BrnResource::GameDataEventSlot&
    CgsContainers::IndexedPool<BrnResource::GameDataEventSlot, 256>::operator[](s16);
