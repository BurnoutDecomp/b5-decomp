#include "types.hpp"
#include <cstdio>
#include <limits>

static unsigned guChecks, guFailures;
namespace CgsModule { struct Event; }
namespace CgsDev {
namespace Message { static u64 gxMessageFilterFlags; }
namespace Log {
    struct Sink { Sink& operator<<(const char*) { return *this; } };
    static Sink* gpDebugPrint;
} }
namespace BrnGui {
struct Queue {
    int miCount, miType, miSize;
    void AddEvent(const CgsModule::Event*, int liType, int liSize) {
        ++miCount; miType = liType; miSize = liSize;
    }
};
struct HudMessageDirector {
    static const f32 KF_MINIMUM_BLACK_BAR_STOP_SIZE;
    f32 mfBlackBarSize;
    bool mbStopFlagBlackBar;
    Queue mHudMessageQueue;
    void SetBlackBarSize(f32);
};
const f32 HudMessageDirector::KF_MINIMUM_BLACK_BAR_STOP_SIZE = 0.1f;
#include "playtest_black_bars_message_filter.inc"
}
static void Check(bool lbPassed, const char* lpcLabel) {
    ++guChecks; if (!lbPassed) { ++guFailures; std::printf("FAIL %s\n", lpcLabel); }
}
int main() {
    BrnGui::HudMessageDirector lDirector = {};
    lDirector.SetBlackBarSize(0.09f);
    Check(!lDirector.mbStopFlagBlackBar && !lDirector.mHudMessageQueue.miCount, "small bars leave messages visible");
    lDirector.SetBlackBarSize(0.1f);
    Check(lDirector.mbStopFlagBlackBar && lDirector.mHudMessageQueue.miCount == 1, "exact threshold stops messages once");
    Check(lDirector.mHudMessageQueue.miType == 156 && lDirector.mHudMessageQueue.miSize == 1, "original stop event and one-byte wire size");
    lDirector.SetBlackBarSize(0.15f);
    Check(lDirector.mHudMessageQueue.miCount == 1, "held cinematic does not repeat stop events");
    lDirector.SetBlackBarSize(0.0f);
    Check(!lDirector.mbStopFlagBlackBar && lDirector.mHudMessageQueue.miCount == 1, "return to gameplay clears filter without another stop");
    lDirector.SetBlackBarSize(0.15f);
    Check(lDirector.mHudMessageQueue.miCount == 2, "next cinematic creates a new rising edge");
    lDirector.mfBlackBarSize = std::numeric_limits<f32>::quiet_NaN();
    lDirector.SetBlackBarSize(0.0f);
    Check(!lDirector.mbStopFlagBlackBar, "unordered previous value follows original falling-edge bge");
    std::printf("PlaytestBlackBarsMessageFilter: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
