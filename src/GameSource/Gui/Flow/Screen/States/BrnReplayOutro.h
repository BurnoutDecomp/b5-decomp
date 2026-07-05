#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::ReplayOutro -- the "replay outro" GUI state (X360 BrnReplayOutro.{h,cpp}, rodata
// path ..\\..\\..\\GameSource\\Gui/Flow/Screen/States/Replays/BrnReplayOutro.cpp). A near-twin
// of the committed BrnGui::ReplayLoading load-screen state, but a full outro FSM: it observes
// one GUI event, streams the outro's static resource list, binds the "ReplaysOutro" apt movie,
// then drains the in-queue every frame and, on the accept event (21), routes the flow to
// credits or advance. Layout / virtual set reconstructed from BURNOUT_X360_ARTIST.XEX
// (OnEnter 0x824BA2E8 / OnLeave 0x824D4F00 / Update 0x824DB868 / UpdateLoadResources 0x824D5038
// / UpdateRunning 0x824D5118).
namespace BrnGui
{
    class GuiCache;

    struct ReplayOutro : public CgsGui::State
    {
        // Internal outro state machine (X360 this+0x3C).
        enum EState
        {
            E_STATE_LOAD_RESOURCES  = 0,   // stream + bind the outro apt movie
            E_STATE_RESOURCES_READY = 1,   // one-frame ready latch
            E_STATE_RUNNING         = 2,   // drain the in-queue; wait for accept
            E_STATE_DONE            = 3,   // leaving / torn down
        };

        // @0x824BA2E8 / @0x824D4F00 / @0x824DB868 -- the FSM virtuals.
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

    private:
        // @0x824D5038 -- stream the outro resources; bind "ReplaysOutro" once loaded.
        bool UpdateLoadResources();
        // @0x824D5118 -- per-frame in-queue drain; on accept, route to credits/advance.
        void UpdateRunning();

        // ---- members (X360 +0x38/+0x3C, over the CgsGui::State base) ----
        GuiCache* mpGuiCache;   // @ +0x38
        EState    meState;      // @ +0x3C

        // ---- statics ----
        // X360 .rdata: the observed outro GUI event id array (&unk_82066A58, count 1) and the
        // outro's static resource list (&unk_82066A60, count 1). The tuple/id VALUES were not
        // decoded in this packet (only the base addresses + the count of 1 are attested);
        // placeholders so the state links, adopted with the XEX-recovered ids when decoded.
        static const s32                    KAI_OBSERVED_EVENTS[];   // @0x82066A58
        static const s32                    KI_NUM_OBSERVED_EVENTS;  // == 1
        static const CgsGui::sResourceTuple maResourcesToLoad[];     // @0x82066A60
        static const u32                    muNumResourcesToLoad;    // == 1
    };
}
