#pragma once

#include "GameShared/GameClasses/Development/CgsStrStream.h"

namespace CgsDev
{
namespace Log
{
    // DecFIGS CgsStrStream.h:322. ARTIST off_820CDEF0's sink points to the
    // genuine shared blr body at 0x8284CB38. Suppressing text is this sink's job.
    struct LogNull : public StrStreamBase
    {
        using StrStreamBase::operator<<;
        StrStreamBase& operator<<(const char*)  { return *this; }
    };
}
}
