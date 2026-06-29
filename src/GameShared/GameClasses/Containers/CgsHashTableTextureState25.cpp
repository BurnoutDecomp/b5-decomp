// Explicit-instantiation gate TU for the Apt rasteriser's per-shape texture-state CACHE:
//   CgsContainers::HashTable<uint32_t, renderengine::TextureState*, 25>
// (the type AptRenderHandler::Render @0x5CB230 drives through GetInternal + the insert path).
//
// HashTable<K,V,N> is a header-only template (CgsHashTable.h); this TU pins the exact
// instantiation the rasteriser needs so the GetInternal/Get/Insert/Remove bodies are compiled
// and gated. renderengine::TextureState is referenced only as a pointer here, so a forward
// declaration is sufficient (no need to pull the full render-engine type into the gate).

#include "GameShared/GameClasses/Containers/CgsHashTable.h"

namespace renderengine { class TextureState; }

namespace CgsContainers
{
    // Force the whole template surface to be emitted/checked for this instantiation.
    template class HashTable<uint32_t, renderengine::TextureState*, 25>;
}
