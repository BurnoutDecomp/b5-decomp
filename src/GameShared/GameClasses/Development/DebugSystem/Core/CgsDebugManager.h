#pragma once

#include "types.hpp"
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
    struct DebugManagerConstructParameters;

    namespace DebugUI { struct DebugUI; }

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

    private:
        // INCREMENTAL: the registered-component list (X360 this+33132) + the UI the manager owns
        // (reached at a fixed offset on X360; modelled as a pointer here) are the members the
        // registration path touches; the rest of the ~33KB layout is the manager follow-on.
        DebugUI::DebugUI*                          mpUI;
        Internal::DebugLinkedList<DebugComponent>  mComponentList;

        static DebugManager* mpInstance;
    };
}
