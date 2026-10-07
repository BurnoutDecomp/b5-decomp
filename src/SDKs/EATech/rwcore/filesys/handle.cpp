// =====================================================================================
// rw::core::filesys::Handle -- constructor, destructor + scalar deleting destructor.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for these TUs.
//
//   rw::core::filesys::Handle::~Handle                 @0x82BBD6A0
//   rw::core::filesys::Handle::`scalar deleting destructor' @0x82BBDFA8
//
// ~Handle: if the handle is open (mpFile != null), close the file through its driver,
// then zero the object in the console's store order: +0x04, +0x08, +0x0C, +0x10, +0x00.
//
// The scalar deleting destructor is the compiler thunk MSVC emits for `delete`: run
// ~Handle, then if (flags & 1) free the storage through the global filesys allocator
// (off_8327F07C) via its vtable slot +0xC -- Free(allocator, this, 0). Modelled with a
// faithful allocator-shaped indirect call; the global allocator object is owned by the
// device-driver TU (extern here).
// =====================================================================================

#include "SDKs/EATech/rwcore/filesys/device.h"   // Handle / Device / DeviceDriver / Manager

#include <cstdio>    // sprintf
#include <cstring>   // strcpy

namespace rw
{
    namespace core
    {
        namespace filesys
        {
            // X360 off_8327F07C -- the global filesys allocator. Installed by filesys
            // bring-up; null until then. Single definition lives here.
            Allocator* gpFileSysAllocator = nullptr;

            // Handle::Handle
            //
            // Open lpcPath (a leading "./" or ".\" dropped) through lpDevice's driver. When
            // lpDevice is the Manager's default device the path is tried against every
            // search location in turn (starting each location's device if it is idle):
            // an absolute path as-is, a relative one as "<location>/<path>". On success the
            // handle records the device/driver the file was found on and the driver's
            // reported size, and owns itself unless the driver linked the file to an
            // existing handle (Open's out-parameter), whose owner and device it adopts.
            Handle::Handle(const char* lpcPath, u32 luFlags, Device* lpDevice)
                : mField0(0)
                , mpOwnerHandle(nullptr)
                , mpFile(nullptr)
                , mpDevice(lpDevice)
                , mpDriver(lpDevice->GetDriver())
                , mu64Size(0)
                , mu64Reserved20(0)
            {
                if (lpcPath[0] == '.' && (lpcPath[1] == '/' || lpcPath[1] == '\\'))
                    lpcPath += 2;

                Handle* lpLinkedHandle = nullptr;
                Device* lpOpenDevice = lpDevice;

                if (lpDevice == gpFileSysManager->mpDefaultDevice)
                {
                    for (u32 luIndex = 0; luIndex < gpFileSysManager->muMaxSearchPaths; ++luIndex)
                    {
                        const SearchPathEntry& lrEntry = gpFileSysManager->mpSearchPathTable[luIndex];
                        const char* lpcLocation = lrEntry.mpcPath;
                        lpOpenDevice = lrEntry.mpDevice;
                        if (!lpcLocation)
                            break;

                        if (!lpOpenDevice->mbThreadRunning)
                            lpOpenDevice->Start();

                        char lacPath[256];
                        if (lpcPath[0] == '\\' || lpcPath[0] == '/')
                            ::strcpy(lacPath, lpcPath);
                        else
                            ::sprintf(lacPath, "%s/%s", lpcLocation, lpcPath);

                        mpFile = lpOpenDevice->GetDriver()->Open(lacPath, luFlags, &lpLinkedHandle);
                        if (mpFile)
                            break;
                    }
                }
                else
                {
                    mpFile = lpDevice->GetDriver()->Open(lpcPath, luFlags, &lpLinkedHandle);
                }

                if (mpFile)
                {
                    mpDevice = lpOpenDevice;
                    mpDriver = lpOpenDevice->GetDriver();
                    mu64Size = mpDriver->GetSize(mpFile);
                    mpOwnerHandle = this;
                    if (lpLinkedHandle)
                    {
                        mpOwnerHandle = lpLinkedHandle;
                        mpDevice = lpLinkedHandle->mpDevice;
                    }
                }
            }

            // ~Handle @0x82BBD6A0.
            //   if (mpFile) mpDriver->Close(mpFile);    // driver vtable +0x10
            //   +0x04 = +0x08 = +0x0C = +0x10 = 0; +0x00 = 0;
            Handle::~Handle()
            {
                if (mpFile)
                    mpDriver->Close(mpFile);

                // Zero in the console's store order: +4, +8, +0xC, +0x10, then +0.
                mpOwnerHandle = nullptr;
                mpFile        = nullptr;
                mpDevice      = nullptr;
                mpDriver      = nullptr;
                mField0       = 0;
            }

            // Handle::`scalar deleting destructor' @0x82BBDFA8.
            //   ~Handle(this);
            //   if (flags & 1)
            //       (*(*gpFileSysAllocator + 0xC))(gpFileSysAllocator, this, 0);  // Free
            //   return this;
            Handle* HandleScalarDeletingDtor(Handle* lpHandle, char lcFlags)
            {
                lpHandle->~Handle();
                if (lcFlags & 1)
                    gpFileSysAllocator->Free(lpHandle, 0);
                return lpHandle;
            }
        }
    }
}
