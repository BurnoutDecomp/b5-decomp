#include "GameShared/GameClasses/RenderWare/PS3/CgsRwVertexDescResourceTypePS3.h"

// The native resource handler is homed in CgsRwVertexDescResourceType.cpp.
// Its FixUp preserves ARTIST828A80F8 and creates the PC declaration through
// the existing descriptor cache. The former local VertexDescriptor definition
// duplicated a different renderengine layout and is intentionally retired.
