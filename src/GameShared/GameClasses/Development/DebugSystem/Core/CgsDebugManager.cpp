#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"

#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"  // DebugComponent (mbActive/OnRegister/DebugUISectionCallback/RenderHUD)
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"      // GetUI().GetVariableManager()/GetFunctionManager()
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"        // Variant
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"  // mp2dRender Begin/End/SetRenderBuffer/Construct/SetDebugFont
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"  // mp3dRender SetDebugFont (font handoff)
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"             // mBufferedRenderer (buffered debug prims)
#include "GameShared/GameClasses/Development/DebugSystem/Core/Internal/CgsDebugInternal.h"     // Internal::SetDebugSingletons
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"        // DebugInterface::Get2dRender (bodied here -- see its note at the bottom)
#include "GameShared/GameClasses/Development/PerfMon/DebugComponent/CgsDebugComponentPerfMonCpu.h"  // the CPU perfmon overlay component
#include "GameShared/GameClasses/Core/CgsAssert.h"                                  // CGS_ASSERT
#include "GameShared/GameClasses/Development/AssertSystem/CgsAssertManager.h"       // Assert::gAssertManager / AssertData (on-screen assert overlay)
#include "GameShared/GameClasses/Development/MapFile/Reader/CgsMapFileReader.h"     // MapFile::Reader::GetStackEntryName (call-stack names)
#include "rw/core/debug/DebugCriticalSection.h"                                     // gDebugManagerSection (ThreadSafeAquire/Release lock)

#include <cstdio>   // snprintf (debug text formatting; the X360 used CgsCore::SPrintf)

// High-resolution frame timer (defined in CgsTimeUtils.cpp) - forward-declared so the frame-health
// heartbeat can measure the present-to-present delta without pulling Windows.h into this TU.
namespace CgsSystem { u32 GetSystemTimerBaseTime(); u32 GetSystemTimerFrequency(); u32 GetAvailablePhysicalMemoryBytes(); }

// CgsDev::DebugManager - the in-game debug systems owner.
//
// Boot/loading path: the per-frame tick is a no-op so the game-module update spine links and runs;
// the full Update (driving the UI) is the manager-construction follow-on.
//
// Registration/lifecycle (what DebugComponent::Register drives): ThreadSafeAquire/Release bracket
// the singleton behind the debug critical section (X360 0x821F1E50 / ThreadSafeRelease);
// RegisterComponent[Simple] thread the component onto the list and surface it in the menu - Simple
// as an mbActive bool toggle (X360 0x8282E1E0), the full form as a section callback row
// (X360 0x82832008). Both clear mbActive and fire the component's OnRegister.
//
// The debug critical section is the same single-threaded-boot no-op as the assert mutex; the real
// lock is deferred to the threading wiring. mpUI is wired by DebugManager::Construct (the
// manager-construction follow-on); GetUI hands it back.

namespace CgsDev
{
    // The debug system's process-wide instances (X360 CgsDebugManager.cpp:85/90): the UI and the
    // renderers are single global objects the manager wires itself to - not heap-allocated. (The 3D
    // renderer g3dInternalDebugRender + the perfmon globals are the render/perfmon follow-on.)
    DebugUI::DebugUI       gInternalDebugUI;
    Debug2DImmediateRender g2dInternalDebugRender;

    // The CPU perfmon overlay component (the on-screen bars). X360 keeps it as a DebugManager member
    // (mDebugComponentPerfMonCpu); modelled as a file-static here so the manager header stays light.
    // DebugManager::Construct constructs + registers + activates it.
    static DebugComponentPerfMonCpu gDebugComponentPerfMonCpu;

    // The buffered debug renderer (X360 DebugManager::mBufferedRenderer). Per-frame debug prims are
    // QUEUED here and flushed by RenderHUD's Dispatch2D. File-static (X360 has it as a by-value member;
    // kept out of the widely-included manager header to avoid pulling the event-queue in everywhere).
    static DebugRender gBufferedRenderer;

    // The per-DebugManager lock the thread-safe accessors bracket (X360 dword_83019264 - a file-static
    // rw DebugCriticalSection passed by &address to Enter/Leave). ThreadSafeAquire enters it, Release
    // leaves it. It stays un-Created (so Enter/Leave are no-ops, matching the X360 "if (*result)" guard)
    // on the single-threaded boot; DebugManager::Construct Creates it when the threading core wires up.
    static rw::core::debug::detail::DebugCriticalSection gDebugManagerSection = { 0 };

    namespace
    {
        u32  gu32LastFrameTick = 0;
        bool gbFrameTickValid  = false;
        f32  gfSmoothedFps     = 0.0f;

        // X360 CgsDev::_InterpolateColour: per-channel lerp between two packed RGBA colours (t in 0..1;
        // RGBA8 packed as 0xAABBGGRR, so each byte is one channel).
        u32 InterpolateColour(u32 luColour0, u32 luColour1, f32 lfT)
        {
            if (lfT < 0.0f) lfT = 0.0f;
            if (lfT > 1.0f) lfT = 1.0f;
            u32 luResult = 0;
            for (s32 liByte = 0; liByte < 4; ++liByte)
            {
                const s32 liShift = liByte * 8;
                const f32 lfC0 = static_cast<f32>((luColour0 >> liShift) & 0xFFu);
                const f32 lfC1 = static_cast<f32>((luColour1 >> liShift) & 0xFFu);
                const u32 luC  = static_cast<u32>(lfC0 + (lfC1 - lfC0) * lfT + 0.5f) & 0xFFu;
                luResult |= (luC << liShift);
            }
            return luResult;
        }

        // Update the smoothed frame rate for the FPS readout (the text itself is queued by
        // RenderFrameRateColouredWithAverage). NOTE: the on-screen squares are NOT a debug-system
        // element - they are BrnRendererModule::RenderThreeThreadMonitors (a separate per-thread monitor
        // drawn by the renderer module), so they are no longer drawn here. Frame time = the real
        // present-to-present delta from the high-res timer.
        void UpdateFrameStats()
        {
            const u32 lu32Now  = CgsSystem::GetSystemTimerBaseTime();
            const u32 lu32Freq = CgsSystem::GetSystemTimerFrequency();
            f32 lfFrameMs = 0.0f;
            if (gbFrameTickValid && lu32Freq != 0u)
                lfFrameMs = static_cast<f32>(static_cast<double>(lu32Now - gu32LastFrameTick) * 1000.0 / static_cast<double>(lu32Freq));
            gu32LastFrameTick = lu32Now;
            gbFrameTickValid  = true;

            const f32 lfCurFps = (lfFrameMs > 0.0f) ? (1000.0f / lfFrameMs) : 0.0f;
            gfSmoothedFps = (gfSmoothedFps <= 0.0f) ? lfCurFps : (gfSmoothedFps * 0.9f + lfCurFps * 0.1f);
        }
    }

    // X360 CgsDebugManager.cpp:113 DebugManagerConstructParameters::DEFAULT - the built-in debug
    // configuration the engine boots with. The pool sizes are reconstructed generously (the exact
    // X360 data-section values are TBD; they affect only capacity, not behaviour - the bounded
    // loading build registers only the perfmon). mpRwAllocator is null: the debug pools' backing
    // currently comes from the global heap (CgsDebugCollections.cpp), which ignores the allocator;
    // the real GetDefaultAllocator wiring is the allocator follow-on.
    const DebugManagerConstructParameters DebugManagerConstructParameters::DEFAULT =
    {
        /* miPerfMonCpuCount          */ 256,   // raised 64 -> 256 (world-module mount 2026-07-26):
                                                // the wired WorldModule::Construct + the real
                                                // SceneManagerModule::Construct register ~60 more
                                                // monitors on top of the game module's ~40, and 64
                                                // overflowed (AddMonitor -1 -> the WorldModule
                                                // handle asserts fired). Capacity-only, per the
                                                // note above.
        /* miPerfMonLogBufferSize     */ 8192,
        /* miMenuWindowPoolSize       */ 16,
        /* miMenuPoolSize             */ 64,
        /* miFunctionPoolSize         */ 128,
        /* miVariablePoolSize         */ 256,
        /* miVariableMetadataPoolSize */ 256,
        /* miConsoleLineCount         */ 64,
        /* mpRwAllocator              */ nullptr,
    };

    DebugManager* DebugManager::mpInstance = nullptr;

    DebugManager::DebugManager()
        : mpUI(nullptr)
        , mp2dRender(nullptr)
        , mp3dRender(nullptr)
    {
        mComponentList.Clear();
    }

    DebugManager::~DebugManager() {}

    // X360 Construct 0x828332C0 (bounded). Claim the singleton, wire the debug-internal accessors,
    // reset the component list + renderer pointers, then construct the UI (-> the three managers ->
    // their pools). The full X360 bring-up also creates the rw debug Manager instance + default
    // allocator, the debug critical section, the two VariableEventQueues, and the perfmon log buffer;
    // those are the bring-up follow-on (none is needed to construct + render the perfmon HUD - the
    // pools take their backing from the global heap, so the allocator is threaded through but unused).
    void DebugManager::Construct(const DebugManagerConstructParameters* lpParameters)
    {
        mpInstance = this;
        mp2dRender = nullptr;
        mp3dRender = nullptr;
        mComponentList.Clear();
        gBufferedRenderer.Construct();   // the buffered debug-prim queue (X360: 2x VariableEventQueue)

        mpUI = &gInternalDebugUI;

        // Wire the singletons every DebugInternal-derived class reaches through GetUI()/
        // GetDebugManager()/GetAllocator(); must precede the manager Constructs + any registration.
        Internal::SetDebugSingletons(this, mpUI, lpParameters->mpRwAllocator);

        mpUI->Construct(lpParameters);

        // Build the CPU perfmon REGISTRY itself (X360 PerfMonCpu::Construct(maxCount, allocator) --
        // this is what miPerfMonCpuCount exists for). Missing until the world-module mount
        // (2026-07-26): with the registry unbuilt every AddMonitor returned -1, which the game
        // module's monitor block silently tolerated but the wired WorldModule::Construct ASSERTS on
        // (its X360 body checks each returned handle >= 0).
        CgsDev::PerfMonCpu::Construct(lpParameters->miPerfMonCpuCount, lpParameters->mpRwAllocator);

        // Bring up + register the full CPU perfmon overlay component (the detailed per-monitor bar
        // table). Construct inits its state; Register threads it onto mComponentList + the debug menu
        // (exercising the function/menu pools). It is left INACTIVE - the detailed table is a
        // menu-toggled view; the always-on indicator is the 3-square frame-health heartbeat drawn in
        // RenderHUD. (Activate it via ActivateComponent to show the full bar table.)
        gDebugComponentPerfMonCpu.Construct(static_cast<s16>(lpParameters->miPerfMonCpuCount), nullptr, 0);
        gDebugComponentPerfMonCpu.Register();
    }

    // X360 ConstructRenderer (CgsDebugManager.cpp:248, bounded). Construct the 2D debug renderer at
    // the screen's virtual resolution and hand it to the UI. The X360 reads that size from the UI
    // Metrics (a deferred heavy member), so the bounded path uses the loading screen's render
    // resolution directly - the Im2d space the squares are drawn in (pixel coords, 1280x720). The 3D
    // renderer + the assert-overlay renderer hookup (Assert::Manager::SetRenderer) are the render
    // follow-on. The renderer's backing/allocator is unused (the box path batches into a fixed array).
    void DebugManager::ConstructRenderer()
    {
        const f32 lfVirtualScreenWidth  = 1280.0f;
        const f32 lfVirtualScreenHeight = 720.0f;

        mp2dRender = &g2dInternalDebugRender;
        mp2dRender->Construct(nullptr, lfVirtualScreenWidth, lfVirtualScreenHeight);

        mpUI->Set2DRenderer(mp2dRender);

        // Hand the same 2D renderer to the assert manager (X360 ConstructRenderer also calls
        // Assert::Manager::SetRenderer) so a failed assert can draw its on-screen overlay.
        Assert::gAssertManager.SetRenderer(mp2dRender);
    }

    void DebugManager::Update(f32) {}

    // Faithful port of X360 0x821F1E50: assert the singleton exists, enter the per-manager debug
    // critical section, then hand the singleton back. (Enter is a no-op until the section is Created.)
    DebugManager* DebugManager::ThreadSafeAquire()
    {
        CGS_ASSERT(mpInstance, "mpInstance");
        gDebugManagerSection.Enter();
        return mpInstance;
    }

    // Faithful port of X360 0x821F1EB8: leave the per-manager debug critical section FIRST (the X360
    // calls Leave before the assert), then assert the released pointer is the live singleton.
    void DebugManager::ThreadSafeRelease(DebugManager* lpDebugManager)
    {
        gDebugManagerSection.Leave();
        CGS_ASSERT(lpDebugManager == mpInstance, "lpDebugManager == mpInstance");
    }

    DebugUI::DebugUI& DebugManager::GetUI()
    {
        return *mpUI;
    }

    // Faithful port of X360 0x823B14E8: hand the loaded font to both debug immediate renderers so
    // their DrawText switches off the vector-font fallback onto the resource-font path. The game's
    // GamePrepare drives this once "Default.font" resolves + Font::CreateTextureState has built the
    // font's texture state. (X360 asserts lrFont != CgsResource::NULLResourceHandle.)
    void DebugManager::SetDebugFont(const CgsResource::SafeResourceHandle<CgsResource::Font>& lrFont)
    {
        // The X360 has both renderers; on the bounded boot only the 2D renderer is constructed
        // (mp3dRender is the Debug3D follow-on), so guard each before handing the font over.
        if (mp3dRender != nullptr)
            mp3dRender->SetDebugFont(lrFont);
        if (mp2dRender != nullptr)
            mp2dRender->SetDebugFont(lrFont);
    }

    bool DebugManager::IsComponentRegistered(DebugComponent* lpComponent)
    {
        return mComponentList.IsAdded(lpComponent);
    }

    // X360 0x82832008. Register a component as a full menu section: a function row that, when
    // selected, fires DebugComponent::DebugUISectionCallback for it.
    void DebugManager::RegisterComponent(DebugComponent* lpComponent, const char* lpcPath, const char* lpcName)
    {
        CGS_ASSERT(!IsComponentRegistered(lpComponent), "!IsComponentRegistered(lpDebugComponent)");

        mComponentList.Add(lpComponent);
        lpComponent->mbActive = false;

        if (lpcName)
            GetUI().GetFunctionManager().RegisterFunction(&DebugComponent::DebugUISectionCallback, lpComponent, lpcPath, lpcName);

        lpComponent->OnRegister();
    }

    // X360 0x8282E1E0. Register a "simple" component: a single bool row wired to its mbActive flag.
    void DebugManager::RegisterComponentSimple(DebugComponent* lpComponent, const char* lpcPath, const char* lpcName)
    {
        CGS_ASSERT(!IsComponentRegistered(lpComponent), "!IsComponentRegistered(lpDebugComponent)");
        CGS_ASSERT(lpcName, "lpcName");

        // FLAG: ARTIST 0x8282E29C-0x8282E2A0 defaults a null path to "Debug/Components"
        // (off_82F31990), NOT "" -- a simple component registers under the Debug/Components menu
        // section when no path is given.
        if (!lpcPath)
            lpcPath = "Debug/Components";

        mComponentList.Add(lpComponent);
        lpComponent->mbActive = false;

        GetUI().GetVariableManager().RegisterVariable(DebugUI::Variant(&lpComponent->mbActive), lpcPath, lpcName);

        lpComponent->OnRegister();
    }

    // X360 ActivateComponent(DebugComponent*): make a registered component active so the render spine
    // draws it. Bounded to flipping the active flag (which puts the component in RenderHUD's draw loop)
    // + the component's OnActivate hook; the X360 also opens the component's variable page in the menu.
    void DebugManager::ActivateComponent(DebugComponent* lpComponent)
    {
        CGS_ASSERT(lpComponent, "lpComponent");
        if (!lpComponent)
            return;
        // FLAG: ARTIST 0x8283213C-0x82832160 guards the whole activation on "if (mbActive == 0)" --
        // re-activating an already-active component is a no-op (it must NOT fire OnActivate again).
        if (lpComponent->IsActive())
            return;
        lpComponent->mbActive = true;
        lpComponent->OnActivate();
    }

    // X360 RenderHUD 0x8282E108 (2D screen-space pass - the debug squares): open the 2D renderer,
    // let each active component draw its HUD, close. The X360 also flushes the buffered debug prims
    // (DebugRender::Dispatch2D over mBufferedRenderer) and renders the DebugUI menus between Begin and
    // the component loop; those two paths are the buffered-render / menu-render follow-on.
    void DebugManager::RenderHUD()
    {
        mp2dRender->Begin();

        // X360 RenderHUD step: flush the buffered debug prims first (the frame-stats overlay queued in
        // Render via QueueFrameStats), then the active components' HUDs.
        gBufferedRenderer.Dispatch2D(mp2dRender, true);

        for (DebugComponent* lpComponent = mComponentList.GetFirst();
             lpComponent;
             lpComponent = mComponentList.GetNext(lpComponent))
        {
            if (lpComponent->IsActive())
                lpComponent->RenderHUD(mp2dRender);
        }

        mp2dRender->End();
    }

    // X360 0x8282D998. Queue the frame-rate readout ("%d fps") coloured by framerate (low->mid->high
    // about the midpoint, via InterpolateColour) into the buffered renderer. The X360 reads the position
    // off the screen-metrics member (DebugManager+320: width@+104, height@+108, lineHeight@+124); that
    // member is the deferred Metrics follow-on, so the known render resolution is used with the X360
    // formula - x = width*0.33, y = height-(lineHeight+10), text height 20 - the real bottom-bar spot.
    // The averaged "%d fps %s" second line is the perfmon-average follow-on.
    void DebugManager::RenderFrameRateColouredWithAverage(f32 lfFramerate, f32 /*lfAverageFramerate*/,
            u32 lHighColour, u32 lLowColour, u32 lMidColour,
            f32 lfHighFramerate, f32 lfLowFramerate,
            const char* /*lpcAverageText*/, f32 /*lfAverageHighlight*/, bool lbIsRealtime)
    {
        CGS_ASSERT(lfLowFramerate < lfHighFramerate, "lfLowFramerate < lfHighFramerate");

        const f32 lfRange    = (lfHighFramerate - lfLowFramerate) * 0.5f;
        const f32 lfMidpoint = lfLowFramerate + lfRange;
        u32 luColour;
        if (lfRange <= 0.0f)
            luColour = lMidColour;
        else if (lfFramerate >= lfMidpoint)
            luColour = InterpolateColour(lMidColour, lHighColour, (lfFramerate - lfMidpoint) / lfRange);
        else
            luColour = InterpolateColour(lLowColour, lMidColour, (lfFramerate - lfLowFramerate) / lfRange);

        const DebugUI::Metrics& lrMetrics = mpUI->GetMetrics();
        const f32 lfX = lrMetrics.mfScreenWidth * 0.33f;
        const f32 lfY = lrMetrics.mfScreenHeight - (lrMetrics.mfScreenBorderBottom + 10.0f);

        char lacText[48];
        const s32 liFps = static_cast<s32>(lfFramerate + 0.5f);
        if (lbIsRealtime)
            std::snprintf(lacText, sizeof(lacText), "%d fps", liFps);
        else
            std::snprintf(lacText, sizeof(lacText), "%d fps & simulation not real time", liFps);

        gBufferedRenderer.Draw2DText(lacText, lfX, lfY, 20.0f, luColour);
    }

    // X360 0x8282D8F8. Queue the build-info string (bottom-left debug readout). The X360 string is a
    // member (field_8174) filled by CalculateBuildDate; reconstructed here as the PC compile date/time
    // (the real CalculateBuildDate is the build-stamp follow-on). Scale 12; x = metrics+112 (a left
    // value - estimated, Metrics member deferred), y = height - lineHeight.
    void DebugManager::RenderBuildInfo()
    {
        static const char* const KPC_BUILD = "Build Date: " __TIME__ " " __DATE__ ;

        const DebugUI::Metrics& lrMetrics = mpUI->GetMetrics();
        const f32 lfX = lrMetrics.mfScreenBorderLeft;
        const f32 lfY = lrMetrics.mfScreenHeight - lrMetrics.mfScreenBorderBottom;

        gBufferedRenderer.Draw2DText(KPC_BUILD, lfX, lfY, 12.0f, 0xFFFFFFFFu);
    }

    // X360 0x8282DD28. Queue the available-memory readout "%uMB %uKB %uB" (bottom centre-right). Memory
    // from GlobalMemoryStatus (CgsSystem::GetAvailablePhysicalMemoryBytes); the MB/KB/B split mirrors the
    // X360. Scale 16; x = width*0.66, y = height - lineHeight (known render res used with the X360 formula).
    void DebugManager::RenderMemory()
    {
        const u32 luAvail = CgsSystem::GetAvailablePhysicalMemoryBytes();
        const u32 luMB = luAvail >> 20;
        const u32 luKB = (luAvail - (luMB << 20)) >> 10;
        const u32 luB  = luAvail - ((luMB << 20) + (luKB << 10));

        const DebugUI::Metrics& lrMetrics = mpUI->GetMetrics();
        const f32 lfX = lrMetrics.mfScreenWidth * 0.66f;
        const f32 lfY = lrMetrics.mfScreenHeight - lrMetrics.mfScreenBorderBottom;

        char lacText[64];
        std::snprintf(lacText, sizeof(lacText), "%uMB %uKB %uB", luMB, luKB, luB);
        gBufferedRenderer.Draw2DText(lacText, lfX, lfY, 16.0f, 0xFFFFFFFFu);
    }

    // X360 0x8282DE28 DebugManager::RenderAssert - the on-screen assert OVERLAY (what the real ARTIST
    // build shows): "line:file", then the failed expression, then one call-stack line per frame - the
    // map-resolved function name (mpMapReader->GetStackEntryName), falling back to "    0x%08X". Queued
    // into the buffered renderer at x=50, text size 16, 18px line advance, in the carved assert colour
    // (dword_82F32268). The X360 reaches the call-stack + map reader through the AssertData record.
    void DebugManager::RenderAssert(const Assert::AssertData* lpData)
    {
        if (!lpData)
            return;

        const u32 luColour = 0xFF32FFFFu;   // carved (dword_82F32268)
        char lacBuffer[1024];

        std::snprintf(lacBuffer, sizeof(lacBuffer), "%d:%s",
                      lpData->miLine, lpData->mpcFile ? lpData->mpcFile : "?");
        gBufferedRenderer.Draw2DText(lacBuffer, 50.0f, 50.0f, 16.0f, luColour);

        gBufferedRenderer.Draw2DText(lpData->macAssertMessage, 50.0f, 68.0f, 16.0f, luColour);

        f32 lfY = 93.0f;
        const s32 liCount = lpData->mStack.GetNumStackAddresses();
        for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
        {
            const char* lpcName = lpData->mpMapReader ? lpData->mpMapReader->GetStackEntryName(liIndex) : nullptr;
            char lacAddr[24];
            if (!lpcName)
            {
                std::snprintf(lacAddr, sizeof(lacAddr), "    0x%08X",
                              static_cast<u32>(lpData->mStack.GetStackAddress(liIndex)));
                lpcName = lacAddr;
            }
            gBufferedRenderer.Draw2DText(lpcName, 50.0f, lfY, 16.0f, luColour);
            lfY += 18.0f;
        }
    }

    // Flush one frame of the assert overlay through the 2D renderer. On the X360 the display-owning
    // thread paints RenderAssert (driven through BrnRendererModule::RenderAssert) while the asserting
    // thread parks in DoAssert; on the single-threaded boot the one (frozen) thread paints it here, from
    // the assert freeze loop. Uses the render buffer the last Render set on mp2dRender.
    void DebugManager::RenderAssertOverlay()
    {
        if (!Assert::gAssertManager.HasAssert() || mp2dRender == nullptr || !mp2dRender->HasRenderBuffer())
            return;

        mp2dRender->Begin();
        RenderAssert(&Assert::gAssertManager.GetAssertData());   // queue line:file + message + call-stack
        gBufferedRenderer.Dispatch2D(mp2dRender, true);          // flush them
        mp2dRender->End();
    }

    // X360 Render 0x8282F770: point the renderers at this frame's buffers, then RenderWorld + RenderHUD.
    // The 3D path (mp3dRender setup + RenderWorld) is the Debug3D follow-on; the 2D path drives the
    // squares into lp2dRenderBuffer.
    void DebugManager::Render(const Matrix44& /*lViewProjection*/, const Vector3& /*lCameraPosition*/,
                             CgsGraphics::Im2d* /*lp3dRenderBuffer*/, CgsGraphics::Im2d* lp2dRenderBuffer)
    {
        mp2dRender->SetRenderBuffer(lp2dRenderBuffer);
        // Orchestrator step (X360 BrnGameModule::DebugManagerRender): update the frame health, then
        // queue the per-frame debug overlay; RenderHUD flushes the buffered renderer (Dispatch2D).
        UpdateFrameStats();
        // The bottom-bar debug readouts (X360 BrnGameModule::DebugManagerRender order): build info, the
        // frame-rate (green high / yellow mid / red low), and available memory. All queue into the
        // buffered renderer; RenderHUD flushes them.
        RenderBuildInfo();
        RenderFrameRateColouredWithAverage(gfSmoothedFps, gfSmoothedFps,
                                           0xFF00FF00u, 0xFF0000FFu, 0xFF00FFFFu,
                                           60.0f, 30.0f, "over last minute", 0.0f, true);
        RenderMemory();
        RenderHUD();
    }

    // ------------------------------------------------------------------------
    // DebugInterface::Get2dRender -- X360 @0x82822750.
    //
    // [marked deviation -- HOME, not behaviour] This is a DebugInterface method and its own assert
    // names GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.cpp:190 as
    // its console home. It is bodied HERE instead, for one reason: the console reaches the buffered
    // renderer as `mpDebugManager + 0x14C` -- a DebugManager MEMBER (mBufferedRenderer) -- and this
    // tree deliberately models that member as the file-static gBufferedRenderer above, "kept out of
    // the widely-included manager header to avoid pulling the event-queue in everywhere" (its own
    // comment). gBufferedRenderer has internal linkage, so only this TU can name it. The
    // alternatives were both worse: growing CgsDebugManager.h by a ~16 KB by-value DebugRender
    // member (and the CgsVariableEventQueue include behind it) into every consumer of that header,
    // or adding a second accessor to the manager purely as a trampoline.
    // MOVE-WHEN: mBufferedRenderer becomes a real named member of DebugManager -- then this body
    // goes back to CgsDebugInterface.cpp and reads it through the manager reference.
    //
    // Behaviour is the console's, unchanged: assert the manager pointer, then return its buffered
    // renderer by reference. (X360: `lwz r11,0(r30); cmplwi; <assert "mpDebugManager">;
    // lwz r11,0(r30); addi r3,r11,0x14C`.)
    //
    // This is what BrnDirector::DebugPrinter::ActualPrint @0x821F71D8 and Camera::Utils::Tweaker's
    // on-screen readout both draw through.
    // ------------------------------------------------------------------------
    DebugRender& DebugInterface::Get2dRender()
    {
        CGS_ASSERT(mpDebugManager, "mpDebugManager");
        return gBufferedRenderer;
    }
}
