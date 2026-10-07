#include "SDKs/EATech/rwcore/filesys/device.h"  // rw::core::filesys::DeviceDriver (shared home)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   rw::core::filesys::DeviceDriver::DeviceDriver @ 0x82BBD548
//
// Constructor: installs the DeviceDriver vtable at offset 0 and copies the device
// name (a null-terminated string, copied including its terminator) into the inline
// name buffer beginning at offset 4.
//
// The DeviceDriver class itself is defined once in device.h; this TU homes the
// constructor.
//
// The compiler-generated `vector deleting destructor` (0x82661060) is intentionally
// omitted — it is a thunk (vtable install + conditional operator delete), not source.

namespace rw
{
    namespace core
    {
        namespace filesys
        {
            DeviceDriver::DeviceDriver(const char* pName)
            {
                char* lpDest = mDeviceName;
                const char* lpSrc = pName;
                char lcCh;
                do
                {
                    lcCh = *lpSrc;
                    *lpDest++ = lcCh;
                    ++lpSrc;
                } while (lcCh);
            }
        }
    }
}
