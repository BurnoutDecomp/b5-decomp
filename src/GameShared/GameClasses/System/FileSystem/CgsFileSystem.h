#pragma once

#include "types.hpp"

// CgsFileSystem::FileSystem - the resource file-I/O subsystem embedded by value inside
// CgsResource::ResourceModule. It owns the device manager (DeviceManager singleton) and, on the
// real X360, the rw::core::filesys layer, per-stream critical sections, a FileLog, and the
// async open/read/close stream machinery the BundleLoaderModule reads .BUNDLE files through.
//
// SOURCES (X360 ARTIST): ctor 0x827DF320, Construct 0x829033C0, Prepare 0x828F04E8,
// Release 0x82903880, Destruct 0x828E8A88.
//
// STATUS: the async device engine (DeviceManager + OperationPool + per-device worker thread +
// the Win32 DevicePhysicalPC leaf) is reconstructed and LIVE; the bundle loader reads through it.
// EnsureDeviceManagerUp() does the idempotent bring-up — FileSystem::Prepare calls it, and the
// bundle loader calls it lazily so EVERY load (incl. the early movie/debug-font bootstrapping,
// which runs before Prepare) goes through the engine (no CRT fallback). The rw::core::filesys
// Manager + the 16-slot BaseFile/OpenReadStream wrapper above DeviceManager remain follow-on.
namespace CgsFileSystem
{
    // Idempotent: initialise the DeviceManager + register the PC physical device (spawning its
    // worker thread) if not already up. Callable from FileSystem::Prepare or lazily from the
    // first file read. (X360: FileSystem::Prepare did this once, early in its boot; on PC the
    // resource module prepares late, so the loader also calls it to cover early bootstrapping.)
    void EnsureDeviceManagerUp();

    class FileSystem
    {
    public:
        void Construct();   // 0x829033C0
        bool Prepare();     // 0x828F04E8 — brings up the async device engine
        bool Release();     // 0x82903880
        void Destruct();    // 0x828E8A88
    };
}
