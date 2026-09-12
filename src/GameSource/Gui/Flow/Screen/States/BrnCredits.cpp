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
// Bodied here (5 ledger functions, primary file
// GameSource/Gui/Flow/Screen/States/BrnCredits.cpp):
//   Credits::OnEnter             @0x824CFA08
//   Credits::OnLeave / Credits::Update
//   Credits::UpdateLoadResources @0x824CFBD8  (Credits::Update)
//   Credits::UpdateRunning       @0x824C1EE0  (Credits::Update)
//
// OnLeave / Update: see the per-function banners below.
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

    // The GuiEvent<588> "credits screen left" command record ({1, 588, 12}; channel 40,
    // 16 bytes) -- the 588 counterpart of OnEnter's 587. muHeader0 == 1 is the one-byte
    // sizeof of an EMPTY payload type (the GuiEventPlayAptLoadingMovie idiom), so the
    // trailing word is pure record padding the console never writes.
    struct GuiEventCreditsLeave : public CgsGui::GuiEvent<588>
    {
        u8 mucPad;   // +0x0C (the 1-byte empty payload; the console leaves it unwritten)

        GuiEventCreditsLeave() : CgsGui::GuiEvent<588>(1, 12), mucPad(0) {}
    };

    // The controller action sub-id that advances the credits (payload word +4 of action
    // event 6; the same sub-id BrnBootLegal names KI_ACTION_STOP).
    const s32 KI_ACTION_ADVANCE_CREDITS = 49;

    // The apt movie + music the running credits bind.
    const char* KPC_CREDITS_MOVIE_NAME = "Credits";
    const s32   KI_CREDITS_MOVIE_LEVEL = 3;

    // OnLeave re-issues both bindings with the EMPTY name at the same level: that is how
    // the screen unbinds the apt movie and silences the menu-music stream.
    const char* KPC_NO_MOVIE_NAME        = "";
    const char* KPC_NO_MUSIC_STREAM_NAME = "";
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

// ---- OnLeave ----------------------------------------------------------------------
// Tear the credits screen down, in the console's order: post the {1, 588, 12} "credits
// left" command record on channel 40, silence the menu-music stream (the same
// GuiEventPlayMusicOnMenuStream the running credits started, now carrying the interned
// EMPTY name -- the "no stream" hash), drop the two observed event channels, unbind the
// apt movie by re-issuing PlayAptMovie with the empty name at the same level, and latch
// LEFT so Update goes inert.
void Credits::OnLeave()
{
    GuiEventCreditsLeave lLeaveEvent;
    mpStateInterface->GetOutputEventQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lLeaveEvent), 40, 16);

    // The console reads a dyn-initialised global whose only writer hashes the empty
    // string; cached here at first use rather than re-hashed per leave.
    static const u32 KU_NO_MUSIC_STREAM_HASH =
        static_cast<u32>(CgsSound::Playback::Name::MakeHash(KPC_NO_MUSIC_STREAM_NAME));

    CgsGui::GuiEventPlayMusicOnMenuStream lMusicEvent(KU_NO_MUSIC_STREAM_HASH);
    mpStateInterface->OutputGuiEvent(lMusicEvent);

    mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

    mpStateInterface->PlayAptMovie(KPC_NO_MOVIE_NAME, KI_CREDITS_MOVIE_LEVEL);

    meInternalState = E_INTERNALSTATE_LEFT;
}

// ---- Update -----------------------------------------------------------------------
// The family's fall-through sub-state ladder: each rung re-stamps meInternalState and,
// when its rung completes, falls straight into the next one in the SAME frame. The
// in-event queue is cleared unconditionally at the tail, so an event no rung consumed is
// still dropped before the next frame.
void Credits::Update()
{
    switch (meInternalState)
    {
    case E_INTERNALSTATE_LOADRESOURCES:
        meInternalState = E_INTERNALSTATE_LOADRESOURCES;
        if (!UpdateLoadResources())
        {
            break;
        }
        // fall through

    case E_INTERNALSTATE_WFINIT:
        meInternalState = E_INTERNALSTATE_WFINIT;
        // fall through -- the wait-for-init rung completes in place for this screen: the
        // console emits no call and no test here, only the re-stamp, so WFINIT is a pure
        // pass-through frame on the way to RUNNING.

    case E_INTERNALSTATE_RUNNING:
        meInternalState = E_INTERNALSTATE_RUNNING;
        UpdateRunning();
        break;

    case E_INTERNALSTATE_LEFT:
        // Inert once OnLeave has latched LEFT (no re-stamp, no rung).
        break;

    default:
        // The original streams the fixed prefix, then meInternalState, then a newline into
        // the assert message buffer and fires unconditionally; the macro carries the fixed
        // half of that text.
        CGS_ASSERT(false, "Invalid internal state : ");
        break;
    }

    reinterpret_cast<CreditsInQueue*>(mpInGuiEventQueue)->Clear();
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
