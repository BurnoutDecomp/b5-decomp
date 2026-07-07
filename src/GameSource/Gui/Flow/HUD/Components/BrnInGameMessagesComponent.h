#pragma once

// BrnGui::InGameMessagesComponent -- owning header.
//   b5-decomp/src/GameSource/Gui/Flow/HUD/Components/BrnInGameMessagesComponent.h
//   class:BrnGui::InGameMessagesComponent
//
// The HUD component that drives the in-game message queue (the double-buffered
// TRANSIN/WAITING message slot machine). This is a MINIMAL-COMPLETE slice: it homes the
// seven X360-emitted accessor/mutator functions the InGameMessagesComponent TU owns
// (GetCurrentIndex/GetNextIndex/SwitchCurrentIndex/SetController/SetDirector/SetGameMode/
// SetInGameMessagesQueue), plus the shared InGameMessagesQueue view and the MessageState
// enum they read. Member list/order + offsets from the DecFIGS DWARF
// (BrnInGameMessagesComponent.h).
//
// FLAG (foreign types): the controller/director pointers reference BrnGui::HudMessageController
// (BrnGuiHudMessageDirector.h forward-decl / SharedClasses/DataLists/BrnHudMessageController.h)
// and BrnGui::HudMessageDirector (BrnGuiHudMessageDirector.h); pointer-only here, so
// forward-declared. The InGameMessagesQueue's full layout is un-homed; it is modelled as
// correctly-sized opaque storage so the two X360-pinned member offsets
// (maeMessageState @ +0x8B0 / muCurrentMessageIndex @ +0x8C0) are exact.

#include "types.hpp"
#include "BrnCommonTypes.h"                               // CgsID (u64)
#include "GameSource/GameState/BrnGameStateSharedIO.h"   // BrnGameState::GameStateModuleIO::EGameModeType
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponent.h" // BrnGui::BrnFlaptComponent (base) + BrnFlapt::MovieClipRef
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"   // BrnFlapt::TextFieldRef (maTextFields by value)
#include "SharedClasses/DataLists/BrnHudMessageController.h" // BrnResource::HudMessageEvent (maMessages by value)

namespace BrnFlapt { struct FileRef; }   // BrnFlaptFileRef.h (Prepare parameter, by const-ref)
namespace CgsModule { struct Event; }    // CgsVariableEventQueue.h (AddMessage parameter, pointer-only)

namespace BrnGui
{
    class HudMessageController;   // BrnGuiHudMessageDirector.h (pointer-only)
    struct HudMessageDirector;    // BrnGuiHudMessageDirector.h (pointer-only)

    // The per-slot message lifecycle state (DWARF BrnInGameMessagesComponent.h:54). The
    // double-buffered queue advances each slot NOMESSAGE -> WAITING -> TRANSIN -> VISIBLE
    // -> TRANSOUT as messages are queued, shown and dismissed.
    enum MessageState
    {
        E_MESSAGESTATE_NOMESSAGE = 0,
        E_MESSAGESTATE_WAITING   = 1,
        E_MESSAGESTATE_TRANSIN   = 2,
        E_MESSAGESTATE_VISIBLE   = 3,
        E_MESSAGESTATE_TRANSOUT  = 4,
    };

    // The shared in-game message queue the component drives (DWARF h:85). Two
    // double-buffered HudMessageEvent slots, each with its own lifecycle state, plus the
    // live end-time and slot selector. The X360 accessors form &maeMessageState[idx] as
    // (556 + idx) words and read/write maMessages[idx] at stride 0x458 (== the attested
    // sizeof(BrnResource::HudMessageEvent), CgsID-aligned):
    //   maMessages[]           @ +0x000 (0)     -- per-slot formatted message event
    //   maeMessageState[]      @ +0x8B0 (2224)  -- per-slot lifecycle state (word each)
    //   muCurrentEventEndTime  @ +0x8B8 (2232)  -- base-time tick the visible slot expires
    //   muCurrentMessageIndex  @ +0x8C0 (2240)  -- live slot selector (0 or 1)
    struct InGameMessagesQueue
    {
        BrnResource::HudMessageEvent maMessages[2];       // +0x000 (h:87)
        MessageState                 maeMessageState[2];  // +0x8B0 (h:88)
        u64                          muCurrentEventEndTime;// +0x8B8 (h:90)
        u8                           muCurrentMessageIndex;// +0x8C0 (h:91)
    };

    // BrnGui::InGameMessagesComponent (DWARF BrnInGameMessagesComponent.h:182). Derives the
    // apt-driven GUI component base (BaseInGameMessagesComponent : BrnFlaptComponent is an
    // empty intermediate in the DWARF; collapsed here as the base is transparent). Member
    // order + byte offsets from the DWARF, gated on the X360 asm.
    class InGameMessagesComponent : public BrnFlaptComponent
    {
    public:
        // @ 0x8241F190 -- h:197. Resolve the named component out of lFile, bind it into the
        // base mAptRef, reset its timeline and install the transition-complete callback.
        void Prepare(const char* lacName, const BrnFlapt::FileRef& lFile);

        // @ 0x8243DC68 -- h:203. Resolve lpEvent through the message controller into a
        // formatted HudMessageEvent, then dispatch it against the live slot: refresh the
        // same updatable id in place, keep the higher-priority pending message, or start it.
        void AddMessage(const CgsModule::Event* lpEvent);

        // @ 0x8243DDE0 -- h:207. Advance the live slot: start a WAITING message, or end a
        // VISIBLE one once its end-time is reached.
        void Update();

        // @ 0x82411058 -- h:211. Frame-trigger handler: promote TRANSIN->VISIBLE (latching
        // the end-time) or retire TRANSOUT->NOMESSAGE and flip the slot.
        void EndTransition();

        // @ 0x82472BE8 -- h:216. Latch the HUD message controller pointer.
        void SetController(const HudMessageController* lpController);
        // @ 0x82472C48 -- h:221. Latch the HUD message director pointer.
        void SetDirector(const HudMessageDirector* lpDirector);
        // @ 0x82472B80 -- h:244. Latch the current game-mode (range-guarded).
        void SetGameMode(BrnGameState::GameStateModuleIO::EGameModeType leCurrentGameMode);
        // @ 0x82475B80 -- h:239. Adopt the shared message queue and reconcile its live slot.
        void SetInGameMessagesQueue(InGameMessagesQueue* lInGameInMessagesQueue);

        // @ 0x824376F0 -- h:248. Retire any message the director no longer allows (clearing
        // its state, dismissing a visible one), then promote a pending next-slot message.
        void TerminateMessages();

    private:
        // @ 0x82437150 -- h:286. Begin transitioning the given slot's message IN (declared
        // here; bodied in a later slice -- it depends on the GUI audio-event output vein).
        void StartMessage(BrnResource::HudMessageEvent* lpEvent);
        // @ 0x824374B8 -- h:306. End the currently-visible message (declared; later slice).
        void EndMessage();

        // @ 0x8241F248 -- h:292. Rebuild the message banner clip/text/icon and publish the
        // named transition animation (declared here; bodied in a later slice -- it depends on
        // the un-homed MovieClipRef bind helper vein).
        void SendGameMessage(const char* lpcAnimName, bool lbNewIcon);

        // @ 0x82410F18 -- h:302. Fill the NEXT double-buffer slot with lpEvent, keeping the
        // higher-priority message when the slot is already occupied.
        void QueueMessage(BrnResource::HudMessageEvent* lpEvent);

        // @ 0x8240EA80 -- h:310. The queue's live message slot index.
        u8   GetCurrentIndex() const;
        // @ 0x8240EAE0 -- h:314. The other slot (double-buffered queue).
        u8   GetNextIndex() const;
        // @ 0x8240EB48 -- h:318. Toggle the double-buffer slot selector in place.
        void SwitchCurrentIndex();

        // @ 0x8241F530 -- h:323. Refresh the live slot's message in place (same id): copy in
        // the new event, and if it is no longer merely WAITING re-latch the end-time, mark it
        // VISIBLE and re-run the update animation.
        void UpdateInPlace(BrnResource::HudMessageEvent* lpEvent);

        // @ 0x824111B0 -- h:328. Is lMessageId one of the fixed set of HUD message ids that
        // may be refreshed in place (rather than re-started) when re-queued while showing?
        // The three ids are X360-attested 64-bit CgsID literals; touches no members.
        bool IsMessageUpdatable(CgsID lMessageId) const;

        // @ 0x82411360 -- h:337. Apt frame-trigger callback: forwards to EndTransition on
        // the InGameMessagesComponent handed through lpUserData. Static (C callback shape).
        static void TransitionCompleteCallback(void* lpUserData, u16 luArg);

        // ---- members (offsets from the DWARF + Construct/SendGameMessage asm) ----
        //   base BrnFlaptComponent           +0x000..+0x00B (mpStateInterface, mAptRef)
        InGameMessagesQueue* mpInGameMessagesQueue;              // +0x00C (h:258)
        BrnFlapt::MovieClipRef mAnimationRef;                    // +0x010 (h:266)
        BrnFlapt::MovieClipRef mIconRef;                         // +0x018 (h:267)
        BrnFlapt::TextFieldRef maTextFields[3];                  // +0x020 (h:270)
        s32  maiCurrentStringParamCount[3];                     // +0x044 (h:273)
        char maacCurrentStringStringId[3][64];                  // +0x050 (h:274)
        s32  maaeCurrentStringParamTypes[3][4];                 // +0x110 (h:275)
        char maaacCurrentStringParams[3][4][64];                // +0x140 (h:276)
        const HudMessageController* mpMessageController;         // +0x440 (h:278)
        const HudMessageDirector*   mpDirector;                 // +0x444 (h:279)
        BrnGameState::GameStateModuleIO::EGameModeType meCurrentGameMode; // +0x448 (h:281)
    };
}
