#pragma once

// ===========================================================================
// D3D::CCommandBuffer -- the X360 "growable command buffer" allocator inside
// BURNOUT_X360_ARTIST.XEX. This header is the canonical OWNING home for the
// CCommandBuffer TYPE and its 7 methods (homed in CCommandBuffer.cpp).
//
// A CCommandBuffer wraps one GPU command-stream backing buffer. It comes in two
// flavours selected by mpCachedAllocation (+0x98):
//   * a FIXED buffer (mpCachedAllocation != 0) allocated once from the XDK heap
//     (XMemFree'd + cache-flushed on Destroy); or
//   * a GROWABLE buffer (mpCachedAllocation == 0) whose backing store is served
//     on demand by a caller-supplied allocate/free callback pair
//     (mpfnAllocate/mpfnFree) under the device's global command-buffer lock.
// Growable buffers are additionally threaded onto a device-global debug list so
// the debug allocator can detect two live command buffers claiming overlapping
// GPU memory (DebugReclaim / DebugAddAllocation).
//
// LAYOUT -- there is NO DWARF and NO reference source for this TU (same footing
// as D3D::CDevice / D3D::CBlocker). The member shape below is reconstructed
// purely from the byte offsets the bodied methods load/store in the X360 asm;
// only the words those methods touch are named, the rest are opaque padding.
// The object uses 4-byte pointers on the X360, so absolute byte offsets past the
// first host pointer (mpCachedAllocation) shift on an LLP64 host and are NOT
// asserted -- everything is accessed by name.
//
// The device-global list head + lock the debug/growable methods reach live on
// D3D::CDevice (mpCommandBufferList / mCommandBufferLock) and are reached through
// the friend relationship declared in CDevice.h -- nothing is offset-hacked.
//
// `D3D` is the X360 graphics-SDK boundary, so its identifiers (CCommandBuffer and
// the method names) are preserved verbatim per the naming convention.
// ===========================================================================

#include "types.hpp"

namespace D3D
{

class CCommandBuffer;

// ---- raw Xbox 360 XDK / GPU-SDK platform externs --------------------------
// Destroy frees a fixed buffer straight through the XDK heap API (bl XMemFree;
// r3=pAddress, r4=attributes) -- the same extern the capture/rwmovie codecs use.
extern void XMemFree(void* pAddress, u32 uAttributes);

// D3D::FlushCachedMemory(startPhys, endPhys, flags) -- flush the CPU cache over a
// GPU-visible physical-address range. Sibling D3D free function, not yet homed;
// declared here so the bodied methods compile (its body is its own TU).
extern s32 FlushCachedMemory(u32 uStartPhys, u32 uEndPhys, s32 iFlags);

class CCommandBuffer
{
public:
    // Growable backing-store callbacks (function-pointer members at +0xAC/+0xB0,
    // invoked via mtctr/bctrl in the asm).
    //   allocate: (context, count, in/out size, alignment) -> allocated address
    //   free:     (context)
    typedef s32  (*TAllocateFn)(void* pContext, s32 iCount, u32* puSize, s32 iAlignment);
    typedef void (*TFreeFn)(void* pContext);

    // -------- FAITHFULLY BODIED (recovered from X360 asm) --------

    // @ 0x8294C8F8 -- tear down: flush+XMemFree a fixed buffer, or run the
    // growable free callback; then unlink from the device debug list.
    void Destroy();

    // @ 0x8294C3D0 -- flush the CPU cache over the fixed cached allocation's
    // GPU-visible physical range (no-op when the cached size is zero).
    void FlushCachedAllocation();

    // @ 0x8294C5E0 -- unlink a growable buffer from the device-global debug
    // command-buffer list, under the device command-buffer lock.
    void DebugDestroy();

    // @ 0x8294C878 -- release the growable backing store: reset the debug-block
    // bookkeeping and run the free callback, under the device lock.
    void GrowableFree();

    // @ 0x8294CB20 -- clamp the requested size to the minimum, take the device
    // lock, run the allocate callback, then register+validate the result via
    // DebugAddAllocation. Returns the allocated address, or 0 on failure.
    s32 GrowableAllocate(s32 iCount, u32* puSize, s32 iAlignment);

    // -------- DECLARATION-ONLY (FLAGGED in CCommandBuffer.cpp) --------
    // The two debug-allocator methods are left unbodied here: see the .cpp for
    // the precise blocking reasons (truncated assert rodata + inline X360 GPU
    // cache-coherency ops on a physically-chained debug tracking structure).

    // @ 0x8294C958 -- append (address,size) to the current debug tracking block,
    // growing a new block when full; reclaims stale conflicting buffers first.
    s32 DebugAddAllocation(s32 iAllocation, s32 iSize);

    // @ 0x8294C668 -- walk the device debug list looking for a live buffer whose
    // tracked allocations overlap [uAddress, uAddress+iSize); reclaim it if safe,
    // otherwise fire the conflict diagnostic and trap.
    static s32 DebugReclaim(u32 uAddress, s32 iSize, CCommandBuffer* pBuffer, void* pContext);

private:
    u8    mPad0000[0x98];        // +0x0000  opaque header (not attested here)
    void* mpCachedAllocation;    // +0x0098  word[38] fixed XDK buffer (0 => growable)
    u32   muCachedSize;          // +0x009C  word[39] size of the fixed buffer
    CCommandBuffer* mpNextDebug; // +0x00A0  word[40] next in the device debug list
    void* mpAllocatorContext;    // +0x00A4  word[41] context passed to the callbacks
    u32   muMinSize;             // +0x00A8  word[42] minimum allocation size (clamp floor)
    TAllocateFn mpfnAllocate;    // +0x00AC  word[43] growable allocate callback
    TFreeFn     mpfnFree;        // +0x00B0  word[44] growable free callback
    u32   muWord45;              // +0x00B4  word[45] (not attested by bodied funcs)
    u32   muReclaimed;           // +0x00B8  word[46] debug "reclaimed" flag (reset by GrowableFree)
    u32   muWord47;              // +0x00BC  word[47] (debug-reclaim bookkeeping)
    u32   muWord48;              // +0x00C0  word[48] (debug-reclaim bookkeeping)
    u32   muDebugBlockListPhys;  // +0x00C4  word[49] physical addr of the first debug block
    void* mpDebugBlockTail;      // +0x00C8  word[50] current debug tracking block (virtual)
    s32   miDebugAllocCount;     // +0x00CC  word[51] live entries in the current block (-1 = none)

    friend void _CCommandBufferAssertLayout();
};

} // namespace D3D
