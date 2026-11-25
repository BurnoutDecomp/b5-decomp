#include "SDKs/RenderEngineClub/MAIN/components/src/vertexbuffer.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include "rw/rwcore_structs.h"
#include <xgraphics.h>
#include <cstring>

// The native constructor calls the XDK libcMT memset provider at 82C0B180.
#pragma function(memset)

namespace renderengine
{
// ARTIST 82B61FE0, reached by Xbox2UnSet's tail branch at 82B63058.
// GetStreamSource adds a reference; release it before comparing the binding.
static void UnbindVertexStreams(VertexBuffer* lpBuffer)
{
    D3DDevice* const device = ::gpD3DDevice;
    if (D3DResource_IsSet(lpBuffer, device))
    {
        for (u32 stream = 0; stream < 16; ++stream)
        {
            UINT offset, stride;
            D3DVertexBuffer* const buffer = D3DDevice_GetStreamSource(device, stream, &offset, &stride);
            if (buffer)
            {
                D3DResource_Release(buffer);
                if (buffer == lpBuffer)
                    D3DDevice_SetStreamSource_Inline(device, stream, 0, 0, 0);
            }
        }
    }
}

// ARTIST 82B62ED8; the resource has all five native allocator lanes.
VertexBuffer* VertexBuffer::Initialize(const rw::Resource& resource, const Parameters& parameters)
{
    VertexBuffer* const buffer = static_cast<VertexBuffer*>(resource.m_baseResources[0]);
    memset(buffer, 0, sizeof(VertexBuffer));
    buffer->Common = D3DCOMMON_TYPE_VERTEXBUFFER;
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
    descriptor.m_baseResourceDescriptors[0] = rw::BaseResourceDescriptor(sizeof(VertexBuffer), 4);
    descriptor.m_baseResourceDescriptors[2] = rw::BaseResourceDescriptor(parameters.bufferSize, D3DVERTEXBUFFER_ALIGNMENT);
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
        u32 common = Common;
        if (flags & (PAGE_NOCACHE | PAGE_WRITECOMBINE))
            common &= ~D3DCOMMON_CPU_CACHED_MEMORY;
        else
            common |= D3DCOMMON_CPU_CACHED_MEMORY;
        Common = common;
    }
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
    UnbindVertexStreams(buffer);
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
        const u32 format = Format.dword[0] & 3;
        Common = 0;
        Format.dword[0] = format;
    }
}

// ARTIST 82B63058 forwards the vertex stream unbind to its resource owner.
void VertexBuffer::Xbox2UnSet()
{
    UnbindVertexStreams(this);
}
}
