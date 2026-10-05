#define NOMINMAX
#include <Windows.h>
#include <atomic>
#include <cstdio>
#include <thread>
#include <unordered_map>
#include <vector>

static HANDLE entered, releaseIO;
static std::atomic<bool> blockNext{false}, failNextFlush{false};
static BOOL WINAPI TestWriteFile(HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED);
static BOOL WINAPI TestFlushFileBuffers(HANDLE);
#define WriteFile TestWriteFile
#define FlushFileBuffers TestFlushFileBuffers
#include "async_save_backend.inc"
#undef WriteFile
#undef FlushFileBuffers
static BOOL WINAPI TestWriteFile(HANDLE h, LPCVOID p, DWORD n, LPDWORD done, LPOVERLAPPED o)
{
    if (blockNext.exchange(false)) { SetEvent(entered); WaitForSingleObject(releaseIO, 5000); }
    return WriteFile(h, p, n, done, o);
}
static BOOL WINAPI TestFlushFileBuffers(HANDLE h)
{ if (failNextFlush.exchange(false)) return FALSE; return FlushFileBuffers(h); }

static unsigned checks, failures;
static void Check(bool ok, const char* name)
{ ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n", name); } }
#define CGS_ASSERT(condition, text) do { if (!(condition)) { ++failures; std::printf("ASSERT %s\n", text); } } while (false)
namespace CgsDev { namespace Log {
    struct Sink {
        std::string text;
        Sink& operator<<(const char* value) { text += value; return *this; }
        Sink& operator<<(u32 value) { char out[32]; std::snprintf(out, sizeof(out), "%u", value); return *this << out; }
        Sink& operator<<(f32 value) { char out[48]; std::snprintf(out, sizeof(out), "%f", value); return *this << out; }
    } sink;
    Sink* gpDebugPrint = &sink;
} }
static DWORD overlappedResult = 997;
static bool waitedForOverlapped = false;
extern "C" DWORD XGetOverlappedResult(void*, DWORD*, BOOL wait)
{ waitedForOverlapped |= wait != FALSE; return overlappedResult; }
namespace RealmcIface {
    struct DataBuffer { void* mpData = nullptr; u32 muSize = 0; };
    struct LoadEntryInfo { LoadEntryInfo() = default; LoadEntryInfo(const char*, const DataBuffer*, const DataBuffer*) {} };
    struct SaveCheckParams { SaveCheckParams(int, void*) {} };
    struct TitleInfo { static TitleInfo& Empty() { static TitleInfo info; return info; } };
}
namespace CgsGui {
    enum ESaveLoadTaskResult { E_SAVELOADTASKRESULT_SUCCESS, E_SAVELOADTASKRESULT_FAILURE };
    struct SaveLoadTaskResultHandler { virtual void HandleSaveLoadTaskResult(ESaveLoadTaskResult) = 0; };
    struct MessageDisplay {};
    struct SaveLoadMetadata { const char* name; void* bytes; int size; };
    struct SaveInfo {};
    struct MemcardFixture {
        unsigned updates = 0, writes = 0;
        void Update(int) { ++updates; }
        void CheckSave(int, const RealmcIface::SaveCheckParams*) {}
        void WriteSave(void*, int count, RealmcIface::LoadEntryInfo*, int, RealmcIface::TitleInfo*) { writes += count == 2; }
    };
    struct SaveLoadSystem {
        MemcardFixture card;
        MemcardFixture* mpMemcardInterface = &card;
        MessageDisplay* mpActiveMessageDisplay = nullptr;
        struct { void* mpData = nullptr; s32 miSize = 0; } mStoredDataView;
        void* mpMugshotBufferData = nullptr;
        s32 miExtraFilesSizeBytes = 0, miNumberOfMugshotsPerType = 0, miNumberOfMugshotTypes = 0;
        u8 mbAsyncOpState = 0, maOverlapped[32]{};
        char macTitle[32]{}, macSaveInfoComment[32]{}, macSaveInfoDescription[256]{};
        void SetMetadata(const void* meta, const void*) { std::strncpy(macTitle, static_cast<const SaveLoadMetadata*>(meta)->name, 31); }
        void CaptureStoredDataView(const SaveLoadMetadata& meta) { mStoredDataView.mpData = meta.bytes; mStoredDataView.miSize = meta.size; }
        s32 GetMugshotBufferSizeBytes() const { return miExtraFilesSizeBytes * miNumberOfMugshotsPerType * miNumberOfMugshotTypes; }
        void Save();
        void Save(SaveLoadTaskResultHandler*, const SaveLoadMetadata&, const SaveInfo&);
        void Update();
    };
}
static void* CreateRealmcSaveInfoPC(void* out, void*) { return out; }
#include "async_save_task.inc"

struct Handler : CgsGui::SaveLoadTaskResultHandler {
    unsigned count = 0;
    DWORD thread = 0;
    CgsGui::ESaveLoadTaskResult result{};
    CgsGui::SaveLoadSystem* chain = nullptr;
    Handler* next = nullptr;
    CgsGui::SaveLoadMetadata nextMetadata{};
    void HandleSaveLoadTaskResult(CgsGui::ESaveLoadTaskResult r) override {
        ++count; thread = GetCurrentThreadId(); result = r;
        if (next) {
            // Force rehash while the callback runs, then emulate the profile
            // manager replaying a buffered save from ReportTaskCompleted.
            sPendingSavesPC.rehash(5000);
            chain->Save(next, nextMetadata, CgsGui::SaveInfo{});
        }
    }
};
static void Pump(CgsGui::SaveLoadSystem& system, Handler& handler, unsigned count)
{
    const auto end = GetTickCount64() + 10000;
    while (handler.count < count && GetTickCount64() < end) { system.Update(); Sleep(1); }
}
static void Block() { ResetEvent(entered); ResetEvent(releaseIO); blockNext = true; }
int main()
{
    using namespace CgsGui;
    entered = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    releaseIO = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    SaveLoadPC::InitializeAsyncWrites();
    SaveLoadSystem system;
    Handler first, second;
    std::vector<u8> image(256, 0x12), nextImage(256, 0x34), output(256);
    SaveLoadMetadata metadata{"Task", image.data(), 256};
    first.chain = &system; first.next = &second;
    first.nextMetadata = {"Chained", nextImage.data(), 256};
    Block(); system.Save(&first, metadata, SaveInfo{});
    Check(WaitForSingleObject(entered, 2000) == WAIT_OBJECT_0, "task reaches blocked native file IO");
    Check(first.count == 0 && sPendingSavesPC[&system].muTicket != 0,
          "save remains pending after submission");
    system.mbAsyncOpState = 1;
    for (unsigned i = 0; i < 1000; ++i) system.Update();
    Check(first.count == 0 && !waitedForOverlapped && system.mbAsyncOpState == 1,
          "update remains nonblocking for both console and native pending operations");
    Check(system.card.updates >= 1000 && system.card.writes == 1,
          "native completion keeps the original memory-card update/submission path");
    std::fill(image.begin(), image.end(), 0xff);
    overlappedResult = 0;
    SetEvent(releaseIO); Pump(system, second, 1);
    Check(first.count == 1 && second.count == 1 && first.thread == GetCurrentThreadId()
          && second.thread == GetCurrentThreadId(), "completion and chained save callbacks run on the game thread exactly once");
    Check(!system.mbAsyncOpState && first.result == E_SAVELOADTASKRESULT_SUCCESS
          && second.result == E_SAVELOADTASKRESULT_SUCCESS, "actual completion reports success");
    Check(SaveLoadPC::ReadContainer("Task", output.data(), 256, nullptr, 0) == SaveLoadPC::E_CONTAINERREAD_OK
          && output == std::vector<u8>(256, 0x12), "task snapshot survives live profile changes");
    Check(SaveLoadPC::ReadContainer("Chained", output.data(), 256, nullptr, 0) == SaveLoadPC::E_CONTAINERREAD_OK
          && output == nextImage, "reentrant buffered save writes its own snapshot");
    for (unsigned i = 0; i < 10; ++i) system.Update();
    Check(first.count == 1 && second.count == 1, "later updates cannot repeat a consumed completion");

    Handler failed; failNextFlush = true;
    system.Save(&failed, metadata, SaveInfo{}); Pump(system, failed, 1);
    Check(failed.count == 1 && failed.result == E_SAVELOADTASKRESULT_FAILURE
          && failed.thread == GetCurrentThreadId(), "IO failure reaches the original result handler on the game thread");
    Handler released; Block(); system.Save(&released, metadata, SaveInfo{});
    Check(WaitForSingleObject(entered, 2000) == WAIT_OBJECT_0, "owner teardown starts with pending write");
    HANDLE finished = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    std::thread release([&] { FinishPendingSavePC(&system); SetEvent(finished); });
    Check(WaitForSingleObject(finished, 100) == WAIT_TIMEOUT, "owner teardown drains its accepted write");
    SetEvent(releaseIO); release.join();
    system.Update();
    Check(released.count == 0 && sPendingSavesPC.find(&system) == sPendingSavesPC.end(),
          "released owner gets no stale callback");
    Handler reused; system.Save(&reused, metadata, SaveInfo{}); Pump(system, reused, 1);
    Check(reused.count == 1 && released.count == 0, "reused owner receives only its new operation's result");
    FinishPendingSavePC(&system);
    CloseHandle(finished); CloseHandle(entered); CloseHandle(releaseIO);
    std::printf("PCAsyncSaveTask: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
