#include "GameShared/GameClasses/System/FileSystem/CgsFileSystem.h"
#include "GameShared/GameClasses/System/FileSystem/CgsDeviceManager.h"
#include "GameShared/GameClasses/System/FileSystem/Devices/CgsDevicePhysicalPC.h"

// CgsFileSystem::FileSystem - see the header. The async device engine bring-up lives in
// EnsureDeviceManagerUp(); the rw filesys layer + BaseFile stream machinery are follow-on.
namespace CgsFileSystem
{
    // Marked PC sentinel allocator. The X360 passed the GameData allocator to
    // InitializeDeviceManager (which only stores it; DeviceManager::operator new mallocs on PC),
    // so any non-null value satisfies the "Allocator required" assert. Routing the manager through
    // the real GameData allocator is deferred.
    static char gcPcAllocatorSentinel;

    // The single PC physical device. A process-static (rather than a FileSystem member) so its
    // lifetime spans the whole run and the worker thread that references it never dangles, and so
    // a lazy bring-up from the bundle loader (before the FileSystem object prepares) has a device
    // to register.
    static DevicePhysicalPC gPhysicalDevice;

    void EnsureDeviceManagerUp()
    {
        if (!DeviceManager::GetIfInitialized())
            DeviceManager::InitializeDeviceManager(&gcPcAllocatorSentinel);

        DeviceManager* lpManager = GetDeviceManager();
        if (lpManager && !lpManager->FindDevice("p_dvd"))
            lpManager->AddPhysicalDevice(&gPhysicalDevice, "p_dvd", 0);
    }

    void FileSystem::Construct() {}

    bool FileSystem::Prepare()
    {
        EnsureDeviceManagerUp();
        return true;
    }

    bool FileSystem::Release()  { return true; }
    void FileSystem::Destruct() {}
}
