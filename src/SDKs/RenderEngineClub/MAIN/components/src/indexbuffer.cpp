// XDK size optimization: ARTIST compiled the renderengine core (0x82B60000..0x82B64xxx) for
// size (record-form cr0 tests), like the state objects' resource descriptors;
// IndexBuffer::Initialize 0x82B62100 matches only so.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
#include "SDKs/RenderEngineClub/MAIN/components/src/indexbuffer.h"
#include "rw/rwcore_structs.h"
#include <cstring>

// The native constructor calls the XDK libcMT memset provider at 82C0B180.
#pragma function(memset)

namespace renderengine
{
// ARTIST 82B62100: the allocator supplies the native header and physical data.
IndexBuffer* IndexBuffer::Initialize(const rw::Resource& resource, const Parameters& parameters)
{
    IndexBuffer* const buffer = static_cast<IndexBuffer*>(resource.m_baseResources[0]);
    memset(buffer, 0, sizeof(IndexBuffer));
    buffer->ReferenceCount = 1;
    buffer->Common = D3DCOMMON_TYPE_INDEXBUFFER;
    buffer->BaseFlush = D3DFLUSH_INITIAL_VALUE;
    buffer->Address = reinterpret_cast<u32>(resource.m_baseResources[2]);
    const u32 bytesPerIndex = parameters.depth == 16 ? 2u : 4u;
    buffer->Size = (bytesPerIndex * parameters.numIndices + 15u) & ~15u;
    buffer->m_numIndices = parameters.numIndices;
    const u32 formatFlags = parameters.depth == 32
        ? D3DINDEXBUFFER_INDEX32 | (GPUENDIAN_8IN32 << D3DINDEXBUFFER_ENDIAN_SHIFT)
        : GPUENDIAN_8IN16 << D3DINDEXBUFFER_ENDIAN_SHIFT;
    buffer->Common = D3DCOMMON_TYPE_INDEXBUFFER | formatFlags;
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
    locked.indexStride = (Common & D3DINDEXBUFFER_INDEX32) ? D3DFMT_INDEX32 : D3DFMT_INDEX16;
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
        if (XQueryMemoryProtect(reinterpret_cast<void*>(Address)) & (PAGE_NOCACHE | PAGE_WRITECOMBINE))
            Common &= ~D3DCOMMON_CPU_CACHED_MEMORY;
        else
            Common |= D3DCOMMON_CPU_CACHED_MEMORY;
    }
}
}
