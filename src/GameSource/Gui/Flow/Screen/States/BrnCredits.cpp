#include "GameSource/Gui/Flow/Screen/States/BrnCredits.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                      // CgsGui::GuiAccessPointers
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface / GuiEventPlayMusicOnMenuStream
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // VariableEventQueue<18432,16> (the in-queue view)
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"              // CgsSound::Playback::Name::MakeHash

// BrnGui::Credits -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (3 ledger functions, DWARF primary file
// GameSource/Gui/Flow/Screen/States/BrnCredits.cpp):
//   Credits::OnEnter             @0x824CFA08
//   Credits::UpdateLoadResources @0x824CFBD8  (Credits::Update)
//   Credits::UpdateRunning       @0x824C1EE0  (Credits::Update)
//
// OnEnter (asm walk): register the two observed events (.data @0x82066648 == { 6, 21 },
// the two controller-action channels -- read from the decrypted XEX), latch the GuiCache
// through the asserting GetAccessPointers()/GetGuiCache() accessors (h:344 /
// CgsGuiShared.h:201), post the GuiEvent<587> screen-entered command record
// ({4, 587, 12}, channel 40, 16 bytes -- the same idiom as BrnPausedHudState's 532), and
// reset the internal state machine.
//
// UpdateLoadResources (asm walk): assert the cache (cpp:178), poll
// GuiCache::EnsureResourcesAreLoaded(maResourcesToLoad, 1) -- not ready -> false; ready ->
// play the credits apt movie (the inlined StateInterface::PlayAptMovie("Credits", 3) --
// the {8, 18, 12, name, level} record on channel 41) and start the credits music
// (OutputGuiEvent<GuiEventPlayMusicOnMenuStream>, hash = CgsSound::Playback::Name::
// MakeHash("Credits"), both flags clear), then true.
//
// UpdateRunning (asm walk): drain the state's in-queue (this+0x18, viewed as the 18432
// queue -- the BrnBootLegal idiom); a controller action (id 6) with sub-id 49 while
// RUNNING sends the "ADVANCE" state event.

namespace BrnGui
{
namespace
{
    // The state IN-queue is an 18KB variable event queue (X360 VariableEventQueue<18432,16>).
    typedef CgsModule::VariableEventQueue<18432, 16> CreditsInQueue;

    // The GuiEvent<587> "credits screen entered" command record (X360 {4, 587, 12, 0};
    // channel 40, 16 bytes). muHeader0 == 4 marks the trailing word as real payload and
    // the X360 explicitly zeroes it (stw of 0 @0x824CFAB4).
    struct GuiEventCreditsEnter : public CgsGui::GuiEvent<587>
    {
        u32 muPayload;   // +0x0C (the 4-byte payload; the X360 posts 0)

        GuiEventCreditsEnter() : CgsGui::GuiEvent<587>(4, 12), muPayload(0) {}
    };

    // The controller action sub-id that advances the credits (payload word +4 of action
    // event 6; the same sub-id BrnBootLegal names KI_ACTION_STOP).
    const s32 KI_ACTION_ADVANCE_CREDITS = 49;

    // The apt movie + music the running credits bind.
    const char* KPC_CREDITS_MOVIE_NAME = "Credits";
    const s32   KI_CREDITS_MOVIE_LEVEL = 3;
}

const s32 Credits::maiEventToObserve[] = { 6, 21 };
const s32 Credits::miNumEventsObserved = 2;

// @ 0x824CFA08
void Credits::OnEnter()
{
    mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

    // Both accessors carry the X360's inlined NULL asserts (h:344 / CgsGuiShared.h:201).
    mpGuiCache = mpStateInterface->GetAccessPointers()->GetGuiCache();

    GuiEventCreditsEnter lEnterEvent;
    mpStateInterface->GetOutputEventQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lEnterEvent), 40, 16);

    meInternalState = E_INTERNALSTATE_LOADRESOURCES;
}

// @ 0x824CFBD8
bool Credits::UpdateLoadResources()
{
    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");

    if (!mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
        return false;

    // Resources are in: bind the credits apt movie (the inlined PlayAptMovie -- the
    // {8, 18, 12, name, level} channel-41 record) and start the credits music stream.
    mpStateInterface->PlayAptMovie(KPC_CREDITS_MOVIE_NAME, KI_CREDITS_MOVIE_LEVEL);

    CgsGui::GuiEventPlayMusicOnMenuStream lMusicEvent(
        static_cast<u32>(CgsSound::Playback::Name::MakeHash(KPC_CREDITS_MOVIE_NAME)));
    mpStateInterface->OutputGuiEvent(lMusicEvent);

    return true;
}

// @ 0x824C1EE0
void Credits::UpdateRunning()
{
    CreditsInQueue* lpInQueue = reinterpret_cast<CreditsInQueue*>(mpInGuiEventQueue);

    const CgsModule::Event* lpEvent = NULL;
    s32 liSize = 0;
    s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent != NULL)
    {
        if (liEventId == 6 && meInternalState == E_INTERNALSTATE_RUNNING)
        {
            const s32 liAction =
                *reinterpret_cast<const s32*>(reinterpret_cast<const u8*>(lpEvent) + 4);
            if (liAction == KI_ACTION_ADVANCE_CREDITS)
                SendStateEvent("ADVANCE");
        }

        liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
    }
}
}
