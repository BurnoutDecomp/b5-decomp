// CgsDeviceMemFileSystem.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsFileSystem::DeviceMemFileSystem::`vector deleting destructor' @ 0x828E0FF8
//
// The X360 codegen at 0x828E0FF8 is MSVC's vector-deleting-destructor for the
// polymorphic DeviceMemFileSystem device: it restores the device vtable pointer
// (off_8209FEA8) at this+0 and, if the low "should-free" flag bit is set, frees the
// object before returning `this`.
//
//   *result = &off_8209FEA8;            // restore the vtable slot
//   if ( a2 & 1 ) free(result);
//
// The in-memory device's own members are not freed by this thunk, so the destructor
// body is empty; the compiler synthesises the vtable-store + conditional free shown
// above from this trivial virtual ~DeviceMemFileSystem(). This TU owns only the
// destructor so its vtable slot is emitted exactly once.

#include "GameShared/GameClasses/System/FileSystem/Devices/CgsDeviceMemFileSystem.h"

namespace CgsFileSystem
{
    DeviceMemFileSystem::~DeviceMemFileSystem()
    {
    }

} // namespace CgsFileSystem
