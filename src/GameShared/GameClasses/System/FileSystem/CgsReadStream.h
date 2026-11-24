#pragma once

// CgsFileSystem::ReadStream -- the by-value handle the resource layer holds onto a
// StreamDeviceDiskRead: one pointer, so passing a ReadStream passes the device pointer (the X360
// FileSystem::GetReadStreamIndex / IsReadStream{Open,Closed}(ReadStream) take it in r4).
//
// The original is this header alone: DecFIGS has no CgsReadStream.cpp compile unit, the DWARF puts
// every member on a line of this header (struct h:43, Construct h:49 .. IsBufferComplete h:89,
// mpStreamDevice h:93), and ARTIST has no out-of-line copy of any of them -- its callers inline
// them (e.g. FileSystem::OpenReadStreamInternal @0x82906A10 stores the null handle, @0x82906AB0
// the device, @0x82906ABC tests it). So every member is defined here, inline, on its DWARF line.
// LogAmountOfDataInBuffer (h:86) is declared by the DWARF, but no ARTIST code attests a body.

#include "types.hpp"
#include "GameShared/GameClasses/System/FileSystem/CgsStreamDeviceDiskRead.h"

namespace CgsFileSystem
{























    // ---- DWARF CgsReadStream.h:43 ----
    struct ReadStream
    {
    public:
        // The file system reads mpStreamDevice directly (the X360 accessors take it in r4).
        friend class FileSystem;

        void Construct(StreamDeviceDiskRead* lpStreamDevice)
        { mpStreamDevice = lpStreamDevice; }




        bool StartAsyncRead(void** lppData, u32* lpuAmount)
        { return mpStreamDevice->StartAsyncRead(lppData, lpuAmount); }



        void StopAsyncRead(u32 luAmount)
        { mpStreamDevice->StopAsyncRead(luAmount); }




        u32 Read(u32 luAmount, void* lpDest)
        { return mpStreamDevice->Read(luAmount, lpDest); }



        void Seek(u64 luPosition)
        { mpStreamDevice->Seek(luPosition); }

        u64 Tell() const
        { return mpStreamDevice->Tell(); }

        bool IsValid() const
        { return mpStreamDevice != 0; }

        ReadStream& operator=(StreamDeviceDiskRead* lpStreamDevice)
        { mpStreamDevice = lpStreamDevice; return *this; }

        u32 GetAmountOfDataInBuffer() const
        { return mpStreamDevice->GetAmountOfDataInBuffer(); }

        void LogAmountOfDataInBuffer() const;


        bool IsBufferComplete() const
        { return mpStreamDevice->IsEndOfFile(); }

    private:
        StreamDeviceDiskRead* mpStreamDevice;
    };

} // namespace CgsFileSystem
