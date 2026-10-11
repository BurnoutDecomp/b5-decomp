#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>

using f32 = float;
using s32 = std::int32_t;
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;

enum Event
{
    E_MODULE_BASE_CONSTRUCT,
    E_DEBUG_BASE_CONSTRUCT,
    E_RESOURCE_BIND,
    E_DEBUG_REGISTER,
    E_RESOURCE_GET,
    E_ASTAR_CONSTRUCT,
    E_MODULE_BASE_PREPARE,
    E_MODULE_BASE_RELEASE,
    E_MODULE_BASE_DESTRUCT,
    E_ASSERT,
    E_DEBUG_BASE_DESTRUCT,
};

static Event gaEvents[64];
static s32 giEventCount;
static s32 giChecks;
static s32 giFailures;
static s32 giAssertionCount;
static const char* gapAssertions[8];
static bool gbBaseSawSeedIds;
static bool gbDebugSawZeroIds;
static bool gbDebugSawNewFlagFalse;
static bool gbBasePrepareResult;
static bool gbBaseReleaseResult;

static void PushEvent(Event leEvent)
{
    if (giEventCount < static_cast<s32>(sizeof(gaEvents) / sizeof(gaEvents[0])))
        gaEvents[giEventCount++] = leEvent;
}

static void HarnessAssert(bool lbCondition, const char* lpcExpression)
{
    if (lbCondition)
        return;
    if (giAssertionCount < static_cast<s32>(sizeof(gapAssertions) / sizeof(gapAssertions[0])))
        gapAssertions[giAssertionCount] = lpcExpression;
    ++giAssertionCount;
    PushEvent(E_ASSERT);
}

static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass)
    {
        ++giFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

static bool Near(f32 lfLeft, f32 lfRight)
{
    return std::fabs(lfLeft - lfRight) < 0.00001f;
}

static bool IsEventSequence(const Event* lpaExpected, s32 liCount)
{
    if (giEventCount != liCount)
        return false;
    for (s32 li = 0; li < liCount; ++li)
        if (gaEvents[li] != lpaExpected[li])
            return false;
    return true;
}

static void ResetEvents()
{
    giEventCount = 0;
}

static void ResetRegistrations();

namespace BrnAI
{
    struct AISectionsData
    {
        u32 muNumSections;
    };

    class AIModule
    {
    };

    class RouteMapModule;
}

namespace CgsDev
{
namespace DebugUI
{
    struct StringList
    {
        s32 miValue;
        const char* mpcName;
    };

    using DebugCallbackFunction = void (*)(void*);
}

class DebugComponent
{
public:
    void Construct();
    void Destruct();
    void Register();

    void RegisterVariable(bool* lpValue, const char* lpcName);
    void RegisterVariable(s32* lpValue, const char* lpcName);
    void RegisterVariable(f32* lpValue, const char* lpcName);
    void SetRange(s32* lpValue, s32 liMinimum, s32 liMaximum);
    void SetRange(f32* lpValue, f32 lfMinimum, f32 lfMaximum);
    void SetStep(f32* lpValue, f32 lfStep);
    void SetOptions(s32* lpValue, const DebugUI::StringList* lpaOptions);
    void RegisterFunction(DebugUI::DebugCallbackFunction lpCallback, void* lpUserData,
                          const char* lpcName);
};
}

enum ValueKind
{
    E_BOOL_VALUE,
    E_S32_VALUE,
    E_F32_VALUE,
};

struct VariableRecord
{
    void* mpValue;
    const char* mpcName;
    ValueKind meKind;
};

struct FunctionRecord
{
    CgsDev::DebugUI::DebugCallbackFunction mpCallback;
    void* mpUserData;
    const char* mpcName;
};

struct RangeRecord
{
    void* mpValue;
    f32 mfMinimum;
    f32 mfMaximum;
    bool mbFloat;
};

struct StepRecord
{
    void* mpValue;
    f32 mfStep;
};

static VariableRecord gaVariables[32];
static s32 giVariableCount;
static FunctionRecord gaFunctions[8];
static s32 giFunctionCount;
static RangeRecord gaRanges[8];
static s32 giRangeCount;
static StepRecord gaSteps[8];
static s32 giStepCount;
static s32* gapOptionsValue;
static const CgsDev::DebugUI::StringList* gapOptions;

static void ResetRegistrations()
{
    giVariableCount = 0;
    giFunctionCount = 0;
    giRangeCount = 0;
    giStepCount = 0;
    gapOptionsValue = nullptr;
    gapOptions = nullptr;
}

namespace CgsResource
{
    struct ResourceHandle
    {
        BrnAI::AISectionsData* mpMemoryResource;
    };
}

namespace BrnAI
{
    template <class T>
    class ResourcePtr
    {
    public:
        T* mpMemoryResource = nullptr;

        ResourcePtr<T>& operator=(const CgsResource::ResourceHandle& lrHandle)
        {
            PushEvent(E_RESOURCE_BIND);
            mpMemoryResource = lrHandle.mpMemoryResource;
            return *this;
        }

        T* GetMemoryResource() const
        {
            PushEvent(E_RESOURCE_GET);
            return mpMemoryResource;
        }
    };

    class AStar
    {
    public:
        AISectionsData* mpSectionsData = nullptr;
        void Construct(AISectionsData* lpSectionsData);
    };
}

namespace CgsModule
{
    class ModuleSingleBuffered
    {
    public:
        bool mbIsNewModule = false;
        void Construct();
        bool Prepare();
        bool Release();
        void Destruct();
    };
}

namespace BrnAI
{
    class RouteMapDebugComponent : public CgsDev::DebugComponent
    {
    public:
        void Construct(RouteMapModule* lpRouteMapModule, AIModule* lpAIModule);
        void Destruct();
        const char* GetName() const;
        const char* GetPath() const;
        void OnActivate();
        static void Advance100SectionsCallback(void* lpUserData);
        static void Back100SectionsCallback(void* lpUserData);

        RouteMapModule* mpRouteMapModule = nullptr;
        AISectionsData* mpAISectionsData = nullptr;
        AIModule* mpAIModule = nullptr;
        f32 mfMapScale = 0.0f;
        f32 mfMapOffsetX = 0.0f;
        f32 mfMapOffsetY = 0.0f;
        s32 miCarIndex = 0;
        s32 miMapColourMode = 0;
        bool mbDrawAISectionMap = false;
        bool mbHighDetailMap = false;
        bool mbDrawCarsInMap = false;
        bool mbDrawCarRoutesInMap = false;
        bool mbDrawMasterRouteInMap = false;
        bool mbDrawAStarInMap = false;
        bool mbCarInfoInMap = false;
        bool mbDrawDeadEndsInMap = false;
        bool mbDrawEventBlockSectionsInMap = false;
        bool mbOnlyDrawOneCar = false;
        bool mbOnlyDrawDataForPlayerSection = false;
        bool mbDrawPortals = false;
        bool mbDrawPortalIds = false;
        bool mbDrawPortalLines = false;
        bool mbDrawHNGAsPlanes = false;
        bool mbShowSectionInfo = false;
        s32 miAISectionIndex = 0;
        CgsDev::DebugUI::StringList maColourModeList[3] = {};
    };

    class RouteMapModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        void Construct(AIModule* lpAIModule);
        bool Prepare(CgsResource::ResourceHandle lHandle);
        bool Release();
        void Destruct();
        AISectionsData* GetAISectionsData() const { return mAISectionsData.GetMemoryResource(); }

        AStar mAStar;
        ResourcePtr<AISectionsData> mAISectionsData;
        u16 muEventId = 0;
        u16 muOwnerId = 0;
        RouteMapDebugComponent mRoupeMapDebugComponent;
    };

    static const f32 KF_MAP_SCALE_DEFAULT = 0.1f;
    static const f32 KF_MAP_OFFSET_X_DEFAULT = 580.0f;
    static const f32 KF_MAP_OFFSET_Y_DEFAULT = 450.0f;
}

static BrnAI::RouteMapModule* gpLifecycleModule;

namespace CgsModule
{
void ModuleSingleBuffered::Construct()
{
    PushEvent(E_MODULE_BASE_CONSTRUCT);
    mbIsNewModule = false;
    if (gpLifecycleModule != nullptr)
        gbBaseSawSeedIds = gpLifecycleModule->muEventId == 0xA55Au &&
                           gpLifecycleModule->muOwnerId == 0x5AA5u;
}

bool ModuleSingleBuffered::Prepare()
{
    PushEvent(E_MODULE_BASE_PREPARE);
    return gbBasePrepareResult;
}

bool ModuleSingleBuffered::Release()
{
    PushEvent(E_MODULE_BASE_RELEASE);
    return gbBaseReleaseResult;
}

void ModuleSingleBuffered::Destruct()
{
    PushEvent(E_MODULE_BASE_DESTRUCT);
}
}

namespace CgsDev
{
void DebugComponent::Construct()
{
    PushEvent(E_DEBUG_BASE_CONSTRUCT);
    if (gpLifecycleModule != nullptr)
    {
        gbDebugSawZeroIds = gpLifecycleModule->muEventId == 0 && gpLifecycleModule->muOwnerId == 0;
        gbDebugSawNewFlagFalse = !gpLifecycleModule->mbIsNewModule;
    }
}

void DebugComponent::Destruct() { PushEvent(E_DEBUG_BASE_DESTRUCT); }
void DebugComponent::Register() { PushEvent(E_DEBUG_REGISTER); }

void DebugComponent::RegisterVariable(bool* lpValue, const char* lpcName)
{
    gaVariables[giVariableCount++] = {lpValue, lpcName, E_BOOL_VALUE};
}

void DebugComponent::RegisterVariable(s32* lpValue, const char* lpcName)
{
    gaVariables[giVariableCount++] = {lpValue, lpcName, E_S32_VALUE};
}

void DebugComponent::RegisterVariable(f32* lpValue, const char* lpcName)
{
    gaVariables[giVariableCount++] = {lpValue, lpcName, E_F32_VALUE};
}

void DebugComponent::SetRange(s32* lpValue, s32 liMinimum, s32 liMaximum)
{
    gaRanges[giRangeCount++] = {lpValue, static_cast<f32>(liMinimum), static_cast<f32>(liMaximum), false};
}

void DebugComponent::SetRange(f32* lpValue, f32 lfMinimum, f32 lfMaximum)
{
    gaRanges[giRangeCount++] = {lpValue, lfMinimum, lfMaximum, true};
}

void DebugComponent::SetStep(f32* lpValue, f32 lfStep)
{
    gaSteps[giStepCount++] = {lpValue, lfStep};
}

void DebugComponent::SetOptions(s32* lpValue, const DebugUI::StringList* lpaOptions)
{
    gapOptionsValue = lpValue;
    gapOptions = lpaOptions;
}

void DebugComponent::RegisterFunction(DebugUI::DebugCallbackFunction lpCallback, void* lpUserData,
                                      const char* lpcName)
{
    gaFunctions[giFunctionCount++] = {lpCallback, lpUserData, lpcName};
}
}

namespace BrnAI
{
void AStar::Construct(AISectionsData* lpSectionsData)
{
    PushEvent(E_ASTAR_CONSTRUCT);
    mpSectionsData = lpSectionsData;
}
}

#define CGS_ASSERT(condition, expression) HarnessAssert((condition), (expression))
namespace BrnAI
{
#include "route_map_debug_lifecycle_methods.inc"
}
#undef CGS_ASSERT

static void CheckName(const VariableRecord& lrRecord, void* lpValue, ValueKind leKind,
                      const char* lpcName)
{
    Check(lrRecord.mpValue == lpValue && lrRecord.meKind == leKind &&
              std::strcmp(lrRecord.mpcName, lpcName) == 0,
          lpcName);
}

int main()
{
    BrnAI::AIModule lAIModule;
    BrnAI::AISectionsData lSections = {0x12345u};
    BrnAI::RouteMapModule lModule;
    gpLifecycleModule = &lModule;
    lModule.muEventId = 0xA55Au;
    lModule.muOwnerId = 0x5AA5u;
    lModule.mbIsNewModule = true;
    ResetEvents();

    lModule.Construct(&lAIModule);
    const Event laConstructOrder[] = {E_MODULE_BASE_CONSTRUCT, E_DEBUG_BASE_CONSTRUCT};
    Check(IsEventSequence(laConstructOrder, 2), "module base Construct precedes embedded debug Construct");
    Check(gbBaseSawSeedIds, "base Construct runs before both route ids are cleared");
    Check(lModule.muEventId == 0 && lModule.muOwnerId == 0, "Construct clears event and owner ids");
    Check(gbDebugSawZeroIds && gbDebugSawNewFlagFalse,
          "debug Construct sees cleared ids before the new-module flag is set");
    Check(lModule.mbIsNewModule, "Construct sets the new-module flag after debug Construct");
    Check(lModule.mRoupeMapDebugComponent.mpRouteMapModule == &lModule &&
              lModule.mRoupeMapDebugComponent.mpAIModule == &lAIModule,
          "embedded component retains the route-map and AI owners");
    Check(lModule.mRoupeMapDebugComponent.mpAISectionsData == nullptr,
          "Construct defers section-data binding until activation");

    BrnAI::RouteMapDebugComponent& lrDebug = lModule.mRoupeMapDebugComponent;
    bool* lpaToggles[] = {
        &lrDebug.mbDrawAISectionMap, &lrDebug.mbHighDetailMap, &lrDebug.mbDrawCarsInMap,
        &lrDebug.mbDrawCarRoutesInMap, &lrDebug.mbDrawMasterRouteInMap, &lrDebug.mbDrawAStarInMap,
        &lrDebug.mbCarInfoInMap, &lrDebug.mbDrawDeadEndsInMap, &lrDebug.mbDrawEventBlockSectionsInMap,
        &lrDebug.mbOnlyDrawOneCar, &lrDebug.mbOnlyDrawDataForPlayerSection, &lrDebug.mbDrawPortals,
        &lrDebug.mbDrawPortalIds, &lrDebug.mbDrawPortalLines, &lrDebug.mbDrawHNGAsPlanes,
        &lrDebug.mbShowSectionInfo,
    };
    bool lbAllTogglesOff = true;
    for (bool* lpToggle : lpaToggles)
        lbAllTogglesOff = lbAllTogglesOff && !*lpToggle;
    Check(lbAllTogglesOff, "all 16 debug toggles start disabled");
    Check(Near(lrDebug.mfMapScale, 0.1f) && Near(lrDebug.mfMapOffsetX, 580.0f) &&
              Near(lrDebug.mfMapOffsetY, 450.0f),
          "map scale and offsets use the original defaults");
    Check(lrDebug.miCarIndex == 0 && lrDebug.miMapColourMode == 0 && lrDebug.miAISectionIndex == 0,
          "car, colour-mode, and section indices start at zero");
    Check(lrDebug.maColourModeList[0].miValue == 0 &&
              std::strcmp(lrDebug.maColourModeList[0].mpcName, "Plain") == 0 &&
              lrDebug.maColourModeList[1].miValue == 1 &&
              std::strcmp(lrDebug.maColourModeList[1].mpcName, "Type") == 0 &&
              lrDebug.maColourModeList[2].miValue == 2 &&
              std::strcmp(lrDebug.maColourModeList[2].mpcName, "Speed") == 0,
          "dropdown retains the three original value/name pairs");
    Check(std::strcmp(lrDebug.GetName(), "Route Map") == 0, "debug component name");
    Check(std::strcmp(lrDebug.GetPath(), "AI") == 0, "debug component path");

    const CgsResource::ResourceHandle lHandle = {&lSections};
    gbBasePrepareResult = false;
    ResetEvents();
    Check(!lModule.Prepare(lHandle), "Prepare returns the base false result");
    const Event laPrepareOrder[] = {E_RESOURCE_BIND, E_DEBUG_REGISTER, E_RESOURCE_GET,
                                    E_ASTAR_CONSTRUCT, E_MODULE_BASE_PREPARE};
    Check(IsEventSequence(laPrepareOrder, 5), "Prepare binds, registers, constructs A-star, then calls base");
    Check(lModule.mAISectionsData.mpMemoryResource == &lSections &&
              lModule.mAStar.mpSectionsData == &lSections,
          "Prepare binds the same section resource into the module and A-star");
    gbBasePrepareResult = true;
    ResetEvents();
    Check(lModule.Prepare(lHandle), "Prepare returns the base true result");
    Check(IsEventSequence(laPrepareOrder, 5), "true-result Prepare keeps the same lifecycle order");

    ResetRegistrations();
    ResetEvents();
    lrDebug.OnActivate();
    Check(lrDebug.mpAISectionsData == &lSections, "OnActivate resolves sections through its module owner");
    Check(giVariableCount == 22, "OnActivate registers all 22 original variables");
    const VariableRecord laExpectedVariables[] = {
        {&lrDebug.mbDrawAISectionMap, "Draw AI section map", E_BOOL_VALUE},
        {&lrDebug.mbHighDetailMap, "High detail map", E_BOOL_VALUE},
        {&lrDebug.miMapColourMode, "Map colour mode", E_S32_VALUE},
        {&lrDebug.mbDrawCarsInMap, "Draw cars in map", E_BOOL_VALUE},
        {&lrDebug.mbDrawMasterRouteInMap, "Draw master route in map", E_BOOL_VALUE},
        {&lrDebug.mbDrawCarRoutesInMap, "Draw car routes in map", E_BOOL_VALUE},
        {&lrDebug.mbCarInfoInMap, "Draw car info in map", E_BOOL_VALUE},
        {&lrDebug.mbDrawAStarInMap, "Draw A* in map", E_BOOL_VALUE},
        {&lrDebug.mbDrawEventBlockSectionsInMap, "Draw event block sections", E_BOOL_VALUE},
        {&lrDebug.mbDrawDeadEndsInMap, "Draw dead ends in map", E_BOOL_VALUE},
        {&lrDebug.mbOnlyDrawOneCar, "Only draw one car", E_BOOL_VALUE},
        {&lrDebug.miCarIndex, "Car Index", E_S32_VALUE},
        {&lrDebug.mfMapScale, "Map scale", E_F32_VALUE},
        {&lrDebug.mfMapOffsetX, "Map offset X", E_F32_VALUE},
        {&lrDebug.mfMapOffsetY, "Map offset Y", E_F32_VALUE},
        {&lrDebug.mbDrawPortals, "Draw Portals", E_BOOL_VALUE},
        {&lrDebug.mbDrawPortalIds, "Draw Portal IDs", E_BOOL_VALUE},
        {&lrDebug.mbDrawPortalLines, "Draw Portal Geometry", E_BOOL_VALUE},
        {&lrDebug.mbDrawHNGAsPlanes, "Draw HNG as planes", E_BOOL_VALUE},
        {&lrDebug.mbOnlyDrawDataForPlayerSection, "Player section data only", E_BOOL_VALUE},
        {&lrDebug.mbShowSectionInfo, "Show AI section info", E_BOOL_VALUE},
        {&lrDebug.miAISectionIndex, "Section Index", E_S32_VALUE},
    };
    for (s32 li = 0; li < static_cast<s32>(sizeof(laExpectedVariables) / sizeof(laExpectedVariables[0])); ++li)
        CheckName(gaVariables[li], laExpectedVariables[li].mpValue, laExpectedVariables[li].meKind,
                  laExpectedVariables[li].mpcName);

    Check(giRangeCount == 4, "OnActivate sets the four original variable ranges");
    Check(giRangeCount > 0 && gaRanges[0].mpValue == &lrDebug.miMapColourMode &&
              gaRanges[0].mfMinimum == 0.0f && gaRanges[0].mfMaximum == 2.0f && !gaRanges[0].mbFloat,
          "colour-mode range is 0 through 2");
    Check(giRangeCount > 1 && gaRanges[1].mpValue == &lrDebug.miCarIndex &&
              gaRanges[1].mfMinimum == 0.0f && gaRanges[1].mfMaximum == 34.0f && !gaRanges[1].mbFloat,
          "car-index range is 0 through 34");
    Check(giRangeCount > 2 && gaRanges[2].mpValue == &lrDebug.mfMapScale &&
              Near(gaRanges[2].mfMinimum, 0.05f) && Near(gaRanges[2].mfMaximum, 1.0f) &&
              gaRanges[2].mbFloat,
          "map-scale range is 0.05 through 1.0");
    Check(giRangeCount > 3 && gaRanges[3].mpValue == &lrDebug.miAISectionIndex &&
              gaRanges[3].mfMinimum == 0.0f && gaRanges[3].mfMaximum == 0x2345u && !gaRanges[3].mbFloat,
          "section-index maximum uses the low 16 bits of the live section count");
    Check(giStepCount == 3 && gaSteps[0].mpValue == &lrDebug.mfMapScale &&
              Near(gaSteps[0].mfStep, 0.02f) && gaSteps[1].mpValue == &lrDebug.mfMapOffsetX &&
              Near(gaSteps[1].mfStep, 10.0f) && gaSteps[2].mpValue == &lrDebug.mfMapOffsetY &&
              Near(gaSteps[2].mfStep, 10.0f),
          "scale and map-offset steps match the original menu controls");
    Check(gapOptionsValue == &lrDebug.miMapColourMode && gapOptions == lrDebug.maColourModeList,
          "colour-mode dropdown is attached to the registered integer and its option list");
    Check(giFunctionCount == 2 && gaFunctions[0].mpCallback == &BrnAI::RouteMapDebugComponent::Advance100SectionsCallback &&
              gaFunctions[0].mpUserData == &lrDebug && std::strcmp(gaFunctions[0].mpcName, "+100 Sections") == 0 &&
              gaFunctions[1].mpCallback == &BrnAI::RouteMapDebugComponent::Back100SectionsCallback &&
              gaFunctions[1].mpUserData == &lrDebug && std::strcmp(gaFunctions[1].mpcName, "-100 Sections") == 0,
          "both section-navigation callbacks are registered with the component as user data");
    lrDebug.miAISectionIndex = 7;
    gaFunctions[0].mpCallback(gaFunctions[0].mpUserData);
    Check(lrDebug.miAISectionIndex == 107, "+100 callback changes the registered section index");
    gaFunctions[1].mpCallback(gaFunctions[1].mpUserData);
    Check(lrDebug.miAISectionIndex == 7, "-100 callback reverses the registered section index");

    gbBaseReleaseResult = false;
    ResetEvents();
    Check(!lModule.Release(), "Release returns the base false result");
    const Event laReleaseOrder[] = {E_MODULE_BASE_RELEASE};
    Check(IsEventSequence(laReleaseOrder, 1), "Release delegates directly to the base");
    gbBaseReleaseResult = true;
    ResetEvents();
    Check(lModule.Release(), "Release returns the base true result");

    ResetEvents();
    lModule.Destruct();
    const Event laDestructOrder[] = {E_MODULE_BASE_DESTRUCT, E_DEBUG_BASE_DESTRUCT};
    Check(IsEventSequence(laDestructOrder, 2), "module base Destruct precedes embedded debug teardown");
    Check(lrDebug.mpRouteMapModule == nullptr, "debug teardown clears the route-map back-pointer");
    Check(lrDebug.mpAIModule == &lAIModule && lrDebug.mpAISectionsData == &lSections,
          "debug teardown retains the AI owner and resolved section resource");

    gpLifecycleModule = nullptr;
    BrnAI::RouteMapDebugComponent lInvalidDebug;
    ResetEvents();
    giAssertionCount = 0;
    lInvalidDebug.Construct(nullptr, &lAIModule);
    Check(giAssertionCount == 1 && std::strcmp(gapAssertions[0], "lpRouteMapModule != NULL") == 0,
          "Construct preserves the original non-null route-map assertion");
    Check(lInvalidDebug.mpRouteMapModule == nullptr && lInvalidDebug.mpAIModule == &lAIModule,
          "failed route-map assertion does not discard the supplied AI owner");
    lInvalidDebug.Destruct();
    Check(giAssertionCount == 2 &&
              std::strcmp(gapAssertions[1], "mpRouteMapModule != NULL") == 0,
          "Destruct preserves the original non-null back-pointer assertion");
    const Event laInvalidDestructOrder[] = {E_DEBUG_BASE_CONSTRUCT, E_ASSERT, E_ASSERT,
                                            E_DEBUG_BASE_DESTRUCT};
    Check(IsEventSequence(laInvalidDestructOrder, 4), "asserting debug teardown still nulls then calls base Destruct");
    Check(lInvalidDebug.mpRouteMapModule == nullptr && lInvalidDebug.mpAIModule == &lAIModule,
          "debug Destruct clears only the route-map back-pointer");

    std::printf("RouteMapDebugLifecycle: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures == 0 ? 0 : 1;
}
