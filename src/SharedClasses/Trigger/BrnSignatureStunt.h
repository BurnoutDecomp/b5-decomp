#pragma once
#include "types.hpp"
#include "BrnCommonTypes.h"
#include <cstddef>

namespace BrnTrigger
{
struct GenericRegion;
struct TriggerData;

// Native24-byte table record. ARTIST8267FC08/8267F800 use the pointer
// at10 and count at14; DecFIGS supplies the field names and scalar types.
struct SignatureStunt
{
private:
    friend struct TriggerData;
    CgsID mId;
    s64 miCamera;
    const GenericRegion** mppStuntElements;
    s32 miStuntElementCount;

    static void AssertNativeLayout()
    {
        CGS_STATIC_ASSERT((sizeof(SignatureStunt) == 24), "X360 signature stunt stride");
        CGS_STATIC_ASSERT((offsetof(SignatureStunt, mppStuntElements) == 0x10), "ARTIST element pointer");
        CGS_STATIC_ASSERT((offsetof(SignatureStunt, miStuntElementCount) == 0x14), "ARTIST element count");
    }
};
}
