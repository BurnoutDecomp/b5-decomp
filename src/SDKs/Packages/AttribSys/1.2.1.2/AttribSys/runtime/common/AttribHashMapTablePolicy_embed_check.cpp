// Translation-unit embed check for Attrib::HashMapTablePolicy.
// Forces the owning header to compile standalone and exercises the bodied Free so
// its signature stays wired to its home.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttribHashMapTablePolicy.h"

namespace
{
void EmbedCheck()
{
    char lBlock[16];
    void* lpResult = Attrib::HashMapTablePolicy::Free(lBlock, sizeof(lBlock));
    (void)lpResult;
}
}
