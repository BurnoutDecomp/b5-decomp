#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"

// BrnGui::InGame - the front-end SCREEN flow's in-game root state. Owns the in-game
// main-menu entry points (pause / main map / event map / driver details), the online
// main-menu privilege + overlay handshakes, the trophy-car-unlock / completion sequence
// timers, and the EA-TRAX next-track debounce.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct @0x824B8D98   OnEnter @0x824D0498   OnLeave @0x824B8DE0
//   Update @0x824E0ED0      HandleControllerInput @0x824E0468
//   HandleOverlayComplete @0x824DA4C8
//   HandlePerformOnlineMainMenuOption @0x824DA350
//   HandleSplashScreenRequests @0x824D08C0
//   HandleInstantFreeburnSearchFail @0x824D0A38
//   ShutDownHudComponents @0x824D0AB0   SelectOnlineMenuOption @0x824D0B08
//   OpenMainMap @0x824DA610  OpenEventMap @0x824DA680  OpenDriverDetails @0x824DA700
//   PauseAllowed @0x824B8DF8  PauseGame @0x824DEFE8  CheckPrivileges @0x824B8EF8
//   GetResourcesToLoad = the ICF fold @0x825011B0 (vtable @0x82074154)
// Member set/order and the declaration shapes are the DecFIGS DWARF (BrnIngame.h:42);
// every member offset in a trailing comment is X360-attested.
//
// DWARF-only methods NOT reconstructed (absent from the X360 export/ledger - the ledger
// decides what exists): GotoPostEvent (h/cpp:1178), GotoCarLogBook (cpp:1196) - dead-
// stripped on X360 (private non-virtuals with no callers) - and HandleInGameFailedEvent
// (h:140), whose body the X360 inlined into Update's event-44 case.
// Handler-param base (pointer-only in this header; the .cpp includes the queue header).
namespace CgsModule { struct Event; }

namespace BrnGui
{
    class GuiCache;   // GameSource/Gui/BrnGuiCache.h (held by pointer only)
    class Profile;    // DWARF BrnIngame.h:78 (pointer-only member; the GUI profile object
                      // arrives by pointer on event 350 - its owning TU is unreconstructed)

    // The online main-menu option ids. DWARF home: the nested enum
    // BrnGui::GuiEventPerformOnlineMainMenuOption::EMainMenuOptions
    // (BrnGuiEventTypeDefs.h:5502, event base GuiEvent<282> on PS3 / wire id 284 on
    // X360). Hoisted to BrnGui scope here because that header's TU has not grown the
    // event type yet and InGame's member + handler shapes need the values; it migrates
    // into the event type when BrnGuiEventTypeDefs.h is next worked (same convention as
    // BrnGuiOptionsDataProfile.h's supporting types). Enumerator names/values verbatim
    // from the DWARF.
    enum EMainMenuOptions
    {
        E_MAIN_MENU_OPTIONS_FREEBURN_PLAY_NOW     = 0,    // -> "TO_FBURN_QK"
        E_MAIN_MENU_OPTIONS_FREEBURN_CUSTOM_MATCH = 1,    // -> "TO_FBURN_CU"
        E_MAIN_MENU_OPTIONS_FREEBURN_CREATE       = 2,    // -> "TO_FBURN_CR"
        E_MAIN_MENU_OPTIONS_IMAGE_GALLERY         = 3,    // -> "TO_IMG_GAL"
        E_MAIN_MENU_OPTIONS_VIEW_CHALLENGES       = 4,    // -> "TO_VIW_CHL"
        E_MAIN_MENU_OPTIONS_UNRANKED_PLAY_NOW     = 5,    // -> "TO_UNRANK_QK"
        E_MAIN_MENU_OPTIONS_UNRANKED_CUSTOM_MATCH = 6,    // -> "TO_UNRANK_CU"
        E_MAIN_MENU_OPTIONS_UNRANKED_CREATE       = 7,    // -> "TO_UNRANK_CR"
        E_MAIN_MENU_OPTIONS_RANKED_PLAY_NOW       = 8,    // -> "TO_RANKED_QK"
        E_MAIN_MENU_OPTIONS_RANKED_CUSTOM_MATCH   = 9,    // -> "TO_RANKED_CU"
        E_MAIN_MENU_OPTIONS_RANKED_CREATE         = 10,   // -> "TO_RANKED_CR"
        E_MAIN_MENU_OPTIONS_SCOREBOARDS           = 11,   // -> "TO_SCOREB"
        E_MAIN_MENU_OPTIONS_NEWS                  = 12,   // -> "TO_NEWS"
        E_MAIN_MENU_OPTIONS_COUNT                 = 13,   // OnEnter's "none selected" seed
    };

    struct InGame : public CgsGui::State
    {
        virtual void Construct(CgsID lId, CgsFsm::ScriptedFsm* lpFsm);
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ the ICF fold 0x825011B0 (== BrnGui::Video::GetResourcesToLoad; the InGame
        // vtable slot @0x82074174 carries it): the in-game state loads nothing through
        // the sResourceTuple path - it only zeroes the out count (the tuple pointer is
        // deliberately left untouched, matching the fold body).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const;

    private:
        // Per-frame in-queue handlers. The in-queue delivers the 12-byte-header-stripped
        // event PAYLOAD pointer (the committed consumer idiom - see BrnGuiOverlaysDirector
        // .cpp's payload views), so the handlers take the generic event pointer and view
        // the payload locally; the DWARF declares them with the typed event params
        // (GuiEventNetworkSplashEvent / GuiEventPerformOnlineMainMenuOption /
        // GuiOverlayCompleteEvent), noted per method.
        void HandleControllerInput(const CgsModule::Event* lpEvent);            // DWARF cpp:754
        void HandleSplashScreenRequests(const CgsModule::Event* lpNetworkSplashEvent);   // DWARF cpp:849 (const GuiEventNetworkSplashEvent*)
        void HandlePerformOnlineMainMenuOption(const CgsModule::Event* lpEvent);         // DWARF cpp:909 (const GuiEventPerformOnlineMainMenuOption*)
        void HandleInstantFreeburnSearchFail();                                 // DWARF cpp:962
        void HandleOverlayComplete(const CgsModule::Event* lpOverlayCompleteEvent);      // DWARF cpp:980 (const GuiOverlayCompleteEvent*)

        void PauseGame(bool lbUserInstigated, bool lbOpenDriverDetails);        // DWARF cpp:1036
        bool PauseAllowed();                                                    // DWARF cpp:1087
        void OpenMainMap();                                                     // DWARF cpp:1128
        void OpenEventMap();                                                    // DWARF cpp:1156
        void OpenDriverDetails();                                               // DWARF cpp:1214
        void ShutDownHudComponents();                                           // DWARF cpp:1232
        bool CheckPrivileges(EMainMenuOptions leMainMenuOption);                // DWARF cpp:1249
        void SelectOnlineMenuOption(EMainMenuOptions leMainMenuOption);         // DWARF cpp:1306

        // DWARF BrnIngame.h:75 - controller-disconnect frames swallowed before the
        // disconnect auto-pause fires.
        static const s32 KI_NUMBER_OF_DISCONNECTS_TO_IGNORE = 10;

        static const s32 maiEventToObserve[30];    // @ 0x82066680 (.rdata; DWARF cpp:31)
        static const s32 miNumEventsObserved;      // @ 0x820666F8 (== 30; DWARF cpp:83)

        // DWARF BrnInGame.cpp:66 - option -> screen-flow state-event string
        // (SelectOnlineMenuOption indexes it). @ 0x82F272A4 (.data).
        static const char* const KAPC_MAIN_MENU_STATE_ACTIONS_TEXT[13];

        GuiCache*        mpGuiCache;                      // DWARF h:77; X360 +0x38 (the 64-event fills it)
        Profile*         mpProfile;                       // DWARF h:78; X360 +0x3C (the 350-event fills it)
        EMainMenuOptions meSelectedOnlineMainMenuOption;  // DWARF h:82; X360 +0x40 (COUNT == none pending)
        bool             mbIsInEventStartLocation;        // DWARF h:84; X360 +0x44 (event-map gate, recomputed per frame)
        bool             mbIsGuideVisible;                // DWARF h:85; X360 +0x45 (event 516; forces the pause path)
        f32              mfTimeUntilTrophyCarUnlockSeq;   // DWARF h:86; X360 +0x48 (-> "TO_TRPHY_UNL")
        f32              mfTimeUntilCompletionSeq;        // DWARF h:87; X360 +0x4C (-> "TO_COMPLETED")
        f32              mfTimeUntilNextEATrack;          // DWARF h:88; X360 +0x50 (-> command 461)
        f32              mfTimeToDisableNextEATrack;      // DWARF h:89; X360 +0x54 (holds the next-track timer down)
        s32              miNumberOfIgnoredDisconnects;    // DWARF h:90; X360 +0x58 (event-9 counter)
    };
}
