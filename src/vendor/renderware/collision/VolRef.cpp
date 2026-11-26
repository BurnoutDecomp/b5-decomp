// XDK size optimization: ARTIST compiled RenderWare's rwcollision for size (record-form cr0
// tests, as in the rwcore filesys sources).
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
#include "vendor/renderware/collision/VolRef.hpp"

namespace rw { namespace collision {

// ARTIST 82BB33E8 copies named fields and leaves alignment gaps untouched.
VolRef& VolRef::operator=(const VolRef& rOther)
{
    volume = rOther.volume;
    tm = rOther.tm;
    tmContents = rOther.tmContents;
    bBox = rOther.bBox;
    tag = rOther.tag;
    numTagBits = rOther.numTagBits;
    return *this;
}

} }
