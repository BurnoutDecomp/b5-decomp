// Real EAJobs, Win32 messages and assertion handoff; only the assert UI/log sink
// is isolated. A missing owner callback deterministically hits the watchdog.
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include "GameShared/GameClasses/Development/AssertSystem/CgsAssertManager.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "SDKs/EATech/eajobs/job.h"
#include "SDKs/EATech/eajobs/job_scheduler.h"
#include "SDKs/EATech/eajobs/job_thread_parameters.h"
#include "SDKs/EATech/eajobs/jobs.h"
#include "pc_mesh_job_owner_wait.inc"

static unsigned checks, failures, assertions;
static DWORD ownerThread, reportedThread;
static bool displayed, capturedStack;
static void Check(bool ok, const char* name)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL %s\n", name); }
}
namespace CgsDev {
namespace Log { DebugPrint* gpDebugPrint = nullptr; void WriteToLog(const char*) {} }
namespace Message { u64 gxMessageFilterFlags = 0; }
namespace Assert {
Manager gAssertManager;
Manager::Manager() {}
void Manager::HandleAssert(const char* text, const char* file, s32 line)
{
    StackUnpick stack; stack.Prepare();
    HandleAssertCapturedPC(text, file, line, stack, true);
}
void Manager::HandleAssertCapturedPC(const char*, const char*, s32, const StackUnpick& stack, bool display)
{
    ++assertions;
    reportedThread = GetCurrentThreadId();
    displayed = display;
    capturedStack = stack.miNumStackAddresses > 0;
}
}
}
struct Allocator : EA::Allocator::ICoreAllocator {
    void* Alloc(size_t n, const char*, unsigned, unsigned alignment, unsigned offset) override
    { if (offset) std::abort(); return _aligned_malloc(n, (std::max)(alignment, 16u)); }
    void* Alloc(size_t n, const char* name, unsigned flags) override { return Alloc(n, name, flags, 16, 0); }
    void Free(void* p, size_t) override { _aligned_free(p); }
};
struct JobData {
    HWND window;
    std::atomic<bool> completed{false}, messageOk{false};
    DWORD thread = 0;
};
static LRESULT CALLBACK WindowProc(HWND h, UINT message, WPARAM w, LPARAM l)
{
    if (message == WM_APP + 1) return w + 17;
    return DefWindowProcW(h, message, w, l);
}
static void Worker(EA::Jobs::Param, EA::Jobs::Param data, EA::Jobs::Param, EA::Jobs::Param)
{
    auto& job = *static_cast<JobData*>(data.mpValue);
    job.thread = GetCurrentThreadId();
    PostThreadMessageW(ownerThread, WM_QUIT, 37, 0);
    DWORD_PTR reply = 0;
    job.messageOk = SendMessageTimeoutW(job.window, WM_APP + 1, 25, 0,
        SMTO_BLOCK | SMTO_ABORTIFHUNG, 2000, &reply) != 0 && reply == 42;
    CGS_ASSERT(false, "mesh conversion worker failure");
    job.completed = true;
}
int main()
{
    std::atomic<bool> finished{false};
    std::thread watchdog([&] {
        for (unsigned i = 0; i < 400 && !finished; ++i) Sleep(10);
        if (!finished) { std::puts("FAIL owner job wait deadlock"); std::fflush(stdout); ExitProcess(3); }
    });
    ownerThread = GetCurrentThreadId();
    CgsDev::Assert::Construct();
    CgsDev::Assert::AttachFrameOwnerPC(&finished, nullptr);
    WNDCLASSW wc{}; wc.lpfnWndProc = WindowProc; wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"MeshJobOwnerWaitFixture";
    RegisterClassW(&wc);
    HWND window = CreateWindowW(wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE,
        nullptr, wc.hInstance, nullptr);
    Check(window != nullptr, "native message-only window belongs to frame owner");
    Allocator allocator; EA::Jobs::SetAllocator(&allocator);
    EA::Jobs::JobScheduler scheduler; scheduler.Initialize(32, 32);
    EA::Jobs::JobThreadParameters parameters; scheduler.AddThread(parameters);
    JobData data; data.window = window;
    {
        EA::Jobs::Job job("mesh-owner-wait");
        job.SetCode(EA::Jobs::JOB_ENVIRONMENT_LOCAL, reinterpret_cast<const void*>(&Worker), 0);
        job.SetData(&data, sizeof(data));
        scheduler.AddJobs(&job, 1);
        renderengine::MeshJobOwnerWaitPC wait;
        job.WaitOn(&renderengine::MeshJobOwnerWaitPC::Poll, &wait);
        Check(job.IsDone() && data.completed && data.thread != ownerThread,
              "real native worker resumes from its queued assertion and completes");
    }
    Check(data.messageOk, "job wait services synchronous window messages");
    Check(assertions == 1 && reportedThread == ownerThread && !displayed && capturedStack,
          "actual assertion handoff reports on owner without drawing an incomplete frame");
    MSG message{};
    Check(PeekMessageW(&message, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE) && message.wParam == 37,
          "job join preserves quit notification and exit code");
    scheduler.Destroy();
    CgsDev::Assert::DetachFrameOwnerPC(&finished);
    DestroyWindow(window); UnregisterClassW(wc.lpszClassName, wc.hInstance);
    finished = true; watchdog.join();
    std::printf("PCMeshJobOwnerWait: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
