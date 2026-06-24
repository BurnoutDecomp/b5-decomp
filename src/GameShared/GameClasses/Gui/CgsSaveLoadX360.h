#pragma once

#include "types.hpp"

#include <Windows.h>   // HANDLE (the X360 stream wraps a Win32 file handle; host Win32 on PC)

namespace CgsMemory { class HeapMalloc; }

// CgsGui::XenonFileInputStream - a read-only file input stream over a Win32 file HANDLE,
// used by the Xenon (X360) save/load path (CgsGui::SaveLoadSystem::Prepare opens a stream
// and reads its header through this). Recovered from the X360 ARTIST binary:
//   GetSize @ 0x8284BD88   Read @ 0x8284BE28
// (assert sites cite the X360 build's gui/CgsSaveLoadX360.h).
//
// Layout (X360 authoritative, from the GetSize/Read member loads):
//   mhFile  [+0x0]  HANDLE  -- the open file handle; INVALID_HANDLE_VALUE (-1) until/if open
//                              fails (both methods assert mhFile != -1, "File open failed!").
//   miSize  [+0x4]  s32     -- the file size (GetSize returns `this[1]` == +4).
namespace CgsGui
{
    class XenonFileInputStream
    {
    public:
        // X360 0x8284BD88. Asserts the file is open (mhFile != INVALID_HANDLE_VALUE),
        // returns the cached file size.
        s32 GetSize() const;

        // X360 0x8284BE28. Asserts the file is open, then reads luBytesToRead bytes from the
        // handle into lpBuffer via Win32 ReadFile (lpOverlapped == NULL). On a ReadFile
        // failure (returns FALSE) it fires "File read failed!". Returns the number of bytes
        // actually read.
        s32 Read(void* lpBuffer, u32 luBytesToRead);

    private:
        HANDLE mhFile; // +0x0 -- open Win32 file handle (-1 == INVALID_HANDLE_VALUE)
        s32    miSize; // +0x4 -- cached file size in bytes
    };

    // CgsGui::MemcardAllocator - a polymorphic allocator front-end the X360 (Xenon) save/load
    // path hands to the memory-card I/O so its dynamic buffers come out of a dedicated
    // HeapMalloc rather than the global heap. Recovered from the X360 ARTIST binary (assert
    // sites cite gui/CgsSaveLoadX360.h):
    //   Alloc                      @ 0x827DBB08  -> mpHeap->Malloc(luSize, /*align*/4); asserts non-null
    //   Free                       @ 0x827DBBC8  -> mpHeap->Free(lpBlock) (tail call)
    //   `vector deleting destructor'@ 0x827DBC00  -> set vtable, then operator delete if (flag & 1)
    //
    // Layout (X360 authoritative, from the member loads):
    //   vptr   [+0x0]  -- leading vtable pointer (the object is polymorphic; Alloc/Free are
    //                     virtuals, and the vector-deleting-destructor writes off_8200F5B4 here).
    //   mpHeap [+0x4]  HeapMalloc* -- the heap every allocation is serviced from (`lwz r3,4(r3)`
    //                     loads it as the `this` for both HeapMalloc::Malloc and HeapMalloc::Free).
    // The KI_DEFAULT_ALIGNMENT(4) the X360 passes to Malloc matches CgsMemory::HeapMalloc's own
    // default alignment. Alloc/Free are declared virtual (X360 dispatch + the destructor's vtable
    // store); the host vtable slot reproduces the +0x0 vptr without a raw offset cast.
    class MemcardAllocator
    {
    public:
        static const s32 KI_DEFAULT_ALIGNMENT = 4;   // X360 `li r5,4` alignment argument

        virtual ~MemcardAllocator();

        // X360 0x827DBB08. Allocate luSize bytes from mpHeap (4-byte aligned). Asserts the
        // HeapMalloc::Malloc result is non-null ("CgsMemory::HeapMalloc::Malloc failed.").
        virtual void* Alloc(s32 luSize);
        // X360 0x827DBBC8. Free a block previously returned by Alloc (forwards to mpHeap->Free).
        virtual void  Free(void* lpBlock);

    private:
        CgsMemory::HeapMalloc* mpHeap;   // +0x4 -- the heap allocations are serviced from
    };
}
