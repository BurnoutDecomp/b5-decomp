#pragma once

#include "types.hpp"
#include <xtl.h>

namespace rw { struct Resource; }

namespace renderengine
{
// ARTIST 82B62100 initializes the SDK header and appends the index count.
class IndexBuffer : public D3DIndexBuffer
{
public:
    // Names from DecFIGS indexbuffer.h; ARTIST 82B60730 writes these words.
    struct Parameters
    {
        u32 type;
        u32 depth;
        u32 numIndices;
    };

    // DecFIGS names; ARTIST 82B60760 writes both words on a successful lock.
    struct Locked { void* indices; u32 indexStride; };
    enum { LOCK_READ = 1, LOCK_WRITE = 2, LOCK_NOOVERWRITE = 8 };
    static IndexBuffer* Initialize(const rw::Resource&, const Parameters&);
    bool Lock(u32 flags, Locked& locked);
    void Unlock(const Locked& locked);
    void GetParameters(Parameters&) const;
    void Xbox2CheckPhysicalMemoryFlags();

protected:
    u32 m_numIndices;
};

CGS_STATIC_ASSERT((sizeof(D3DIndexBuffer) == 0x20), "ARTIST SDK index header");
CGS_STATIC_ASSERT((offsetof(D3DIndexBuffer, Address) == 0x18), "ARTIST index GPU address");
CGS_STATIC_ASSERT((sizeof(IndexBuffer) == 0x24), "ARTIST index buffer");
CGS_STATIC_ASSERT((sizeof(IndexBuffer::Parameters) == 0x0C), "ARTIST index parameters");
}
