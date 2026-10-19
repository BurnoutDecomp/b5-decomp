#include "GameShared/GameClasses/Development/Log/CgsLogFileBuffered.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

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
        : miFile(INVALID_HANDLE_VALUE)
        , mpcBuffer(nullptr)
        , miBufferLength(0)
        , miBufferPosition(0)
    {
        macFileName[0] = '\0';
    }

    LogFileBuffered::~LogFileBuffered()
    {
        Flush(); //inlined in ARTIST828EE1B0..828EE1C0
    }

    // ARTIST82820978. Open truncates the file, closes the handle immediately,
    // and retains the name/buffer for subsequent append-mode flushes.
    bool LogFileBuffered::Open(const char* lpcFileName, char* lpcBuffer, s32 liBufferLength)
    {
        const s32 liLength = static_cast<s32>(std::strlen(lpcFileName));
        if (liLength >= 512)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::SimpleStrStream lMessage;
            lMessage << "String " << (lpcFileName ? lpcFileName : "<NULLSTRING>")
                     << " is too long. Buffer size = " << 512
                     << ", string length = " << liLength << "\n";
            CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
                "..\\..\\..\\GameShared\\GameClasses\\Core/CgsStringUtils.h", 65);
            CgsDev::Assert::EndAssert();
        }
        std::strcpy(macFileName, lpcFileName);
        mpcBuffer = lpcBuffer;
        miBufferLength = liBufferLength;
        miBufferPosition = 0;
        miFile = CreateFileA(lpcFileName, GENERIC_WRITE, FILE_SHARE_WRITE, nullptr,
                             CREATE_ALWAYS, 0, nullptr);
        if (miFile == INVALID_HANDLE_VALUE)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("mFileHandle != INVALID_HANDLE_VALUE",
                "..\\..\\..\\GameShared\\GameClasses\\Development/Log/CgsLogFileBuffered.cpp", 66);
            CgsDev::Assert::EndAssert();
        }
        CloseHandle(miFile);
        miFile = INVALID_HANDLE_VALUE;
        return true;
    }

    // ARTIST82820B90: even a zero-byte flush reopens an existing named file.
    void LogFileBuffered::Flush()
    {
        if (macFileName[0] != '\0')
        {
            miFile = CreateFileA(macFileName, GENERIC_WRITE, FILE_SHARE_WRITE,
                                 nullptr, OPEN_EXISTING, 0, nullptr);
            if (miFile == INVALID_HANDLE_VALUE)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("mFileHandle != INVALID_HANDLE_VALUE",
                    "..\\..\\..\\GameShared\\GameClasses\\Development/Log/CgsLogFileBuffered.cpp", 156);
                CgsDev::Assert::EndAssert();
            }
            SetFilePointer(miFile, 0, nullptr, FILE_END);
            DWORD luWritten;
            WriteFile(miFile, mpcBuffer, static_cast<DWORD>(miBufferPosition), &luWritten, nullptr);
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
        mpcBuffer = nullptr;
        miBufferLength = 0;
        miBufferPosition = 0;
    }

    // The typed C++ char sink maps to the original Append vtable entry.
    StrStreamBase& LogFileBuffered::operator<<(const char* lpcText)
    {
        Append(lpcText);
        return *this;
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
            if (liAvailable >= liRemaining)
            {
                std::memcpy(mpcBuffer + miBufferPosition, lpcText + liCopied,
                            static_cast<size_t>(liRemaining));
                liCopied += liRemaining;
                miBufferPosition += liRemaining;
            }
            else
            {
                std::memcpy(mpcBuffer + miBufferPosition, lpcText + liCopied,
                            static_cast<size_t>(liAvailable));
                liCopied += liAvailable;
                miBufferPosition += liAvailable;
                Flush();
            }
        }
    }
}
}
