#include "GameShared/GameClasses/Development/Log/CgsLogFileBuffered.h"
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"  // CgsCore::StrCpy
#define CGS_ARTIST_FILE "..\\..\\..\\GameShared\\GameClasses\\Development/Log/CgsLogFileBuffered.cpp"
#include <cstring>

// CgsDev::Log::LogFileBuffered - the buffered log-file stream ctor + sink. Reconstructed from the
// DecFIGS DWARF (Development/Log/CgsLogFileBuffered.h) + the X360 default-ctor at 0x826613A8.

namespace CgsDev
{
namespace Log
{
    // ARTIST82820978. Open truncates the file, closes the handle immediately,
    // and retains the name/buffer for subsequent append-mode flushes.
    bool LogFileBuffered::Open(const char* lpcFileName, char* lpcBuffer, s32 liBufferLength)
    {
        // ARTIST 0x828209CC: the inlined CgsCore::StrCpy (its CgsStringUtils.h:65 assert), with
        // BeginAssert/EndAssert inlined as CgsDevelopmentUnity defines them.
        CgsCore::StrCpy(macFileName, sizeof(macFileName), lpcFileName);
        mpcBuffer = lpcBuffer;
        miBufferLength = liBufferLength;
        miBufferPosition = 0;
        miFile = CreateFileA(lpcFileName, GENERIC_WRITE, FILE_SHARE_WRITE, 0,
                             CREATE_ALWAYS, 0, 0);
        CGS_ASSERT_AT(miFile != INVALID_HANDLE_VALUE, "mFileHandle != INVALID_HANDLE_VALUE",
            CGS_ARTIST_FILE, 66);
        CloseHandle(miFile);
        miFile = INVALID_HANDLE_VALUE;
        return true;
    }

    // ARTIST82820B90: even a zero-byte flush reopens an existing named file.
    void LogFileBuffered::Flush()
    {
        // ARTIST 0x82820BA8..0x82820BAC forms the filename argument and tests the member separately.
        const char* lpcFileName = macFileName;
        if (macFileName[0] != '\0')
        {
            miFile = CreateFileA(lpcFileName, GENERIC_WRITE, FILE_SHARE_WRITE,
                                 0, OPEN_EXISTING, 0, 0);
            // ARTIST 0x82820BE0: BeginAssert/EndAssert inlined (CgsDevelopmentUnity defines them).
            CGS_ASSERT_AT(miFile != INVALID_HANDLE_VALUE, "mFileHandle != INVALID_HANDLE_VALUE",
                CGS_ARTIST_FILE, 156);
            SetFilePointer(miFile, 0, 0, FILE_END);
            DWORD luWritten;
            WriteFile(miFile, mpcBuffer, static_cast<DWORD>(miBufferPosition), &luWritten, 0);
            CloseHandle(miFile);
            miFile = INVALID_HANDLE_VALUE;
            miBufferPosition = 0;
        }
    }

    void LogFileBuffered::Close()
    {
        Flush();
        macFileName[0] = '\0';
        miFile = INVALID_HANDLE_VALUE;
        mpcBuffer = 0;
        miBufferLength = 0;
        miBufferPosition = 0;
    }

    // ARTIST82827110. Equality leaves a full buffer pending; only an overflow
    // flushes in this loop. The caller must supply an opened, nonzero buffer.
    void LogFileBuffered::Append(const char* lpcText)
    {
        const s32 liLength = static_cast<s32>(std::strlen(lpcText));
        s32 liCopied = 0;
        while (liCopied < liLength)
        {
            const s32 liRemaining = liLength - liCopied;
            const s32 liAvailable = miBufferLength - miBufferPosition;
            if (liAvailable < liRemaining)
            {
                std::memcpy(mpcBuffer + miBufferPosition, lpcText + liCopied,
                            static_cast<size_t>(liAvailable));
                miBufferPosition += liAvailable;
                liCopied += liAvailable;
                Flush();
            }
            else
            {
                std::memcpy(mpcBuffer + miBufferPosition, lpcText + liCopied,
                            static_cast<size_t>(liRemaining));
                miBufferPosition += liRemaining;
                liCopied += liRemaining;
            }
        }
    }
}
}

#undef CGS_ARTIST_FILE
