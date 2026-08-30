#pragma once

// CgsDevicePhysicalX360 — the concrete physical file device for the Xbox 360 build: a
// CgsFileSystem::Device backed by the XDK Win32 file API (CreateFileA / ReadFile /
// SetFilePointerEx / FindFirstFileA / CloseHandle) against the DVD/HDD. This is the faithful
// reconstruction of the X360 device the PC build's CgsDevicePhysicalPC re-expresses with host
// I/O; the two are siblings (same Device base, same worker-dispatched op shape).
//
// SOURCES (X360 ARTIST):
//   Init           @ 0x828DE998   Open           @ 0x828F9728   Close          @ 0x828F9D90
//   Read           @ 0x828DE9F0   Seek           @ 0x828DEB98
//   OpenDirectory  @ 0x828F9E40   ReadDirectory  @ 0x828DEC30   CloseDirectory @ 0x828FA8B0
//
// Unlike the PC sibling (which maps an int handle onto a Win32 HANDLE through a flat table), the
// X360 device owns an embedded IndexedPool<FileHandleRecord,N,s8> at this+16 that hands out
// stable byte handles; each live element IS a FileHandleRecord (the open Win32 HANDLE + the seek
// cursor + the resolved path). Open claims a pool slot and stores the record there; Close returns
// the slot. The op methods therefore receive a FileHandleRecord* (the resolved pool element),
// matching the asm (`lwz r3, 8(r30)` reads the HANDLE at record+8). The Device base's
// worker-dispatched virtuals take the opaque native-width DeviceHandle; here it is the resolved
// FileHandleRecord pointer itself, exactly matching the asm's direct record dereferences.
//
// LAYOUT NOTE (asm): the FileHandleRecord stride is 0x110 (272), proven by the pool's
// GetObjectIndex divide-by-272 (@0x828EAE38) and Open's element write (`*(rec+8)=HANDLE`,
// `*rec=position`, `*(rec+12)=mode`, name copied to rec+16). The device's pool counters are read
// at this+24/+25/+20 (pool@this+16, byte-indexed: +8 alloc / +9 free / +4 free-list), so the
// pool is the first member after the Device base + vptr.

#include <cstddef>   // offsetof (uncalled _AssertLayout)
#include "types.hpp"
#include "GameShared/GameClasses/System/FileSystem/Devices/CgsDevice.h"
#include "GameShared/GameClasses/Containers/CgsIndexedPool.h"   // CgsContainers::IndexedPool

namespace CgsFileSystem
{
    // The disposition modes Open decodes out of its flags arg (X360 Open switch on (liFlags & 0xC),
    // mapping to the CreateFileA dwCreationDisposition the asm loads: see CgsDevicePhysicalX360.cpp).
    // Recovered values only; named for the switch.
    static const u32 KU_X360_OPEN_FLAG_WRITE     = 0x2;  // (liFlags & 2) -> GENERIC_READ|GENERIC_WRITE
    static const u32 KU_X360_OPEN_DISPOSITION_MASK = 0xC; // (liFlags & 0xC) selects the disposition

    // A single open-file slot in the device's handle pool. On the X360 this is 272 bytes (stride
    // 0x110, 32-bit HANDLE): { u64 position @+0; HANDLE @+8; s32 flags @+0xC; char path[256] @+0x10 }.
    // The fields are kept in that order and accessed BY NAME; on the LLP64 gate host the HANDLE
    // widens to 8 bytes so the trailing offsets (and the total stride) shift — that is expected and
    // immaterial because the pool resolves indices through sizeof(T) self-consistently. Field
    // ORDER and the +0 position word are the pointer-invariant facts (see _AssertLayout).
    struct FileHandleRecord
    {
        u64   mu64Position;  // X360 +0x00  running byte offset (Read advances it; Seek sets it)
        void* mpHandle;      // X360 +0x08  Win32 HANDLE from CreateFileA (asm `8(rec)`)
        s32   miFlags;       // X360 +0x0C  the open flags arg (asm `*(rec+12)=a3`)
        char  macPath[256];  // X360 +0x10  resolved "%s:\\%s" backslash path (asm copies to rec+16)
    };

    // A single directory entry as written by OpenDirectory/ReadDirectory into the caller's buffer.
    // 260 bytes (X360 advances the write cursor by 260 and the type cursor by 65 ints == 260): the
    // name then a type tag (1 == file, 2 == directory; X360 computes ((attrib & 0x10)==0)+1).
    struct DirectoryEntry
    {
        char macName[256];   // +0x00  entry name (FindFirstFileA/FindNextFileA cFileName)
        s32  miType;         // +0x100 1 == file, 2 == directory ((dwFileAttributes & 0x10)==0)+1
    };

    class DevicePhysicalX360 : public Device
    {
    public:
        // Capacity of the embedded handle pool (runtime field msCapacity in the pool; the literal
        // extent is a placeholder, the bodies are capacity-driven through the pool).
        static const s32 KI_MAX_OPEN_FILES = 256;

        // ---- worker-dispatched ops (Device base vtable order) ----
        int Connect() override;                                                                  // Init @0x828DE998 (debug trace)
        int Open(const char* lpcPath, u32 luMode, Handle::DeviceHandle* lppOutHandle) override; // @0x828F9728
        int Close(Handle::DeviceHandle lpHandle) override;                                      // @0x828F9D90
        int Read(Handle::DeviceHandle lpHandle, void* lpBuffer, u32 luSize,
                 u32* lpuOutResult) override;                                                    // @0x828DE9F0
        int Seek(Handle::DeviceHandle lpHandle, u64 lu64Offset,
                 u64* lpu64OutPosition) override;                                                // @0x828DEB98

        // ---- directory enumeration (Device base directory virtuals) ----
        int OpenDirectory(const char* lpcPath, void* lpEntryBuffer, u32 luMaxEntries,
                          u32* lpuOutCount, Handle::DeviceHandle* lppOutHandle) override;          // @0x828F9E40
        int CloseDirectory(Handle::DeviceHandle lpHandle) override;                              // @0x828FA8B0
        int ReadDirectory(Handle::DeviceHandle lpHandle, void* lpEntryBuffer, u32 luMaxEntries,
                          u32* lpuOutCount) override;                                             // @0x828DEC30

    private:
        // Embedded handle pool (this+16; byte-indexed, element stride 0x110). The opaque handle
        // is a live FileHandleRecord*; Close/CloseDirectory recover its index when returning it.
        CgsContainers::IndexedPool<FileHandleRecord, KI_MAX_OPEN_FILES, s8> mHandlePool;  // X360 this+0x10
    };

    // Pointer-invariant layout facts only (the X360 32-bit-pointer offsets +8/+0xC/+0x10 and the
    // 0x110 stride are NOT pointer-invariant on the LLP64 gate host, so they are documented in the
    // struct comments, not asserted). The seek-position word sits at +0 on every target, and the
    // DirectoryEntry type tag follows the 256-byte name. Uncalled.
    inline void _AssertLayout()
    {
        static_assert(offsetof(FileHandleRecord, mu64Position) == 0x00, "position word at record+0");
        static_assert(offsetof(DirectoryEntry,   miType)       == 0x100, "dir-entry type tag at +256");
    }
}
