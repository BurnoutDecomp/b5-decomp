#include "SDKs/D3D/CCommandBuffer.h"
#include "SDKs/D3D/CDevice.h"

#include <cstddef> // offsetof (uncalled _CCommandBufferAssertLayout)
#include <cstdint> // uintptr_t (X360 address math on the host pointer)

// ===========================================================================
// D3D::CCommandBuffer -- reconstructed from BURNOUT_X360_ARTIST.XEX. See
// CCommandBuffer.h for the layout and the bodied-vs-declaration-only split.
//
// Source-of-truth: X360 asm only (no DWARF, no reference source). The five
// methods below are pure software (linked-list edit / lock-guarded callback /
// cache-flush) recovered faithfully with no added control flow.
// ===========================================================================

// X360 kernel critical-section primitives (the same externs the Cgs file-system
// futex paths declare).
extern "C" void RtlEnterCriticalSection(void* lpCriticalSection);
extern "C" void RtlLeaveCriticalSection(void* lpCriticalSection);

// The XGRAPHICS global device slot. The X360 asm loads the stored value and then
// dereferences it once (two loads: `lwz r11, VdGlobalDevice@l(r11)` then
// `lwz r31, 0(r11)`) to reach the internal D3D::CDevice, so the symbol is a
// pointer-to-(CDevice*).
extern "C" D3D::CDevice** VdGlobalDevice;

namespace D3D
{

// FLAG PC-platform leaf: X360 CPU-virtual -> GPU-visible physical-mirror address
// translation. The GPU reads the write-combined physical alias of a CPU buffer;
// this is the fixed X360 formula the asm inlines (srwi/addi/rlwinm/subf) and is
// reproduced verbatim. Meaningless off the X360 -- fixed hardware behaviour.
static inline u32 ToGpuPhysical(u32 uVirtual)
{
    return (((uVirtual >> 20) + 512) & 0x1000) + (uVirtual & 0x1FFFFFFFu) - 0x40000000u;
}

// --- @ 0x8294C3D0 ----------------------------------------------------------
// Flush the CPU cache over the fixed cached allocation's GPU-visible physical
// range [start, start+size-1]. The asm reads the size first and does nothing
// when it is zero.
void CCommandBuffer::FlushCachedAllocation()
{
    if (muCachedSize != 0)
    {
        u32 uStart = static_cast<u32>(reinterpret_cast<uintptr_t>(mpCachedAllocation));
        u32 uEnd   = uStart + muCachedSize - 1;
        FlushCachedMemory(ToGpuPhysical(uStart), ToGpuPhysical(uEnd), 0);
    }
}

// --- @ 0x8294C8F8 ----------------------------------------------------------
// Tear down the buffer. A fixed buffer (mpCachedAllocation != 0) is cache-flushed
// then returned to the XDK heap; a growable buffer runs its free callback. In
// both cases the buffer is finally unlinked from the device debug list.
void CCommandBuffer::Destroy()
{
    if (mpCachedAllocation != nullptr)
    {
        FlushCachedAllocation();
        XMemFree(mpCachedAllocation, 0xB1800000u); // lis r4,-0x4E80 -> 0xB1800000
    }
    else
    {
        GrowableFree();
    }
    DebugDestroy();
}

// --- @ 0x8294C5E0 ----------------------------------------------------------
// Unlink a growable buffer from the device-global debug command-buffer list,
// under the device command-buffer lock. Fixed buffers (mpCachedAllocation != 0)
// were never on the list, so they early-out.
void CCommandBuffer::DebugDestroy()
{
    if (mpCachedAllocation != nullptr)
        return;

    CDevice* pDevice = *VdGlobalDevice;
    RtlEnterCriticalSection(&pDevice->mCommandBufferLock);

    if (pDevice->mpCommandBufferList == this)
    {
        pDevice->mpCommandBufferList = mpNextDebug;
    }
    else
    {
        CCommandBuffer* pPrev = pDevice->mpCommandBufferList;
        CCommandBuffer* pNode = pPrev->mpNextDebug;
        while (pNode != this)
        {
            pPrev = pNode;
            pNode = pNode->mpNextDebug;
        }
        pPrev->mpNextDebug = pNode->mpNextDebug;
    }

    RtlLeaveCriticalSection(&pDevice->mCommandBufferLock);
}

// --- @ 0x8294C878 ----------------------------------------------------------
// Release the growable backing store: under the device lock, reset the debug
// tracking bookkeeping (head/tail/count/reclaimed flag) and run the free
// callback with the stored context. The bookkeeping is cleared BEFORE the
// callback, exactly as the asm stores it.
void CCommandBuffer::GrowableFree()
{
    CDevice* pDevice = *VdGlobalDevice;
    RtlEnterCriticalSection(&pDevice->mCommandBufferLock);

    void*   pContext = mpAllocatorContext;
    TFreeFn pfnFree  = mpfnFree;

    muDebugBlockListPhys = 0;
    mpDebugBlockTail     = nullptr;
    miDebugAllocCount    = -1;
    muReclaimed          = 0;

    pfnFree(pContext);

    RtlLeaveCriticalSection(&pDevice->mCommandBufferLock);
}

// --- @ 0x8294CB20 ----------------------------------------------------------
// Serve a growable allocation: clamp the requested size up to the minimum, take
// the device lock, run the allocate callback, then register+validate the result
// through the debug allocator. Returns the allocated address on success.
s32 CCommandBuffer::GrowableAllocate(s32 iCount, u32* puSize, s32 iAlignment)
{
    CDevice* pDevice = *VdGlobalDevice;

    if (*puSize < muMinSize)
        *puSize = muMinSize;

    RtlEnterCriticalSection(&pDevice->mCommandBufferLock);

    s32 iAllocation = mpfnAllocate(mpAllocatorContext, iCount, puSize, iAlignment);
    if (DebugAddAllocation(iAllocation, static_cast<s32>(*puSize)))
    {
        RtlLeaveCriticalSection(&pDevice->mCommandBufferLock);
        return iAllocation;
    }

    // Faithful quirk: on the debug-allocator failure path the X360 code returns
    // WITHOUT releasing the lock (a conflicting allocation has already trapped
    // inside DebugReclaim). Do NOT add an unlock here.
    return 0;
}

// ===========================================================================
// DECLARATION-ONLY -- FLAGGED. Left unbodied here; both are the X360 DEBUG
// allocator's overlap detector and require inputs the reconstruction rules will
// not let me fabricate:
//
//   DebugReclaim      @ 0x8294C668
//     * Its conflict diagnostic fires a fixed sequence of DbgPrint() calls whose
//       format strings are verbatim X360 rodata -- and every one is TRUNCATED in
//       the dossier ("Please fix your growable command buffer"..., "Conflicting
//       allocation: Address 0x%x, S"..., "ERR[D3D]: The range to be reclaimed
//       ove"..., etc.). CGS-style assert/diagnostic strings must be reproduced
//       exactly; I cannot recover the full text, so bodying it would fabricate
//       rodata. BLOCKED on the truncated diagnostic strings.
//
//   DebugAddAllocation @ 0x8294C958
//     * Builds/extends a physically-chained debug tracking block whose `next`
//       links are stored as GPU physical-mirror addresses and are kept coherent
//       with inline `dcbf` cache-line flushes -- X360 GPU hardware behaviour that
//       is only fully attested together with its consumer DebugReclaim (blocked
//       above). Modeling the physical-vs-virtual pointer duality of that chain
//       without the reader is a guess. BLOCKED pending DebugReclaim.
//
// GrowableAllocate above compiles against DebugAddAllocation's declaration and
// links against its trap-stub until this is bodied.
// ===========================================================================

// Pointer-invariant / relative layout facts (uncalled). mpCachedAllocation is
// the FIRST host pointer, so its own offset is pre-widening and pinned
// absolutely; fields past it shift by the 4->8 pointer widening on LLP64 and are
// only checked for ordering / contiguous 4-byte stride.
void _CCommandBufferAssertLayout()
{
    static_assert(offsetof(CCommandBuffer, mpCachedAllocation) == 0x98,
                  "fixed-allocation pointer @ +0x98 (word[38], first host pointer)");
    static_assert(offsetof(CCommandBuffer, muCachedSize) > offsetof(CCommandBuffer, mpCachedAllocation),
                  "cached size follows the cached-allocation pointer");
    static_assert(offsetof(CCommandBuffer, muReclaimed) < offsetof(CCommandBuffer, muWord47),
                  "reclaimed flag precedes the reclaim bookkeeping words");
    static_assert(offsetof(CCommandBuffer, muWord48) - offsetof(CCommandBuffer, muWord47) == 4,
                  "contiguous debug-reclaim bookkeeping words");
    static_assert(offsetof(CCommandBuffer, muDebugBlockListPhys) - offsetof(CCommandBuffer, muWord48) == 4,
                  "debug-block list-head physical addr follows the bookkeeping words");
    static_assert(offsetof(CCommandBuffer, miDebugAllocCount) > offsetof(CCommandBuffer, mpDebugBlockTail),
                  "debug alloc count follows the current-block pointer");
}

} // namespace D3D
