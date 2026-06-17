#pragma once

// b5-decomp/src/GameSource/GameState/RoadRules/BrnRoadRulesDebugComponent.h
//
// BrnGameState::RoadRulesDebugComponent - the in-game debug menu + HUD overlay for the road-rules
// manager. Derives from the real CgsDev::DebugComponent. Base/member layout is authoritative from
// the DecFIGS DWARF (the manager embeds this as its first member mRoadRulesDebugComponent @ +0, so
// the RoadRulesManager.h DWARF pins this object's size at 20 bytes) and the X360 binary:
//   DebugComponent base sub-object  +0x00..+0x0B  (vtable@0, mbActive@4, mpDebugLinkedListNext@8)
//   mpRoadRulesManager              +0x0C   (== the *(this+12) back-pointer deref in every fn)
//   mbRenderInfo                    +0x10   (== *(this+16); RegisterVariable(this+16,"Render info"))
//   mbRenderTimes                   +0x11   (== *(this+17); RegisterVariable(this+17,"Render times"))
// (padded to 20 bytes -> matches the maRoadRulesDebugComponentStorage[20] placeholder the committed
// BrnRoadRulesManager.h reserves at its +0.)
//
// This component reaches DIRECTLY into the (private) road-rules manager state, so the real build
// makes RoadRulesManager grant friendship to this class. The callbacks/RenderHUD touch the manager's
// mfTime/mfStuntTime/miCrashScore/maiChallengeRoadIndex/miLastRoadIndex/mpStreetManager (those
// members are materialised + the friend granted in the BrnRoadRulesManager.h grow that lands with
// this slice).

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"  // CgsDev::DebugComponent (real base)

namespace CgsDev { struct Debug2DImmediateRender; struct Debug3DImmediateRender; }

namespace BrnGameState
{
    class RoadRulesManager;   // back-pointer member only; full def in BrnRoadRulesManager.h (the .cpp includes it)

    class RoadRulesDebugComponent : public CgsDev::DebugComponent
    {
    public:
        void Construct(RoadRulesManager* lpRoadRulesManager);   // @ BrnRoadRulesDebugComponent.cpp:50 (own pass)
        void Destruct();                                        // @ BrnRoadRulesDebugComponent.cpp:65 (own pass)

        void RenderHUD(CgsDev::Debug2DImmediateRender* lpRender) override;   // @ 0x82335350
        void RenderWorld(CgsDev::Debug3DImmediateRender* lpRender) override; // declared-only (own pass)

        // Menu-action implementations (the manual "Decrease current time" etc. buttons).
        void DecreaseCurrentTime();        // declared-only (own pass)
        void DecreaseCurrentStuntTime();   // declared-only (own pass)
        void AddCrashScore();              // declared-only (own pass)

    protected:
        const char* GetName() const override;     // declared-only (own pass)
        const char* GetPath() const override;     // declared-only (own pass)
        void        OnActivate() override;          // @ 0x823248E0

    private:
        // Static callbacks registered with the debug menu via RegisterFunction(&cb, this, name).
        // The void* user-data the menu passes back IS this component.
        static void DecreaseCurrentTimeCallback(void* lpData);       // @ 0x823171F0
        static void DecreaseCurrentStuntTimeCallback(void* lpData);  // @ 0x82317220
        static void AddCrashScoreCallback(void* lpData);             // @ 0x82317250

        // HUD column / row layout constants (BrnRoadRulesDebugComponent.cpp:34-38). The X360 keeps
        // these as file/class-scope const f32 in .rdata; the score-table header X positions + the
        // header Y. (DWARF lists them inside the struct, defined in the .cpp.)
        static const f32 KF_PAR_SCORES_X;      // 250.0 (Par column)
        static const f32 KF_PLAYER_SCORES_X;   // 300.0 (Player column)
        static const f32 KF_NET_SCORES_X;      // 350.0 (Net/online column)
        static const f32 KF_ROAD_NAME_X;       // road-name/index column X (X360 flt_820049E0; FLAGGED, see .cpp)
        static const f32 KF_SCORES_Y;          // 50.0  (header row Y)

        // ---- DWARF member layout (BrnRoadRulesDebugComponent.h:106-108) ----
        RoadRulesManager* mpRoadRulesManager;   // +0x0C
        bool              mbRenderInfo;          // +0x10
        bool              mbRenderTimes;         // +0x11
    };
}
