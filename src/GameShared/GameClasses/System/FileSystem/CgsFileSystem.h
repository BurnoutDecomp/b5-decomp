#pragma once

#include "types.hpp"

// CgsFileSystem::FileSystem - the resource file-I/O subsystem embedded by value inside
// CgsResource::ResourceModule. It owns the rw::core::filesys device manager, a bank of
// per-stream Win32 critical sections, a FileLog, and the async open/read/close stream
// machinery the BundleLoaderModule reads .BUNDLE files through.
//
// SOURCES (X360 ARTIST): ctor 0x827DF320, Construct 0x829033C0, Prepare 0x828F04E8,
// Release 0x82903880, Destruct 0x828E8A88, + the OpenReadStream/Read/Close internals.
//
// DEFER STATUS: unlike the other resource sub-modules this is NOT a ModuleSingleBuffered,
// and its real Prepare/Release ARE the rw::core::filesys::Manager + DeviceManager + Win32
// critical-section bring-up (platform + rw-filesys gated). This pass lands only the TYPE so
// ResourceModule can embed it and chain its lifecycle; the bodies are inert marked stubs
// (Prepare/Release report success so the ResourceModule stage machine advances) until the
// rw-filesys layer is reconstructed. The real layout (16+8 critical sections, FileLog, the
// device table and stream slots) is added then; this is a placeholder.
namespace CgsFileSystem
{
    class FileSystem
    {
    public:
        void Construct();   // 0x829033C0 (deferred - rw-filesys/Win32)
        bool Prepare();     // 0x828F04E8 (deferred - rw::core::filesys::Manager + devices)
        bool Release();     // 0x82903880 (deferred)
        void Destruct();    // 0x828E8A88 (deferred)

    private:
        u32 muPlaceholder;  // real layout added with the rw-filesys bring-up
    };
}
