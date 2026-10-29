#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/PerfMon/CgsPerfMon.h"

// Native ARTIST CPU profiling registry; declaration names and shapes from DecFIGS.
namespace rw { struct IResourceAllocator; }

namespace CgsDev
{
    // X360 CgsPerfMonCpu.h:40.
    static const s32 KI_PERFMONCPU_MAXSTRINGLENGTH = 32;

    // The page a monitor is grouped under in the overlay (X360 CgsPerfMonCpu.h:47).
    enum PerfMonCpuPage
    {
        E_PMP_GENERAL = 0,
        E_PMP_1       = 1,
        E_PMP_2       = 2,
        E_PMP_3       = 3,
        E_PMP_4       = 4,   // CgsPhysics::PhysicsSimulationModule::Construct's four "Sim *" monitors
        E_PMP_5       = 5,   // [stuntrace waveB] BrnGameState::ModeManager::Construct's two
                             // "ModeManager PreWorld" / "ModeManager PostWorld" monitors: the
                             // AddMonitor call at 0x823406C4 is preceded by `li r4, 5` @0x823406BC
                             // (with r3 = "ModeManager PostWorld", r5 = 0, r7 = 1, f1 = the 1.0f
                             // budget at flt_82001C98)
        E_PMP_6       = 6,   // BrnPhysics::Vehicle::VehicleManager::Construct's "PHYS ValidateRCWorldContact"
        E_PMP_7       = 7,
        E_PMP_8       = 8,
        E_PMP_9       = 9,   // BrnNetwork::BrnServerInterfaceX360::Construct's two "Int - * Update" monitors
        E_PMP_10      = 10,
        E_PMP_11      = 11,
        E_PMP_12      = 12,  // VehicleManager::Construct's other twenty-nine "VMan: ..." monitors (`li r4, 0xC`)
        E_PMP_13      = 13,
        E_PMP_14      = 14,
        E_PMP_15      = 15,
        E_PMP_16      = 16,
        E_PMP_17      = 17,
        E_PMP_18      = 18,
        E_PMP_19      = 19,
        E_PMP_20      = 20,
        E_PMP_21      = 21,  // LionPerfMon::Construct's twenty-three Lion monitors (`li r4, 0x15`
                             // @0x82279EB4 and twenty-two more, all with r5 = 0, r7 = 0 and the
                             // f1 budget flt_820049E0 == 100.0f loaded once into f31 @0x82279EB8)
        E_PMP_22      = 22,
        E_PMP_23      = 23,
        E_PMP_MAX     = 24,
    };

    // One registered CPU monitor (X360 CgsPerfMonCpu.h:118). mu64StartValue / mu64Value hold raw timer
    // ticks (the start stamp + the last region's elapsed ticks); mfCurrentValue is that elapsed time in
    // percent of one game frame - what the overlay draws as a bar.
    struct PerfMonCpuInstance
    {
        u64            mu64StartValue;
        u64            mu64Value;
        s32            miFrameCounter;
        s32            miNumCalls;
        s32            miMaxCalls;
        char           macName[KI_PERFMONCPU_MAXSTRINGLENGTH];
        bool           mbActive;
        bool           mbMinimum;
        bool           mbScaled;
        bool           mbLibPerfTagged;
        PerfMonCpuPage mePage;
        f32            mfCurrentValue;
        f32            mfMinMaxValue;
        f32            mfAverageValue;
        f32            mfAverageAccumulator;
        f32            mfCpuBudget;
        s32            miOrigLibPerfTraceId;
        s32            miLibPerfTraceId;
    };

    // The per-monitor snapshot the overlay / report callback reads (X360 CgsPerfMonCpu.h:86).
    struct PerfMonCpuMonitorData
    {
        const char* mpcName;
        f32         mfCurrentValue;
        f32         mfAverageValue;
        f32         mfMinMaxValue;
        f32         mfCpuBudget;
        s32         miNumCalls;
        s32         miMaxCalls;
        bool        mbTraced;
        f32         mfCurrentTraceValue;
        f32         mfAverageTraceValue;
        f32         mfMinMaxTraceValue;
    };

    typedef void FPerfMonCpuReportCallback(const PerfMonCpuMonitorData& lrData, void* lpUserData);

    struct DebugComponentPerfMonCpu;

    struct PerfMonCpu
    {
        static bool Construct(s32 liMaxMonitorCount, rw::IResourceAllocator* lpAllocator);
        static void Destruct();
        static void SetGameFrequency(PerfMonGameFrequency leFrequency);
        static void StartProfiling();
        static void StopProfiling();
        static void PrepareActiveMonitorsForUpdate();
        static void ResetValuesInActiveMonitors();
        static s32 AddMonitor(const char* lpcName, PerfMonCpuPage lePage, bool lbMinimum,
                              f32 lfCpuBudget, bool lbScaled);
        static void StartMonitor(s32 liMonitorHandle);
        static void StopMonitor(s32 liMonitorHandle);
        static void SetNumIterationsTaken(s32 liNumIterations);
        static s32 GetMonitorCount();
        static s32 GetMaxMonitorCount();
        static f32 GetMonitorTime(s32 liMonitorHandle);
        static void GetMonitorData(s32 liMonitorHandle, PerfMonCpuMonitorData* lpData);
        static bool IsMonitorOverBudget(s32 liMonitorHandle);
        static PerfMonCpuPage GetMonitorPage(s32 liMonitorHandle);
        static void ReportMonitors(FPerfMonCpuReportCallback* lpfReportCallback,
                                   void* lpUserData, PerfMonCpuPage lePage);
        static void AddPIXCounters();

    private:
        friend struct DebugComponentPerfMonCpu;
        static PerfMonCpuInstance* mpMonitorsArray;
        static s32 miMaxMonitorCount;
        static s32 miMonitorCount;
        static PerfMonGameFrequency meGameFrequency;
        static bool mbResetAllValues;
        static s32 miNumIterations;
        static bool mbIgnoreZeroCallsInAverage;
        static s32 miLibPerfMode;
    };
}
