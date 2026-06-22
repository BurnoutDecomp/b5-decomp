#include "GameShared/GameClasses/Development/Log/CgsLogFileBuffered.h"

#include <cstring>

// CgsDev::Log::LogFileBuffered - the buffered log-file stream ctor + sink. Reconstructed from the
// DecFIGS DWARF (Development/Log/CgsLogFileBuffered.h) + the X360 default-ctor at 0x826613A8.

namespace CgsDev
{
namespace Log
{
    // X360 0x826613A8. Member stores from the pseudocode map onto the DWARF layout:
    //   *(this+8)  = -1   -> miFile
    //   *(this+12) = 0    -> mpcBuffer
    //   *(this+16) = 0    -> miBufferLength
    //   *(this+20) = 0    -> macFileName[0] (clear the path)
    //   *(this+532)= 0    -> miBufferPosition  (20 + 512 == 532)
    // mePrintMode (this+4) and the vtable (this+0) are set by the StrStreamBase base ctor.
    LogFileBuffered::LogFileBuffered()
        : miFile(-1)
        , mpcBuffer(nullptr)
        , miBufferLength(0)
        , miBufferPosition(0)
        , mbAppend(false)
    {
        macFileName[0] = '\0';
    }

    // operator<<(const char*) sink. NOTE: this override is COMPILE-REQUIRED (the base
    // StrStreamBase::operator<<(const char*) is pure-virtual, so a concrete LogFileBuffered must
    // provide it to be instantiable) but it is NOT this TU's ledger func and has NO asm in this
    // TU's dossier -- the real buffering/flush logic lives in LogFileBuffered::Append/Flush/Open
    // (their own not-yet-done TUs). The body below is a CONSERVATIVE RECONSTRUCTION (not recovered
    // from the binary): while the stream is closed/unbuffered (the default-constructed state) there
    // is nowhere to write, so it is a guarded no-op; if a buffer has been bound it appends bounded.
    StrStreamBase& LogFileBuffered::operator<<(const char* lpcText)
    {
        if (lpcText && mpcBuffer && miBufferLength > 0)
        {
            for (const char* lpc = lpcText; *lpc; ++lpc)
            {
                if (miBufferPosition >= miBufferLength)
                    break; // flush is performed by Flush()/Close() (their own TUs)
                mpcBuffer[miBufferPosition++] = *lpc;
            }
        }
        return *this;
    }
}
}
