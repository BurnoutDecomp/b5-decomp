#pragma once

// CgsDevice.h — canonical home for CgsFileSystem::Device, the abstract base of
// every file device in the resource file-I/O subsystem (the physical X360
// device, the in-memory device, the remap/pass-through device, ...).
//
// SOURCES (X360 ARTIST):
//   Device::CallErrorCallback @ 0x828D65F0  (invokes the registered error
//                                             callback, then yields the thread)
//   Device::GetFileSize       @ 0x828DDE50  ("Not implemented" default, returns -2)
//   Device::OpenDirectory     @ 0x828DDEE0  ("Not implemented" default, returns -2)
//   Device::CloseDirectory    @ 0x828DDF70  ("Not implemented" default, returns -2)
//   Device::ReadDirectory     @ 0x828DE000  ("Not implemented" default, returns -2)
//   (default-body source line cites CgsDevice.cpp:91/103/111/122)
//
// The four directory/size entry points are NON-pure virtuals: the base supplies a
// default body that fires CGS_ASSERT(false, "Not implemented\n") and returns -2 so
// a device that does not support the operation reports a generic failure. Concrete
// devices (PhysicalX360Device, DeviceMemFileSystem) override the ones they
// implement. CallErrorCallback is a non-virtual base helper.
//
// LAYOUT NOTE (from the asm): CallErrorCallback reads the error-callback fn-ptr at
// this+8 (the asm does `lwz r11, 8(r3)`), so the base carries an
// ErrorCallback member named mpfErrorCallback that every concrete device inherits;
// PhysicalX360Device::Seek/CloseDirectory call CallErrorCallback(this, ...) and the
// base reads that inherited slot. Members are accessed strictly by name.

#include "types.hpp"

namespace CgsFileSystem
{
    // A device-level error callback: it receives the device-specific error code and
    // returns the (possibly remapped) result the failing operation should report.
    typedef int (*ErrorCallback)(int liError);

    class Device
    {
    public:
        virtual ~Device() {}

        // ---- the per-operation virtual interface (vtable slots 1..5) ----------
        // Pure here: every concrete device must supply them.
        virtual int Init(int liMode, int liArg) = 0;
        virtual int Close() = 0;
        virtual int Read() = 0;
        virtual int Write() = 0;
        virtual int Seek() = 0;

        // ---- default-implemented virtuals (vtable slots 6..9) -----------------
        // The base supplies a "Not implemented" body (CgsDevice.cpp); concrete
        // devices override the operations they support.
        virtual int GetFileSize();      // 0x828DDE50
        virtual int OpenDirectory();    // 0x828DDEE0
        virtual int CloseDirectory();   // 0x828DDF70
        virtual int ReadDirectory();    // 0x828DE000

        // ---- non-virtual helper ----------------------------------------------
        // @0x828D65F0. If an error callback is registered, invoke it with liError
        // and remember its result; then yield the calling thread for 5ms (the X360
        // build sleeps so a hammering retry loop does not starve the I/O thread)
        // and return the callback's result (0 when no callback is set).
        int CallErrorCallback(int liError);

    protected:
        // The registered per-device error callback (asm: 8(this) in
        // CallErrorCallback). 0 when no callback is installed.
        ErrorCallback mpfErrorCallback;
    };

} // namespace CgsFileSystem
