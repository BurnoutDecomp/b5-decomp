// CgsXboxPlatformPC.cpp -- existing Xbox platform API names with PC producers.
//
// FLAG PC-platform leaf: XPhysicalAlloc/Free use real low4GiB Windows virtual
// reservations on this CPU/software platform. The requested size/alignment is
// honored; no Xbox GPU/DMA physical-address identity is claimed. The existing
// PC cached read/write protection policy remains: Xbox physical-address and
// cache/large-page hint words are not passed as Windows allocation flags.
// The actual reservation base is released by the same LowMemory API family.

#include "GameShared/GameClasses/Memory/PC/CgsLowMemoryPC.h"
#include <windows.h>
#include <cstring>   // existing XMemCpy ownership note below

extern "C" void *XPhysicalAlloc(unsigned long dwSize, unsigned long /*ulPhysicalAddress*/,
                                unsigned long dwAlignment, unsigned long /*flProtect*/)
{
    const size_t lnAlignment = dwAlignment ? static_cast<size_t>(dwAlignment) : 16u;
    return CgsMemory::LowMemory::ReserveAligned(
        static_cast<size_t>(dwSize), lnAlignment, PAGE_READWRITE);
}

extern "C" void XPhysicalFree(void *lpAddress)
{
    CgsMemory::LowMemory::Release(lpAddress);
}

// XMemCpy is NOT defined here: the mounted XenonD3D9Shims.cpp (src/pc/gcm/
// renderengine) already carries the one host definition; the vendor rw/audio
// declarations bind to it.
