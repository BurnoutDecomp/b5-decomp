#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include <xtl.h>

// CgsDev::Log::LogFileBuffered - a StrStreamBase whose char* sink buffers text in a caller-supplied
// scratch buffer and reopens the named file for each flush. Used by the resource/allocator debug dump
// paths (BrnResource::GameDataIO::AllocatorList::DebugDumpStats, CgsResource::DebugComponent::DumpStats)
// to write stats to a log file via `stream << ...`. Reconstructed from the DecFIGS DWARF
// (Development/Log/CgsLogFileBuffered.h) + ARTIST826613A8,82820978,82820B90,828270C8,82827110.
//
// Layout (X360, from DWARF): StrStreamBase (vtable + mePrintMode) then miFile, mpcBuffer,
// miBufferLength, macFileName[512], miBufferPosition, mbAppend. The default ctor leaves the stream
// closed (miFile = -1) with no buffer attached; Open() binds a file + buffer before use.
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
        ~LogFileBuffered();

        bool Open(const char* lpcFileName, char* lpcBuffer, s32 liBufferLength);
        void Close();
        void Flush();
        using StrStreamBase::operator<<;

        // The char sink forwards to the recovered Append body; Open() is required.
        StrStreamBase& operator<<(const char* lpcText) ;

    protected:
        HANDLE miFile;            // X360 +0x08, DWARF CgsLogFileBuffered.h:81
        void Append(const char* lpcText);
        char* mpcBuffer;          // caller-supplied flush scratch
        s32   miBufferLength;     // capacity of mpcBuffer
        char  macFileName[512];   // file path this stream writes to
        s32   miBufferPosition;   // bytes currently buffered in mpcBuffer
        bool  mbAppend;           // declared by DecFIGS; untouched by these ARTIST bodies
    };
}
}
