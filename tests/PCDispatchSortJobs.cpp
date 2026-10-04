// Real dispatch storage and native scheduler. Blocking unrelated jobs makes
// missing consumer/reuse joins observable, without altering the sort entries.
#include <Windows.h>
#include <intrin.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "pc/gcm/renderengine/DispatchSortJobsPCLeaf.h"
#include "pc/gcm/renderengine/FrameProfilePCLeaf.h"
#include "pc/gcm/renderengine/MeshJobOwnerWaitPCLeaf.h"
#include "SDKs/EATech/eajobs/job_thread_parameters.h"
#include "SDKs/EATech/eajobs/jobs.h"

static unsigned checks, failures;
static void Check(bool ok, const char* name)
{ ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n", name); } }
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* text, const char*, int) { std::printf("ASSERT %s\n", text); std::abort(); }
void* EndAssert() { return nullptr; }
void ServiceWorkerAssertsWhileWaitingPC() {}
}
struct StrStream {
    StrStream(char* out, unsigned) { out[0] = 0; }
    template<class T> StrStream& operator<<(const T&) { return *this; }
};
}
using namespace CgsGraphics;
using renderengine::DispatchSortJobsPC;
struct Allocator : EA::Allocator::ICoreAllocator {
    void* Alloc(size_t n, const char*, unsigned, unsigned alignment, unsigned offset) override
    { if (offset) std::abort(); return _aligned_malloc(n, (std::max)(alignment, 16u)); }
    void* Alloc(size_t n, const char* name, unsigned flags) override { return Alloc(n, name, flags, 16, 0); }
    void Free(void* p, size_t) override { _aligned_free(p); }
};
static unsigned shutdownStage;
struct BrnRendererModule {
    struct Bank { DispatchSortJobsPC mSortJobs; } maPreparedMeshFramesPC[2];
    void EndMeshFramesPC();
};
namespace BrnGame {
struct BrnGameModule {
    struct Layout {
        void SynchronizeDispatchPC() { shutdownStage = 1; }
        void EndPC() { shutdownStage = 2; }
    } mThreadLayout;
    BrnRendererModule mRenderModule;
    bool mbFrameLayoutInitializedPC = true;
    void EndFramesPC();
};
}
#include "pc_dispatch_sort_jobs.inc"

struct Frame {
    static constexpr size_t bytes = 12 * 1024 * 1024;
    unsigned char* data = static_cast<unsigned char*>(VirtualAlloc(nullptr, bytes + 4096,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    DispatchFrame frame{};
    DispatchList lists[25]{};
    std::array<std::vector<u64>, 25> input, sorted;
    explicit Frame(unsigned seed, bool large = false)
    {
        if (!data) std::abort();
        std::memset(data + bytes, 0xCE, 4096);
        frame.m_paLists = lists; frame.muNumDispatchLists = 25;
        frame.GetBin().SetBinRange(reinterpret_cast<DispatchCommand*>(data),
                                  reinterpret_cast<DispatchCommand*>(data + bytes));
        for (auto& list : lists) { list.mpDispatchBin = &frame.GetBin(); list.m_pBinBase = frame.GetBin().GetBase(); }
        frame.Reset();
        for (unsigned l = 0; l < 25; ++l) {
            unsigned count = l == 0 ? 0 : l == 1 ? 1 : 63 + l * 7;
            if (large && (l == 2 || l == 4 || l == 10)) count = l == 2 ? 65535 : l == 4 ? 65536 : 65537;
            for (unsigned i = 0; i < count; ++i) {
                lists[l].ReserveKey();
                auto& bin = frame.GetBin(); bin.BeginPacket();
                auto* packet = bin.AllocateCommand(0); packet->muWords[0] = 0;
                seed = seed * 1664525u + 1013904223u;
                // Include equal upper keys, top-bit set keys, and packet offsets.
                const u64 key = (u64(seed & 1) << 43) | (seed % 127);
                lists[l].Submit(key, bin.EndPacket());
                input[l].push_back((key << 20) | u64(packet - bin.GetBase()));
            }
            sorted[l] = input[l]; std::sort(sorted[l].begin(), sorted[l].end());
        }
    }
    ~Frame() { VirtualFree(data, 0, MEM_RELEASE); }
    bool Matches(unsigned l) const
    {
        return lists[l].GetCount() == sorted[l].size()
            && (sorted[l].empty() ? lists[l].mpSortedKeys == nullptr
                : lists[l].mpSortedKeys && std::equal(sorted[l].begin(), sorted[l].end(), lists[l].mpSortedKeys));
    }
    void Verify()
    {
        for (unsigned l = 0; l < 25; ++l) {
            bool selected = false;
            // Independent console list set, not the production mapping array.
            for (unsigned id : {0,1,2,3,4,5,6,7,8,9,10,11,15,19,20,21}) selected |= l == id;
            if (selected) Check(Matches(l), "selected list has every unsigned 64-bit record in order");
            else {
                std::vector<u64> untouched;
                for (auto* block = lists[l].mpBlockListHead; block; block = block->mpNext)
                    untouched.insert(untouched.end(), block->mpKeys, block->mpKeys + block->muCount);
                Check(lists[l].mpSortedKeys == nullptr && untouched == input[l], "auxiliary list remains untouched");
            }
        }
        bool intact = true; for (size_t i = 0; i < 4096; ++i) intact &= data[bytes + i] == 0xCE;
        Check(intact, "sort workers never overrun the frame arena");
    }
};

// Occupy every worker, forcing new sort jobs to stay queued until Release.
struct Gate {
    HANDLE release = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    std::atomic<unsigned> entered{0};
    std::array<EA::Jobs::Job, 3> jobs = {EA::Jobs::Job("gate"), EA::Jobs::Job("gate"), EA::Jobs::Job("gate")};
    static void Entry(EA::Jobs::Param, EA::Jobs::Param data, EA::Jobs::Param, EA::Jobs::Param)
    {
        auto& gate = *static_cast<Gate*>(data.mpValue); ++gate.entered;
        if (WaitForSingleObject(gate.release, 8000) != WAIT_OBJECT_0) ExitProcess(4);
    }
    explicit Gate(EA::Jobs::JobScheduler& scheduler)
    {
        for (auto& job : jobs) {
            job.SetCode(EA::Jobs::JOB_ENVIRONMENT_LOCAL, reinterpret_cast<const void*>(&Entry), 0);
            job.SetData(this, sizeof(*this)); scheduler.AddJobs(&job, 1);
        }
        const ULONGLONG deadline = GetTickCount64() + 4000;
        while (entered != 3 && GetTickCount64() < deadline) Sleep(0);
        if (entered != 3) ExitProcess(5);
    }
    void Release() { SetEvent(release); }
    ~Gate() { Release(); for (auto& job : jobs) job.WaitOn(); CloseHandle(release); }
};
static LRESULT CALLBACK WindowProc(HWND h, UINT message, WPARAM w, LPARAM l)
{ return message == WM_APP + 1 ? (shutdownStage == 2 ? 0 : w + 17) : DefWindowProcW(h, message, w, l); }
static std::thread ReleaseThroughOwner(Gate& gate, HWND window, bool& messageOk)
{
    return std::thread([&gate, window, &messageOk] {
        DWORD_PTR reply = 0;
        messageOk = SendMessageTimeoutW(window, WM_APP + 1, 25, 0,
            SMTO_BLOCK | SMTO_ABORTIFHUNG, 2000, &reply) != 0 && reply == 42;
        gate.Release();
    });
}
int main()
{
    std::atomic<bool> finished{false};
    std::thread watchdog([&] {
        for (unsigned i = 0; i < 2000 && !finished; ++i) Sleep(10);
        if (!finished) { std::puts("FAIL sort job deadlock"); std::fflush(stdout); ExitProcess(3); }
    });
    WNDCLASSW wc{}; wc.lpfnWndProc = WindowProc; wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"DispatchSortJobsFixture"; RegisterClassW(&wc);
    HWND window = CreateWindowW(wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    Check(window != nullptr, "native owner message window exists");
    Allocator allocator; EA::Jobs::SetAllocator(&allocator);
    EA::Jobs::JobScheduler scheduler; scheduler.Initialize(128, 128);
    for (unsigned i = 0; i < 3; ++i) { EA::Jobs::JobThreadParameters params; scheduler.AddThread(params); }
    for (bool wide : {false, true}) {
        Frame a(17, true), b(97);
        DispatchSortJobsPC first, second;
        Gate gate(scheduler);
        first.Begin(&a.frame, &scheduler, wide); second.Begin(&b.frame, &scheduler, wide);
        Check(first.Pending() && second.Pending() && !first.maJobs[4].IsDone(),
              "both banks submit work without waiting for busy workers");
        Check(a.lists[4].mpSortedKeys != b.lists[4].mpSortedKeys
              && reinterpret_cast<uintptr_t>(a.lists[4].mpSortedKeys) > UINT32_MAX,
              "banks retain disjoint full-width native key addresses");
        bool messageOk = false;
        auto release = ReleaseThroughOwner(gate, window, messageOk);
        first.WaitList(4);
        Check(a.Matches(4), "fifth shadow-list consumer joins its own sort job");
        first.WaitAll(); release.join();
        Check(messageOk, "sort owner wait services synchronous window messages");
        // Exercise the renderer-side wait from a different thread.
        std::thread render([&] { second.WaitList(19); second.WaitAll(); }); render.join();
        Check(!first.Pending() && !second.Pending(), "both banks join all jobs including disabled passes");
        a.Verify(); b.Verify();
    }
    {
        Frame oldFrame(101), nextFrame(503);
        DispatchSortJobsPC jobs;
        Gate gate(scheduler);
        jobs.Begin(&oldFrame.frame, &scheduler, false);
        bool messageOk = false; auto release = ReleaseThroughOwner(gate, window, messageOk);
        jobs.Begin(&nextFrame.frame, &scheduler, true);
        Check(oldFrame.Matches(20), "descriptor reuse joins old bank before replacing its pointers");
        jobs.WaitAll(); release.join();
        Check(messageOk && nextFrame.Matches(20), "reused descriptors complete the new frame");
    }
    {
        Frame frame(43); Gate gate(scheduler); bool messageOk = false;
        std::thread release;
        {
            DispatchSortJobsPC jobs; jobs.Begin(&frame.frame, &scheduler, true);
            release = ReleaseThroughOwner(gate, window, messageOk);
        }
        release.join();
        Check(messageOk && frame.Matches(20), "destruction waits before releasing in-flight descriptors");
    }
    {
        Frame a(33), b(19);
        auto* game = new BrnGame::BrnGameModule;
        {
            Gate gate(scheduler);
            auto& banks = game->mRenderModule.maPreparedMeshFramesPC;
            banks[0].mSortJobs.Begin(&a.frame, &scheduler, false);
            banks[1].mSortJobs.Begin(&b.frame, &scheduler, true);
            bool messageOk = false; auto release = ReleaseThroughOwner(gate, window, messageOk);
            game->EndFramesPC(); release.join();
            Check(!banks[0].mSortJobs.Pending() && !banks[1].mSortJobs.Pending(),
                  "actual frame shutdown joins both consumed and last published banks");
            Check(shutdownStage == 2 && messageOk && !game->mbFrameLayoutInitializedPC,
                  "shutdown joins sorts after render synchronization and before closing the owner");
            // The negative must report the omitted join without invoking UB.
            banks[0].mSortJobs.WaitAll(); banks[1].mSortJobs.WaitAll();
            Check(a.Matches(20) && b.Matches(20), "final bank sorts complete before backend teardown");
        }
        scheduler.Destroy();
        delete game; // Match the game's backend-before-static-renderer destruction.
    }
    DestroyWindow(window); UnregisterClassW(wc.lpszClassName, wc.hInstance);
    finished = true; watchdog.join();
    std::printf("PCDispatchSortJobs: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
