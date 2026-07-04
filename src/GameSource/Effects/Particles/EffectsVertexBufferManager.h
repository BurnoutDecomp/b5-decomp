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
//   EffectsVertexBuffer         mCurrentBuffer;            // +0x16 (opaque here)
// =============================================================================

namespace renderengine { class VertexBuffer; }

class EffectsVertexBufferManager
{
public:
    // GetVertexBuffer @ 0x82278320 -- assert the manager is not locked for write,
    // then return the vertex buffer for the current double-buffer slot.
    renderengine::VertexBuffer* GetVertexBuffer() const;

private:
    renderengine::VertexBuffer* mapVertexBuffer[2];        // +0x00
    void*                       mapVertexBufferAddress[2]; // +0x08
    u32                         muVertexBufferSize;        // +0x10
    u8                          muCurrentBuffer;           // +0x14
    bool                        mbLocked;                  // +0x15  (byte-sized boolean)
    // EffectsVertexBuffer mCurrentBuffer; // +0x16 -- opaque, not modelled (no
    // attested member of this instantiation touches it). Grow with the real type
    // when its home lands.
};

#endif // EFFECTS_VERTEX_BUFFER_MANAGER_H
