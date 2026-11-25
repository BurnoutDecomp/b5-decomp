#include "SDKs/XAudio/XAudioXMemMemoryManager.h"

#include <xtl.h>

// ===========================================================================
// XAUDIO::CXMemMemoryManager -- bodies reconstructed from
// BURNOUT_X360_ARTIST.XEX. Both methods drop the `this`/manager argument and
// tail-forward (size/ptr, attributes) to the XDK XMem heap APIs.
// ===========================================================================

namespace XAUDIO
{

// @ 0x82964D78 -- mr r3,r4 ; mr r4,r5 ; b XMemAlloc
void* CXMemMemoryManager::XMemAlloc(u32 uSize, u32 uAttributes)
{
    return ::XMemAlloc(uSize, uAttributes);
}

// @ 0x82964D88 -- mr r3,r4 ; mr r4,r5 ; b XMemFree
void CXMemMemoryManager::XMemFree(void* pAddress, u32 uAttributes)
{
    ::XMemFree(pAddress, uAttributes);
}

} // namespace XAUDIO
