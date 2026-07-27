#include "GameShared/GameClasses/Graphics/CgsBufferedDispatchFrame.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (BeginAssert/FireAssert/EndAssert)

#include <new>      // ::operator new[] / ::operator delete[] (the frame-pointer array backing)
#include <malloc.h> // _aligned_malloc / _aligned_free (the PC RW aligned-memory thunks)

// =============================================================================
// CgsBufferedDispatchFrame.cpp
//
// Bodies for CgsGraphics::BufferedDispatchFrame, the N-deep ring of
// CgsGraphics::DispatchFrame objects with independent write/read cursors.
// Store-for-store from the BURNOUT_X360_ARTIST.XEX disassembly:
//
//   CgsGraphics::BufferedDispatchFrame::Construct                @ 0x827F97F8
//   CgsGraphics::BufferedDispatchFrame::Destruct                 @ 0x827ECE90
//   CgsGraphics::BufferedDispatchFrame::Swap                     @ 0x827E7768
//   CgsGraphics::BufferedDispatchFrame::GetDispatchListForWrite  @ 0x827E77A8
//   CgsGraphics::BufferedDispatchFrame::GetDispatchBinForWrite   @ 0x827E7828
//   CgsGraphics::BufferedDispatchFrame::GetDispatchFrameForWrite @ 0x827E7898
//   CgsGraphics::BufferedDispatchFrame::GetDispatchFrameForRead  @ 0x827E7900
//
// The X360 maps `sub_82C08C00` to operator new[] (it is the same `new T[n]`
// helper the shader-constant / texture-scope TUs use). The per-slot
// DispatchFrame objects come from the RenderWare aligned allocator
// (RwMallocMemAligned, indirect off_82F91A04) and are freed through
// RwFreeMemAligned (indirect off_82F91A0C); both are global engine memory
// thunks whose bodies are homed by the RenderWare-memory follow-on.
// =============================================================================

namespace
{
// The two global RenderWare aligned-memory thunks the X360 calls indirectly
// through off_82F91A04 / off_82F91A0C.
// X360: RwMallocMemAligned(0x180, 0x10) per per-slot DispatchFrame; the result
// is the storage Construct places the frame into.
//
// FLAG PC-platform leaf: the RenderWare aligned-memory function table
// (off_82F91A04 / off_82F91A0C) is an engine-global indirection whose PC entries
// are the CRT aligned allocator -- the X360 slots point at the console's
// RwMallocMemAligned/RwFreeMemAligned, which are themselves thin wrappers over
// the platform aligned heap. Routed straight to _aligned_malloc/_aligned_free
// here until the RenderWare memory layer's function table is homed.
void* RwMallocMemAligned(u32 luSize, u32 luAlignment)
{
    return ::_aligned_malloc(static_cast<size_t>(luSize), static_cast<size_t>(luAlignment));
}

void RwFreeMemAligned(void* lpMemory)
{
    ::_aligned_free(lpMemory);
}
}

namespace CgsGraphics
{

// @ 0x827F97F8
// Allocate the per-slot frame-pointer array (one entry per muNumDispatchFrames),
// then allocate + Construct one DispatchFrame per slot through the RenderWare
// aligned allocator. Cursors start at write=0; with more than one frame the read
// cursor leads by one (read=1), otherwise it stays at its constructed value.
// muNumDispatchFrames must already be set (SetNumDispatchFrames) before this runs.
void BufferedDispatchFrame::Construct(u32 luFramesPerList, u32 luBinSizeQwords, rw::IResourceAllocator* lpAllocator)
{
    mapDispatchFrame = new DispatchFrame*[muNumDispatchFrames];

    for (u32 luFrame = 0; luFrame < muNumDispatchFrames; ++luFrame)
    {
        mapDispatchFrame[luFrame] =
            static_cast<DispatchFrame*>(RwMallocMemAligned(sizeof(DispatchFrame), 16u));
        mapDispatchFrame[luFrame]->Construct(luFramesPerList, luBinSizeQwords, lpAllocator);
    }

    muCurrentFrameForWrite = 0;
    if (muNumDispatchFrames > 1)
    {
        muCurrentFrameForRead = 1;
    }
}

// CgsBufferedDispatchFrame.cpp:77 / :95 (DecFIGS DWARF) -- the class' own
// Prepare/Release lifecycle slots. NOT RECONSTRUCTED: neither carries an entry
// in the X360 ARTIST export set (the ledger lists Construct/Destruct/Swap and
// the four cursor accessors only; the two lifecycle slots are either folded by
// ICF onto another trivial body or never emitted). They are declared to keep the
// vtable order the DWARF attests, and trap rather than fake a return value --
// nothing on the PC render path calls them (BrnRendererModule drives the frame
// through Construct/Swap/GetDispatchFrameFor* and tears down via Destruct).
bool BufferedDispatchFrame::Prepare()
{
    CGS_ASSERT(false,
               "BufferedDispatchFrame::Prepare: no X360 body in the ARTIST export set "
               "(vtable-order slot; unreferenced on the PC render path)");
    return false;
}

bool BufferedDispatchFrame::Release()
{
    CGS_ASSERT(false,
               "BufferedDispatchFrame::Release: no X360 body in the ARTIST export set "
               "(vtable-order slot; unreferenced on the PC render path)");
    return false;
}

// @ 0x827ECE90
// Release + free every per-slot DispatchFrame (aligned free), free the
// frame-pointer array, then reset all members to the empty state.
void BufferedDispatchFrame::Destruct()
{
    for (u32 luFrame = 0; luFrame < muNumDispatchFrames; ++luFrame)
    {
        mapDispatchFrame[luFrame]->Release();
        RwFreeMemAligned(mapDispatchFrame[luFrame]);
    }

    delete[] mapDispatchFrame;

    mapDispatchFrame       = NULL;
    muNumDispatchFrames    = 0;
    muCurrentFrameForRead  = 0;
    muCurrentFrameForWrite = 0;
}

// @ 0x827E7768
// Advance both cursors by one, wrapping each back to 0 when it reaches
// muNumDispatchFrames (the ring length).
void BufferedDispatchFrame::Swap()
{
    ++muCurrentFrameForWrite;
    ++muCurrentFrameForRead;

    if (muCurrentFrameForWrite >= muNumDispatchFrames)
    {
        muCurrentFrameForWrite = 0;
    }
    if (muCurrentFrameForRead >= muNumDispatchFrames)
    {
        muCurrentFrameForRead = 0;
    }
}

// @ 0x827E77A8
// List `luListId` of the frame under the write cursor.
DispatchList& BufferedDispatchFrame::GetDispatchListForWrite(u32 luListId)
{
    CGS_ASSERT(muNumDispatchFrames > muCurrentFrameForWrite,
               "muNumDispatchFrames > muCurrentFrameForWrite");

    return *mapDispatchFrame[muCurrentFrameForWrite]->GetList(luListId);
}

// @ 0x827E7828
// Embedded dispatch bin of the frame under the write cursor (X360: frame + 0x80).
DispatchBin& BufferedDispatchFrame::GetDispatchBinForWrite()
{
    CGS_ASSERT(muNumDispatchFrames > muCurrentFrameForWrite,
               "muNumDispatchFrames > muCurrentFrameForWrite");

    return mapDispatchFrame[muCurrentFrameForWrite]->GetBin();
}

// @ 0x827E7898
// The frame under the write cursor.
DispatchFrame& BufferedDispatchFrame::GetDispatchFrameForWrite()
{
    CGS_ASSERT(muNumDispatchFrames > muCurrentFrameForWrite,
               "muNumDispatchFrames > muCurrentFrameForWrite");

    return *mapDispatchFrame[muCurrentFrameForWrite];
}

// @ 0x827E7900
// The frame under the read cursor.
DispatchFrame& BufferedDispatchFrame::GetDispatchFrameForRead()
{
    CGS_ASSERT(muNumDispatchFrames > muCurrentFrameForRead,
               "muNumDispatchFrames > muCurrentFrameForRead");

    return *mapDispatchFrame[muCurrentFrameForRead];
}

} // namespace CgsGraphics
