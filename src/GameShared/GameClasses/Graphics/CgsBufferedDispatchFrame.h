#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.h"   // DispatchFrame / DispatchList / DispatchBin + fwd rw::IResourceAllocator

// =============================================================================
// CgsBufferedDispatchFrame.h  (GameShared/GameClasses/Graphics)
//
// CgsGraphics::BufferedDispatchFrame — an N-deep ring of CgsGraphics::DispatchFrame
// objects with independent write/read cursors. Construct allocates the pointer
// array plus one DispatchFrame per slot; the original heap request has alignment16
// even though the type's embedded-bin alignment is128. The accessors hand
// out the frame (and its embedded bin / lists) under the cursor.
//
// Layout is reconstructed from the BURNOUT_X360_ARTIST.XEX disassembly
// (authoritative for member offsets) and cross-checked against the DecFIGS PS3
// DWARF dump (member names / virtual shape). X360-verified member offsets
// (word offsets from `this`, 4-byte ABI):
//   0x00  _vptr                                          (vtable)
//   0x04  mapDispatchFrame        DispatchFrame**        (a1[1])
//   0x08  muNumDispatchFrames     uint32_t               (a1[2])
//   0x0C  muCurrentFrameForWrite  uint32_t               (a1[3])
//   0x10  muCurrentFrameForRead   uint32_t               (a1[4])
//
// Each per-slot DispatchFrame is heap-allocated through the RenderWare aligned
// allocator (RwMallocMemAligned, 384 bytes / 16-byte alignment) and freed through
// RwFreeMemAligned; the DispatchFrame* array itself comes from operator new[] /
// operator delete[].
// =============================================================================

namespace CgsGraphics
{

class BufferedDispatchFrame
{
public:
    // ARTIST827E7968: the actual default ring starts with two frames, write0/read1.
    BufferedDispatchFrame();

    // Native vtable820D5174 corroborates the DWARF's nine slots. Prepare and
    // Release share the original return-true ICF body at82C296C8.

    // @ 0x827F97F8 — allocate the frame array + per-slot frames. muNumDispatchFrames
    // must already be set (via SetNumDispatchFrames) before this is called.
    virtual void Construct(u32 luFramesPerList, u32 luBinSizeQwords, rw::IResourceAllocator* lpAllocator);

    virtual bool Prepare();
    virtual bool Release();

    // @ 0x827ECE90 — Release + free every per-slot frame, free the array, reset.
    virtual void Destruct();

    // @ 0x827E7768 — advance both cursors by one, wrapping at muNumDispatchFrames.
    virtual void Swap();

    // @ 0x827E77A8 — list `luListId` of the frame under the write cursor.
    virtual DispatchList& GetDispatchListForWrite(u32 luListId);

    // @ 0x827E7828 — embedded bin of the frame under the write cursor.
    virtual DispatchBin& GetDispatchBinForWrite();

    // @ 0x827E7898 — the frame under the write cursor.
    virtual DispatchFrame& GetDispatchFrameForWrite();

    // @ 0x827E7900 — the frame under the read cursor.
    virtual DispatchFrame& GetDispatchFrameForRead();

    // ---- Non-virtual inline accessors (DWARF .h:99 / .h:117) ----------------
    const u32& GetNumDispatchFrames() const { return muNumDispatchFrames; }
    void       SetNumDispatchFrames(u32 luNumDispatchFrames) { muNumDispatchFrames = luNumDispatchFrames; }

private:
    DispatchFrame** mapDispatchFrame;        // 0x04
    u32             muNumDispatchFrames;      // 0x08
    u32             muCurrentFrameForWrite;   // 0x0C
    u32             muCurrentFrameForRead;    // 0x10
};

} // namespace CgsGraphics
