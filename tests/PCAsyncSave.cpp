#define NOMINMAX
#include <Windows.h>
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

static HANDLE entered, releaseIO;
static std::atomic<bool> blockNext{false}, failFlush{false};
static std::atomic<DWORD> ioThread{0};
static BOOL WINAPI TestWriteFile(HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED);
static BOOL WINAPI TestFlushFileBuffers(HANDLE);
#define WriteFile TestWriteFile
#define FlushFileBuffers TestFlushFileBuffers
#include "async_save_source.inc"
#undef WriteFile
#undef FlushFileBuffers

static BOOL WINAPI TestWriteFile(HANDLE file, LPCVOID data, DWORD bytes, LPDWORD written, LPOVERLAPPED overlapped)
{
    ioThread = GetCurrentThreadId();
    if (blockNext.exchange(false)) { SetEvent(entered); WaitForSingleObject(releaseIO, 5000); }
    return WriteFile(file, data, bytes, written, overlapped);
}
static BOOL WINAPI TestFlushFileBuffers(HANDLE file)
{
    if (failFlush.exchange(false)) { SetLastError(ERROR_WRITE_FAULT); return FALSE; }
    return FlushFileBuffers(file);
}
static unsigned checks, failures;
static void Check(bool value, const char* name)
{ ++checks; if (!value) { ++failures; std::printf("FAIL %s\n", name); } }
static void Block() { ResetEvent(entered); ResetEvent(releaseIO); blockNext = true; }
using namespace CgsGui::SaveLoadPC;
static EWriteStatus Await(WriteTicket ticket, WriteReport& report)
{
    const ULONGLONG until = GetTickCount64() + 10000;
    EWriteStatus status;
    do { status = PollWriteContainer(ticket, report); if (status != E_WRITE_PENDING) return status; Sleep(1); }
    while (GetTickCount64() < until);
    return status;
}
static bool ReadEquals(const char* name, const std::vector<u8>& image, const std::vector<u8>& mugs)
{
    std::vector<u8> out(image.size()), pictures(mugs.size());
    return ReadContainer(name, out.data(), static_cast<u32>(out.size()),
        pictures.data(), static_cast<u32>(pictures.size())) == E_CONTAINERREAD_OK
        && out == image && pictures == mugs;
}
int main()
{
    char cwd[MAX_PATH]; GetCurrentDirectoryA(MAX_PATH, cwd);
    const std::string root = cwd, a = root + "\\case_a", b = root + "\\case_b";
    CreateDirectoryA(a.c_str(), nullptr); CreateDirectoryA(b.c_str(), nullptr);
    entered = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    releaseIO = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    const DWORD mainThread = GetCurrentThreadId();
    Check(InitializeAsyncWrites(), "writer starts before gameplay");
    std::vector<u8> original(262144, 0x55), image(262144, 0x11), mugs(960800, 0x22);
    const auto capturedImage = image;
    Check(WriteContainer("Atomic", original.data(), static_cast<u32>(original.size()),
        mugs.data(), static_cast<u32>(mugs.size()), "title", "description"), "baseline container writes");
    Block();
    HANDLE returned = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    WriteTicket first = 0;
    std::thread submit([&] {
        first = BeginWriteContainer("Atomic", image.data(), static_cast<u32>(image.size()),
            mugs.data(), static_cast<u32>(mugs.size()), "title", "description");
        SetEvent(returned);
    });
    Check(WaitForSingleObject(entered, 2000) == WAIT_OBJECT_0, "native write reaches the controlled slow sink");
    const bool nonblocking = WaitForSingleObject(returned, 200) == WAIT_OBJECT_0;
    Check(nonblocking, "submission returns while the disk writer is still blocked");
    if (!nonblocking) SetEvent(releaseIO);
    submit.join();
    if (!first || !nonblocking) {
        SetEvent(releaseIO); GetAsyncWriter().Stop();
        std::printf("PCAsyncSave: %u checks, %u failures\n", checks, failures);
        return 1;
    }
    Check(ioThread != mainThread, "file IO executes on another thread");
    WriteReport report;
    bool pending = true;
    for (unsigned n = 0; n < 1000; ++n) pending &= PollWriteContainer(first, report) == E_WRITE_PENDING;
    Check(pending, "game-loop polling never completes or waits for an unfinished write");
    Check(ReadEquals("Atomic", original, mugs), "old save remains readable until the atomic replacement");

    SetCurrentDirectoryA(a.c_str());
    std::vector<u8> shortImage(64, 0x31), pictures(128, 0x42);
    const auto expectedSmall = shortImage, expectedPictures = pictures;
    char name[] = "Snapshot", title[] = "snapshot title", description[] = "snapshot description";
    const auto second = BeginWriteContainer(name, shortImage.data(), 64, pictures.data(), 128, title, description);
    std::fill(image.begin(), image.end(), 0xee);
    std::fill(shortImage.begin(), shortImage.end(), 0xff); std::fill(pictures.begin(), pictures.end(), 0xff);
    name[0] = 'X'; title[0] = 'X'; description[0] = 'X';
    SetCurrentDirectoryA(b.c_str());
    const auto third = BeginWriteContainer("Snapshot", shortImage.data(), 64, nullptr, 128, "other", "other");
    Check(second && third && second != third && PollWriteContainer(second, report) == E_WRITE_PENDING,
          "later writes remain queued with distinct identities");
    SetEvent(releaseIO);
    Check(Await(first, report) == E_WRITE_SUCCEEDED && report.muImageSize == 262144
          && report.muMugshotsSize == 960800, "completed write reports its snapshot sizes");
    Check(PollWriteContainer(first, report) == E_WRITE_UNKNOWN, "completion is consumed exactly once");
    Check(Await(second, report) == E_WRITE_SUCCEEDED && std::string(report.macName) == "Snapshot",
          "queued save owns its name and completion metadata");
    Check(Await(third, report) == E_WRITE_SUCCEEDED && report.muMugshotsSize == 0,
          "null mugshot data preserves optional-payload behavior");
    Check(ReadEquals("Snapshot", shortImage, {}), "second directory receives its own queued snapshot");
    SetCurrentDirectoryA(a.c_str());
    Check(ReadEquals("Snapshot", expectedSmall, expectedPictures),
          "queued data and destination survive caller mutation and working-directory changes");
    SetCurrentDirectoryA(root.c_str());
    Check(ReadEquals("Atomic", capturedImage, mugs), "writer uses captured profile bytes after caller mutation");

    failFlush = true;
    const auto failed = BeginWriteContainer("Atomic", image.data(), static_cast<u32>(image.size()),
        mugs.data(), static_cast<u32>(mugs.size()), "title", "description");
    Check(Await(failed, report) == E_WRITE_FAILED, "flush failure is reported as failure");
    Check(ReadEquals("Atomic", capturedImage, mugs), "flush failure preserves the previous good save");
    HANDLE held = CreateFileA("Memcard\\Atomic.sav", GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    const auto renameFailure = BeginWriteContainer("Atomic", image.data(), static_cast<u32>(image.size()),
        mugs.data(), static_cast<u32>(mugs.size()), "title", "description");
    Check(held != INVALID_HANDLE_VALUE && Await(renameFailure, report) == E_WRITE_FAILED,
          "replacement failure is reported as failure");
    if (held != INVALID_HANDLE_VALUE) CloseHandle(held);
    Check(ReadEquals("Atomic", capturedImage, mugs), "replacement failure preserves the previous good save");
    Check(BeginWriteContainer("", image.data(), 4, nullptr, 0, nullptr, nullptr) == 0
          && BeginWriteContainer("Invalid", nullptr, 4, nullptr, 0, nullptr, nullptr) == 0,
          "invalid submissions never create a task");

    Block();
    std::vector<WriteTicket> queued;
    queued.push_back(BeginWriteContainer("Queue", shortImage.data(), 64, nullptr, 0, nullptr, nullptr));
    Check(WaitForSingleObject(entered, 2000) == WAIT_OBJECT_0, "queue capacity test holds the file writer");
    for (unsigned n = 1; n < 32; ++n)
        queued.push_back(BeginWriteContainer("Queue", shortImage.data(), 64, nullptr, 0, nullptr, nullptr));
    const auto overflow = BeginWriteContainer("Queue", shortImage.data(), 64, nullptr, 0, nullptr, nullptr);
    Check(!overflow && std::all_of(queued.begin(), queued.end(), [](auto ticket) { return ticket != 0; }),
          "backlog is bounded and rejects excess work without blocking");
    if (overflow) queued.push_back(overflow);
    SetEvent(releaseIO);
    bool allSucceeded = true;
    for (auto ticket : queued) allSucceeded &= Await(ticket, report) == E_WRITE_SUCCEEDED;
    Check(allSucceeded, "every accepted queued write completes");

    Block();
    const auto last = BeginWriteContainer("Shutdown", shortImage.data(), 64, nullptr, 0, nullptr, nullptr);
    Check(WaitForSingleObject(entered, 2000) == WAIT_OBJECT_0, "shutdown starts with active IO");
    HANDLE stopped = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    std::thread stop([&] { GetAsyncWriter().Stop(); SetEvent(stopped); });
    Check(WaitForSingleObject(stopped, 100) == WAIT_TIMEOUT, "teardown waits for accepted data to finish");
    SetEvent(releaseIO); stop.join();
    Check(Await(last, report) == E_WRITE_SUCCEEDED && ReadEquals("Shutdown", shortImage, {}),
          "shutdown drains the accepted write to a valid container");
    Check(BeginWriteContainer("Late", shortImage.data(), 64, nullptr, 0, nullptr, nullptr) == 0,
          "shutdown rejects new tasks");
    CloseHandle(stopped); CloseHandle(returned); CloseHandle(entered); CloseHandle(releaseIO);
    std::printf("PCAsyncSave: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
