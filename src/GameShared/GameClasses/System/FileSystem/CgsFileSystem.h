#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/System/FileSystem/Devices/CgsDevicePhysicalPC.h"  // DevicePhysicalPC

// CgsFileSystem::FileSystem - the resource file-I/O subsystem embedded by value inside
// CgsResource::ResourceModule. It owns the device manager (DeviceManager singleton) and, on the
// real X360, the rw::core::filesys layer, per-stream critical sections, a FileLog, and the
// async open/read/close stream machinery the BundleLoaderModule reads .BUNDLE files through.
//
// SOURCES (X360 ARTIST): ctor 0x827DF320, Construct 0x829033C0, Prepare 0x828F04E8,
// Release 0x82903880, Destruct 0x828E8A88.
//
// B3 STATUS: Prepare now brings the async device engine LIVE — it initialises the DeviceManager
// and registers a concrete PC physical device (DevicePhysicalPC, the Win32 IO leaf), which spawns
// that device's worker thread. (X360 split this: Prepare did InitializeDeviceManager + the rw
// filesys Manager; the physical devices p_dvd/p_hdd were added in Construct. Consolidated here for
// the PC bring-up; the rw::core::filesys layer + the 16-slot BaseFile stream machinery remain
// follow-on work.) The bundle loader is routed through this engine in B4.
namespace CgsFileSystem
{
    class FileSystem
    {
    public:
        void Construct();   // 0x829033C0
        bool Prepare();     // 0x828F04E8 — brings up the DeviceManager + registers the PC device
        bool Release();     // 0x82903880
        void Destruct();    // 0x828E8A88

    private:
        // The concrete PC physical device (Win32 leaf), registered with the DeviceManager in
        // Prepare. Owned by value so its lifetime matches the FileSystem (and the worker thread
        // that references it).
        DevicePhysicalPC mPhysicalDevice;
    };
}
