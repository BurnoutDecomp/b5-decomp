#include "SDKs/XAudio/XAudioCriticalSection.h"

#include "SDKs/XAudio/XAudioXMemMemoryManager.h" // gXMemMemoryManager

// ===========================================================================
// XAUDIO::CCriticalSection -- bodies reconstructed from BURNOUT_X360_ARTIST.XEX.
// See XAudioCriticalSection.h for the layout and asm notes.
//
//   `scalar deleting destructor' @ 0x8295F3B8: store vtable off_82108FF0 into
//   *this; if (flag & 1) XMemFree(&unk_83222C40, this, 0x61820000). The host
//   compiler emits this thunk from the virtual dtor + the class operator delete.
// ===========================================================================

namespace XAUDIO
{

// Empty teardown -- the asm dtor only resets the vtable and conditionally frees;
// there is no member release in this TU.
CCriticalSection::~CCriticalSection()
{
}

// @ 0x8295F3B8 (free arm): the deleting destructor releases `this` through the
// XAudio XMem manager singleton with the baked attribute word.
void CCriticalSection::operator delete(void* pMemory)
{
    gXMemMemoryManager.XMemFree(pMemory, KU_XMEM_FREE_ATTRIBUTES);
}

} // namespace XAUDIO
