#pragma once

// CgsDeviceManager.h — canonical home for CgsFileSystem::DeviceManager
// Reconstructed from BURNOUT_X360_ARTIST.XEX + DecFIGS DWARF
// (CgsDeviceManager.h, internal PS3 build, used for member names/types only)
//
// MINIMAL SLICE: this TU only exercises DeviceManager::mpDeviceManager (the
// singleton) and the four KI_ constants. The heavy instance members
// (mInitializationThread, maPhysicalDevices[8], maVirtualDevices[32],
// mDeviceListFutex, macDefaultPath[257]) embed OperationPool, Thread, Futex, and
// EAThreadDynamicData::ThreadId BY VALUE. Adding them here would pull in a large
// EAThread/OperationPool header cascade; those types are not yet reconstructed.
// The future DeviceManager work (extending this header + CgsDeviceManager.cpp) MUST:
//   1. #include the headers for OperationPool, Thread, Futex, EAThreadDynamicData::ThreadId.
//   2. Uncomment and flesh out the DEFERRED MEMBERS block below IN THIS SAME FILE
//      (do not fork or re-declare DeviceManager elsewhere).
//   3. Add a correct sizeof check once the full layout is present.

#include "types.hpp"
// CgsAssert.h provides CGS_ASSERT.
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsFileSystem
{
    // Forward-declared: used only as a pointer in the deferred member block below.
    // The full layout lives in CgsDevice.h (not yet reconstructed).
    class Device;

    struct DeviceManager
    {
        // ---- static integral constants (DWARF CgsDeviceManager.h:50-54) ----
        static const s32 KI_MAX_PREFIX_LENGTH       = 8;
        static const s32 KI_MAX_PHYSICAL_DEVICES    = 8;
        static const s32 KI_MAX_VIRTUAL_DEVICES     = 32;
        static const s32 KI_MAX_DEFAULT_PATH_LENGTH = 256;

        // ---- static singleton (DWARF CgsDeviceManager.h:153) ---------------
        // Defined once in CgsDeviceManager.cpp. External linkage is required so
        // that InitializeDeviceManager (in that same .cpp) can write it and every
        // caller TU (BrnReplays::*, CgsFileSystem::FileSystem::Prepare, etc.) can
        // read it through this declaration.
    private:
        static DeviceManager* mpDeviceManager;
        // static rw::IResourceAllocator* mpAllocator;  // DWARF:154 — deferred (rw:: not yet in scope)

        // ---- DEFERRED INSTANCE MEMBERS (uncomment when EAThread cascade exists) ----
        // DWARF CgsDeviceManager.h:139-145:
        //   EAThreadDynamicData::ThreadId mInitializationThread;     // :139
        //   bool                          mbInitialized;              // :140
        //   bool                          mbReleasing;                // :141
        //   PhysicalDeviceSlot            maPhysicalDevices[KI_MAX_PHYSICAL_DEVICES]; // :142
        //   VirtualDeviceSlot             maVirtualDevices[KI_MAX_VIRTUAL_DEVICES];   // :143
        //   Futex                         mDeviceListFutex;           // :144
        //   char                          macDefaultPath[KI_MAX_DEFAULT_PATH_LENGTH + 1]; // :145
        //
        // DEFERRED NESTED STRUCTS (DWARF CgsDeviceManager.h:57-69):
        //   struct PhysicalDeviceSlot { char macPrefix[KI_MAX_PREFIX_LENGTH+1]; Device* mpDevice; OperationPool mOperations; Thread mThread; };
        //   struct VirtualDeviceSlot  { char macPrefix[KI_MAX_PREFIX_LENGTH+1]; Device* mpDevice; };

    public:
        // CgsFileSystem::GetDeviceManager() is a namespace-scope free function
        // (DWARF CgsDeviceManager.h:176) that reads the private mpDeviceManager.
        // Grant it friend access so no public accessor is needed.
        friend DeviceManager* GetDeviceManager();
    };

    // Namespace-scope free function (DWARF CgsDeviceManager.h:176).
    DeviceManager* GetDeviceManager();

} // namespace CgsFileSystem
