#include "GameShared/GameClasses/System/FileSystem/CgsFileSystem.h"
#include "GameShared/GameClasses/System/FileSystem/CgsDeviceManager.h"

// CgsFileSystem::FileSystem - see the header. Prepare brings up the async device engine; the rw
// filesys layer + BaseFile stream machinery are follow-on. Release/Destruct stay light for now.
namespace CgsFileSystem
{
    // Marked PC sentinel allocator. The X360 passed the GameData allocator to
    // InitializeDeviceManager (which only stores it; DeviceManager::operator new mallocs on PC),
    // so any non-null value satisfies the "Allocator required" assert. Routing the manager through
    // the real GameData allocator is deferred.
    static char gcPcAllocatorSentinel;

    void FileSystem::Construct() {}

    bool FileSystem::Prepare()
    {
        // Initialise the device manager once (spawns nothing yet), then register the PC physical
        // device under "p_dvd" — AddPhysicalDevice spawns that device's worker thread, making the
        // async file engine LIVE.
        if (!DeviceManager::GetIfInitialized())
            DeviceManager::InitializeDeviceManager(&gcPcAllocatorSentinel);

        DeviceManager* lpManager = GetDeviceManager();
        if (lpManager && !lpManager->FindDevice("p_dvd"))
            lpManager->AddPhysicalDevice(&mPhysicalDevice, "p_dvd", 0);

        return true;
    }

    bool FileSystem::Release()  { return true; }
    void FileSystem::Destruct() {}
}
