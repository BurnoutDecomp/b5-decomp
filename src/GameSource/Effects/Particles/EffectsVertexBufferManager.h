#ifndef EFFECTS_VERTEX_BUFFER_MANAGER_H
#define EFFECTS_VERTEX_BUFFER_MANAGER_H

#include "types.hpp"

// =============================================================================
// GameSource/Effects/Particles/EffectsVertexBufferManager.h
//
// EffectsVertexBufferManager -- the double-buffered vertex-buffer pool the
// particle system draws from. Global namespace (DecFIGS DWARF, global-ns unity
// dump: references/DecFIGS/dwarfdump/GameSource/Effects/Particles/
// EffectsVertexBufferManager.{h,cpp}).
//
// LAYOUT (DWARF-authoritative names/offsets; the muCurrentBuffer@+0x14 /
// mbLocked@+0x15 offsets match the X360 asm's lbz 0x14 / lbz 0x15 exactly --
// 2*4 (mapVertexBuffer) + 2*4 (mapVertexBufferAddress) + 4 (muVertexBufferSize)
// = 0x14):
//   renderengine::VertexBuffer* mapVertexBuffer[2];        // +0x00
//   void*                       mapVertexBufferAddress[2]; // +0x08
//   u32                         muVertexBufferSize;        // +0x10
//   u8                          muCurrentBuffer;           // +0x14
//   bool                        mbLocked;                  // +0x15 (byte-sized boolean)
//   EffectsVertexBuffer         mCurrentBuffer;            // +0x18 (pointer-aligned
//                                                          //  after the +0x15 byte,
//                                                          //  so 2 bytes of pad)
//
// The mCurrentBuffer@+0x18 offset is pinned by the X360 asm: Lock/UnLock do
// `addi r,this,0x18` then load/store the EffectsVertexBuffer members at +0/4/8/0xC
// off that (base/current/top/mxFlags) -- see EffectsVertexBuffer.h.
// =============================================================================

#include "GameSource/Effects/Particles/EffectsVertexBuffer.h"

namespace renderengine { class VertexBuffer; }
namespace rw { struct IResourceAllocator; }

class EffectsVertexBufferManager
{
public:
    // Construct @ 0x822807D0 -- allocate KU_NUM_BUFFERS vertex buffers from the
    // resource allocator, lock each once to capture its GPU write address, and
    // zero the double-buffer bookkeeping + the embedded EffectsVertexBuffer.
    void Construct(rw::IResourceAllocator* lpAllocator, u32 luVertexBufferSize,
                   bool lbPs3CreateInMainMemory);

    // Lock @ 0x82279B18 -- open a write window over the current slot's GPU
    // address range and hand back the locked view.
    EffectsVertexBufferLocked& Lock();

    // UnLock @ 0x82279BA8 -- close the write window on the current slot.
    void UnLock();

    // GetVertexBuffer @ 0x82278320 -- assert the manager is not locked for write,
    // then return the vertex buffer for the current double-buffer slot.
    renderengine::VertexBuffer* GetVertexBuffer() const;

    static const u32 KU_NUM_BUFFERS = 2;  // EffectsVertexBufferManager.h:78

private:
    renderengine::VertexBuffer* mapVertexBuffer[KU_NUM_BUFFERS];        // +0x00
    void*                       mapVertexBufferAddress[KU_NUM_BUFFERS]; // +0x08
    u32                         muVertexBufferSize;                     // +0x10
    u8                          muCurrentBuffer;                        // +0x14
    bool                        mbLocked;                               // +0x15
    EffectsVertexBuffer         mCurrentBuffer;                         // +0x18
};

#endif // EFFECTS_VERTEX_BUFFER_MANAGER_H
