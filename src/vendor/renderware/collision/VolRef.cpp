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
