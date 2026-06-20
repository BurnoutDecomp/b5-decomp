#include "SDKs/Packages/ICE/ICEMemory.hpp"
#include "GameSource/Resource/BrnResourceAllocator.h"   // BrnResource debug allocator (ICE debug-memory block)
#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT

// ICE::ICEMemory -- the ICE memory manager. It IS-A CgsMemory::HeapMalloc (the ICE
// general heap, at offset 0) and embeds a second HeapMalloc (mEditHeap, at +0x520)
// dedicated to the editable take-data buffers. Reconstructed from the X360 asm
// (Construct @0x82533468, GetMemory @0x8252CC80).

namespace ICE
{

// The ICE debug-memory pool: 0x1C0000 bytes (1.75 MB), 4-byte aligned, carved from
// the global debug allocator and handed to the embedded edit heap. X360: the
// descriptor built on Construct's stack (size 0x1C0000, alignment 4).
static const s32 KI_ICE_DEBUG_MEMORY_SIZE      = 1835008;   // ICEMemory.cpp:31 (0x1C0000)
static const s32 KI_ICE_DEBUG_MEMORY_ALIGNMENT = 4;

// ---------------------------------------------------------------------------
// ICEMemory::Construct(void* lpBuffer, s32 lnBufferSize)  @0x82533468
//
// Build the base ICE heap over the caller-supplied block, then allocate a 1.75 MB
// "ICE Debug Memory" block from the global debug allocator and build the embedded
// edit heap (mEditHeap, +0x520) over it.
// ---------------------------------------------------------------------------
void ICEMemory::Construct(void* lpBuffer, s32 lnBufferSize)
{
    // Base CgsMemory::HeapMalloc (offset 0) over the caller block.
    CgsMemory::HeapMalloc::Construct(lpBuffer, lnBufferSize);

    // Describe the debug-memory pool: a single base resource of 1.75 MB at 4-byte
    // alignment (the remaining base-resource slots stay empty -- alignment 1).
    rw::ResourceDescriptor lDescriptor = {};
    lDescriptor.m_baseResourceDescriptors[0].m_size      = (u32)KI_ICE_DEBUG_MEMORY_SIZE;
    lDescriptor.m_baseResourceDescriptors[0].m_alignment = (u32)KI_ICE_DEBUG_MEMORY_ALIGNMENT;
    for (u32 luIndex = 1; luIndex < 4; ++luIndex)
    {
        lDescriptor.m_baseResourceDescriptors[luIndex].m_size      = 0;
        lDescriptor.m_baseResourceDescriptors[luIndex].m_alignment = 1;
    }

    CGS_ASSERT(BrnResource::Allocators::mpInternalDebugAllocator != 0,
               "Allocators::mpInternalDebugAllocator");

    // Allocate the debug pool; the resource's first base pointer is the memory block.
    rw::Resource lResource = BrnResource::GetDebugAllocator()->Allocate(lDescriptor, "ICE Debug Memory");
    void* lpDebugMemory = lResource.m_baseResources[0];

    CGS_ASSERT(lpDebugMemory != 0, "lpResourcePtr != NULL");   // ICEMemory.cpp:60

    // Embedded edit heap (mEditHeap, +0x520) over the debug pool.
    mEditHeap.Construct(lpDebugMemory, KI_ICE_DEBUG_MEMORY_SIZE);
}

// ---------------------------------------------------------------------------
// ICEMemory::GetMemory(u32 luSize)  @0x8252CC80
//
// Allocate luSize bytes (4-byte aligned) from the embedded edit heap. On
// exhaustion, dump the base heap's allocations and assert "ICE out of memory".
// ---------------------------------------------------------------------------
void* ICEMemory::GetMemory(u32 luSize)
{
    void* lpMemory = mEditHeap.Malloc((s32)luSize, KI_ICE_DEBUG_MEMORY_ALIGNMENT);
    if (lpMemory == 0)
    {
        // Diagnostics: report the (base) ICE heap's live allocations, then assert.
        CgsMemory::HeapMalloc::PrintAllocations();
        CGS_ASSERT(false, "\nICE out of memory\n");   // ICEMemory.cpp:94
    }
    return lpMemory;
}

} // namespace ICE
