#include "GameSource/Gui/Flow/Screen/States/BrnReplayMain.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                      // CgsGui::GuiAccessPointers
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface (Register/GetAccessPointers)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // VariableEventQueue<18432,16> (the in-queue view)
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache

// BrnGui::ReplayMain -- reconstructed from BURNOUT_X360_ARTIST.XEX. The running replay screen
// state; a sibling of the committed BrnGui::ReplayLoading / BrnGui::ReplayOutro.
//   OnEnter       0x824BA1C0
//   OnLeave       0x824BA2A0
//   UpdateRunning 0x824C2970
//   Update        0x824C2C88

namespace BrnGui
{
namespace
{
    // The state IN-queue is an 18KB variable event queue (X360 VariableEventQueue<18432,16>),
    // the same view Credits / ReplayLoading / ReplayOutro cast mpInGuiEventQueue to.
    typedef CgsModule::VariableEventQueue<18432, 16> ReplayInQueue;

    // The in-queue controller-accept event id UpdateRunning acts on (X360 *(event+4) == 0x32).
    const s32 KI_EVENT_ACCEPT = 50;
    // The in-queue return code that flags an event carrying an id payload (X360 result == 6).
    const s32 KI_EVENT_RESULT_WITH_ID = 6;

    // FLAG boundary helper: the far GuiCache replay flags this state reads. There is no named
    // accessor on the committed BrnGui::GuiCache API for these, so they are read here through a
    // file-local boundary against the documented X360 byte offsets (mirrors the committed
    // ReplayLoadingCacheBoundary / ReplayOutroCacheBoundary). Replace with named GuiCache
    // accessors when the GuiCache TU homes them.
    struct ReplayMainCacheBoundary
    {
        // X360 *(mpGuiCache + 0x141D0) (float): the replay time remaining; < 1.0 -> advance.
        static const u32 KU_TIME_REMAINING_OFFSET = 0x141D0;
        // X360 *(mpGuiCache + 0x13BBC) (word): bit 6 set -> the replay is aborting; advance.
        static const u32 KU_ABORT_FLAG_WORD_OFFSET = 0x13BBC;
        // X360 *(mpGuiCache + 0x141D5) (byte): non-zero -> a replay HUD message banner is present.
        static const u32 KU_HUD_MESSAGE_FLAG_OFFSET = 0x141D5;

        static f32 GetTimeRemaining(const GuiCache* lpGuiCache)
        {
            return *reinterpret_cast<const f32*>(reinterpret_cast<const u8*>(lpGuiCache) +
                                                 KU_TIME_REMAINING_OFFSET);
        }
        static bool IsAborting(const GuiCache* lpGuiCache)
        {
            const u32 luFlags = *reinterpret_cast<const u32*>(reinterpret_cast<const u8*>(lpGuiCache) +
                                                              KU_ABORT_FLAG_WORD_OFFSET);
            return ((luFlags >> 6) & 1) == 1;
        }
        static u8 GetHudMessageFlag(const GuiCache* lpGuiCache)
        {
            return *(reinterpret_cast<const u8*>(lpGuiCache) + KU_HUD_MESSAGE_FLAG_OFFSET);
        }
    };
}   // anonymous namespace

// ---- statics -----------------------------------------------------------------
// X360 .rdata: the single observed replay GUI event id (&dword_82066A4C, count 1). FLAG: the id
// VALUE was not decoded in this packet (only the base address + the count of 1 are attested);
// placeholder so the state links, adopted with the XEX-recovered id when decoded.
const s32 ReplayMain::KAI_OBSERVED_EVENTS[]  = { 0 };   // @0x82066A4C (value not yet decoded)
const s32 ReplayMain::KI_NUM_OBSERVED_EVENTS = 1;

// ------------------------------------------------ OnEnter @ 0x824BA1C0
void ReplayMain::OnEnter()
{
    // Observe the single replay GUI event (X360 &dword_82066A4C, count 1).
    mpStateInterface->RegisterForEvents(KAI_OBSERVED_EVENTS, KI_NUM_OBSERVED_EVENTS);

    // Latch the GuiCache. Both accessors carry the X360's inlined NULL asserts
    // (CgsGuiStateInterface.h:344 "mpAccessPointers != NULL" / CgsGuiShared.h:201 "mpGuiCache").
    mpGuiCache = mpStateInterface->GetAccessPointers()->GetGuiCache();   // this+0x38

    // When the cache flags a replay HUD message banner (X360 *(mpGuiCache+0x141D5) == 1),
    // construct the embedded ReplayHudMessageComponent ("HudMessage_mc"). The X360 caller sets
    // only name + stateInterface (r4/r5); the base component parent-name arg is left unset here.
    if (ReplayMainCacheBoundary::GetHudMessageFlag(mpGuiCache) == 1)
        mHudMessage.Construct("HudMessage_mc", mpStateInterface, 0);   // this+0x40

    meState = E_STATE_ENTER;   // this+0x3C = 0
}

// ------------------------------------------------ OnLeave @ 0x824BA2A0
void ReplayMain::OnLeave()
{
    mpStateInterface->UnRegisterForEvents(KAI_OBSERVED_EVENTS, KI_NUM_OBSERVED_EVENTS);
    meState = E_STATE_DONE;   // this+0x3C = 3
}

// ------------------------------------------------ UpdateRunning @ 0x824C2970
void ReplayMain::UpdateRunning()
{
    ReplayInQueue* lpInQueue = reinterpret_cast<ReplayInQueue*>(mpInGuiEventQueue);

    // Drain the in-queue. On an id-carrying accept event (result 6, event id 50) while RUNNING,
    // send ADVANCE. If the state is no longer RUNNING, stop draining and skip the tail. When the
    // queue is empty (or the first event is null), fall through to the clock/abort/HUD tail.
    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    s32 liResult = lpInQueue->GetFirstEvent(&lpEvent, &liSize);

    bool lbFellThrough = true;
    if (lpEvent != 0)
    {
        while (true)
        {
            if (liResult == KI_EVENT_RESULT_WITH_ID)
            {
                if (meState == E_STATE_RUNNING && *reinterpret_cast<const s32*>(
                        reinterpret_cast<const u8*>(lpEvent) + 4) == KI_EVENT_ACCEPT)
                    SendStateEvent("ADVANCE");

                if (meState != E_STATE_RUNNING)
                {
                    lbFellThrough = false;   // X360 break -> skip the clock/abort/HUD tail
                    break;
                }
            }

            const CgsModule::Event* lpNextEvent = 0;
            liResult = lpInQueue->GetNextEvent(lpEvent, &lpNextEvent, &liSize);
            lpEvent  = lpNextEvent;
            if (lpEvent == 0)
                break;   // queue drained -> fall through to the tail
        }
    }

    if (!lbFellThrough)
        return;

    // Tail (also reached directly when the queue was empty): advance when the replay clock has
    // run out or the cache abort bit is set, otherwise tick the HUD banner if one is present.
    if (ReplayMainCacheBoundary::GetTimeRemaining(mpGuiCache) < 1.0f ||
        ReplayMainCacheBoundary::IsAborting(mpGuiCache))
    {
        SendStateEvent("ADVANCE");
    }
    else if (ReplayMainCacheBoundary::GetHudMessageFlag(mpGuiCache) != 0)
    {
        mHudMessage.Update();   // this+0x40
    }
}

// ------------------------------------------------ Update @ 0x824C2C88
void ReplayMain::Update()
{
    ReplayInQueue* lpInQueue = reinterpret_cast<ReplayInQueue*>(mpInGuiEventQueue);

    switch (meState)
    {
    case E_STATE_ENTER:
        meState = E_STATE_ENTER;      // re-arm (this+0x3C = 0)
        /* fallthrough */

    case E_STATE_READY:
        meState = E_STATE_READY;      // this+0x3C = 1
        /* fallthrough */

    case E_STATE_RUNNING:
        meState = E_STATE_RUNNING;    // this+0x3C = 2
        UpdateRunning();
        break;                        // -> Clear + return

    case E_STATE_DONE:
        break;                        // -> Clear + return

    default:
        // X360 builds "Invalid internal state : " + meState + "\n" into the message buffer; the
        // de-inlined StrStream/FireAssert machinery collapses to one CGS_ASSERT with the verbatim
        // rodata prefix (id + trailing \n dropped).
        CGS_ASSERT(false, "Invalid internal state : ");
        break;
    }

    lpInQueue->Clear();   // shared tail: VariableEventQueue<18432,16>::Clear(mpInGuiEventQueue)
}
}   // namespace BrnGui
