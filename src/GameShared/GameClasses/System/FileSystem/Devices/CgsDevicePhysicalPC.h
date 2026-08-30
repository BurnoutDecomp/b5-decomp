#pragma once

// CgsDevicePhysicalPC — the concrete physical file device for the PC build: a CgsFileSystem::
// Device backed by real Win32 file I/O. This is the PC-IO-AT-THE-LEAF reconstruction of the
// X360 PhysicalX360Device (0x828F9728 etc.), which used the XDK CreateFileA/ReadFile against
// the DVD/HDD. The async shape above it (DeviceManager OperationPool + worker thread) is the
// faithful X360 engine; only this leaf — the actual byte transfer — is re-expressed with the
// host file API (marked), exactly as the bundle loader's CRT leaf is today.
//
// The device's worker-dispatched ops receive the opaque native-width DeviceHandle declared by
// DecFIGS. This leaf stores the Win32 HANDLE directly in that token and keeps a small table only
// so Shutdown can close handles that callers left live.

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
        int Open(const char* lpcPath, u32 luMode, Handle::DeviceHandle* lppOutHandle) override; // CreateFileA
        int Close(Handle::DeviceHandle lpHandle) override;                                     // CloseHandle
        int Read(Handle::DeviceHandle lpHandle, void* lpBuffer, u32 luSize,
                 u32* lpuOutResult) override;                                                   // ReadFile
        int Write(Handle::DeviceHandle lpHandle, const void* lpBuffer, u32 luSize,
                  u32* lpuOutResult) override;                                                  // WriteFile
        int GetFileSize(Handle::DeviceHandle lpHandle, u64* lpu64OutSize) override;             // GetFileSizeEx
        int Seek(Handle::DeviceHandle lpHandle, u64 lu64Offset,
                 u64* lpu64OutPosition) override;                                               // SetFilePointerEx
        int Shutdown() override;                                                              // close all open handles

    private:
        static const int KI_MAX_OPEN_FILES = 32;

        // Validate that an opaque token is one of this device's live Win32 handles.
        void* HandleFor(Handle::DeviceHandle lpHandle) const;

        void* mapHandles[KI_MAX_OPEN_FILES];  // Win32 HANDLEs (INVALID_HANDLE_VALUE == free)
    };
}
