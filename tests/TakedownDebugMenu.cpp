#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <vector>

using s8 = signed char;
using s32 = int;
using u8 = unsigned char;
using u32 = unsigned int;
using f32 = float;

enum EActiveRaceCarIndex : s32
{
    E_ACTIVE_RACE_CAR_INDEX_0 = 0,
    E_ACTIVE_RACE_CAR_INDEX_1,
    E_ACTIVE_RACE_CAR_INDEX_2,
    E_ACTIVE_RACE_CAR_INDEX_3,
    E_ACTIVE_RACE_CAR_INDEX_4,
    E_ACTIVE_RACE_CAR_INDEX_5,
    E_ACTIVE_RACE_CAR_INDEX_6,
    E_ACTIVE_RACE_CAR_INDEX_7,
    E_ACTIVE_RACE_CAR_INDEX_COUNT,
    E_ACTIVE_RACE_CAR_INDEX_INVALID = -1,
};

namespace rw
{
    namespace math { namespace vpu
    {
        struct Vector3
        {
            f32 x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
        };

        struct Matrix44Affine
        {
            Vector3 xAxis, yAxis, zAxis, wAxis;
            Vector3& Pos() { return wAxis; }
            const Vector3& Pos() const { return wAxis; }
        };
    } }

    struct RGBA
    {
        RGBA(u8 red, u8 green, u8 blue, u8 alpha);
        u32 m_rgba;
    };
}

namespace CgsDev
{
    using RGBA = u32;
    struct Debug2DImmediateRender {};

    struct Debug3DImmediateRender
    {
        struct Sphere
        {
            rw::math::vpu::Vector3 mCentre;
            f32 mfRadius;
            u32 muColour;
        };

        void DrawSolidSphere(rw::math::vpu::Vector3 lCentre, f32 lfRadius, rw::RGBA lColour)
        {
            maSpheres.push_back({lCentre, lfRadius, lColour.m_rgba});
        }

        std::vector<Sphere> maSpheres;
    };

    struct SimpleStrStream
    {
        SimpleStrStream& operator<<(const char* lpcValue)
        {
            mText += lpcValue;
            return *this;
        }
        SimpleStrStream& operator<<(s32 liValue)
        {
            char lacBuffer[32];
            std::snprintf(lacBuffer, sizeof(lacBuffer), "%d", liValue);
            mText += lacBuffer;
            return *this;
        }
        SimpleStrStream& operator<<(u32 luValue)
        {
            char lacBuffer[32];
            std::snprintf(lacBuffer, sizeof(lacBuffer), "%u", luValue);
            mText += lacBuffer;
            return *this;
        }
        SimpleStrStream& operator<<(f32 lfValue)
        {
            char lacBuffer[64];
            std::snprintf(lacBuffer, sizeof(lacBuffer), "%g", static_cast<double>(lfValue));
            mText += lacBuffer;
            return *this;
        }
        const char* GetBuffer() const
        {
            return mText.c_str();
        }
        std::string mText;
    };

    struct DebugComponent
    {
        struct VariableBinding { bool* mpValue; std::string mLabel; };
        struct FunctionBinding
        {
            void (*mpCallback)(void*);
            void* mpUserData;
            std::string mLabel;
        };

        virtual ~DebugComponent() = default;
        virtual const char* GetName() const { return ""; }
        virtual void OnActivate() {}
        virtual void RenderHUD(Debug2DImmediateRender*) {}
        virtual void RenderWorld(Debug3DImmediateRender*) {}
        virtual const char* GetPath() const { return "Gameplay"; }

        void Register() { ++miRegisterCalls; }
        void RegisterVariable(bool* lpValue, const char* lpcLabel)
        {
            maVariables.push_back({lpValue, lpcLabel});
        }
        void RegisterFunction(void (*lpCallback)(void*), void* lpUserData, const char* lpcLabel)
        {
            maFunctions.push_back({lpCallback, lpUserData, lpcLabel});
        }

        int miRegisterCalls = 0;
        std::vector<VariableBinding> maVariables;
        std::vector<FunctionBinding> maFunctions;
    };

    std::string sLastDrawnText;
    f32 sfTextX = 0.0f, sfTextY = 0.0f, sfTextScale = 0.0f;
    RGBA suTextColour = 0;
    bool sbTextCentred = false;
    int giAssertionCount = 0;

    namespace Log
    {
        struct TestLog
        {
            TestLog& operator<<(const char* lpcText) { mText += lpcText; return *this; }
            std::string mText;
        };
        TestLog* gpDebugPrint = nullptr;
    }
}

#define CGS_ASSERT(condition, message) do { if (!(condition)) { ++CgsDev::giAssertionCount; } } while (0)

int MaybeDrawText(CgsDev::Debug2DImmediateRender*, const char* lpcText, f32 lfX, f32 lfY,
                  f32 lfScale, CgsDev::RGBA lColour, bool lbCentred)
{
    CgsDev::sLastDrawnText = lpcText;
    CgsDev::sfTextX = lfX;
    CgsDev::sfTextY = lfY;
    CgsDev::sfTextScale = lfScale;
    CgsDev::suTextColour = lColour;
    CgsDev::sbTextCentred = lbCentred;
    return 0;
}

namespace BrnPhysics { namespace Vehicle
{
    struct VehiclePhysics
    {
        struct SlamEffect { f32 mfSlamLife = 0.0f; };
        struct ShuntEffect
        {
            struct DesiredSpeed { f32 w = 0.0f; } mDirectionPlusDesiredSpeed;
            struct Life { f32 x = 0.0f; } mv4_Life_SpeedIncreaseToQuit;
            bool IsActive() const;
        };
    };

    struct RaceCarState
    {
        VehiclePhysics::SlamEffect mSlamEffect;
        VehiclePhysics::ShuntEffect mShuntEffect;
        rw::math::vpu::Matrix44Affine mTransform;
        f32 mfTimeSinceLastRaceCarContact = 0.0f;
        s8 mi8LastContactedRaceCar = -1;
    };
} }

namespace BrnWorld { namespace RaceCarEntityModuleIO
{
    struct RCEntityActiveRaceCarOutputInterface
    {
        bool IsRaceCarActive(::EActiveRaceCarIndex leIndex) const { return mabActive[leIndex]; }
        const BrnPhysics::Vehicle::RaceCarState* GetRaceCarState(::EActiveRaceCarIndex leIndex) const
        {
            return &maRaceCarStates[leIndex];
        }
        bool IsPlayerCarActive() const { return mbPlayerCarActive; }
        ::EActiveRaceCarIndex GetPlayerActiveRaceCarIndex() const { return mePlayerActiveRaceCarIndex; }

        bool mabActive[E_ACTIVE_RACE_CAR_INDEX_COUNT] = {};
        BrnPhysics::Vehicle::RaceCarState maRaceCarStates[E_ACTIVE_RACE_CAR_INDEX_COUNT];
        bool mbPlayerCarActive = false;
        ::EActiveRaceCarIndex mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
    };
} }

namespace BrnGameState
{
    enum ETakedownType : s32 { E_TAKEDOWN_STANDARD = 0 };

    struct TakedownEvent
    {
        EActiveRaceCarIndex meAggressorIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
        EActiveRaceCarIndex meVictimIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
        ETakedownType meType = E_TAKEDOWN_STANDARD;
    };

    struct GameStateModule
    {
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
        GetLastActiveRaceCarInterface() const { return &mActiveCars; }

        BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface mActiveCars;
    };

    struct ModeManager
    {
        GameStateModule* GetGameStateModule() const { return mpGameStateModule; }
        GameStateModule* mpGameStateModule = nullptr;
    };

    struct TakedownManager
    {
        struct RaceCarData
        {
            bool mabRevengeRelationships[E_ACTIVE_RACE_CAR_INDEX_COUNT] = {};
            f32 mfTimeSinceLastTakenDown = 0.0f;
            f32 mfTimeSinceVictimCrashed = 0.0f;
            bool mbWaitingOnTakedown = false;
            TakedownEvent mPendingTakedownEvent;
            f32 mfTimeSinceLastTakedown = 0.0f;
            s32 miTakedownChainLength = 0;
            s32 miMultipleTakedownLength = 0;
            void Clear();
        };

        RaceCarData* GetRaceCarData(EActiveRaceCarIndex leIndex) { return &maRaceCarData[leIndex]; }

        RaceCarData maRaceCarData[E_ACTIVE_RACE_CAR_INDEX_COUNT];
        ModeManager* mpModeManager = nullptr;
    };

    class TakedownManagerDebugComponent : public CgsDev::DebugComponent
    {
    public:
        void Construct(TakedownManager* lpTakedownManager);
        void RecordTakedown(EActiveRaceCarIndex leAggressorIndex, EActiveRaceCarIndex leVictimIndex);
        const char* GetName() const override;
        const char* GetPath() const override { return "Gameplay"; }
        void OnActivate() override;
        void RenderHUD(CgsDev::Debug2DImmediateRender* lpRender) override;
        void RenderWorld(CgsDev::Debug3DImmediateRender* lpRender) override;

    private:
        static void ForceTakedownCallback(void* lpContext);
        TakedownManager* mpTakedownManager = nullptr;
        EActiveRaceCarIndex meLastAggressorIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
        EActiveRaceCarIndex meLastVictimIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
        f32 mfLastTakedownContactTime = -1.0f;
        bool mbShowVulnerability = false;
        bool mbShowLastTakedownInfo = false;
        bool mbShowRevengeTakedownInfo = false;
    };
}

#include "takedown_debug_menu.inc"

static int giChecks = 0;
static int giFailures = 0;
static void Check(bool lbCondition, const char* lpcName)
{
    ++giChecks;
    if (!lbCondition)
    {
        ++giFailures;
        std::printf("FAIL %s\n", lpcName);
    }
}

static bool Near(f32 lfLeft, f32 lfRight)
{
    return std::fabs(lfLeft - lfRight) < 0.0001f;
}

static int CountOccurrences(const std::string& lrText, const std::string& lrNeedle)
{
    int liCount = 0;
    std::string::size_type liPosition = 0;
    while ((liPosition = lrText.find(lrNeedle, liPosition)) != std::string::npos)
    {
        ++liCount;
        liPosition += lrNeedle.size();
    }
    return liCount;
}

static void SetPosition(BrnPhysics::Vehicle::RaceCarState& lrState, f32 x, f32 y, f32 z)
{
    lrState.mTransform.wAxis = {x, y, z, 0.0f};
}

int main()
{
    using namespace BrnGameState;
    TakedownManager lManager;
    GameStateModule lGameState;
    ModeManager lModeManager;
    lModeManager.mpGameStateModule = &lGameState;
    lManager.mpModeManager = &lModeManager;

    TakedownManagerDebugComponent lComponent;
    lComponent.Construct(&lManager);
    Check(lComponent.GetName() == std::string("TakedownManager"), "component keeps the TakedownManager name");
    Check(lComponent.GetPath() == std::string("Gameplay"), "component stays under the Gameplay debug page");
    Check(lComponent.miRegisterCalls == 1, "Construct calls the base Register once");
    lComponent.OnActivate();
    Check(lComponent.maVariables.size() == 3, "all three display controls are registered");
    Check(lComponent.maFunctions.size() == 1, "the force takedown action is registered");
    Check(lComponent.maVariables[0].mLabel == "Show last takedown info" &&
          lComponent.maVariables[0].mpValue != nullptr, "last-takedown toggle binds its component field");
    Check(lComponent.maVariables[1].mLabel == "Show vulnerability" &&
          lComponent.maVariables[1].mpValue != nullptr, "vulnerability toggle binds its component field");
    Check(lComponent.maVariables[2].mLabel == "Show revenge takedown info" &&
          lComponent.maVariables[2].mpValue != nullptr, "revenge toggle binds its component field");
    Check(lComponent.maFunctions[0].mLabel == "Force takedown" &&
          lComponent.maFunctions[0].mpUserData == &lComponent &&
          lComponent.maFunctions[0].mpCallback != nullptr, "force action keeps its label and context");
    for (const auto& lrBinding : lComponent.maVariables)
        Check(!*lrBinding.mpValue, "display toggles start disabled");

    auto& lrActiveCars = lGameState.mActiveCars;
    lrActiveCars.mabActive[E_ACTIVE_RACE_CAR_INDEX_1] = true;
    lrActiveCars.maRaceCarStates[E_ACTIVE_RACE_CAR_INDEX_1].mi8LastContactedRaceCar = 3;
    lrActiveCars.maRaceCarStates[E_ACTIVE_RACE_CAR_INDEX_1].mfTimeSinceLastRaceCarContact = 1.25f;
    *lComponent.maVariables[0].mpValue = true;
    CgsDev::Debug2DImmediateRender lHud;
    lComponent.RenderHUD(&lHud);
    Check(CgsDev::sLastDrawnText == "Invalid", "last-takedown toggle shows its original empty state");

    lComponent.RecordTakedown(E_ACTIVE_RACE_CAR_INDEX_0, E_ACTIVE_RACE_CAR_INDEX_1);
    lComponent.RenderHUD(&lHud);
    Check(CgsDev::sLastDrawnText == "Car 0 takes down car 1, last contacted: 1.25s",
          "the last-takedown toggle renders the recorded car pair and contact time");
    Check(Near(CgsDev::sfTextX, 100.0f) && Near(CgsDev::sfTextY, 50.0f) &&
          Near(CgsDev::sfTextScale, 20.0f) && CgsDev::suTextColour == 0xFFFFFFFFu &&
          !CgsDev::sbTextCentred, "last-takedown text keeps the original placement and style");

    lrActiveCars.maRaceCarStates[E_ACTIVE_RACE_CAR_INDEX_1].mi8LastContactedRaceCar = -1;
    lComponent.RecordTakedown(E_ACTIVE_RACE_CAR_INDEX_1, E_ACTIVE_RACE_CAR_INDEX_1);
    lComponent.RenderHUD(&lHud);
    Check(CgsDev::sLastDrawnText == "Car 1 takes down car 1, slam/shunt takedown",
          "the last-takedown toggle preserves the no-car-contact label");

    for (s32 liIndex = 0; liIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liIndex)
        SetPosition(lrActiveCars.maRaceCarStates[liIndex], static_cast<f32>(liIndex),
                    static_cast<f32>(liIndex + 10), static_cast<f32>(liIndex + 20));

    lrActiveCars.mabActive[0] = true;
    lrActiveCars.maRaceCarStates[0].mShuntEffect.mDirectionPlusDesiredSpeed.w = 1.0f;
    lrActiveCars.maRaceCarStates[0].mShuntEffect.mv4_Life_SpeedIncreaseToQuit.x = 1.0f;
    lrActiveCars.mabActive[2] = true;
    lrActiveCars.maRaceCarStates[1].mSlamEffect.mfSlamLife = 0.25f;
    lrActiveCars.mabActive[2] = true;
    lrActiveCars.maRaceCarStates[2].mfTimeSinceLastRaceCarContact = 1.9f;
    lrActiveCars.mabActive[3] = true;
    lrActiveCars.maRaceCarStates[3].mfTimeSinceLastRaceCarContact = 2.0f;
    lrActiveCars.maRaceCarStates[4].mShuntEffect.mDirectionPlusDesiredSpeed.w = 3.0f;
    lrActiveCars.maRaceCarStates[4].mShuntEffect.mv4_Life_SpeedIncreaseToQuit.x = 2.0f;
    lrActiveCars.mabActive[5] = true;
    lrActiveCars.maRaceCarStates[5].mShuntEffect.mDirectionPlusDesiredSpeed.w = 0.0f;
    lrActiveCars.maRaceCarStates[5].mShuntEffect.mv4_Life_SpeedIncreaseToQuit.x = 2.0f;
    lrActiveCars.maRaceCarStates[5].mfTimeSinceLastRaceCarContact = 2.0f;
    lrActiveCars.mabActive[6] = true;
    lrActiveCars.maRaceCarStates[6].mfTimeSinceLastRaceCarContact = 1.999f;
    lrActiveCars.mabActive[7] = true;
    lrActiveCars.maRaceCarStates[7].mfTimeSinceLastRaceCarContact = 2.0f;

    *lComponent.maVariables[1].mpValue = true;
    CgsDev::Debug3DImmediateRender lWorld;
    lComponent.RenderWorld(&lWorld);
    Check(lWorld.maSpheres.size() == 4, "vulnerability draws only active shunt, slam, and recent-contact cars");
    Check(lWorld.maSpheres.size() == 4 &&
          lWorld.maSpheres[0].muColour == 0x400000FFu &&
          lWorld.maSpheres[1].muColour == 0x400000FFu &&
          lWorld.maSpheres[2].muColour == 0x4000FFFFu &&
          lWorld.maSpheres[3].muColour == 0x4000FFFFu,
          "vulnerability preserves the console's red/yellow conditions and color words");
    Check(lWorld.maSpheres.size() == 4 &&
          Near(lWorld.maSpheres[0].mfRadius, 2.0f) && Near(lWorld.maSpheres[1].mfRadius, 2.0f) &&
          Near(lWorld.maSpheres[2].mfRadius, 2.0f) && Near(lWorld.maSpheres[3].mfRadius, 2.0f) &&
          Near(lWorld.maSpheres[0].mCentre.x, 0.0f) && Near(lWorld.maSpheres[0].mCentre.y, 10.0f) &&
          Near(lWorld.maSpheres[0].mCentre.z, 20.0f) && Near(lWorld.maSpheres[3].mCentre.x, 6.0f),
          "vulnerability markers use the active car transform position and 2 m radius");

    *lComponent.maVariables[1].mpValue = false;
    *lComponent.maVariables[2].mpValue = true;
    lrActiveCars.mbPlayerCarActive = true;
    lrActiveCars.mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
    lManager.maRaceCarData[0].mabRevengeRelationships[1] = true;
    lManager.maRaceCarData[0].mabRevengeRelationships[4] = true;
    lManager.maRaceCarData[0].mabRevengeRelationships[6] = true;
    lWorld.maSpheres.clear();
    lComponent.RenderWorld(&lWorld);
    Check(lWorld.maSpheres.size() == 2, "revenge draws active cars only from the player's revenge relationship set");
    Check(lWorld.maSpheres.size() == 2 && lWorld.maSpheres[0].muColour == 0x400000FFu &&
          lWorld.maSpheres[1].muColour == 0x400000FFu &&
          Near(lWorld.maSpheres[0].mCentre.x, 1.0f) && Near(lWorld.maSpheres[1].mCentre.x, 6.0f),
          "revenge markers use each matched car's position and the original color");

    lrActiveCars.mbPlayerCarActive = false;
    lWorld.maSpheres.clear();
    lComponent.RenderWorld(&lWorld);
    Check(lWorld.maSpheres.empty(), "revenge markers require the original active-player gate");

    lrActiveCars.mbPlayerCarActive = true;
    *lComponent.maVariables[1].mpValue = true;
    lWorld.maSpheres.clear();
    lComponent.RenderWorld(&lWorld);
    Check(lWorld.maSpheres.size() == 6, "both 3D toggles compose in one RenderWorld pass");

    *lComponent.maVariables[0].mpValue = false;
    *lComponent.maVariables[1].mpValue = false;
    *lComponent.maVariables[2].mpValue = false;
    CgsDev::sLastDrawnText.clear();
    lWorld.maSpheres.clear();
    lComponent.RenderHUD(&lHud);
    lComponent.RenderWorld(&lWorld);
    Check(CgsDev::sLastDrawnText.empty() && lWorld.maSpheres.empty(),
          "turning all display controls off stops both HUD and world output");

    CgsDev::Log::TestLog lTraceLog;
    CgsDev::Log::gpDebugPrint = &lTraceLog;
    _putenv_s("BRN_TAKEDOWN_DEBUG_TRACE", "1");
    for (int liFrame = 0; liFrame < 200; ++liFrame)
        lComponent.RenderWorld(&lWorld);
    Check(lTraceLog.mText.empty(), "steady disabled frames do not consume or emit trace output");
    *lComponent.maVariables[1].mpValue = true;
    *lComponent.maVariables[2].mpValue = true;
    lComponent.RenderWorld(&lWorld);
    Check(lTraceLog.mText.find("event=transition") != std::string::npos &&
          lTraceLog.mText.find("vulnerability-recent-contact=") != std::string::npos &&
          lTraceLog.mText.find("revenge=") != std::string::npos &&
          lTraceLog.mText.find("geometry-submissions=") != std::string::npos,
          "trace records the enabled cached-slot branches and actual submissions");
    lTraceLog.mText.clear();
    for (int liFrame = 0; liFrame < 60; ++liFrame)
        lComponent.RenderWorld(&lWorld);
    Check(lTraceLog.mText.find("event=sample") != std::string::npos,
          "trace emits a bounded periodic sample for stable enabled controls");
    lTraceLog.mText.clear();
    for (int liFrame = 0; liFrame < 1200; ++liFrame)
        lComponent.RenderWorld(&lWorld);
    Check(CountOccurrences(lTraceLog.mText, "event=sample") == 7,
          "trace stops at its independent eight-sample process cap");
    *lComponent.maVariables[1].mpValue = false;
    *lComponent.maVariables[2].mpValue = false;
    lTraceLog.mText.clear();
    lComponent.RenderWorld(&lWorld);
    Check(lTraceLog.mText.find("event=transition") != std::string::npos &&
          lTraceLog.mText.find("toggles=0") != std::string::npos,
          "trace preserves the hide transition after enabled samples");
    lTraceLog.mText.clear();
    for (int liTransition = 0; liTransition < 40; ++liTransition)
    {
        *lComponent.maVariables[1].mpValue = (liTransition % 2) == 0;
        lComponent.RenderWorld(&lWorld);
    }
    Check(CountOccurrences(lTraceLog.mText, "event=transition") == 30,
          "trace obeys its independent 32-transition process cap");
    *lComponent.maVariables[1].mpValue = false;
    *lComponent.maVariables[2].mpValue = false;
    _putenv_s("BRN_TAKEDOWN_DEBUG_TRACE", "0");
    CgsDev::Log::gpDebugPrint = nullptr;

    TakedownManager::RaceCarData& lrPlayerData = lManager.maRaceCarData[0];
    lrPlayerData.mabRevengeRelationships[7] = true;
    lrPlayerData.mfTimeSinceLastTakenDown = 4.0f;
    lrPlayerData.mfTimeSinceVictimCrashed = 8.0f;
    lrPlayerData.mbWaitingOnTakedown = false;
    lrPlayerData.mPendingTakedownEvent.meAggressorIndex = E_ACTIVE_RACE_CAR_INDEX_5;
    lrPlayerData.mPendingTakedownEvent.meVictimIndex = E_ACTIVE_RACE_CAR_INDEX_6;
    lrPlayerData.mPendingTakedownEvent.meType = E_TAKEDOWN_STANDARD;
    lComponent.maFunctions[0].mpCallback(lComponent.maFunctions[0].mpUserData);
    Check(!lrPlayerData.mabRevengeRelationships[7] && !lrPlayerData.mabRevengeRelationships[1] &&
          lrPlayerData.mfTimeSinceLastTakenDown == 3.4028235e38f &&
          lrPlayerData.mfTimeSinceVictimCrashed == 0.0f && lrPlayerData.mfTimeSinceLastTakedown == -1.0f &&
          lrPlayerData.miTakedownChainLength == 0 && lrPlayerData.miMultipleTakedownLength == 0,
          "force action clears the player's prior takedown state first");
    Check(lrPlayerData.mfTimeSinceVictimCrashed == 0.0f && lrPlayerData.mbWaitingOnTakedown &&
          lrPlayerData.mPendingTakedownEvent.meAggressorIndex == E_ACTIVE_RACE_CAR_INDEX_0 &&
          lrPlayerData.mPendingTakedownEvent.meVictimIndex == E_ACTIVE_RACE_CAR_INDEX_1 &&
          lrPlayerData.mPendingTakedownEvent.meType == E_TAKEDOWN_STANDARD,
          "force action arms the original standard car-0-to-car-1 takedown");
    Check(CgsDev::giAssertionCount == 0, "valid controls preserve the original assertion guards");

    std::printf("TakedownDebugMenu: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures == 0 ? 0 : 1;
}
