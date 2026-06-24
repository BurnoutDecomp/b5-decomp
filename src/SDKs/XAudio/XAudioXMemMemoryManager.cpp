#include "SDKs/XAudio/XAudioXMemMemoryManager.h"

// ===========================================================================
// XAUDIO::CXMemMemoryManager -- bodies reconstructed from
// BURNOUT_X360_ARTIST.XEX. Both methods drop the `this`/manager argument and
// tail-forward (size/ptr, attributes) to the platform XMem heap APIs. See
// XAudioXMemMemoryManager.h for the asm notes and the platform-extern flag.
// ===========================================================================

namespace XAUDIO
{

// @ 0x82964D78 -- mr r3,r4 ; mr r4,r5 ; b XMemAlloc
void* CXMemMemoryManager::XMemAlloc(u32 uSize, u32 uAttributes)
{
    return XMemAllocPlatform(uSize, uAttributes);
}

// @ 0x82964D88 -- mr r3,r4 ; mr r4,r5 ; b XMemFree
void CXMemMemoryManager::XMemFree(void* pAddress, u32 uAttributes)
{
    XMemFreePlatform(pAddress, uAttributes);
}

} // namespace XAUDIO
