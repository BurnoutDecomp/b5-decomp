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
                void* Construct(const char* pName);
            };

            void* DeviceDriver::Construct(const char* pName)
            {
                u32* lpThis = reinterpret_cast<u32*>(this);
                lpThis[0] = static_cast<u32>(reinterpret_cast<uintptr_t>(&gDeviceDriverVTable));

                char* lpDest = reinterpret_cast<char*>(this) + 4;
                const char* lpSrc = pName;
                char lcCh;
                do
                {
                    lcCh = *lpSrc;
                    *lpDest++ = lcCh;
                    ++lpSrc;
                } while (lcCh);

                return lpThis;
            }
        }
    }
}
