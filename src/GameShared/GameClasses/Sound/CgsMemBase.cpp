#include "GameShared/GameClasses/Sound/CgsMemBase.h"
#include "rw/rwcore_structs.h"

namespace CgsSound
{
// The operators and the destructor are inline in the header (DWARF CgsMemBase.h:83..406).
rw::IResourceAllocator* MemBase::mpAllocator = 0;
}
