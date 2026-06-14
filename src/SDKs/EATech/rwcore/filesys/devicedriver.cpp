#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   rw::core::filesys::DeviceDriver::DeviceDriver @ 0x82BBD548
//
// Constructor: installs the DeviceDriver vtable at offset 0 and copies the device
// name (a null-terminated string, copied including its terminator) into the inline
// name buffer beginning at offset 4.
//
// The compiler-generated `vector deleting destructor` (0x82661060) is intentionally
// omitted — it is a thunk (vtable install + conditional operator delete), not source.

namespace rw
{
    namespace core
    {
        namespace filesys
        {
            // DeviceDriver vtable; defined with the class' out-of-line methods (other TU).
            extern void* const gDeviceDriverVTable[];

            class DeviceDriver
            {
            public:
                DeviceDriver(const char* pName);

            private:
                void* mpVTable;
                char  macName[1];
            };

            DeviceDriver::DeviceDriver(const char* pName)
            {
                mpVTable = const_cast<void*>(reinterpret_cast<const void*>(&gDeviceDriverVTable));

                char* lpDest = macName;
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
