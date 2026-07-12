#include "GameSource/Gui/Flow/Screen/States/BrnReplayOutro.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                      // CgsGui::GuiAccessPointers
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>, CgsModule::Event
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface (Register/PlayAptMovie/GetAccessPointers/GetOutputEventQueue)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // VariableEventQueue<18432,16> (the in-queue view)
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache

// BrnGui::ReplayOutro -- the "replay outro" GUI state, reconstructed from
// BURNOUT_X360_ARTIST.XEX. A full outro FSM twin of the committed BrnGui::ReplayLoading:
// it observes one GUI event, streams the outro's static resource list, binds the
// "ReplaysOutro" apt movie, then drains the in-queue every frame and, on the accept
// event (21), routes the flow to credits or advance.
//   OnEnter 0x824BA2E8 / OnLeave 0x824D4F00 / Update 0x824DB868 /
//   UpdateLoadResources 0x824D5038 / UpdateRunning 0x824D5118

namespace BrnGui
{
namespace
{
    // The state IN-queue is an 18KB variable event queue (X360 VariableEventQueue<18432,16>),
    // the same view Credits / BootLegal / ReplayLoading cast mpInGuiEventQueue to.
    typedef CgsModule::VariableEventQueue<18432, 16> ReplayInQueue;

    // The "ReplaysOutro" apt movie the loaded outro binds; level 4 (its own layer, above the
    // loading-screen level 3).
    const char* const KAC_REPLAYS_OUTRO_MOVIE = "ReplaysOutro";
    const s32         KI_REPLAYS_OUTRO_LEVEL  = 4;

    // Out-queue channel id (the X360 `liType` passed to AddEvent on mpStateInterface+0xC).
    const s32 KI_CHANNEL_GUI_OUT = 40;   // GuiEventOut

    // The in-queue controller-accept event id (X360 0x15).
    const s32 KI_EVENT_ACCEPT = 21;

    // 16-byte GuiEvent<N> command with header { 1, N, 12 } and one trailing flag byte
    // (mirrors BrnBootLegal.cpp's GuiCommandEvent16; that one is file-local in BootLegal's
    // own anon namespace, so it is re-declared here rather than shared).
    template <s32 N>
    struct GuiCommandEvent16 : public CgsGui::GuiEvent<N>
    {
        u8  mu8Flag;        // +0x0C
        u8  maPad[3];
        GuiCommandEvent16(u8 lu8Flag = 0)
            : CgsGui::GuiEvent<N>(1, 12), mu8Flag(lu8Flag) { maPad[0] = maPad[1] = maPad[2] = 0; }
    };

    // Post a 16-byte GuiEvent<N> { 1, N, 12 } command on the given channel.
    template <s32 N>
    void PostCommand16(CgsGui::StateInterface* lpInterface, s32 liChannel, u8 lu8Flag = 0)
    {
        GuiCommandEvent16<N> lEvent(lu8Flag);
        lpInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lEvent), liChannel, 16);
    }

    // 20-byte GuiEvent<25> "set option" command: header { 8, 25, 12 } then a reserved word
    // and a 1.0f payload word (the X360 UpdateRunning posts { 8, 25, 12, 0, 1.0f } on channel
    // 40, size 20). Same shape as BrnBootLegal.cpp's GuiOptionEvent20 (file-local in that TU's
    // anon namespace), re-declared here.
    struct GuiOptionEvent20 : public CgsGui::GuiEvent<25>
    {
        s32 miReserved;     // +0x0C (0)
        f32 mfValue;        // +0x10 (1.0f)
        GuiOptionEvent20() : CgsGui::GuiEvent<25>(8, 12), miReserved(0), mfValue(1.0f) {}
    };

    // FLAG boundary helper: the two far GuiCache flags the outro reads. No named accessor on
    // the committed BrnGui::GuiCache API, so they are read here through a file-local boundary
    // against the documented X360 byte offsets (mirrors ReplayLoadingCacheBoundary). Replace
    // with named accessors when the GuiCache TU homes them.
    struct ReplayOutroCacheBoundary
    {
        // X360 *(mpGuiCache + 0x141D5) (byte): non-zero -> the loading movie is still active.
        static const u32 KU_LOADING_MOVIE_ACTIVE_OFFSET = 0x141D5;
        // X360 *(mpGuiCache + 0x141DE) (byte): non-zero -> the replay ends into the credits.
        static const u32 KU_CREDITS_FOLLOW_ON_OFFSET     = 0x141DE;

        static bool IsLoadingMovieActive(const GuiCache* lpGuiCache)
        {
            return *(reinterpret_cast<const u8*>(lpGuiCache) +
                     KU_LOADING_MOVIE_ACTIVE_OFFSET) == 1;
        }
        static bool IsCreditsFollowOn(const GuiCache* lpGuiCache)
        {
            return *(reinterpret_cast<const u8*>(lpGuiCache) +
                     KU_CREDITS_FOLLOW_ON_OFFSET) != 0;
        }
    };

    // Boundary onto the cache's resource watcher: the X360 OnLeave calls
    // UnloadAllResources(mpGuiCache, 5) on the StateLoadingHelper sub-object. The
    // named GuiCache accessor (UnloadAllResources @0x824FEBB0, the watcher
    // tail-branch) is homed in BrnGuiCache.cpp now, so the boundary is a plain
    // forward. (Defined in-TU: an anonymous-namespace function can only ever
    // resolve from this TU.)
    void ReplayOutroUnloadAllResources(GuiCache* lpCache, CgsGui::ResourceRequestTypes leType)
    {
        lpCache->UnloadAllResources(leType);
    }
}   // anonymous namespace

// ---- statics -----------------------------------------------------------------
// X360 .rdata: the observed outro GUI event id array (&unk_82066A58, count 1) and the outro's
// static resource list (&unk_82066A60, count 1). FLAG: the id/tuple VALUES were not decoded in
// this packet (only the base addresses + the count of 1 are attested); placeholders so the
// state links, adopted with the XEX-recovered ids when decoded.
const s32                    ReplayOutro::KAI_OBSERVED_EVENTS[]  = { 0 };   // @0x82066A58
const s32                    ReplayOutro::KI_NUM_OBSERVED_EVENTS = 1;
const CgsGui::sResourceTuple ReplayOutro::maResourcesToLoad[]    =
{
    { 0u, static_cast<CgsGui::ResourceRequestTypes>(0) },   // @0x82066A60 (values not yet decoded)
};
const u32                    ReplayOutro::muNumResourcesToLoad   = 1;       // @0x82066A60 count

// ------------------------------------------------ OnEnter @ 0x824BA2E8
void ReplayOutro::OnEnter()
{
    // Observe the single outro GUI event (X360 &unk_82066A58, count 1).
    mpStateInterface->RegisterForEvents(KAI_OBSERVED_EVENTS, KI_NUM_OBSERVED_EVENTS);

    // Latch the GuiCache. Both accessors carry the X360's inlined NULL asserts
    // (CgsGuiStateInterface.h:344 "mpAccessPointers != NULL" / CgsGuiShared.h:201 "mpGuiCache").
    mpGuiCache = mpStateInterface->GetAccessPointers()->GetGuiCache();
    meState    = E_STATE_LOAD_RESOURCES;   // this+0x3C = 0
}

// ------------------------------------------------ OnLeave @ 0x824D4F00
void ReplayOutro::OnLeave()
{
    mpStateInterface->UnRegisterForEvents(KAI_OBSERVED_EVENTS, KI_NUM_OBSERVED_EVENTS);

    // If the cache still holds the outro loading movie (X360 far flag *(mpGuiCache+0x141D5)==1),
    // tear it down first: bind the empty-name apt movie at level 3.
    if (ReplayOutroCacheBoundary::IsLoadingMovieActive(mpGuiCache))
        mpStateInterface->PlayAptMovie("", 3);   // { 8, 18, 12, "", 3 } ch 41, size 20

    // Bind the empty-name apt movie at level 4 (the outro's own layer).
    mpStateInterface->PlayAptMovie("", 4);       // { 8, 18, 12, "", 4 } ch 41, size 20

    // Unload the outro's loading-screen apt resources and mark the state done.
    ReplayOutroUnloadAllResources(mpGuiCache, CgsGui::E_GUI_RESOURCETYPE_APT_LOADING_SCREEN);   // type 5
    meState = E_STATE_DONE;   // this+0x3C = 3

    // Post the two teardown command records onto the out-queue (channel 40):
    //   GuiEvent<528> { 1, 528, 12 }                (16 bytes)
    //   GuiEvent<215> { 1, 215, 12 } + flag byte 0  (16 bytes)
    PostCommand16<528>(mpStateInterface, KI_CHANNEL_GUI_OUT);
    PostCommand16<215>(mpStateInterface, KI_CHANNEL_GUI_OUT, 0);
}

// ------------------------------------------------ UpdateLoadResources @ 0x824D5038
bool ReplayOutro::UpdateLoadResources()
{
    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");

    // Stream the outro's static resource list; keep waiting until it is fully loaded.
    if (!mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
        return false;

    // Loaded: bind the "ReplaysOutro" apt movie at level 4 (the inlined PlayAptMovie --
    // { 8, 18, 12, name, level } channel-41 record, size 20) and report ready.
    mpStateInterface->PlayAptMovie(KAC_REPLAYS_OUTRO_MOVIE, KI_REPLAYS_OUTRO_LEVEL);
    return true;
}

// ------------------------------------------------ UpdateRunning @ 0x824D5118
void ReplayOutro::UpdateRunning()
{
    ReplayInQueue* lpInQueue = reinterpret_cast<ReplayInQueue*>(mpInGuiEventQueue);

    // Drain the in-queue; on the controller-accept event (id 21) route the outro to
    // credits or advance, then post the { 8, 25, 12, 0, 1.0f } option command (ch 40).
    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent != 0)
    {
        if (liEventId == KI_EVENT_ACCEPT)   // 21
        {
            // The cache far flag *(mpGuiCache+0x141DE): non-zero -> the replay ends into the
            // credits sequence; zero -> a plain advance out of the outro.
            const char* lpacEvent = ReplayOutroCacheBoundary::IsCreditsFollowOn(mpGuiCache)
                                        ? "TO_CREDITS" : "ADVANCE";
            SendStateEvent(lpacEvent);

            GuiOptionEvent20 lOption;   // GuiEvent<25> { 8, 25, 12, 0, 1.0f }
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lOption), KI_CHANNEL_GUI_OUT, 20);
        }

        const CgsModule::Event* lpNextEvent = 0;
        liEventId = lpInQueue->GetNextEvent(lpEvent, &lpNextEvent, &liSize);
        lpEvent   = lpNextEvent;
    }
}

// ------------------------------------------------ Update @ 0x824DB868
void ReplayOutro::Update()
{
    ReplayInQueue* lpInQueue = reinterpret_cast<ReplayInQueue*>(mpInGuiEventQueue);

    switch (meState)
    {
    case E_STATE_LOAD_RESOURCES:
        meState = E_STATE_LOAD_RESOURCES;         // re-arm (this+0x3C = 0)
        if (!UpdateLoadResources())
            break;                                // still streaming -> Clear + return
        /* fallthrough: resources ready this frame */

    case E_STATE_RESOURCES_READY:
        meState = E_STATE_RESOURCES_READY;        // this+0x3C = 1
        /* fallthrough */

    case E_STATE_RUNNING:
        meState = E_STATE_RUNNING;                // this+0x3C = 2
        UpdateRunning();
        break;                                    // -> Clear + return

    case E_STATE_DONE:
        break;                                    // -> Clear + return

    default:
        // X360 builds "Invalid internal state : " + meState + "\n" into the message buffer;
        // the de-inlined StrStream/FireAssert machinery collapses to one CGS_ASSERT with the
        // verbatim rodata prefix (id + trailing \n dropped).
        CGS_ASSERT(false, "Invalid internal state : ");
        break;
    }

    lpInQueue->Clear();   // shared tail: VariableEventQueue<18432,16>::Clear(mpInGuiEventQueue)
}
}   // namespace BrnGui
