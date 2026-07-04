#include "GameSource/Gui/Flow/Screen/States/BrnReplayLoading.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                      // CgsGui::GuiAccessPointers
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface (PlayAptMovie / GetAccessPointers)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // VariableEventQueue<18432,16> (the in-queue view)
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache

// BrnGui::ReplayLoading -- reconstructed from BURNOUT_X360_ARTIST.XEX. A twin of the
// committed BrnGui::Credits / BrnGui::ScreenLoading loading-screen states.

namespace BrnGui
{
namespace
{
    // The state IN-queue is an 18KB variable event queue (X360 VariableEventQueue<18432,16>),
    // the same view Credits / BootLegal cast mpInGuiEventQueue to.
    typedef CgsModule::VariableEventQueue<18432, 16> ReplayInQueue;

    // The "ReplaysLoading" apt movie the loaded screen binds; level 3 (the loading-screen
    // idiom shared with Credits' "Credits" @ level 3).
    const char* KPC_REPLAYS_LOADING_MOVIE = "ReplaysLoading";
    const s32   KI_REPLAYS_LOADING_LEVEL  = 3;

    // The far GuiCache advance-block flag Update() polls: X360 ((*(mpGuiCache + 0x13BBC)
    // >> 4) & 1). The cache exposes no named accessor for this bit in the committed home,
    // so it is read here through a file-local boundary helper against the documented X360
    // byte offset (semantic parity; the flag word lives deep in the GuiCache). When the
    // GuiCache TU homes a named accessor for this bit, replace this with it.
    struct ReplayLoadingCacheBoundary
    {
        static const u32 KU_ADVANCE_BLOCK_FLAG_OFFSET = 0x13BBC;  // X360 GuiCache far member

        static bool IsAdvanceBlocked(const GuiCache* lpGuiCache)
        {
            const u8 lu8Flags = *(reinterpret_cast<const u8*>(lpGuiCache) +
                                  KU_ADVANCE_BLOCK_FLAG_OFFSET);
            return ((lu8Flags >> 4) & 1) != 0;
        }
    };
}

// ---- statics -----------------------------------------------------------------
// X360 .rdata @0x82066A20 (the screen's single static resource) + @0x82066A28 (count == 1).
// FLAG: the tuple's id/type words were not decoded in this packet (only the +0x82066A20
// base and the count of 1 are attested); the element is defined as a placeholder so the
// state links, and adopted with the real XEX-recovered { muId, meType } when decoded.
const CgsGui::sResourceTuple ReplayLoading::maResourcesToLoad[] =
{
    { 0u, static_cast<CgsGui::ResourceRequestTypes>(0) },   // @0x82066A20 (values not yet decoded)
};
const u32 ReplayLoading::muNumResourcesToLoad = 1;   // @0x82066A28

// ------------------------------------------------ GetResourcesToLoad @ 0x825009C0
void ReplayLoading::GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                       u32* lpuNumberOfResources) const
{
    *lppResourceTuples    = maResourcesToLoad;    // X360 &unk_82066A20
    *lpuNumberOfResources = muNumResourcesToLoad;  // X360 dword_82066A28 == 1
}

// ------------------------------------------------ OnEnter @ 0x824BA120
void ReplayLoading::OnEnter()
{
    // Latch the GuiCache. Both accessors carry the X360's inlined NULL asserts
    // (CgsGuiStateInterface.h:344 "mpAccessPointers != NULL" / CgsGuiShared.h:201 "mpGuiCache").
    mpGuiCache  = mpStateInterface->GetAccessPointers()->GetGuiCache();
    meLoadState = E_LOAD_WAIT_RESOURCES;   // this+0x3C = 0
}

// ------------------------------------------------ OnLeave @ 0x824D4978
void ReplayLoading::OnLeave()
{
    // Tear the screen down: bind the empty-name apt movie (the inlined PlayAptMovie -- the
    // { 8, 18, 12, name, level } channel-41 record, size 20). unk_820046A7 is the empty-name
    // sentinel; level 3.
    mpStateInterface->PlayAptMovie("", 3);
    meLoadState = E_LOAD_DONE;   // this+0x3C = 3
}

// ------------------------------------------------ UpdateLoadResources @ 0x824D49F8
bool ReplayLoading::UpdateLoadResources()
{
    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");

    // Stream the screen's single static resource; keep waiting until it is fully loaded.
    if (!mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
        return false;

    // Loaded: bind the "ReplaysLoading" apt movie (the inlined PlayAptMovie -- the
    // { 8, 18, 12, name, level } channel-41 record, size 20) and report ready.
    mpStateInterface->PlayAptMovie(KPC_REPLAYS_LOADING_MOVIE, KI_REPLAYS_LOADING_LEVEL);
    return true;
}

// ------------------------------------------------ Update @ 0x824DB5B0
void ReplayLoading::Update()
{
    ReplayInQueue* lpInQueue = reinterpret_cast<ReplayInQueue*>(mpInGuiEventQueue);

    switch (meLoadState)
    {
    case E_LOAD_WAIT_RESOURCES:
        meLoadState = E_LOAD_WAIT_RESOURCES;      // re-arm (this+0x3C = 0)
        if (!UpdateLoadResources())
            break;                                // still streaming -> Clear + return
        /* fallthrough: resources ready this frame */

    case E_LOAD_RESOURCES_READY:
        meLoadState = E_LOAD_RESOURCES_READY;     // this+0x3C = 1
        /* fallthrough */

    case E_LOAD_ADVANCE:
        meLoadState = E_LOAD_ADVANCE;             // this+0x3C = 2
        // Once the cache stops blocking, send ADVANCE (leaves the loading screen).
        // X360: ((*(mpGuiCache + 0x13BBC) >> 4) & 1) -- a far GuiCache advance-block flag.
        if (ReplayLoadingCacheBoundary::IsAdvanceBlocked(mpGuiCache))
            break;                                // blocked -> Clear + return
        SendStateEvent("ADVANCE");
        break;

    case E_LOAD_DONE:
        break;                                    // -> Clear + return

    default:
        // X360 builds "Invalid internal state : " + meLoadState + "\n" into the message buffer;
        // per project rule that de-inlined StrStream/FireAssert machinery collapses to one
        // CGS_ASSERT with the verbatim rodata prefix (id + trailing \n dropped).
        CGS_ASSERT(false, "Invalid internal state : ");
        break;
    }

    lpInQueue->Clear();   // shared tail: VariableEventQueue<18432,16>::Clear(mpInGuiEventQueue)
}
}
