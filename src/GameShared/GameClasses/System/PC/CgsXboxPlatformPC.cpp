// ============================================================================
// CgsXboxPlatformPC.cpp -- PC bodies for the Xbox 360 platform primitives the
// vendor rw::audio::core TUs declare (2026-08-25, faithful-audio-engine phase A2).
//
// FLAG [PC platform leaf]: these are the sanctioned host stand-ins for the XAM/
// xtl platform CRT the console links -- physically impossible on PC, so each is
// the minimal host-faithful equivalent:
//   XPhysicalAlloc / XPhysicalFree -- the console carves GPU/DMA-visible physical
//     memory (XMA input buffers, decoder rings). On the host plain aligned heap
//     memory is exactly equivalent for the software decode path (there is no
//     hardware DMA consumer; the software XMA HAL in CgsXmaHardwarePC reads the
//     same buffers through the CPU).
//   XMemCpy -- the console's optimized block copy; memcpy on the host.
//
// Signatures match the vendor declarations verbatim (System.cpp:39-41,
// Voice.cpp:19, Decoder.cpp, DelayLine.cpp -- extern "C", so ONE definition
// serves them all).
// ============================================================================

#include <cstring>   // memcpy
#include <malloc.h>  // _aligned_malloc / _aligned_free

extern "C" void *XPhysicalAlloc(unsigned long dwSize, unsigned long /*ulPhysicalAddress*/,
                                unsigned long dwAlignment, unsigned long /*flProtect*/)
{
    // The console alignment argument is a byte alignment (16/128/4096 at the rwaudio
    // call sites); 0 means "default" -- map it to the allocator minimum.
    unsigned long lAlignment = dwAlignment ? dwAlignment : 16;
    return _aligned_malloc(dwSize, lAlignment);
}

extern "C" void XPhysicalFree(void *lpAddress)
{
    _aligned_free(lpAddress);
}

// XMemCpy is NOT defined here: the mounted XenonD3D9Shims.cpp (src/pc/gcm/
// renderengine) already carries the one host definition; the vendor rw/audio
// declarations bind to it.
