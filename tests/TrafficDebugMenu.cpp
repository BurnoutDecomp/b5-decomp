#include <algorithm>
#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

using f32 = float;
using s32 = int;
using s8 = signed char;
using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;

static int giChecks = 0;
static int giFailures = 0;
static int giAssertions = 0;
static std::vector<std::string> gaRenderTrace;

static void Check(bool lbCondition, const char* lpcName)
{
    ++giChecks;
    if (!lbCondition)
    {
        ++giFailures;
        std::printf("FAIL %s\n", lpcName);
    }
}

#define CGS_ASSERT(condition, message) do { if (!(condition)) { ++giAssertions; } } while (0)

struct Vector3
{
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    f32 w = 0.0f;
};

struct VecFloat
{
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    f32 w = 0.0f;
};

struct Vector2
{
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    f32 w = 0.0f;
};

namespace rw
{
    struct RGBA
    {
        u32 m_rgba = 0;
    };
}

namespace CgsDev
{
    using RGBA = u32;

    namespace DebugUI
    {
        struct StringList
        {
            s32 miValue;
            const char* mpcName;
        };
    }

    struct Debug2DImmediateRender
    {
        struct Box
        {
            Vector2 mPosition;
            Vector2 mSize;
            RGBA mColour;
        };

        struct Line
        {
            Vector2 mStart;
            Vector2 mEnd;
            RGBA mColour;
        };

        Vector2 mVirtualScreenSize = {1280.0f, 720.0f, 0.0f, 0.0f};
        std::vector<Box> maBoxes;
        std::vector<Line> maLines;

        Vector2 GetVirtualScreenSize() const { return mVirtualScreenSize; }
        void DrawBox(Vector2 lPosition, Vector2 lSize, RGBA lColour)
        {
            maBoxes.push_back({lPosition, lSize, lColour});
            gaRenderTrace.emplace_back("box");
        }
        void DrawLine(Vector2 lStart, Vector2 lEnd, RGBA lColour)
        {
            maLines.push_back({lStart, lEnd, lColour});
            gaRenderTrace.emplace_back("line");
        }
    };

    struct PerfMonCpuMonitorData
    {
        const char* mpcName = nullptr;
        f32 mfCurrentValue = 0.0f;
        f32 mfAverageValue = 0.0f;
        f32 mfMinMaxValue = 0.0f;
        f32 mfCpuBudget = 0.0f;
        s32 miNumCalls = 0;
        s32 miMaxCalls = 0;
        bool mbTraced = false;
        f32 mfCurrentTraceValue = 0.0f;
        f32 mfAverageTraceValue = 0.0f;
        f32 mfMinMaxTraceValue = 0.0f;
    };

    struct TextDraw
    {
        std::string mText;
        f32 mfX;
        f32 mfY;
        f32 mfScale;
        RGBA mColour;
    };

    class DebugRender
    {
    public:
        std::vector<TextDraw> maText;
        void Draw2DText(const char* lpcText, f32 lfX, f32 lfY, f32 lfScale, RGBA lColour)
        {
            maText.push_back({lpcText ? lpcText : "", lfX, lfY, lfScale, lColour});
            gaRenderTrace.emplace_back("stream:" + maText.back().mText);
        }
    };

    struct StrStream
    {
        StrStream(char* lpcBuffer, s32 liBufferSize)
            : mpcBuffer(lpcBuffer), miBufferSize(liBufferSize)
        {
            if (mpcBuffer && miBufferSize > 0)
                mpcBuffer[0] = '\0';
        }

        char* GetBuffer() { return mpcBuffer; }
        void Reset()
        {
            if (mpcBuffer && miBufferSize > 0)
                mpcBuffer[0] = '\0';
        }
        void AppendFormat(const char* lpcFormat, ...)
        {
            if (!mpcBuffer || miBufferSize <= 0)
                return;
            const size_t luUsed = std::strlen(mpcBuffer);
            if (luUsed >= static_cast<size_t>(miBufferSize))
                return;
            va_list lArgs;
            va_start(lArgs, lpcFormat);
            std::vsnprintf(mpcBuffer + luUsed, static_cast<size_t>(miBufferSize) - luUsed,
                           lpcFormat, lArgs);
            va_end(lArgs);
        }

    private:
        char* mpcBuffer;
        s32 miBufferSize;
    };

    struct Debug3DImmediateRender
    {
        bool mbDrewSphere = false;
        Vector3 mSphereCentre = {};
        f32 mfSphereRadius = 0.0f;
        rw::RGBA mSphereColour = {};

        void DrawSolidSphere(Vector3 lCentre, f32 lfRadius, rw::RGBA lColour)
        {
            mbDrewSphere = true;
            mSphereCentre = lCentre;
            mfSphereRadius = lfRadius;
            mSphereColour = lColour;
        }
    };

    class DebugComponent
    {
    public:
        enum EventKind
        {
            E_VARIABLE,
            E_FUNCTION
        };

        using Callback = void (*)(void*);

        struct Event
        {
            EventKind meKind;
            void* mpValue;
            Callback mpCallback;
            void* mpUserData;
            std::string mPath;
            std::string mLabel;
        };

        struct Metadata
        {
            bool mbHasRange = false;
            double mfMinimum = 0.0;
            double mfMaximum = 0.0;
            bool mbHasStep = false;
            double mfStep = 0.0;
            bool mbReadOnly = false;
            const DebugUI::StringList* mpaOptions = nullptr;
        };

        int miBaseConstructs = 0;
        int miMetadataCalls = 0;
        std::vector<Event> maEvents;
        std::vector<std::pair<void*, Metadata>> maMetadata;

        void Construct() { ++miBaseConstructs; }

        template <class T>
        void RegisterVariable(T* lpValue, const char* lpcLabel)
        {
            RegisterVariable(lpValue, "", lpcLabel);
        }

        template <class T>
        void RegisterVariable(T* lpValue, const char* lpcPath, const char* lpcLabel)
        {
            maEvents.push_back({E_VARIABLE, static_cast<void*>(lpValue), nullptr, nullptr,
                                lpcPath ? lpcPath : "", lpcLabel ? lpcLabel : ""});
        }

        void RegisterFunction(Callback lpCallback, void* lpUserData, const char* lpcLabel)
        {
            RegisterFunction(lpCallback, lpUserData, "", lpcLabel);
        }

        void RegisterFunction(Callback lpCallback, void* lpUserData,
                              const char* lpcPath, const char* lpcLabel)
        {
            maEvents.push_back({E_FUNCTION, nullptr, lpCallback, lpUserData,
                                lpcPath ? lpcPath : "", lpcLabel ? lpcLabel : ""});
        }

        template <class T, class Minimum, class Maximum>
        void SetRange(T* lpValue, Minimum lMinimum, Maximum lMaximum)
        {
            Metadata& lrMetadata = For(lpValue);
            lrMetadata.mbHasRange = true;
            lrMetadata.mfMinimum = static_cast<double>(lMinimum);
            lrMetadata.mfMaximum = static_cast<double>(lMaximum);
            ++miMetadataCalls;
        }

        template <class T, class Step>
        void SetStep(T* lpValue, Step lStep)
        {
            Metadata& lrMetadata = For(lpValue);
            lrMetadata.mbHasStep = true;
            lrMetadata.mfStep = static_cast<double>(lStep);
            ++miMetadataCalls;
        }

        template <class T>
        void SetReadOnly(T* lpValue, bool lbReadOnly)
        {
            For(lpValue).mbReadOnly = lbReadOnly;
            ++miMetadataCalls;
        }

        template <class T>
        void SetOptions(T* lpValue, const DebugUI::StringList* lpaOptions)
        {
            For(lpValue).mpaOptions = lpaOptions;
            ++miMetadataCalls;
        }

        const Metadata* FindMetadata(const void* lpValue) const
        {
            for (const auto& lrEntry : maMetadata)
                if (lrEntry.first == lpValue)
                    return &lrEntry.second;
            return nullptr;
        }

    private:
        Metadata& For(const void* lpValue)
        {
            for (auto& lrEntry : maMetadata)
                if (lrEntry.first == lpValue)
                    return lrEntry.second;
            maMetadata.push_back({const_cast<void*>(lpValue), Metadata{}});
            return maMetadata.back().second;
        }
    };
}

struct MaybeDrawTextCall
{
    std::string mText;
    f32 mfX;
    f32 mfY;
    f32 mfScale;
    CgsDev::RGBA mColour;
    bool mbCentred;
};

static std::vector<MaybeDrawTextCall> gaMaybeDrawTextCalls;

int MaybeDrawText(CgsDev::Debug2DImmediateRender*, const char* lpcText,
                  f32 lfX, f32 lfY, f32 lfScale, CgsDev::RGBA lColour, bool lbCentred)
{
    const std::string lText = lpcText ? lpcText : "";
    gaMaybeDrawTextCalls.push_back({lText, lfX, lfY, lfScale, lColour, lbCentred});
    gaRenderTrace.emplace_back("text:" + lText);
    return 1;
}

namespace CgsCore
{
    int SPrintf(char* lpcBuffer, size_t luSize, const char* lpcFormat, ...)
    {
        va_list lArgs;
        va_start(lArgs, lpcFormat);
        const int liResult = std::vsnprintf(lpcBuffer, luSize, lpcFormat, lArgs);
        va_end(lArgs);
        return liResult;
    }
}

namespace BrnTraffic
{
    static constexpr s32 E_ACTIVE_RACE_CAR_INDEX_INVALID = -1;
    static constexpr u32 KU_MAX_TOTAL_TRAFFIC = 600;

    struct ParamTransform
    {
        Vector3 mDeterministicPosition = {};
        Vector3 GetDeterministicPos() const { return mDeterministicPosition; }
    };

    struct VehicleTransformRecord
    {
        Vector3 mPosition = {};
        Vector3 Pos() const { return mPosition; }
    };

    struct DEBUG_VehicleFuzzyLogic
    {
        u32 muVehicle = 0;
        Vector3 mRenderPosition = {};
        f32 mafParamOutputScores[6] = {};
    };

    struct CountedArray
    {
        s32 miCount = 0;
        s32 GetLength() const { return miCount; }
    };

    struct FiredKillZoneInfo
    {
        s32 miFramesLeftToRemember;
    };

    template <class T>
    struct DynamicArray
    {
        std::vector<T> maItems;
        s32 GetLength() const { return static_cast<s32>(maItems.size()); }
        T& GetItem(u32 luIndex) { return maItems[luIndex]; }
        void Erase(u32 luIndex) { maItems.erase(maItems.begin() + luIndex); }
    };

    struct Pvs
    {
        u32 muNumCells_X = 0;
        u32 muNumCells = 0;
    };

    struct VehicleTypeData
    {
        u32 muAssetId = 0;
    };

    struct VehicleAssetData
    {
        u64 mId = 0;
        u64 GetVehicleId() const { return mId; }
    };

    struct TrafficData
    {
        u32 muNumHulls = 0;
        u32 muNumVehicleTypes = 0;
        u32 muNumFlowTypes = 0;
        u32 muNumKillZones = 0;
        u64 mpaKillZoneIds[16] = {};
        VehicleTypeData mpaVehicleTypes[16] = {};
        VehicleAssetData mpaVehicleAssets[16] = {};
        Pvs* mpPvs = nullptr;
    };

    struct FuzzyBehaviours
    {
        bool mbDEBUGRenderParamScores = false;
        Vector3 mDEBUGLastCameraPos = {};
        int miReloads = 0;
        void ReloadBehaviours() { ++miReloads; }
    };

    class TrafficEntityModule
    {
    public:
        TrafficData* mpData = nullptr;

        bool mbDEBUGTurnTrafficOff = false;
        s32 miDEBUGOverBudgetness = 0;
        bool mbDEBUGDontRenderMeshes = false;
        bool mbDEBUGShowtimeStuff = false;
        bool mbDEBUGRenderContacts = false;
        bool mbDEBUGStopTrafficMoving = false;
        bool mbDEBUGAllowAnarchy = false;
        f32 mfDEBUGDistanceForAnarchy = 0.0f;
        f32 mfSpeedMultiplier = 0.0f;
        f32 mfDEBUGTrafficLightTimeMultiplier = 0.0f;
        f32 mfGameModeDensityScale = 0.0f;
        f32 mfTrafficAmountScale = 0.0f;
        s32 miBigVehicleAmount = 0;
        s32 miDEBUGOverrideVehicleToSpawn = -1;
        s32 miDEBUGFlowtypeOverride = 0;
        bool mbDEBUGEnableKillzones = false;
        bool mbDEBUGRunningWorstCase = false;
        u32 muNumUpdateVehiclesJobs = 4;
        bool mbDEBUGRenderSpecialPerfmons = false;
        bool mbDEBUGEnableAvoidance = false;
        bool mbDEBUGTestSympCrash = false;
        bool mbDEBUGFakeShowtime = false;
        f32 mfCrashSliderCrashScoreFactor = 0.0f;
        f32 mfCrashSliderCrashScoreDecay = 0.0f;
        f32 mfCrashSliderCrashScore = 0.0f;
        f32 mfCrashSliderFinalValue = 0.0f;
        f32 mfPlayerIdleTime = 0.0f;
        f32 mfShowtimeTimer = 0.0f;
        f32 mfShowtimeTimeNextCrashSpike = 0.0f;
        f32 mfShowtimeMisBounceTimer = 0.0f;
        f32 mfJunctionFUP = 0.0f;
        bool mbDEBUGOverrideJunctionFUP = false;
        bool mbDEBUGPickVehicleFromCamera = false;
        bool mbDEBUGPick_StopVehicle = false;
        bool mbDEBUGPick_DontStopForPickedVehicle = false;
        bool mbDEBUGEnablePressureSystem = false;
        f32 mfTimeSincePlayerWasDrivingQuickly = 0.0f;
        f32 mfTimeSincePlayerHullChange = 0.0f;
        FuzzyBehaviours mFuzzyBehaviours;
        ParamTransform maParamTransforms[400] = {};
        VehicleTransformRecord maVehicleTransforms[KU_MAX_TOTAL_TRAFFIC] = {};
        DEBUG_VehicleFuzzyLogic maDebugFuzzyLogicRecords[40] = {};
        DEBUG_VehicleFuzzyLogic* mpaDEBUGVehicleFuzzyLogic = nullptr;
        u32 muDEBUGVehicleFuzzyLogicCount = 0;
        CgsDev::PerfMonCpuMonitorData mPostPhysPerfmonData_Decision;
        CgsDev::PerfMonCpuMonitorData mParamPerfmonData_Decision;
        CgsDev::PerfMonCpuMonitorData mVehiclePerfmonData_Decision;
        CgsDev::PerfMonCpuMonitorData mPostPhysPerfmonData_NonDecision;
        CgsDev::PerfMonCpuMonitorData mParamPerfmonData_NonDecision;
        CgsDev::PerfMonCpuMonitorData mVehiclePerfmonData_NonDecision;
        CountedArray maPurgatoryList;
        CountedArray mFreeParams;
        CountedArray mActiveHullsForLocalPlayer;
        DynamicArray<u16> mActiveHulls;
        DynamicArray<u16> maaRaceCarHulls[4];
        Vector3 mAveragePhysicalCentre = {};
        Vector3 mLocalPlayerPosition = {};
        s32 meLocalPlayerIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
        s32 meDEBUGAirRamToFire = 0;
        u32 muDEBUGPickedVehicle = 0;
        int miKillAllCalls = 0;
        f32 mfKillRadius = 0.0f;
        f32 mfKillHeight = 0.0f;
        bool mbKillKeepParked = false;
        u64 muLastFiredKillZone = 0;
        int miSoaValidations = 0;
        DynamicArray<FiredKillZoneInfo> mDEBUGRecentlyFiredKillZones;

        ParamTransform* GetParamTransform(u32 luParam) { return &maParamTransforms[luParam]; }
        void DEBUG_AddFuzzyLogicData(u32 luParam, VecFloat* lpafFuzzyOutputs);
        void UpdateDEBUG();
        void KillAllTrafficInCylinder(Vector3, f32 lfRadius, f32 lfHeight, bool lbKeepParked)
        {
            ++miKillAllCalls;
            mfKillRadius = lfRadius;
            mfKillHeight = lfHeight;
            mbKillKeepParked = lbKeepParked;
        }

        void FireKillZone(u64 luId) { muLastFiredKillZone = luId; }
        void DEBUGValidateSoaData() { ++miSoaValidations; }
    };

    class DebugComponent : public CgsDev::DebugComponent
    {
    public:
        TrafficEntityModule* mpModule = nullptr;
        f32 mfRenderCullDistance = 0.0f;
        bool mbDrawSegments = false;
        bool mbDrawSections = false;
        bool mbDrawSectionCurves = false;
        bool mbDrawNeighbours = false;
        bool mbDrawJunctions = false;
        bool mbDrawStopLines = false;
        bool mbDrawStartLines = false;
        bool mbDrawSplitters = false;
        bool mbDrawParams = false;
        bool mbDrawLerpedParams = false;
        bool mbDrawParamBehaviours = false;
        bool mbDrawParamSlowData = false;
        bool mbDrawParamTrails = false;
        bool mbDrawVehicles = false;
        bool mbDrawNonCollidableVehicles = false;
        bool mbDrawCollidableVehicles = false;
        bool mbDrawPhysicalVehicleState = false;
        bool mbDrawStuckDetection = false;
        bool mbDrawVehicleManoeuvres = false;
        bool mbDrawParamVehicleLinks = false;
        bool mbDrawParamParamLinks = false;
        bool mbDrawNearestVehicleId = false;
        bool mbDrawVehicleIds = false;
        bool mbDrawActiveHulls = false;
        bool mbDrawPlayerHullId = false;
        bool mbDrawAllKillZones = false;
        bool mbDrawRecentKillZones = false;
        bool mbDrawSpontaneousCones = false;
        bool mbDrawRearArticPoints = false;
        bool mbDrawFrontArticPoints = false;
        bool mbDrawAxles = false;
        bool mbDrawHorns = false;
        bool mbUseCameraPosition = false;
        s32 miHullToDrawFor = -1;
        s32 miAirRamTypeToFire = 0;
        u32 muKillZoneToFire = 0;
        u32 muDummyVehicleName = 0;
        char macOverrideVehicleName[13] = {};
        s32 miNumActiveParams = -1;
        s32 miNumPurgatoryParams = -1;
        s32 miNumFreeParams = -1;
        s32 miNumActiveHulls = -1;
        bool mbDrawVehicleSwervingDebug = false;
        bool mbDrawAvoidance = false;
        bool mbDrawAvoidanceAllShit = false;
        bool mbDrawPressure = false;
        bool mbDrawJunctionFUPDetection = false;
        u32 muCurrentVehicleToPick = static_cast<u32>(-1);
        mutable std::vector<std::string> maDrawCalls;

        void Construct(TrafficEntityModule* lpModule);
        const char* GetName() const;
        void Update();
        void OnActivate();
        bool UseCameraPosition() const { return mbUseCameraPosition; }
        void UpdateStats();
        static void FireAirRam(void* lpUserData);
        static void OnKillNearPlayer(void* lpUserData);
        static void FireKillZone(void* lpUserData);
        static void OnPickVehicle(void* lpUserData);
        static void ReloadBehaviourData(void* lpUserData);
        void RenderWorld(CgsDev::Debug3DImmediateRender* lpDisplay);
        void RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay);
        void RenderPerformanceTable(CgsDev::Debug2DImmediateRender* lpDisplay);
        void RenderPerfmon(CgsDev::Debug2DImmediateRender* lpDisplay,
                           const CgsDev::PerfMonCpuMonitorData* lpData,
                           f32 lfX, f32 lfY, f32 lfScale);
        void DrawActiveHulls(CgsDev::Debug2DImmediateRender* lpDisplay) const;
        void DrawPlayerCarHullId(CgsDev::Debug2DImmediateRender* lpDisplay) const;
        void DrawTextWithOffsets(CgsDev::DebugRender& lrDisplay, CgsDev::StrStream* lpStream,
                                 f32 lfX, f32 lfY) const;

#define TRAFFIC_DEBUG_DRAW(name) \
        void name(CgsDev::Debug3DImmediateRender*) { maDrawCalls.emplace_back(#name); }
        TRAFFIC_DEBUG_DRAW(DrawSegments)
        TRAFFIC_DEBUG_DRAW(DrawSections)
        TRAFFIC_DEBUG_DRAW(DrawSectionCurves)
        TRAFFIC_DEBUG_DRAW(DrawNeighbours)
        TRAFFIC_DEBUG_DRAW(DrawJunctions)
        TRAFFIC_DEBUG_DRAW(DrawStopLines)
        TRAFFIC_DEBUG_DRAW(DrawStartLines)
        TRAFFIC_DEBUG_DRAW(DrawSplitters)
        TRAFFIC_DEBUG_DRAW(DrawParams)
        TRAFFIC_DEBUG_DRAW(DrawLerpedParams)
        TRAFFIC_DEBUG_DRAW(DrawParamBehaviours)
        TRAFFIC_DEBUG_DRAW(DrawParamSlowData)
        TRAFFIC_DEBUG_DRAW(DrawParamTrails)
        TRAFFIC_DEBUG_DRAW(DrawVehicles)
        TRAFFIC_DEBUG_DRAW(DrawNonCollidableVehicles)
        TRAFFIC_DEBUG_DRAW(DrawCollidableVehicles)
        TRAFFIC_DEBUG_DRAW(DrawPhysicalVehicleState)
        TRAFFIC_DEBUG_DRAW(DrawStuckDetection)
        TRAFFIC_DEBUG_DRAW(DrawVehicleManoeuvres)
        TRAFFIC_DEBUG_DRAW(DrawParamVehicleLinks)
        TRAFFIC_DEBUG_DRAW(DrawParamParamLinks)
        TRAFFIC_DEBUG_DRAW(DrawNearestVehicleId)
        TRAFFIC_DEBUG_DRAW(DrawVehicleIds)
        TRAFFIC_DEBUG_DRAW(DrawAllKillZones)
        TRAFFIC_DEBUG_DRAW(DrawRecentKillZones)
        TRAFFIC_DEBUG_DRAW(DrawFrontArticPoints)
        TRAFFIC_DEBUG_DRAW(DrawRearArticPoints)
        TRAFFIC_DEBUG_DRAW(DrawAxles)
        TRAFFIC_DEBUG_DRAW(DrawHorns)
        TRAFFIC_DEBUG_DRAW(DrawAvoidance)
        TRAFFIC_DEBUG_DRAW(DrawFuzzyLogic)
        TRAFFIC_DEBUG_DRAW(DrawShowtime)
        TRAFFIC_DEBUG_DRAW(DrawPressure)
        TRAFFIC_DEBUG_DRAW(DrawVehicleSwervingDebugging)
        TRAFFIC_DEBUG_DRAW(DrawPickedVehicle)
#undef TRAFFIC_DEBUG_DRAW

    };

    f32 kfSideRamProportion = 0.15f;
    f32 kfSideRamStrength = 0.04f;
    f32 unk_8300D018 = 0.0f;
    f32 unk_82F2FFFC = 0.01f;
    f32 unk_82F30000 = 0.05f;
    f32 unk_82F30004 = 0.01f;
    f32 unk_82F30008 = 0.05f;
    f32 unk_82F3000C = 0.15f;
    f32 unk_82F30010 = 0.4f;
    f32 unk_82F30014 = 1.0f;
    f32 unk_82F30018 = 0.04f;
    f32 unk_82F3001C = 0.06f;
    f32 unk_82F30020 = 0.07f;
    f32 unk_82F30024 = 0.09f;
    f32 unk_82F30028 = 0.5f;
    f32 unk_82F30030 = 20.0f;
    f32 unk_82F30034 = 3.0f;
    f32 unk_82F30038 = 30.0f;

    static const CgsDev::DebugUI::StringList KA_AIR_RAM_TYPE_OPTIONS[] = {
        {0, "SPIN CW FRONT"}, {1, "SPIN CW REAR"}, {2, "SPIN CCW FRONT"},
        {3, "SPIN CCW REAR"}, {4, "ROLL LEFT"}, {5, "ROLL RIGHT"},
        {6, "SPIN OUT"}, {7, "TEST"}, {0, nullptr},
    };
}

static constexpr u32 KU_MAX_PARAMS = 400;

void CgsIDUnCompress(u64 luId, char* lpcName)
{
    std::snprintf(lpcName, 13, "ID_%llu", luId);
}

struct ExpectedRegistration
{
    CgsDev::DebugComponent::EventKind meKind;
    const char* mpcPath;
    const char* mpcCaption;
};

#include "traffic_debug_expected.inc"

namespace BrnTraffic
{
#include "traffic_debug_menu_methods.inc"
}

static const CgsDev::DebugComponent::Event* FindEvent(
    const BrnTraffic::DebugComponent& lrComponent, const char* lpcCaption,
    CgsDev::DebugComponent::EventKind leKind = CgsDev::DebugComponent::E_VARIABLE)
{
    for (const auto& lrEvent : lrComponent.maEvents)
        if (lrEvent.meKind == leKind && lrEvent.mLabel == lpcCaption)
            return &lrEvent;
    return nullptr;
}

static void CheckStorage(const BrnTraffic::DebugComponent& lrComponent,
                         const char* lpcCaption, const void* lpExpected)
{
    const auto* lpEvent = FindEvent(lrComponent, lpcCaption);
    Check(lpEvent != nullptr && lpEvent->mpValue == lpExpected, lpcCaption);
}

int main()
{
    using namespace BrnTraffic;

    TrafficData lData = {};
    lData.muNumHulls = 12;
    lData.muNumVehicleTypes = 7;
    lData.muNumFlowTypes = 9;
    lData.muNumKillZones = 4;
    lData.mpaKillZoneIds[2] = 0x123456789ABCDEF0ull;
    lData.mpaVehicleTypes[1].muAssetId = 3;
    lData.mpaVehicleAssets[3].mId = 707;
    Pvs lPvs;
    lPvs.muNumCells_X = 10;
    lPvs.muNumCells = 20;
    lData.mpPvs = &lPvs;

    TrafficEntityModule lModule;
    lModule.mpData = &lData;
    lModule.maaRaceCarHulls[0].maItems = {73};
    lModule.maPurgatoryList.miCount = 5;
    lModule.mFreeParams.miCount = 13;
    lModule.mActiveHullsForLocalPlayer.miCount = 3;
    lModule.mActiveHulls.maItems = {2, 4, 6, 8, 10};

    DebugComponent lComponent;
    lComponent.Construct(&lModule);
    Check(lComponent.miBaseConstructs == 1, "Construct calls the base component constructor once");
    Check(lComponent.mpModule == &lModule && lComponent.mfRenderCullDistance == 200.0f,
          "Construct stores its owner and seeds the cull distance");
    Check(!lComponent.mbDrawSegments && !lComponent.mbDrawSections && !lComponent.mbDrawSectionCurves &&
          !lComponent.mbDrawNeighbours && !lComponent.mbDrawJunctions && !lComponent.mbDrawStopLines &&
          !lComponent.mbDrawStartLines && !lComponent.mbDrawSplitters && !lComponent.mbDrawParams &&
          !lComponent.mbDrawLerpedParams && !lComponent.mbDrawParamBehaviours &&
          !lComponent.mbDrawParamSlowData && !lComponent.mbDrawParamTrails && !lComponent.mbDrawVehicles &&
          !lComponent.mbDrawNonCollidableVehicles && !lComponent.mbDrawCollidableVehicles &&
          !lComponent.mbDrawPhysicalVehicleState && !lComponent.mbDrawStuckDetection &&
          !lComponent.mbDrawVehicleManoeuvres && !lComponent.mbDrawParamVehicleLinks &&
          !lComponent.mbDrawParamParamLinks && !lComponent.mbDrawNearestVehicleId &&
          !lComponent.mbDrawVehicleIds && !lComponent.mbDrawActiveHulls && !lComponent.mbDrawPlayerHullId &&
          !lComponent.mbDrawAllKillZones && !lComponent.mbDrawRecentKillZones &&
          !lComponent.mbDrawSpontaneousCones && !lComponent.mbDrawRearArticPoints &&
          !lComponent.mbDrawFrontArticPoints && !lComponent.mbDrawAxles && !lComponent.mbDrawHorns &&
          !lComponent.mbUseCameraPosition && !lComponent.mbDrawVehicleSwervingDebug &&
          !lComponent.mbDrawAvoidance && !lComponent.mbDrawAvoidanceAllShit && !lComponent.mbDrawPressure &&
          !lComponent.mbDrawJunctionFUPDetection,
          "Construct clears the full original set of draw toggles");
    Check(!lComponent.UseCameraPosition(), "UseCameraPosition reads its default-disabled component byte");
    Check(lComponent.miHullToDrawFor == -1 && lComponent.miAirRamTypeToFire == 0 &&
          lComponent.muKillZoneToFire == 0 && lComponent.muCurrentVehicleToPick == static_cast<u32>(-1),
          "Construct seeds selectors and pick sentinel");
    Check(std::strcmp(lComponent.macOverrideVehicleName, "DEFAULT") == 0 &&
          lComponent.miNumActiveParams == -1 && lComponent.miNumPurgatoryParams == -1 &&
          lComponent.miNumFreeParams == -1 && lComponent.miNumActiveHulls == -1,
          "Construct seeds the name and four counters");
    Check(std::strcmp(lComponent.GetName(), "Traffic") == 0, "page name remains Traffic");

    lComponent.OnActivate();
    Check(lComponent.maEvents.size() == 108, "all original registrations execute in one activation");
    Check(lComponent.miMetadataCalls == 55,
          "all original direct and wrapped range, step, options, and read-only calls execute");

    bool lbAllRowsMatch = lComponent.maEvents.size() == 108;
    for (size_t luIndex = 0; lbAllRowsMatch && luIndex < lComponent.maEvents.size(); ++luIndex)
    {
        const auto& lrActual = lComponent.maEvents[luIndex];
        const auto& lrExpected = gExpectedTrafficRegistrations[luIndex];
        const bool lbCaptionMatches =
            (std::strcmp(lrExpected.mpcCaption, "$macOverrideVehicleName") == 0)
                ? (lrActual.mLabel == lComponent.macOverrideVehicleName)
                : (lrActual.mLabel == lrExpected.mpcCaption);
        lbAllRowsMatch = lrActual.meKind == lrExpected.meKind &&
                         lrActual.mPath == lrExpected.mpcPath && lbCaptionMatches;
    }
    Check(lbAllRowsMatch, "all 108 rows preserve original order, kind, group, and caption");

    CheckStorage(lComponent, "Turn traffic off", &lModule.mbDEBUGTurnTrafficOff);
    CheckStorage(lComponent, "Over budget-ness", &lModule.miDEBUGOverBudgetness);
    CheckStorage(lComponent, "Active params", &lComponent.miNumActiveParams);
    CheckStorage(lComponent, "Cull distance", &lComponent.mfRenderCullDistance);
    CheckStorage(lComponent, "Use camera position", &lComponent.mbUseCameraPosition);
    CheckStorage(lComponent, "DEFAULT", &lComponent.muDummyVehicleName);
    CheckStorage(lComponent, "Render param fuzzy scores",
                 &lModule.mFuzzyBehaviours.mbDEBUGRenderParamScores);
    CheckStorage(lComponent, "Vehicle to spawn", &lModule.miDEBUGOverrideVehicleToSpawn);
    CheckStorage(lComponent, "Hull to draw for (-1 is all)", &lComponent.miHullToDrawFor);
    CheckStorage(lComponent, "KillZone to fire", &lComponent.muKillZoneToFire);
    CheckStorage(lComponent, "Running Worst Case", &lModule.mbDEBUGRunningWorstCase);
    CheckStorage(lComponent, "Junction FUP", &lModule.mfJunctionFUP);

    const auto* lpHullMetadata = lComponent.FindMetadata(&lComponent.miHullToDrawFor);
    const auto* lpSpawnMetadata = lComponent.FindMetadata(&lModule.miDEBUGOverrideVehicleToSpawn);
    const auto* lpFlowMetadata = lComponent.FindMetadata(&lModule.miDEBUGFlowtypeOverride);
    const auto* lpKillMetadata = lComponent.FindMetadata(&lComponent.muKillZoneToFire);
    const auto* lpJobsMetadata = lComponent.FindMetadata(&lModule.muNumUpdateVehiclesJobs);
    Check(lpHullMetadata && lpHullMetadata->mbHasRange && lpHullMetadata->mfMinimum == -1.0 &&
          lpHullMetadata->mfMaximum == 7.0,
          "the later Vehicle-to-spawn range write still re-targets the hull selector");
    Check(!lpSpawnMetadata || !lpSpawnMetadata->mbHasRange,
          "Vehicle to spawn keeps its original absence of range metadata");
    Check(lpFlowMetadata && lpFlowMetadata->mbHasRange && lpFlowMetadata->mfMinimum == -1.0 &&
          lpFlowMetadata->mfMaximum == 8.0,
          "flowtype selector retains the -1 through count-minus-one range");
    Check(lpKillMetadata && lpKillMetadata->mbHasRange && lpKillMetadata->mfMinimum == 0.0 &&
          lpKillMetadata->mfMaximum == 3.0,
          "kill-zone selector retains the original data-bound range");
    Check(lpJobsMetadata && lpJobsMetadata->mbHasRange && lpJobsMetadata->mfMinimum == 1.0 &&
          lpJobsMetadata->mfMaximum == 4.0,
          "update-vehicle jobs retain their 1 through 4 range");
    const auto* lpAirRamMetadata = lComponent.FindMetadata(&lComponent.miAirRamTypeToFire);
    Check(lpAirRamMetadata && lpAirRamMetadata->mpaOptions == KA_AIR_RAM_TYPE_OPTIONS &&
          KA_AIR_RAM_TYPE_OPTIONS[7].miValue == 7 && KA_AIR_RAM_TYPE_OPTIONS[8].mpcName == nullptr,
          "air-ram option table preserves values 0 through 7 and the sentinel");

    const struct GlobalBinding
    {
        const char* mpcCaption;
        f32* mpValue;
    } laCheckingGlobals[] = {
        {"Side ram proportion", &kfSideRamProportion},
        {"Side ram strength", &kfSideRamStrength},
        {"Min strength stat for traffic check", &unk_8300D018},
        {"After check min first ram strength", &unk_82F2FFFC},
        {"After check max first ram strength", &unk_82F30000},
        {"After slam min first ram strength", &unk_82F30004},
        {"After slam max first ram strength", &unk_82F30008},
        {"After check first ram decay", &unk_82F3000C},
        {"After check min swerve time (seconds)", &unk_82F30010},
        {"After check max swerve time (seconds)", &unk_82F30014},
        {"After check flipout spin min ram strength", &unk_82F30018},
        {"After check flipout spin max ram strength", &unk_82F3001C},
        {"After check flipout flip min ram strength", &unk_82F30020},
        {"After check flipout flip max ram strength", &unk_82F30024},
        {"Racecar kick strength", &unk_82F30028},
        {"Slow driving speed", &unk_82F30030},
        {"Max slow driving time", &unk_82F30034},
        {"Max time in one hull", &unk_82F30038},
    };
    bool lbAllGlobalBindingsMatch = true;
    for (const GlobalBinding& lrGlobal : laCheckingGlobals)
    {
        const auto* lpEvent = FindEvent(lComponent, lrGlobal.mpcCaption);
        lbAllGlobalBindingsMatch &= lpEvent && lpEvent->mpValue == lrGlobal.mpValue;
    }
    Check(lbAllGlobalBindingsMatch, "all 18 original Checking and Traffic Jams globals remain live storage");

    const auto* lpReadOnly = lComponent.FindMetadata(&lModule.mfTrafficAmountScale);
    const auto* lpNameReadOnly = lComponent.FindMetadata(&lComponent.muDummyVehicleName);
    Check(lpReadOnly && lpReadOnly->mbReadOnly && lpNameReadOnly && lpNameReadOnly->mbReadOnly,
          "FINAL density scale and the dynamic override name stay read-only");
    lComponent.mbUseCameraPosition = true;
    Check(lComponent.UseCameraPosition(), "Use camera position toggles its X360 sim-centre override");

    lComponent.Update();
    Check(lComponent.miNumPurgatoryParams == 5 && lComponent.miNumFreeParams == 13 &&
          lComponent.miNumActiveParams == 382 && lComponent.miNumActiveHulls == 5 &&
          lModule.mActiveHullsForLocalPlayer.miCount == 3 && lModule.mActiveHulls.GetLength() == 5 &&
          std::strcmp(lComponent.macOverrideVehicleName, "DEFAULT") == 0,
          "Update reports the global active-hull count, not the distinct local-player count");

    lModule.mpaDEBUGVehicleFuzzyLogic = lModule.maDebugFuzzyLogicRecords;
    lModule.mFuzzyBehaviours.mbDEBUGRenderParamScores = true;
    lModule.mFuzzyBehaviours.mDEBUGLastCameraPos = {100, 200, 300, 0};
    lModule.maParamTransforms[7].mDeterministicPosition = {110, 200, 300, 0};
    lModule.maVehicleTransforms[7].mPosition = {11, 20, 30, 0};
    VecFloat laFuzzyScores[6] = {
        {0.1f, 0, 0, 0}, {0.2f, 0, 0, 0}, {0.3f, 0, 0, 0},
        {0.4f, 0, 0, 0}, {0.5f, 0, 0, 0}, {0.6f, 0, 0, 0}};
    lModule.DEBUG_AddFuzzyLogicData(7, laFuzzyScores);
    Check(lModule.muDEBUGVehicleFuzzyLogicCount == 1 &&
          lModule.maDebugFuzzyLogicRecords[0].muVehicle == 7 &&
          lModule.maDebugFuzzyLogicRecords[0].mRenderPosition.x == 11.0f &&
          lModule.maDebugFuzzyLogicRecords[0].mRenderPosition.y == 22.5f &&
          lModule.maDebugFuzzyLogicRecords[0].mRenderPosition.z == 30.0f &&
          lModule.maDebugFuzzyLogicRecords[0].mafParamOutputScores[0] == 0.1f &&
          lModule.maDebugFuzzyLogicRecords[0].mafParamOutputScores[5] == 0.6f,
          "fuzzy debug producer records typed transform and six raw rule scores");
    lModule.maParamTransforms[7].mDeterministicPosition = {120, 200, 300, 0};
    lModule.maVehicleTransforms[7].mPosition = {21, 30, 40, 0};
    laFuzzyScores[0].x = 0.9f;
    lModule.DEBUG_AddFuzzyLogicData(7, laFuzzyScores);
    Check(lModule.muDEBUGVehicleFuzzyLogicCount == 2 &&
          lModule.maDebugFuzzyLogicRecords[0].muVehicle == 7 &&
          lModule.maDebugFuzzyLogicRecords[0].mRenderPosition.x == 21.0f &&
          lModule.maDebugFuzzyLogicRecords[0].mRenderPosition.y == 32.5f &&
          lModule.maDebugFuzzyLogicRecords[0].mafParamOutputScores[0] == 0.9f &&
          lModule.maDebugFuzzyLogicRecords[1].muVehicle == 0,
          "fuzzy debug producer refreshes the matching record and advances the ring cursor");
    lModule.maParamTransforms[7].mDeterministicPosition = {130, 200, 300, 0};
    lModule.DEBUG_AddFuzzyLogicData(7, laFuzzyScores);
    Check(lModule.muDEBUGVehicleFuzzyLogicCount == 2,
          "fuzzy debug producer excludes the original 900-unit boundary");
    lModule.mFuzzyBehaviours.mbDEBUGRenderParamScores = false;
    lModule.maParamTransforms[7].mDeterministicPosition = {110, 200, 300, 0};
    lModule.DEBUG_AddFuzzyLogicData(7, laFuzzyScores);
    Check(lModule.muDEBUGVehicleFuzzyLogicCount == 2,
          "fuzzy debug producer remains gated by the original page toggle");
    lModule.mActiveHulls.maItems.clear();

    lModule.miDEBUGOverrideVehicleToSpawn = 1;
    lComponent.Update();
    Check(std::strcmp(lComponent.macOverrideVehicleName, "ID_707") == 0,
          "Update resolves the selected type through its asset id");

    const auto* lpFireAirRam = FindEvent(lComponent, "Fire!", CgsDev::DebugComponent::E_FUNCTION);
    lComponent.miAirRamTypeToFire = 6;
    lpFireAirRam->mpCallback(lpFireAirRam->mpUserData);
    Check(lModule.meDEBUGAirRamToFire == 6, "Fire! copies the selected air-ram type to the module latch");

    const auto* lpFireKillZone = FindEvent(lComponent, "Fire KillZone!", CgsDev::DebugComponent::E_FUNCTION);
    lComponent.muKillZoneToFire = 2;
    lpFireKillZone->mpCallback(lpFireKillZone->mpUserData);
    Check(lModule.muLastFiredKillZone == 0x123456789ABCDEF0ull,
          "Fire KillZone! selects the original table id and forwards it to the module");

    const auto* lpKillNearPlayer = FindEvent(lComponent, "Kill traffic near player",
                                             CgsDev::DebugComponent::E_FUNCTION);
    lModule.meLocalPlayerIndex = 0;
    lpKillNearPlayer->mpCallback(lpKillNearPlayer->mpUserData);
    Check(lModule.miKillAllCalls == 1 && lModule.mfKillRadius == 150.0f &&
          lModule.mfKillHeight == 1000.0f && !lModule.mbKillKeepParked,
          "near-player action uses the original cylinder and valid-player guard");
    lModule.meLocalPlayerIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
    lpKillNearPlayer->mpCallback(lpKillNearPlayer->mpUserData);
    Check(lModule.miKillAllCalls == 1, "near-player action is inert without a valid local player");

    const auto* lpReload = FindEvent(lComponent, "Reload Behaviour Data", CgsDev::DebugComponent::E_FUNCTION);
    lpReload->mpCallback(lpReload->mpUserData);
    Check(lModule.mFuzzyBehaviours.miReloads == 1, "reload action reaches the owning fuzzy behaviours");

    const auto* lpPick = FindEvent(lComponent, "Pick vehicle", CgsDev::DebugComponent::E_FUNCTION);
    lComponent.muCurrentVehicleToPick = 19;
    lpPick->mpCallback(lpPick->mpUserData);
    Check(lModule.muDEBUGPickedVehicle == 19, "pick action copies the selected vehicle id");

    CgsDev::Debug3DImmediateRender lWorldRenderer;
    lComponent.maDrawCalls.clear();
    lComponent.RenderWorld(&lWorldRenderer);
    Check(lComponent.maDrawCalls.empty() && !lWorldRenderer.mbDrewSphere,
          "RenderWorld is quiet while all controls are off");

    lComponent.mbDrawSegments = true;
    lComponent.mbDrawSections = true;
    lComponent.mbDrawFrontArticPoints = true;
    lComponent.mbDrawRearArticPoints = true;
    lComponent.mbDrawAvoidance = true;
    lModule.mFuzzyBehaviours.mbDEBUGRenderParamScores = true;
    lModule.mbDEBUGShowtimeStuff = true;
    lComponent.mbDrawPressure = true;
    lComponent.mbDrawVehicleSwervingDebug = true;
    lComponent.mbDrawJunctionFUPDetection = true;
    lModule.mbDEBUGPickVehicleFromCamera = true;
    lModule.mfJunctionFUP = 64.0f;
    lWorldRenderer = {};
    lComponent.maDrawCalls.clear();
    lComponent.RenderWorld(&lWorldRenderer);
    Check(lComponent.maDrawCalls == std::vector<std::string>({
              "DrawSegments", "DrawSections", "DrawFrontArticPoints", "DrawRearArticPoints",
              "DrawAvoidance", "DrawFuzzyLogic", "DrawShowtime", "DrawPressure",
              "DrawVehicleSwervingDebugging", "DrawPickedVehicle"}) && lWorldRenderer.mbDrewSphere,
          "RenderWorld preserves toggle-to-reader order around the inline junction sphere");
    Check(lWorldRenderer.mfSphereRadius == 60.0f && lWorldRenderer.mSphereColour.m_rgba == 0x64640000u,
          "junction FUP below 65 uses the original sphere radius and packed colour");
    lModule.mfJunctionFUP = 65.0f;
    lWorldRenderer = {};
    lComponent.RenderWorld(&lWorldRenderer);
    Check(lWorldRenderer.mSphereColour.m_rgba == 0x640000FFu,
          "junction FUP at the 65 boundary selects the original alternate colour");

    CgsDev::Debug2DImmediateRender lHudRenderer;
    lComponent.maDrawCalls.clear();
    lComponent.mbDrawPlayerHullId = true;
    lComponent.mbDrawActiveHulls = true;
    lModule.mbDEBUGRenderSpecialPerfmons = true;
    lModule.meLocalPlayerIndex = 0;
    gaMaybeDrawTextCalls.clear();
    gaRenderTrace.clear();
    lComponent.RenderHUD(&lHudRenderer);
    Check(gaMaybeDrawTextCalls.size() == 38 && lHudRenderer.maLines.size() == 14 &&
          gaRenderTrace.size() > 15 && gaRenderTrace[0] == "text:Traf Hull: 73" &&
          gaRenderTrace[15] == "text:Traffic [special]",
          "RenderHUD preserves player-hull, active-grid, and perf-table order through their real bodies");
    lComponent.maDrawCalls.clear();
    lComponent.mbDrawPlayerHullId = false;
    lComponent.mbDrawActiveHulls = false;
    lModule.mbDEBUGRenderSpecialPerfmons = false;
    gaMaybeDrawTextCalls.clear();
    gaRenderTrace.clear();
    lComponent.RenderHUD(&lHudRenderer);
    Check(gaRenderTrace.empty(), "RenderHUD is quiet when its three controls are off");

    lModule.mPostPhysPerfmonData_Decision = {
        "PostPhysicsUpdate", 1.234f, 2.345f, 0.456f, 8.0f, 7, 9, false, 0.0f, 0.0f, 0.0f};
    lModule.mParamPerfmonData_Decision = {
        "UpdateParam", 2.5f, 3.5f, 1.5f, 8.0f, 11, 13, false, 0.0f, 0.0f, 0.0f};
    lModule.mVehiclePerfmonData_Decision = {
        "UpdateVehicle", 4.0f, 5.0f, 2.0f, 8.0f, 17, 19, false, 0.0f, 0.0f, 0.0f};
    lModule.mPostPhysPerfmonData_NonDecision = {
        "PostPhysicsUpdate ND", 1.0f, 1.5f, 0.5f, 8.0f, 3, 4, false, 0.0f, 0.0f, 0.0f};
    lModule.mParamPerfmonData_NonDecision = {
        "UpdateParam ND", 2.0f, 2.5f, 1.0f, 8.0f, 5, 6, false, 0.0f, 0.0f, 0.0f};
    lModule.mVehiclePerfmonData_NonDecision = {
        "UpdateVehicle ND", 3.0f, 3.5f, 1.5f, 8.0f, 7, 8, false, 0.0f, 0.0f, 0.0f};
    gaMaybeDrawTextCalls.clear();
    lComponent.RenderPerformanceTable(&lHudRenderer);
    Check(gaMaybeDrawTextCalls.size() == 37,
          "performance table emits its five headings, two sections, and six five-cell rows");
    Check(gaMaybeDrawTextCalls.size() == 37 &&
          gaMaybeDrawTextCalls[0].mText == "Traffic [special]" &&
          gaMaybeDrawTextCalls[0].mfX == 690.0f && gaMaybeDrawTextCalls[0].mfY == 76.0f &&
          gaMaybeDrawTextCalls[0].mfScale == 24.0f &&
          gaMaybeDrawTextCalls[7].mText == "1.234" &&
          gaMaybeDrawTextCalls[8].mText == "2.345" &&
          gaMaybeDrawTextCalls[10].mText == "7" &&
          gaMaybeDrawTextCalls[21].mText == "NON-DECISION" &&
          gaMaybeDrawTextCalls[36].mText == "7",
          "performance table preserves original screen-width centering and monitor cell formatting");

    lModule.mActiveHulls.maItems = {3, 15};
    lHudRenderer.maBoxes.clear();
    lHudRenderer.maLines.clear();
    lComponent.DrawActiveHulls(&lHudRenderer);
    Check(lHudRenderer.maBoxes.size() == 2 && lHudRenderer.maLines.size() == 14,
          "active-hull display draws each active cell and the original 10-by-2 grid");
    Check(lHudRenderer.maBoxes.size() == 2 &&
          lHudRenderer.maBoxes[0].mPosition.x == 131.0f &&
          lHudRenderer.maBoxes[0].mPosition.y == 80.0f &&
          lHudRenderer.maBoxes[1].mPosition.x == 165.0f &&
          lHudRenderer.maBoxes[1].mPosition.y == 165.0f &&
          lHudRenderer.maBoxes[0].mSize.x == 17.0f &&
          lHudRenderer.maBoxes[0].mSize.y == 85.0f &&
          lHudRenderer.maBoxes[0].mColour == 0x88FFFF00u,
          "active-hull coordinates use stored column count and position-plus-size boxes");

    lModule.meLocalPlayerIndex = 1;
    lModule.maaRaceCarHulls[0].maItems = {73};
    gaMaybeDrawTextCalls.clear();
    lComponent.DrawPlayerCarHullId(&lHudRenderer);
    Check(gaMaybeDrawTextCalls.size() == 2 &&
          gaMaybeDrawTextCalls[0].mText == "Warning: Hull ID is for car zero, not player" &&
          gaMaybeDrawTextCalls[1].mText == "Traf Hull: 73",
          "player hull label keeps the car-zero warning and reads race-car slot zero");

    char lacMultiLineInput[64];
    CgsDev::StrStream lMultiLineStream(lacMultiLineInput, sizeof(lacMultiLineInput));
    std::strcpy(lacMultiLineInput, "alpha\nbeta\n");
    CgsDev::DebugRender lDebugRender;
    lComponent.DrawTextWithOffsets(lDebugRender, &lMultiLineStream, 10.0f, 20.0f);
    Check(lDebugRender.maText.size() == 3 &&
          lDebugRender.maText[0].mText == "alpha\n" &&
          lDebugRender.maText[1].mText == "beta\n" &&
          lDebugRender.maText[2].mText.empty(),
          "offset text keeps newline delimiters and the final empty draw after a trailing newline");
    Check(lDebugRender.maText.size() == 3 &&
          lDebugRender.maText[0].mfY == 20.0f && lDebugRender.maText[1].mfY == 32.0f &&
          lDebugRender.maText[2].mfY == 44.0f && lDebugRender.maText[0].mfScale == 12.0f,
          "offset text advances each original line by twelve screen units");

    std::vector<char> lacLongInput(3001, 'x');
    lacLongInput.back() = '\0';
    CgsDev::StrStream lLongInputStream(lacLongInput.data(),
                                       static_cast<s32>(lacLongInput.size()));
    std::fill(lacLongInput.begin(), lacLongInput.end() - 1, 'x');
    lacLongInput.back() = '\0';
    lDebugRender.maText.clear();
    lComponent.DrawTextWithOffsets(lDebugRender, &lLongInputStream, 0.0f, 0.0f);
    Check(lDebugRender.maText.size() == 1 && lDebugRender.maText[0].mText.size() == 3000,
          "offset text uses the original 4096-byte local stream for long lines");

    lModule.mDEBUGRecentlyFiredKillZones.maItems = {{1}, {2}, {0}};
    lModule.UpdateDEBUG();
    Check(lModule.mDEBUGRecentlyFiredKillZones.GetLength() == 1 &&
          lModule.mDEBUGRecentlyFiredKillZones.GetItem(0).miFramesLeftToRemember == 1 &&
          lModule.miSoaValidations == 1,
          "UpdateDEBUG ages and erases expired records before validating the SoA");
    Check(giAssertions == 0, "the exercised original paths stay assertion-clean");

    std::printf("TrafficDebugMenu: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures == 0 ? 0 : 1;
}
