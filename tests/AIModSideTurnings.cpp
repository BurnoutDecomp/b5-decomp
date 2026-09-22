// FX-AIMOD (crash parity 2026-09-22), G07-D2 + G07-D4: reset type 5.
//   ResetOnTrackManager::ResetAheadFromSideTurnings   @0x827909F0
//   ResetOnTrackManager::ScanForwardsAndAlongJunction @0x827852C0 (X360 export hole, ppcdis)
//   ResetOnTrackManager::InterpolatePositionFromAngle @0x82784FD8
// The runner extracts those three PRODUCTION bodies, the Strategies.cpp helper namespace,
// GetAICar (BrnResetOnTrackManager.cpp) and AICar::GetRandomNumber + its 0x8300D5D0 stream
// (BrnAICar_Update.cpp). The other strategies ResetAheadFromSideTurnings falls back to are
// observed fixtures, so each arm's arguments are measured; section / portal geometry is data.
//
// Stream after Construct: 0.0, 0.78315496, 0.19288969 ... -- ScanForwardsAndAlongJunction eats the
// first draw (0x82785304..0x82785374, value dead), so the fallback distance is
// 0.78315496 * 40 + 160 = 191.3262 (0x82790A58..0x82790AE8).
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAIPortal.h"
#include "GameSource/World/AI/Route/BrnRoute.h"
#include "GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.h"
#include "SharedClasses/AI/AISectionsResourceType.h"
#undef protected
#undef private
#include "GameSource/Math/BrnMathUtils.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cmath>
#include <cstdio>
#include <cstring>

static unsigned gAssertions = 0, gChecks = 0, gFailures = 0;
namespace CgsDev {
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { u64 gxMessageFilterFlags = 0; }
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcText, const char*, int) { ++gAssertions; std::fprintf(stderr, "assert: %s\n", lpcText); return 0; }
void* EndAssert() { return nullptr; }
} }
namespace CgsResource {
BaseResourcePtr::BaseResourcePtr() {}
BaseResourcePtr::~BaseResourcePtr() {}
}

// ---- geometry fixtures ----------------------------------------------------------------------
static Vector3 gaMiddles[16];
namespace BrnAI {
Vector3 AICar::GetPosition() const { return mPosition; }
Vector3 AICar::GetDirection() const { return mDirection; }
s32 AICar::GetNextRouteNodeIndex() const { return miNextRouteNodeIndex; }
const AISection* AISectionsData::GetAISection(u32 luIndex) const { return &mpaSections[luIndex]; }
Vector3 AISection::GetMiddle() const { return gaMiddles[mId]; }
const Portal* AISection::GetPortal(u8 luPortalIndex) const { return &mpaPortals[luPortalIndex]; }
u16 Portal::GetLinkSectionIndex() const { return mu16LinkSection; }
}

// ---- observed fixtures: the strategies ResetAheadFromSideTurnings falls back to --------------
struct Calls
{
    int miForward, miBackward, miConvert, miEnsure, miAway, miLookingBack;
    float mfForwardDistance, mfBackwardDistance, mfConvertSide, mfConvertDistance;
    int miBackwardType;
    bool mbForwardAnswer, mbBackwardAnswer, mbConvertAnswer, mbAwayAnswer, mbLookingBack;
};
static Calls gCalls;
static BrnAI::RouteNode gPrevNode, gNextNode;
namespace BrnAI {
bool ResetOnTrackManager::ScanForwardsAlongExtrapolatedRoute(const RouteNode*& lrpPrevNode, const RouteNode*& lrpNextNode, f32 lfResetDistance)
{
    ++gCalls.miForward; gCalls.mfForwardDistance = lfResetDistance;
    if (gCalls.mbForwardAnswer) { lrpPrevNode = &gPrevNode; lrpNextNode = &gNextNode; }
    return gCalls.mbForwardAnswer;
}
bool ResetOnTrackManager::ScanBackwardsAlongExtrapolatedRoute(const RouteNode*& lrpPrevNode, const RouteNode*& lrpNextNode, f32 lfResetDistance, EExtrapolateType leType)
{
    ++gCalls.miBackward; gCalls.mfBackwardDistance = lfResetDistance; gCalls.miBackwardType = static_cast<int>(leType);
    if (gCalls.mbBackwardAnswer) { lrpPrevNode = &gPrevNode; lrpNextNode = &gNextNode; }
    return gCalls.mbBackwardAnswer;
}
bool ResetOnTrackManager::ConvertNodesToPositionAndDirection(const RouteNode* lpPrev, const RouteNode* lpNext, f32 lfRoadSide, f32 lfResetDistance, ResetOnTrackCoords* lpResetData)
{
    ++gCalls.miConvert; gCalls.mfConvertSide = lfRoadSide; gCalls.mfConvertDistance = lfResetDistance;
    if (lpPrev != &gPrevNode || lpNext != &gNextNode) { ++gFailures; std::fprintf(stderr, "FAIL: Convert got foreign nodes\n"); }
    if (gCalls.mbConvertAnswer) { lpResetData->mPosition = Vector3{ 7.0f, 8.0f, 9.0f, 0.0f }; }
    return gCalls.mbConvertAnswer;
}
void ResetOnTrackManager::EnsureAIIsDrivingSameDirectionAsPlayer(const AICar*, ResetOnTrackCoords*) { ++gCalls.miEnsure; }
bool ResetOnTrackManager::ResetAwayFromPlayer(ResetOnTrackCoords*) { ++gCalls.miAway; return gCalls.mbAwayAnswer; }
bool ResetOnTrackManager::PlayerIsLookingBackwards() { ++gCalls.miLookingBack; return gCalls.mbLookingBack; }
}

#include "restored_methods.inc"

using namespace BrnAI;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass) { ++gFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}
static bool Near(Vector3 lA, f32 lfX, f32 lfY, f32 lfZ, f32 lfTol = 1e-3f)
{
    return std::fabs(lA.x - lfX) < lfTol && std::fabs(lA.y - lfY) < lfTol && std::fabs(lA.z - lfZ) < lfTol;
}

// ---- the world: sections, their portals, the player's car and route ------------------------
static AISection saSections[16];
static Portal saPortals[16][6];
static AISectionsData sData;
static AICar saCars[1];
static ResetOnTrackManager sManager;

static void ClearWorld()
{
    std::memset(saSections, 0, sizeof(saSections));
    std::memset(saPortals, 0, sizeof(saPortals));
    std::memset(gaMiddles, 0, sizeof(gaMiddles));
    for (u32 luSection = 0; luSection < 16; ++luSection)
    {
        saSections[luSection].mId = luSection;
        saSections[luSection].mpaPortals = saPortals[luSection];
    }
    sData.mpaSections = saSections;
    sData.muNumSections = 16;
    sManager.mpAISectionData.mpResourceMemory = &sData;
    sManager.mpaAICars = saCars;
    sManager.mePlayerGlobalRaceCarIndex = E_GLOBAL_RACE_CAR_INDEX_0;
    AICar& lrCar = saCars[0];
    lrCar.mPosition  = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
    lrCar.mDirection = Vector3{ 0.0f, 0.0f, 1.0f, 0.0f };
    lrCar.miNextRouteNodeIndex = 1;
    lrCar.muBestSectionIndex = 0;
    lrCar.muDefaultSectionIndex = 0;
    std::memset(&gCalls, 0, sizeof(gCalls));
    gNextNode.muSectionIndex = 4;
    BrnAI::gAICarRandom.Construct();   // what AICar::Construct / Reset do to the 0x8300D5D0 stream
}
static void SetMiddle(u32 luSection, f32 lfX, f32 lfZ, u8 lu8Flags = 0)
{
    gaMiddles[luSection] = Vector3{ lfX, 0.0f, lfZ, 0.0f };
    saSections[luSection].mx8Flags = lu8Flags;
}
static void AddPortal(u32 luFrom, u16 luTo, f32 lfX, f32 lfZ)
{
    Portal& lrPortal = saPortals[luFrom][saSections[luFrom].mu8NumPortals++];
    lrPortal.mPositionX = lfX;
    lrPortal.mPositionY = 0.0f;
    lrPortal.mPositionZ = lfZ;
    lrPortal.mu16LinkSection = luTo;
}
static void SetRoute(const u16* lpauSections, s32 liCount)
{
    BrnAI::Route* lpRoute = saCars[0].GetRoute();
    lpRoute->miNodeCount = liCount;
    for (s32 liNode = 0; liNode < liCount; ++liNode)
    {
        reinterpret_cast<RouteNode*>(&lpRoute->maNodes[liNode])->muSectionIndex = lpauSections[liNode];
    }
}

// The junction world. Player at the origin heading +Z; route S0 -> S1 (110 m, NOT a junction) ->
// S2 (150 m, JUNCTION) -> S3 -> S4. S2's side roads: S5 (runs back into the junction, dot 0.5547),
// S6 (dot 0.1961), S7 (dot 1.0 but a SHORTCUT). S5 leads out to S8, S8 to S10.
static void JunctionWorld(f32 lfEntranceX, f32 lfEntranceZ, f32 lfExitX, f32 lfExitZ)
{
    static const u16 kauRoute[] = { 0, 1, 2, 3, 4 };
    SetRoute(kauRoute, 5);
    SetMiddle(0, 0.0f, 0.0f);
    SetMiddle(1, 0.0f, 110.0f);
    SetMiddle(2, 0.0f, 150.0f, 0x10);
    SetMiddle(3, 0.0f, 200.0f);
    SetMiddle(4, 0.0f, 260.0f);
    SetMiddle(5, -60.0f, 110.0f);
    SetMiddle(6, 50.0f, 140.0f);
    SetMiddle(7, 0.0f, 100.0f, 0x01);
    SetMiddle(8, -40.0f, 260.0f);
    SetMiddle(9, -90.0f, 120.0f);
    SetMiddle(10, -130.0f, 250.0f);
    AddPortal(2, 1, 0.0f, 130.0f);
    AddPortal(2, 3, 0.0f, 175.0f);
    AddPortal(2, 6, 10.0f, 145.0f);           // dot 0.1961: taken first, then beaten by S5
    AddPortal(2, 5, lfEntranceX, lfEntranceZ);   // the entrance node
    AddPortal(2, 7, 0.0f, 125.0f);
    AddPortal(5, 2, -5.0f, 150.0f);
    AddPortal(5, 8, lfExitX, lfExitZ);           // first exit (S8 is the farther of S8 / S9)
    AddPortal(5, 9, -80.0f, 120.0f);
    AddPortal(8, 5, -20.0f, 190.0f);
    AddPortal(8, 10, -100.0f, 190.0f);           // second exit
}

int main()
{
    ResetOnTrackManager::ResetOnTrackCoords lCoords{};

    // (A) The junction join succeeds: two side-scan steps, the join point interpolated at cos 20.
    ClearWorld();
    JunctionWorld(-10.0f, 140.0f, -20.0f, 200.0f);
    const bool lbA = sManager.ResetAheadFromSideTurnings(&lCoords);
    Check(lbA, "A: type 5 answers from the side turning");
    Check(Near(lCoords.mPosition, -60.20707f, 0.0f, 194.97412f), "A: join point = lerp(entrance (-20,200), exit (-100,190), t 0.50259)");
    Check(Near(lCoords.mDirection, 0.9922779f, 0.0f, 0.12403474f, 1e-5f), "A: faces entrance - exit, normalised (towards the junction)");
    Check(lCoords.mpAISection == &saSections[10], "A: section = the last side section walked (S10)");
    Check(gCalls.miForward == 0 && gCalls.miBackward == 0 && gCalls.miConvert == 0 && gCalls.miEnsure == 0 && gCalls.miAway == 0,
          "A: no fallback strategy runs");
    Check(std::fabs(saCars[0].GetRandomNumber() - 0.78315496f) < 1e-6f, "A: the junction scan consumed exactly one draw");

    // (B) No junction on the route: the scan eats draw 1, the fallback distance is draw 2.
    ClearWorld();
    {
        static const u16 kauRoute[] = { 0, 1, 2, 3 };
        SetRoute(kauRoute, 4);
        SetMiddle(0, 0.0f, 10.0f, 0x10); SetMiddle(1, 0.0f, 40.0f, 0x10); SetMiddle(2, 0.0f, 70.0f, 0x10); SetMiddle(3, 0.0f, 99.0f, 0x10);
    }
    gCalls.mbForwardAnswer = true; gCalls.mbConvertAnswer = true;
    const bool lbB = sManager.ResetAheadFromSideTurnings(&lCoords);
    Check(lbB, "B: forward fallback answers");
    Check(gCalls.miForward == 1 && std::fabs(gCalls.mfForwardDistance - 191.3262f) < 1e-3f, "B: forward distance = draw 2 * 40 + 160 = 191.3262");
    Check(gCalls.miConvert == 1 && gCalls.mfConvertSide == 0.5f && std::fabs(gCalls.mfConvertDistance - 191.3262f) < 1e-3f, "B: Convert(prev, next, 0.5, 191.3262)");
    Check(gCalls.miEnsure == 1 && gCalls.miBackward == 0 && gCalls.miAway == 0 && gCalls.miLookingBack == 0, "B: Ensure runs, nothing else");
    Check(lCoords.mpAISection == &saSections[4], "B: section = GetAISection(next node's section)");

    // (C) Forward fails, the player is not looking back: 25 m behind, RoadRage extrapolation.
    ClearWorld();
    gCalls.mbBackwardAnswer = true; gCalls.mbConvertAnswer = true;
    const bool lbC = sManager.ResetAheadFromSideTurnings(&lCoords);
    Check(lbC && gCalls.miLookingBack == 1 && gCalls.miBackward == 1 && gCalls.mfBackwardDistance == -25.0f && gCalls.miBackwardType == 1,
          "C: ScanBackwards(-25.0, eExtrapolateType_RoadRage)");
    Check(gCalls.miConvert == 1 && gCalls.mfConvertDistance == -25.0f && gCalls.miEnsure == 1, "C: Convert at -25.0 and Ensure");

    // (D) Forward fails and the player IS looking back: no answer, nothing else runs.
    ClearWorld();
    gCalls.mbLookingBack = true; gCalls.mbBackwardAnswer = true;
    Check(!sManager.ResetAheadFromSideTurnings(&lCoords) && gCalls.miBackward == 0 && gCalls.miAway == 0 && gCalls.miConvert == 0,
          "D: looking back -> false before the backwards scan");

    // (E) Convert refuses: ResetAwayFromPlayer answers instead.
    ClearWorld();
    gCalls.mbForwardAnswer = true; gCalls.mbConvertAnswer = false; gCalls.mbAwayAnswer = true;
    Check(sManager.ResetAheadFromSideTurnings(&lCoords) && gCalls.miAway == 1 && gCalls.miEnsure == 0, "E: Convert failure -> ResetAwayFromPlayer's answer");

    // (F) Backwards refuses too: ResetAwayFromPlayer answers.
    ClearWorld();
    gCalls.mbAwayAnswer = false;
    Check(!sManager.ResetAheadFromSideTurnings(&lCoords) && gCalls.miBackward == 1 && gCalls.miAway == 1 && gCalls.miConvert == 0,
          "F: backwards failure -> ResetAwayFromPlayer's answer");

    // (G) No player section at all: false before any scan or draw.
    ClearWorld();
    saCars[0].muBestSectionIndex = 0x7FFF; saCars[0].muDefaultSectionIndex = 0x7FFF;
    Check(!sManager.ResetAheadFromSideTurnings(&lCoords) && gCalls.miForward == 0, "G: invalid sections -> false");
    Check(saCars[0].GetRandomNumber() == 0.0f, "G: no draw consumed");

    // (H) Pop-up prevented: the join point clamps to the entrance (-60, 40): 72 m away (>= 50,
    //     < 100) and 56 degrees off the heading (dot 0.5547 < cos 40).
    ClearWorld();
    JunctionWorld(-60.0f, 40.0f, -70.0f, 20.0f);
    lCoords.mDirection = Vector3{ 5.0f, 5.0f, 5.0f, 0.0f };
    Check(!sManager.ScanForwardsAndAlongJunction(&lCoords), "H: pop-up prevented -> false");
    Check(Near(lCoords.mPosition, -60.0f, 0.0f, 40.0f), "H: mPosition already written (t -1.375 -> the entrance)");
    Check(Near(lCoords.mDirection, 5.0f, 5.0f, 5.0f), "H: mDirection untouched");

    // (J) Too close: the entrance (-20, 20) is 28 m away (< 50).
    ClearWorld();
    JunctionWorld(-20.0f, 20.0f, -30.0f, 10.0f);
    Check(!sManager.ScanForwardsAndAlongJunction(&lCoords) && Near(lCoords.mPosition, -20.0f, 0.0f, 20.0f), "J: < 50 m -> false");

    // (I) The first node scanned is already a junction: no entry section -> false (draw still eaten).
    ClearWorld();
    JunctionWorld(-10.0f, 140.0f, -20.0f, 200.0f);
    saCars[0].miNextRouteNodeIndex = 0;
    SetMiddle(0, 0.0f, 150.0f, 0x10);
    Check(!sManager.ScanForwardsAndAlongJunction(&lCoords), "I: junction at the first node -> false");
    Check(std::fabs(saCars[0].GetRandomNumber() - 0.78315496f) < 1e-6f, "I: the dead draw still happened");

    // (K) InterpolatePositionFromAngle's clamps.
    const Vector2 lOrigin = { 0.0f, 0.0f, 0.0f, 0.0f };
    const Vector2 lHeading = { 0.0f, 1.0f, 0.0f, 0.0f };
    const f32 lfCos20 = 0.9396926f;
    Check(Near(sManager.InterpolatePositionFromAngle(lOrigin, lHeading, Vector3{ -10.0f, 0.0f, 100.0f, 0.0f }, Vector3{ -30.0f, 0.0f, 100.0f, 0.0f }, lfCos20), -30.0f, 0.0f, 100.0f),
          "K: t 1.487 > 1 -> the exit node");
    Check(Near(sManager.InterpolatePositionFromAngle(lOrigin, lHeading, Vector3{ 0.0f, 0.0f, 100.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 200.0f, 0.0f }, lfCos20), 0.0f, 0.0f, 200.0f),
          "K: equal bearings (range 0) -> the exit node");
    Check(Near(sManager.InterpolatePositionFromAngle(lOrigin, lHeading, Vector3{ 0.0f, 3.0f, 0.0f, 0.0f }, Vector3{ -5.0f, 0.0f, 50.0f, 0.0f }, lfCos20), -5.0f, 0.0f, 50.0f),
          "K: entrance on the player -> the exit node");
    Check(Near(sManager.InterpolatePositionFromAngle(lOrigin, lHeading, Vector3{ -60.0f, 0.0f, 40.0f, 0.0f }, Vector3{ -70.0f, 0.0f, 20.0f, 0.0f }, lfCos20), -60.0f, 0.0f, 40.0f),
          "K: t -1.375 < 0 -> the entrance node");
    Check(Near(sManager.InterpolatePositionFromAngle(lOrigin, lHeading, Vector3{ -20.0f, 0.0f, 200.0f, 0.0f }, Vector3{ -100.0f, 4.0f, 190.0f, 0.0f }, lfCos20), -60.20707f, 2.0103536f, 194.97412f),
          "K: lerp on all lanes at t 0.50259");

    Check(gAssertions == 0, "valid fixtures raise no assertion");
    std::printf("AIModSideTurnings: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
