#include <cmath>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

#include "GameSource/World/AI/Route/BrnRouteMapDebugComponent.h"
#include "GameSource/World/AI/BrnAIPortal.h"
#include "GameSource/World/AI/BrnRouteRequestManager.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"

namespace BrnAI
{
// The HUD body under test reads the real route-manager field and enum. The rest of AIModule is
// outside this fixture; GetMasterRoute is a draw-boundary input and returns an empty route.
class AIModule
{
public:
    RouteRequestManager mRouteRequestManager;
    const Route* GetMasterRoute() const;
};
}

namespace CgsDev
{
using RGBA = u32;

struct Debug2DImmediateRender
{
    void DrawBox(f32 lfX0, f32 lfY0, f32 lfX1, f32 lfY1, RGBA lColour);
    void DrawLine(Vector2 lv2Start, Vector2 lv2End, RGBA lColour);
};
}

namespace
{
struct BoxCall
{
    f32 mfX0, mfY0, mfX1, mfY1;
    u32 muColour;
};

struct LineCall
{
    Vector2 mv2Start, mv2End;
    u32 muColour;
};

struct TextCall
{
    std::string mText;
    f32 mfX, mfY, mfScale;
    u32 muColour;
    bool mbCentred;
};

std::vector<BoxCall> gaBoxes;
std::vector<LineCall> gaLines;
std::vector<TextCall> gaTexts;
std::vector<u32> gauRouteColours;
std::map<const BrnAI::Portal*, u16> gauPortalLinks;
std::map<const BrnAI::AISection*, Vector3> gaSectionMiddles;
unsigned guChecks;
unsigned guFailures;
u32 guAccessibleSectionCount = 11;
unsigned guSectionLookups;
unsigned guInvalidSectionLookups;

void Check(bool lbPass, const char* lpcName)
{
    ++guChecks;
    if (!lbPass)
    {
        ++guFailures;
        std::printf("FAIL %s\n", lpcName);
    }
}

bool Near(f32 lfLeft, f32 lfRight)
{
    return std::fabs(lfLeft - lfRight) < 0.0001f;
}

void ResetDraws()
{
    gaBoxes.clear();
    gaLines.clear();
    gaTexts.clear();
    gauRouteColours.clear();
}

BrnAI::AISection gaSections[15] = {};
BrnAI::Portal gaPortals[20] = {};
BrnAI::AISectionsData gSectionsData = {};

void ConfigureSections()
{
    gauPortalLinks.clear();
    gaSectionMiddles.clear();
    u32 luPortal = 0;

    const auto setSection = [&](u32 luIndex, u8 luFlags, std::initializer_list<u16> laLinks)
    {
        BrnAI::AISection& lrSection = gaSections[luIndex];
        lrSection.mId = 100u + luIndex;
        lrSection.mx8Flags = luFlags;
        lrSection.mu8NumPortals = static_cast<u8>(laLinks.size());
        lrSection.mpaPortals = laLinks.size() ? &gaPortals[luPortal] : nullptr;
        gaSectionMiddles[&lrSection] = Vector3{10.0f * luIndex, 0.0f, 20.0f * luIndex, 0.0f};

        for (u16 luTarget : laLinks)
            gauPortalLinks[&gaPortals[luPortal++]] = luTarget;
    };

    setSection(0, 0x03u, {0});       // NO_RESET wins over SHORTCUT.
    setSection(1, 0x01u, {1});       // A reciprocal self-link preserves shortcut blue.
    setSection(2, 0x00u, {2});       // Ordinary one-portal section is green.
    setSection(3, 0x03u, {4, 5});    // Two one-way links; portal-count flags are ignored.
    setSection(4, 0x00u, {11, 12});  // Valid reciprocal destinations, so section 4 is filtered.
    setSection(5, 0x00u, {13, 14});  // Valid reciprocal destinations, so section 5 is filtered.
    setSection(6, 0x00u, {7, 7});   // Reciprocal duplicate destination becomes white.
    setSection(7, 0x00u, {6});
    setSection(8, 0x00u, {9, 10});  // Reciprocal distinct destinations are filtered out.
    setSection(9, 0x00u, {8});
    setSection(10, 0x00u, {8});
    setSection(11, 0x00u, {4});
    setSection(12, 0x00u, {4});
    setSection(13, 0x00u, {5});
    setSection(14, 0x00u, {5});

    gSectionsData.mpaSections = gaSections;
    gSectionsData.muNumSections = 15;
    guAccessibleSectionCount = 15;
    guSectionLookups = 0;
    guInvalidSectionLookups = 0;
}
}

int MaybeDrawText(CgsDev::Debug2DImmediateRender*, const char* lpcText,
                  f32 lfX, f32 lfY, f32 lfScale, CgsDev::RGBA luColour, bool lbCentred)
{
    gaTexts.push_back({lpcText ? lpcText : "<NULLSTRING>", lfX, lfY, lfScale, luColour, lbCentred});
    return 1;
}

namespace CgsDev
{
void Debug2DImmediateRender::DrawBox(f32 lfX0, f32 lfY0, f32 lfX1, f32 lfY1, RGBA luColour)
{
    gaBoxes.push_back({lfX0, lfY0, lfX1, lfY1, luColour});
}

void Debug2DImmediateRender::DrawLine(Vector2 lv2Start, Vector2 lv2End, RGBA luColour)
{
    gaLines.push_back({lv2Start, lv2End, luColour});
}
}

namespace CgsDev::Assert
{
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { return 0; }
void* EndAssert() { return nullptr; }
}

namespace BrnAI
{
const Route* AIModule::GetMasterRoute() const
{
    return nullptr;
}

const AISection* AISectionsData::GetAISection(u32 luSectionIndex) const
{
    ++guSectionLookups;
    if (luSectionIndex >= guAccessibleSectionCount)
    {
        ++guInvalidSectionLookups;
        return nullptr;
    }
    return &mpaSections[luSectionIndex];
}

const Portal* AISection::GetPortal(u8 luPortalIndex) const
{
    return &mpaPortals[luPortalIndex];
}

Vector3 AISection::GetMiddle() const
{
    return gaSectionMiddles[this];
}

u16 Portal::GetLinkSectionIndex() const
{
    return gauPortalLinks[this];
}
}

#include "route_map_draw_helpers.inc"

namespace BrnAI
{
void RouteMapDebugComponent::RenderWorld(CgsDev::Debug3DImmediateRender*) {}
const char* RouteMapDebugComponent::GetName() const { return "Route Map"; }
const char* RouteMapDebugComponent::GetPath() const { return "AI"; }
void RouteMapDebugComponent::OnActivate() {}

void RouteMapDebugComponent::DrawAISectionMap(CgsDev::Debug2DImmediateRender*) const {}
void RouteMapDebugComponent::DrawCarsInMap(CgsDev::Debug2DImmediateRender*) const {}
void RouteMapDebugComponent::DrawRouteInMap(CgsDev::Debug2DImmediateRender*, const Route*, RGBA luColour) const
{
    gauRouteColours.push_back(luColour);
}
void RouteMapDebugComponent::DrawAStarInMap(CgsDev::Debug2DImmediateRender*) const {}
void RouteMapDebugComponent::DrawEventBlockSectionsInMap(CgsDev::Debug2DImmediateRender*) const {}
void RouteMapDebugComponent::DrawAISectionInfo(CgsDev::Debug2DImmediateRender*, u16) {}
}

int main()
{
    CgsDev::Debug2DImmediateRender lDrawSink;
    BrnAI::RouteMapDebugComponent lDebug{};

    ResetDraws();
    lDebug.DrawPoint(&lDrawSink, Vector2{50.0f, 70.0f, 0.0f, 0.0f}, 4.5f, 0xA1B2C3D4u);
    Check(gaBoxes.size() == 1, "DrawPoint emits one box");
    if (gaBoxes.size() == 1)
    {
        Check(Near(gaBoxes[0].mfX0, 45.5f) && Near(gaBoxes[0].mfY0, 65.5f) &&
                  Near(gaBoxes[0].mfX1, 54.5f) && Near(gaBoxes[0].mfY1, 74.5f),
              "DrawPoint sends centre minus/plus size as scalar min/max corners");
        Check(gaBoxes[0].muColour == 0xA1B2C3D4u, "DrawPoint forwards the packed colour unchanged");
    }

    ConfigureSections();
    lDebug.mpAISectionsData = &gSectionsData;
    lDebug.mfMapScale = 2.0f;
    lDebug.mfMapOffsetX = 100.0f;
    lDebug.mfMapOffsetY = 200.0f;
    lDebug.mbHighDetailMap = true;
    ResetDraws();
    lDebug.DrawDeadEndsInMap(&lDrawSink);
    Check(gaLines.size() == 24, "dead-end scan draws exactly twelve selected sections");
    Check(gaTexts.size() == 12, "high-detail scan labels each selected section");
    if (gaLines.size() == 24)
    {
        Check(gaLines[0].muColour == 0xFFFF0000u && gaLines[1].muColour == 0xFFFF0000u,
              "NO_RESET takes color precedence when both section flags are set");
        Check(gaLines[2].muColour == 0xFF0000FFu && gaLines[3].muColour == 0xFF0000FFu,
              "one-portal SHORTCUT uses the original blue packed word");
        Check(gaLines[4].muColour == 0xFF00FF00u && gaLines[5].muColour == 0xFF00FF00u,
              "ordinary one-portal section uses the original green packed word");
        Check(gaLines[6].muColour == 0xFFFFFF00u && gaLines[7].muColour == 0xFFFFFF00u,
              "nonreciprocal portal links mark the section yellow");
        Check(gaLines[8].muColour == 0xFFFFFFFFu && gaLines[9].muColour == 0xFFFFFFFFu,
              "duplicate portal destinations override with white");
        Check(Near(gaLines[6].mv2Start.x, 155.0f) && Near(gaLines[6].mv2Start.y, 315.0f) &&
                  Near(gaLines[6].mv2End.x, 165.0f) && Near(gaLines[6].mv2End.y, 325.0f),
              "dead-end marker uses the scaled section middle and five-unit X size");
    }
    if (gaTexts.size() == 12)
    {
        Check(gaTexts[0].mText == "0-100" && gaTexts[3].mText == "3-103" &&
                  gaTexts[4].mText == "6-106" && gaTexts[11].mText == "14-114",
              "high-detail label retains the original section-index and ID format");
        Check(gaTexts[3].muColour == 0xFFFFFFFFu && Near(gaTexts[3].mfX, 160.0f) &&
                  Near(gaTexts[3].mfY, 320.0f) && Near(gaTexts[3].mfScale, 12.0f) &&
                  !gaTexts[3].mbCentred,
              "high-detail label is white, uncentred, and anchored at the mapped middle");
    }

    lDebug.mbHighDetailMap = false;
    ResetDraws();
    lDebug.DrawDeadEndsInMap(&lDrawSink);
    Check(gaLines.size() == 24 && gaTexts.empty(), "low-detail scan keeps markers and suppresses labels");
    bool lbSection8WasFiltered = true;
    for (const LineCall& lrLine : gaLines)
        lbSection8WasFiltered = lbSection8WasFiltered &&
            !(Near(lrLine.mv2Start.x, 255.0f) && Near(lrLine.mv2Start.y, 515.0f));
    Check(lbSection8WasFiltered, "reciprocal distinct-link section is omitted");

    lDebug.mbHighDetailMap = false;
    gSectionsData.mpaSections = gaSections;
    gSectionsData.muNumSections = 0x10001u;
    guAccessibleSectionCount = 1;
    guSectionLookups = 0;
    guInvalidSectionLookups = 0;
    ResetDraws();
    lDebug.DrawDeadEndsInMap(&lDrawSink);
    Check(gaLines.size() == 2 && guSectionLookups == 2 && guInvalidSectionLookups == 0,
          "section count is masked to 16 bits before the outer scan");

    BrnAI::AIModule lAIModule{};
    lDebug.mpAIModule = &lAIModule;
    lDebug.mbDrawAISectionMap = true;
    lDebug.mbHighDetailMap = false;
    lDebug.mbDrawMasterRouteInMap = true;
    const BrnAI::AStarDistanceFunction laFunctions[] = {
        BrnAI::E_ASTAR_DISTANCE_EUCLIDEAN,
        BrnAI::E_ASTAR_DISTANCE_EUCLIDEAN_X_BIASED,
        BrnAI::E_ASTAR_DISTANCE_EUCLIDEAN_Y_BIASED,
        BrnAI::E_ASTAR_DISTANCE_MANHATTAN,
        BrnAI::E_ASTAR_DISTANCE_DIAGONAL,
    };
    const char* const lapcLabels[] = {
        "Default distance function: Euclidean",
        "Default distance function: EuclideanXBiased",
        "Default distance function: EuclideanYBiased",
        "Default distance function: Manhattan",
        "Default distance function: Diagonal",
    };
    for (u32 luFunction = 0; luFunction < 5; ++luFunction)
    {
        lAIModule.mRouteRequestManager.meDefaultAStarDistanceFunction = laFunctions[luFunction];
        ResetDraws();
        lDebug.RenderHUD(&lDrawSink);
        Check(gaTexts.size() == 1 && gaTexts[0].mText == lapcLabels[luFunction],
              "RenderHUD displays the matching original heuristic label");
        Check(gaTexts.size() == 1 && gaTexts[0].muColour == 0xFFFFFFFFu,
              "RenderHUD distance label uses original white packed color");
        Check(gauRouteColours.size() == 1 && gauRouteColours[0] == 0xFF0000FFu,
              "master route keeps its original packed route color");
    }

    lDebug.mbDrawMasterRouteInMap = false;
    ResetDraws();
    lDebug.RenderHUD(&lDrawSink);
    Check(gaTexts.empty() && gauRouteColours.empty(), "master-route toggle suppresses label and route");

    lDebug.mbDrawMasterRouteInMap = true;
    lDebug.mbDrawAISectionMap = false;
    ResetDraws();
    lDebug.RenderHUD(&lDrawSink);
    Check(gaTexts.empty() && gauRouteColours.empty(), "HUD master map gate suppresses every overlay");

    std::printf("RouteMapDrawHelpers: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
