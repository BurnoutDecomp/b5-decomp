#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameSource/Gui/Flow/Screen/Components/Replays/BrnReplayHudMessageComponent.h"

// BrnGui::ReplayMain -- the "replay main" GUI state (X360 BrnReplayMain.{h,cpp}, rodata path
// ..\\..\\..\\GameSource\\Gui/Flow/Screen/States/Replays/BrnReplayMain.cpp). A sibling of the
// committed BrnGui::ReplayLoading / BrnGui::ReplayOutro replay screen states, but the *running*
// replay screen: it observes one GUI event, latches the GuiCache, optionally embeds a
// ReplayHudMessageComponent (when the cache's replay-hud flag is set), then per-frame drains the
// in-queue and, on the controller-accept event (id 50) or when the replay clock runs out / a
// cache abort bit is set, sends ADVANCE; otherwise it ticks the embedded HUD-message banner.
// Layout / virtual set reconstructed from BURNOUT_X360_ARTIST.XEX
// (OnEnter 0x824BA1C0 / OnLeave 0x824BA2A0 / Update 0x824C2C88 / UpdateRunning 0x824C2970).
namespace BrnGui
{
    class GuiCache;

    struct ReplayMain : public CgsGui::State
    {
        // Internal replay-screen state machine (X360 this+0x3C). Update() walks 0 -> 1 -> 2 like
        // the loading/outro states (each case re-arms then falls through), but there is no
        // resource-load phase here: cases 0/1/2 all end in the RUNNING tick.
        enum EState
        {
            E_STATE_ENTER    = 0,   // one-frame enter latch
            E_STATE_READY    = 1,   // one-frame ready latch
            E_STATE_RUNNING  = 2,   // drain the in-queue; tick the HUD banner
            E_STATE_DONE     = 3,   // leaving / torn down
        };

        // @0x824BA1C0 / @0x824BA2A0 / @0x824C2C88 -- the FSM virtuals.
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

    private:
        // @0x824C2970 -- per-frame in-queue drain; on accept (50) send ADVANCE, else on the
        // replay clock running out / cache abort bit send ADVANCE, else tick the HUD banner.
        void UpdateRunning();

        // ---- members (X360 +0x38 / +0x3C / +0x40, over the CgsGui::State base) ----
        GuiCache*                 mpGuiCache;      // @ +0x38
        EState                    meState;         // @ +0x3C
        ReplayHudMessageComponent mHudMessage;     // @ +0x40 (constructed only when the cache's
                                                   //          replay-hud flag is set)

        // ---- statics ----
        // X360 .rdata: the single observed replay GUI event id (&dword_82066A4C, count 1). The
        // id VALUE was not decoded in this packet (only the base address + the count of 1 are
        // attested); placeholder so the state links, adopted with the XEX-recovered id when
        // decoded. Shared by OnEnter (RegisterForEvents) and OnLeave (UnRegisterForEvents).
        static const s32 KAI_OBSERVED_EVENTS[];   // @0x82066A4C
        static const s32 KI_NUM_OBSERVED_EVENTS;  // == 1
    };
}
