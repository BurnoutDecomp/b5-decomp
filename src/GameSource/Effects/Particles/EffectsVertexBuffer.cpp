#include "GameSource/Effects/Particles/EffectsVertexBuffer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// EffectsVertexBuffer::Lock  X360 0x822798E0
// Asserts the buffer is not already locked, resets the write cursor to the base
// of the locked region, sets KX_FLAG_LOCKED, and returns *this reinterpreted as
// the locked-buffer view (the X360 return is simply r3=this).
//   asm: lbz mxFlags; test KX_FLAG_LOCKED -> assert not set;
//        mpLockedBufferCurrentAddress = mpLockedBufferBaseAddress;
//        mxFlags |= KX_FLAG_LOCKED; return this.
EffectsVertexBufferLocked& EffectsVertexBuffer::Lock()
{
    CGS_ASSERT((mxFlags & KX_FLAG_LOCKED) == 0,
               "( mxFlags & KX_FLAG_LOCKED ) == 0");

    mpLockedBufferCurrentAddress = mpLockedBufferBaseAddress;
    mxFlags |= KX_FLAG_LOCKED;

    // The X360 return is simply r3 = this: EffectsVertexBufferLocked shares this object's
    // storage exactly. A static_cast base->derived is inaccessible here because the
    // inheritance is protected, so reinterpret the same storage (identical layout, identical
    // pointer value -- the raw `return this` the asm performs).
    return reinterpret_cast<EffectsVertexBufferLocked&>(*this);
}

// EffectsVertexBufferLocked::BeginBatch  X360 0x82279950
// Opens a write batch: computes the aligned start vertex from the current write
// offset, seeds the vertex iterator (start/current/top/stride) over the remaining
// locked region, initialises the batch record, and flags KX_FLAG_IN_BATCH.
EffectsVertexBufferLocked& EffectsVertexBufferLocked::BeginBatch(
        EffectsVertexBufferIterator& lOutVertexIterator,
        EffectsVertexBufferBatch&    lOutBatch,
        u32                          luVertexStride)
{
    CGS_ASSERT((mxFlags & (KX_FLAG_LOCKED | KX_FLAG_IN_BATCH)) == KX_FLAG_LOCKED,
               "( mxFlags & ( KX_FLAG_LOCKED | KX_FLAG_IN_BATCH ) ) == KX_FLAG_LOCKED");

    // Bytes already consumed within the locked region (current - base).
    const u32 luCurrentOffset =
        static_cast<u32>(mpLockedBufferCurrentAddress - mpLockedBufferBaseAddress);
    CGS_ASSERT((luCurrentOffset & 15) == 0, "( luCurrentOffset & 15 ) == 0");

    // Round the current offset up to a whole number of vertices (ceil divide).
    // twllei on X360 traps if luVertexStride == 0.
    const u32 luVertexOffset  = (luCurrentOffset + luVertexStride - 1) / luVertexStride;
    u8* const lpAlignedAddress = mpLockedBufferBaseAddress + luVertexOffset * luVertexStride;

    lOutVertexIterator.SetBaseAddress(lpAlignedAddress);
    lOutVertexIterator.SetCurrentAddress(lpAlignedAddress - 4);
    lOutVertexIterator.SetStride(luVertexStride);
    lOutVertexIterator.SetTopAddress(mpLockedBufferTopAddress);

    lOutBatch.muStartVertex = luVertexOffset;
    lOutBatch.muVertexCount = 0;

    mxFlags |= KX_FLAG_IN_BATCH;

    return *this;
}

// EffectsVertexBufferLocked::EndBatch  X360 0x82279A28
// Closes a write batch: clears KX_FLAG_IN_BATCH, measures the bytes the iterator
// advanced (current - start + stride-slack of 4), asserts a whole number of
// vertices was written, folds that count into the batch record, resets the
// iterator cursor, and advances the buffer's write pointer.
EffectsVertexBufferLocked& EffectsVertexBufferLocked::EndBatch(
        EffectsVertexBufferIterator& lInOutVertexIterator,
        EffectsVertexBufferBatch&    lInOutBatch,
        u32                          luVertexStride)
{
    CGS_ASSERT((mxFlags & (KX_FLAG_LOCKED | KX_FLAG_IN_BATCH)) == (KX_FLAG_LOCKED | KX_FLAG_IN_BATCH),
               "( mxFlags & ( KX_FLAG_LOCKED | KX_FLAG_IN_BATCH ) ) == ( KX_FLAG_LOCKED | KX_FLAG_IN_BATCH )");
    CGS_ASSERT(luVertexStride == lInOutVertexIterator.GetStride(),
               "luVertexStride == lInOutVertexIterator.GetStride()");

    // twllei on X360 traps if luVertexStride == 0.
    mxFlags &= static_cast<u8>(~KX_FLAG_IN_BATCH);

    const u8* const lpIteratorCurrentAddress = lInOutVertexIterator.GetCurrentAddress();
    const u8* const lpIteratorBaseAddress    = lInOutVertexIterator.GetBaseAddress();
    const u32 luBytesWritten =
        static_cast<u32>(lpIteratorCurrentAddress - lpIteratorBaseAddress) + 4;

    const u32 luVerticesWritten = luBytesWritten / luVertexStride;
    CGS_ASSERT((luVerticesWritten * luVertexStride) == luBytesWritten,
               "( luVerticesWritten * luVertexStride ) == luBytesWritten");

    lInOutBatch.muVertexCount += luVerticesWritten;
    lInOutVertexIterator.SetCurrentAddress(reinterpret_cast<u8*>(-4));
    mpLockedBufferCurrentAddress += luBytesWritten;

    return *this;
}

// ---------------------------------------------------------------------------------------------
// EffectsVertexBuffer::Construct(void*, u32) / ::UnLock
//
// No X360 export -- both are inlined into EffectsVertexBufferManager (Construct @0x822807D0
// captures each buffer's locked GPU write address and hands it here; Lock @0x82279B18 /
// UnLock @0x82279BA8 open and close the write window). Construct seats the write cursor at
// the base of the mapped range and records its extent; UnLock closes the window by clearing
// the cursor, which is what makes the manager's `!mbLocked` assert (EffectsVertexBufferManager.h)
// meaningful. Both are spelled against this type's own members -- see its header for the
// per-member attestation.
// ---------------------------------------------------------------------------------------------
void EffectsVertexBuffer::Construct(void* lpBuffer, u32 luBufferSize)
{
    u8* const lpBase = static_cast<u8*>(lpBuffer);
    mpLockedBufferBaseAddress    = lpBase;
    mpLockedBufferCurrentAddress = lpBase;
    mpLockedBufferTopAddress     = lpBase + luBufferSize;
    mxFlags                      = 0;   // neither locked nor in a batch
}

void EffectsVertexBuffer::Destruct()
{
    // Nothing owned: the locked span is the manager's vertex-buffer memory, borrowed.
}

void EffectsVertexBuffer::UnLock()
{
    CGS_ASSERT((mxFlags & (KX_FLAG_LOCKED | KX_FLAG_IN_BATCH)) == KX_FLAG_LOCKED,
               "( mxFlags & ( KX_FLAG_LOCKED | KX_FLAG_IN_BATCH ) ) == KX_FLAG_LOCKED");
    mxFlags = static_cast<u8>(mxFlags & ~KX_FLAG_LOCKED);
}
