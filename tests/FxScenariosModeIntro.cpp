// FX-SCENARIOS (crash parity 2026-09-25): the game modes' intro lengths -- GameMode vtable slot 8,
// GetIntroDurationSeconds -- and what IntroState does with them.
//
// The PRODUCTION bodies are extracted by run_fxscenarios_mode_intro.py (working tree, or --rev <b5 rev>) into
// fxscenarios_mode_intro.inc and compiled here as members of small stand-in classes that carry exactly the
// members those bodies read:
//   GameMode::GetIntroDurationSeconds          @0x82315A88 (BrnGameMode.cpp)      -- the base, 10 vtables inherit it
//   CrashMode::GetIntroDurationSeconds          @0x827E2600 (BrnCrashMode.cpp)     -- slot 8 of vtable 0x820D0570
//   CrashMode::GetName                          @0x827E24C8 (BrnCrashMode.cpp)     -- slot 6 of vtable 0x820D0570
//   PursuitMode::GetIntroDurationSeconds        @0x827E24E8 (BrnPursuitMode.cpp)   -- slot 8 of vtable 0x820D0650
//   FaceOffMode::GetIntroDurationSeconds        @0x827EAB30 (BrnFaceOffMode.cpp)   -- slot 8 of vtable 0x820D0500
//   IntroState::OnEnter / IntroState::Update    @0x823163C8 / 0x823164A0 (BrnIntroState.cpp)
// An override the revision does not have is simply not declared on its stand-in (FXSC_HAS_* = 0), so an old
// revision runs with the base it really inherited and the checks say what that did.
// The REAL mode headers are included as well (shadowed to the revision under test): whether a mode class
// DECLARES its own slot-8 / slot-6 override is read off the class of its member pointer, which is the mode
// itself only when the mode declares the member.
//
// Every expected number is read from the ARTIST image (x360rd):
//   flt_82008718 = 0x3951B717 (0.0002f)  CrashMode / OnlineShowtimeMode slot 8
//   flt_82001C98 = 0x3F800000 (1.0f)     PursuitMode slot 8 (ICF 0x827E24E8), and the base's "+ 1.0"
//   flt_82001CC0 = 0x00000000 (0.0f)     FaceOffMode slot 8 (ICF 0x827EAB30); StartModeIntro's `> 0.0f`
//   flt_820211C8 = 0x40C00000 (6.0f)     base: KF_INTRO_TIME_SECONDS
//   flt_820211CC = 0x40000000 (2.0f)     base: mode 15 / 16 (KF_ONLINE_FREEBURN_INTRO_TIME_SECONDS)
//   flt_820211D4 = 0x40800000 (4.0f)     base: per flyby car (KF_ONLINE_INTRO_TIME_SECONDS_PER_CAR)
#include "types.hpp"
#include "GameSource/GameState/ModeManager/GameModes/BrnCrashMode.h"
#include "GameSource/GameState/ModeManager/GameModes/BrnPursuitMode.h"
#include "GameSource/GameState/ModeManager/GameModes/BrnFaceOffMode.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

#include "fxscenarios_mode_intro_config.inc"   // FXSC_SECTION_* / FXSC_HAS_* from the runner

static unsigned gChecks = 0, gFailures = 0, gAsserts = 0;

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

static void Check(bool lbPass, const char* lpcName, const char* lpcDetail = "")
{
    ++gChecks;
    if (!lbPass) { ++gFailures; }
    std::printf("%s  %s %s\n", lbPass ? "PASS" : "FAIL", lpcName, lpcDetail);
}

static u32 Bits(f32 lf)
{
    u32 lu;
    std::memcpy(&lu, &lf, sizeof(lu));
    return lu;
}

// ---- the stand-ins: exactly the members the extracted bodies read ---------------------------------------------------
namespace BrnGameState
{
namespace Fx
{
using GameStateModuleIO::EGameModeType;

class GameMode;

class ModeManager
{
public:
    EGameModeType meCurrentGameModeType;
    GameMode*     mpCurrentGameMode;
    s32           miNumberOfCarsInFlyby;
    mutable s32   miFlybyQueries;
    bool          mbInstantIntro;
    bool          mbWaitingForModeData;

    EGameModeType   GetCurrentGameModeType() const        { return meCurrentGameModeType; }
    const GameMode* GetCurrentGameMode() const            { return mpCurrentGameMode; }
    s32             GetNumberOfCarsInFlyby()              { ++miFlybyQueries; return miNumberOfCarsInFlyby; }
    bool            IsOnlineModeWithInstantIntro() const  { return mbInstantIntro; }
    bool            IsWaitingForModeDataToLoad() const    { return mbWaitingForModeData; }
};

class GameMode
{
public:
    virtual ~GameMode() {}
    virtual const char* GetName() const { return "(GameMode base)"; }
    virtual f32         GetIntroDurationSeconds() const;                 // the extracted base body
    virtual void        SendEvent(EGameModeEvent leEvent) { ++miEvents; meLastEvent = leEvent; }

    bool IsOnline() const               { return mbIsOnline; }
    f32  GetUpdateTimeStep() const      { return mfTimeStepSeconds; }
    void SetFinished(bool lbFinished)   { mbFinished = lbFinished; }

    ModeManager*   mpModeManager     = nullptr;
    f32            mfTimeStepSeconds = 0.0f;
    bool           mbIsOnline        = false;
    bool           mbFinished        = true;
    s32            miEvents          = 0;
    EGameModeEvent meLastEvent       = E_GME_ABORT;
};

class CrashMode : public GameMode
{
public:
#if FXSC_HAS_CRASH_INTRO
    virtual f32 GetIntroDurationSeconds() const;
#endif
#if FXSC_HAS_CRASH_NAME
    virtual const char* GetName() const;
#endif
};

class PursuitMode : public GameMode
{
public:
#if FXSC_HAS_PURSUIT_INTRO
    virtual f32 GetIntroDurationSeconds() const;
#endif
};

class FaceOffMode : public GameMode
{
public:
#if FXSC_HAS_FACEOFF_INTRO
    virtual f32 GetIntroDurationSeconds() const;
#endif
};

class IntroState
{
public:
    void OnEnter();
    void Update();

    const ModeManager* mpModeManager      = nullptr;
    GameMode*          mpGameMode         = nullptr;
    f32                mfCountdownSeconds = -1.0f;
    bool               mbUseCountdown     = false;
};

#include "fxscenarios_mode_intro.inc"   // the production bodies (and their file-scope constants)
}
}

using namespace BrnGameState;

// One mode through IntroState: OnEnter, then Update at the console's 60 Hz step until NEXT is posted (or 1000
// updates). Returns the update on which NEXT was posted, 0 if never.
struct IntroRun
{
    f32  mfSeeded;
    bool mbTimed;
    s32  miNextOnUpdate;
};

static IntroRun RunIntro(Fx::GameMode& lrMode, Fx::ModeManager& lrManager)
{
    const f32 KF_STEP = 1.0f / 60.0f;
    lrMode.mpModeManager      = &lrManager;
    lrMode.mfTimeStepSeconds  = KF_STEP;
    lrManager.mpCurrentGameMode = &lrMode;

    Fx::IntroState lState;
    lState.mpModeManager = &lrManager;
    lState.mpGameMode    = &lrMode;
    lState.OnEnter();

    IntroRun lRun = { lState.mfCountdownSeconds, lState.mbUseCountdown, 0 };
    for (s32 liUpdate = 1; liUpdate <= 1000 && lRun.miNextOnUpdate == 0; ++liUpdate)
    {
        const s32 liBefore = lrMode.miEvents;
        lState.Update();
        if (lrMode.miEvents != liBefore && lrMode.meLastEvent == E_GME_NEXT)
        {
            lRun.miNextOnUpdate = liUpdate;
        }
    }
    return lRun;
}

static Fx::ModeManager Manager(GameStateModuleIO::EGameModeType leType, s32 liCars = 0)
{
    Fx::ModeManager lManager = {};
    lManager.meCurrentGameModeType = leType;
    lManager.miNumberOfCarsInFlyby = liCars;
    return lManager;
}

// The base body for one (mode type, online flag, flyby count); lbNoMode = no current game mode at all.
static f32 Base(GameStateModuleIO::EGameModeType leType, bool lbOnline, s32 liCars, s32* lpiQueries, bool lbNoMode = false)
{
    Fx::ModeManager lManager = Manager(leType, liCars);
    Fx::GameMode    lMode;
    lMode.mpModeManager = &lManager;
    lMode.mbIsOnline    = lbOnline;
    lManager.mpCurrentGameMode = lbNoMode ? nullptr : &lMode;
    const f32 lfResult = lMode.GetIntroDurationSeconds();
    *lpiQueries = lManager.miFlybyQueries;
    return lfResult;
}

int main()
{
    char lacDetail[256];
    using namespace GameStateModuleIO;

#if FXSC_SECTION_CRASH
    // ---- CrashMode: slot 8 0x827E2600 (0.0002f) and slot 6 0x827E24C8 ("CrashMode") ---------------------------
    Check(std::is_same<decltype(&BrnGameState::CrashMode::GetIntroDurationSeconds),
                       f32 (BrnGameState::CrashMode::*)() const>::value,
          "crash: class CrashMode declares its own slot-8 GetIntroDurationSeconds (DWARF BrnCrashMode.h:100)");
    Check(std::is_same<decltype(&BrnGameState::CrashMode::GetName),
                       const char* (BrnGameState::CrashMode::*)() const>::value,
          "crash: class CrashMode declares its own slot-6 GetName (DWARF BrnCrashMode.h:88)");
    {
        Fx::ModeManager lManager = Manager(E_MODE_OFFLINE_SHOWTIME);
        Fx::CrashMode   lMode;
        lMode.mpModeManager = &lManager;
        const f32 lf = static_cast<Fx::GameMode&>(lMode).GetIntroDurationSeconds();
        std::snprintf(lacDetail, sizeof(lacDetail), "(got %g = 0x%08X)", lf, Bits(lf));
        Check(Bits(lf) == 0x3951B717u, "crash: a Showtime's slot 8 returns flt_82008718 = 0x3951B717 (0.0002f)", lacDetail);

        const IntroRun lRun = RunIntro(lMode, lManager);
        std::snprintf(lacDetail, sizeof(lacDetail), "(seeded %g timed %d)", lRun.mfSeeded, lRun.mbTimed ? 1 : 0);
        Check(Bits(lRun.mfSeeded) == 0x3951B717u && lRun.mbTimed,
              "crash: IntroState::OnEnter seeds the Showtime countdown with 0.0002 and times it (mode type 2)", lacDetail);
        std::snprintf(lacDetail, sizeof(lacDetail), "(NEXT on update %d; the 6.0 s stub posts it on update 360)", lRun.miNextOnUpdate);
        Check(lRun.miNextOnUpdate == 1,
              "crash: IntroState::Update posts NEXT on the FIRST mode update at 1/60 s (so crash mode takes the frame at once)", lacDetail);

        const char* lpcName = static_cast<Fx::GameMode&>(lMode).GetName();
        std::snprintf(lacDetail, sizeof(lacDetail), "(got \"%s\")", lpcName);
        Check(std::strcmp(lpcName, "CrashMode") == 0, "crash: slot 6 returns \"CrashMode\" (0x820D05D8)", lacDetail);
    }
#endif

#if FXSC_SECTION_BASE
    // ---- the base @0x82315A88: offline 6.0; mode 15 / 16 2.0; other online (flyby cars + 1) * 4.0 --------------
    {
        struct Row { const char* mpcName; EGameModeType meType; bool mbOnline; s32 miCars; bool mbNoMode; u32 muBits; s32 miQueries; };
        const Row kaRows[] =
        {
            { "offline race (type 0)",                          E_MODE_OFFLINE_RACE,           false, 5, false, 0x40C00000u, 0 },
            { "offline road rage (type 3)",                     E_MODE_ROAD_RAGE,              false, 5, false, 0x40C00000u, 0 },
            { "offline burning route (type 5)",                 E_MODE_BURNING_ROUTE,          false, 5, false, 0x40C00000u, 0 },
            { "online race, 0 flyby cars (type 10)",            E_MODE_ONLINE_RACE,            true,  0, false, 0x40800000u, 1 },
            { "online race, 3 flyby cars",                      E_MODE_ONLINE_RACE,            true,  3, false, 0x41800000u, 1 },
            { "online free burn, 7 flyby cars (type 14)",       E_MODE_ONLINE_FREE_BURN,       true,  7, false, 0x42000000u, 1 },
            { "online free-burn lobby (type 15)",               E_MODE_ONLINE_FREE_BURN_LOBBY, true,  5, false, 0x40000000u, 0 },
            { "online showtime type (16) through the base",     E_MODE_ONLINE_SHOWTIME,        true,  5, false, 0x40000000u, 0 },
            { "type 16 with an OFFLINE current mode",           E_MODE_ONLINE_SHOWTIME,        false, 5, false, 0x40000000u, 0 },
            { "no current game mode, type 0",                   E_MODE_OFFLINE_RACE,           true,  5, true,  0x40C00000u, 0 },
            { "no current game mode, type 15",                  E_MODE_ONLINE_FREE_BURN_LOBBY, true,  5, true,  0x40000000u, 0 },
            { "online race, flyby count -1 (signed fcfid)",     E_MODE_ONLINE_RACE,            true, -1, false, 0x00000000u, 1 },
        };
        for (const Row& lrRow : kaRows)
        {
            s32 liQueries = -1;
            const f32 lf = Base(lrRow.meType, lrRow.mbOnline, lrRow.miCars, &liQueries, lrRow.mbNoMode);
            char lacName[160];
            std::snprintf(lacName, sizeof(lacName), "base: %s -> 0x%08X, %d flyby quer%s",
                          lrRow.mpcName, lrRow.muBits, lrRow.miQueries, lrRow.miQueries == 1 ? "y" : "ies");
            std::snprintf(lacDetail, sizeof(lacDetail), "(got %g = 0x%08X, %d quer%s)", lf, Bits(lf), liQueries, liQueries == 1 ? "y" : "ies");
            Check(Bits(lf) == lrRow.muBits && liQueries == lrRow.miQueries, lacName, lacDetail);
        }
    }
#endif

#if FXSC_SECTION_PURSUIT
    // ---- PursuitMode: slot 8 0x827E24E8 (ICF) = flt_82001C98 = 1.0f --------------------------------------------
    Check(std::is_same<decltype(&BrnGameState::PursuitMode::GetIntroDurationSeconds),
                       f32 (BrnGameState::PursuitMode::*)() const>::value,
          "pursuit: class PursuitMode declares its own slot-8 GetIntroDurationSeconds (DWARF BrnPursuitMode.h:80)");
    {
        Fx::ModeManager lManager = Manager(E_MODE_PURSUIT);
        Fx::PursuitMode lMode;
        lMode.mpModeManager = &lManager;
        const f32 lf = static_cast<Fx::GameMode&>(lMode).GetIntroDurationSeconds();
        std::snprintf(lacDetail, sizeof(lacDetail), "(got %g = 0x%08X)", lf, Bits(lf));
        Check(Bits(lf) == 0x3F800000u, "pursuit: slot 8 returns flt_82001C98 = 0x3F800000 (1.0f, KF_PURSUIT_INTRO_TIME)", lacDetail);
        const IntroRun lRun = RunIntro(lMode, lManager);
        std::snprintf(lacDetail, sizeof(lacDetail), "(seeded %g timed %d)", lRun.mfSeeded, lRun.mbTimed ? 1 : 0);
        Check(Bits(lRun.mfSeeded) == 0x3F800000u && !lRun.mbTimed,
              "pursuit: IntroState::OnEnter seeds 1.0 and leaves an offline pursuit untimed", lacDetail);
    }
#endif

#if FXSC_SECTION_FACEOFF
    // ---- FaceOffMode: slot 8 0x827EAB30 (ICF) = flt_82001CC0 = 0.0f -> StartModeIntro's `> 0.0f` says NO intro --
    Check(std::is_same<decltype(&BrnGameState::FaceOffMode::GetIntroDurationSeconds),
                       f32 (BrnGameState::FaceOffMode::*)() const>::value,
          "faceoff: class FaceOffMode declares its own slot-8 GetIntroDurationSeconds (DWARF BrnFaceOffMode.h:99)");
    {
        Fx::ModeManager lManager = Manager(E_MODE_FACE_OFF);
        Fx::FaceOffMode lMode;
        lMode.mpModeManager = &lManager;
        const f32 lf = static_cast<Fx::GameMode&>(lMode).GetIntroDurationSeconds();
        std::snprintf(lacDetail, sizeof(lacDetail), "(got %g = 0x%08X)", lf, Bits(lf));
        Check(Bits(lf) == 0x00000000u, "faceoff: slot 8 returns flt_82001CC0 = 0x00000000 (+0.0f)", lacDetail);
        const bool lbDoIntro = (lf > 0.0f);   // StartModeIntro's mbDoIntro rule (fcmpu + bgt vs flt_82001CC0)
        Check(!lbDoIntro, "faceoff: StartModeIntro's mbDoIntro (duration > 0.0f) is FALSE: a Face Off has no intro");
    }
#endif

    std::printf("FxScenariosModeIntro: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
