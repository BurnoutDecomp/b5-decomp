#include "GameShared/GameClasses/Development/Log/CgsLogFileBuffered.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstring>
#include <new>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
    struct Failure { std::string message, file; int line; };
    std::vector<Failure> gAsserts;
    int gChecks = 0, gFailures = 0;
    void Check(bool value, const char* label)
    {
        ++gChecks;
        if (!value) { ++gFailures; std::printf("FAIL %s\n", label); }
    }
    std::string Read(const char* path)
    {
        HANDLE input = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (input == INVALID_HANDLE_VALUE) return {};
        char bytes[256];
        DWORD count = 0;
        ReadFile(input, bytes, sizeof(bytes), &count, nullptr);
        CloseHandle(input);
        return std::string(bytes, count);
    }
}
namespace CgsDev { namespace Assert
{
    int BeginAssert() { return 0; }
    int FireAssert(const char* message, const char* file, int line)
    {
        gAsserts.push_back({message, file, line});
        return 0;
    }
    void* EndAssert() { return nullptr; }
}}

#include "ResourceLog.inc"

int main()
{
    using CgsDev::Log::LogFileBuffered;
    const char* const path = "resource-buffered-log.tmp";
    char buffer[4];
    {
        LogFileBuffered log;
        Check(log.miFile == INVALID_HANDLE_VALUE, "constructor invalid handle");
        Check(log.mpcBuffer == nullptr, "constructor no buffer");
        Check(log.miBufferLength == 0, "constructor length");
        Check(log.miBufferPosition == 0, "constructor position");
        Check(log.macFileName[0] == 0, "constructor empty filename");
        Check(log.Open(path, buffer, 4), "Open returns true");
        Check(log.miFile == INVALID_HANDLE_VALUE, "Open closes temporary handle");
        Check(log.mpcBuffer == buffer && log.miBufferLength == 4, "Open retains scratch buffer");
        Check(GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES && Read(path).empty(), "Open creates empty file");
        log << "abc";
        Check(log.miBufferPosition == 3, "short append remains buffered");
        Check(Read(path).empty(), "short append has not flushed");
        log << "d";
        Check(log.miBufferPosition == 4, "exact capacity remains buffered");
        Check(Read(path).empty(), "equality does not flush");
        log << "";
        Check(log.miBufferPosition == 4 && Read(path).empty(), "empty append does not flush full buffer");
        log << "efghi";
        Check(Read(path) == "abcdefgh", "overflow writes both complete blocks");
        Check(log.miBufferPosition == 1, "overflow retains trailing byte");
        Check(buffer[0] == 'i', "overflow copies trailing byte");
        log.Flush();
        Check(Read(path) == "abcdefghi", "explicit Flush writes remainder");
        Check(log.miBufferPosition == 0, "Flush resets position");
        Check(log.miFile == INVALID_HANDLE_VALUE, "Flush closes handle");
        log.Flush();
        Check(Read(path) == "abcdefghi", "zero-byte Flush preserves file");
        log << 42;
        log.Close();
        Check(Read(path) == "abcdefghi42", "numeric stream uses char sink and Close flushes");
        Check(log.macFileName[0] == 0, "Close clears filename");
        Check(log.miFile == INVALID_HANDLE_VALUE, "Close invalidates handle");
        Check(log.mpcBuffer == nullptr && log.miBufferLength == 0, "Close clears buffer binding");
        Check(log.miBufferPosition == 0, "Close resets position");
    }
    Check(gAsserts.empty(), "valid native lifecycle has no assertions");
    {
        LogFileBuffered log;
        log.Open(path, buffer, 4);
        Check(Read(path).empty(), "Open truncates existing file");
        log << "0123456789";
    }
    Check(Read(path) == "0123456789", "destructor flushes tail after multiple blocks");
    {
        LogFileBuffered log;
        log.Open(path, buffer, 4);
        const char text[] = {'x', 0, 'y', 0};
        log << text;
    }
    Check(Read(path) == "x", "NUL terminates append");
    {
        alignas(LogFileBuffered) unsigned char storage[sizeof(LogFileBuffered)];
        std::memset(storage, 0x5A, sizeof(storage));
        LogFileBuffered* log = new (storage) LogFileBuffered;
        unsigned char untouched;
        std::memcpy(&untouched, &log->mbAppend, 1);
        Check(untouched == 0x5A, "constructor leaves unattested mbAppend byte untouched");
        log->~LogFileBuffered();
    }
    {
        LogFileBuffered log;
        Check(log.Open("missing-directory-for-resource-test/failed.tmp", buffer, 4), "failed Open still returns true");
        Check(gAsserts.size() == 1 && gAsserts[0].line == 66, "failed Open asserts original line66");
        Check(!gAsserts.empty() && gAsserts[0].message == "mFileHandle != INVALID_HANDLE_VALUE", "failed Open assertion message");
        log.Close();
        Check(gAsserts.size() == 2 && gAsserts[1].line == 156, "failed Flush asserts original line156");
        Check(log.macFileName[0] == 0 && log.mpcBuffer == nullptr, "Close clears state after failed Flush");
    }
    std::printf("ResourceLog: %d checks, %d failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
