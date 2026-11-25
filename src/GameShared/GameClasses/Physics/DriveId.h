#pragma once

#include "types.hpp"

namespace CgsPhysics
{
    // DecFIGS DriveId.h:53, :67-72. The drive drains read the complete
    // handle as one doubleword, and the slot table uses an eight-byte stride.
    struct DriveId
    {
        DriveId() {}
        DriveId(u64 luId) : mId(luId) {}
        operator u64() const { return mId; }

    private:
        u64 mId;
    };
}
