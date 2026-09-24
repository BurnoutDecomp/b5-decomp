// FX-CRASHSND2 (crash parity 2026-09-24, item 2): the collision-sound scrape legs against the ARTIST
// machine code.
//
//   CollisionStateManager::UpdateScrapes              0x826D3F50     DWARF cpp:2678
//   CollisionStateManager::UpdateScrapeHistory        0x826BEB98     DWARF cpp:2916
//   CollisionStateManager::FindOldestScrapeInHistory  sub_82688A58   DWARF cpp:2881
//   CollisionStateManager::FindEntity                 0x826A0398     DWARF cpp:3000
//   CollisionStateManager::UniqueScrape               0x82688B20     DWARF cpp:2975
//
// run_fxcrashsnd2_scrapes.py extracts the PRODUCTION scrape region of BrnCollisionStateManager.cpp
// (the banner through UpdateScrapes), FindInScrapeHistory, the entity-id helpers, GenericEntity
// (BrnCollisionStateManager.h), ScrapeInfo::operator== / UpdateHistory (BrnCollisionDataStructures.cpp),
// CollisionState::Attach (BrnCollisionState.cpp), the base StateManager::GetFreeState
// (CgsStateManager.cpp) and GetTrafficEntityIndex (BrnTrafficSoundInterfaces.cpp), and compiles them
// here against a fixture manager / state list / sound input. InputCollision / OutputCollision /
// ScrapeInfo / FrameInformation and the traffic interface are the real headers.
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/BurnoutConstants.h"                 // EActiveRaceCarIndex
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Sound/Collision/BrnCollisionDataStructures.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficSoundInterfaces.h"

#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

static unsigned guAsserts = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++guAsserts; return 0; }
void* EndAssert() { return nullptr; }
} }

// The [DIAG] lines go to a string so a check can see them.
static std::string gDiag;
namespace CgsDev { namespace Log {
struct DebugPrintFixture
{
    DebugPrintFixture& operator<<(const char* lpcText) { gDiag += lpcText; return *this; }
};
DebugPrintFixture gDebugPrint;
DebugPrintFixture* gpDebugPrint = &gDebugPrint;
} }

// ---- the sound input FindEntity reads --------------------------------------------------------------
namespace BrnPhysics { namespace Vehicle {
struct RaceCarState { Matrix44Affine mTransform; Vector3 mLinearVelocity; bool mbCrashing; };
} }
namespace BrnWorld { namespace RaceCarEntityModuleIO {
struct RCEntityActiveRaceCarOutputInterface
{
    bool mabActive[8] = {};
    s32 miPlayer = 0;
    bool mabNoState[8] = {};
    BrnPhysics::Vehicle::RaceCarState maStates[8] = {};
    mutable int miActiveQueries = 0;
    bool IsRaceCarActive(EActiveRaceCarIndex le) const { ++miActiveQueries; return mabActive[le]; }
    EActiveRaceCarIndex GetPlayerActiveRaceCarIndex() const { return static_cast<EActiveRaceCarIndex>(miPlayer); }
    const BrnPhysics::Vehicle::RaceCarState* GetRaceCarState(EActiveRaceCarIndex le) const
    {
        return mabNoState[le] ? nullptr : &maStates[le];
    }
};
} }
namespace BrnSound { namespace Module { namespace Io {
struct RootInputBuffer
{
    BrnTraffic::BrnTrafficIO::TrafficSoundOutputInterface mTraffic = {};
    BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface mVehicles;
    const BrnTraffic::BrnTrafficIO::TrafficSoundOutputInterface& GetTrafficOutputInterface() const { return mTraffic; }
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* GetVehicleInterface() const { return &mVehicles; }
};
} } }

namespace BrnTraffic { namespace BrnTrafficIO {
#include "fxcrashsnd2_get_traffic_entity_index.inc"
} }

// ---- the state machine the scrape legs walk ------------------------------------------------------
namespace CgsSound { namespace Logic {
class Module
{
public:
    virtual ~Module() {}
};

class State
{
public:
    virtual ~State() {}
    virtual void Attach(void* apvAttachment) { mpvAttachment = apvAttachment; mbIsAttached = true; }
    State* GetNextState() const { return mpNextState; }
    bool IsAttached() const { return mbIsAttached; }
    void* mpvAttachment = nullptr;
    State* mpNextState = nullptr;
    bool mbIsAttached = false;
    f32 mfCurTime = 0.0f;
};

class StateManager
{
public:
    virtual ~StateManager() {}
    virtual State* GetFreeState(void* apvAttachment);
    State* GetHeadState() const { return mpHeadState; }
    Module* GetLogicModule() const { return mpLogicModule; }
    f32 mfCurrentTime = 0.0f;
    State* mpHeadState = nullptr;
    Module* mpLogicModule = nullptr;
};

#include "fxcrashsnd2_base_get_free_state.inc"
} }

namespace BrnSound { namespace Module {
class SoundLogicModule : public CgsSound::Logic::Module
{
public:
    Io::RootInputBuffer* mpInput = nullptr;
    Io::RootInputBuffer* GetBrnInputStructure() { return mpInput; }
};
} }

namespace BrnSound { namespace Logic { namespace Collision {

#include "fxcrashsnd2_scrape_info_bodies.inc"

class CollisionState : public CgsSound::Logic::State
{
public:
    enum ELifetime { E_NONE = 0, E_COLLISION = 1, E_SCRAPE = 2 };
    void Attach(void* apvAttachment) override;
    void SetLifetime(ELifetime aeLifetime) { meLifetime.Update(aeLifetime); }
    const OutputCollision& GetOutputCollision() const { return mOutputCollision; }
    OutputCollision& GetOutputCollision() { return mOutputCollision; }
    CgsSound::Utils::DataPoint<ELifetime> meLifetime;
    OutputCollision mOutputCollision;
    f32 mfTimeWeAttached = 0.0f;
};

#include "fxcrashsnd2_collision_state_attach.inc"

#include "fxcrashsnd2_generic_entity.inc"

namespace
{
bool CollisionAudioDiagEnabled() { return true; }
#include "fxcrashsnd2_entity_helpers.inc"
}

class CollisionStateManager : public CgsSound::Logic::StateManager
{
public:
    enum { E_MAX_SCRAPE_HISTORY = 16 };

    // The collision manager's own GetFreeState steals the lowest-priority state for an incoming
    // collision; UpdateScrapes must not reach it (the console calls the BASE one directly).
    CgsSound::Logic::State* GetFreeState(void*) override { ++miOverrideCalls; return nullptr; }

    BrnSound::Logic::Collision::ScrapeInfo* FindInScrapeHistory(const BrnSound::Logic::Collision::ScrapeInfo& rScrapeInfo);
    void UpdateScrapes(const BrnSound::Logic::FrameInformation& lrFrame);
    BrnSound::Logic::Collision::ScrapeInfo* FindOldestScrapeInHistory();
    bool FindEntity(const EntityId& lEntityId, GenericEntity& lEntity) const;
    bool UniqueScrape(const BrnSound::Logic::Collision::ScrapeInfo& lScrapeInfo,
                      const InputCollision* const* lapCollisions, u32 lu32Count) const;
    void UpdateScrapeHistory(const BrnSound::Logic::FrameInformation& lrFrame);

    InputCollision maInputCollision[64];
    u32 mu32InputCollisionCount = 0;
    BrnSound::Logic::FrameInformation mFrameInformation;
    BrnSound::Logic::Collision::ScrapeInfo maScrapeHistory[E_MAX_SCRAPE_HISTORY];
    int miOverrideCalls = 0;
};

#include "fxcrashsnd2_find_in_scrape_history.inc"

#include "fxcrashsnd2_scrapes_region.inc"

} } }

// ---- checks ----------------------------------------------------------------------------------------
using namespace BrnSound::Logic;
using namespace BrnSound::Logic::Collision;
namespace eO = AttribSys::Enums::eOrientation;

static int giChecks = 0;
static int giFailures = 0;
static void Check(bool lbCondition, const char* lpcLabel)
{
    ++giChecks;
    if (!lbCondition)
    {
        ++giFailures;
        std::printf("FAIL  %s\n", lpcLabel);
    }
}

static const f32 KF_NAN = std::numeric_limits<f32>::quiet_NaN();

static EntityId Id(u32 luOwner, u32 luIndex) { EntityId l; l.muValue = (luOwner << 24) | (luIndex << 10); return l; }
static Vector3 V(f32 x, f32 y, f32 z, f32 w = 0.0f) { Vector3 l = { x, y, z, w }; return l; }
static Matrix44Affine At(Vector3 lAt, Vector3 lPos)
{
    Matrix44Affine l; l.xAxis = V(1, 0, 0); l.yAxis = V(0, 1, 0); l.zAxis = lAt; l.wAxis = lPos; return l;
}

static ScrapeInfo Scrape(EntityId a, EntityId b, eO::eOrientation le, f32 lfStamp, f32 lfIntensity = 1.0f)
{
    ScrapeInfo l;
    l.mEntityIdA = a; l.mEntityIdB = b; l.meOrientation = le; l.mfTimeStamp = lfStamp;
    l.mfIntensity = lfIntensity; l.mbValid = true;
    return l;
}

static InputCollision Input(const ScrapeInfo& lScrape, Vector3 lPosition)
{
    InputCollision l;
    l.mScrapeInfo = lScrape;
    l.mPosition = lPosition;
    l.maEntityID[0] = lScrape.mEntityIdA;
    l.maEntityID[1] = lScrape.mEntityIdB;
    return l;
}

// One manager + input + three collision states on the manager's list.
struct World
{
    BrnSound::Module::Io::RootInputBuffer mInput;
    BrnSound::Module::SoundLogicModule mModule;
    CollisionState maStates[3];
    CollisionStateManager mMgr;

    World()
    {
        mModule.mpInput = &mInput;
        mMgr.mpLogicModule = &mModule;
        mMgr.mpHeadState = &maStates[0];
        maStates[0].mpNextState = &maStates[1];
        maStates[1].mpNextState = &maStates[2];
        mMgr.mFrameInformation.mPlayerTransform = At(V(0, 0, 1), V(0, 0, 0));
        // Race car 0 is the player's, car 1 an AI car; both active.
        mInput.mVehicles.mabActive[0] = mInput.mVehicles.mabActive[1] = true;
        mInput.mVehicles.miPlayer = 0;
        mInput.mVehicles.maStates[0].mTransform = At(V(0, 0, 1), V(1, 2, 3));
        mInput.mVehicles.maStates[0].mLinearVelocity = V(10, 0, 0);
        mInput.mVehicles.maStates[1].mTransform = At(V(0, 0, 1), V(4, 5, 6));
        mInput.mVehicles.maStates[1].mLinearVelocity = V(0, 0, 20);
    }

    void AddTraffic(u16 lu16Index, bool lbCrashed, f32 lfSpeed, Vector3 lAt, Vector3 lPos)
    {
        BrnTraffic::BrnTrafficIO::TrafficSoundEntity& lr = mInput.mTraffic.maActiveEntityList[mInput.mTraffic.mu16EntityCount++];
        lr = BrnTraffic::BrnTrafficIO::TrafficSoundEntity();
        lr.mu16EntityIndex = lu16Index;
        lr.mbIsCrashed = lbCrashed;
        lr.mfSpeed = lfSpeed;
        lr.mLocalTransform = At(lAt, lPos);
    }

    void SetInputs(const InputCollision* lpInputs, u32 lu32Count)
    {
        for (u32 i = 0; i < lu32Count; ++i)
            mMgr.maInputCollision[i] = lpInputs[i];
        mMgr.mu32InputCollisionCount = lu32Count;
    }
};

static u32 ValidHistory(const CollisionStateManager& lrMgr)
{
    u32 lu = 0;
    for (u32 i = 0; i < 16; ++i)
        lu += lrMgr.maScrapeHistory[i].mbValid ? 1u : 0u;
    return lu;
}

int main()
{
    const EntityId kCar0 = Id(1, 0), kCar1 = Id(1, 1), kCar2 = Id(1, 2), kWorld = Id(0, 0);
    const EntityId kTraffic7 = Id(2, 7), kTraffic8 = Id(2, 8), kProp = Id(3, 4);

    // ---- FindOldestScrapeInHistory (sub_82688A58) -------------------------------------------------
    {
        World w;
        Check(w.mMgr.FindOldestScrapeInHistory() == &w.mMgr.maScrapeHistory[0],
              "FindOldest: an empty history gives slot 0");
        for (u32 i = 0; i < 5; ++i)
            w.mMgr.maScrapeHistory[i] = Scrape(kCar0, kWorld, eO::Side, 1.0f);
        Check(w.mMgr.FindOldestScrapeInHistory() == &w.mMgr.maScrapeHistory[5],
              "FindOldest: the first invalid slot wins before any time stamp is compared");
        for (u32 i = 0; i < 16; ++i)
            w.mMgr.maScrapeHistory[i] = Scrape(kCar0, kWorld, eO::Side, 5.0f + static_cast<f32>(i));
        w.mMgr.maScrapeHistory[9].mfTimeStamp = 2.0f;
        Check(w.mMgr.FindOldestScrapeInHistory() == &w.mMgr.maScrapeHistory[9],
              "FindOldest: a full history gives the smallest time stamp");
        w.mMgr.maScrapeHistory[4].mfTimeStamp = 2.0f;
        Check(w.mMgr.FindOldestScrapeInHistory() == &w.mMgr.maScrapeHistory[4],
              "FindOldest: on a tie the earlier slot wins (strict less-than, `fcmpu ; bge`)");
        w.mMgr.maScrapeHistory[1].mfTimeStamp = KF_NAN;
        Check(w.mMgr.FindOldestScrapeInHistory() == &w.mMgr.maScrapeHistory[4],
              "FindOldest: a NaN stamp is never the oldest");
        Check(guAsserts == 0, "FindOldest: no assert while a slot is found");
    }

    // ---- UniqueScrape (0x82688B20) ----------------------------------------------------------------
    {
        World w;
        const InputCollision lA = Input(Scrape(kCar0, kWorld, eO::Side, 1.0f), V(0, 0, 0));
        const InputCollision lB = Input(Scrape(kCar0, kCar1, eO::Front, 1.0f), V(0, 0, 0));
        const InputCollision* lap[2] = { &lA, &lB };
        Check(w.mMgr.UniqueScrape(Scrape(kCar0, kWorld, eO::Side, 9.0f), lap, 0),
              "UniqueScrape: an empty list is unique");
        Check(!w.mMgr.UniqueScrape(Scrape(kCar0, kWorld, eO::Side, 9.0f), lap, 2),
              "UniqueScrape: the same pair and orientation is not unique (stamp ignored)");
        Check(!w.mMgr.UniqueScrape(Scrape(kCar1, kCar0, eO::Front, 9.0f), lap, 2),
              "UniqueScrape: the reversed pair is the same scrape (operator== is unordered)");
        Check(w.mMgr.UniqueScrape(Scrape(kCar0, kWorld, eO::Rear, 9.0f), lap, 2),
              "UniqueScrape: another orientation is another scrape");
        Check(w.mMgr.UniqueScrape(Scrape(kCar0, kWorld, eO::Side, 9.0f), lap + 1, 1),
              "UniqueScrape: only the first lu32Count entries are compared");
    }

    // ---- FindEntity (0x826A0398) ------------------------------------------------------------------
    {
        World w;
        w.mInput.mVehicles.maStates[0].mbCrashing = true;
        w.AddTraffic(7, true, 4.0f, V(0, 0.6f, 0.8f, 0.5f), V(30, 1, 2));

        GenericEntity lSeed;
        lSeed.mPosition = V(-1, -1, -1, -1); lSeed.mVelocity = V(-2, -2, -2, -2);
        lSeed.mbCrashing = true; lSeed.mbPlayer = true; lSeed.mbWorld = false;

        GenericEntity le = lSeed;
        Check(w.mMgr.FindEntity(kWorld, le) && le.mbWorld && le.mPosition.x == -1.0f &&
                  le.mVelocity.x == -2.0f && le.mbPlayer && le.mbCrashing,
              "FindEntity: the world sets mbWorld only and leaves the rest as it was");

        le = lSeed;
        le.mbPlayer = false;
        Check(w.mMgr.FindEntity(kCar0, le) && le.mbCrashing && le.mbPlayer && !le.mbWorld &&
                  le.mPosition.x == 1.0f && le.mPosition.z == 3.0f && le.mVelocity.x == 10.0f,
              "FindEntity: an active race car gives its crash flag, transform position, linear velocity and whether it "
              "is the player's");

        le = lSeed;
        Check(w.mMgr.FindEntity(kCar1, le) && !le.mbCrashing && !le.mbPlayer && le.mVelocity.z == 20.0f,
              "FindEntity: another race car is not the player (mbPlayer written false)");

        le = lSeed;
        Check(!w.mMgr.FindEntity(kCar2, le) && le.mPosition.x == -1.0f && le.mbPlayer,
              "FindEntity: an inactive race car is not found and nothing is written");

        w.mInput.mVehicles.mabActive[2] = true;
        w.mInput.mVehicles.mabNoState[2] = true;
        Check(!w.mMgr.FindEntity(kCar2, le), "FindEntity: an active race car without a state is not found");

        le = lSeed;
        Check(w.mMgr.FindEntity(kTraffic7, le) && le.mbCrashing && le.mbPlayer && !le.mbWorld &&
                  le.mPosition.x == 30.0f && le.mPosition.y == 1.0f,
              "FindEntity: a traffic car gives mbIsCrashed and its position, and leaves mbPlayer as it was");
        Check(le.mVelocity.x == 0.0f && std::fabs(le.mVelocity.y - 2.4f) < 1e-6f &&
                  std::fabs(le.mVelocity.z - 3.2f) < 1e-6f && le.mVelocity.w == 2.0f,
              "FindEntity: a traffic car's velocity is its At row times mfSpeed, all four lanes (vmulfp128 by the splat)");

        le = lSeed;
        Check(!w.mMgr.FindEntity(kTraffic8, le) && le.mPosition.x == -1.0f,
              "FindEntity: a traffic car the sound output does not hold is not found");
        Check(!w.mMgr.FindEntity(kProp, le) && !w.mMgr.FindEntity(Id(5, 0), le) && le.mPosition.x == -1.0f,
              "FindEntity: a prop or any higher owner is not found");
    }

    // ---- UpdateScrapeHistory (0x826BEB98) ---------------------------------------------------------
    {
        World w;
        FrameInformation lFrame;
        w.mMgr.mfCurrentTime = 10.0f;
        w.mInput.mVehicles.maStates[0].mbCrashing = true;
        w.AddTraffic(7, true, 5.0f, V(0, 0, 1), V(0, 0, 0));

        w.mMgr.maScrapeHistory[0] = Scrape(kCar0, kWorld, eO::Side, 9.9f);        // fresh: player crashing
        w.mMgr.maScrapeHistory[1] = Scrape(kTraffic7, kWorld, eO::Front, 9.9f);   // A = crashed traffic
        w.mMgr.maScrapeHistory[2] = Scrape(kCar1, kWorld, eO::Side, 1.0f);        // long stale
        w.mMgr.maScrapeHistory[3] = Scrape(kCar1, kWorld, eO::Rear, KF_NAN);      // NaN stamp
        w.mMgr.maScrapeHistory[4] = Scrape(kCar2, kWorld, eO::Roof, 9.95f);       // car 2 inactive
        w.mMgr.maScrapeHistory[5] = Scrape(kCar1, kTraffic7, eO::Side, 9.95f);    // AI car vs traffic
        w.mMgr.maScrapeHistory[6] = Scrape(kCar1, kWorld, eO::Side, 9.7f);        // 0.3 old
        w.mMgr.maScrapeHistory[6].mbValid = false;                                // and already invalid

        w.mMgr.UpdateScrapeHistory(lFrame);

        const ScrapeInfo* h = w.mMgr.maScrapeHistory;
        Check(h[0].mbValid && h[0].mbCrashing && h[0].mRelativeVelocity.x == -10.0f &&
                  h[0].mRelativeVelocity.z == 0.0f,
              "History: a fresh slot stays; relative velocity = B's velocity - A's; the crashing player makes it crashing");
        Check(h[1].mbValid && h[1].mbCrashing,
              "History: the two entity locals are built ONCE -- traffic A keeps the player flag car 0 left, so a crashed "
              "traffic car counts as a crashing player (the console's quirk)");
        Check(h[1].mRelativeVelocity.z == -5.0f,
              "History: traffic A's velocity is its At row times its speed; the world B keeps the previous (zero) velocity");
        Check(!h[2].mbValid, "History: a slot older than KF_SCRAPE_IDLE_TIME is dropped");
        Check(!h[3].mbValid, "History: a NaN time stamp is dropped (a NaN age fails the vcmpgtfp.)");
        Check(!h[4].mbValid, "History: a side FindEntity cannot find drops the slot");
        Check(h[5].mbValid && !h[5].mbCrashing && h[5].mRelativeVelocity.z == 5.0f - 20.0f,
              "History: B found on its own lookup; neither side a crashing player");
        Check(!h[6].mbValid && h[6].mRelativeVelocity.x == 0.0f, "History: an invalid slot is skipped untouched");
        Check(guAsserts == 0, "History: no assert on finite velocities");

        // The cached pair: a slot whose A is the last slot's A reuses the last lookup.
        World w2;
        w2.mMgr.mfCurrentTime = 10.0f;
        w2.mMgr.maScrapeHistory[0] = Scrape(kCar1, kWorld, eO::Side, 9.9f);
        w2.mMgr.maScrapeHistory[1] = Scrape(kCar1, kWorld, eO::Rear, 9.9f);
        w2.mMgr.UpdateScrapeHistory(lFrame);
        Check(w2.mMgr.maScrapeHistory[1].mbValid && w2.mMgr.maScrapeHistory[1].mRelativeVelocity.z == -20.0f &&
                  w2.mInput.mVehicles.miActiveQueries == 1,
              "History: a repeated pair skips FindEntity and reuses the locals (one lookup for two slots)");

        // The idle time is a strict bound: exactly 0.2 s old is dropped, just under is kept.
        World w3;
        w3.mMgr.mfCurrentTime = 0.2f;
        w3.mMgr.maScrapeHistory[0] = Scrape(kCar1, kWorld, eO::Side, 0.0f);
        w3.mMgr.maScrapeHistory[1] = Scrape(kCar1, kWorld, eO::Rear, 1.0e-6f);
        w3.mMgr.UpdateScrapeHistory(lFrame);
        Check(!w3.mMgr.maScrapeHistory[0].mbValid && w3.mMgr.maScrapeHistory[1].mbValid,
              "History: exactly KF_SCRAPE_IDLE_TIME (0.2) old is dropped (`vcmpgtfp.` 0.2 > age), a hair younger is kept");
    }

    // ---- UpdateScrapes (0x826D3F50) ---------------------------------------------------------------
    {
        // Frame 1: nothing in the history -- a scrape near the player is recorded once; a far one,
        // one without a scrape and a duplicate are not.
        World w;
        w.mMgr.mfCurrentTime = 10.0f;
        FrameInformation& lrFrame = w.mMgr.mFrameInformation;
        const ScrapeInfo kWall = Scrape(kCar0, kWorld, eO::Side, 10.0f, 3.0f);
        InputCollision laInputs[4] = {
            Input(kWall, V(10, 0, 0)),
            Input(kWall, V(11, 0, 0)),
            Input(Scrape(kCar1, kWorld, eO::Rear, 10.0f), V(60, 0, 0)),
            Input(Scrape(kCar1, kWorld, eO::Roof, 10.0f), V(1, 0, 0)),
        };
        laInputs[3].mScrapeInfo.mbValid = false;
        w.SetInputs(laInputs, 4);
        w.mMgr.UpdateScrapes(lrFrame);
        Check(ValidHistory(w.mMgr) == 1 && w.mMgr.maScrapeHistory[0] == kWall &&
                  w.mMgr.maScrapeHistory[0].mfIntensity == 3.0f,
              "Scrapes: a new scrape near the player enters the history once (UniqueScrape keeps the duplicate out)");
        Check(!w.mMgr.maInputCollision[0].mbCull && !w.mMgr.maInputCollision[1].mbCull,
              "Scrapes: a new scrape's collision is not eaten");
        Check(!w.maStates[0].mbIsAttached && !w.maStates[1].mbIsAttached && !w.maStates[2].mbIsAttached,
              "Scrapes: a new scrape does not take a state");
        Check(gDiag.find("[collision-audio] scrape history A=1:0 B=0:0 orient=2 ") != std::string::npos,
              "Scrapes: the [DIAG] history line names the pair");

        // Frame 2 (0.05 s later): the same scrape continues. The collision is eaten, the history
        // takes its stamp and intensity, and -- no state holding it -- a free state takes it with
        // the SCRAPE lifetime through the BASE GetFreeState.
        w.mMgr.mfCurrentTime = 10.05f;
        w.mInput.mVehicles.maStates[0].mbCrashing = true;   // UpdateScrapeHistory: crashing, velocity -10 x
        InputCollision laFrame2[2] = {
            Input(Scrape(kCar0, kWorld, eO::Side, 10.05f, 5.0f), V(10, 0, 0)),
            Input(Scrape(kWorld, kCar0, eO::Side, 10.05f, 6.0f), V(12, 0, 0)),
        };
        w.maStates[0].mbIsAttached = true;   // a busy state (another collision) -- skipped by the base
        w.maStates[0].mOutputCollision.mScrapeInfo = Scrape(kCar1, kCar0, eO::Front, 9.0f);
        w.maStates[0].meLifetime.Flush(CollisionState::E_COLLISION);
        w.SetInputs(laFrame2, 2);
        w.mMgr.UpdateScrapes(lrFrame);
        const InputCollision& lr0 = w.mMgr.maInputCollision[0];
        const InputCollision& lr1 = w.mMgr.maInputCollision[1];
        Check(lr0.mbCull && lr1.mbCull, "Scrapes: outside a fatality every collision continuing a scrape is eaten");
        Check(lr0.mScrapeInfo.mRelativeVelocity.x == -10.0f && lr0.mScrapeInfo.mbCrashing,
              "Scrapes: the continuing collision takes the history's relative velocity and crashing flag "
              "(the history aged first: B's velocity - A's, the crashing player)");
        Check(lr1.mScrapeInfo.mRelativeVelocity.x == 0.0f && lr1.mScrapeInfo.mbCrashing,
              "Scrapes: the reversed duplicate is not listed (no velocity) but still takes the crashing flag");
        Check(w.mMgr.maScrapeHistory[0].mfTimeStamp == 10.05f && w.mMgr.maScrapeHistory[0].mfIntensity == 6.0f,
              "Scrapes: every matching collision refreshes the history's stamp and intensity (UpdateHistory)");
        const CollisionState& lrNew = w.maStates[1];
        Check(lrNew.mbIsAttached && lrNew.meLifetime.GetCurrent() == CollisionState::E_SCRAPE &&
                  lrNew.meLifetime.GetPrevious() == CollisionState::E_NONE,
              "Scrapes: a continuing scrape no state holds takes the first free state with the SCRAPE lifetime");
        Check(lrNew.mOutputCollision.mPosition.x == 10.0f && lrNew.mOutputCollision.mScrapeInfo.mfIntensity == 5.0f &&
                  lrNew.mOutputCollision.mScrapeInfo.mbValid && lrNew.mOutputCollision.mBinKey == 0u,
              "Scrapes: the state's OutputCollision holds the collision's position and scrape, bin key 0");
        Check(!w.maStates[2].mbIsAttached && w.mMgr.miOverrideCalls == 0,
              "Scrapes: one state per scrape, taken by the base StateManager::GetFreeState (no priority steal)");
        Check(w.maStates[0].meLifetime.GetCurrent() == CollisionState::E_COLLISION,
              "Scrapes: an attached state holding another scrape is left alone");

        // Frame 3: the scrape continues; now a state holds it -- it keeps the SCRAPE lifetime and
        // takes the new scrape; no new state is attached.
        w.mMgr.mfCurrentTime = 10.1f;
        InputCollision laFrame3[1] = { Input(Scrape(kCar0, kWorld, eO::Side, 10.1f, 7.0f), V(10, 0, 0)) };
        w.SetInputs(laFrame3, 1);
        w.mMgr.UpdateScrapes(lrFrame);
        Check(w.maStates[1].meLifetime.GetCurrent() == CollisionState::E_SCRAPE &&
                  w.maStates[1].meLifetime.GetPrevious() == CollisionState::E_SCRAPE &&
                  w.maStates[1].mOutputCollision.mScrapeInfo.mfIntensity == 7.0f &&
                  w.maStates[1].mOutputCollision.mScrapeInfo.mfTimeStamp == 10.1f,
              "Scrapes: the state holding the scrape keeps the SCRAPE lifetime and takes the new scrape");
        Check(!w.maStates[2].mbIsAttached, "Scrapes: no second state for a held scrape");

        // A collision state from an impact (lifetime COLLISION) whose scrape continues becomes the
        // scrape's state.
        World w3;
        w3.mMgr.mfCurrentTime = 20.0f;
        w3.mMgr.maScrapeHistory[3] = Scrape(kCar0, kCar1, eO::Front, 19.95f);
        w3.maStates[2].mbIsAttached = true;
        w3.maStates[2].mOutputCollision.mScrapeInfo = Scrape(kCar1, kCar0, eO::Front, 19.95f);
        w3.maStates[2].meLifetime.Flush(CollisionState::E_COLLISION);
        InputCollision laImpact[1] = { Input(Scrape(kCar0, kCar1, eO::Front, 20.0f, 2.0f), V(0, 0, 5)) };
        w3.SetInputs(laImpact, 1);
        w3.mMgr.UpdateScrapes(w3.mMgr.mFrameInformation);
        Check(w3.maStates[2].meLifetime.GetCurrent() == CollisionState::E_SCRAPE &&
                  w3.maStates[2].meLifetime.GetPrevious() == CollisionState::E_COLLISION &&
                  w3.maStates[2].mOutputCollision.mScrapeInfo.mEntityIdA.muValue == kCar0.muValue,
              "Scrapes: an impact's collision state whose scrape continues turns into the scrape (COLLISION -> SCRAPE)");
        Check(!w3.maStates[0].mbIsAttached && !w3.maStates[1].mbIsAttached,
              "Scrapes: a scrape a state took needs no free state");

        // Between 0.1 s and the idle time: refreshed and eaten, but not handed to a state.
        World w4;
        w4.mMgr.mfCurrentTime = 30.0f;
        w4.mMgr.maScrapeHistory[0] = Scrape(kCar0, kWorld, eO::Roof, 29.85f);
        InputCollision laLate[1] = { Input(Scrape(kCar0, kWorld, eO::Roof, 30.0f), V(0, 0, 0)) };
        w4.SetInputs(laLate, 1);
        w4.mMgr.UpdateScrapes(w4.mMgr.mFrameInformation);
        Check(w4.mMgr.maInputCollision[0].mbCull && w4.mMgr.maScrapeHistory[0].mfTimeStamp == 30.0f &&
                  !w4.maStates[0].mbIsAttached,
              "Scrapes: a history scrape 0.15 s old is refreshed and eats the collision but takes no state (< 0.1 s, flt_820ABB14)");

        // During a fatality the collision is not eaten.
        World w5;
        w5.mMgr.mfCurrentTime = 40.0f;
        w5.mMgr.mFrameInformation.meFatality.Flush(E_FATAL_ON);
        w5.mMgr.maScrapeHistory[0] = Scrape(kCar0, kWorld, eO::Side, 39.99f);
        InputCollision laFatal[1] = { Input(Scrape(kCar0, kWorld, eO::Side, 40.0f), V(0, 0, 0)) };
        w5.SetInputs(laFatal, 1);
        w5.mMgr.UpdateScrapes(w5.mMgr.mFrameInformation);
        Check(!w5.mMgr.maInputCollision[0].mbCull && w5.maStates[0].mbIsAttached,
              "Scrapes: in a fatality (meFatality != E_FATAL_OFF) the collision is kept and the scrape still takes a state");

        // No free state: the leg ends; the new-scrape leg still runs.
        World w6;
        w6.mMgr.mfCurrentTime = 50.0f;
        for (u32 i = 0; i < 3; ++i)
        {
            w6.maStates[i].mbIsAttached = true;
            w6.maStates[i].mOutputCollision.mScrapeInfo = Scrape(kCar2, kWorld, eO::Bottom, 1.0f);
        }
        w6.mMgr.maScrapeHistory[0] = Scrape(kCar0, kWorld, eO::Side, 49.95f);
        InputCollision laBusy[2] = {
            Input(Scrape(kCar0, kWorld, eO::Side, 50.0f), V(0, 0, 0)),
            Input(Scrape(kCar1, kWorld, eO::Rear, 50.0f), V(0, 0, 0)),
        };
        w6.SetInputs(laBusy, 2);
        w6.mMgr.UpdateScrapes(w6.mMgr.mFrameInformation);
        Check(w6.maStates[0].mOutputCollision.mScrapeInfo.meOrientation == eO::Bottom && w6.mMgr.miOverrideCalls == 0 &&
                  ValidHistory(w6.mMgr) == 2 && w6.mMgr.maScrapeHistory[1].mEntityIdA.muValue == kCar1.muValue,
              "Scrapes: with every state busy the continuing scrape is dropped and the new one is still recorded");

        // A new scrape one side of which FindEntity cannot find is not recorded; a NaN distance
        // counts as near.
        World w7;
        w7.mMgr.mfCurrentTime = 60.0f;
        InputCollision laUnknown[2] = {
            Input(Scrape(kCar2, kWorld, eO::Side, 60.0f), V(0, 0, 0)),
            Input(Scrape(kCar1, kWorld, eO::Rear, 60.0f), V(KF_NAN, 0, 0)),
        };
        w7.SetInputs(laUnknown, 2);
        w7.mMgr.UpdateScrapes(w7.mMgr.mFrameInformation);
        Check(ValidHistory(w7.mMgr) == 1 && w7.mMgr.maScrapeHistory[0].mEntityIdA.muValue == kCar1.muValue,
              "Scrapes: an unknown side keeps a scrape out of the history; a NaN distance is tracked (`vcmpgefp` + vnot)");

        // The distance is to the PLAYER's position in the frame, not the origin.
        World w8;
        w8.mMgr.mfCurrentTime = 70.0f;
        FrameInformation lFar;
        lFar.mPlayerTransform = At(V(0, 0, 1), V(1000, 0, 0));
        InputCollision laNear[2] = {
            Input(Scrape(kCar0, kWorld, eO::Side, 70.0f), V(1030, 0, 40)),   // 50 m exactly: out
            Input(Scrape(kCar1, kWorld, eO::Side, 70.0f), V(1030, 0, 39)),   // just inside
        };
        w8.SetInputs(laNear, 2);
        w8.mMgr.UpdateScrapes(lFar);
        Check(ValidHistory(w8.mMgr) == 1 && w8.mMgr.maScrapeHistory[0].mEntityIdA.muValue == kCar1.muValue,
              "Scrapes: only scrapes closer than 50 m to the player (KF_CULLING_DISTANCE_SQUARED 2500) are tracked");

        // UpdateScrapes ages the history first.
        World w9;
        w9.mMgr.mfCurrentTime = 80.0f;
        w9.mMgr.maScrapeHistory[0] = Scrape(kCar0, kWorld, eO::Side, 79.5f);
        InputCollision laStale[1] = { Input(Scrape(kCar0, kWorld, eO::Side, 80.0f), V(0, 0, 0)) };
        w9.SetInputs(laStale, 1);
        w9.mMgr.UpdateScrapes(w9.mMgr.mFrameInformation);
        Check(!w9.mMgr.maInputCollision[0].mbCull && ValidHistory(w9.mMgr) == 1 &&
                  w9.mMgr.maScrapeHistory[0].mfTimeStamp == 80.0f,
              "Scrapes: a stale history entry is aged out first, so the collision starts a new scrape instead of being eaten");
    }

    Check(guAsserts == 0, "no assert fired anywhere");
    std::printf("FxCrashSnd2Scrapes: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
