#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                                            // Matrix44, Vector3 (render entry)
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugCollections.h"  // DebugLinkedList<DebugComponent>

// CgsDev::DebugManager - the process-wide owner of the in-game debug systems (perfmon overlays,
// debug menus, console, on-screen variables): it holds the DebugUI, the resource allocator, and
// the registered-component list, ticks them each frame, and serialises access behind a debug
// critical section. Recovered from the DecFIGS DWARF
// (Development/DebugSystem/Core/CgsDebugManager.h).
//
// INCREMENTAL LAYOUT: the full manager is large (the X360 reaches the registered-component list
// head at this+33132, with the DebugUI / render / allocator members alongside it). Only the
// per-frame Update entry point (the game module's update spine calls it) and the singleton pointer
// the thread-safe accessors hand out are modelled; the rest of the layout + the method bodies are
// the manager-reconstruction follow-on. This header is the interface both the game-module update
// spine and the DebugComponent registration path (Register -> RegisterComponent[Simple]) compile
// against.

namespace CgsDev
{
    class DebugComponent;
    struct Debug2DImmediateRender;   // the 2D debug renderer (HUD squares/lines/text)
    struct Debug3DImmediateRender;   // the 3D (world-space) debug renderer - render follow-on

    namespace DebugUI { struct DebugUI; }

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
namespace CgsGraphics { struct Im2d; }
namespace CgsDev
{

    class DebugManager
    {
    public:
        DebugManager();
        ~DebugManager();

        // Per-frame tick (called by BrnGameModule's update spine with the frame delta time).
        void Update(f32 lfDeltaTime);

        // Thread-safe singleton access (X360 ThreadSafeAquire 0x821F1E50 / ThreadSafeRelease):
        // Aquire asserts the singleton exists, enters the debug critical section, and returns it;
        // Release leaves the section.
        static DebugManager* ThreadSafeAquire();
        static void          ThreadSafeRelease(DebugManager* lpManager);

        void Construct(const DebugManagerConstructParameters* lpParameters);
        void ConstructRenderer();
        void Destruct();

        bool IsComponentRegistered(DebugComponent* lpComponent);
        void ActivateComponent(DebugComponent* lpComponent);

        // Add a component to the menu tree. RegisterComponent builds the full menu hierarchy;
        // RegisterComponentSimple registers it as a flat ("simple") entry. Both assert the
        // component is not already registered (X360 RegisterComponentSimple 0x8282E1E0).
        void RegisterComponent(DebugComponent* lpComponent, const char* lpcPath, const char* lpcName);
        void RegisterComponentSimple(DebugComponent* lpComponent, const char* lpcPath, const char* lpcName);

        DebugComponent*   FindComponentByName(const char* lpcName);
        DebugUI::DebugUI& GetUI();

        // Per-frame debug render spine (X360 Render 0x8282F770 -> RenderWorld + RenderHUD 0x8282E108).
        // RenderHUD is the 2D screen-space pass that draws the debug overlay (the "debug squares"):
        // Debug2DImmediateRender::Begin -> flush queued debug prims + each active component's RenderHUD
        // + the DebugUI -> Debug2DImmediateRender::End. The bodies + the renderers (mp2dRender/mp3dRender)
        // + the full Render(viewproj, cameraPos, buffers) entry are the render-spine follow-on; this
        // declares the HUD pass the loading-screen render path will drive into mIm2dDebugRenderBuffer.
        void RenderHUD();

        // X360 Render 0x8282F770: point the renderers at this frame's buffers, then RenderWorld (3D)
        // + RenderHUD (2D). The 3D path (RenderWorld + mp3dRender setup) is the Debug3D follow-on;
        // the 2D path drives the squares.
        void Render(const Matrix44& lViewProjection, const Vector3& lCameraPosition,
                    CgsGraphics::Im2d* lp3dRenderBuffer, CgsGraphics::Im2d* lp2dRenderBuffer);
        void RenderWorld(const Matrix44& lViewProjection, const Vector3& lCameraPosition);

    private:
        // INCREMENTAL: the registered-component list (X360 this+33132) + the UI the manager owns
        // (reached at a fixed offset on X360; modelled as a pointer here) are the members the
        // registration path touches; the rest of the ~33KB layout is the manager follow-on.
        DebugUI::DebugUI*                          mpUI;
        Internal::DebugLinkedList<DebugComponent>  mComponentList;

        // The debug renderers the render spine drives (set up by ConstructRenderer; the renderer
        // types themselves are the next brick - Debug2DImmediateRender draws the HUD squares).
        Debug2DImmediateRender*                    mp2dRender;
        Debug3DImmediateRender*                    mp3dRender;

        static DebugManager* mpInstance;
    };
}
