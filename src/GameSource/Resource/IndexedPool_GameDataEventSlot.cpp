#include "GameShared/GameClasses/Containers/CgsIndexedPool.h"
#include "GameSource/Resource/BrnGameDataEventSlot.h"   // BrnResource::GameDataEventSlot (0x30 element)

// Per-instantiation TU for CgsContainers::IndexedPool<BrnResource::GameDataEventSlot, s16>.
// The member bodies are inline in CgsIndexedPool.h; this .cpp forces the out-of-line
// emission of the four members the X360 ARTIST build attested under the IDA-truncated
// symbol "BrnResource::GameDataEventSlot,short>":
//
//   operator[](s16)        @ 0x82663010  (baked asserts CgsIndexedPool.h:241 / :250)
//   Clear()                @ 0x826638D0
//   GetObjectIndex(T*)     @ 0x82662F50  (assert CgsIndexedPool.h:222)
//   PushIndex(s16)         @ 0x82663180  (asserts CgsIndexedPool.h:301 / :310)
//
template BrnResource::GameDataEventSlot&
    CgsContainers::IndexedPool<BrnResource::GameDataEventSlot, s16>::operator[](s16);
template void
    CgsContainers::IndexedPool<BrnResource::GameDataEventSlot, s16>::Clear();
template s16
    CgsContainers::IndexedPool<BrnResource::GameDataEventSlot, s16>::GetObjectIndex(
        const BrnResource::GameDataEventSlot*);
template void
    CgsContainers::IndexedPool<BrnResource::GameDataEventSlot, s16>::PushIndex(s16);
