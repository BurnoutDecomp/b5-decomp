#pragma once

// ===========================================================================
// XAUDIO::CXMemMemoryManager -- the XAudio memory-manager façade that routes
// XAudio's allocations through the Xbox-360 XDK XMem heap APIs in
// BURNOUT_X360_ARTIST.XEX. This header is the canonical OWNING home for:
//
//     XAUDIO::CXMemMemoryManager::XMemAlloc  @ 0x82964D78
//     XAUDIO::CXMemMemoryManager::XMemFree   @ 0x82964D88
//
// There is no Feb-2007 leak source and no DWARF for this TU. Both methods are
// thin forwarders: the X360 asm tail-branches (`b XMemAlloc` / `b XMemFree`)
// to the platform XMem free functions after dropping the `this` argument and
// shifting (a2, a3) into the first two argument registers. `XAUDIO` is an
// external middleware boundary, so its identifiers are preserved verbatim per
// the naming convention.
//
// XMemAlloc @ 0x82964D78 (asm): mr r3,r4 ; mr r4,r5 ; b XMemAlloc
//     -> ::XMemAlloc(size /*a2*/, attributes /*a3*/)
// XMemFree  @ 0x82964D88 (asm): mr r3,r4 ; mr r4,r5 ; b XMemFree
//     -> ::XMemFree(pAddress /*a2*/, attributes /*a3*/)
//
// PLATFORM EXTERNS (flagged): XMemAlloc / XMemFree are the Xbox-360 XDK heap
// APIs (XMemAlloc(SIZE_T,DWORD) -> LPVOID ; XMemFree(LPVOID,DWORD) -> void).
// They are not part of the host build, so they are declared here as the X360
// platform boundary and supplied by the platform layer; this TU only routes
// to them.
// ===========================================================================

#include "types.hpp"

namespace XAUDIO
{

// The Xbox-360 XDK XMem heap APIs (platform boundary). Declared with the
// argument/return shapes the X360 call sites use (size/attributes for alloc,
// pointer/attributes for free). Supplied by the platform layer.
void* XMemAllocPlatform(u32 uSize, u32 uAttributes);
void  XMemFreePlatform(void* pAddress, u32 uAttributes);

class CXMemMemoryManager
{
public:
    // @ 0x82964D78 -- forward to the platform XMemAlloc, dropping `this`.
    void* XMemAlloc(u32 uSize, u32 uAttributes);

    // @ 0x82964D88 -- forward to the platform XMemFree, dropping `this`.
    void  XMemFree(void* pAddress, u32 uAttributes);
};

} // namespace XAUDIO
