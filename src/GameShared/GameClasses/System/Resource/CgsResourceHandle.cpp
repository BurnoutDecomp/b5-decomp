#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceBasePool.h"  // CgsResource::Entry (mID)
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT

// CgsResource::ResourceHandle -- a handle onto a loaded resource (resource memory + source
// pool entry). This TU owns the out-of-line accessor the X360 ARTIST build emitted.

namespace CgsResource
{
    // Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x828D8438
    //   (CgsResource::ResourceHandle::GetResourceId).  [ledger function for this TU]
    //
    // Return the resource's identity hash, read from the source pool entry:
    //   assert mpSourceEntry (+0x04) != NULL  -- "mpSourceEntry != NULL"
    //   return *(u64*)mpSourceEntry           -- the Entry's first 8 bytes == Entry::mID
    // The asm loads mpSourceEntry from handle +0x04, asserts it non-null, then does a single
    // 8-byte load from offset 0 of the entry (ld r11,0(r11)) and stores it into the ID return
    // slot. The Entry's first member is `ID mID` (a u64 hash), so this is mpSourceEntry->mID
    // accessed BY NAME. (The asm's baked assert file/line are dropped per project convention.)
    ID ResourceHandle::GetResourceId() const
    {
        CGS_ASSERT(mpSourceEntry != 0, "mpSourceEntry != NULL");
        return mpSourceEntry->mID;
    }

    // The null handle sentinel (X360 .data qword @0x82FB3688 == {NULL, NULL}, read from the
    // decrypted XEX). Compared against by e.g. ColourCalibrationScreen::Update.
    const ResourceHandle NULLResourceHandle = { 0, 0 };
}
