// Actual frame recorder with a deterministic clock; verify that measuring many
// draws adds no per-draw clock reads and still preserves pacing/combat evidence.
#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static unsigned suClockReads = 0;
static LONGLONG siTicks = 0;
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
#include "pc_frame_timing_only.inc"
#undef QueryPerformanceCounter
#undef QueryPerformanceFrequency

namespace fp = renderengine::FrameProfile;
static unsigned suChecks = 0, suFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++suChecks;
    if (!lbPass) { ++suFailures; std::printf("FAIL: %s\n", lpcName); }
}
static void Reset(const char* lpcEnabled, const char* lpcTimingOnly, const char* lpcDetail)
{
    delete[] fp::gCapture.mpFrames;
    fp::gCapture = fp::Capture{};
    _putenv_s("BRN_FRAME_PROFILE", lpcEnabled);
    _putenv_s("BRN_FRAME_TIMING_ONLY", lpcTimingOnly);
    _putenv_s("BRN_FRAME_DETAIL", lpcDetail);
    _putenv_s("BRN_GPU_PROFILE", "0");
    suClockReads = 0;
    siTicks = 100;
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

int main()
{
    Reset("1", "1", "1");
    fp::Camera(1);
    fp::Begin();
    for (unsigned lu = 0; lu < 5000; ++lu)
    {
        fp::Scope lSubmit(fp::GEOMETRY_SUBMIT);
        fp::DetailScope lDetail(fp::MESH_CONSTANTS);
        fp::Draw();
    }
    Check(suClockReads == 1, "5000 draws add no clocks, including explicit detailed mode");
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

    Reset("0", "1", "1");
    fp::Begin();
    { fp::Scope lSubmit(fp::GEOMETRY_SUBMIT); fp::DetailScope lDetail(fp::MESH_CONSTANTS); }
    fp::End();
    Check(!fp::Active() && !fp::gCapture.mpFrames && suClockReads == 0,
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
