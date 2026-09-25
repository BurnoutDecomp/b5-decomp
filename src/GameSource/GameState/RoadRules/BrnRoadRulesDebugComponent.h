#pragma once

// b5-decomp/src/GameSource/GameState/RoadRules/BrnRoadRulesDebugComponent.h
//
// BrnGameState::RoadRulesDebugComponent - the in-game debug menu + HUD overlay for the road-rules
// manager. Derives from the real CgsDev::DebugComponent and is embedded BY VALUE as the first
// member of BrnGameState::RoadRulesManager (mRoadRulesDebugComponent, manager +0x00).
//
// Console object layout (0x14 bytes):
//   CgsDev::DebugComponent base  +0x00..+0x0B  (vtable, mbActive, mpDebugLinkedListNext)
//   mpRoadRulesManager           +0x0C  (the back-pointer every callback derefs)
//   mbRenderInfo                 +0x10  (RegisterVariable(&mbRenderInfo, "Render info"))
//   mbRenderTimes                +0x11  (RegisterVariable(&mbRenderTimes, "Render times"))
//
// Console vtable (8 slots, the DebugComponent order): Update (shared empty body), RenderWorld
// (shared empty body), RenderHUD, GetName ("Road Rules", folded with the network road-rules
// component's GetName), GetPath ("Gameplay", the shared gameplay-path body), IsSimple (base),
// OnActivate, OnRegister (base).
//
// The component reads and writes the manager's private timing / score state directly, so
// RoadRulesManager grants it friendship.

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"  // CgsDev::DebugComponent (real base)

#include <cstddef>   // offsetof (_AssertLayout)

namespace CgsDev { struct Debug2DImmediateRender; struct Debug3DImmediateRender; }

namespace BrnGameState
{
    class RoadRulesManager;   // back-pointer member only; full definition in BrnRoadRulesManager.h

    class RoadRulesDebugComponent : public CgsDev::DebugComponent
    {
    public:
        // Inlined into RoadRulesManager::Construct: store the back-pointer, clear both render
        // toggles, then CgsDev::DebugComponent::Register() (the base Construct is not called).
        void Construct(RoadRulesManager* lpRoadRulesManager);

        void RenderHUD(CgsDev::Debug2DImmediateRender* lpRender) override;
        void RenderWorld(CgsDev::Debug3DImmediateRender* lpRender) override;

        // Menu-action bodies; each is inlined into its static callback below.
        void DecreaseCurrentTime();
        void DecreaseCurrentStuntTime();
        void AddCrashScore();

    protected:
        const char* GetName() const override;
        const char* GetPath() const override;
        void        OnActivate() override;

    private:
        // Static callbacks registered with the debug menu via RegisterFunction(&cb, this, name).
        // The void* user-data the menu passes back IS this component.
        static void DecreaseCurrentTimeCallback(void* lpData);
        static void DecreaseCurrentStuntTimeCallback(void* lpData);
        static void AddCrashScoreCallback(void* lpData);

        // HUD layout constants (read-only data): the score-table column X positions, the
        // row-label column X and the header-row Y.
        static const f32 KF_PAR_SCORES_X;      // 250.0
        static const f32 KF_PLAYER_SCORES_X;   // 300.0
        static const f32 KF_NET_SCORES_X;      // 350.0
        static const f32 KF_ROAD_NAME_X;       // 100.0
        static const f32 KF_SCORES_Y;          //  50.0

        RoadRulesManager* mpRoadRulesManager;   // +0x0C
        bool              mbRenderInfo;          // +0x10
        bool              mbRenderTimes;         // +0x11

    public:
        // Never called; pins the member ORDER on the host. The three members follow the base
        // sub-object directly (console +0x0C == sizeof the console base), so the host offsets are
        // stated against sizeof(CgsDev::DebugComponent) and sizeof(void*), not the console numbers.
        static void _AssertLayout()
        {
            static_assert(offsetof(RoadRulesDebugComponent, mpRoadRulesManager) == sizeof(CgsDev::DebugComponent),
                          "mpRoadRulesManager directly follows the DebugComponent base (console +0x0C)");
            static_assert(offsetof(RoadRulesDebugComponent, mbRenderInfo) ==
                          offsetof(RoadRulesDebugComponent, mpRoadRulesManager) + sizeof(void*),
                          "mbRenderInfo follows mpRoadRulesManager (console +0x10)");
            static_assert(offsetof(RoadRulesDebugComponent, mbRenderTimes) ==
                          offsetof(RoadRulesDebugComponent, mbRenderInfo) + 1,
                          "mbRenderTimes follows mbRenderInfo (console +0x11)");
        }
    };
}
