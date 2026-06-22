#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/CgsStrStream.h"

// CgsDev::Log::LogFileBuffered - a StrStreamBase whose char* sink buffers text in a caller-supplied
// scratch buffer and flushes it to an open file handle. Used by the resource/allocator debug dump
// paths (BrnResource::GameDataIO::AllocatorList::DebugDumpStats, CgsResource::DebugComponent::DumpStats)
// to write stats to a log file via `stream << ...`. Reconstructed from the DecFIGS DWARF
// (Development/Log/CgsLogFileBuffered.h) + the X360 default-ctor at 0x826613A8.
//
// Layout (X360, from DWARF): StrStreamBase (vtable + mePrintMode) then miFile, mpcBuffer,
// miBufferLength, macFileName[512], miBufferPosition, mbAppend. The default ctor leaves the stream
// closed (miFile = -1) with no buffer attached; Open() (its own TU) binds a file + buffer before use.
namespace CgsDev
{
namespace Log
{
    struct LogFileBuffered : public StrStreamBase
    {
        // X360 0x826613A8. Default-construct a closed, unbuffered log stream: no file (miFile = -1),
        // no scratch buffer (mpcBuffer/miBufferLength = 0), empty filename, write position 0. The
        // StrStreamBase base ctor sets mePrintMode = E_PRINTMODE_DECIMAL; the X360 then clears the
        // remaining members word-for-word (the `BasePriorityQueue::Clear` the decompiler attributes
        // is the folded mePrintMode=0 store, already covered by the base ctor).
        LogFileBuffered();

        // The buffer sink. COMPILE-REQUIRED override of the pure-virtual base sink (not this TU's
        // ledger func; no asm in this TU): on a bound stream it would buffer text into mpcBuffer and
        // flush to miFile (real Append/Flush logic owned by their own not-yet-done TUs). While the
        // stream is closed (the default-constructed state this TU produces) it is a no-op.
        StrStreamBase& operator<<(const char* lpcText) override;

    protected:
        s32   miFile;             // open file handle (-1 = closed)
        char* mpcBuffer;          // caller-supplied flush scratch
        s32   miBufferLength;     // capacity of mpcBuffer
        char  macFileName[512];   // file path this stream writes to
        s32   miBufferPosition;   // bytes currently buffered in mpcBuffer
        bool  mbAppend;           // open in append mode vs truncate
    };
}
}
