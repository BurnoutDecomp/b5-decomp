#include "pc/gcm/renderengine/GpuFrameTiming.h"
#include "pc/gcm/renderengine/WindowPresentation.h"
#include <vector>
#include <cstdio>
#include <cmath>

using namespace renderengine::GpuFrameTimingPC;
static renderengine::PCPresentation gPresentation;
namespace renderengine {
    IDirect3DDevice9* gDevice = nullptr;
    struct Device { static bool FrameBeginNoClear(); };
}
#include "pc_gpu_frame_timing.inc"
static unsigned checks, failures;
static std::vector<Result> results;
static void Collect(const Result& result) { results.push_back(result); }
static void Check(bool value, const char* label)
{ ++checks; if (!value) { ++failures; std::printf("FAIL %s\n", label); } }
struct ControlledBackend
{
    struct Query { Point point; };
    unsigned creates = 0, issues = 0, reads = 0, live = 0;
    unsigned failCreate = ~0u, failIssue = ~0u;
    bool ready = false, disjoint = false, failRead = false;
    unsigned long long frequency = 1000000, scene = 2400;
    HRESULT Create(Point point, Query** query)
    {
        if (creates++ == failCreate) return E_OUTOFMEMORY;
        *query = new Query{point}; ++live; return S_OK;
    }
    HRESULT Issue(Query*, DWORD) { return issues++ == failIssue ? E_FAIL : S_OK; }
    HRESULT Read(Query* query, void* data, DWORD size)
    {
        ++reads;
        if (failRead) return E_FAIL;
        if (!ready) return S_FALSE;
        if (query->point == Disjoint) {
            if (size != sizeof(BOOL)) return E_INVALIDARG;
            *static_cast<BOOL*>(data) = disjoint; return S_OK;
        }
        if (size != sizeof(unsigned long long)) return E_INVALIDARG;
        *static_cast<unsigned long long*>(data) = query->point == BeginStamp ? 400
            : query->point == SceneStamp ? scene : query->point == OutputStamp ? 2900 : frequency;
        return S_OK;
    }
    void Release(Query*& query) { if (query) { delete query; --live; } query = nullptr; }
};
template<class Backend> static void Frame(Recorder<Backend>& recorder, unsigned index)
{ recorder.Begin(index); recorder.SceneComplete(); recorder.OutputComplete(); }
static void ControlledChecks()
{
    ControlledBackend backend;
    Recorder<ControlledBackend> recorder(backend, Collect);
    recorder.SceneComplete(); recorder.OutputComplete(); recorder.Poll();
    Check(!backend.creates && !backend.issues && !backend.reads, "idle diagnostic performs no GPU calls");
    for (unsigned i = 0; i < 8; ++i) Frame(recorder, 100+i);
    const unsigned issued = backend.issues, read = backend.reads;
    Frame(recorder, 108);
    Check(results.back().status == RingFull && backend.issues == issued && backend.reads-read <= 8,
        "a full pending ring drops attribution without waiting or overwriting an earlier query");
    backend.ready = true; recorder.Poll();
    bool correct = true; unsigned valid = 0;
    for (const Result& result : results) if (result.status == Valid) {
        ++valid; correct &= result.frame >=100 && result.frame<108 && result.sceneMs == 2 && result.outputMs == .5;
    }
    Check(valid == 8 && correct, "delayed timestamps retain their original frame and 64-bit frequency units");
    const unsigned reuseCreates = backend.creates;
    Frame(recorder, 109); const unsigned once = backend.issues;
    recorder.SceneComplete(); recorder.OutputComplete(); recorder.Poll();
    Check(backend.creates == reuseCreates && backend.issues == once && results.back().status == Valid,
        "completed queries are reusable and duplicate end hooks are harmless");
    backend.disjoint = true; Frame(recorder, 110); recorder.Poll();
    Check(results.back().status == Invalid && results.back().sceneMs == -1,
        "disjoint samples never become plausible GPU timings");
    backend.disjoint = false; backend.frequency = 0; Frame(recorder, 111); recorder.Poll();
    Check(results.back().status == Invalid, "a zero frequency cannot produce a valid measurement");
    backend.frequency = 1000000; backend.scene = 100; Frame(recorder, 112); recorder.Poll();
    Check(results.back().status == Invalid, "backward timestamps are rejected before unsigned subtraction");
    backend.scene = 2400; backend.failRead = true; Frame(recorder, 113); recorder.Poll();
    const bool invalidRead = results.back().status == Invalid;
    const unsigned beforeUnavailable = backend.issues;
    recorder.Begin(114);
    Check(invalidRead && results.back().status == Unavailable && backend.issues == beforeUnavailable,
        "a query error disables reissue until reset");
    recorder.Reset();
    Check(backend.live == 0, "reset retires every native query");
    backend.failRead = false; backend.ready = false; Frame(recorder, 115); recorder.Reset();
    Check(results.back().frame == 115 && results.back().status == Invalid && !backend.live,
        "shutdown preserves unfinished-sample identity and does not wait");
    backend.failCreate = backend.creates+3; recorder.Begin(116);
    Check(results.back().status == Unavailable && !backend.live, "partial query creation is unwound");
    recorder.Reset(); backend.failCreate = ~0u; backend.failIssue = backend.issues+1;
    recorder.Begin(117); const unsigned failedIssueCount = backend.issues; recorder.Begin(118);
    Check(results.back().status == Unavailable && backend.issues == failedIssueCount,
        "an incomplete issue sequence is not silently reused");
    recorder.Reset();
}
static bool NativeChecks()
{
    HWND window = CreateWindowA("STATIC", "GPU timestamp checks", WS_OVERLAPPEDWINDOW,
        0,0,96,64,nullptr,nullptr,GetModuleHandleA(nullptr),nullptr);
    IDirect3D9* api = Direct3DCreate9(D3D_SDK_VERSION);
    IDirect3DDevice9* device = nullptr;
    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_COPY; pp.hDeviceWindow = window;
    pp.BackBufferWidth = 96; pp.BackBufferHeight = 64; pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    if (!api || FAILED(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,&device)))
        return false;
    results.clear();
    NativeBackend backend; backend.device = device;
    {
        Recorder<NativeBackend> recorder(backend, Collect);
        device->SetRenderState(D3DRS_ZENABLE, FALSE);
        for (unsigned i=0; i<40; ++i) {
            recorder.Begin(i);
            device->BeginScene();
            device->Clear(0,nullptr,D3DCLEAR_TARGET,0xFF123456,1,0);
            device->EndScene();
            recorder.SceneComplete();
            recorder.OutputComplete();
            device->Present(nullptr,nullptr,nullptr,nullptr);
        }
        recorder.Poll();
        unsigned valid = 0; bool finite = true;
        for (const Result& result : results) if (result.status == Valid) {
            ++valid; finite &= std::isfinite(result.sceneMs) && result.sceneMs>=0
                && std::isfinite(result.outputMs) && result.outputMs>=0;
        }
        Check(valid > 0 && finite, "real D3D9 timestamp/disjoint/frequency queries complete through normal presents");
        DWORD depth=1; device->GetRenderState(D3DRS_ZENABLE,&depth);
        IDirect3DSurface9 *back=nullptr,*copy=nullptr;
        device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back);
        bool pixels = SUCCEEDED(device->CreateOffscreenPlainSurface(96,64,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&copy,nullptr))
            && SUCCEEDED(device->GetRenderTargetData(back,copy));
        D3DLOCKED_RECT lock{};
        if (pixels && SUCCEEDED(copy->LockRect(&lock,nullptr,D3DLOCK_READONLY))) {
            for(unsigned y=0; y<64; ++y) for(unsigned x=0; x<96; ++x)
                pixels &= (reinterpret_cast<unsigned*>(static_cast<char*>(lock.pBits)+y*lock.Pitch)[x]&0xFFFFFF)==0x123456;
            copy->UnlockRect();
        } else pixels=false;
        Check(pixels && depth == FALSE, "query attribution preserves rendered pixels and depth state");
        if(copy)copy->Release(); if(back)back->Release();
    }
    // Actual production FrameBeginNoClear body: assert takeover first presents
    // the interrupted scene, then opens its overlay while the same CPU frame is
    // still active. That prefix must never be reported as the full game frame.
    using namespace renderengine;
    std::vector<FrameProfile::Frame> frames(41);
    FrameProfile::gCapture.mpFrames = frames.data();
    FrameProfile::gCapture.mpCurrent = &frames[0];
    FrameProfile::gCapture.muCount = 0;
    FrameProfile::gCapture.mbGpuTiming = true;
    gDevice = device;
    GpuFrameTimingPC::Begin(device);
    device->BeginScene(); device->EndScene();
    GpuFrameTimingPC::SceneComplete(); GpuFrameTimingPC::OutputComplete();
    device->Present(nullptr,nullptr,nullptr,nullptr);
    const bool opened = Device::FrameBeginNoClear();
    if (opened) { device->EndScene(); device->Present(nullptr,nullptr,nullptr,nullptr); }
    GpuFrameTimingPC::gRecorder.Poll();
    Check(opened && frames[0].muGpuStatus == Invalid && frames[0].mfGpuSceneMs == -1,
        "real assert-overlay hook invalidates the prefix that was already presented");
    for (unsigned i=1; i<frames.size(); ++i) {
        FrameProfile::gCapture.mpCurrent = &frames[i]; FrameProfile::gCapture.muCount = i;
        GpuFrameTimingPC::Begin(device);
        device->BeginScene(); device->Clear(0,nullptr,D3DCLEAR_TARGET,0xFF123456,1,0); device->EndScene();
        GpuFrameTimingPC::SceneComplete(); GpuFrameTimingPC::OutputComplete();
        device->Present(nullptr,nullptr,nullptr,nullptr);
    }
    GpuFrameTimingPC::gRecorder.Poll();
    unsigned resumedValid = 0;
    for (unsigned i=1; i<frames.size(); ++i) resumedValid += frames[i].muGpuStatus == Valid;
    Check(frames[0].muGpuStatus == Invalid && resumedValid > 0,
        "resumed game frames get fresh samples without resurrecting the abandoned frame");
    GpuFrameTimingPC::Finish();
    FrameProfile::gCapture.mpFrames = FrameProfile::gCapture.mpCurrent = nullptr;
    FrameProfile::gCapture.mpFinishGpu = nullptr;
    FrameProfile::gCapture.mbGpuTiming = false;
    gDevice = nullptr;
    device->Release(); api->Release(); DestroyWindow(window);
    return true;
}
int main()
{
    ControlledChecks();
    if (!NativeChecks()) { std::puts("Native D3D9 device unavailable"); return 2; }
    std::printf("PCGpuFrameTiming: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
