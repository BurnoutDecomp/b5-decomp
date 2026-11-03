#include "SDKs/RenderEngineClub/MAIN/components/src/indexbuffer.h"
#include "rw/rwcore_structs.h"
#include <cstring>

namespace renderengine
{
// ARTIST 82B62100: the allocator supplies the native header and physical data.
IndexBuffer* IndexBuffer::Initialize(const rw::Resource& resource, const Parameters& parameters)
{
    IndexBuffer* const buffer = static_cast<IndexBuffer*>(resource.m_baseResources[0]);
    std::memset(buffer, 0, sizeof(IndexBuffer));
    buffer->ReferenceCount = 1;
    buffer->BaseFlush = D3DFLUSH_INITIAL_VALUE;
    buffer->Address = reinterpret_cast<u32>(resource.m_baseResources[2]);
    const u32 bytesPerIndex = parameters.depth == 16 ? 2u : 4u;
    buffer->Size = (bytesPerIndex * parameters.numIndices + 15u) & ~15u;
    buffer->m_numIndices = parameters.numIndices;
    buffer->Common = 2u | (parameters.depth == 32 ? 0xC0000000u : 0x20000000u);
    buffer->Xbox2CheckPhysicalMemoryFlags();
    return buffer;
}

// ARTIST 82B60760.
bool IndexBuffer::Lock(u32 flags, Locked& locked)
{
    if (!Address) return false;
    DWORD sdkFlags = 0;
    if ((flags & LOCK_READ) && !(flags & LOCK_WRITE))
    {
        if (XMemGetPageSize(reinterpret_cast<void*>(Address)) <= 4096)
            sdkFlags = D3DLOCK_READONLY;
    }
    else if ((flags & LOCK_WRITE) && (flags & LOCK_NOOVERWRITE))
        sdkFlags = D3DLOCK_NOOVERWRITE;
    void* const indices = D3DIndexBuffer_Lock(this, 0, 0, sdkFlags);
    locked.indices = indices;
    if (!indices) return false;
    locked.indexStride = static_cast<s32>(Common) >= 0 ? 1 : 6;
    return true;
}

// Native inline call at OcclusionCullManager::Construct 827F2334.
void IndexBuffer::Unlock(const Locked&)
{
    D3DIndexBuffer_Unlock(this);
}

// ARTIST 82B60730. r3 is this; r4 is the three-word output reference.
// DecFIGS indexbuffer.h:119 declares the const void member function.
void IndexBuffer::GetParameters(Parameters& parameters) const
{
    parameters.type = 0;
    parameters.depth = (Common & D3DINDEXBUFFER_INDEX32) ? 32u : 16u;
    parameters.numIndices = m_numIndices;
}

// ARTIST 82B60818. The SDK header's Address is passed directly to the
// native protection query; unlike vertex buffers, no low bits are stripped.
void IndexBuffer::Xbox2CheckPhysicalMemoryFlags()
{
    if (Address)
    {
        const DWORD flags = XQueryMemoryProtect(reinterpret_cast<void*>(Address));
        if (flags & (PAGE_NOCACHE | PAGE_WRITECOMBINE))
            Common &= ~D3DCOMMON_CPU_CACHED_MEMORY;
        else
            Common |= D3DCOMMON_CPU_CACHED_MEMORY;
    }
}
}
