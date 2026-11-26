#include "GameSource/Gui/Flow/Screen/States/BrnReplayLoading.h"
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/Replays/BrnReplayLoading.cpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                      // CgsGui::GuiAccessPointers
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface (PlayAptMovie / GetAccessPointers)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // VariableEventQueue<18432,16> (the in-queue view)
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiShared.h"                                  // gGuiResourceIdentifier

// BrnGui::ReplayLoading -- reconstructed from BURNOUT_X360_ARTIST.XEX. A twin of the
// committed BrnGui::Credits / BrnGui::ScreenLoading loading-screen states.

namespace BrnGui
{
namespace
{
    // The state IN-queue is an 18KB variable event queue (X360 VariableEventQueue<18432,16>),
    // the same view Credits / BootLegal cast mpInGuiEventQueue to.
    typedef CgsModule::VariableEventQueue<18432, 16> ReplayInQueue;

    // gGuiResourceIdentifier[161] == "ReplaysLoading" (table @0x82F278E0, ARTIST loads
    // entry +0x284 @0x824D4A74), played on level 3.
    const u32 KU_REPLAYS_LOADING_RESOURCE = 161;
    const s32 KI_REPLAYS_LOADING_LEVEL    = 3;

}

// ---- statics -----------------------------------------------------------------
// X360 .rdata @0x82066A20 { 0xA1, 4 } (the screen's single static resource) + @0x82066A28
// (count == 1).
const CgsGui::sResourceTuple ReplayLoading::maResourcesToLoad[] =
{
    { KU_REPLAYS_LOADING_RESOURCE, CgsGui::E_GUI_RESOURCETYPE_APT },   // @0x82066A20
};
const u32 ReplayLoading::muNumResourcesToLoad = 1;   // @0x82066A28

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
    CGS_ASSERT_AT(mpGuiCache != NULL, "mpGuiCache", CGS_ARTIST_FILE, 173);

    // Stream the screen's single static resource; keep waiting until it is fully loaded.
    // The loaded arm returns inside the if (ARTIST branches to a shared `return false` tail).
    if (mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad) == true)   // cmplwi 1 @0x824D4A50
    {
        // Loaded: bind the "ReplaysLoading" apt movie (the inlined PlayAptMovie -- the
        // { 8, 18, 12, name, level } channel-41 record, size 20) and report ready.
        mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KU_REPLAYS_LOADING_RESOURCE], KI_REPLAYS_LOADING_LEVEL);
        return true;
    }
    return false;
}

// ------------------------------------------------ Update @ 0x824DB5B0
void ReplayLoading::Update()
{


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
        if (((mpGuiCache->GetReplayStatusInterface().GetStatusFlags() >> 4) & 1u) != 0)
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

    mpInGuiEventQueue->Clear();   // shared tail: VariableEventQueue<18432,16>::Clear(mpInGuiEventQueue)
}
}
