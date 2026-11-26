#include <cstring>
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
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

// ARTIST 0x8246EC90.
MovieClipRef FileRef::FindComponent(const char* lpcName) const
{
    CGS_ASSERT_AT(mpFileInst != 0, "mpFileInst", "..\\..\\..\\GameSource\\Gui/Flapt/BrnFlaptFileRef.cpp", 45);
    CGS_ASSERT_AT(lpcName != 0, "lpcName", "..\\..\\..\\GameSource\\Gui/Flapt/BrnFlaptFileRef.cpp", 46);

    const u32 luHash = CgsContainers::CgsHash::CalculateHash(const_cast<char*>(lpcName), static_cast<u32>(std::strlen(lpcName)));
    MovieClipRef lComponent;
    mpFileInst->FindComponent(luHash, &lComponent, lpcName);
    return lComponent;
}

// ---- GetRootMovieClip @ 0x8246B410 ---------------------------------------
// Assert the handle, then forward to FlaptFileInstance::GetRootMovieClip, which
// writes the root timeline's MovieClipRef into lpOutRef and returns it.
MovieClipRef FileRef::GetRootMovieClip() const
{
    CGS_ASSERT_AT(mpFileInst != 0, "mpFileInst", "..\\..\\..\\GameSource\\Gui/Flapt/BrnFlaptFileRef.cpp", 63);
    return mpFileInst->GetRootMovieClip();
}

}
