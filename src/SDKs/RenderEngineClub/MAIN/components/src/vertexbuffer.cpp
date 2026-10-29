#include "SDKs/RenderEngineClub/MAIN/components/src/vertexbuffer.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include "rw/rwcore_structs.h"
#include <xgraphics.h>
#include <cstring>

namespace renderengine
{
// ARTIST sub_82B61FE0, reached by Xbox2UnSet's tail branch at 82B63058.
// GetStreamSource adds a reference; release it before comparing the binding.
void VertexBuffer::Xbox2UnSet()
{
    D3DDevice* const device = ::gpD3DDevice;
    if (D3DResource_IsSet(this, device))
    {
        for (u32 stream = 0; stream < 16; ++stream)
        {
            UINT offset, stride;
            D3DVertexBuffer* const buffer = D3DDevice_GetStreamSource(device, stream, &offset, &stride);
            if (buffer)
            {
                D3DResource_Release(buffer);
                if (buffer == this)
                    D3DDevice_SetStreamSource_Inline(device, stream, 0, 0, 0);
            }
        }
    }
}

// ARTIST 82B62ED8; the resource has all five native allocator lanes.
VertexBuffer* VertexBuffer::Initialize(const rw::Resource& resource, const Parameters& parameters)
{
    VertexBuffer* const buffer = static_cast<VertexBuffer*>(resource.m_baseResources[0]);
    std::memset(buffer, 0, sizeof(VertexBuffer));
    buffer->Common = 1;
    buffer->Fence = 0;
    buffer->ReadFence = 0;
    buffer->ReferenceCount = 1;
    buffer->BaseFlush = D3DFLUSH_INITIAL_VALUE;
    buffer->Format.dword[0] |= 3;
    buffer->m_bufferSize = parameters.bufferSize;
    buffer->m_type = parameters.type & 0xFF;
    XGSetVertexBufferHeader(parameters.bufferSize, 0, D3DPOOL_MANAGED,
        reinterpret_cast<u32>(resource.m_baseResources[2]), buffer);
    buffer->Xbox2CheckPhysicalMemoryFlags();
    return buffer;
}

// ARTIST 82B63778: descriptor returned by value, using the native hidden result.
rw::ResourceDescriptor VertexBuffer::GetResourceDescriptor(const Parameters& parameters)
{
    rw::ResourceDescriptor descriptor;
    for (u32 i = 0; i != 5; ++i)
    {
        descriptor.m_baseResourceDescriptors[i].m_size = 0;
        descriptor.m_baseResourceDescriptors[i].m_alignment = 1;
    }
    descriptor.m_baseResourceDescriptors[0].m_size = sizeof(VertexBuffer);
    descriptor.m_baseResourceDescriptors[0].m_alignment = 4;
    descriptor.m_baseResourceDescriptors[2].m_size = parameters.bufferSize;
    descriptor.m_baseResourceDescriptors[2].m_alignment = 4;
    return descriptor;
}

// ARTIST 82B61130.
void VertexBuffer::GetParameters(Parameters& parameters) const
{
    parameters.type = m_type;
    parameters.bufferSize = m_bufferSize;
}

// ARTIST 82B61148.
void VertexBuffer::Xbox2CheckPhysicalMemoryFlags()
{
    void* const base = reinterpret_cast<void*>(Format.dword[0] & ~3u);
    if (base)
    {
        const DWORD flags = XQueryMemoryProtect(base);
        if (flags & (PAGE_NOCACHE | PAGE_WRITECOMBINE))
            Common &= ~D3DCOMMON_CPU_CACHED_MEMORY;
        else
            Common |= D3DCOMMON_CPU_CACHED_MEMORY;
    }
}

// ARTIST 8227D0A0: offset and size describe the iterator; SDK maps the whole VB.
bool VertexBuffer::Lock(u32 flags, BatchIterator& iterator, u32 offset, u32 size)
{
    if (!size) size = m_bufferSize;
    Xbox2UnSet();
    DWORD lockFlags = 0;
    if ((flags & LOCK_READ) && !(flags & LOCK_WRITE))
    {
        if (XMemGetPageSize(reinterpret_cast<void*>(Format.dword[0] & ~3u)) <= 4096)
            lockFlags = D3DLOCK_READONLY;
    }
    else if ((flags & LOCK_WRITE) && (flags & LOCK_DISCARD))
        lockFlags = D3DLOCK_NOOVERWRITE;
    u8* const data = static_cast<u8*>(D3DVertexBuffer_Lock(this, 0, 0, lockFlags));
    iterator.m_bufferSize = size;
    iterator.m_flags = flags;
    iterator.m_vb = this;
    iterator.m_lockedBuffer = data + offset;
    return true;
}

// The native inline Unlock is also emitted at 82B63FF0 by VertexBufferHelper.
void VertexBuffer::Unlock(const BatchIterator&)
{
    D3DVertexBuffer_Unlock(this);
}

// ARTIST 82B62F70.
void VertexBuffer::Destruct(VertexBuffer* buffer)
{
    D3DDevice* const device = ::gpD3DDevice;
    buffer->Xbox2UnSet();
    if (!D3DResource_IsSet(buffer, device))
    {
        if (buffer->Fence)
        {
            D3DResource_BlockUntilNotBusy(buffer);
            buffer->Fence = 0;
        }
        D3DDevice_InvalidateGpuCache(device,
            reinterpret_cast<void*>(buffer->Format.dword[0] & ~3u), buffer->m_bufferSize, 0);
    }
}

// ARTIST 82B62FF8.
void VertexBuffer::Release()
{
    if (Common)
    {
        Destruct(this);
        if (Common & D3DCOMMON_D3DCREATED) D3DResource_Release(this);
        Common = 0;
        Format.dword[0] &= 3;
    }
}
}
