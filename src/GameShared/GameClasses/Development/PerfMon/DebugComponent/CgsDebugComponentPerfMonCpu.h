#ifndef CGS_DEBUG_COMPONENT_PERF_MON_CPU_H
#define CGS_DEBUG_COMPONENT_PERF_MON_CPU_H

#include "DebugSystem/Core/CgsDebugComponent.h"
#include "DebugSystem/Core/UI/CgsTypes.h"
#include "PerfMon/Cpu/CgsPerfMonCpu.h"

namespace CgsDev
{

class DebugComponentPerfMonCpu : public DebugComponent
{
public:
    void Construct( int16_t liMaxMonitors, void* lpLogBuffer, uint32_t luLogBufferSize );
    void Destruct();
    virtual void RenderHUD( Debug2DImmediateRender* lpDebug2DRender );
    virtual void Update();
    void SetPageName( int32_t liPage, const char* lpcPageName );
    void GetCurrentPage( int32_t* lpiPage, char* lpcName, int32_t liNameLen );

protected:
    virtual const char* GetName() const { return "CPU Monitors"; }
    virtual const char* GetPath() const { return "Core/Debug/Performance"; }
    virtual bool IsSimple() const { return false; }
    virtual void OnActivate();

    void RenderPerformanceTable( Debug2DImmediateRender* lpDebug2DRender );
    void RenderPerformanceGraph( Debug2DImmediateRender* lpDebug2DRenderr );
    bool mbDisplayAsGraph;
    float mfMaxCpu;

    bool mbResetMonitors;
    static void DebugCallbackResetCounters( void* );
    static void DebugCallbackDumpLogFile( void* );

    bool mbEnableTracing;
    static void DebugEnableTracingCallback(void* lpValue,void* lpUserData);

    int32_t miTraceMode;
    DebugUI::StringList maTraceModeList[3];
    static void DebugChangeTraceModeCallback(void* lpValue,void* lpUserData);

    static const float32_t KF_COLORBARHEIGHT;

    int32_t miCurrentPage;
    DebugUI::StringList maStringList[E_PMP_MAX+2];

    // Optional CPU-trace log buffer (filled when tracing is enabled).
    void LogBufferEnable( void* lpBuffer, int32_t liSize );
    void LogBufferUpdate();
    void LogBufferDump( char* lpcFileName );

    void* mpLogBuffer;
    void* mpLogBufferEnd;
    bool mbLogBufferOverflow;
    uint16_t* mpau16LogBufferWritePtr;
};

}

#endif
