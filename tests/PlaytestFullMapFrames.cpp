#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"
#include <cstdio>
#include <cstring>

namespace BrnGui {
class CrashNavIconRenderer {
public:
    bool mbRenderEnabled;
    GuiEventSetHoveredEventIcon mHoveredEventIcon, mHoveredEventIconLastFrame;
    GuiEventMapCursorStatus mGuiEventMapCursorStatus;
    GuiEventMapIconStatus mGuiEventMapIconStatus;
    GuiEventRoadSignIconStatus mRoadSignIconStatus;
#include "playtest_full_map_frame_shape.inc"
    FrameInputPC mFrameInputPC;
    bool mbFrameInputPCValid;
    void Update();
    void RestoreFrameInputPC();
    void SetRenderEnabled(bool);
    void Consume() {
#include "playtest_full_map_consume.inc"
    }
};
#include "playtest_full_map_frame_bodies.inc"
}

static unsigned guChecks, guFailures;
static void Check(bool lbPass, const char* lpcName) {
    ++guChecks;
    if (!lbPass) { ++guFailures; std::printf("FAIL %s\n", lpcName); }
}

int main() {
    BrnGui::CrashNavIconRenderer lRenderer = {};
    int liBank = 0, liSigns = 0;
    const char* lpcRoad = "NAKAMURA";
    lRenderer.mbRenderEnabled = true;
    lRenderer.mGuiEventMapIconStatus.lpSatNavIcons =
        reinterpret_cast<BrnGui::CrashNavIconComponent*>(&liBank);
    lRenderer.mGuiEventMapIconStatus.liNumberOfIcons = 2;
    lRenderer.mRoadSignIconStatus.mpRoadSignIcons =
        reinterpret_cast<BrnGui::RoadSignIcon*>(&liSigns);
    lRenderer.mRoadSignIconStatus.mfScaleFactor = 0.75f;
    lRenderer.mHoveredEventIcon.muHoveredEventID = 1234;
    lRenderer.mHoveredEventIcon.mpcHoveredRoadName = lpcRoad;
    lRenderer.mGuiEventMapCursorStatus.miAnimationState = 0;
    lRenderer.mGuiEventMapCursorStatus.miDisplayState = 1;
    lRenderer.mGuiEventMapCursorStatus.mv2Position.x = 856.0f;
    lRenderer.Update();
    lRenderer.Consume();
    Check(lRenderer.mGuiEventMapIconStatus.lpSatNavIcons == nullptr &&
          lRenderer.mGuiEventMapIconStatus.liNumberOfIcons == 0 &&
          lRenderer.mHoveredEventIcon.muHoveredEventID == 0,
          "ARTIST consumption still clears working records");
    bool lbAllBanks = true, lbAllCursors = true, lbAllHovers = true;
    for (int liPresent = 0; liPresent < 5; ++liPresent) {
        lRenderer.RestoreFrameInputPC();
        lbAllBanks &= lRenderer.mGuiEventMapIconStatus.lpSatNavIcons != nullptr &&
                      lRenderer.mGuiEventMapIconStatus.liNumberOfIcons == 2 &&
                      lRenderer.mRoadSignIconStatus.mpRoadSignIcons != nullptr &&
                      lRenderer.mRoadSignIconStatus.mfScaleFactor == 0.75f;
        lbAllCursors &= lRenderer.mGuiEventMapCursorStatus.miAnimationState == 0 &&
                        lRenderer.mGuiEventMapCursorStatus.mv2Position.x == 856.0f;
        lbAllHovers &= lRenderer.mHoveredEventIcon.muHoveredEventID == 1234 &&
                       lRenderer.mHoveredEventIcon.mpcHoveredRoadName == lpcRoad;
        lRenderer.Consume();
    }
    Check(lbAllBanks, "all render-only presentations retain the published banks");
    Check(lbAllCursors, "all render-only presentations retain cursor position and animation");
    Check(lbAllHovers, "all render-only presentations retain the same hover");
    lRenderer.Update();
    lRenderer.RestoreFrameInputPC();
    Check(lRenderer.mGuiEventMapIconStatus.lpSatNavIcons == nullptr &&
          lRenderer.mHoveredEventIcon.muHoveredEventID == 0,
          "next empty GUI update replaces rather than prolongs old input");
    lRenderer.mGuiEventMapIconStatus.liNumberOfIcons = 4;
    lRenderer.Update();
    lRenderer.Consume();
    lRenderer.RestoreFrameInputPC();
    Check(lRenderer.mGuiEventMapIconStatus.liNumberOfIcons == 4,
          "new GUI update replaces snapshot contents");
    lRenderer.SetRenderEnabled(false);
    lRenderer.RestoreFrameInputPC();
    Check(!lRenderer.mbFrameInputPCValid && lRenderer.mGuiEventMapIconStatus.liNumberOfIcons == 0 &&
          lRenderer.mHoveredEventIcon.muHoveredEventID == 0,
          "disabling renderer discards snapshot and hover");
    std::printf("PlaytestFullMapFrames: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
