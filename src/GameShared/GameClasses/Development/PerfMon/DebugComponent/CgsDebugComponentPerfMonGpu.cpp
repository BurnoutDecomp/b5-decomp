#include "GameShared/GameClasses/Development/PerfMon/DebugComponent/CgsDebugComponentPerfMonGpu.h"

#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h" // DrawBox/DrawFrame/DrawText
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"             // DebugManager::GetInstance()
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"               // GetUI().GetMetrics()
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"                 // DebugUI::Metrics
#include "GameShared/GameClasses/Development/PerfMon/Gpu/CgsPerfMonGpu.h"                    // PerfMonGpu / PerfMonGpuMonitorData
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                                      // CgsCore::SPrintf

#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdlib>
#include <cmath>  // std::fma preserves the X360 fused scale-bar endpoint

// CgsDev::DebugComponentPerfMonGpu - the GPU performance overlay bodies. RenderHUD picks the table or
// graph view; both walk the PerfMonGpu registry via ReportMonitors, drawing one row/bar per monitor
// through the static RenderTableCallback / RenderGraphCallback (the shared draw cursor + running total
// are threaded through RenderCallbackParameter). The layout metrics come from the debug UI's screen
// Metrics (row/text height + the first draw origin).

namespace CgsDev
{
    namespace
    {
        // ARTIST RenderGraph constants, read from the original image with x360rd.py and checked
        // against the loads/stores in 0x8282CEA0 and 0x8281F250. The two scales differ by one ULP.
        const f32 KF_GRAPHSEGMENTSCALE      = 2.8800001f;       // flt_820DC614, word 0x403851EC
        const f32 KF_GRAPHREFERENCEBARSCALE = 2.8799999f;       // flt_820DD6F4, word 0x403851EB
        const f32 KF_GRAPHBARHEIGHT         = 4.0f;             // flt_82004EF4

        const f32 KF_GRAPHORIGINX  = 32.0f;                     // flt_82013FA8
        const f32 KF_GRAPHORIGINY  = 416.0f;                    // flt_820DDC58
        const f32 KF_GRAPHSCALEMIN = 0.0f;                      // flt_82001CC0
        const f32 KF_GRAPHSCALEMAX = 200.0f;                    // flt_8201A1F0

        const f32 KF_GRAPHFRAMERIGHT  = 608.0f;                 // flt_820DD6FC
        const f32 KF_GRAPHFRAMEBOTTOM = 420.0f;                 // flt_8206B4A0
        const f32 KF_GRAPHFRAMEBORDER = 2.0f;                   // flt_82001D9C

        const f32 KF_GRAPHCOLUMNPITCH = 144.0f;                 // flt_82020A78
        const f32 KF_GRAPHGRIDHALF    = 5.0f;                    // flt_8200426C
        const f32 KF_GRAPHGRIDEND     = 7.0f;                    // flt_820054D0, relative to origin Y
        const f32 KF_GRAPHLABELDROP   = 8.0f;                    // flt_820DC2E0
        const f32 KF_GRAPHLABEL50INSET = 8.0f;                   // same flt_820DC2E0 load
        const f32 KF_GRAPHLABELINSET  = 16.0f;                   // flt_82004000
        const f32 KF_GRAPHLABELSIZE   = 15.0f;                   // flt_820047C4
        const f32 KF_GRAPHLABEL100X   = 288.0f;                  // flt_820DDC4C
        const f32 KF_GRAPHLABEL150X   = 432.0f;                  // flt_820DDC40
        const f32 KF_GRAPHLABEL200X   = 576.0f;                  // flt_820DDC34

        const f32 KF_TABLETITLEHEIGHT = 1.5f;                    // flt_82004D04
        const f32 KF_TABLEVALUEOFFSET = 270.0f;                  // flt_82058984

        const RGBA KC_GRAPHGRID     = 0xFFFFFFFFu;               // li r26,-1
        const RGBA KC_GRAPHFRAME    = 0x80000000u;               // lis r8,-0x8000
        const RGBA KC_GRAPHLABEL    = 0xFF6666FFu;               // lis/ori immediate in 0x8282CEA0
        const RGBA KC_GRAPHSCALEBAR = 0x80252525u;               // lis/ori immediate in 0x8282CEA0

    }

    // PS3 DecFIGS @0xB213B0; the X360 inlines the identical body into DebugManager::Construct
    // (@0x828333C8-E4: +0x10 <- 0.0 [flt_82001CC0], +0xD <- 0, +0xC <- 1, then
    // PerfMonGpu::Construct). Reset the overlay state and bring up the GPU perfmon registry.
    void DebugComponentPerfMonGpu::Construct()
    {
        mfMaxGpu         = 0.0f;
        mbVisible        = true;
        mbDisplayAsGraph = false;
        PerfMonGpu::Construct();
    }

    // X360 CgsDev::DebugComponentPerfMonGpu::OnActivate @ 0x8282F488.
    // Register the two overlay toggles with the debug menu under this component's own path.
    // The asm inlines DebugComponent::RegisterVariable(bool*, name) twice: each builds a Variant
    // (tag 8 = E_TYPE_PTR_BOOL, pointing at the member), fetches the component path via
    // GetComponentPath, and funnels it into GetUI().GetVariableManager().RegisterVariable (see
    // CgsDebugComponent.cpp:187, the leaf RegisterVariable(bool*, name) overload).
    void DebugComponentPerfMonGpu::OnActivate()
    {
        RegisterVariable( &mbVisible,        "Draw GPU monitors" );
        RegisterVariable( &mbDisplayAsGraph, "Draw As Graph" );
    }

    // X360 CgsDev::DebugComponentPerfMonGpu::RenderHUD @ 0x8282F520.
    // Draw nothing unless the overlay is enabled; then pick the graph or the table view.
    // (Unlike the CPU sibling, this does NOT null-check the render pointer.)
    void DebugComponentPerfMonGpu::RenderHUD( Debug2DImmediateRender* lpDebug2DRender )
    {
        if ( !mbVisible )
            return;

        if ( mbDisplayAsGraph )
            RenderPerformanceGraph( lpDebug2DRender );
        else
            RenderPerformanceTable( lpDebug2DRender );
    }

    // X360 0x8282CDF0. Draw the GPU performance table: the "GPU Render Cost" title one and a half
    // text-rows above the first entry, then walk every registered monitor (plus the totals) via
    // PerfMonGpu::ReportMonitors, each row rendered by RenderTableCallback. The layout metrics (text
    // size = row height, and the top-left screen border as the first draw origin) come from the debug
    // UI's screen metrics (X360: mpInstance->mpUI(+0x140)->Metrics @ +0x5C/+0x70/+0x74).
    void DebugComponentPerfMonGpu::RenderPerformanceTable( Debug2DImmediateRender* lpDebug2DRender )
    {
        const DebugUI::Metrics& lrMetrics = DebugManager::GetInstance()->GetUI().GetMetrics();

        const f32 lfRowHeight = lrMetrics.mfTextSize;          // metrics @ +0x5C

        RenderCallbackParameter lParameter;
        lParameter.mpThis   = this;
        lParameter.mpRender = lpDebug2DRender;
        lParameter.mfDrawX  = lrMetrics.mfScreenBorderLeft;    // metrics @ +0x70
        lParameter.mfDrawY  = lrMetrics.mfScreenBorderTop;     // metrics @ +0x74

        // The title sits 1.5 text rows above the first monitor row (fnmsubs, flt_82004D04).
        const Vector2 lv2TitlePos = { lrMetrics.mfScreenBorderLeft,
                                     std::fma( -lfRowHeight, KF_TABLETITLEHEIGHT,
                                               lrMetrics.mfScreenBorderTop ),
                                     0.0f, 0.0f };
        lpDebug2DRender->DrawText( "GPU Render Cost", lv2TitlePos, lfRowHeight, 0xFFFFFFFFu, false );

        PerfMonGpu::ReportMonitors( &RenderTableCallback, &lParameter, /*lbReportTotal*/ true );
    }

    // X360 CgsDev::DebugComponentPerfMonGpu::RenderPerformanceGraph @ 0x8282CEA0.
    // Draw the original frame, grid bars, 0/50/100/150/200% labels and mfMaxGpu reference bar, then
    // each reported monitor segment. Raw ARTIST loads and the original data strings pin the values.
    // ReportMonitors updates the callback parameter's total. ARTIST reloads
    // that same stack field after the callbacks and raises the stored maximum,
    // capped at200, for the next frame's translucent reference bar.
    void DebugComponentPerfMonGpu::RenderPerformanceGraph( Debug2DImmediateRender* lpDebug2DRender )
    {
        // The user-data threaded through the per-monitor callback.
        RenderCallbackParameter lParam;
        lParam.mpThis     = this;
        lParam.mpRender   = lpDebug2DRender;
        lParam.mfDrawX    = KF_GRAPHORIGINX;    // flt_82013FA8
        lParam.mfDrawY    = KF_GRAPHORIGINY;    // flt_820DDC58
        lParam.mfTotalGpu = KF_GRAPHSCALEMIN;   // flt_82001CC0, initial callback total

        // sub_8281C960 is DrawFrame(x0,y0,x1,y1,colour,border); r8 carries packed color 0x80000000.
        lpDebug2DRender->DrawFrame( KF_GRAPHORIGINX, KF_GRAPHORIGINY,
                                    KF_GRAPHFRAMERIGHT, KF_GRAPHFRAMEBOTTOM,
                                    KC_GRAPHFRAME, KF_GRAPHFRAMEBORDER );

        // Five 10-pixel-wide grid bars. Their y span is (originY+4) through (originY+7).
        for ( u32 luLine = 0; luLine < 5u; ++luLine )
        {
            const f32 lfGridX = std::fma( static_cast<f32>( luLine ), KF_GRAPHCOLUMNPITCH,
                                          KF_GRAPHORIGINX );
            const f32 lfGridY = KF_GRAPHORIGINY + KF_GRAPHBARHEIGHT;
            const Vector2 lv2Min = { lfGridX - KF_GRAPHGRIDHALF, lfGridY, 0.0f, 0.0f };
            const Vector2 lv2Size = { (lfGridX + KF_GRAPHGRIDHALF) - lv2Min.x,
                                      (KF_GRAPHORIGINY + KF_GRAPHGRIDEND) - lv2Min.y,
                                      0.0f, 0.0f };
            lpDebug2DRender->DrawBox( lv2Min, lv2Size, KC_GRAPHGRID );
        }

        // The first two raw strings are "0%" (unk_820DDC54) and "50%" (unk_820DDC50).
        static const char* const KAPCLABELS[5] = { "0%", "50%", "100%", "150%", "200%" };
        const f32 KAF_LABELX[5] = {
            KF_GRAPHORIGINX,
            (KF_GRAPHORIGINX + KF_GRAPHCOLUMNPITCH) - KF_GRAPHLABEL50INSET,
            (KF_GRAPHORIGINX + KF_GRAPHLABEL100X) - KF_GRAPHLABELINSET,
            (KF_GRAPHORIGINX + KF_GRAPHLABEL150X) - KF_GRAPHLABELINSET,
            (KF_GRAPHORIGINX + KF_GRAPHLABEL200X) - KF_GRAPHLABELINSET
        };
        for ( u32 luLabel = 0; luLabel < 5u; ++luLabel )
        {
            const f32 lfLabelX = KAF_LABELX[luLabel];
            const Vector2 lv2Pos = { lfLabelX, KF_GRAPHORIGINY + KF_GRAPHLABELDROP, 0.0f, 0.0f };
            lpDebug2DRender->DrawText( KAPCLABELS[luLabel], lv2Pos, KF_GRAPHLABELSIZE, KC_GRAPHLABEL, false );
        }

        // The translucent reference bar showing the current vertical scale (mfMaxGpu).
        {
            const f32 lfBarRight = std::fma( mfMaxGpu, KF_GRAPHREFERENCEBARSCALE,
                                             KF_GRAPHORIGINX ); // fmadds, flt_820DD6F4
            const Vector2 lv2Min = { KF_GRAPHORIGINX, KF_GRAPHORIGINY,                     0.0f, 0.0f };
            const Vector2 lv2Size = { lfBarRight - lv2Min.x,
                                      (KF_GRAPHORIGINY + KF_GRAPHBARHEIGHT) - lv2Min.y, 0.0f, 0.0f };
            lpDebug2DRender->DrawBox( lv2Min, lv2Size, KC_GRAPHSCALEBAR );
        }

        // Draw every monitor's segment (report-total suppressed: last arg false).
        PerfMonGpu::ReportMonitors( &DebugComponentPerfMonGpu::RenderGraphCallback, &lParam, false );

        // ARTIST8282D1D4 reloads var60: &var70+0x10, the total written by
        // RenderGraphCallback. Compare after drawing, as the original does.
        if ( mfMaxGpu < lParam.mfTotalGpu )
        {
            mfMaxGpu = lParam.mfTotalGpu;
            if ( lParam.mfTotalGpu > KF_GRAPHSCALEMAX )
                mfMaxGpu = KF_GRAPHSCALEMAX;
        }
    }

    // X360 0x82826FC8. Per-monitor row of the GPU performance table: draw the name at the cursor and
    // the value as "%.03f" at x+270 (flt_82058984). The color is opaque monitor color or red over budget.
    // Advances the shared draw-Y cursor by one text row so the next monitor stacks below.
    void DebugComponentPerfMonGpu::RenderTableCallback( const PerfMonGpuMonitorData& lParameters,
                                                        void* lpUserData )
    {
        RenderCallbackParameter* lpParameter = static_cast<RenderCallbackParameter*>( lpUserData );

        const f32 lfRowHeight = DebugManager::GetInstance()->GetUI().GetMetrics().mfTextSize;   // metrics @ +0x5C

        const RGBA lColour = lParameters.mbOverBudget
                                 ? 0xFF0000FFu                                    // red - over budget
                                 : ( lParameters.mColour.m_rgba | 0xFF000000u );  // monitor colour, opaque

        // The monitor name.
        const Vector2 lv2NamePos = { lpParameter->mfDrawX, lpParameter->mfDrawY, 0.0f, 0.0f };
        lpParameter->mpRender->DrawText( lParameters.mpcName, lv2NamePos, lfRowHeight, lColour, false );

        // The cost value.
        char lacValue[32];
        CgsCore::SPrintf( lacValue, 32, "%.03f", lParameters.mfCurrentValue );

        const Vector2 lv2ValuePos = { lpParameter->mfDrawX + KF_TABLEVALUEOFFSET,
                                      lpParameter->mfDrawY, 0.0f, 0.0f };
        lpParameter->mpRender->DrawText( lacValue, lv2ValuePos, lfRowHeight, lColour, false );

        // Advance to the next row.
        lpParameter->mfDrawY += lfRowHeight;
    }

    // X360 CgsDev::DebugComponentPerfMonGpu::RenderGraphCallback @ 0x8281F250.
    // Per-monitor callback for the graph view: draw this monitor's percentage-of-frame contribution
    // as a horizontal segment, advance the draw-X cursor, and update the threaded total. ARTIST
    // writes that total to RenderCallbackParameter; the graph reads it afterward
    // to raise and cap the reference-bar maximum.
    // Static: ReportMonitors receives this as a plain FPerfMonGpuReportCallback*.
    void DebugComponentPerfMonGpu::RenderGraphCallback( const PerfMonGpuMonitorData& lrData, void* lpUserData )
    {
        RenderCallbackParameter* lpParam = static_cast<RenderCallbackParameter*>( lpUserData );

        // Percentage-of-frame -> pixels along the bar (flt_820DC614, word 0x403851EC).
        const f32 lfBarLength = lrData.mfCurrentValue * KF_GRAPHSEGMENTSCALE;
        // Force the segment opaque (mColour | 0xFF000000).
        const RGBA lColour = static_cast<RGBA>( lrData.mColour.m_rgba ) | 0xFF000000u;

        const f32 lfX = lpParam->mfDrawX;
        const f32 lfY = lpParam->mfDrawY;

        // ARTIST 8281F2C4/CC subtracts the origin to form the vector size.
        const Vector2 lv2Min = { lfX,               lfY,                         0.0f, 0.0f };
        const Vector2 lv2Size = { (lfX + lfBarLength) - lfX,
                                  (lfY + KF_GRAPHBARHEIGHT) - lfY, 0.0f, 0.0f };
        lpParam->mpRender->DrawBox( lv2Min, lv2Size, lColour );

        lpParam->mfDrawX    += lfBarLength;
        lpParam->mfTotalGpu += lrData.mfCurrentValue;
    }
}
