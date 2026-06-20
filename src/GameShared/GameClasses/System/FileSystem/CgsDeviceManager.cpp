// CgsDeviceManager.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsFileSystem::GetDeviceManager @ 0x8264AD88
//
// This TU owns:
//   - The out-of-line definition of DeviceManager::mpDeviceManager (the singleton).
//   - The body of CgsFileSystem::GetDeviceManager().
//
// Future DeviceManager method bodies (InitializeDeviceManager, ReleaseDeviceManager,
// AddPhysicalDevice, Open, Read, Write, Close, Shutdown, ...) belong here too,
// once their transitive type dependencies (EAThread, OperationPool, Futex, rw::)
// are reconstructed and the deferred member block in CgsDeviceManager.h is uncommented.

#include "GameShared/GameClasses/System/FileSystem/CgsDeviceManager.h"

namespace CgsFileSystem
{
    // Out-of-line definition of the private static singleton.
    // This is the unique definition with external linkage — every other TU that
    // calls GetDeviceManager() reads through the friend declaration.
    // X360 global: dword_830EA400
    DeviceManager* DeviceManager::mpDeviceManager = nullptr;

    // CgsFileSystem::GetDeviceManager @ 0x8264AD88
    // Returns the DeviceManager singleton; asserts if not yet initialised.
    DeviceManager* GetDeviceManager()
    {
        CGS_ASSERT(DeviceManager::mpDeviceManager != nullptr, "Device manager not initialized\n");
        return DeviceManager::mpDeviceManager;
    }

} // namespace CgsFileSystem
