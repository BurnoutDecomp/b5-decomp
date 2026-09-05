#ifndef EFFECTS_VERTEX_BUFFER_H
#define EFFECTS_VERTEX_BUFFER_H

#include "types.hpp"

// =============================================================================
// GameSource/Effects/Particles/EffectsVertexBuffer.h
//
// The particle-system vertex-buffer write view. A raw memory span is Lock()'d,
// producing an EffectsVertexBufferLocked over which callers open Begin/EndBatch
// windows, streaming vertices through an EffectsVertexBufferIterator and
// accumulating an EffectsVertexBufferBatch (start-vertex / vertex-count) draw
// record. Global namespace.
//
// Names/offsets are DWARF-authoritative (references/DecFIGS/dwarfdump/GameSource/
// Effects/Particles/EffectsVertexBuffer.{h,cpp}); byte offsets are pinned to the
// X360 asm at 0x822798E0 / 0x82279950 / 0x82279A28.
//
// EffectsVertexBuffer LAYOUT (asm: lbz/lwz displacements 0/4/8/0xC on r31):
//   u8* mpLockedBufferBaseAddress;    // +0x00
//   u8* mpLockedBufferCurrentAddress; // +0x04
//   u8* mpLockedBufferTopAddress;     // +0x08
//   u8  mxFlags;                      // +0x0C
// =============================================================================

// -----------------------------------------------------------------------------
// EffectsVertexBufferBatch -- draw record produced by a Begin/EndBatch window.
// asm: stw at 0/4 of the batch pointer.
struct EffectsVertexBufferBatch
{
public:
    u32 GetStartVertex() const { return muStartVertex; }
    u32 GetVertexCount() const { return muVertexCount; }

    u32 muStartVertex;  // +0x00
    u32 muVertexCount;  // +0x04
};

// -----------------------------------------------------------------------------
// EffectsVertexBufferIterator -- the per-batch write cursor. DWARF gives it a
// renderengine::VertexIteratorBaseClass base plus (mpStartAddress, mpTopAddress,
// muStride). The base's current-write pointer sits at +0x00; the attested asm
// stores are: current @+0x00, mpStartAddress @+0x04, mpTopAddress @+0x08,
// muStride @+0x0C. The committed VertexIteratorBaseClass stand-in
// (BrnNativeParticleVertex.h) already absorbs all four members, so inheriting it
// here would double the members and break offsets. This iterator is therefore
// modelled as a flat 4-member struct to keep the offsets exact; FLAG: it does not
// share the committed base type.
struct EffectsVertexBufferIterator
{
public:
    u32 GetStride() const { return muStride; }

    const u8* GetBaseAddress() const    { return mpStartAddress; }
    const u8* GetTopAddress() const     { return mpTopAddress; }
    const u8* GetCurrentAddress() const { return mpCurrentAddress; }

    // Vertices still writable in this batch: (top - current) / stride. The X360 inlines
    // this into LionBlendVertex::VertexIterator::Write, where it is one of the two values
    // the asm asserts on before emitting a 36-byte vertex. Guarded against a zero stride
    // (an unconstructed iterator reads 0 here) so the assert reports empty rather than
    // dividing by zero.
    u32 GetVerticesFree() const
    {
        if (muStride == 0u || mpTopAddress <= mpCurrentAddress)
        {
            return 0u;
        }
        return static_cast<u32>(mpTopAddress - mpCurrentAddress) / muStride;
    }

    void SetBaseAddress(const u8* lpAddress)    { mpStartAddress = lpAddress; }
    void SetTopAddress(const u8* lpAddress)     { mpTopAddress = lpAddress; }
    void SetCurrentAddress(u8* lpAddress)       { mpCurrentAddress = lpAddress; }
    void SetStride(u32 luStride)                { muStride = luStride; }

private:
    const u8* mpCurrentAddress; // +0x00 (renderengine::VertexIteratorBaseClass cursor)
    const u8* mpStartAddress;   // +0x04
    const u8* mpTopAddress;     // +0x08
    u32       muStride;         // +0x0C
};

// -----------------------------------------------------------------------------
struct EffectsVertexBufferLocked; // forward: Lock() returns a reference to it.

struct EffectsVertexBuffer
{
public:
    void Construct(void* lpBuffer, u32 luBufferSize);
    void Destruct();
    EffectsVertexBufferLocked& Lock();
    void UnLock();

protected:
    // mxFlags bit values (DWARF EffectsVertexBuffer.h:181-182).
    static const u8 KX_FLAG_LOCKED   = 1;
    static const u8 KX_FLAG_IN_BATCH = 2;

    u8* mpLockedBufferBaseAddress;    // +0x00
    u8* mpLockedBufferCurrentAddress; // +0x04
    u8* mpLockedBufferTopAddress;     // +0x08
    u8  mxFlags;                      // +0x0C
};

// -----------------------------------------------------------------------------
// EffectsVertexBufferLocked -- the locked view; shares EffectsVertexBuffer's
// storage exactly (Lock() returns *this reinterpreted). Protected inheritance
// per DWARF (EffectsVertexBuffer.h:191).
struct EffectsVertexBufferLocked : protected EffectsVertexBuffer
{
public:
    EffectsVertexBufferLocked& BeginBatch(EffectsVertexBufferIterator& lOutVertexIterator,
                                          EffectsVertexBufferBatch&    lOutBatch,
                                          u32                          luVertexStride);
    EffectsVertexBufferLocked& EndBatch(EffectsVertexBufferIterator& lInOutVertexIterator,
                                        EffectsVertexBufferBatch&    lInOutBatch,
                                        u32                          luVertexStride);

    u32 GetBytesUsed() const;
    u32 GetBytesFree() const;
};

#endif // EFFECTS_VERTEX_BUFFER_H
