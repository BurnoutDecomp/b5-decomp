// Native EAThread/barrier and Win32-window exercise of the production frame owner.
#include <Windows.h>
#undef DrawText
#include <atomic>
#include <cstdio>
#include <vector>
#include <stdexcept>
#include <thread>
#include <string>
#include "GameShared/GameClasses/System/Threads/CgsThreadLayout.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Development/AssertSystem/CgsAssertManager.h"
#include "GameShared/GameClasses/System/FileSystem/CgsDeviceAsyncOp.h"

namespace CgsFileSystem {
#include "file_wait.inc"
}

static int checks, failures;
static std::vector<int> monitorCalls;
struct AssertRecord { std::string message; DWORD thread; bool display; bool foreignStack; };
static std::vector<AssertRecord> assertRecords;
static DWORD ownerThread;
static HWND assertWindow;
static std::thread assertForeignWorker;
static bool assertWindowMessageSucceeded;
static std::atomic<bool> dispatchDrawing{false};
static std::atomic<unsigned> assertOwnershipFailures{0};
__declspec(noinline) static void ForeignAssertBody();
static bool ContainsForeignFrame(const CgsDev::StackUnpick& stack) {
    DWORD64 base = 0;
    const auto* entry = RtlLookupFunctionEntry(reinterpret_cast<DWORD64>(&ForeignAssertBody), &base, nullptr);
    if (!entry) return false;
    for (int i=0;i<stack.miNumStackAddresses;++i)
        if (stack.maStack[i] >= base+entry->BeginAddress && stack.maStack[i] < base+entry->EndAddress) return true;
    return false;
}
static void Check(bool good, const char* label) {
    ++checks;
    if (!good) { ++failures; std::printf("FAIL %s\n",label); }
}
namespace CgsDev {
namespace Log { DebugPrint* gpDebugPrint = nullptr; void WriteToLog(const char*) {} }
namespace Message { u64 gxMessageFilterFlags = 0; }
namespace PerfMonCpu {
void StartMonitor(s32 id) { monitorCalls.push_back(id); }
void StopMonitor(s32 id) { monitorCalls.push_back(-id); }
}
namespace Assert {
// Only the UI sink is isolated here. Begin/Fire/End, queueing, stack capture,
// native assert mutex and the actual frame coordinator are production code.
Manager gAssertManager;
Manager::Manager() {}
void Manager::HandleAssert(const char* text,const char* file,s32 line) {
    StackUnpick stack; stack.Prepare();
    HandleAssertCapturedPC(text,file,line,stack,true);
}
void Manager::HandleAssertCapturedPC(const char* text,const char*,s32,const StackUnpick& stack,bool display) {
    const DWORD thread=GetCurrentThreadId();
    if (display && thread==ownerThread && dispatchDrawing) ++assertOwnershipFailures;
    assertRecords.push_back({text,thread,display,ContainsForeignFrame(stack)});
    if (std::string(text)=="dispatch assertion") Sleep(20); // main enters while the assert mutex is held
    if (std::string(text)=="dispatch assertion with window dependency") {
        std::atomic<bool> started{false};
        assertForeignWorker=std::thread([&] { started=true; ForeignAssertBody(); });
        while (!started.load()) Sleep(1);
        Sleep(20); // let the foreign request wake the owner while this mutex is held
        DWORD_PTR reply=0;
        assertWindowMessageSucceeded=SendMessageTimeoutW(assertWindow,WM_APP+1,25,0,
            SMTO_BLOCK|SMTO_ABORTIFHUNG,2000,&reply)!=0 && reply==42;
    }
}
}
}
__declspec(noinline) static void ForeignAssertBody() {
    CgsDev::Assert::BeginAssert();
    CgsDev::Assert::FireAssert("foreign assertion",__FILE__,__LINE__);
    CgsDev::Assert::EndAssert();
}
struct FrameCallbacks : CgsSystem::IThreadClass {
    struct Packet { unsigned frame = 0; unsigned words[4096] = {}; } packets[2];
    CgsSystem::ThreadLayout* layout = nullptr;
    HWND window = nullptr;
    DWORD mainThread = GetCurrentThreadId(), dispatchThread = 0;
    HANDLE updateEntered = CreateEventW(nullptr,FALSE,FALSE,nullptr);
    HANDLE dispatchEntered = CreateEventW(nullptr,FALSE,FALSE,nullptr);
    std::atomic<bool> updateDone{false}, dispatchDone{false};
    std::atomic<unsigned> workerFailures{0}, receivedMessages{0};
    unsigned frame = 0, writeIndex = 0, completedPhase = 0, asserts = 0;
    ~FrameCallbacks() { CloseHandle(updateEntered); CloseHandle(dispatchEntered); }
    void OnStartOfUpdateFrame() override {
        Check(GetCurrentThreadId() == mainThread,"update callback stays on its owner");
        completedPhase = 0; updateDone = false; dispatchDone = false;
    }
    bool UpdateThread() override {
        SetEvent(updateEntered);
        // This cannot succeed if DispatchThread is invoked only after UpdateThread.
        Check(WaitForSingleObject(dispatchEntered,2000) == WAIT_OBJECT_0,
              "dispatch begins while update is still executing");
        Packet& p = packets[writeIndex]; p.frame = frame;
        for (unsigned i=0;i<4096;++i) p.words[i] = frame * 8191u + i;
        if (frame == 3)
            Check(layout->InteruptThreadForAssert(nullptr),"owner assert joins dispatch once");
        if (frame == 8) PostQuitMessage(37);
        updateDone = true;
        return frame != 8;
    }
    void DispatchThread() override {
        dispatchThread = GetCurrentThreadId();
        if (dispatchThread == mainThread) ++workerFailures;
        if (WaitForSingleObject(updateEntered,2000) != WAIT_OBJECT_0) ++workerFailures;
        SetEvent(dispatchEntered);
        const Packet& p = packets[writeIndex ^ 1u];
        if (p.frame != frame - 1u) ++workerFailures;
        if (p.frame) for (unsigned i=0;i<4096;++i)
            if (p.words[i] != p.frame * 8191u + i) { ++workerFailures; break; }
        layout->WaitForUpdateCompletionPC();
        if (frame != 3 && !updateDone) ++workerFailures;
        // A synchronous call to the main window would deadlock a raw barrier
        // wait. The timeout bounds a regression instead of hanging the test.
        DWORD_PTR result = 0;
        if (!SendMessageTimeoutW(window,WM_APP+1,frame,0,SMTO_ABORTIFHUNG,2000,&result)
            || result != frame + 17u) ++workerFailures;
        dispatchDone = true;
    }
    void OnCompletionOfVsyncWait() override {
        Check(updateDone && dispatchDone,"both frame halves finish before timing update");
        completedPhase = 1;
    }
    void ResourceUpdateThread(Mutex* mutex) override {
        Check(GetCurrentThreadId() == mainThread && !mutex && completedPhase == 1,
              "resource update runs after join on the update owner");
        completedPhase = 2;
    }
    void OnEndOfUpdateFrame() override {
        Check(completedPhase == 2,"publication follows resource update");
        writeIndex ^= 1u;
        completedPhase = 3;
    }
    void RenderAssert(const AssertData*) override {
        Check(GetCurrentThreadId() == mainThread && dispatchDone,
              "assert renderer owns a joined frame");
        ++asserts;
    }
};
static LRESULT CALLBACK WindowProc(HWND window,UINT message,WPARAM w,LPARAM l) {
    if (message == WM_NCCREATE) {
        auto create = reinterpret_cast<CREATESTRUCTW*>(l);
        SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    if (message == WM_APP+1) {
        auto state = reinterpret_cast<FrameCallbacks*>(GetWindowLongPtrW(window,GWLP_USERDATA));
        if (GetCurrentThreadId() != state->mainThread) ++state->workerFailures;
        ++state->receivedMessages;
        return w + 17;
    }
    return DefWindowProcW(window,message,w,l);
}
struct SerialCallbacks : CgsSystem::IThreadClass {
    std::vector<int> order;
    void OnStartOfUpdateFrame() override { order.push_back(1); }
    bool UpdateThread() override { order.push_back(2); return false; }
    void DispatchThread() override { order.push_back(3); }
    void OnCompletionOfVsyncWait() override { order.push_back(4); }
    void ResourceUpdateThread(Mutex*) override { order.push_back(5); }
    void OnEndOfUpdateFrame() override { order.push_back(6); }
    void RenderAssert(const AssertData*) override {}
};
struct ThrowingCallbacks : SerialCallbacks {
    CgsSystem::ThreadLayout* layout = nullptr;
    bool throwOnDispatch = false;
    std::atomic<bool> dispatchLeft{false};
    bool UpdateThread() override {
        if (!throwOnDispatch) throw std::runtime_error("update failure");
        return true;
    }
    void DispatchThread() override {
        layout->WaitForUpdateCompletionPC();
        dispatchLeft = true;
        if (throwOnDispatch) throw std::runtime_error("dispatch failure");
    }
};
struct AssertingCallbacks : CgsSystem::IThreadClass {
    CgsSystem::ThreadLayout* layout = nullptr;
    HANDLE entered = CreateEventW(nullptr,FALSE,FALSE,nullptr);
    bool mainFails = false, dispatchFails = false;
    ~AssertingCallbacks() { CloseHandle(entered); }
    bool UpdateThread() override {
        Check(WaitForSingleObject(entered,2000)==WAIT_OBJECT_0,"assert frame starts dispatch");
        if (mainFails) CGS_ASSERT(false,"main assertion");
        return true;
    }
    void DispatchThread() override {
        dispatchDrawing = true;
        SetEvent(entered);
        if (dispatchFails) {
            // A main assertion must release the update tail before joining.
            // Taking gAssertMutex first now deterministically deadlocks this
            // simultaneous case, rather than depending on which thread wins.
            layout->WaitForUpdateCompletionPC();
            CGS_ASSERT(false,"dispatch assertion");
        }
        dispatchDrawing = false;
    }
    void ResourceUpdateThread(Mutex*) override {}
    void OnStartOfUpdateFrame() override {}
    void OnEndOfUpdateFrame() override {}
    void OnCompletionOfVsyncWait() override {}
    void RenderAssert(const AssertData*) override {}
};
int main() {
    ownerThread=GetCurrentThreadId();
    CgsDev::Assert::Construct();
    std::atomic<bool> finished{false};
    std::thread watchdog([&] {
        for (unsigned i=0;i<400 && !finished;++i) Sleep(50);
        if (!finished) { std::puts("FAIL native assertion/frame deadlock"); std::fflush(stdout); ExitProcess(3); }
    });
    WNDCLASSW cls = {}; cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpfnWndProc = WindowProc; cls.lpszClassName = L"BurnoutThreadLayoutRegression";
    RegisterClassW(&cls);
    FrameCallbacks callbacks;
    callbacks.window = CreateWindowW(cls.lpszClassName,L"Frame handshake",WS_OVERLAPPEDWINDOW,
        0,0,100,100,nullptr,nullptr,cls.hInstance,&callbacks);
    Check(callbacks.window != nullptr,"native owner window created");
    CgsSystem::ThreadLayout layout;
    callbacks.layout = &layout;
    layout.Begin(&callbacks,11,12,13);
    for (unsigned frame=1;frame<=8;++frame) {
        callbacks.frame = frame;
        Check(layout.Update() == (frame != 8),"update result is forwarded after publication");
    }
    layout.EndPC(); layout.EndPC();
    Check(callbacks.workerFailures == 0,"frozen packets and synchronous window callbacks stay valid");
    Check(callbacks.receivedMessages == 8,"all dispatch window messages were serviced");
    Check(callbacks.asserts == 1,"assert continuation neither skips nor double-joins a frame");
    const int expected[] = {12,13,-13,-12,11,-11};
    bool monitorOrder = monitorCalls.size() == 8 * 6;
    for (unsigned i=0;i<monitorCalls.size();++i)
        monitorOrder &= monitorCalls[i] == expected[i % 6];
    Check(monitorOrder,"sync/resource monitor order matches ARTIST");
    MSG message = {};
    Check(PeekMessageW(&message,nullptr,WM_QUIT,WM_QUIT,PM_REMOVE) && message.wParam == 37,
          "message-aware wait preserves WM_QUIT and its exit code");
    assertWindow=callbacks.window;
    SerialCallbacks serial;
    layout.SetParallelEnabledPC(false);
    layout.Begin(&serial,-1,-1,-1);
    Check(!layout.Update(),"serial control preserves the update result");
    Check(serial.order == std::vector<int>({1,2,3,4,5,6}),
          "serial control uses the same publication and resource ordering");
    layout.EndPC();
    for (bool fromDispatch : {false, true}) {
        ThrowingCallbacks throwing;
        throwing.layout = &layout;
        throwing.throwOnDispatch = fromDispatch;
        layout.SetParallelEnabledPC(true);
        layout.Begin(&throwing,-1,-1,-1);
        bool caught = false;
        try { layout.Update(); }
        catch (const std::runtime_error& e) {
            caught = std::string(e.what()) == (fromDispatch ? "dispatch failure" : "update failure");
        }
        Check(caught,"callback exceptions reach the frame owner");
        Check(throwing.dispatchLeft,"exception unwinding releases and joins the debug tail");
        Check(throwing.order == std::vector<int>({1}),
              "a failed frame does not publish or pump resources");
        layout.EndPC();
    }
    serial.order.clear();
    layout.SetParallelEnabledPC(false);
    layout.Begin(&serial,-1,-1,-1);
    Check(!layout.Update(),"serial restart clears the previous worker exception");
    Check(serial.order == std::vector<int>({1,2,3,4,5,6}),"restart completes all frame phases");
    layout.EndPC();
    for (unsigned mode=1;mode<=3;++mode) {
        AssertingCallbacks asserting;
        asserting.layout=&layout;
        asserting.mainFails=(mode&1)!=0; asserting.dispatchFails=(mode&2)!=0;
        const auto before=assertRecords.size();
        layout.SetParallelEnabledPC(true);
        layout.Begin(&asserting,-1,-1,-1);
        layout.Update();
        layout.EndPC();
        Check(assertRecords.size()==before+(asserting.mainFails?1u:0u)+(asserting.dispatchFails?1u:0u),
              "real assertion entry points report both simultaneous failures");
    }
    Check(assertOwnershipFailures==0,"main assertions paint only after dispatch is joined");
    {
        AssertingCallbacks ordinary;
        layout.SetParallelEnabledPC(true);
        layout.Begin(&ordinary,-1,-1,-1);
        std::atomic<bool> resumed{false};
        std::thread foreign([&] { ForeignAssertBody(); resumed=true; });
        for (unsigned n=0;n<100 && !resumed;++n) { layout.Update(); Sleep(1); }
        Check(resumed,"foreign assertion resumes after the owner consumes it");
        if (!resumed) { std::fflush(stdout); ExitProcess(3); }
        foreign.join();
        Check(assertRecords.back().thread==ownerThread,
              "foreign assertion is consumed on the frame owner");
        Check(assertRecords.back().foreignStack,"handoff preserves the failing worker's stack");
        layout.EndPC();
    }
    {
        AssertingCallbacks closing;
        layout.Begin(&closing,-1,-1,-1);
        std::atomic<bool> started{false}, resumed{false};
        std::thread foreign([&] { started=true; ForeignAssertBody(); resumed=true; });
        while(!started) Sleep(1);
        Sleep(10);
        layout.EndPC();
        foreign.join();
        Check(resumed && !assertRecords.back().display,"shutdown releases pending worker assertions without drawing");
    }
    {
        AssertingCallbacks ordinary;
        layout.Begin(&ordinary,-1,-1,-1);
        CgsFileSystem::AsyncOp operation;
        operation.mbOperating=true;
        std::thread deviceWorker([&] {
            ForeignAssertBody();
            CgsFileSystem::Handle handle; handle.Clear();
            CgsFileSystem::AsyncOp::CompletionCallback(0,handle,123,&operation);
        });
        operation.Wait();
        deviceWorker.join();
        Check(!operation.mbOperating && operation.GetLastOperationSize()==123,
              "synchronous file wait releases an asserting device worker");
        Check(assertRecords.back().thread==ownerThread && !assertRecords.back().display && assertRecords.back().foreignStack,
              "file wait logs the original stack without rendering an incomplete frame");
        layout.EndPC();
    }
    {
        struct WaitsForForeign : AssertingCallbacks {
            void DispatchThread() override {
                SetEvent(entered);
                std::thread worker([] { ForeignAssertBody(); });
                worker.join();
            }
        } waiting;
        layout.Begin(&waiting,-1,-1,-1);
        layout.Update();
        layout.EndPC();
        Check(assertRecords.back().thread==ownerThread && !assertRecords.back().display,
              "dispatch join services an assertion from the worker it depends on");
    }
    for (bool waitInUpdate : {false, true}) {
        struct WindowAsserting : AssertingCallbacks {
            bool waitInUpdate = false;
            EA::Thread::Semaphore completion{0};
            bool UpdateThread() override {
                Check(WaitForSingleObject(entered,2000)==WAIT_OBJECT_0,"modal assertion dispatch starts");
                if (waitInUpdate) CgsDev::Assert::WaitForWorkerPC(completion);
                return true;
            }
            void DispatchThread() override {
                SetEvent(entered);
                CGS_ASSERT(false,"dispatch assertion with window dependency");
                assertForeignWorker.join();
                completion.Post(1);
            }
        } asserting;
        asserting.waitInUpdate=waitInUpdate;
        layout.Begin(&asserting,-1,-1,-1);
        layout.Update();
        layout.EndPC();
        Check(assertWindowMessageSucceeded,
              "queued foreign assertion cannot block modal dispatch's synchronous window call");
        Check(assertRecords.back().thread==ownerThread && !assertRecords.back().display,
              "foreign request is retried after the modal mutex is released");
    }
    DestroyWindow(callbacks.window);
    finished=true;
    watchdog.join();
    std::printf("PCThreadLayout: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
