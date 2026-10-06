#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"
#include <cstdio>
#include <cstring>
#undef CGS_ASSERT
#define CGS_ASSERT(test, message) do { if (!(test)) ++guFailures; } while (0)

static unsigned guChecks, guFailures;
struct RecordingQueue {
    unsigned muCalls;
    s32 miChannel, miSize;
    u32 mauHeader[3];
    void AddEvent(const CgsModule::Event* lpEvent, s32 liChannel, s32 liSize) {
        ++muCalls; miChannel = liChannel; miSize = liSize;
        std::memcpy(mauHeader, lpEvent, sizeof(mauHeader));
    }
};
struct StateInterface {
    RecordingQueue mQueue;
    RecordingQueue* GetOutputEventQueue() { return &mQueue; }
};
namespace BrnGui {
struct GuiCache {};
#include "playtest_full_map_rank_request_shape.inc"
struct PanelBindFixture {
    GuiCache* mpGuiCache;
    StateInterface* mpStateInterface;
    bool Bind(const CgsModule::Event* lpEvent) {
#include "playtest_full_map_rank_request.inc"
    }
};
}
static void Check(bool lbPass, const char* lpcName) {
    ++guChecks;
    if (!lbPass) { ++guFailures; std::printf("FAIL %s\n", lpcName); }
}
int main() {
    StateInterface lState = {};
    BrnGui::GuiCache lCache;
    BrnGui::GuiCache* lpCache = &lCache;
    BrnGui::PanelBindFixture lPanel = {nullptr, &lState};
    const bool lbFilterChanged = lPanel.Bind(reinterpret_cast<const CgsModule::Event*>(&lpCache));
    Check(lPanel.mpGuiCache == lpCache && !lbFilterChanged,
          "cache bind preserves original return and member store");
    Check(lState.mQueue.muCalls == 1 && lState.mQueue.miChannel == 40 && lState.mQueue.miSize == 16,
          "rank request posts one 16-byte GUI-output record");
    Check(lState.mQueue.mauHeader[0] == 1 && lState.mQueue.mauHeader[1] == 437 &&
          lState.mQueue.mauHeader[2] == 12,
          "rank request header matches ARTIST payload size, id and offset");
    lPanel.Bind(reinterpret_cast<const CgsModule::Event*>(&lpCache));
    Check(lState.mQueue.muCalls == 1, "repeat bind does not issue duplicate rank queries");
    std::printf("PlaytestFullMapRankRequest: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
