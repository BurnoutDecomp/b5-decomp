// FX-TAILS-A item 8 (crash parity 2026-09-24): the ResetOnTrack Strategies TU logs under the console's filter bit.
// run_fxtailsa_rot_log_gates.py extracts the production ResetNearRoutelessPlayer @0x827844D8,
// DeterminePositionBetweenNodes @0x82785DC8, ConvertNodesToPositionAndDirection @0x82790300, the TU helper namespace
// and GetAICar. Every print below is `ld gxMessageFilterFlags ; clrldi r11,r11,63 ; cmpldi ; beq skip` on the console
// (bit 0), then `lwz r3, gpDebugPrint ; bl StrStreamBase::operator<<`:
//   "<AI> routeless fail\n"                                              0x8278450C..0x82784530
//   "<AI> Car tried to start ahead of player. Forced behind instead.\n"  0x82784738..0x8278475C
//   "<AI> Nodes in same place\n"                                         0x82785E9C..0x82785EC8
//   "Lerping " t " from " prev " to " next "\n"  (floats via "%f")       0x82785ED4..0x82785F38
//   "Converting nodes to position & direction\n"                         0x82790380..0x827903A4
// The tree printed the first, second, third and fifth whenever gpDebugPrint was set, and never printed the fourth.
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAIPortal.h"
#include "GameSource/World/AI/BrnAIBoundaryLine.h"
#include "GameSource/World/AI/Route/BrnRoute.h"
#include "GameSource/World/AI/Route/BrnRouteMapModule.h"
#include "GameSource/World/AI/RacingLine/BrnRacingLineGenerator.h"
#include "GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.h"
#include "SharedClasses/AI/AISectionsResourceType.h"
#include "GameSource/Math/BrnMathUtils.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

static std::string gPrinted;
namespace CgsDev {
namespace Log {
StrStreamBase& DebugPrint::operator<<(const char* lpcText) { gPrinted += (lpcText != nullptr) ? lpcText : "<null>"; return *this; }
static DebugPrint gCapture;
DebugPrint* gpDebugPrint = &gCapture;
}
namespace Message { u64 gxMessageFilterFlags = 0; }
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { return 0; }
void* EndAssert() { return nullptr; }
} }
namespace CgsResource {
BaseResourcePtr::BaseResourcePtr() {}
BaseResourcePtr::~BaseResourcePtr() {}
}

static BrnAI::BoundaryLine gaBoundaries[2];
static std::map<const BrnAI::BoundaryLine*, Vector2> gaInterp;
namespace BrnAI {
Vector3 AICar::GetPosition() const { return mPosition; }
Vector3 AICar::GetDirection() const { return mDirection; }
const AISection* AISectionsData::GetAISection(u32 luIndex) const { return &mpaSections[luIndex]; }
const Portal* AISection::GetPortal(u8 luPortalIndex) const { return &mpaPortals[luPortalIndex]; }
f32 Portal::GetPositionX() const { return mPositionX; }
f32 Portal::GetPositionY() const { return mPositionY; }
f32 Portal::GetPositionZ() const { return mPositionZ; }
const BoundaryLine* Portal::GetBoundaryLine(u8) const { return &gaBoundaries[static_cast<int>(mPositionX)]; }
void BoundaryLine::GetInterp(Vector2* lpv2Out, f32) const { *lpv2Out = gaInterp[this]; }
}

#include "restored_methods.inc"

using namespace BrnAI;

namespace
{
    unsigned guChecks = 0, guFailures = 0;
    void Check(bool lbPass, const char* lpcLabel)
    {
        ++guChecks;
        std::printf("%s  %s\n", lbPass ? "PASS" : "FAIL", lpcLabel);
        if (!lbPass) { ++guFailures; }
    }
    int Count(const char* lpcNeedle)
    {
        int n = 0;
        for (size_t p = gPrinted.find(lpcNeedle); p != std::string::npos; p = gPrinted.find(lpcNeedle, p + 1)) ++n;
        return n;
    }
    Vector2 V2(f32 lfX, f32 lfY) { Vector2 lV; lV.x = lfX; lV.y = lfY; lV.z = 0.0f; lV.w = 0.0f; return lV; }

    AISection      saSections[2];
    Portal         saPortals[2][2];
    AISectionsData sData;
    AICar          saCars[1];
    ResetOnTrackManager sManager;

    // Two one-portal sections; portal i's X doubles as its boundary-line index for the GetBoundaryLine stub.
    // The player car sits at the origin facing +Z.
    void World(u16 luRoutelessSection, Vector3 lPrev, Vector3 lNext)
    {
        std::memset(saSections, 0, sizeof(saSections));
        std::memset(saPortals, 0, sizeof(saPortals));
        for (int li = 0; li < 2; ++li)
        {
            saSections[li].mpaPortals = saPortals[li];
            saSections[li].mu8NumPortals = 2;
        }
        // section 0: portal 0 = lPrev, portal 1 = lNext (ResetNearRoutelessPlayer's start / end pair)
        saPortals[0][0].mPositionX = lPrev.x; saPortals[0][0].mPositionY = lPrev.y; saPortals[0][0].mPositionZ = lPrev.z;
        saPortals[0][1].mPositionX = lNext.x; saPortals[0][1].mPositionY = lNext.y; saPortals[0][1].mPositionZ = lNext.z;
        sData.mpaSections = saSections;
        sData.muNumSections = 2;
        sManager.mpAISectionData.mpResourceMemory = &sData;
        sManager.mpaAICars = saCars;
        sManager.mePlayerGlobalRaceCarIndex = E_GLOBAL_RACE_CAR_INDEX_0;
        AICar& lrCar = saCars[0];
        lrCar.mPosition  = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
        lrCar.mDirection = Vector3{ 0.0f, 0.0f, 1.0f, 0.0f };
        lrCar.muResetOnTrackSectionIndex = luRoutelessSection;
        lrCar.muResetOnTrackStartPortal  = 0;
        lrCar.muResetOnTrackEndPortal    = 1;
        gPrinted.clear();
    }

    void Both(const char* lpcLine, const char* lpcLabel0, const char* lpcLabel1, void (*lpfnRun)())
    {
        CgsDev::Message::gxMessageFilterFlags = 0xFFFFFFFEull;   // every bit but 0
        lpfnRun();
        Check(Count(lpcLine) == 0, lpcLabel0);
        CgsDev::Message::gxMessageFilterFlags = 1ull;             // bit 0 only
        lpfnRun();
        Check(Count(lpcLine) == 1, lpcLabel1);
    }

    void RunRouteless()
    {
        World(0x7FFFu, Vector3{ 0.0f, 0.0f, 40.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 60.0f, 0.0f });
        ResetOnTrackManager::ResetOnTrackCoords lCoords;
        std::memset(&lCoords, 0, sizeof(lCoords));
        sManager.ResetNearRoutelessPlayer(&lCoords);
    }
    void RunAhead()
    {
        World(0, Vector3{ 0.0f, 0.0f, 40.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 60.0f, 0.0f });
        ResetOnTrackManager::ResetOnTrackCoords lCoords;
        std::memset(&lCoords, 0, sizeof(lCoords));
        sManager.ResetNearRoutelessPlayer(&lCoords);
    }
    void RunSamePlace()
    {
        World(0, Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 0.0f, 0.0f });
        sManager.DeterminePositionBetweenNodes(Vector3{ 0.0f, 0.0f, 10.0f, 0.0f }, Vector3{ 5.0f, 0.0f, 10.0f, 0.0f },
                                               &saCars[0], 20.0f);
    }
    void RunLerp()
    {
        World(0, Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 0.0f, 0.0f });
        sManager.DeterminePositionBetweenNodes(Vector3{ 0.0f, 0.0f, 10.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 30.0f, 0.0f },
                                               &saCars[0], 20.0f);
    }
    void RunConvert()
    {
        World(0, Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }, Vector3{ 1.0f, 0.0f, 0.0f, 0.0f });
        gaInterp[&gaBoundaries[0]] = V2(0.0f, 10.0f);             // prev portal (X 0 -> boundary 0)
        gaInterp[&gaBoundaries[1]] = V2(0.0f, 30.0f);             // next portal (X 1 -> boundary 1)
        RouteNode lPrev, lNext;
        std::memset(&lPrev, 0, sizeof(lPrev));
        std::memset(&lNext, 0, sizeof(lNext));
        lPrev.muSectionIndex = 0; lPrev.muPad0x0E = 0;            // section 0, portal 0
        lNext.muSectionIndex = 0; lNext.muPad0x0E = 1;            // section 0, portal 1
        ResetOnTrackManager::ResetOnTrackCoords lCoords;
        std::memset(&lCoords, 0, sizeof(lCoords));
        sManager.ConvertNodesToPositionAndDirection(&lPrev, &lNext, 0.5f, 20.0f, &lCoords);
    }
}

int main()
{
    Both("<AI> routeless fail\n",
         "routeless fail: silent with filter bit 0 clear (0x8278451C beq)",
         "routeless fail: printed once with bit 0 set", RunRouteless);
    Both("<AI> Car tried to start ahead of player. Forced behind instead.\n",
         "start-ahead: silent with bit 0 clear (0x82784748 beq)",
         "start-ahead: printed once with bit 0 set", RunAhead);
    Both("<AI> Nodes in same place\n",
         "nodes in same place: silent with bit 0 clear (0x82785EB4 beq)",
         "nodes in same place: printed once with bit 0 set", RunSamePlace);
    Both("Lerping 0.500000 from 10.000000 to 30.000000\n",
         "lerping: silent with bit 0 clear (0x82785EDC beq)",
         "lerping: `Lerping t from prev to next` with %f floats, once with bit 0 set (was never printed)", RunLerp);
    Both("Converting nodes to position & direction\n",
         "converting nodes: silent with bit 0 clear (0x82790390 beq)",
         "converting nodes: printed once with bit 0 set", RunConvert);

    std::printf("FxTailsARotLogGates: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
