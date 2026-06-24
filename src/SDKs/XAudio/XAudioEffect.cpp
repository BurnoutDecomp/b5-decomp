#include "SDKs/XAudio/XAudioEffect.h"

#include "SDKs/XAudio/XAudioXMemMemoryManager.h" // gXMemMemoryManager

// ===========================================================================
// XAUDIO::CEffect -- bodies reconstructed from BURNOUT_X360_ARTIST.XEX.
// See XAudioEffect.h for the layout and asm notes.
// ===========================================================================

namespace XAUDIO
{

// Empty teardown -- the asm dtor only resets the vtable (off_82108FF4) and
// conditionally frees; no member release in this TU.
CEffect::~CEffect()
{
}

// @ 0x8295F4A8:
//   lwz  r11, 8(r3)   ; r11 = mpContext
//   li   r3, 0        ; return 0
//   stw  r11, 0(r4)   ; *ppContext = mpContext
s32 CEffect::GetContext(void** ppContext)
{
    *ppContext = mpContext;
    return 0;
}

// @ 0x82961060 (free arm): release `this` through the XAudio XMem manager
// singleton with the baked attribute word.
void CEffect::operator delete(void* pMemory)
{
    gXMemMemoryManager.XMemFree(pMemory, KU_XMEM_FREE_ATTRIBUTES);
}

} // namespace XAUDIO
