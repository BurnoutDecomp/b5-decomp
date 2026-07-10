#include "GameSource/Gui/Flow/HUD/States/BrnBootLegal.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface (Register/OutputGuiEvent/PlayAptMovie/GetOutputEventQueue)
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>, CgsModule::Event
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::VariableEventQueue (the in-queue)
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsID, CgsIDCompress
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CgsDev::Assert (the X360 "unexpected event" assert)
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiOverlayRequest
#include "GameSource/Gui/BrnGuiVideoEvents.h"                             // BrnGui::GuiEventPlayVideo/StopVideo (508/509, the attract loop)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // CgsDev::Log (diag probe)
#include <cstdio>                                                         // std::snprintf (diag probe)
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"         // CgsResource::ID::HashString (the attract video resource id)

// BrnGui::BootLegal -- the boot legal / title-screen state (BF_LEGAL), reconstructed from
// BURNOUT_X360_ARTIST.XEX (OnEnter 0x82477028 / Update 0x82477270 / OnLeave 0x82477E28 /
// DisplaySelectionMenu 0x82475C20 / UpdateSelectionMenu 0x82473978). It builds the five apt
// animator components + the title-screen menu, drives a 0..10 stage machine that fades the HD
// composite / ESRB in, runs the attract loop, and on "press start" shows the title_screen02
// menu and posts the boot-flow command events.
//
// x64-native faithful reconstruction. The X360 reaches:
//   * the state IN-queue at `this+0x18`  -> the base CgsGui::State::mpInGuiEventQueue,
//     reinterpreted as CgsModule::VariableEventQueue<18432,16>* (same idiom as BrnBootVideos);
//   * the StateInterface OUT-queue at `mpStateInterface+0x0C` -> reached by name through
//     CgsGui::StateInterface::GetOutputEventQueue() (a VariableEventQueue<65536,16>), the same
//     pattern BrnPausedHudState uses to post its GuiEvent record.
// The inline "AddEvent(out, &record, channel, size)" sites are modelled as the GuiEvent<N>
// command records the X360 builds byte-for-byte (header { muHeader0, muEventType, muHeader2 }
// + the trailing payload words), pushed with their X360 channel id (40 = GuiEventOut,
// 41 = GuiOutViewState) and record size.
//
// Rodata dumped from the decrypted ARTIST XEX (file_off = 0x3000 + vaddr - 0x82000000, BE):
//   off_82F25CF8 menu-text table : { "$TITLESCREEN_MENU_NORMAL", "$TITLESCREEN_MENU_BEATTHETEAM" }  (ends @ off_82F25D00)
//   off_82F25D00 view-state table: { "Normal", "BeatTheTeam" }                                       (indexed by mu8SelectedMenuIndex)
//   off_82F25D08 attract table   : { "attract" }                                                     (the attract movie name)
//   off_82F27AE0[0]              : "Title_Screen02"  (the title-screen apt movie)
//   flt_8205AAF0 = 3.0 (wait-start dwell)   flt_8205AB7C = 25.0 (attract idle timeout)   flt_82001DA0 = 0.5 (accept dwell)
//   dword_8205AAD0 = { 6, 143, 510, 64, 567, 190, 189 } (the 7 observed event ids)

namespace BrnGui
{
    // The menu-facade highlighted-row accessor (BrnBootLegalBoundary.cpp; FLAG -- the
    // real SelectableGroup writes the byte back into this state directly).
    s32 BootLegalMenuFacade_GetHighlightedIndex();

namespace
{
    // ---- The state IN-queue is an 18KB variable event queue (X360 VariableEventQueue<18432,16>). ----
    typedef CgsModule::VariableEventQueue<18432, 16> BootInQueue;

    // ---- Observed GUI event ids (dword_8205AAD0, OnEnter registers all 7). ----
    const s32 KAI_OBSERVED_EVENTS[] = { 6, 143, 510, 64, 567, 190, 189 };
    const s32 KI_NUM_OBSERVED_EVENTS = 7;

    // ---- Update in-queue event ids (read off GetFirstEvent/GetNextEvent). ----
    const s32 KI_EVENT_ACTION_A        = 6;    // controller action (sub-id at payload+4)
    const s32 KI_EVENT_ACTION_B        = 21;   // controller action (alternate channel)
    const s32 KI_EVENT_CACHE_READY     = 64;   // the GuiCache is ready (payload = GuiCache*)
    const s32 KI_EVENT_START           = 143;  // "press start" feedback
    const s32 KI_EVENT_OVERLAY_RESULT  = 189;  // sign-out overlay result (id + button)
    const s32 KI_EVENT_OVERLAY_OPENED  = 190;  // sign-out overlay opened (id)
    const s32 KI_EVENT_VIDEO_FINISHED  = 510;  // attract video finished
    const s32 KI_EVENT_RESOURCE_READY  = 567;  // boot resources ready

    // ---- Controller action sub-ids (payload word +4 of action events 6/21). ----
    const s32 KI_ACTION_MENU_NEXT = 41;
    const s32 KI_ACTION_MENU_PREV = 42;
    const s32 KI_ACTION_BACK      = 45;  // -> request an attract restart / back-out
    const s32 KI_ACTION_STOP      = 49;  // -> stop the attract video

    // ---- Stage timing constants (rodata floats). ----
    const f32 KF_WAIT_START_DWELL    = 3.0f;   // flt_8205AAF0
    const f32 KF_ATTRACT_IDLE_TIMEOUT = 25.0f; // flt_8205AB7C
    const f32 KF_ACCEPT_DWELL        = 0.5f;   // flt_82001DA0

    // ---- Out-queue channel ids (the X360 `liType` passed to AddEvent on mpStateInterface+0xC). ----
    const s32 KI_CHANNEL_GUI_OUT    = 40;  // GuiEventOut
    const s32 KI_CHANNEL_VIEW_STATE = 41;  // GuiOutViewState

    // ---- The apt / view-state / movie strings (rodata). ----
    const char* const KAC_TITLE_MOVIE = "Title_Screen02";  // off_82F27AE0[0]

    // off_82F25CF8 .. off_82F25D00 : the two title-screen menu localisation keys.
    const char* const KAAC_MENU_TEXT[] =
    {
        "$TITLESCREEN_MENU_NORMAL",       // off 82F25CF8 -> 8205A660
        "$TITLESCREEN_MENU_BEATTHETEAM",  // off 82F25CFC -> 8205A640
    };
    const s32 KI_NUM_MENU_ENTRIES = 2;

    // off_82F25D00 : the per-entry apt view-state name (indexed by the chosen menu entry).
    const char* const KAAC_MENU_VIEW_STATE[] =
    {
        "Normal",       // off 82F25D00 -> 82051230
        "BeatTheTeam",  // off 82F25D04 -> 8205A634
    };

    // off_82F25D08 : the attract-movie name table (one entry in the ARTIST build).
    const char* const KAAC_ATTRACT_MOVIE[] =
    {
        "attract",  // off 82F25D08 -> 8205A62C
    };

    // ----------------------------------------------------------------------------------
    // The GuiEvent<N> command records the X360 posts onto the StateInterface out-queue.
    // Each reproduces the exact bytes the disasm stores into its stack record before the
    // AddEvent(out, &record, channel, size) call (header { muHeader0, muEventType, muHeader2 }
    // then the trailing payload words). Modelled like BrnPausedHudState::GuiEventPausedHudEnter.
    // ----------------------------------------------------------------------------------

    // 16-byte GuiEvent<N> command with header { 1, N, 12 } and one trailing flag byte. Used for
    // the channel-40 command events (resource-ready 589, load-machine 90/71/70, etc.) and the
    // channel-41 view-state command 214 (header { 1, 214, 12 }, flag 0).
    template <s32 N>
    struct GuiCommandEvent16 : public CgsGui::GuiEvent<N>
    {
        u8  mu8Flag;        // +0x0C (the X360 stores a single byte then leaves the record at 16)
        u8  maPad[3];
        GuiCommandEvent16(u8 lu8Flag = 0) : CgsGui::GuiEvent<N>(1, 12), mu8Flag(lu8Flag) { maPad[0] = maPad[1] = maPad[2] = 0; }
    };

    // 20-byte GuiEvent<25> "set option" command: header { 8, 25, 12 } then a 1.0f payload word
    // (the X360 OnEnter posts { 8, 25, 12, 0, 1.0f } on channel 41, size 20; OnLeave's first
    // post is the same header/shape but with miReserved=1 -- 0x82477E3C li r28,1 / stw r28,var_50).
    struct GuiOptionEvent20 : public CgsGui::GuiEvent<25>
    {
        s32 miReserved;     // +0x0C (0 in OnEnter, 1 in OnLeave's first post)
        f32 mfValue;        // +0x10 (1.0f)
        explicit GuiOptionEvent20(s32 liReserved = 0) : CgsGui::GuiEvent<25>(8, 12), miReserved(liReserved), mfValue(1.0f) {}
    };

    // 20-byte GuiEvent<18> "apt name + flag" command: header { 8, 18, 12 } then a (const char*,
    // s32) payload. The X360 OnLeave posts { 8, 18, 12, &unk_820046A7 ("" sentinel), 1 } on
    // channel 41, size 20 (0x82477EAC-0x82477EE8) -- NOT the GuiOptionEvent20/{reserved,float}
    // shape (event type 18, not 25).
    struct GuiAptNameFlagEvent20 : public CgsGui::GuiEvent<18>
    {
        const char* mpacAptName;   // +0x0C (&unk_820046A7, the empty-name sentinel)
        s32         miFlag;        // +0x10 (1)
        explicit GuiAptNameFlagEvent20(const char* lpacAptName, s32 liFlag)
            : CgsGui::GuiEvent<18>(8, 12), mpacAptName(lpacAptName), miFlag(liFlag) {}
    };

    // Post a 16-byte GuiEvent<N> { 1, N, 12 } command on the given channel.
    template <s32 N>
    void PostCommand16(CgsGui::StateInterface* lpInterface, s32 liChannel, u8 lu8Flag = 0)
    {
        GuiCommandEvent16<N> lEvent(lu8Flag);
        lpInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lEvent), liChannel, 16);
    }

    // ----------------------------------------------------------------------------------
    // FLAG: the higher-layer GUI event payloads the X360 emits through OutputGuiEvent<T>.
    // The full record bodies live in the movie-manager / sound / game-state headers; modelled
    // here as minimal GuiEvent-derived carriers so OutputGuiEvent<T> can size/queue them. The
    // queued bytes are the small leading header the boot path fills; the engine consumers fill
    // the rest. (Same modelling discipline as BrnPausedHudState's GuiEventPausedHudEnter.)
    // ----------------------------------------------------------------------------------

    // CgsGui::GuiEventPlayMusicOnMenuStream -- carries a sound-name hash + two flag bytes.
    struct MusicOnMenuStreamEvent : public CgsGui::GuiEvent<155>
    {
        s32 miHash;     // +0x0C (the MakeHash result, or dword_830082A8)
        u8  mu8FlagA;   // +0x10
        u8  mu8FlagB;   // +0x11
        u8  maPad[2];
        explicit MusicOnMenuStreamEvent(s32 liHash)
            : CgsGui::GuiEvent<155>(0, 12), miHash(liHash), mu8FlagA(0), mu8FlagB(0)
        { maPad[0] = maPad[1] = 0; }
    };

    // BrnGui::GuiMuteDac -- a one-byte mute command.
    struct MuteDacEvent : public CgsGui::GuiEvent<200>
    {
        u8 mu8Mute;     // +0x0C (0)
        u8 maPad[3];
        MuteDacEvent() : CgsGui::GuiEvent<200>(0, 12), mu8Mute(0) { maPad[0] = maPad[1] = maPad[2] = 0; }
    };

    // BrnGui::GuiEventPlayVideo / GuiEventStopVideo -- carry a 112-byte MovieManager
    // VideoDefinition the boot path Prepare()s on the stack; modelled by value here.
    struct PlayVideoEvent : public CgsGui::GuiEvent<508>
    {
        u8 maVideoDefinition[112];
        explicit PlayVideoEvent(const void* lpVideoDef) : CgsGui::GuiEvent<508>(0, 12)
        { for (s32 i = 0; i < 112; ++i) maVideoDefinition[i] = reinterpret_cast<const u8*>(lpVideoDef)[i]; }
    };
    struct StopVideoEvent : public CgsGui::GuiEvent<509>
    {
        u8 maVideoDefinition[112];
        explicit StopVideoEvent(const void* lpVideoDef) : CgsGui::GuiEvent<509>(0, 12)
        { for (s32 i = 0; i < 112; ++i) maVideoDefinition[i] = reinterpret_cast<const u8*>(lpVideoDef)[i]; }
    };

    // BrnGui::GuiEventSetPlayer0ControllerPort -- the saved controller port (invite reboot).
    struct SetPlayer0ControllerPortEvent : public CgsGui::GuiEvent<156>
    {
        s32 miPort;     // +0x0C
        explicit SetPlayer0ControllerPortEvent(s32 liPort) : CgsGui::GuiEvent<156>(0, 12), miPort(liPort) {}
    };

    // BrnGui::GuiAudioTriggerEvent -- a 112-byte audio-trigger record the X360 Construct()s
    // with (channel, apt-name, trigger-name). Modelled by value; Construct fills the body.
    struct GuiAudioTriggerEvent : public CgsGui::GuiEvent<201>
    {
        u8 maBody[112];
        GuiAudioTriggerEvent() : CgsGui::GuiEvent<201>(0, 12) { for (s32 i = 0; i < 112; ++i) maBody[i] = 0; }
        // FLAG (bring-up carrier layout): the X360 fills the 112-byte audio record with the channel +
        // apt/trigger NAME HASHES for the AEMS sound logic; that exact record layout is the deferred
        // audio-engine boundary. Until it is recovered, carry { s32 channel @+0, trigger name @+4,
        // apt name @+68 } so the PC channel-201 consumer can key the trigger (it currently logs the
        // trigger; the AEMS patch playback is the follow-on).
        void Construct(s32 liChannel, const char* lpacAptName, const char* lpacTrigger)
        {
            *reinterpret_cast<s32*>(&maBody[0]) = liChannel;
            const char* lpacT = (lpacTrigger != 0) ? lpacTrigger : "";
            const char* lpacA = (lpacAptName != 0) ? lpacAptName : "";
            s32 li = 0;
            for (; li < 63 && lpacT[li] != 0; ++li) maBody[4 + li] = static_cast<u8>(lpacT[li]);
            maBody[4 + li] = 0;
            for (li = 0; li < 43 && lpacA[li] != 0; ++li) maBody[68 + li] = static_cast<u8>(lpacA[li]);
            maBody[68 + li] = 0;
        }
    };
}   // anonymous namespace
}   // namespace BrnGui

// ==================================================================================
// FLAG -- out-of-scope engine boundaries. These are real X360 callees the boot-legal
// flow drives; their bodies live in subsystems not reconstructed here (the GUI cache
// component/apt machinery, the movie manager, the sound name hasher, the soft-reboot
// data on BrnGameModule and the DLC manager). Declared so this state compiles and links
// against them; do NOT body the engines from here.
// ==================================================================================
// External-linkage FLAG declarations (their addresses are taken / they are odr-used, so
// they must be declared with external linkage; the definitions link from their owning TUs).
namespace BootLegalFlag
{
    // --- BrnGameModule soft-reboot state (off_830102D0 is the module singleton). ---
    // The X360 passes the module pointer; modelled here as opaque-pointer queries.
    extern void* gpBrnGameModule;                                       // off_830102D0
    bool  HasGameBeenSoftRebooted(void* lpModule);                       // 0x823C0278  // FLAG
    bool  HasGameBeenRebootedDueToInvite(void* lpModule);               // 0x823C0288  // FLAG
    const s32* GetSoftRebootData(void* lpModule);                        // GetSoftRebootData      // FLAG

    // --- The movie-manager video definition the attract path prepares/plays. ---
    void  MovieVideoDefinition_Prepare(void* lpDefinition);             // BrnGui::MovieManager::VideoDefinition::Prepare  // FLAG

    // --- The sound-playback name hasher (returns the music/sound name hash). ---
    s32   SoundPlaybackNameMakeHash(const char* lpacName);             // CgsSound::Playback::Name::MakeHash  // FLAG

    // --- The DLC "beat the team" game enable flag (byte_82FFA7F0). ---
    extern u8 gu8BeatTheTeamDlcEnabled;                                 // byte_82FFA7F0  // FLAG
    void  DLCBeatTheTeamGame_SetEnabledState(u8* lpFlag, s32 liState); // BrnResource::DLCBeatTheTeamGame::SetEnabledState  // FLAG
}
using namespace BootLegalFlag;

// --- GuiCache helpers the X360 reaches that are not on the committed BrnGui::GuiCache API. ---
// FLAG: the apt-component watcher half of the cache (ClearExpectedAptComponentList /
// AppendExpectedAptComponent (sub_824F87C0) / AreAllAptComponentsInitialised) and the
// resource (un)load entry the X360 uses (UnloadResources). Declared as free helpers taking
// the cache pointer so this state stays off raw cache offsets; bodies link from the GuiCache TU.
namespace BrnGui
{
namespace BootLegalCacheBoundary  // FLAG boundary helpers
{
    void ClearExpectedAptComponentList(GuiCache* lpCache, s32 liFlow);                    // FLAG
    void AppendExpectedAptComponent(GuiCache* lpCache, s32 liFlow, CgsGui::GuiComponent* lpComponent); // sub_824F87C0  // FLAG
    bool AreAllAptComponentsInitialised(const GuiCache* lpCache, s32 liFlow);             // FLAG
    bool EnsureResourcesAreLoaded(GuiCache* lpCache);                                     // FLAG
    void EnsureBootResourceIsLoaded(GuiCache* lpCache);                                   // FLAG (v43={127,4})
    void UnloadResources(GuiCache* lpCache, const CgsGui::sResourceTuple* lpResources, u32 luCount); // FLAG
    f32  GetTime(const GuiCache* lpCache);                                                // FLAG

    // X360 *(cache+19273) (byte): non-zero suppresses the HD-composite "transin" call in
    // E_STAGE_FADE_IN (0x824778D8). FLAG: no named accessor on the committed GuiCache API.
    bool IsHDCompAlreadyTransitioned(const GuiCache* lpCache);                            // FLAG
    // X360 *(cache+42996) (word): non-zero -> "visible", zero -> "invisible" for the ESRB
    // panel in E_STAGE_FADE_IN (0x8247790C). FLAG: no named accessor on the committed GuiCache API.
    bool IsEsrbVisible(const GuiCache* lpCache);                                          // FLAG
    // X360 *(cache+77578) / *(cache+77579) (bytes): either non-zero forces the
    // "start message" transin + advance-to-menu path in E_STAGE_PRESTART_OR_IDLE, the same
    // as mbWaitForStartPressed (0x82477A1C / 0x82477A2C). FLAG: no named accessor on the
    // committed GuiCache API.
    bool IsStartMessageForced(const GuiCache* lpCache);                                   // FLAG
}
}

// ==================================================================================

namespace BrnGui
{
    // FLAG: the legal-screen static resource list (.rdata @ off_82F25CEC, X360 count @ 0x82F25CF4 = 1).
    // The tuple's .rdata values are not extracted; defined empty (count 0) so GetResourcesToLoad /
    // UnloadResources iterate nothing -- the legal-screen resource preload is deferred, the flow still
    // reaches BootLegal + the title_screen02 request.
    const CgsGui::sResourceTuple BootLegal::maResourcesToLoad[1] = {};
    const u32                    BootLegal::muNumResourcesToLoad  = 0u;

    BootLegal::BootLegal()
        : mpGuiCache(0)
        , muCurrentAttractMovieIdx(0)
        , mbWaitForStartPressed(true)
        , meUpdateStage(E_STAGE_WAIT_CACHE)
        , mfStageStartTime(0.0f)
        , mbSignOutPending(false)
        , mfAcceptTime(0.0f)
        , mu8SelectedMenuIndex(0)
    {
    }

    // ------------------------------------------------------------------ OnEnter @ 0x82477028
    void BootLegal::OnEnter()
    {
        // Reset the cache + stage bookkeeping (the X360 zeroes this+0x48 and this+0x38).
        meUpdateStage = E_STAGE_WAIT_CACHE;
        mpGuiCache    = 0;

        // Observe the 7 boot GUI events.
        mpStateInterface->RegisterForEvents(KAI_OBSERVED_EVENTS, KI_NUM_OBSERVED_EVENTS);

        // Post the three boot-setup command records onto the out-queue (mpStateInterface+0xC):
        //   ch 41 (view-state) GuiEvent<214> { 1, 214, 12 } flag 0     (16 bytes)
        //   ch 40 (gui-out)    GuiEvent<20>  { 1, 20, 12 }            (16 bytes)
        //   ch 41 (view-state) GuiEvent<25>  { 8, 25, 12, 0, 1.0f }   (20 bytes)
        PostCommand16<214>(mpStateInterface, KI_CHANNEL_VIEW_STATE, 0);
        PostCommand16<20>(mpStateInterface, KI_CHANNEL_GUI_OUT);
        {
            GuiOptionEvent20 lOption;
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lOption), KI_CHANNEL_VIEW_STATE, 20);
        }

        // Build the five apt animator components + the title-screen menu (the X360 calls each
        // component's vtable Construct; here we call CgsGui::GuiComponent::Construct by name).
        mHDCompAnimator.Construct("HDCompAnimator_mc", mpStateInterface, 0);
        mEsrbAnimator.Construct("esrb_anim", mpStateInterface, 0);
        mStartMessageAnimator.Construct("StartMessageAnimatorComponent", mpStateInterface, 0);
        mBackgroundAnimator.Construct("BackgroundAnimatorComponent", mpStateInterface, 0);
        mSelectionMenuAnimator.Construct("SelectionMenuAnimatorComponent", mpStateInterface, 0);
        mSelectionMenu.Construct("MenuItem", mpStateInterface, KI_NUM_MENU_ENTRIES, 0, -1);

        // Initial state: idx 0, no sign-out, wait-for-start armed, stage timer 0.
        muCurrentAttractMovieIdx = 0;
        mbSignOutPending         = false;
        mbWaitForStartPressed    = true;
        mfStageStartTime         = 0.0f;

        // A soft reboot that is NOT an invite reboot pops the "you were signed out" overlay and
        // posts a sign-out command (GuiEvent<88> { 1, 88, 12 } flag 1, ch 40).
        if (HasGameBeenSoftRebooted(gpBrnGameModule) &&
            !HasGameBeenRebootedDueToInvite(gpBrnGameModule))
        {
            GuiOverlayRequest lOverlay;
            lOverlay.Construct("SignOut");
            // The X360 emits this via the specialised CgsGui::StateInterface::OutputGuiEvent<
            // BrnGui::GuiOverlayRequest> (0x82436BE0), which pushes the overlay record onto the
            // out-queue. GuiOverlayRequest is not a GuiEvent<N> (it has no GetEventType), so post
            // it directly onto the queue -- the faithful effect of that specialisation. The
            // overlay's GUI event type id is the channel; the record is the full request struct.
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lOverlay),
                /*overlay event channel*/ KI_CHANNEL_GUI_OUT, static_cast<s32>(sizeof(GuiOverlayRequest)));
            PostCommand16<88>(mpStateInterface, KI_CHANNEL_GUI_OUT, 1);
        }
    }

    // ------------------------------------------------------------------ OnLeave @ 0x82477E28
    void BootLegal::OnLeave()
    {
        // Post the leave view-state command (GuiEvent<25> { 8, 25, 12, 1, 1.0f }, ch 41, size 20 --
        // 0x82477E3C li r28,1 feeds the reserved dword, unlike OnEnter's {..,0,1.0f} post),
        // tear down the menu component (menu vtable +0x18 == slot 6 == MenuComponent::Clear), then
        // unregister the observed events.
        {
            GuiOptionEvent20 lOption(1);
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lOption), KI_CHANNEL_VIEW_STATE, 20);
        }
        mSelectionMenu.Clear();
        mpStateInterface->UnRegisterForEvents(KAI_OBSERVED_EVENTS, KI_NUM_OBSERVED_EVENTS);

        // Post the second leave command (GuiEvent<18> { 8, 18, 12, "", 1 }, ch 41 -- the
        // X360 packs the empty apt-name pointer + a 1 flag at size 20 with 4-byte
        // pointers; on the PC/x64 gate the name pointer widens, so the copied payload is
        // sizeof(GuiAptNameFlagEvent20) -- the standard x64 record-width rule (a 20-byte
        // copy truncates the pointer and drops the level word, which the view-module
        // dispatch then reads as garbage).
        {
            GuiAptNameFlagEvent20 lOption("", 1);
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lOption), KI_CHANNEL_VIEW_STATE,
                static_cast<s32>(sizeof(GuiAptNameFlagEvent20)));
        }

        // Unload the legal screen's resources from the cache and clear its expected apt list.
        if (mpGuiCache != 0)
            BootLegalCacheBoundary::UnloadResources(mpGuiCache, maResourcesToLoad, muNumResourcesToLoad);
        if (mpGuiCache != 0)
            BootLegalCacheBoundary::ClearExpectedAptComponentList(mpGuiCache, 1);
    }

    // ------------------------------------------------ DisplaySelectionMenu @ 0x82475C20
    void BootLegal::DisplaySelectionMenu()
    {
        // Lay out the 2-entry menu and set each row's localisation-key text.
        mSelectionMenu.SetupMenu(KI_NUM_MENU_ENTRIES, true);
        for (s32 liIndex = 0; liIndex < KI_NUM_MENU_ENTRIES; ++liIndex)
            mSelectionMenu.SetText(liIndex, KAAC_MENU_TEXT[liIndex]);
        mSelectionMenu.Update();                        // (*(*menu + 0x14))(menu) == slot 5

        // FLAG: the apt-view boundary. AddOutputAptViewState hands the transition to the apt
        // engine (a CgsGui::GuiComponent boundary); do NOT body the apt engine here.
        mStartMessageAnimator.AddOutputAptViewState("apt_Transition", "transout", false);  // this+0x170
        mSelectionMenuAnimator.AddOutputAptViewState("apt_Transition", "transin", false);  // this+0x288
    }

    // ------------------------------------------------ UpdateSelectionMenu @ 0x82473978
    void BootLegal::UpdateSelectionMenu(s32 liAction)
    {
        // Action 41 = move down (menu vtable +0x38 == HighlightNext), 42 = move up
        // (+0x34 == HighlightPrevious).
        if (liAction == KI_ACTION_MENU_NEXT)
            mSelectionMenu.HighlightNext();
        else if (liAction == KI_ACTION_MENU_PREV)
            mSelectionMenu.HighlightPrevious();

        // Read the highlighted row straight off the menu's SelectableGroup (the X360 reads its
        // cached +0x3BD copy of this).
        mu8SelectedMenuIndex = static_cast<u8>(mSelectionMenu.miHighlightedIndex);

        mSelectionMenu.Update();                        // (*(*menu + 0x14))(menu) == slot 5

        // Re-add the background's apt view state for the now-selected entry ("Normal"/"BeatTheTeam").
        // FLAG: the apt-view boundary (see DisplaySelectionMenu).
        mBackgroundAnimator.AddOutputAptViewState(           // this+0x1FC
            "apt_Transition", KAAC_MENU_VIEW_STATE[mu8SelectedMenuIndex], false);
    }

    // ------------------------------------------------------------------ Update @ 0x82477270
    void BootLegal::Update()
    {
        BootInQueue* lpInQueue = reinterpret_cast<BootInQueue*>(mpInGuiEventQueue);
        if (lpInQueue == 0)
            return;

        // Bring-up diagnostic: one line per stage-machine transition (cheap, load-bearing
        // for boot-flow triage -- the attract/press-start regressions were found with it).
        {
            static s32 s_iLastLoggedStage = -1;
            if (s_iLastLoggedStage != static_cast<s32>(meUpdateStage))
            {
                char lacStage[80];
                std::snprintf(lacStage, sizeof(lacStage), "[BootLegal] stage %d -> %d\n",
                              s_iLastLoggedStage, static_cast<s32>(meUpdateStage));
                CgsDev::Log::WriteToLog(lacStage);
                s_iLastLoggedStage = static_cast<s32>(meUpdateStage);
            }
        }

        bool lbVideoFinished = false;   // v2:  a 510 (video-finished) event arrived this frame
        bool lbBackRequested = false;   // v39: a 45/49 (back/stop) action arrived this frame
        bool lbCacheLoaded   = false;   // v3:  the cache reported its resources loaded this frame
        bool lbStartPressed  = false;   // v38: a 143/45 (start) event arrived this frame

        // ---- Drain the in-queue, accumulating the per-frame flags. ----
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
        while (liEventId >= 0 && lpEvent != 0)
        {
            const u8* lpPayload = reinterpret_cast<const u8*>(lpEvent);
            const s32 liSubId   = *reinterpret_cast<const s32*>(lpPayload + 4);

            switch (liEventId)
            {
            case KI_EVENT_ACTION_A:
            case KI_EVENT_ACTION_B:
                if (liSubId == KI_ACTION_BACK || liSubId == KI_ACTION_STOP)
                {
                    lbBackRequested = true;
                }
                else if ((liSubId == KI_ACTION_MENU_NEXT || liSubId == KI_ACTION_MENU_PREV) &&
                         meUpdateStage == E_STAGE_MENU_ACTIVE)
                {
                    UpdateSelectionMenu(liSubId);
                }
                if (liSubId == KI_ACTION_BACK)
                    lbStartPressed = true;   // X360: a 45 also falls through to the "start" path
                break;

            case KI_EVENT_CACHE_READY:
                if (meUpdateStage == E_STAGE_WAIT_CACHE)
                {
                    GuiCache* lpCache = *reinterpret_cast<GuiCache* const*>(lpPayload);
                    mpGuiCache = lpCache;
                    if (BootLegalCacheBoundary::EnsureResourcesAreLoaded(lpCache))
                    {
                        BootLegalCacheBoundary::EnsureBootResourceIsLoaded(mpGuiCache);  // v43={127,4}
                        lbCacheLoaded = true;
                    }
                }
                break;

            case KI_EVENT_START:
                lbStartPressed = true;
                break;

            case KI_EVENT_VIDEO_FINISHED:
                lbVideoFinished = true;
                break;

            case KI_EVENT_RESOURCE_READY:
                mbWaitForStartPressed = true;   // *(this+68) = 1
                break;

            case KI_EVENT_OVERLAY_OPENED:
                // 190: a sign-out overlay opened -- if it is the SignOut overlay, flag it.
                if (CgsIDCompress("SignOut") == *reinterpret_cast<const u64*>(lpPayload))
                    mbSignOutPending = true;
                break;

            case KI_EVENT_OVERLAY_RESULT:
                // 189: the SignOut overlay closed -- clear the flag and reset the stage timer.
                // X360: ld r11,0(r29); cmpld r3,r11 -- compares the 8-byte hash at payload+0
                // (same offset as the 190/OVERLAY_OPENED case), not payload+4.
                if (CgsIDCompress("SignOut") == *reinterpret_cast<const u64*>(lpPayload) &&
                    *reinterpret_cast<const s32*>(lpPayload + 8) == 1)
                {
                    mbSignOutPending = false;
                    mfStageStartTime = BootLegalCacheBoundary::GetTime(mpGuiCache);
                }
                break;

            default:
                // The X360 fires an assert on any unexpected event (verbatim file/line).
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(
                    "Unexpected event in IntroHudState::Update",
                    "..\\..\\..\\GameSource\\Gui/Flow/HUD/States/BrnBootLegal.cpp",
                    293);
                CgsDev::Assert::EndAssert();
                break;
            }

            const CgsModule::Event* lpNextEvent = 0;
            liEventId = lpInQueue->GetNextEvent(lpEvent, &lpNextEvent, &liSize);
            lpEvent   = lpNextEvent;
        }

        // ---- The stage machine (X360 switch on this+0x48). ----
        switch (meUpdateStage)
        {
        case E_STAGE_WAIT_CACHE:
            if (lbCacheLoaded)
            {
                meUpdateStage = E_STAGE_START_MOVIE;
                lpInQueue->Clear();
                return;
            }
            break;

        case E_STAGE_START_MOVIE:
            // Register the 5 apt components as "expected" with the cache, play the title-screen
            // apt movie + the menu-stream music, and (on a non-invite soft reboot) mute the DAC.
            BootLegalCacheBoundary::ClearExpectedAptComponentList(mpGuiCache, 1);
            BootLegalCacheBoundary::AppendExpectedAptComponent(mpGuiCache, 1, &mHDCompAnimator);
            BootLegalCacheBoundary::AppendExpectedAptComponent(mpGuiCache, 1, &mEsrbAnimator);
            BootLegalCacheBoundary::AppendExpectedAptComponent(mpGuiCache, 1, &mStartMessageAnimator);
            BootLegalCacheBoundary::AppendExpectedAptComponent(mpGuiCache, 1, &mBackgroundAnimator);
            BootLegalCacheBoundary::AppendExpectedAptComponent(mpGuiCache, 1, &mSelectionMenuAnimator);

            PostCommand16<589>(mpStateInterface, KI_CHANNEL_GUI_OUT);
            mpStateInterface->PlayAptMovie(KAC_TITLE_MOVIE, 1);

            {
                // GuiEventPlayMusicOnMenuStream { hash = MakeHash("GunsAndRoses") } (ch via OutputGuiEvent).
                s32 liHash = SoundPlaybackNameMakeHash("GunsAndRoses");
                MusicOnMenuStreamEvent lMusic(liHash);
                mpStateInterface->OutputGuiEvent<MusicOnMenuStreamEvent>(lMusic);
            }

            if (HasGameBeenSoftRebooted(gpBrnGameModule) &&
                !HasGameBeenRebootedDueToInvite(gpBrnGameModule))
            {
                MuteDacEvent lMute;
                mpStateInterface->OutputGuiEvent<MuteDacEvent>(lMute);
            }

            meUpdateStage = E_STAGE_FADE_IN;
            break;

        case E_STAGE_FADE_IN:
            // Wait until every expected apt component has initialised, then fade the HD composite
            // in (unless already faded) and show/hide the ESRB panel.
            if (!BootLegalCacheBoundary::AreAllAptComponentsInitialised(mpGuiCache, 1))
                break;

            // FLAG: the two AddOutputAptViewState calls below are the apt-view boundary.
            // *(cache+19273)/*(cache+42996) are far cache flags the X360 reads to choose the
            // transition (0x824778D8 / 0x8247790C) -- gated via the cache boundary helpers.
            if (!BootLegalCacheBoundary::IsHDCompAlreadyTransitioned(mpGuiCache))
                mHDCompAnimator.AddOutputAptViewState("apt_Transition", "transin", false);  // this+0x58
            mEsrbAnimator.AddOutputAptViewState("apt_Transition",
                BootLegalCacheBoundary::IsEsrbVisible(mpGuiCache) ? "visible" : "invisible", false); // this+0xE4

            mfStageStartTime = BootLegalCacheBoundary::GetTime(mpGuiCache);
            meUpdateStage    = E_STAGE_WAIT_START;
            break;

        case E_STAGE_WAIT_START:
            // Hold for the wait-start dwell unless a sign-out is up, then post the
            // GuiEvent<90> command and advance.
            if (mbSignOutPending ||
                BootLegalCacheBoundary::GetTime(mpGuiCache) <= (mfStageStartTime + KF_WAIT_START_DWELL))
                break;
            PostCommand16<90>(mpStateInterface, KI_CHANNEL_GUI_OUT);
            meUpdateStage = E_STAGE_PRESTART_OR_IDLE;
            break;

        case E_STAGE_PRESTART_OR_IDLE:
            // If start was already pressed -- or the cache forces it via *(cache+77578)/
            // *(cache+77579) (0x82477A1C / 0x82477A2C) -- transin the start message and jump
            // to the menu path; otherwise idle until the attract timeout, then play the attract loop.
            if (mbWaitForStartPressed || BootLegalCacheBoundary::IsStartMessageForced(mpGuiCache))
            {
                mStartMessageAnimator.AddOutputAptViewState("apt_Transition", "transin", false); // this+0x170  // FLAG apt-view
                meUpdateStage = E_STAGE_START_PRESSED;
                break;
            }
            if (BootLegalCacheBoundary::GetTime(mpGuiCache) > (mfStageStartTime + KF_ATTRACT_IDLE_TIMEOUT))
            {
                // -> begin the attract loop (LABEL_64): play the empty-name apt + reset the idx.
                mpStateInterface->PlayAptMovie("", 1);
                muCurrentAttractMovieIdx = 0;
                meUpdateStage = E_STAGE_ATTRACT_PREPARE;
            }
            break;

        case E_STAGE_ATTRACT_PREPARE:
            // Prepare + play the attract video, then wait for it to finish.
            {
                MusicOnMenuStreamEvent lMusic(0);   // X360 uses dword_830082A8 (the boot music id)
                mpStateInterface->OutputGuiEvent<MusicOnMenuStreamEvent>(lMusic);

                // The X360 Prepare()s a MovieManager VideoDefinition and fills its video
                // resource id (CgsResource::ID::HashString(attractName)) + sound name
                // (MakeHash) from the attract table entry off_82F25D08[idx], then posts the
                // def as GuiEventPlayVideo. Post the typed BrnGui::GuiEventPlayVideo with
                // that id -- the reconstructed MovieManager's 508 record (BootVideos' proven
                // path); the def rect/crossfade defaults match, the sound-stream name is the
                // same follow-on as BootVideos. (The prior raw 112-byte def was never filled
                // -- a garbage id that hung the manager, killing press-start after the 25s
                // attract timeout.)
                BrnGui::GuiEventPlayVideo lPlay;
                lPlay.muVideoResourceId = static_cast<u32>(CgsResource::ID::HashString(
                    reinterpret_cast<const u8*>(KAAC_ATTRACT_MOVIE[muCurrentAttractMovieIdx])));
                mpStateInterface->OutputGuiEvent<BrnGui::GuiEventPlayVideo>(lPlay);
            }
            meUpdateStage = E_STAGE_ATTRACT_PLAYING;
            break;

        case E_STAGE_ATTRACT_PLAYING:
            // On finish, advance the attract index (and loop). On a back/stop, stop the video.
            if (lbVideoFinished)
            {
                ++muCurrentAttractMovieIdx;
                meUpdateStage = (muCurrentAttractMovieIdx != 0) ? E_STAGE_START_MOVIE
                                                                : E_STAGE_ATTRACT_PREPARE;
            }
            if (lbBackRequested)
            {
                // X360: Prepare a def + post the stop record. The reconstructed MovieManager's
                // 509 record is the typed BrnGui::GuiEventStopVideo (immediate-stop flag clear,
                // the def-stop default).
                BrnGui::GuiEventStopVideo lStop;
                mpStateInterface->OutputGuiEvent<BrnGui::GuiEventStopVideo>(lStop);
                lpInQueue->Clear();
                return;
            }
            break;

        case E_STAGE_START_PRESSED:
            // Start pressed: open the selection menu (and, if the BeatTheTeam DLC is enabled,
            // build it immediately) -- otherwise fall through to the attract / accept logic.
            if (lbStartPressed)
            {
                meUpdateStage = E_STAGE_MENU_ACTIVE;
                if (gu8BeatTheTeamDlcEnabled)
                {
                    DisplaySelectionMenu();
                    lpInQueue->Clear();
                    return;
                }
            }
            else if (mbWaitForStartPressed &&
                     BootLegalCacheBoundary::GetTime(mpGuiCache) > (mfStageStartTime + KF_ATTRACT_IDLE_TIMEOUT))
            {
                mpStateInterface->PlayAptMovie("", 1);
                muCurrentAttractMovieIdx = 0;
                meUpdateStage = E_STAGE_ATTRACT_PREPARE;
                break;
            }
            // An invite reboot routes the saved player-0 controller port back out, then clears.
            if (HasGameBeenRebootedDueToInvite(gpBrnGameModule))
            {
                const s32* lpSoftRebootData = GetSoftRebootData(gpBrnGameModule);
                SetPlayer0ControllerPortEvent lPort(*lpSoftRebootData);
                mpStateInterface->OutputGuiEvent<SetPlayer0ControllerPortEvent>(lPort);
                lpInQueue->Clear();
                return;
            }
            // fall through to the shared accept handler.
            /* fallthrough */

        case E_STAGE_MENU_ACTIVE:
            // The accept handler (LABEL_85): on a back/accept action, post the accept commands,
            // play the accept sound, latch the chosen DLC mode, and advance to the dwell stage.
            if (lbBackRequested)
            {
                PostCommand16<590>(mpStateInterface, KI_CHANNEL_GUI_OUT);
                mfAcceptTime = BootLegalCacheBoundary::GetTime(mpGuiCache);

                {
                    GuiAudioTriggerEvent lAudio;
                    lAudio.Construct(7, "", "Accept");
                    mpStateInterface->OutputGuiEvent<GuiAudioTriggerEvent>(lAudio);
                }

                if (gu8BeatTheTeamDlcEnabled && mu8SelectedMenuIndex == 1)
                    DLCBeatTheTeamGame_SetEnabledState(&gu8BeatTheTeamDlcEnabled, 1);

                meUpdateStage = E_STAGE_ACCEPT_DWELL;
                PostCommand16<71>(mpStateInterface, KI_CHANNEL_GUI_OUT);
                lpInQueue->Clear();
                return;
            }
            lpInQueue->Clear();
            return;

        case E_STAGE_ACCEPT_DWELL:
            // After the accept dwell, post the final load command and advance to ACCEPTED.
            if ((BootLegalCacheBoundary::GetTime(mpGuiCache) - mfAcceptTime) <= KF_ACCEPT_DWELL)
                break;
            PostCommand16<70>(mpStateInterface, KI_CHANNEL_GUI_OUT);
            meUpdateStage = E_STAGE_ACCEPTED;
            break;

        default:
            break;
        }

        lpInQueue->Clear();
    }
}
