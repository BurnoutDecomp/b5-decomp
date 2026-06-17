#ifndef CGS_DEBUG_COMPONENT_PERF_MON_CPU_H
#define CGS_DEBUG_COMPONENT_PERF_MON_CPU_H

#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"

// CgsDev::DebugComponentPerfMonCpu - the on-screen CPU performance overlay: the debug component whose
// RenderHUD draws one coloured bar per registered CPU monitor (the "debug squares" on the loading
// screen). Recovered from the DecFIGS DWARF (Development/PerfMon/DebugComponent/CgsDebugComponentPerfMonCpu.h).
// Reads the per-monitor times from the PerfMonCpu registry and draws them through Debug2DImmediateRender.
//
// BOUNDED: the reconstructed bodies are Construct (member init), OnActivate, Update, RenderHUD ->
// RenderPerformanceTable (the bars). The graph mode, page navigation, the reset/dump/tracing debug
// callbacks, and the CPU-trace log buffer are declared (full interface) but are the perfmon follow-on;
// the bars need none of them. GetName/GetPath/IsSimple are the inline overrides the X360 header carries.

namespace CgsDev
{
    class DebugComponentPerfMonCpu : public DebugComponent
    {
    public:
        void Construct( s16 liMaxMonitors, void* lpLogBuffer, u32 luLogBufferSize );
        void Destruct();
        virtual void RenderHUD( Debug2DImmediateRender* lpDebug2DRender );
        virtual void Update();
        void SetPageName( s32 liPage, const char* lpcPageName );
        void GetCurrentPage( s32* lpiPage, char* lpcName, s32 liNameLen );

    protected:
        virtual const char* GetName() const { return "CPU Monitors"; }
        virtual const char* GetPath() const { return "Core/Debug/Performance"; }
        virtual bool IsSimple() const { return false; }
        virtual void OnActivate();

        void RenderPerformanceTable( Debug2DImmediateRender* lpDebug2DRender );
        void RenderPerformanceGraph( Debug2DImmediateRender* lpDebug2DRenderr );
        bool mbDisplayAsGraph;
        f32 mfMaxCpu;

        bool mbResetMonitors;
        static void DebugCallbackResetCounters( void* );
        static void DebugCallbackDumpLogFile( void* );

        bool mbEnableTracing;
        static void DebugEnableTracingCallback(void* lpValue,void* lpUserData);

        s32 miTraceMode;
        DebugUI::StringList maTraceModeList[3];
        static void DebugChangeTraceModeCallback(void* lpValue,void* lpUserData);

        static const f32 KF_COLORBARHEIGHT;

        s32 miCurrentPage;
        DebugUI::StringList maStringList[E_PMP_MAX+2];

        // Optional CPU-trace log buffer (filled when tracing is enabled).
        void LogBufferEnable( void* lpBuffer, s32 liSize );
        void LogBufferUpdate();
        void LogBufferDump( char* lpcFileName );

        void* mpLogBuffer;
        void* mpLogBufferEnd;
        bool mbLogBufferOverflow;
        u16* mpau16LogBufferWritePtr;
    };
}

#endif
