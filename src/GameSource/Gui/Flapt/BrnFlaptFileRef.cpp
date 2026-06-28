#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"

#include "GameSource/Gui/Flapt/BrnFlaptFileInstance.h"   // BrnFlapt::FlaptFileInstance
#include "GameShared/GameClasses/Containers/CgsHash.h"    // CgsContainers::CgsHash::CalculateHash
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT

// BrnFlapt::FileRef member functions, reconstructed from BURNOUT_X360_ARTIST.XEX.
// This TU (GameSource/Gui/Flapt/BrnFlaptFileRef.cpp) bodies the two X360-emitted
// accessors:
//
//   FindComponent    @ 0x8246EC90
//   GetRootMovieClip @ 0x8246B410
//
// Both are thin forwarders onto the referenced FlaptFileInstance, guarded by the
// X360's null-pointer asserts. Each logically returns a MovieClipRef by value; the
// X360 sret slot is modeled, per this module's house convention (see
// FlaptFileInstance::GetRootMovieClip), as an explicit lpOutRef that is written and
// returned. The X360-baked BrnFlaptFileRef.h file/line cites are discarded per
// project convention.

namespace BrnFlapt
{

// ---- FindComponent @ 0x8246EC90 ------------------------------------------
// Assert the handle and the name, hash the NUL-terminated name (length excludes
// the terminator, matching the inlined strlen the X360 performs), then forward to
// FlaptFileInstance::FindComponent, which writes the located component's
// MovieClipRef into lpOutRef and returns it.
MovieClipRef* FileRef::FindComponent(MovieClipRef* lpOutRef, const char* lpcName) const
{
    CGS_ASSERT(mpFileInstance != 0, "mpFileInst");
    CGS_ASSERT(lpcName != 0, "lpcName");

    int liLength = 0;
    while (lpcName[liLength] != '\0')
    {
        ++liLength;
    }

    u32 luHash = CgsContainers::CgsHash::CalculateHash(const_cast<char*>(lpcName), liLength);

    return static_cast<FlaptFileInstance*>(mpFileInstance)->FindComponent(luHash, lpOutRef, lpcName);
}

// ---- GetRootMovieClip @ 0x8246B410 ---------------------------------------
// Assert the handle, then forward to FlaptFileInstance::GetRootMovieClip, which
// writes the root timeline's MovieClipRef into lpOutRef and returns it.
MovieClipRef* FileRef::GetRootMovieClip(MovieClipRef* lpOutRef) const
{
    CGS_ASSERT(mpFileInstance != 0, "mpFileInst");

    return static_cast<FlaptFileInstance*>(mpFileInstance)->GetRootMovieClip(lpOutRef);
}

}
