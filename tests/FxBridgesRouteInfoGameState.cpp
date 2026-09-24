// FX-BRIDGES (crash parity 2026-09-24) CC-11, the GAME-STATE half of the checkpoint-distance route pair: the
// PRODUCTION bodies of
//   GameStateModule::SendRouteRequestAction              @0x82381DC8 (BrnGameStateModule.cpp)
//   FindNearestAISectionWithoutPointMap                  (0x8267A588 reproduced there, AISectionsData.h:447)
//   ModeManager::HandleCheckpointDistanceResponse        @0x8231E6C8 (BrnModeManager_UpdateMode.cpp)
// extracted verbatim by run_fxbridges_route_info.py and compiled against the REAL LandmarkRouteRequestEvent /
// ModeManagerRouteInfoEvent (BrnGameEvents.h) and RequestRouteInfoAction (BrnGameActions.h), with small fixtures
// for the progression manager, the AI-sections resource, the action queue and the scoring system.
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
}

// ---- AI-sections fixtures: the public surface FindNearestAISectionWithoutPointMap reads --------------------------
namespace BrnAI
{
    struct AISection
    {
        Vector3 mMiddle;
        u8      mx8Flags;
        bool    IsShortcut() const   { return (mx8Flags & 0x01) != 0; }
        bool    IsAIShortcut() const { return (mx8Flags & 0x40) != 0; }
        Vector3 GetMiddle() const    { return mMiddle; }
    };
    struct AISectionsData
    {
        AISection maSections[8];
        u32       muNumSections;
        const AISection* GetAISection(u32 luSectionIndex) const
        {
            CGS_ASSERT(luSectionIndex < muNumSections, "luSectionIndex < muNumSections");
            return &maSections[luSectionIndex];
        }
    };
    namespace RouteMapModuleIO
    {
        enum RequestOwner { E_OWNER_AI = 0, E_OWNER_GUI = 1, E_OWNER_MODE_MANAGER = 2, E_OWNER_COUNT = 3 };
    }
}

namespace BrnGameState
{
namespace
{
#include "fxbridges_route_info_nearest.inc"
}

    struct ProgressionFixture
    {
        const BrnAI::AISectionsData* mpAISections;
        CgsID                        maLandmarks[4];
        u16                          mauLandmarkSections[4];
        mutable s32                  miLandmarkLookups;
        u16 FindLandmarkAISectionIndex(CgsID lLandmarkId) const
        {
            ++miLandmarkLookups;
            for (s32 i = 0; i < 4; ++i)
            {
                if (maLandmarks[i] == lLandmarkId) return mauLandmarkSections[i];
            }
            return 0xFFFF;
        }
        const BrnAI::AISectionsData* GetAISectionsData() const { return mpAISections; }
    };

    struct ActionQueueFixture
    {
        s32                                     miEvents;
        s32                                     miType;
        s32                                     miSize;
        GameStateModuleIO::RequestRouteInfoAction mLast;
        bool AddEvent(const CgsModule::Event* lpEvent, s32 liType, s32 liSize)
        {
            ++miEvents;
            miType = liType;
            miSize = liSize;
            std::memcpy(&mLast, lpEvent, sizeof(mLast));
            return true;
        }
    };

    struct GameStateModuleFixture
    {
        ProgressionFixture mProgressionManager;
        void SendRouteRequestAction(const GameStateModuleIO::LandmarkRouteRequestEvent* lpRouteRequestEvent,
                                    ActionQueueFixture*                                  lpOutputActionQueue,
                                    BrnAI::RouteMapModuleIO::RequestOwner                leRequestOwner)
#include "fxbridges_route_info_send.inc"
    };

    struct ScoringFixture
    {
        s32 miSetCalls;
        u32 muLastCheckpoint;
        f32 mfLastDistance;
        f32 mafDistances[8];
        s32 miFinishCalls;
        s32 miFinishArgument;
        void SetCheckpointDistances(u32 luCheckpoint, f32 lfDistance)
        {
            ++miSetCalls;
            muLastCheckpoint = luCheckpoint;
            mfLastDistance   = lfDistance;
            if (luCheckpoint < 8) mafDistances[luCheckpoint] = lfDistance;
        }
        void ProcessFinishDistances(s32 liNumCheckpoints) { ++miFinishCalls; miFinishArgument = liNumCheckpoints; }
    };

    struct ModeManagerFixture
    {
        u32            muNumLandmarks;
        u32            muNextDistanceRequestCheckpoint;
        bool           mbNeedToSendNextRequest;
        bool           mbIsCalculatingCheckpointDistances;
        ScoringFixture mScoringSystem;
        void HandleCheckpointDistanceResponse(const GameStateModuleIO::ModeManagerRouteInfoEvent* lpRouteInfoEvent)
#include "fxbridges_route_info_handle.inc"
    };
}

using namespace BrnGameState;
typedef GameStateModuleIO::LandmarkRouteRequestEvent RouteRequest;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("  FAIL %s\n", lpcName);
    }
    else
    {
        std::printf("  ok   %s\n", lpcName);
    }
}

static bool SameXYZ(const Vector3& a, f32 x, f32 y, f32 z) { return a.x == x && a.y == y && a.z == z; }

int main()
{
    // ---- the AI sections: 5 sections, one shortcut, one AI shortcut --------------------------------------------
    static BrnAI::AISectionsData lSections;
    std::memset(&lSections, 0, sizeof(lSections));
    lSections.muNumSections = 5;
    lSections.maSections[0].mMiddle = Vector3{ 100.0f, 0.0f, 100.0f, 0.0f };
    lSections.maSections[1].mMiddle = Vector3{ 10.0f, 0.0f, 10.0f, 0.0f };   lSections.maSections[1].mx8Flags = 0x01; // shortcut
    lSections.maSections[2].mMiddle = Vector3{ 12.0f, 0.0f, 12.0f, 0.0f };   lSections.maSections[2].mx8Flags = 0x40; // AI shortcut
    lSections.maSections[3].mMiddle = Vector3{ 20.0f, 5.0f, 20.0f, 0.0f };
    lSections.maSections[4].mMiddle = Vector3{ -50.0f, 0.0f, 0.0f, 0.0f };

    Check(FindNearestAISectionWithoutPointMap(&lSections, Vector3{ 11.0f, 0.0f, 11.0f, 0.0f }) == 3,
          "nearest section skips IsShortcut (bit 0x01) and IsAIShortcut (bit 0x40) sections (0x8267A60C)");
    Check(FindNearestAISectionWithoutPointMap(&lSections, Vector3{ 20.0f, 105.0f, 20.0f, 0.0f }) == 3,
          "the distance is 3D (vmsum3fp128 over x, y, z), not ground-plane");
    BrnAI::AISectionsData lTie;
    std::memset(&lTie, 0, sizeof(lTie));
    lTie.muNumSections = 2;
    lTie.maSections[0].mMiddle = Vector3{ 1.0f, 0.0f, 0.0f, 0.0f };
    lTie.maSections[1].mMiddle = Vector3{ -1.0f, 0.0f, 0.0f, 0.0f };
    Check(FindNearestAISectionWithoutPointMap(&lTie, Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }) == 0,
          "a tie keeps the FIRST section (strict `fcmpu ; bge` @0x8267A648)");
    BrnAI::AISectionsData lAllShortcuts;
    std::memset(&lAllShortcuts, 0, sizeof(lAllShortcuts));
    lAllShortcuts.muNumSections = 1;
    lAllShortcuts.maSections[0].mx8Flags = 0x41;
    Check(FindNearestAISectionWithoutPointMap(&lAllShortcuts, Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }) == 0x7FFF,
          "no eligible section -> 0x7FFF (`li r26, 0x7FFF` @0x8267A5AC)");

    // ---- SendRouteRequestAction: the mode manager's LANDMARK pair -----------------------------------------------
    GameStateModuleFixture lModule;
    std::memset(&lModule, 0, sizeof(lModule));
    lModule.mProgressionManager.mpAISections = &lSections;
    lModule.mProgressionManager.maLandmarks[0] = 0x1111;  lModule.mProgressionManager.mauLandmarkSections[0] = 41;
    lModule.mProgressionManager.maLandmarks[1] = 0x2222;  lModule.mProgressionManager.mauLandmarkSections[1] = 42;
    ActionQueueFixture lQueue;
    std::memset(&lQueue, 0, sizeof(lQueue));

    RouteRequest lRequest;
    std::memset(&lRequest, 0, sizeof(lRequest));
    lRequest.maPositions[0] = Vector3{ 1.0f, 2.0f, 3.0f, 0.0f };
    lRequest.maPositions[1] = Vector3{ 4.0f, 5.0f, 6.0f, 0.0f };
    lRequest.mePointTypes[0] = RouteRequest::E_ROUTE_END_POINT_TYPE_LANDMARK;
    lRequest.mePointTypes[1] = RouteRequest::E_ROUTE_END_POINT_TYPE_LANDMARK;
    lRequest.maLandmarkIDs[0] = 0x1111;
    lRequest.maLandmarkIDs[1] = 0x2222;
    lRequest.mu16EventID = 3;
    gAsserts = 0;
    lModule.SendRouteRequestAction(&lRequest, &lQueue, BrnAI::RouteMapModuleIO::E_OWNER_MODE_MANAGER);
    Check(lQueue.miEvents == 1 && lQueue.miType == 50 && lQueue.miSize == 0x30,
          "one action 50 (E_ACTION_REQUEST_ROUTE_INFO), 0x30 bytes (li r5, 0x32 ; li r6, 0x30 @0x82381FE0)");
    Check(lQueue.mLast.muStartSectionIndex == 41 && lQueue.mLast.muEndSectionIndex == 42
          && lModule.mProgressionManager.miLandmarkLookups == 2,
          "LANDMARK ends resolve through FindLandmarkAISectionIndex(landmark id) (@0x82381FAC)");
    Check(SameXYZ(lQueue.mLast.mStartPosition, 1.0f, 2.0f, 3.0f) && SameXYZ(lQueue.mLast.mEndPosition, 4.0f, 5.0f, 6.0f),
          "both ends' positions are copied into the action (+0x00 / +0x10)");
    Check(lQueue.mLast.muEventId == 3 && lQueue.mLast.miOwnerId == 2 && gAsserts == 0,
          "event id from +0x44, owner = the third argument (E_OWNER_MODE_MANAGER 2), no asserts");

    // ---- SendRouteRequestAction: JUNCTION / PLAYERPOS ends and an unknown type ---------------------------------
    lRequest.mePointTypes[0] = RouteRequest::E_ROUTE_END_POINT_TYPE_JUNCTION;
    lRequest.mePointTypes[1] = RouteRequest::E_ROUTE_END_POINT_TYPE_PLAYERPOS;
    lRequest.maPositions[0] = Vector3{ 99.0f, 0.0f, 99.0f, 0.0f };
    lRequest.maPositions[1] = Vector3{ -49.0f, 0.0f, 1.0f, 0.0f };
    lModule.mProgressionManager.miLandmarkLookups = 0;
    lModule.SendRouteRequestAction(&lRequest, &lQueue, BrnAI::RouteMapModuleIO::E_OWNER_GUI);
    Check(lQueue.mLast.muStartSectionIndex == 0 && lQueue.mLast.muEndSectionIndex == 4
          && lModule.mProgressionManager.miLandmarkLookups == 0 && lQueue.mLast.miOwnerId == 1,
          "JUNCTION / PLAYERPOS ends take the AI-sections nearest section by position (@0x82381F88)");
    lRequest.mePointTypes[0] = static_cast<RouteRequest::ERouteEndPointType>(7);
    gAsserts = 0;
    lModule.SendRouteRequestAction(&lRequest, &lQueue, BrnAI::RouteMapModuleIO::E_OWNER_GUI);
    Check(gAsserts == 1 && lQueue.mLast.muStartSectionIndex == 0,
          "an unknown point type asserts once (line 0x1731) and still takes the position lookup");

    // ---- HandleCheckpointDistanceResponse -------------------------------------------------------------------
    ModeManagerFixture lModes;
    std::memset(&lModes, 0, sizeof(lModes));
    lModes.muNumLandmarks = 4;                        // three legs: 0-1, 1-2, 2-3
    lModes.mbNeedToSendNextRequest = true;
    lModes.mbIsCalculatingCheckpointDistances = true;
    GameStateModuleIO::ModeManagerRouteInfoEvent lInfo;
    gAsserts = 0;

    lInfo.miEventId = 1; lInfo.mfRouteDistance = 999.0f;
    lModes.HandleCheckpointDistanceResponse(&lInfo);
    Check(lModes.mScoringSystem.miSetCalls == 0 && lModes.muNextDistanceRequestCheckpoint == 0,
          "an answer for another pair is ignored (`cmpw` against the next checkpoint @0x8231E750)");

    lInfo.miEventId = 0; lInfo.mfRouteDistance = 250.5f;
    lModes.HandleCheckpointDistanceResponse(&lInfo);
    Check(lModes.mScoringSystem.miSetCalls == 1 && lModes.mScoringSystem.muLastCheckpoint == 0
          && lModes.mScoringSystem.mfLastDistance == 250.5f && lModes.muNextDistanceRequestCheckpoint == 1
          && lModes.mbNeedToSendNextRequest && lModes.mbIsCalculatingCheckpointDistances,
          "leg 0 answered: SetCheckpointDistances(0, distance), next = 1, the pump asks again (+0x8C0C = 1)");

    lInfo.miEventId = 1; lInfo.mfRouteDistance = 300.0f;
    lModes.HandleCheckpointDistanceResponse(&lInfo);
    lInfo.miEventId = 1; lInfo.mfRouteDistance = 123.0f;   // a duplicate answer for an already-taken leg
    lModes.HandleCheckpointDistanceResponse(&lInfo);
    Check(lModes.mScoringSystem.miSetCalls == 2 && lModes.mScoringSystem.mafDistances[1] == 300.0f
          && lModes.muNextDistanceRequestCheckpoint == 2,
          "each leg is taken once; a later duplicate for it is ignored");

    lInfo.miEventId = 2; lInfo.mfRouteDistance = 75.0f;
    lModes.HandleCheckpointDistanceResponse(&lInfo);
    Check(lModes.mScoringSystem.miFinishCalls == 1 && lModes.mScoringSystem.miFinishArgument == 4
          && !lModes.mbNeedToSendNextRequest && !lModes.mbIsCalculatingCheckpointDistances
          && lModes.muNextDistanceRequestCheckpoint == 3,
          "last leg: ProcessFinishDistances(muNumLandmarks) and the pump stops (+0x8C0D = 0, +0x8C0C = 0)");
    Check(gAsserts == 0, "no asserts on a well-formed sequence (lines 2331 / 2332)");

    std::printf("FxBridgesRouteInfoGameState: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
