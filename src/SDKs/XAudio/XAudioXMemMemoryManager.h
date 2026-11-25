#pragma once

// ===========================================================================
// XAUDIO::CXMemMemoryManager -- the XAudio memory-manager façade that routes
// XAudio's allocations through the Xbox-360 XDK XMem heap APIs in
// BURNOUT_X360_ARTIST.XEX. This header is the canonical OWNING home for:
//
//     XAUDIO::CXMemMemoryManager::XMemAlloc  @ 0x82964D78
//     XAUDIO::CXMemMemoryManager::XMemFree   @ 0x82964D88
//
// There is no reference source and no DWARF for this TU. Both methods are
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
// ===========================================================================

#include "types.hpp"

namespace XAUDIO
{

class CXMemMemoryManager
{
public:
    // @ 0x82964D78 -- forward to the platform XMemAlloc, dropping `this`.
    void* XMemAlloc(u32 uSize, u32 uAttributes);

    // @ 0x82964D88 -- forward to the platform XMemFree, dropping `this`.
    void  XMemFree(void* pAddress, u32 uAttributes);
};

// The XAudio-wide singleton memory-manager instance. The X360 image references
// it by the fixed data address &unk_83222C40: every XAudio class-specific
// allocate/free routes through `gXMemMemoryManager.XMemAlloc/XMemFree(...)`
// (e.g. the scalar/vector deleting destructors of XAudio objects pass
// `(&unk_83222C40, this, 0x61820000)` to XMemFree). Declared here as the shared
// owning home; defined once in XAudioBatchAllocatedObject.cpp.
extern CXMemMemoryManager gXMemMemoryManager;

// The attribute word the X360 free sites bake in (`lis r5, 0x6182` -> r5 =
// 0x61820000) when releasing XAudio batch-allocated objects through XMemFree.
static const u32 KU_XMEM_FREE_ATTRIBUTES = 0x61820000u;

} // namespace XAUDIO
