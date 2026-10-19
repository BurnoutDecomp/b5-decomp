#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "pc/gcm/renderengine/GpuMonitorTiming.h"
#ifdef AddMonitor
#undef AddMonitor
#endif
#include "GameShared/GameClasses/Development/PerfMon/Gpu/CgsPerfMonGpu.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <vector>
#include <string>
#include <cstdio>
#include <cmath>
#include <cstring>

namespace renderengine { IDirect3DDevice9* gDevice = nullptr; }
static unsigned checks, failures, asserts;
namespace CgsDev::Assert
{
    int BeginAssert() { return 0; }
    void* EndAssert() { return nullptr; }
    int FireAssert(const char* text, const char*, int)
    { ++asserts; std::printf("ASSERT %s\n", text); return 0; }
}
namespace CgsDev::Log { StrStreamBase* gpDebugPrint = nullptr; }

using namespace renderengine::GpuMonitorTimingPC;
static std::vector<Result> results;
static void Collect(const Result& result) { results.push_back(result); }
static void Check(bool passed, const char* label)
{ ++checks; if (!passed) { ++failures; std::printf("FAIL %s\n", label); } }
struct ControlledBackend
{
    struct Query { unsigned index, packet; std::uint64_t value = 0; };
    unsigned creates = 0, issues = 0, reads = 0, live = 0;
    unsigned failCreate = ~0u, failIssue = ~0u, pendingIndex = ~0u;
    bool ready[5] = {}, disjoint = false, failRead = false, backward = false;
    std::uint64_t clock = 100, frequency = 1000000;
    HRESULT Create(unsigned index, Query** query)
    {
        unsigned n = creates++;
        if (n == failCreate) return E_OUTOFMEMORY;
        *query = new Query{index, (n / NumQueries) % 5}; ++live; return S_OK;
    }
    HRESULT Issue(Query* query, DWORD flags)
    {
        if (issues++ == failIssue) return E_FAIL;
        if (query->index < FrequencyQuery) query->value = clock++;
        else if (query->index == FrequencyQuery) query->value = frequency;
        return flags == D3DISSUE_BEGIN || flags == D3DISSUE_END ? S_OK : E_INVALIDARG;
    }
    HRESULT Read(Query* query, void* data, DWORD bytes)
    {
        ++reads;
        if (failRead) return E_FAIL;
        if (!ready[query->packet] || query->index == pendingIndex) return S_FALSE;
        if (query->index == DisjointQuery)
        {
            if (bytes != sizeof(BOOL)) return E_INVALIDARG;
            *static_cast<BOOL*>(data) = disjoint; return S_OK;
        }
        if (bytes != sizeof(std::uint64_t)) return E_INVALIDARG;
        *static_cast<std::uint64_t*>(data) = backward && query->index == FrameTimer ? 0 : query->value;
        return S_OK;
    }
    void Release(Query*& query) { if (query) { delete query; --live; } query = nullptr; }
};
static void Frame(Recorder<ControlledBackend>& recorder, unsigned frame)
{ recorder.Begin(frame); recorder.Start(0); recorder.Stop(0); recorder.Start(31); recorder.Stop(31); recorder.End(); }
static void ControlledChecks()
{
    ControlledBackend backend;
    Recorder<ControlledBackend> recorder(backend);
    recorder.SetReport(Collect);
    recorder.Start(0); recorder.Stop(0); recorder.End(); recorder.Poll();
    Check(!backend.creates && !backend.issues && !backend.reads, "idle transport performs no GPU calls");
    for (unsigned i = 0; i < 5; ++i) Frame(recorder, 100 + i);
    const unsigned issued = backend.issues, read = backend.reads;
    Frame(recorder, 105);
    Check(results.back().status == RingFull && backend.issues == issued && backend.reads - read <= 5,
        "five pending packets are never overwritten or waited on");
    backend.ready[0] = true; recorder.Poll();
    const Result first = results.back();
    Check(first.status == Valid && first.frame == 100 && first.frequency == 1000000
        && first.used == 0x80000001 && first.ticks[0] == 1 && first.ticks[31] == 1
        && first.ticks[1] == 0 && first.ticks[FrameTimer] == 5,
        "delayed monitor0/31 spans retain frame identity, 64-bit units and zero unused costs");
    const unsigned creates = backend.creates;
    Frame(recorder, 106); recorder.Poll();
    Check(backend.creates == creates && results.back().status == Valid && results.back().frame == 106,
        "a completed packet is reused without reallocating queries");
    for (bool& ready : backend.ready) ready = true;
    backend.pendingIndex = 0; recorder.Poll();
    bool partial = false;
    for (const Result& result : results) partial |= result.status == Valid && result.frame >= 101 && result.frame <= 104;
    Check(!partial, "S_FALSE on one monitor cannot publish a mixed partial frame");
    backend.pendingIndex = ~0u; recorder.Poll();
    unsigned valid = 0;
    for (const Result& result : results) valid += result.status == Valid;
    Check(valid == 6, "all completed packets are eventually published once, including older out-of-order frames");
    backend.disjoint = true; Frame(recorder, 107); recorder.Poll();
    Check(results.back().status == Invalid, "disjoint GPU clocks reject a sample");
    backend.disjoint = false; backend.frequency = 0; Frame(recorder, 108); recorder.Poll();
    Check(results.back().status == Invalid, "zero GPU frequency never produces percentages");
    backend.frequency = 1000000; backend.backward = true; Frame(recorder, 109); recorder.Poll();
    Check(results.back().status == Invalid, "backward timestamps are rejected");
    backend.backward = false; backend.failRead = true; Frame(recorder, 110); recorder.Poll();
    const bool invalidRead = results.back().status == Invalid;
    const unsigned priorIssues = backend.issues; recorder.Begin(111);
    Check(invalidRead && results.back().status == Unavailable && backend.issues == priorIssues,
        "device/query errors disable reissue until reset");
    recorder.ResetQueries();
    Check(!backend.live && results.back().status == Reset, "reset releases every query and clears the published generation");
    backend.failRead = false; backend.failCreate = backend.creates + 3; recorder.Begin(112);
    Check(results.back().status == Unavailable && !backend.live, "partial query allocation is unwound");
    recorder.ResetQueries(); backend.failCreate = ~0u; backend.failIssue = backend.issues + 1;
    recorder.Begin(113); const unsigned failedIssues = backend.issues; recorder.Begin(114);
    Check(results.back().status == Unavailable && backend.issues == failedIssues,
        "an incomplete Issue sequence cannot be recycled as a new frame");
    recorder.ResetQueries(); backend.failIssue = ~0u;
    recorder.Begin(115); recorder.Start(0); recorder.Start(1); recorder.End();
    Check(results.back().status == Invalid, "overlapping monitors cannot become valid native measurements");
    recorder.ResetQueries(); recorder.Begin(116); recorder.Start(0); recorder.Stop(1);
    Check(results.back().status == Invalid, "a mismatched end rejects the native packet");
    recorder.ResetQueries(); recorder.Begin(117); recorder.Start(0); recorder.AbandonActive(); recorder.Poll();
    bool abandonedValid = false;
    for (const Result& result : results) abandonedValid |= result.frame == 117 && result.status == Valid;
    Check(!abandonedValid, "modal interruption drains its last marker without publishing a complete-frame cost");
    Frame(recorder, 118); recorder.Poll();
    Check(results.back().status == Valid && results.back().frame == 118,
        "normal frames resume after an interrupted query packet");
    recorder.ResetQueries(); recorder.Begin(119); recorder.Start(32);
    Check(results.back().status == Invalid, "the whole-frame timer cannot be used as a numbered monitor");
    recorder.ResetQueries();
}
struct MonitorReport { std::string name; float value; bool over; };
static std::vector<MonitorReport> monitors;
static void CollectMonitor(const CgsDev::PerfMonGpuMonitorData& data, void*)
{ monitors.push_back({data.mpcName, data.mfCurrentValue, data.mbOverBudget}); }
static bool NativeChecks()
{
    HWND window = CreateWindowA("STATIC", "GPU monitor checks", WS_OVERLAPPEDWINDOW,
        0, 0, 96, 64, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    IDirect3D9* api = Direct3DCreate9(D3D_SDK_VERSION);
    IDirect3DDevice9* device = nullptr;
    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_COPY; pp.hDeviceWindow = window;
    pp.BackBufferWidth = 96; pp.BackBufferHeight = 64; pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    if (!api || FAILED(api->CreateDevice(0, D3DDEVTYPE_HAL, window,
        D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &device)))
    { Check(false, "native D3D9 device setup"); if (api) api->Release(); if (window) DestroyWindow(window); return false; }
    using CgsDev::PerfMonGpu;
    renderengine::gDevice = device;
    PerfMonGpu::Construct();
    const rw::RGBA white(255, 255, 255, 255);
    const int first = PerfMonGpu::AddMonitor("Clear", white, 1.0f);
    const int second = PerfMonGpu::AddMonitor("Tail", white, 1.0f);
    Check(first == 0 && second == 1, "real engine registry allocates distinct monitor IDs");
    device->SetRenderState(D3DRS_ZENABLE, FALSE);
    unsigned visible = 0;
    bool reports = true;
    auto DrawFrame = [&]()
    {
        device->BeginScene();
        PerfMonGpu::StartProfiling();
        PerfMonGpu::StartMonitor(first);
        device->Clear(0, nullptr, D3DCLEAR_TARGET, 0xFF123456, 1, 0);
        PerfMonGpu::StopMonitor(first);
        PerfMonGpu::StartMonitor(second);
        for (unsigned j = 0; j < 4; ++j) device->Clear(0, nullptr, D3DCLEAR_TARGET, 0xFF123456, 1, 0);
        PerfMonGpu::StopMonitor(second);
        device->EndScene(); device->Present(nullptr, nullptr, nullptr, nullptr);
        PerfMonGpu::StopProfiling();
        PerfMonGpu::Swap();
        monitors.clear(); PerfMonGpu::ReportMonitors(CollectMonitor, nullptr, true);
        const float sum = PerfMonGpu::GetLastMonitorValue(first) + PerfMonGpu::GetLastMonitorValue(second);
        visible += sum > 0;
        reports &= monitors.size() >= 4 && monitors[0].name == "Clear" && monitors[1].name == "Tail"
            && monitors[2].name == "TOTAL MONITORED" && monitors.back().name == "TOTAL RENDERED"
            && std::fabs(monitors[2].value - sum) < .001f && std::fabs(PerfMonGpu::GetLastTotal() - sum) < .001f
            && monitors.back().value + .001f >= sum && std::isfinite(monitors.back().value);
        // This paces the tiny test workload, not the production query reader.
        Sleep(1);
    };
    for (unsigned i = 0; i < 64; ++i) DrawFrame();
    Check(visible > 0, "real GPU timestamps reach the engine monitor values through Swap");
    Check(reports, "real engine reporting preserves individual values, monitored total and rendered total percentages");
    monitors.clear(); PerfMonGpu::ReportMonitors(CollectMonitor, nullptr, false);
    bool noTotals = monitors.size() >= 2;
    for (const auto& monitor : monitors) noTotals &= monitor.name != "TOTAL MONITORED" && monitor.name != "TOTAL RENDERED";
    Check(noTotals, "graph reporting omits the two totals while retaining named monitor rows");
    DWORD depth = 1; device->GetRenderState(D3DRS_ZENABLE, &depth);
    IDirect3DSurface9 *back = nullptr, *copy = nullptr;
    device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back);
    bool pixels = SUCCEEDED(device->CreateOffscreenPlainSurface(96, 64, D3DFMT_X8R8G8B8,
        D3DPOOL_SYSTEMMEM, &copy, nullptr)) && SUCCEEDED(device->GetRenderTargetData(back, copy));
    D3DLOCKED_RECT lock{};
    if (pixels && SUCCEEDED(copy->LockRect(&lock, nullptr, D3DLOCK_READONLY)))
    {
        for (unsigned y = 0; y < 64; ++y) for (unsigned x = 0; x < 96; ++x)
            pixels &= (reinterpret_cast<unsigned*>(static_cast<char*>(lock.pBits) + y * lock.Pitch)[x] & 0xFFFFFF) == 0x123456;
        copy->UnlockRect();
    }
    else pixels = false;
    Check(pixels && depth == FALSE, "timestamp attribution preserves the actual rendered pixels and depth state");
    if (copy) copy->Release(); if (back) back->Release();
    renderengine::GpuMonitorTimingPC::DeviceReset(); PerfMonGpu::Swap();
    Check(PerfMonGpu::GetLastTotal() == 0, "device reset clears the old completed sample");
    visible = 0; for (unsigned i = 0; i < 32; ++i) DrawFrame();
    Check(visible > 0, "fresh GPU packets complete after reset without reusing old queries");
    Check(asserts == 0, "the complete real registry/profiler/report path fires no engine assertions");
    renderengine::GpuMonitorTimingPC::DeviceReset(); renderengine::gDevice = nullptr;
    device->Release(); api->Release(); DestroyWindow(window);
    return true;
}
int main()
{
    ControlledChecks(); NativeChecks();
    std::printf("PCGpuMonitorTiming: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
