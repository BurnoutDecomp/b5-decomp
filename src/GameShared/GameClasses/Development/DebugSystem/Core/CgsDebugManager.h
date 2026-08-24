#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                                            // Matrix44, Vector3 (render entry)
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugCollections.h"  // DebugLinkedList<DebugComponent>
#include "GameShared/GameClasses/Fonts/CgsFont.h"                                      // SafeResourceHandle<Font> (SetDebugFont)
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"     // mBufferedRenderer (by value, X360 +0x14C)
#include "GameShared/GameClasses/Development/PerfMon/DebugComponent/CgsDebugComponentPerfMonCpu.h"     // mDebugComponentPerfMonCpu (+0x000)
#include "GameShared/GameClasses/Development/PerfMon/DebugComponent/CgsDebugComponentPerfMonGpu.h"     // mDebugComponentPerfMonGpu (+0x118)
#include "GameShared/GameClasses/Development/MessageSystem/DebugComponent/CgsDebugComponentMessageFilter.h" // mDebugComponentMessageFilter (+0x12C)

// CgsDev::DebugManager - the process-wide owner of the in-game debug systems (perfmon overlays,
// debug menus, console, on-screen variables): it holds the DebugUI, the resource allocator, and
// the registered-component list, ticks them each frame, and serialises access behind a debug
// critical section. Recovered from the DecFIGS DWARF
// (Development/DebugSystem/Core/CgsDebugManager.h).
//
// LAYOUT (X360 ctor @0x82822370 + Construct @0x828332C0): the manager OWNS its three built-in
// debug components by value at the front of the object (CPU perfmon @+0x000, GPU perfmon @+0x118,
// message filter @+0x12C), then the UI/renderer pointers (+0x140/+0x144/+0x148), the by-value
// buffered renderer (two VariableEventQueue<16384,16>, +0x14C), the registered-component list head
// (+0x816C), the debug resource allocator (+0x8170) and the build-date string CalculateBuildDate
// fills (+0x8174, 0x24 bytes). All access is by member name (x64 offsets differ).

namespace CgsDev
{
    class DebugComponent;
    struct Debug2DImmediateRender;   // the 2D debug renderer (HUD squares/lines/text)
    struct Debug3DImmediateRender;   // the 3D (world-space) debug renderer - render follow-on

    namespace DebugUI { struct DebugUI; struct ScriptInterface; }
    namespace Assert { struct AssertData; }   // RenderAssert's input (the failing assert)

    // X360 CgsDebugManager.h:95. The pool sizes + perfmon/console configuration the whole debug
    // system is sized from: DebugManager::Construct forwards this to DebugUI::Construct, which hands
    // it to the three managers - each sizes its DebugStaticPools from the matching field (menu pool
    // <- miMenuPoolSize, menu-item pool <- the owning element's pool size, etc.). DEFAULT is the
    // built-in configuration the engine boots with when no explicit parameters are supplied; its
    // values are defined in CgsDebugManager.cpp. Field order/types mirror the DWARF.
    struct DebugManagerConstructParameters
    {
        s16 miPerfMonCpuCount;
        s32 miPerfMonLogBufferSize;
        s16 miMenuWindowPoolSize;
        s16 miMenuPoolSize;
        s16 miFunctionPoolSize;
        s16 miVariablePoolSize;
        s16 miVariableMetadataPoolSize;
        s8  miConsoleLineCount;
        rw::IResourceAllocator* mpRwAllocator;

        static const DebugManagerConstructParameters DEFAULT;
    };
}
namespace CgsGraphics { struct Im2d; class Im3dRenderBuffer; }
namespace CgsDev
{

    class DebugManager
    {
    public:
        // X360 @0x82822370: run the three by-value component ctors, assert the singleton slot is
        // free ("mpInstance == NULL", CgsDebugManager.cpp:107), then CLAIM it (the ctor, not
        // Construct, publishes mpInstance).
        DebugManager();
        ~DebugManager();

        // Per-frame tick (called by BrnGameModule's update spine with the frame delta time).
        void Update(f32 lfDeltaTime);

        // Thread-safe singleton access (X360 ThreadSafeAquire 0x821F1E50 / ThreadSafeRelease):
        // Aquire asserts the singleton exists, enters the debug critical section, and returns it;
        // Release leaves the section.
        static DebugManager* ThreadSafeAquire();
        static void          ThreadSafeRelease(DebugManager* lpManager);

        // Non-locking singleton read (X360: the raw DebugManager::mpInstance load). The render pass
        // already holds the debug section, so it reads mpInstance directly rather than re-entering
        // the critical section via ThreadSafeAquire (which asserts + locks).
        static DebugManager* GetInstance() { return mpInstance; }

        // X360 Construct @0x828332C0 -- the debug-system bring-up. In the console's order: assert
        // front-end Construct, rw debug Manager instance (over the default allocator), latch the
        // debug allocator (parameter, defaulted to rw's), clear the component list, Create the
        // debug critical section, null the renderers, construct the buffered renderer's queues,
        // wire + construct the UI, carve the optional perfmon log buffer, construct + Register the
        // three built-in components, CalculateBuildDate, ConstructRenderer. Note ConstructRenderer
        // is called BY Construct (the game module does not call it separately).
        void Construct(const DebugManagerConstructParameters* lpParameters);
        // X360 @0x8281ADD0: wire + construct BOTH immediate renderers (3D then 2D) at the UI
        // metrics' screen size, hand the 2D renderer to the UI and to the assert system.
        void ConstructRenderer();
        void Destruct();

        // X360 @0x828224B8: fill macBuildDate ("Build Date: ...") from the running executable's
        // file timestamp, falling back to the compile date/time when the file can't be opened.
        void CalculateBuildDate();

        bool IsComponentRegistered(DebugComponent* lpComponent);
        void ActivateComponent(DebugComponent* lpComponent);

        // Add a component to the menu tree. RegisterComponent builds the full menu hierarchy;
        // RegisterComponentSimple registers it as a flat ("simple") entry. Both assert the
        // component is not already registered (X360 RegisterComponentSimple 0x8282E1E0).
        void RegisterComponent(DebugComponent* lpComponent, const char* lpcPath, const char* lpcName);
        void RegisterComponentSimple(DebugComponent* lpComponent, const char* lpcPath, const char* lpcName);

        DebugComponent*   FindComponentByName(const char* lpcName);
        DebugUI::DebugUI& GetUI();

        // Hand a loaded bitmap font to the debug renderers (X360 0x823B14E8). The game calls this from
        // GamePrepare once the "Default.font" bundle resolves + Font::CreateTextureState has run; it
        // sets the font on BOTH the 3D and 2D immediate renderers so their DrawText uses the resource
        // font (instead of the vector-font fallback). Asserts the handle is not NULLResourceHandle.
        void SetDebugFont(const CgsResource::SafeResourceHandle<CgsResource::Font>& lrFont);

        // Per-frame debug render spine (X360 Render 0x8282F770 -> RenderWorld + RenderHUD 0x8282E108).
        // RenderHUD is the 2D screen-space pass that draws the debug overlay (the "debug squares"):
        // Debug2DImmediateRender::Begin -> flush queued debug prims + each active component's RenderHUD
        // + the DebugUI -> Debug2DImmediateRender::End. The bodies + the renderers (mp2dRender/mp3dRender)
        // + the full Render(viewproj, cameraPos, buffers) entry are the render-spine follow-on; this
        // declares the HUD pass the loading-screen render path will drive into mIm2dDebugRenderBuffer.
        void RenderHUD();

        // X360 Render @0x8282F770: assert both renderers exist, point them at this frame's debug
        // render buffers, then RenderWorld (3D) + RenderHUD (2D). Nothing else - the overlay
        // QUEUEING (build info / fps / memory) is BrnGameModule::DebugManagerRender's job.
        void Render(const Matrix44& lViewProjection, const Vector3& lCameraPosition,
                    CgsGraphics::Im3dRenderBuffer* lp3dRenderBuffer, CgsGraphics::Im2d* lp2dRenderBuffer);
        // X360 @0x8282E030: 3D Begin -> Dispatch3D the buffered world-space prims -> each active
        // component's RenderWorld -> End. (The 3D draw bodies are the Debug3D render follow-on.)
        void RenderWorld(const Matrix44& lViewProjection, const Vector3& lCameraPosition);

        // X360 0x8282D998. Queue the on-screen frame-rate readout ("%d fps") into the buffered renderer,
        // coloured by framerate (low->mid->high, e.g. red->yellow->green) via _InterpolateColour. Colours
        // are packed RGBA (u32). Positioned from the screen metrics (bottom debug bar). RenderHUD flushes it.
        void RenderFrameRateColouredWithAverage(f32 lfFramerate, f32 lfAverageFramerate,
                                                u32 lHighColour, u32 lLowColour, u32 lMidColour,
                                                f32 lfHighFramerate, f32 lfLowFramerate,
                                                const char* lpcAverageText, f32 lfAverageHighlight, bool lbIsRealtime);

        // X360 0x8282D8F8 / 0x8282DD28. The other two bottom-bar debug readouts (queued into the buffered
        // renderer): the build-info string (left) and the available-memory line (centre-right).
        void RenderBuildInfo();
        void RenderMemory();

        // X360 0x8282DE28 DebugManager::RenderAssert - the on-screen assert OVERLAY (what the real ARTIST
        // build shows): "line:file" + the failed expression + the call-stack (map-resolved names, else
        // 0x%08X), queued into the buffered renderer at x=50, size 16, 18px line advance. The render path
        // (BrnRendererModule::RenderAssert, per-thread) drives this; RenderAssertOverlay flushes one frame
        // of it through the 2D renderer for the single-threaded freeze.
        void RenderAssert(const Assert::AssertData* lpData);
        void RenderAssertOverlay();

        // The script runner's SaveState serialises the active components by walking the (private,
        // accessor-less) registered-component list inline (X360 SaveState 0x82832660 reads the head
        // at this+0x816C). dwarfdump does not surface friend declarations; attested by that asm.
        friend struct DebugUI::ScriptInterface;
        // DebugInterface::Get2dRender (X360 @0x82822750) returns mBufferedRenderer (this+0x14C)
        // directly, and the DebugInternal accessors (e.g. GetUI @0x82815F08) read the singleton's
        // members raw -- both reach private members by offset on the console, modelled as friends.
        friend struct DebugInterface;
        friend struct Internal::DebugInternal;

    private:
        // X360 member layout (ctor @0x82822370 / Construct @0x828332C0); offsets in comments are
        // the console's -- access is by name.
        DebugComponentPerfMonCpu     mDebugComponentPerfMonCpu;     // +0x0000 the CPU perfmon overlay
        DebugComponentPerfMonGpu     mDebugComponentPerfMonGpu;     // +0x0118 the GPU perfmon overlay
        DebugComponentMessageFilter  mDebugComponentMessageFilter;  // +0x012C the message-filter page
        DebugUI::DebugUI*            mpUI;                          // +0x0140 -> gInternalDebugUI
        Debug3DImmediateRender*      mp3dRender;                    // +0x0144 -> g3dInternalDebugRender
        Debug2DImmediateRender*      mp2dRender;                    // +0x0148 -> g2dInternalDebugRender
        DebugRender                  mBufferedRenderer;             // +0x014C the buffered debug prims
        Internal::DebugLinkedList<DebugComponent> mComponentList;   // +0x816C registered components
        rw::IResourceAllocator*      mpAllocator;                   // +0x8170 the debug allocator
        char                         macBuildDate[36];              // +0x8174 CalculateBuildDate's string (0x24 bytes)

        static DebugManager* mpInstance;
    };
}
