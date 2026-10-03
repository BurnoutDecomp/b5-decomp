// Actual frame recorder with a deterministic clock; verify that measuring many
// draws adds no per-draw clock reads and still preserves pacing/combat evidence.
#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

static unsigned suClockReads = 0;
static LONGLONG siTicks = 0;
static unsigned suCycleReads = 0;
static ULONG64 suCycles = 0;
static bool sbCycleApiAvailable = true, sbCycleReadSucceeds = true;
static BOOL WINAPI TestCycles(HANDLE, PULONG64 lpValue)
{
    ++suCycleReads;
    if (!sbCycleReadSucceeds) return FALSE;
    *lpValue = suCycles;
    return TRUE;
}
static BOOL WINAPI TestFailingCycles(HANDLE, PULONG64) { return FALSE; }
static FARPROC WINAPI TestGetProcAddress(HMODULE, LPCSTR lpcName)
{
    return sbCycleApiAvailable && std::strcmp(lpcName, "QueryThreadCycleTime") == 0
        ? reinterpret_cast<FARPROC>(&TestCycles) : nullptr;
}
static BOOL WINAPI TestCounter(LARGE_INTEGER* lpValue)
{
    ++suClockReads;
    lpValue->QuadPart = siTicks;
    return TRUE;
}
static BOOL WINAPI TestFrequency(LARGE_INTEGER* lpValue)
{
    lpValue->QuadPart = 1000000;
    return TRUE;
}
#define QueryPerformanceCounter TestCounter
#define QueryPerformanceFrequency TestFrequency
#define GetProcAddress TestGetProcAddress
#include "pc_frame_timing_only.inc"
#undef GetProcAddress
#undef QueryPerformanceCounter
#undef QueryPerformanceFrequency

namespace fp = renderengine::FrameProfile;
static unsigned suChecks = 0, suFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++suChecks;
    if (!lbPass) { ++suFailures; std::printf("FAIL: %s\n", lpcName); }
}
static void Reset(const char* lpcEnabled, const char* lpcTimingOnly, const char* lpcDetail,
                  const char* lpcCoarse = "0", const char* lpcCycles = "0")
{
    delete[] fp::gCapture.mpFrames;
    fp::gCapture = fp::Capture{};
    _putenv_s("BRN_FRAME_PROFILE", lpcEnabled);
    _putenv_s("BRN_FRAME_TIMING_ONLY", lpcTimingOnly);
    _putenv_s("BRN_FRAME_DETAIL", lpcDetail);
    _putenv_s("BRN_FRAME_COARSE", lpcCoarse);
    _putenv_s("BRN_FRAME_CPU_CYCLES", lpcCycles);
    _putenv_s("BRN_GPU_PROFILE", "0");
    suClockReads = 0;
    siTicks = 100;
    suCycleReads = 0; suCycles = 10;
    sbCycleApiAvailable = sbCycleReadSucceeds = true;
}
static std::string ReadOutput(const char* lpcSuffix)
{
    char lacPath[MAX_PATH];
    GetModuleFileNameA(nullptr, lacPath, MAX_PATH);
    const std::string lPath = std::string(lacPath) + lpcSuffix;
    std::string lContents;
    if (FILE* lpFile = std::fopen(lPath.c_str(), "rb"))
    {
        char lacBuffer[4096];
        while (const size_t luBytes = std::fread(lacBuffer, 1, sizeof(lacBuffer), lpFile))
            lContents.append(lacBuffer, luBytes);
        std::fclose(lpFile);
    }
    return lContents;
}
static double CsvValue(const std::string& lrCsv, const char* lpcColumn)
{
    const size_t luHeaderEnd = lrCsv.find('\n');
    const size_t luName = lrCsv.find(lpcColumn);
    if (luName == std::string::npos || luName >= luHeaderEnd) return -1;
    unsigned luColumn = 0;
    for (size_t lu = 0; lu < luName; ++lu) if (lrCsv[lu] == ',') ++luColumn;
    size_t luValue = luHeaderEnd + 1;
    while (luColumn--) {
        luValue = lrCsv.find(',', luValue);
        if (luValue == std::string::npos) return -1;
        ++luValue;
    }
    return std::atof(lrCsv.c_str() + luValue);
}

int main()
{
    Reset("1", "1", "1", "1", "1");
    fp::Camera(1);
    fp::Begin();
    fp::CameraOutput(reinterpret_cast<void*>(1), reinterpret_cast<void*>(2), true);
    { fp::Stage lStage(fp::RENDER_SETUP); lStage.Next(fp::RENDER_SHADOWS); }
    { fp::CycleScope lCreate(fp::GEOMETRY_CREATE); }
    for (unsigned lu = fp::POOL_REQUESTS; lu < fp::NUM_SECTIONS; ++lu)
    { fp::CycleScope lScope(static_cast<fp::Section>(lu)); }
    for (unsigned lu = 0; lu < 5000; ++lu)
    {
        fp::Scope lSubmit(fp::GEOMETRY_SUBMIT);
        fp::DetailScope lDetail(fp::MESH_CONSTANTS);
        fp::Draw();
    }
    Check(suClockReads == 1 && suCycleReads == 0, "5000 draws add no clocks, including explicit detailed/cycle mode");
    fp::Camera(3);
    fp::PlayerTakedown(6);
    fp::RivalSample(6, 3, 2);
    fp::RivalSample(5, 2, 1);
    fp::SceneLists(297, 2676, 249);
    fp::Instanced(4);
    fp::GeometryBinding(true, true);
    fp::GeometryBinding(false, false);
    fp::ResourceTexture(false); fp::ResourceTexture(false); fp::ResourceTexture(true);
    fp::TextureStaging(false); fp::TextureStaging(true); fp::TextureStaging(true);
    fp::Present();
    siTicks = 6100;
    fp::End();
    const fp::Frame& lrFirst = fp::gCapture.mpFrames[0];
    Check(suClockReads == 2 && lrFirst.miBegin == 100 && lrFirst.miEnd == 6100,
          "frame endpoints retain exact timestamps with two clocks");
    Check(lrFirst.muDraws == 5000 && lrFirst.muPresents == 1,
          "draw and successful-present counts survive");
    Check(lrFirst.muPlayerTakedowns == 1 && lrFirst.muTakedownVictims == (1u << 6)
          && lrFirst.muRivals == 6 && lrFirst.muCrashingRivals == 3 && lrFirst.muAirborneRivals == 2,
          "combat qualification retains credits, victim mask and busiest step");
    Check(lrFirst.miCameraBegin == 1 && lrFirst.miCameraEnd == 3 && lrFirst.muCameraChanges == 1,
          "camera transition remains attached to its frame");
    Check(lrFirst.muPreZMeshes == 297 && lrFirst.muWorldOpaqueMeshes == 2676
          && lrFirst.muCarOpaqueMeshes == 249 && lrFirst.muInstancedDraws == 1
          && lrFirst.muInstances == 4 && lrFirst.muVertexBindSkips == 1
          && lrFirst.muIndexBindRequests == 1,
          "workload and native-binding counters survive");
    bool lbNoSectionTimes = true;
    for (unsigned lu = 0; lu < fp::NUM_SECTIONS; ++lu)
        lbNoSectionTimes &= lrFirst.maTicks[lu] == 0;
    Check(lbNoSectionTimes, "disabled section times are not fabricated");

    siTicks = 6300;
    fp::Begin();
    siTicks = 12100;
    fp::End();
    const fp::Frame& lrSecond = fp::gCapture.mpFrames[1];
    Check(suClockReads == 4 && lrSecond.miPreviousEnd == 6100
          && lrSecond.miEnd - lrSecond.miPreviousEnd == 6000,
          "frame interval includes the gap between end and next begin");
    Check(lrSecond.muPresents == 0 && lrSecond.muPlayerTakedowns == 0
          && lrSecond.miCameraBegin == 3 && lrSecond.muCameraChanges == 0,
          "nonpresented frame remains distinguishable and counters reset");
    fp::Finish();
    const std::string lMetadata = ReadOutput(".frames.json");
    const std::string lCsv = ReadOutput(".frames.csv");
    Check(lMetadata.find("\"timing_only\":true") != std::string::npos
          && lMetadata.find("\"frames\":2") != std::string::npos,
          "saved metadata identifies timing-only records");
    Check(lCsv.find("0,0.006000,6.000000,6.000000,") != std::string::npos
          && lCsv.find("1,0.012000,6.000000,5.800000,") != std::string::npos,
          "saved CSV preserves interval and active duration independently");

    Check(CsvValue(lCsv, "raster_creates") == 2 && CsvValue(lCsv, "raster_releases") == 1,
          "native raster ownership counters survive timing-only capture");

    Check(CsvValue(lCsv, "staging_creates") == 1 && CsvValue(lCsv, "staging_reuses") == 2,
          "staging counters coexist with native ownership counters in timing-only output");

    Reset("1", "0", "1");
    fp::Begin();
    { fp::Scope lSubmit(fp::GEOMETRY_SUBMIT); siTicks += 40; }
    { fp::DetailScope lDetail(fp::MESH_CONSTANTS); siTicks += 60; }
    fp::End();
    Check(suClockReads == 6 && fp::gCapture.mpFrames[0].maTicks[fp::GEOMETRY_SUBMIT] == 40
          && fp::gCapture.mpFrames[0].maTicks[fp::MESH_CONSTANTS] == 60,
          "full section profiling remains available");

    Reset("1", "0", "1", "1");
    fp::Begin();
    {
        fp::Stage lStage(fp::RENDER_SETUP);
        siTicks += 40;
        lStage.Next(fp::RENDER_SHADOWS);
        for (unsigned lu = 0; lu < 5000; ++lu) {
            fp::Scope lSubmit(fp::GEOMETRY_SUBMIT);
            fp::DetailScope lDetail(fp::WORLD_DRAW);
            fp::Draw();
        }
        siTicks += 80;
        lStage.Next(fp::RENDER_WORLD);
        siTicks += 60;
    }
    fp::Present();
    fp::End();
    Check(suClockReads == 6, "coarse mode records stages without 5000 per-draw clocks");
    const fp::Frame& lrCoarse = fp::gCapture.mpFrames[0];
    Check(lrCoarse.maTicks[fp::RENDER_SETUP] == 40
          && lrCoarse.maTicks[fp::RENDER_SHADOWS] == 80
          && lrCoarse.maTicks[fp::RENDER_WORLD] == 60
          && lrCoarse.miEnd - lrCoarse.miBegin == 180,
          "consecutive stages partition elapsed time without overlap or gaps");
    Check(lrCoarse.maTicks[fp::WORLD_DRAW] == 0 && lrCoarse.maTicks[fp::GEOMETRY_SUBMIT] == 0
          && lrCoarse.muDraws == 5000 && lrCoarse.muPresents == 1,
          "coarse mode preserves counts and suppresses detailed mode even when requested");
    fp::Finish();
    const std::string lCoarseCsv = ReadOutput(".frames.csv");
    Check(CsvValue(lCoarseCsv, "render_setup_ms") == 0.04
          && CsvValue(lCoarseCsv, "render_shadows_ms") == 0.08
          && CsvValue(lCoarseCsv, "render_world_ms") == 0.06
          && CsvValue(lCoarseCsv, "draws") == 5000
          && CsvValue(lCoarseCsv, "presents") == 1,
          "new CSV columns retain stage and existing counter alignment");
    Check(ReadOutput(".frames.json").find("\"coarse\":true") != std::string::npos,
          "saved metadata identifies coarse attribution");

    Reset("1", "0", "0", "1", "1");
    fp::Begin();
    { fp::Scope lPrepare(fp::GEOMETRY_PREPARE);
      siTicks += 30;
      { fp::CycleScope lCreate(fp::GEOMETRY_CREATE); siTicks += 50; suCycles += 9; }
      { fp::CycleScope lLock(fp::GEOMETRY_LOCK); siTicks += 10; suCycles += 17; }
    }
    fp::End();
    Check(suClockReads == 8 && fp::gCapture.mpFrames[0].maTicks[fp::GEOMETRY_PREPARE] == 90
          && fp::gCapture.mpFrames[0].maTicks[fp::GEOMETRY_LOCK] == 10,
          "coarse mode retains cold allocation attribution without timing cached draws");
    Check(suCycleReads == 4 && fp::gCapture.mpFrames[0].maCycles[fp::GEOMETRY_CREATE] == 9
          && fp::gCapture.mpFrames[0].maCycles[fp::GEOMETRY_LOCK] == 17
          && fp::gCapture.mpFrames[0].muCycleReadFailures == 0,
          "thread cycles remain independent of elapsed time and nested preparation");
    fp::Finish();
    const std::string lCycleCsv = ReadOutput(".frames.csv");
    Check(CsvValue(lCycleCsv, "geometry_create_ms") == 0.05
          && CsvValue(lCycleCsv, "geometry_create_cycles") == 9
          && CsvValue(lCycleCsv, "geometry_lock_cycles") == 17
          && CsvValue(lCycleCsv, "cycle_read_failures") == 0,
          "CSV names align with new wall-time and raw-cycle columns");
    Check(ReadOutput(".frames.json").find("\"cpu_cycles_available\":true") != std::string::npos,
          "metadata identifies the available optional cycle source");

    Reset("1", "0", "0", "1", "1");
    fp::Begin();
    for (unsigned lu = fp::POOL_REQUESTS; lu < fp::NUM_SECTIONS; ++lu)
    {
        fp::CycleScope lScope(static_cast<fp::Section>(lu));
        siTicks += (lu - fp::POOL_REQUESTS + 1) * 1000;
        suCycles += (lu - fp::POOL_REQUESTS + 1) * 100;
    }
    fp::End(); fp::Finish();
    const std::string lResourceCsv = ReadOutput(".frames.csv");
    Check(CsvValue(lResourceCsv, "pool_requests_ms") == 1
          && CsvValue(lResourceCsv, "pool_requests_cycles") == 100,
          "pool_requests wall time and raw cycles keep separate CSV columns");
    Check(CsvValue(lResourceCsv, "pool_states_ms") == 2
          && CsvValue(lResourceCsv, "pool_states_cycles") == 200,
          "pool_states wall time and raw cycles keep separate CSV columns");
    Check(CsvValue(lResourceCsv, "pool_retire_ms") == 3
          && CsvValue(lResourceCsv, "pool_retire_cycles") == 300,
          "pool_retire wall time and raw cycles keep separate CSV columns");
    Check(CsvValue(lResourceCsv, "texture_realize_ms") == 4
          && CsvValue(lResourceCsv, "texture_realize_cycles") == 400,
          "texture_realize wall time and raw cycles keep separate CSV columns");
    Check(CsvValue(lResourceCsv, "texture_gpu_create_ms") == 5
          && CsvValue(lResourceCsv, "texture_gpu_create_cycles") == 500,
          "texture_gpu_create wall time and raw cycles keep separate CSV columns");
    Check(CsvValue(lResourceCsv, "texture_cpu_create_ms") == 6
          && CsvValue(lResourceCsv, "texture_cpu_create_cycles") == 600,
          "texture_cpu_create wall time and raw cycles keep separate CSV columns");
    Check(CsvValue(lResourceCsv, "texture_upload_ms") == 7
          && CsvValue(lResourceCsv, "texture_upload_cycles") == 700,
          "texture_upload wall time and raw cycles keep separate CSV columns");
    Check(CsvValue(lResourceCsv, "draws") == 0
          && CsvValue(lResourceCsv, "cycle_read_failures") == 0,
          "resource sections preserve subsequent counter alignment");

    Reset("1", "0", "0", "1", "1");
    fp::Begin(); sbCycleReadSucceeds = false;
    { fp::CycleScope lCopy(fp::GEOMETRY_COPY); siTicks += 5; suCycles += 10; }
    fp::End();
    Check(fp::gCapture.mpFrames[0].maTicks[fp::GEOMETRY_COPY] == 5
          && fp::gCapture.mpFrames[0].maCycles[fp::GEOMETRY_COPY] == 0
          && fp::gCapture.mpFrames[0].muCycleReadFailures == 1,
          "failed start sample retains wall time and marks unavailable cycle data");

    Reset("1", "0", "0", "1", "1");
    fp::Begin();
    { fp::CycleScope lCopy(fp::GEOMETRY_COPY); siTicks += 5; sbCycleReadSucceeds = false; }
    fp::End();
    Check(fp::gCapture.mpFrames[0].maCycles[fp::GEOMETRY_COPY] == 0
          && fp::gCapture.mpFrames[0].muCycleReadFailures == 1 && suCycleReads == 2,
          "failed end sample cannot publish a partial cycle measurement");

    Reset("1", "0", "0", "1", "1");
    sbCycleApiAvailable = false; fp::Begin();
    { fp::CycleScope lCreate(fp::GEOMETRY_CREATE); siTicks += 5; }
    fp::End();
    Check(!fp::gCapture.mpReadThreadCycles && suCycleReads == 0
          && fp::gCapture.mpFrames[0].muCycleReadFailures == 1,
          "unsupported API preserves capture without calling a missing entry point");

    Reset("0", "1", "1", "1", "1");
    fp::Begin();
    fp::CameraOutput(reinterpret_cast<void*>(1), reinterpret_cast<void*>(2), true);
    { fp::Scope lSubmit(fp::GEOMETRY_SUBMIT); fp::DetailScope lDetail(fp::MESH_CONSTANTS);
      fp::Stage lStage(fp::RENDER_SETUP); lStage.Next(fp::RENDER_WORLD);
      fp::CycleScope lCreate(fp::GEOMETRY_CREATE); }
    fp::End();
    Check(!fp::Active() && !fp::gCapture.mpFrames && suClockReads == 0 && suCycleReads == 0
          && fp::gCapture.muBehaviour == 0 && fp::gCapture.muShot == 0,
          "timing-only option never enables a disabled capture");

    Reset("1", "1", "0");
    fp::Camera(3);
    fp::Begin();
    fp::CameraOutput(reinterpret_cast<void*>(1234), reinterpret_cast<void*>(5678), true);
    fp::End();
    Check(fp::gCapture.mpFrames[0].muCameraPublications == 1
          && fp::gCapture.mpFrames[0].muNewCameraPublications == 1
          && fp::gCapture.mpFrames[0].muBehaviourChanges == 1
          && fp::gCapture.mpFrames[0].muShotChanges == 1,
          "published camera producers and native new-shot flag are retained");
    fp::Begin();
    fp::CameraOutput(reinterpret_cast<void*>(1234), reinterpret_cast<void*>(5678), true);
    fp::CameraOutput(reinterpret_cast<void*>(1234), reinterpret_cast<void*>(9012), false);
    fp::End();
    Check(fp::gCapture.mpFrames[1].muCameraChanges == 0
          && fp::gCapture.mpFrames[1].muCameraPublications == 2
          && fp::gCapture.mpFrames[1].muNewCameraPublications == 1
          && fp::gCapture.mpFrames[1].muBehaviourChanges == 0
          && fp::gCapture.mpFrames[1].muShotChanges == 1
          && fp::gCapture.mpFrames[1].muShotBegin == 5678
          && fp::gCapture.mpFrames[1].muShotEnd == 9012,
          "same-state and reused-producer shot changes survive multiple publications");
    fp::Begin(); fp::End();
    Check(fp::gCapture.mpFrames[2].muShotBegin == 9012
          && fp::gCapture.mpFrames[2].muShotEnd == 9012
          && fp::gCapture.mpFrames[2].muCameraPublications == 0
          && suClockReads == 6 && suCycleReads == 0,
          "frames without simulation retain camera identity without extra clocks");
    fp::Finish();
    const std::string lCameraCsv = ReadOutput(".frames.csv");
    Check(CsvValue(lCameraCsv, "camera_behaviour_begin") == 0
          && CsvValue(lCameraCsv, "camera_behaviour_end") == 1234
          && CsvValue(lCameraCsv, "camera_shot_end") == 5678
          && CsvValue(lCameraCsv, "camera_new_publications") == 1
          && CsvValue(lCameraCsv, "camera_changes") == 0,
          "camera publication columns preserve the distinct arbitrator-state marker");

    Reset("1", "0", "0", "1", "1");
    fp::Begin();
    { fp::CycleScope lUpdate(fp::UPDATE, fp::UPDATE_SIMULATION);
      siTicks += 20; suCycles += 17;
      { fp::CycleScope lMesh(fp::UPDATE_MESH_PREPARE); siTicks += 5; suCycles += 7; }
    }
    { fp::CycleScope lDispatch(fp::DISPATCH); siTicks += 60; suCycles += 3; }
    fp::End();
    Check(fp::gCapture.mpFrames[0].maTicks[fp::UPDATE_SIMULATION] == 25
          && fp::gCapture.mpFrames[0].maTicks[fp::DISPATCH] == 60
          && fp::gCapture.mpFrames[0].maCycles[fp::UPDATE] == 24
          && fp::gCapture.mpFrames[0].maCycles[fp::UPDATE_SIMULATION] == 24
          && fp::gCapture.mpFrames[0].maCycles[fp::UPDATE_MESH_PREPARE] == 7
          && fp::gCapture.mpFrames[0].maCycles[fp::DISPATCH] == 3,
          "simulation detail and dispatch cycles remain independent of elapsed waits");
    fp::Finish();
    const std::string lThreadCsv = ReadOutput(".frames.csv");
    Check(CsvValue(lThreadCsv, "update_cycles") == -1
          && CsvValue(lThreadCsv, "update_simulation_cycles") == 24
          && CsvValue(lThreadCsv, "dispatch_cycles") == 3
          && CsvValue(lThreadCsv, "update_mesh_prepare_cycles") == 7
          && CsvValue(lThreadCsv, "update_simulation_ms") == 0.025,
          "root thread cycle totals and nested preparation use the correct CSV columns");

    Reset("1", "0", "0", "1", "0");
    fp::Begin();
    fp::gCapture.mpReadThreadCycles = &TestFailingCycles;
    {
        // Construct/destroy on the owner so the deterministic wall clock and
        // section accumulators are not concurrently written by this test.
        fp::CycleScope lScope(fp::UPDATE);
        std::thread workers[4];
        for (auto& worker : workers)
            worker = std::thread([&lScope] {
                ULONG64 cycles = 0;
                for (unsigned i = 0; i < 100000; ++i) lScope.Read(cycles);
            });
        for (auto& worker : workers) worker.join();
    }
    fp::End();
    Check(fp::gCapture.mpFrames[0].muCycleReadFailures == 400000,
          "concurrent cycle-query failures retain every report");

    Reset("1", "1", "0");
    fp::Begin();
    fp::End();
    fp::gCapture.muCount = fp::KU_CAPACITY - 1;
    fp::Begin();
    fp::End();
    const unsigned luBeforeOverflow = suClockReads;
    fp::Begin();
    fp::End();
    Check(fp::gCapture.muCount == fp::KU_CAPACITY && fp::gCapture.muDropped == 1
          && !fp::Active() && suClockReads == luBeforeOverflow,
          "bounded capture reports overflow without recording beyond capacity");
    delete[] fp::gCapture.mpFrames;
    fp::gCapture = fp::Capture{};
    std::printf("PCFrameTimingOnly: %u checks, %u failures\n", suChecks, suFailures);
    return suFailures ? 1 : 0;
}
