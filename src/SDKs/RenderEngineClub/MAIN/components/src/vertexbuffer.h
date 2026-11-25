#pragma once

#include "types.hpp"
#include <xtl.h>

namespace rw { struct Resource; struct ResourceDescriptor; }

namespace renderengine
{
// ARTIST 82B62ED8: the real SDK header, then buffer size and resource type.
class VertexBuffer : public D3DVertexBuffer
{
public:
    enum { MAXNUMELEMENTS = 16, LOCK_READ = 1, LOCK_WRITE = 2,
           LOCK_DISCARD = 4, LOCK_NOOVERWRITE = 8, LOCK_READWRITE = 3 };

    struct Parameters
    {
        u32 type;
        u32 bufferSize;

        void SetType(u32 value) { type = value; }
        void SetBufferSize(u32 value) { bufferSize = value; }
        u32 GetType() const { return type; }
        u32 GetBufferSize() const { return bufferSize; }
    };

    struct BatchIterator
    {
        void* GetLockedBuffer() const { return m_lockedBuffer; }
        u32 GetLockedBufferSize() const { return m_bufferSize; }
        u32 GetFlags() const { return m_flags; }

        void Initialize(void* lpData, u32 luSize, VertexBuffer* lpVertexBuffer, u32 luFlags)
        {
            m_bufferSize = luSize;
            m_flags = luFlags;
            m_vb = lpVertexBuffer;
            m_lockedBuffer = static_cast<u8*>(lpData);
        }

    protected:
        friend class VertexBuffer;
        u8* m_lockedBuffer;
        VertexBuffer* m_vb;
        u32 m_flags;
        u32 m_bufferSize;
    };

    static rw::ResourceDescriptor GetResourceDescriptor(const Parameters&);
    static VertexBuffer* Initialize(const rw::Resource&, const Parameters&);
    static void Destruct(VertexBuffer*);
    void Release();
    void GetParameters(Parameters&) const;
    bool Lock(u32 flags, BatchIterator& iterator, u32 offset = 0, u32 size = 0)
    {
        if (size == 0)
            size = m_bufferSize;
        Xbox2UnSet();

        DWORD lockFlags = 0;
        if (flags & LOCK_READ)
        {
            if (!(flags & LOCK_WRITE))
            {
                if (XMemGetPageSize(reinterpret_cast<void*>(Format.dword[0] & ~3u)) <= 4096)
                    lockFlags = D3DLOCK_READONLY;
            }
            else if (flags & LOCK_DISCARD)
            {
                lockFlags = D3DLOCK_NOOVERWRITE;
            }
        }
        else if ((flags & LOCK_WRITE) && (flags & LOCK_DISCARD))
        {
            lockFlags = D3DLOCK_NOOVERWRITE;
        }

        u8* const data = static_cast<u8*>(D3DVertexBuffer_Lock(this, 0, 0, lockFlags)) + offset;
        iterator.Initialize(data, size, this, flags);
        return true;
    }
    void Unlock(const BatchIterator&);
    u32 GetType() const { return m_type; }
    u32 GetBufferSize() const { return m_bufferSize; }
    void Xbox2CheckPhysicalMemoryFlags();
    void Xbox2UnSet();

protected:
    u32 m_bufferSize;
    u32 m_type;
};

CGS_STATIC_ASSERT((sizeof(D3DVertexBuffer) == 0x20), "ARTIST SDK vertex header");
CGS_STATIC_ASSERT((sizeof(VertexBuffer) == 0x28), "ARTIST vertex buffer");
CGS_STATIC_ASSERT((sizeof(VertexBuffer::Parameters) == 8), "ARTIST vertex parameters");
CGS_STATIC_ASSERT((sizeof(VertexBuffer::BatchIterator) == 16), "ARTIST vertex lock iterator");
}
