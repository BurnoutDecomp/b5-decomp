#include "GameSource/Gui/Flow/HUD/States/BrnPausedHudState.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // CgsGui::StateInterface, GetOutputEventQueue
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                      // CgsGui::GuiEvent, GuiEventWrapper
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the state in-queue
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT

// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX.
//
//   OnEnter @0x8247CBE8 -- RegisterForEvents(maiEventToObserve, 3), then post a GuiEvent<532>
//                          { muHeader0 = 1, muEventType = 532, muHeader2 = 12 } onto the state's
//                          large output queue with channel id 40 (GuiEventOut), record size 16.
//                          The X360 reaches the queue as `mpStateInterface + 0xC` (the
//                          StateInterface's mOutEventQueue); we reach it by name through
//                          GetOutputEventQueue(). The trailing word of the 16-byte record is left
//                          uninitialised by the X360 (it builds only the 12-byte event header).
//   OnLeave @0x82475390 -- UnRegisterForEvents(maiEventToObserve, 3) (tail call).

namespace BrnGui
{

// The GUI event PausedHudState publishes when it is entered. The X360 fills a GuiEvent<532>
// header { muHeader0 = 1, muEventType = 532, muHeader2 = 12 } and pushes a 16-byte record
// (channel id 40 = GuiEventOut); the trailing payload word is left uninitialised. Type id 532
// is the X360 GuiEvent<N> template id; the payload semantics beyond the header are not recovered.
struct GuiEventPausedHudEnter : public CgsGui::GuiEvent<532>
{
    u32 muReserved;   // +0x0C (X360 leaves this gap word uninitialised; record size is 16)

    GuiEventPausedHudEnter() : CgsGui::GuiEvent<532>(1, 12) {}
};

// The 3 observed event ids (dword_8205B060). The FLAG that stood here -- "the exports carry no
// values; 0 is a never-posted placeholder id until the table is recovered" -- is RETIRED, and it
// never needed .rdata to retire it: Update @0x8247CC58 dispatches on exactly these three ids and
// asserts "Unexpected event" on anything else (0x0E / 0x94 / 0x179 at 0x8247CCF8-0x8247CD08), so
// the table is pinned by the consumer that reads it. Same technique BootAttract already used to
// pin its single id from its own Update.
//   14  -- tolerated, no-op arm (no assert, no action)
//  148  -- the pause/unpause toggle from InGame::OpenMainMap / InGame::OnEnter
//  377  -- GuiPlayerCrashingStateChangeEvent (live since the GUI-377 producer landed)
const s32 PausedHudState::maiEventToObserve[3] = { 14, 148, 377 };
const s32 PausedHudState::miNumEventsObserved = 3;

// @ 0x8247CBE8
void PausedHudState::OnEnter()
{
    mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

    GuiEventPausedHudEnter lEvent;
    mpStateInterface->GetOutputEventQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lEvent), 40, 16);
}

// @ 0x82475390
void PausedHudState::OnLeave()
{
    mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
}

// =================================================================================================
//  @0x8247CC58  BrnGui::PausedHudState::Update
//
//  Drain mpInGuiEventQueue (VariableEventQueue<18432,16>), dispatch, then Clear() it -- the same
//  spine BootAttract::Update uses, with three arms instead of one:
//     id  14 -> tolerated, no-op (falls straight through to the next event; NO assert)
//     id 148 -> `lbz r11, 0(r19)` -- ONE BYTE of the payload. If non-zero: SendStateEvent("UNPAUSE")
//               and post the 40-byte record decoded below.
//     id 377 -> `lwz r11, 0(r19)` -- a WORD. 0|2 -> "START_CRASH" ; 1|3 -> "END_CRASH" ;
//               anything else -> assert "Invalid state in event : " <n>   (:128)
//     default -> assert "Unexpected event in IntroHudState::Update"       (:137)
//  ⚠️ That default string really does say IntroHudState -- it is the console's own copy-paste
//  and it is kept VERBATIM. Correcting it would silently break string-identity with the image.
//
//  ⭐⭐ THE 40-BYTE RECORD IS A GuiEventWrapper, NOT A GuiEvent<456>, AND THAT RESOLVES BOTH OF
//  ITS "UNINITIALISED" WORDS. Read store-for-store from 0x8247CE10-0x8247CE64, the record base is
//  r1+0xA0 and the words land as {24, 456, 16, ?, 2, 1, -1, ?, 0, 0}. Modelled as a GuiEvent<456>
//  with "header2 = 16" that looks like two dropped fields. Modelled as this tree's already-committed
//  CgsGui::GuiEventWrapper<T,40> -- {miOutEventSize, miOutEventType, miOutEventOffset, T} -- it is
//  exact and self-consistent:
//      miOutEventSize   = 24   (r17 = 0x18)  == sizeof(T)
//      miOutEventType   = 456  (r18 = 0x1C8) == T::GetEventType()
//      miOutEventOffset = 16   (r16 = 0x10)  == offsetof(mOutEvent)
//  and an offset of 16 rather than the usual 12 is produced by an 8-BYTE-ALIGNED payload -- which
//  T is, because it ends with the 64-bit zero the console writes as a single `std r28, var_D0`.
//  So the word at record +0x0C is the ALIGNMENT PAD the u64 forces, not a field the console forgot
//  to fill, and the word at +0x1C is T's own interior padding. Nothing is dropped and nothing is
//  invented. AddEvent(queue, rec, channel 40, size 40) closes: 16 + 24 == 40.
//  (The consumer of event 456 is not identified; posting it is what the console does.)
// =================================================================================================

namespace
{
    typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

    // The 24-byte, 8-byte-aligned payload boxed by the UNPAUSE arm. Three words are attested
    // (2, 1, -1) and the trailing 64-bit zero is attested as one `std`; muPad is the alignment
    // padding described above. The console leaves that pad word unwritten; it is zeroed here
    // because writing a deterministic record beats copying an uninitialised stack word (and
    // reading one would be UB on the host). Deviation stated, not hidden.
    struct GuiPausedHudUnpausePayload
    {
        s32 mi0;        // +0x00  == 2   (r8)
        s32 mi1;        // +0x04  == 1   (r14)
        s32 mi2;        // +0x08  == -1  (r15)
        u32 muPad;      // +0x0C  alignment pad forced by the u64 below (X360 leaves it unwritten)
        u64 mu64Zero;   // +0x10  == 0   (`std r28`) -- and what makes alignof(T) == 8

        GuiPausedHudUnpausePayload() : mi0(2), mi1(1), mi2(-1), muPad(0), mu64Zero(0) {}
        s32 GetEventType() const { return 456; }
    };
}

// @ 0x8247CC58
void PausedHudState::Update()
{
    StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
    if (lpInQueue == 0)
        return;

    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
         lpEvent != 0;
         liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
    {
        const s32* lpiPayload = reinterpret_cast<const s32*>(lpEvent);

        if (liEventId == 14)
        {
            // 0x8247CCF8 `cmpwi r3, 0xE ; beq` straight to the loop tail. Tolerated, no action.
            continue;
        }

        if (liEventId == 148)
        {
            // 0x8247CDF8 `lbz r11, 0(r19)` -- ONE BYTE, not a word.
            if (*reinterpret_cast<const u8*>(lpEvent) == 0)
                continue;

            SendStateEvent("UNPAUSE");

            GuiPausedHudUnpausePayload lPayload;
            CgsGui::GuiEventWrapper<GuiPausedHudUnpausePayload, 40> lRecord(lPayload);
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lRecord), 40,
                static_cast<s32>(sizeof(lRecord)));

            if (CgsDev::Log::gpDebugPrint != 0)
            {
                // [hud-pause] witness. NOT X360. The FBurn HUD stage sequence reappearing after
                // the unpause is the real proof; this line says which arm produced it.
                static bool sbLoggedUnpause = false;
                if (!sbLoggedUnpause)
                {
                    sbLoggedUnpause = true;
                    *CgsDev::Log::gpDebugPrint
                        << "[hud-pause] PausedHudState: GUI 148 payload non-zero -> "
                           "SendStateEvent(\"UNPAUSE\") + GuiEventWrapper<456> (40 bytes)\n";
                }
            }
            continue;
        }

        if (liEventId == 377)
        {
            // 0x8247CD2C `lwz r11, 0(r19)` + a 4-case jump table.
            switch (lpiPayload[0])
            {
            case 0:
            case 2:
                SendStateEvent("START_CRASH");
                break;
            case 1:
            case 3:
                SendStateEvent("END_CRASH");
                break;
            default:
                // :128 -- the console streams the offending value after the text.
                CGS_ASSERT(false, "Invalid state in event : ");
                break;
            }
            continue;
        }

        // :137 -- kept VERBATIM, IntroHudState and all (the console's own copy-paste).
        CGS_ASSERT(false, "Unexpected event in IntroHudState::Update");
    }

    lpInQueue->Clear();
}

} // namespace BrnGui
