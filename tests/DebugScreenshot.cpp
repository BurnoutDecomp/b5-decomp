// Production screenshot writer + exact request/present slices. No game or D3D device.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include "types.hpp"
#include "pc/gcm/renderengine/pixelformat.h"

namespace {
struct CaptureBoundary {
    bool lockFails = false;
    std::array<u8,24> working = {}, presented = {};
    unsigned presents = 0, locks = 0, unlocks = 0;
    std::string order, diagnostics;
} gCapture;
unsigned guChecks = 0, guFailures = 0;
void Check(const char* id, bool passed) {
    ++guChecks; if (!passed) ++guFailures;
    std::printf("CHECK %s %s\n", id, passed ? "PASS" : "FAIL");
}
void ResetBoundary() {
    gCapture = {};
    gCapture.working = {{1,2,3,0xE0,4,5,6,0xE1,0xA5,0xA5,0xA5,0xA5,
                         7,8,9,0xE2,10,11,12,0xE3,0xA5,0xA5,0xA5,0xA5}};
}
std::vector<u8> Bytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
struct CaseDirectory {
    std::filesystem::path before = std::filesystem::current_path();
    explicit CaseDirectory(const char* name) {
        const auto directory = before / name;
        if (!std::filesystem::create_directory(directory)) throw std::runtime_error("case directory already exists");
        std::filesystem::current_path(directory);
        ResetBoundary();
    }
    ~CaseDirectory() { std::filesystem::current_path(before); }
};
// Passive profiling boundary; the copied production stage statement stays intact.
constexpr int RENDER_PRESENT = 0;
struct StageObserver { void Next(int) {} } lRenderStage;
}

// The candidate descriptor below is copied from the actual Device declaration.
// The old control has no lock API/writer, so no substitute writer is supplied.
#include "debug_screenshot_types.inc"

namespace CgsCore { void SPrintf(char*, u32, const char*, ...); }
namespace CgsDev {
namespace Log { void WriteToLog(const char* message) { gCapture.diagnostics += message; } }
namespace PerfMonGpu { void StopProfiling() { gCapture.order += 'S'; } }
}

// Fixture storage mirrors the real named declarations. This is not an ABI/layout test.
struct BrnRendererModule {
    bool mbUpdateThreadTakeScreenshot = false, mbDispatchThreadTakeScreenshot = false;
    bool mbCaptureOverlaysInScreenshot = false, mbRenderHudImmediateMode = true;
    u32 muScreenshotCounter = 0;
    char macScreenShotText[32] = {};
#if SCREENSHOT_HAS_PRODUCTION_WRITER
    void TakeScreenshot();
#endif
    void FixtureHandoff();
    void FixturePresent();
    bool FixtureOverlayPermission() const;
};

void renderengine::Device::ShowPixelBuffer() {
    ++gCapture.presents; gCapture.order += 'P';
    gCapture.presented = gCapture.working;
}
#if SCREENSHOT_HAS_PRODUCTION_WRITER
bool renderengine::Device::LockFrontBuffer(LockedFrontBuffer& output) {
    ++gCapture.locks; gCapture.order += 'L'; output = {}; output.format = PIXELFORMAT_NA;
    if (gCapture.lockFails) return false;
    output.pixelData = gCapture.presented.data(); output.format = PIXELFORMAT_X8R8G8B8;
    output.pitch = 12; output.width = 2; output.height = 2;
    return true;
}
void renderengine::Device::UnlockFrontBuffer(LockedFrontBuffer&) {
    ++gCapture.unlocks; gCapture.order += 'U';
}
#endif

#include "debug_screenshot_extracted.inc"

int main() {
    try {
        {
            CaseDirectory directory("handoff"); BrnRendererModule renderer;
            renderer.mbUpdateThreadTakeScreenshot = true; renderer.FixtureHandoff();
            Check("handoff_update_cleared", !renderer.mbUpdateThreadTakeScreenshot);
            Check("handoff_dispatch_set", renderer.mbDispatchThreadTakeScreenshot);
        }
        {
            CaseDirectory directory("idle"); BrnRendererModule renderer; renderer.muScreenshotCounter = 7;
            renderer.FixturePresent();
            Check("no_request_no_lock", gCapture.locks == 0 && gCapture.unlocks == 0);
            Check("no_request_no_counter", renderer.muScreenshotCounter == 7);
        }
        {
            CaseDirectory directory("lock_failure"); BrnRendererModule renderer;
            renderer.muScreenshotCounter = 7; renderer.mbUpdateThreadTakeScreenshot = true;
            gCapture.lockFails = true; renderer.FixtureHandoff(); renderer.FixturePresent();
            Check("lock_fail_request_consumed", !renderer.mbDispatchThreadTakeScreenshot);
            Check("lock_fail_counter_unchanged", renderer.muScreenshotCounter == 7);
            Check("lock_fail_no_unlock", gCapture.unlocks == 0);
            Check("lock_fail_attempt_after_present", gCapture.order == "PSL" && gCapture.locks == 1);
        }
        {
            CaseDirectory directory("no_directory"); BrnRendererModule renderer;
            renderer.muScreenshotCounter = 10; renderer.mbUpdateThreadTakeScreenshot = true;
            renderer.FixtureHandoff(); renderer.FixturePresent();
            Check("no_dir_no_creation", !std::filesystem::exists("d_"));
            Check("no_dir_request_consumed", !renderer.mbDispatchThreadTakeScreenshot);
            Check("no_dir_counter_advanced", renderer.muScreenshotCounter == 11);
            Check("no_dir_unlock", gCapture.unlocks == 1);
            Check("no_dir_original_open_failure", gCapture.diagnostics.find("open failed: d_\\screenshot10_.tga") != std::string::npos);
        }
        {
            CaseDirectory directory("tga_bytes"); std::filesystem::create_directory("d_");
            BrnRendererModule renderer; renderer.muScreenshotCounter = 7;
            std::strncpy(renderer.macScreenShotText, "Crash: A", sizeof(renderer.macScreenShotText));
            renderer.mbUpdateThreadTakeScreenshot = true; renderer.FixtureHandoff(); renderer.FixturePresent();
            const std::filesystem::path first("d_\\screenshot7_Crash__A.tga");
            const auto bytes = Bytes(first);
            const std::array<u8,18> header = {{0,0,2,0,0,0,0,0,0,0,0,0,2,0,2,0,24,0}};
            const std::array<u8,12> pixels = {{7,8,9,10,11,12,1,2,3,4,5,6}};
            Check("file_exists", std::filesystem::is_regular_file(first));
            Check("header_exact", bytes.size() >= header.size() && std::equal(header.begin(),header.end(),bytes.begin()));
            Check("bottomup_bgr_exact", bytes.size() == 30 && std::equal(pixels.begin(),pixels.end(),bytes.begin()+18));
            Check("alpha_and_padding_removed", bytes.size() == 30 &&
                std::find(bytes.begin(),bytes.end(),0xA5) == bytes.end() &&
                std::none_of(bytes.begin(),bytes.end(),[](u8 value){return value >= 0xE0 && value <= 0xE3;}));
            Check("counter_filename_before_increment", std::filesystem::is_regular_file(first) &&
                !std::filesystem::exists("d_\\screenshot8_Crash__A.tga"));
            Check("success_request_consumed", !renderer.mbDispatchThreadTakeScreenshot);
            Check("success_counter_advanced", renderer.muScreenshotCounter == 8);
            Check("success_single_lock_unlock", gCapture.locks == 1 && gCapture.unlocks == 1);
            Check("success_post_present_order", gCapture.order == "PSLU");
            const auto oldCounter = renderer.muScreenshotCounter; const auto oldLocks = gCapture.locks;
            renderer.FixturePresent();
            Check("idle_no_duplicate_capture", renderer.muScreenshotCounter == oldCounter && gCapture.locks == oldLocks);
            renderer.mbUpdateThreadTakeScreenshot = true; renderer.FixtureHandoff(); renderer.FixturePresent();
            Check("second_request_new_file", std::filesystem::is_regular_file("d_\\screenshot8_Crash__A.tga"));
            Check("second_request_counter", renderer.muScreenshotCounter == 9);
        }
        {
            CaseDirectory directory("open_collision"); std::filesystem::create_directory("d_");
            std::filesystem::create_directory("d_\\screenshot3_.tga");
            BrnRendererModule renderer; renderer.muScreenshotCounter = 3;
            renderer.mbUpdateThreadTakeScreenshot = true; renderer.FixtureHandoff(); renderer.FixturePresent();
            Check("fopen_collision_request_consumed", !renderer.mbDispatchThreadTakeScreenshot);
            Check("fopen_collision_counter_advanced", renderer.muScreenshotCounter == 4);
            Check("fopen_collision_unlocked", gCapture.unlocks == 1);
            Check("fopen_collision_not_replaced", std::filesystem::is_directory("d_\\screenshot3_.tga"));
        }
        {
            BrnRendererModule renderer; renderer.mbRenderHudImmediateMode = false;
            Check("overlay_hud_gate", !renderer.FixtureOverlayPermission());
            renderer.mbRenderHudImmediateMode = true;
            Check("overlay_idle_visible", renderer.FixtureOverlayPermission());
            renderer.mbDispatchThreadTakeScreenshot = true;
            Check("overlay_request_false_hides", !renderer.FixtureOverlayPermission());
            renderer.mbCaptureOverlaysInScreenshot = true;
            Check("overlay_request_true_keeps", renderer.FixtureOverlayPermission());
        }
    } catch (const std::exception& error) {
        std::printf("UNEXPECTED_FIXTURE_EXCEPTION %s\n", error.what()); return 2;
    }
    std::printf("DebugScreenshot: %u checks, %u failures\n",guChecks,guFailures);
    return guFailures ? 1 : 0;
}
