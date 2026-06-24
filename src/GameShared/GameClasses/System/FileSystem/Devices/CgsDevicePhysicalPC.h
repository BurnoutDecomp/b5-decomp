#pragma once

// CgsDevicePhysicalPC — the concrete physical file device for the PC build: a CgsFileSystem::
// Device backed by real Win32 file I/O. This is the PC-IO-AT-THE-LEAF reconstruction of the
// X360 PhysicalX360Device (0x828F9728 etc.), which used the XDK CreateFileA/ReadFile against
// the DVD/HDD. The async shape above it (DeviceManager OperationPool + worker thread) is the
// faithful X360 engine; only this leaf — the actual byte transfer — is re-expressed with the
// host file API (marked), exactly as the bundle loader's CRT leaf is today.
//
// The device's worker-dispatched ops receive an integer file handle; this device maps those
// integers onto Win32 HANDLEs through a small fixed table (the int handle is the table index).

#include "types.hpp"
#include "GameShared/GameClasses/System/FileSystem/Devices/CgsDevice.h"

namespace CgsFileSystem
{
    class DevicePhysicalPC : public Device
    {
    public:
        DevicePhysicalPC();
        virtual ~DevicePhysicalPC();

        // ---- worker-dispatched ops (see CgsDevice.h for the opcode->method mapping) ----
        int Connect() override;                                                              // worker start: no-op
        int Open(const char* lpcPath, int liMode, int* lpiOutHandle) override;               // CreateFileA
        int Close(int liHandle) override;                                                    // CloseHandle
        int Read(int liHandle, u64 lu64Offset, u32 luSize, void* lpBuffer, int* lpiOutResult) override;        // SetFilePointerEx + ReadFile
        int Write(int liHandle, u64 lu64Offset, u32 luSize, const void* lpBuffer, int* lpiOutResult) override;  // SetFilePointerEx + WriteFile
        int GetFileSize(int liHandle, u64* lpu64OutSize) override;                            // GetFileSizeEx
        int Seek(int liHandle, u64 lu64Offset, int* lpiOutResult) override;                   // SetFilePointerEx
        int Shutdown() override;                                                              // close all open handles

    private:
        static const int KI_MAX_OPEN_FILES = 32;

        // Look up the Win32 HANDLE for an integer handle (the table index); returns the
        // INVALID sentinel for an out-of-range / closed slot.
        void* HandleFor(int liHandle) const;

        void* mapHandles[KI_MAX_OPEN_FILES];  // Win32 HANDLEs (INVALID_HANDLE_VALUE == free)
    };
}
