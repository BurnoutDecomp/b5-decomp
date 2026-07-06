#include "GameShared/GameClasses/Development/PerfMon/DebugComponent/CgsDebugComponentPerfMonGpu.h"

#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h" // DrawBox/DrawFrame/DrawText
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"             // DebugManager::GetInstance()
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"               // GetUI().GetMetrics()
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"                 // DebugUI::Metrics
#include "GameShared/GameClasses/Development/PerfMon/Gpu/CgsPerfMonGpu.h"                    // PerfMonGpu / PerfMonGpuMonitorData
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                                      // CgsCore::SPrintf

// CgsDev::DebugComponentPerfMonGpu - the GPU performance overlay bodies. RenderHUD picks the table or
// graph view; both walk the PerfMonGpu registry via ReportMonitors, drawing one row/bar per monitor
// through the static RenderTableCallback / RenderGraphCallback (the shared draw cursor + running total
// are threaded through RenderCallbackParameter). The layout metrics come from the debug UI's screen
// Metrics (row/text height + the first draw origin).

namespace CgsDev
{
    namespace
    {
        // Graph-view constants. The stacked-segment value scale (ms -> px, flt_820DC614) and the bar
        // height (4.0, flt_82004EF4) ARE attested; the frame-rect magnitudes, gridline pitch and the
        // (non-uniform) per-label X insets are NOT rodata-attested (IDA reported failed local-var
        // allocation for RenderPerformanceGraph) and are reconstructed by role.
        const f32 KF_GRAPHVALUESCALE = 2.8800001f;   // flt_820DC614 - value (ms) -> pixels
        const f32 KF_GRAPHBARHEIGHT  = 4.0f;         // flt_82004EF4 - segment / gridline height

        const f32 KF_GRAPHORIGINX    = 32.0f;        // flt_82013FA8 - initial draw cursor X
        const f32 KF_GRAPHORIGINY    = 420.0f;       // flt_820DDC58 - initial draw cursor Y
        const f32 KF_GRAPHSCALEMIN   = 0.0f;         // flt_82001CC0 - running-total seed + clamp floor
        const f32 KF_GRAPHSCALEMAX   = 200.0f;       // clamp ceiling (200%)

        const f32 KF_GRAPHFRAMELEFT   = 32.0f;       // plot frame rect (best-effort)
        const f32 KF_GRAPHFRAMETOP    = 416.0f;
        const f32 KF_GRAPHFRAMERIGHT  = 608.0f;
        const f32 KF_GRAPHFRAMEBOTTOM = 420.0f;
        const f32 KF_GRAPHFRAMEBORDER = 2.0f;        // sub_8281C960 border thickness

        const f32 KF_GRAPHCOLUMNPITCH = 144.0f;      // gridline / label column pitch (best-effort)
        const f32 KF_GRAPHGRIDHALF    = 1.0f;        // half-width of a vertical gridline (flt_8200426C)
        const f32 KF_GRAPHLABELINSET  = 8.0f;        // label X inset (best-effort)
        const f32 KF_GRAPHLABELDROP   = 8.0f;        // label Y drop (best-effort)
        const f32 KF_GRAPHLABELSIZE   = 15.0f;       // flt_820047C4 - label text scale

        const RGBA KC_GRAPHFRAME    = 0xFFFFFFFFu;   // plot frame colour
        const RGBA KC_GRAPHLABEL    = 0xFF6666FFu;   // scale-label colour (light red)
        const RGBA KC_GRAPHSCALEBAR = 0x80252525u;   // translucent reference bar
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

        // The title sits 1.5 rows above the first monitor row (fnmsubs: mfScreenBorderTop - mfTextSize*1.5).
        const Vector2 lv2TitlePos = { lrMetrics.mfScreenBorderLeft,
                                     lrMetrics.mfScreenBorderTop - lfRowHeight * 1.5f, 0.0f, 0.0f };
        lpDebug2DRender->DrawText( "GPU Render Cost", lv2TitlePos, lfRowHeight, 0xFFFFFFFFu, false );

        PerfMonGpu::ReportMonitors( &RenderTableCallback, &lParameter, /*lbReportTotal*/ true );
    }

    // X360 CgsDev::DebugComponentPerfMonGpu::RenderPerformanceGraph @ 0x8282CEA0.
    // The graph (history) view: a bordered plot frame, five vertical grid-lines, five scale labels
    // (empty / empty / 100% / 150% / 200%), a translucent 'current mfMaxGpu' reference bar, then one
    // stacked segment per monitor via ReportMonitors(RenderGraphCallback). Finally the vertical scale
    // (mfMaxGpu) is grown to fit and capped: if it falls below the floor it is raised to the floor,
    // and if the floor itself exceeds 200% it is capped there.
    //
    // CONFIDENCE: LOW. IDA reported failed local-variable allocation and no rodata dump is available,
    // so the exact frame-rect magnitudes, the gridline pitch, and the (non-uniform) per-label X insets
    // are reconstructed by role, not attested to the bit. The control flow, member offsets, callback
    // wiring, colours and the [floor,200] clamp ARE attested.
    void DebugComponentPerfMonGpu::RenderPerformanceGraph( Debug2DImmediateRender* lpDebug2DRender )
    {
        // The user-data threaded through the per-monitor callback.
        RenderCallbackParameter lParam;
        lParam.mpThis     = this;
        lParam.mpRender   = lpDebug2DRender;
        lParam.mfDrawX    = KF_GRAPHORIGINX;    // flt_82013FA8
        lParam.mfDrawY    = KF_GRAPHORIGINY;    // flt_820DDC58
        lParam.mfTotalGpu = KF_GRAPHSCALEMIN;   // flt_82001CC0 (also the vertical-scale clamp floor)

        // The plot border (sub_8281C960 = DrawFrame(x0,y0,x1,y1,colour,border=2.0)).
        lpDebug2DRender->DrawFrame( KF_GRAPHFRAMELEFT, KF_GRAPHFRAMETOP,
                                    KF_GRAPHFRAMERIGHT, KF_GRAPHFRAMEBOTTOM,
                                    KC_GRAPHFRAME, KF_GRAPHFRAMEBORDER );

        // Five vertical grid-lines, spaced by KF_GRAPHCOLUMNPITCH along X.
        for ( u32 luLine = 0; luLine < 5u; ++luLine )
        {
            const f32 lfGridX = KF_GRAPHORIGINX + static_cast<f32>( luLine ) * KF_GRAPHCOLUMNPITCH;
            const Vector2 lv2Min = { lfGridX - KF_GRAPHGRIDHALF, KF_GRAPHORIGINY,                     0.0f, 0.0f };
            const Vector2 lv2Max = { lfGridX + KF_GRAPHGRIDHALF, KF_GRAPHORIGINY + KF_GRAPHBARHEIGHT, 0.0f, 0.0f };
            lpDebug2DRender->DrawBox( lv2Min, lv2Max, 0xFFFFFFFFu );
        }

        // The five scale labels in light red (0xFF6666FF), scale 15.0. The first two are raw rodata
        // sentinels (unk_820DDC54 / unk_820DDC50) -> empty strings; the rest literal.
        static const char* const KAPCLABELS[5] = { "", "", "100%", "150%", "200%" };
        for ( u32 luLabel = 0; luLabel < 5u; ++luLabel )
        {
            const f32 lfLabelX = KF_GRAPHORIGINX + static_cast<f32>( luLabel ) * KF_GRAPHCOLUMNPITCH
                                 - KF_GRAPHLABELINSET;
            const Vector2 lv2Pos = { lfLabelX, KF_GRAPHORIGINY + KF_GRAPHLABELDROP, 0.0f, 0.0f };
            lpDebug2DRender->DrawText( KAPCLABELS[luLabel], lv2Pos, KF_GRAPHLABELSIZE, KC_GRAPHLABEL, false );
        }

        // The translucent reference bar showing the current vertical scale (mfMaxGpu).
        {
            const f32 lfBarRight = KF_GRAPHORIGINX + mfMaxGpu * KF_GRAPHVALUESCALE;   // fmadds vs flt_820DD6F4
            const Vector2 lv2Min = { KF_GRAPHORIGINX, KF_GRAPHORIGINY,                     0.0f, 0.0f };
            const Vector2 lv2Max = { lfBarRight,      KF_GRAPHORIGINY + KF_GRAPHBARHEIGHT, 0.0f, 0.0f };
            lpDebug2DRender->DrawBox( lv2Min, lv2Max, KC_GRAPHSCALEBAR );
        }

        // Draw every monitor's segment (report-total suppressed: last arg false).
        PerfMonGpu::ReportMonitors( &DebugComponentPerfMonGpu::RenderGraphCallback, &lParam, false );

        // Grow the vertical scale to fit, but never below the floor and never above 200%.
        if ( mfMaxGpu < KF_GRAPHSCALEMIN )
        {
            mfMaxGpu = KF_GRAPHSCALEMIN;
            if ( KF_GRAPHSCALEMIN > KF_GRAPHSCALEMAX )
                mfMaxGpu = KF_GRAPHSCALEMAX;
        }
    }

    // X360 0x82826FC8. Per-monitor row of the GPU performance table (the ReportMonitors callback):
    // the monitor's name on the left and its cost formatted as "%.03f" 270px to the right, both at the
    // running draw cursor. Colour is the monitor's own colour forced opaque, or red when over budget.
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

        const Vector2 lv2ValuePos = { lpParameter->mfDrawX + 270.0f, lpParameter->mfDrawY, 0.0f, 0.0f };
        lpParameter->mpRender->DrawText( lacValue, lv2ValuePos, lfRowHeight, lColour, false );

        // Advance to the next row.
        lpParameter->mfDrawY += lfRowHeight;
    }

    // X360 CgsDev::DebugComponentPerfMonGpu::RenderGraphCallback @ 0x8281F250.
    // Per-monitor callback for the graph view: draw this monitor's contribution as a horizontal
    // segment of the stacked bar, advance the running draw-X cursor, and accumulate the total GPU
    // cost. Static: threaded through PerfMonGpu::ReportMonitors as a plain FPerfMonGpuReportCallback*
    // (the owning component is reached via lpUserData->mpThis).
    void DebugComponentPerfMonGpu::RenderGraphCallback( const PerfMonGpuMonitorData& lrData, void* lpUserData )
    {
        RenderCallbackParameter* lpParam = static_cast<RenderCallbackParameter*>( lpUserData );

        // Value (ms) -> pixels along the bar (flt_820DC614).
        const f32 lfBarLength = lrData.mfCurrentValue * KF_GRAPHVALUESCALE;
        // Force the segment opaque (mColour | 0xFF000000).
        const RGBA lColour = static_cast<RGBA>( lrData.mColour.m_rgba ) | 0xFF000000u;

        const f32 lfX = lpParam->mfDrawX;
        const f32 lfY = lpParam->mfDrawY;

        // The asm builds the max corner itself (x+w, y+h) and calls the Vector2-min/Vector2-max
        // DrawBox (see CgsWidgetLineGraph.cpp).
        const Vector2 lv2Min = { lfX,               lfY,                         0.0f, 0.0f };
        const Vector2 lv2Max = { lfX + lfBarLength, lfY + KF_GRAPHBARHEIGHT,      0.0f, 0.0f };
        lpParam->mpRender->DrawBox( lv2Min, lv2Max, lColour );

        lpParam->mfDrawX    += lfBarLength;
        lpParam->mfTotalGpu += lrData.mfCurrentValue;
    }
}
