#pragma once

// CgsDeviceMemFileSystem.h — canonical home for CgsFileSystem::DeviceMemFileSystem,
// the in-memory file-system device (a CgsFileSystem::Device that serves files out
// of a RAM image rather than physical storage).
//
// SOURCE (X360 ARTIST):
//   DeviceMemFileSystem::`vector deleting destructor' @ 0x828E0FF8
//
// MINIMAL SLICE: this pass lands only the polymorphic type + its virtual destructor
// so the destructor's vtable slot (X360 off_8209FEA8) is emitted exactly once. The
// device's real members (the backing memory image, the per-file table) and its
// Init/Close/Read/Write/Seek overrides are added when those bodies are
// reconstructed. The destructor itself owns no heap members — the X360 thunk only
// restores the vtable pointer and conditionally frees the object.

#include "types.hpp"
#include "GameShared/GameClasses/System/FileSystem/Devices/CgsDevice.h"

namespace CgsFileSystem
{
    class DeviceMemFileSystem : public Device
    {
    public:
        // @0x828E0FF8 (MSVC synthesises the 'vector deleting destructor' thunk from
        // this virtual ~DeviceMemFileSystem(): it restores the vtable slot at this+0
        // and, when the low "should-free" flag bit is set, frees the object).
        virtual ~DeviceMemFileSystem();

        // The per-operation overrides (Init/Close/Read/Write/Seek) are reconstructed
        // separately; until then the class inherits Device's pure declarations and
        // stays abstract. The defined virtual destructor below still forces the
        // vtable (and the vector-deleting-destructor thunk) to be emitted in the .cpp.
    };

} // namespace CgsFileSystem
