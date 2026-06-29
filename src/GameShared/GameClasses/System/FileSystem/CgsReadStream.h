#pragma once

// CgsReadStream.h — canonical home for CgsFileSystem::ReadStream, the lightweight by-value
// handle the resource layer holds onto a StreamDeviceDiskRead. It is a single-pointer wrapper
// (so passing a ReadStream by value passes the underlying StreamDeviceDiskRead*), which is why
// the X360 FileSystem::GetReadStreamIndex / Is{Open,Closed}(ReadStream) accessors receive the
// raw device pointer in r4.
//
// SOURCES: DecFIGS DWARF (CgsReadStream.h) for the single member + the public API shape.
// MINIMAL SLICE: only the member + the API surface are declared; the forwarding bodies live in
// CgsReadStream.cpp (a follow-on TU). FileSystem only needs the pointer.

#include "types.hpp"

namespace CgsFileSystem
{
    struct StreamDeviceDiskRead;

    // ---- DWARF CgsReadStream.h:43 ----
    struct ReadStream
    {
    public:
        // The file system reads mpStreamDevice directly (the X360 accessors take it in r4).
        friend class FileSystem;

        // ---- public API (DWARF CgsReadStream.h:49-89) ----
        // DECLARE-ONLY: bodies forward to mpStreamDevice in CgsReadStream.cpp.
        void        Construct(StreamDeviceDiskRead* lpStreamDevice); // :49
        bool        StartAsyncRead(void** lppData, u32* lpuAmount);  // :55
        void        StopAsyncRead(u32 luAmount);                     // :60
        u32         Read(u32 luAmount, void* lpDest);                // :66
        void        Seek(u64 luPosition);                            // :71
        u64         Tell() const;                                    // :74
        bool        IsValid() const;                                 // :77
        ReadStream& operator=(StreamDeviceDiskRead* lpStreamDevice); // :80
        u32         GetAmountOfDataInBuffer() const;                 // :83
        void        LogAmountOfDataInBuffer() const;                 // :86
        bool        IsBufferComplete() const;                        // :89

    private:
        StreamDeviceDiskRead* mpStreamDevice; // :93
    };

} // namespace CgsFileSystem
