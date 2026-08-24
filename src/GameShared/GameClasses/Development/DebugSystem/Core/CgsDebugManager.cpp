#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"

#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"  // DebugComponent (mbActive/OnRegister/DebugUISectionCallback/RenderHUD)
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"      // GetUI().GetVariableManager()/GetFunctionManager()
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"        // Variant
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"  // mp2dRender Begin/End/SetRenderBuffer/Construct/SetDebugFont
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"  // mp3dRender SetDebugFont (font handoff)
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"             // mBufferedRenderer (buffered debug prims)
#include "GameShared/GameClasses/Core/CgsAssert.h"                                  // CGS_ASSERT + Assert::Construct (the bring-up's first call)
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                             // CgsCore::SPrintf (CalculateBuildDate)
#include "GameShared/GameClasses/Development/AssertSystem/CgsAssertManager.h"       // Assert::gAssertManager / AssertData (on-screen assert overlay)
#include "GameShared/GameClasses/Development/MapFile/Reader/CgsMapFileReader.h"     // MapFile::Reader::GetStackEntryName (call-stack names)
#include "rw/core/debug/DebugCriticalSection.h"                                     // gDebugManagerSection (ThreadSafeAquire/Release lock)
#include "rw/rwcore_structs.h"                                                       // rw::ResourceAllocatorRegistry / rw::core::debug::Manager / rw::BaseResourceDescriptors

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
// The debug critical section and the assert mutex are both Created by the bring-up now
// (Construct @0x828332C0 / Assert::Construct @0x82820758), exactly as on the console. mpUI is
// wired by DebugManager::Construct; GetUI hands it back.

namespace CgsDev
{
    // The debug system's process-wide instances (X360 CgsDebugManager.cpp:85/90): the UI and the
    // renderers are single global objects the manager wires itself to - not heap-allocated
    // (X360 gInternalDebugUI / unk_830199C0 / unk_83029060).
    DebugUI::DebugUI       gInternalDebugUI;
    Debug3DImmediateRender g3dInternalDebugRender;
    Debug2DImmediateRender g2dInternalDebugRender;

    // The per-DebugManager lock the thread-safe accessors bracket (X360 dword_83019264 - a file-static
    // rw DebugCriticalSection passed by &address to Enter/Leave). ThreadSafeAquire enters it, Release
    // leaves it. DebugManager::Construct @0x828332C0 Creates it (the console's own bring-up order).
    static rw::core::debug::detail::DebugCriticalSection gDebugManagerSection = { 0 };

    namespace
    {
        // The console's `rw::IResourceAllocator::AllocateMemoryResource` @0x823FF7D0 is an INLINE
        // that builds a five-entry serialised descriptor and tail-calls the allocator's DoAllocate
        // slot; the PC rwcore models DoAllocate with the narrower <4> alias, so the descriptor is
        // built as <5> and reinterpret_cast down at the call - the same idiom
        // CgsPhysicsSimulationModule.cpp and rwgpfxtint.cpp already use. Carves the perfmon
        // CPU-trace log buffer in DebugManager::Construct.
        void* AllocateMemoryResource(rw::IResourceAllocator* lpAllocator, u32 luSize, u32 luAlignment)
        {
            rw::BaseResourceDescriptors<5> lDescriptor;
            for (u32 luEntry = 0u; luEntry < 5u; ++luEntry)
            {
                lDescriptor.m_baseResourceDescriptors[luEntry].m_size      = 0u;
                lDescriptor.m_baseResourceDescriptors[luEntry].m_alignment = 1u;
            }
            lDescriptor.m_baseResourceDescriptors[0].m_size      = luSize;
            lDescriptor.m_baseResourceDescriptors[0].m_alignment = luAlignment;

            rw::Resource lResource = lpAllocator->DoAllocate(
                reinterpret_cast<const rw::ResourceDescriptor&>(lDescriptor), 0);
            return lResource.m_baseResources[0];
        }

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
    // configuration the engine boots with. ⭐ THE REAL X360 VALUES (rodata @0x820DC120, big-endian,
    // read from the decrypted XEX): {100, 0x200000, 25, 200, 50, 50, 50, 10, NULL}. The game module's
    // Construct copies this block onto the stack and overrides six fields before calling Construct
    // (see BrnGameModule.cpp) - the two menu pool sizes are the only fields the game keeps at DEFAULT.
    const DebugManagerConstructParameters DebugManagerConstructParameters::DEFAULT =
    {
        /* miPerfMonCpuCount          */ 100,       // 0x64
        /* miPerfMonLogBufferSize     */ 2097152,   // 0x200000 (2MB CPU-trace log buffer)
        /* miMenuWindowPoolSize       */ 25,
        /* miMenuPoolSize             */ 200,
        /* miFunctionPoolSize         */ 50,
        /* miVariablePoolSize         */ 50,
        /* miVariableMetadataPoolSize */ 50,
        /* miConsoleLineCount         */ 10,
        /* mpRwAllocator              */ nullptr,
    };

    DebugManager* DebugManager::mpInstance = nullptr;

    // X360 @0x82822370: the three by-value components construct (their vtable stores are the inlined
    // component ctors; the two buffered-renderer queue flag bytes zero via the queues' own ctors),
    // then the singleton slot is asserted free and CLAIMED - by the ctor, not Construct.
    // FLAG (static-message assert): the X360 fires "mpInstance == NULL" at CgsDebugManager.cpp:107;
    // the CGS_ASSERT macro supplies THIS file/line instead.
    DebugManager::DebugManager()
    {
        CGS_ASSERT(mpInstance == nullptr, "mpInstance == NULL");
        mpInstance = this;
    }

    DebugManager::~DebugManager() {}

    // Faithful port of X360 Construct @0x828332C0, in the console's own order:
    //   1. CgsDev::Assert::Construct()                       - assert-system bring-up
    //   2. rw::core::debug::Manager::CreateInstance(default) - the rw debug subsystem
    //   3. mpAllocator = params->mpRwAllocator, defaulted to rw's default allocator
    //   4. mComponentList = 0; DebugCriticalSection::Create(gDebugManagerSection)
    //   5. mp3dRender = mp2dRender = 0; the two buffered-renderer queues Construct
    //   6. mpUI = &gInternalDebugUI; UI Construct(params)
    //   7. carve the optional CPU-trace log buffer (params->miPerfMonLogBufferSize, align 16,
    //      through params->mpRwAllocator - asserted non-null on a failed carve)
    //   8. CPU perfmon component Construct(count, buffer, size) + Register
    //   9. GPU perfmon component Construct + Register (the X360 inlines its Construct)
    //  10. message-filter component Construct + Register
    //  11. CalculateBuildDate; ConstructRenderer
    void DebugManager::Construct(const DebugManagerConstructParameters* lpParameters)
    {
        Assert::Construct();
        rw::core::debug::Manager::CreateInstance(rw::ResourceAllocatorRegistry::GetDefaultAllocator());

        mpAllocator = lpParameters->mpRwAllocator;
        if (mpAllocator == nullptr)
            mpAllocator = rw::ResourceAllocatorRegistry::GetDefaultAllocator();

        mComponentList.Clear();
        gDebugManagerSection.Create();

        mp3dRender = nullptr;
        mp2dRender = nullptr;
        mBufferedRenderer.Construct();   // the two VariableEventQueue<16384,16>s (X360 inlines the pair)

        mpUI = &gInternalDebugUI;
        mpUI->Construct(lpParameters);

        // The optional CPU-trace log buffer (X360 @0x8283334C-B0): carved through the PARAMETER
        // allocator (not the defaulted member - the console reads params+0x14 again), align 16.
        // The game module passes size 0, so the carve is skipped on the boot path.
        void* lpBuffer = nullptr;
        s32 liLogBufferSize = 0;
        if (lpParameters->miPerfMonLogBufferSize > 0)
        {
            lpBuffer = AllocateMemoryResource(lpParameters->mpRwAllocator,
                                              static_cast<u32>(lpParameters->miPerfMonLogBufferSize), 16u);
            CGS_ASSERT(lpBuffer != nullptr, "lpBuffer != NULL");
            liLogBufferSize = lpParameters->miPerfMonLogBufferSize;
        }

        mDebugComponentPerfMonCpu.Construct(lpParameters->miPerfMonCpuCount, lpBuffer,
                                            static_cast<u32>(liLogBufferSize));
        mDebugComponentPerfMonCpu.Register();

        mDebugComponentPerfMonGpu.Construct();
        mDebugComponentPerfMonGpu.Register();

        mDebugComponentMessageFilter.Construct();
        mDebugComponentMessageFilter.Register();

        CalculateBuildDate();
        ConstructRenderer();
    }

    // Faithful port of X360 ConstructRenderer @0x8281ADD0: read the virtual screen size out of the
    // UI Metrics (mpUI+0x68/+0x6C = mfScreenWidth/mfScreenHeight - DebugUI::Construct has already
    // copied Metrics::DEFAULT in), wire + construct the 3D then the 2D immediate renderer (each gets
    // the manager's allocator + the screen size), hand the 2D renderer to the UI (mpUI+0x14), and
    // publish it to the assert system (X360: the dword_83018F1C global the Assert::Manager draw
    // path reads; modelled as Assert::Manager::SetRenderer).
    void DebugManager::ConstructRenderer()
    {
        const DebugUI::Metrics& lrMetrics = mpUI->GetMetrics();

        mp3dRender = &g3dInternalDebugRender;
        mp3dRender->Construct(mpAllocator, lrMetrics.mfScreenWidth, lrMetrics.mfScreenHeight);

        mp2dRender = &g2dInternalDebugRender;
        mp2dRender->Construct(mpAllocator, lrMetrics.mfScreenWidth, lrMetrics.mfScreenHeight);

        mpUI->Set2DRenderer(mp2dRender);
        Assert::gAssertManager.SetRenderer(mp2dRender);
    }

    // Faithful port of X360 CalculateBuildDate @0x828224B8: derive the running executable's path
    // from the command line ("D:\%s", quotes stripped - the console's disc root), read its file
    // timestamp, and SPrintf "Build Date: hh:mm:ss dd/mm/yyyy" into macBuildDate (0x24 bytes). When
    // the file can't be opened the fallback is the compile date/time - the X360 image carries its
    // own baked "Jan 30 2008" / "18:46:29" literals here, i.e. __DATE__/__TIME__. (On PC the
    // "D:\<command line>" path never resolves, so the fallback branch is the live one - same code,
    // console-identical behaviour up to the CreateFileA result.)
    void DebugManager::CalculateBuildDate()
    {
        const char* lpcCommandLine = GetCommandLineA();
        if (*lpcCommandLine == '"')
            ++lpcCommandLine;

        char lacFileName[0x104];
        CgsCore::SPrintf(lacFileName, 0x104, "D:\\%s", lpcCommandLine);

        // Strip a trailing quote (the closing half of a quoted module path).
        u32 luLength = 0;
        while (lacFileName[luLength] != '\0')
            ++luLength;
        if (luLength > 0 && lacFileName[luLength - 1] == '"')
            lacFileName[luLength - 1] = '\0';

        HANDLE lhFile = CreateFileA(lacFileName, GENERIC_READ, FILE_SHARE_READ, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (lhFile == INVALID_HANDLE_VALUE)
        {
            CgsCore::SPrintf(macBuildDate, 0x24, "Build Date: %s %s", __DATE__, __TIME__);
            return;
        }

        FILETIME lLastWriteTime;
        const bool lbGetTimeSuccess = GetFileTime(lhFile, nullptr, nullptr, &lLastWriteTime) != 0;
        CGS_ASSERT(lbGetTimeSuccess, "lbGetTimeSuccess");
        CloseHandle(lhFile);

        FILETIME   lLocalFileTime;
        SYSTEMTIME lSystemTime;
        FileTimeToLocalFileTime(&lLastWriteTime, &lLocalFileTime);
        FileTimeToSystemTime(&lLocalFileTime, &lSystemTime);

        CgsCore::SPrintf(macBuildDate, 0x24, "Build Date: %02d:%02d:%02d %02d/%02d/%02d",
                         lSystemTime.wHour, lSystemTime.wMinute, lSystemTime.wSecond,
                         lSystemTime.wDay, lSystemTime.wMonth, lSystemTime.wYear);
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
        mBufferedRenderer.Dispatch2D(mp2dRender, true);

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

        mBufferedRenderer.Draw2DText(lacText, lfX, lfY, 20.0f, luColour);
    }

    // X360 0x8282D8F8. Queue the build-info string (bottom-left debug readout): the macBuildDate
    // member CalculateBuildDate filled during Construct. Scale 12; x = metrics+112 (a left value -
    // estimated), y = height - lineHeight.
    void DebugManager::RenderBuildInfo()
    {
        const DebugUI::Metrics& lrMetrics = mpUI->GetMetrics();
        const f32 lfX = lrMetrics.mfScreenBorderLeft;
        const f32 lfY = lrMetrics.mfScreenHeight - lrMetrics.mfScreenBorderBottom;

        mBufferedRenderer.Draw2DText(macBuildDate, lfX, lfY, 12.0f, 0xFFFFFFFFu);
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
        mBufferedRenderer.Draw2DText(lacText, lfX, lfY, 16.0f, 0xFFFFFFFFu);
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
        mBufferedRenderer.Draw2DText(lacBuffer, 50.0f, 50.0f, 16.0f, luColour);

        mBufferedRenderer.Draw2DText(lpData->macAssertMessage, 50.0f, 68.0f, 16.0f, luColour);

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
            mBufferedRenderer.Draw2DText(lpcName, 50.0f, lfY, 16.0f, luColour);
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
        mBufferedRenderer.Dispatch2D(mp2dRender, true);          // flush them
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

    // DebugInterface::Get2dRender lived here while mBufferedRenderer was modelled as a file-static;
    // it is now a real DebugManager member, so the body is back at its console home
    // (CgsDebugInterface.cpp, X360 @0x82822750).
}
