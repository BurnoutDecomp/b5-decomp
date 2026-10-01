// Actual frame recorder with a deterministic clock; verify that measuring many
// draws adds no per-draw clock reads and still preserves pacing/combat evidence.
#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

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
    { fp::Stage lStage(fp::RENDER_SETUP); lStage.Next(fp::RENDER_SHADOWS); }
    { fp::CycleScope lCreate(fp::GEOMETRY_CREATE); }
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
    { fp::Scope lSubmit(fp::GEOMETRY_SUBMIT); fp::DetailScope lDetail(fp::MESH_CONSTANTS);
      fp::Stage lStage(fp::RENDER_SETUP); lStage.Next(fp::RENDER_WORLD);
      fp::CycleScope lCreate(fp::GEOMETRY_CREATE); }
    fp::End();
    Check(!fp::Active() && !fp::gCapture.mpFrames && suClockReads == 0 && suCycleReads == 0,
          "timing-only option never enables a disabled capture");

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
