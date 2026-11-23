#include "GameSource/Effects/Particles/EffectsVertexBuffer.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT

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
        CgsDev::Assert::MessageAt(
            "( mxFlags & KX_FLAG_LOCKED ) == 0",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\"
            "../Effects/Particles/EffectsVertexBuffer.cpp", 98 /* ARTIST 0x8227990C */));

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
void EffectsVertexBufferLocked::BeginBatch(
        EffectsVertexBufferIterator& lOutVertexIterator,
        EffectsVertexBufferBatch&    lOutBatch,
        u32                          luVertexStride)
{
    CGS_ASSERT((mxFlags & (KX_FLAG_LOCKED | KX_FLAG_IN_BATCH)) == KX_FLAG_LOCKED,
        CgsDev::Assert::MessageAt(
            "( mxFlags & ( KX_FLAG_LOCKED | KX_FLAG_IN_BATCH ) ) == KX_FLAG_LOCKED",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\"
            "../Effects/Particles/EffectsVertexBuffer.cpp", 149));

    // Bytes already consumed within the locked region (current - base).
    const u32 luCurrentOffset =
        static_cast<u32>(mpLockedBufferCurrentAddress - mpLockedBufferBaseAddress);
    CGS_ASSERT((luCurrentOffset & 15) == 0, CgsDev::Assert::MessageAt(
            "( luCurrentOffset & 15 ) == 0",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\"
            "../Effects/Particles/EffectsVertexBuffer.cpp", 153));

    // Round the current offset up to a whole number of vertices (ceil divide).
    // twllei on X360 traps if luVertexStride == 0.
    const u32 luVertexOffset  = (luCurrentOffset + luVertexStride - 1) / luVertexStride;
    u8* const lpAlignedAddress = mpLockedBufferBaseAddress + luVertexOffset * luVertexStride;

    lOutVertexIterator.SetBaseAddress(lpAlignedAddress);
    lOutVertexIterator.SetCurrentAddress(lpAlignedAddress - 4);
    // Top before stride: ARTIST reads mpLockedBufferTopAddress (0x82279A00) before it stores the
    // stride (0x82279A04), which the store could alias were the stride set first.
    lOutVertexIterator.SetTopAddress(mpLockedBufferTopAddress);
    lOutVertexIterator.SetStride(luVertexStride);

    lOutBatch.muStartVertex = luVertexOffset;
    lOutBatch.muVertexCount = 0;

    mxFlags |= KX_FLAG_IN_BATCH;
}

// EffectsVertexBufferLocked::EndBatch  X360 0x82279A28
// Closes a write batch: clears KX_FLAG_IN_BATCH, measures the bytes the iterator
// advanced (current - start + stride-slack of 4), asserts a whole number of
// vertices was written, folds that count into the batch record, resets the
// iterator cursor, and advances the buffer's write pointer.
void EffectsVertexBufferLocked::EndBatch(
        EffectsVertexBufferIterator& lInOutVertexIterator,
        EffectsVertexBufferBatch&    lInOutBatch,
        u32                          luVertexStride)
{
    CGS_ASSERT((mxFlags & (KX_FLAG_LOCKED | KX_FLAG_IN_BATCH)) == (KX_FLAG_LOCKED | KX_FLAG_IN_BATCH),
        CgsDev::Assert::MessageAt(
            "( mxFlags & ( KX_FLAG_LOCKED | KX_FLAG_IN_BATCH ) ) == ( KX_FLAG_LOCKED | KX_FLAG_IN_BATCH )",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\"
            "../Effects/Particles/EffectsVertexBuffer.cpp", 205));
    CGS_ASSERT(luVertexStride == lInOutVertexIterator.GetStride(),
        CgsDev::Assert::MessageAt(
            "luVertexStride == lInOutVertexIterator.GetStride()",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\"
            "../Effects/Particles/EffectsVertexBuffer.cpp", 206));

    // twllei on X360 traps if luVertexStride == 0.
    mxFlags &= static_cast<u8>(~KX_FLAG_IN_BATCH);

    const u8* const lpIteratorCurrentAddress = lInOutVertexIterator.GetCurrentAddress();
    const u8* const lpIteratorBaseAddress    = lInOutVertexIterator.GetBaseAddress();
    const u32 luBytesWritten =
        static_cast<u32>(lpIteratorCurrentAddress - lpIteratorBaseAddress) + 4;

    const u32 luVerticesWritten = luBytesWritten / luVertexStride;
    CGS_ASSERT((luVerticesWritten * luVertexStride) == luBytesWritten,
        CgsDev::Assert::MessageAt(
            "( luVerticesWritten * luVertexStride ) == luBytesWritten",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\"
            "../Effects/Particles/EffectsVertexBuffer.cpp", 215));

    lInOutBatch.muVertexCount += luVerticesWritten;
    lInOutVertexIterator.SetCurrentAddress(reinterpret_cast<u8*>(-4));
    mpLockedBufferCurrentAddress += luBytesWritten;
}

// ---------------------------------------------------------------------------------------------
// EffectsVertexBuffer::Construct(void*, u32) / ::Destruct / ::UnLock
//
// No X360 export: all three exist only inlined into EffectsVertexBufferManager (same unity TU).
// Their bodies are read off those copies:
//   Construct -- Manager::Lock @0x82279B58..0x82279B8C: current = NULL, flags = 0,
//                base = address, top = address + size (the cursor is seated by Lock()).
//   UnLock    -- Manager::UnLock @0x82279BE4..0x82279C2C: assert the LOCKED bit
//                (EffectsVertexBuffer.cpp:122), clear it, current = NULL.
//   Destruct  -- Manager::UnLock @0x82279C30..0x82279C60: assert mxFlags == 0
//                (EffectsVertexBuffer.cpp:78), then base = top = current = NULL.
// ---------------------------------------------------------------------------------------------
void EffectsVertexBuffer::Construct(void* lpBuffer, u32 luBufferSize)
{
    u8* const lpBase = static_cast<u8*>(lpBuffer);
    mpLockedBufferCurrentAddress = 0;
    mxFlags                      = 0;   // neither locked nor in a batch
    mpLockedBufferBaseAddress    = lpBase;
    mpLockedBufferTopAddress     = lpBase + luBufferSize;
}

void EffectsVertexBuffer::Destruct()
{
    CGS_ASSERT(mxFlags == 0,
        CgsDev::Assert::MessageAt(
            "mxFlags == 0",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\"
            "../Effects/Particles/EffectsVertexBuffer.cpp", 78));
    mpLockedBufferBaseAddress    = 0;
    mpLockedBufferTopAddress     = 0;
    mpLockedBufferCurrentAddress = 0;
}

void EffectsVertexBuffer::UnLock()
{
    CGS_ASSERT((mxFlags & KX_FLAG_LOCKED) != 0,
        CgsDev::Assert::MessageAt(
            "( mxFlags & KX_FLAG_LOCKED ) != 0",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\"
            "../Effects/Particles/EffectsVertexBuffer.cpp", 122));
    mxFlags = static_cast<u8>(mxFlags & ~KX_FLAG_LOCKED);
    mpLockedBufferCurrentAddress = 0;
}
