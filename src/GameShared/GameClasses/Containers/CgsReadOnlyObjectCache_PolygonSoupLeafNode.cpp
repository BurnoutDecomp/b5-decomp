#include "GameShared/GameClasses/Containers/CgsReadOnlyObjectCache.h"

// Per-instantiation TU for CgsContainers::ReadOnlyObjectCache<CgsGeometric::PolygonSoupLeafNode>.
// The generic Construct/Release bodies are inline in CgsReadOnlyObjectCache.h; this .cpp only
// forces the out-of-line emission of the two symbols the X360 ARTIST build attested for this
// element type:
//
//   Construct @ 0x829170F8   (guard source/count, store mpSourceBuffer + miNumSourceEntries)
//   Release   @ 0x829172D0   (empty -- fetched entry is a direct pointer into the read-only
//                             source buffer, so releasing it is a no-op)
//
// CgsGeometric::PolygonSoupLeafNode is forward-declared (NOT #include'd): both bodies only
// store/cast a `const Type*` (Construct copies the pointer + count; Release ignores its arg),
// so an incomplete class type is sufficient and the full leaf-node layout (its own TU) is not
// pulled in. The complete type is homed in the PolygonSoup spatial-map TU tree
// (CgsPolygonSoupListSpatialMap.h forward-declares it).
namespace CgsGeometric { struct PolygonSoupLeafNode; }

template void CgsContainers::ReadOnlyObjectCache<CgsGeometric::PolygonSoupLeafNode>::Construct(
    const CgsGeometric::PolygonSoupLeafNode*, s32, s32, s32);
template void CgsContainers::ReadOnlyObjectCache<CgsGeometric::PolygonSoupLeafNode>::Release(
    const CgsGeometric::PolygonSoupLeafNode*);
