// ===================================================================================
// wave-J partfile 02 -- BrnGui::PreRaceFlyByState::OnEnter @0x824C6498 (cpp:187)
//                       BrnGui::PreRaceFlyByState::OnLeave @0x824C68F0 (cpp:565)
//
// Both bodies come from the raw X360 asm (OnEnter: the disassembly at the bottom of
// scratchpad/waveJ/prfb_xrefs.txt; OnLeave: scratchpad/waveJ/asm_onleave.txt), arbitrated
// over Hex-Rays throughout.
//
// The two records this partfile puts on the wire, and how each is published:
//   * id 532, 16 bytes on channel 40 (OnEnter). A BAKED-HEADER post: the file-local type
//     below derives CgsGui::GuiEvent<532> and goes straight onto the out-queue, because
//     the committed OutputGuiEvent<T> direct-passes the event and prepends nothing (see
//     the FLAG in CgsGuiStateInterface.h).
//   * id 213, 24 bytes on channels 41 and 42 (OnLeave). A WRAPPED post: the payload type
//     BrnGui::GuiEventShowHideSatNav is deliberately the RAW 12-byte payload with no
//     GuiEvent<213> base, so it must travel through StateInterface::OutputViewState /
//     OutputInternalState, which build the GuiEventWrapper<T,41|42> header the console
//     stack-builds here (BrnGuiDemangledEventTypes.h:566 names this very call site).
//
// 2026-08-03 RECONCILIATION: this partfile was parked while GuiCache's
// UnloadResource/GetMapIconManager/RefreshMapState/SetPreRaceFlyByActive,
// MapIconManager's SetIconsVisible/SetOwnerParameters/ReleaseResources + flag members,
// GuiEventDrawEventIcons::EIconDisplayType and GuiEventShowHideSatNav's real shape were
// all missing. Every one of them has since landed, so both bodies are here. The OnLeave
// sat-nav post was rebuilt onto OutputViewState/OutputInternalState in the same pass --
// posting the now-raw 12-byte struct directly would have shipped a headerless record.
//
// The class header GameSource/Gui/Flow/PreEvent/States/BrnPreRaceFlyBy.h needs no change:
// every static table, every embedded component and IsMapApplicableToGameMode compile
// from it as committed.
// ===================================================================================

#include "GameSource/Gui/Flow/PreEvent/States/BrnPreRaceFlyBy.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStream (the streamed assert)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N> / CgsModule::Event
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                      // CgsGui::GuiAccessPointers
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface + the out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiEventDrawEventIcons::EIconDisplayType
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // GuiEventShowHideSatNav
#include "GameSource/Gui/SatNav/BrnMainMap.h"                             // MainMapComponent + MainMapParameterBundle
#include "GameSource/Gui/SatNav/BrnMapIconManager.h"                      // MapIconManager

namespace BrnGui
{
    namespace GSM = BrnGameState::GameStateModuleIO;

    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) ----------------------------
        const s32 KI_CHANNEL_GUI_OUT = 40;   // 0x28

        // The log-category bit the debug print is gated on (`clrldi r11,r11,63`).
        const u64 KX_MESSAGE_FILTER_BIT = 1;

        // id 532 -- the "pre-race fly-by entered" GUI-out notification. X360 OnEnter builds
        // { 1, 532, 12 } at 0x824C64E4..0x824C64F0 and posts a 16-byte record on channel 40
        // (`li r6,0x10` / `li r5,0x28`). Per CgsGuiEvent.h the first header word is the
        // PAYLOAD size, so the payload is ONE byte at +12 -- the emitter simply never
        // writes it, and a 1-byte payload at +12 pads the record out to the 16 AddEvent is
        // given. The house shape for a 1-byte payload is BrnGuiDemangledEventTypes.h's
        // GuiEventShowHideBoostBar (id 214 size 1). Exactly the record BrnPausedHudState
        // posts for the same id (BrnPausedHudState.cpp:24). FLAG: consumer-derived type
        // name -- no DWARF row survives for the X360 id-532 event, and the payload byte's
        // meaning is not recovered (nothing in the image ever stores it).
        struct GuiEventPreRaceFlyByEnter : public CgsGui::GuiEvent<532>
        {
            u8 mu8Payload;   // +0x0C -- the 1-byte payload the size word names; UNWRITTEN by the emitter

            GuiEventPreRaceFlyByEnter()
                : CgsGui::GuiEvent<532>(
                      static_cast<u32>(sizeof(mu8Payload)),                            // X360 word0 == 1
                      static_cast<u32>(offsetof(GuiEventPreRaceFlyByEnter, mu8Payload))) // X360 word2 == 12
                , mu8Payload(0)
            {
            }
        };
        // Host layout pin: CgsGui::GuiEvent<N> is the three header words over an empty
        // CgsModule::Event base, so the record is 16 bytes on the host exactly as on the
        // console -- the AddEvent size argument below is a host sizeof, never a baked 16.
        static_assert(sizeof(GuiEventPreRaceFlyByEnter) == 16,
                      "host record matches the X360 16-byte id-532 post");
    }

    // ================================================================================
    //  OnEnter  @ 0x824C6498  (cpp:187)
    //
    //  Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
    //  (disassembly in scratchpad/waveJ/prfb_xrefs.txt; asm arbitrated over Hex-Rays).
    //
    //  Bring the fly-by up: observe the flow's event set, announce the entry, cache the
    //  GuiCache, construct every embedded component against the "flyByHud_mc" apt clip,
    //  reset the state's own scalars, and -- for the modes that get a map -- take
    //  ownership of the shared MapIconManager for the pre-race route display.
    //
    //  Notes taken from the asm rather than the pseudocode:
    //   * The description-field loop really runs all FIVE fields: r30 walks
    //     off_82F26BEC and stops at &qword_82F26C00, i.e. 20 bytes / 4 == 5 iterations
    //     (0x824C65F0/0x824C65F8). It is AppendExpectedComponents that only registers 3.
    //   * mEventName / mModeType / the description fields / mStateAnimator go through the
    //     component vtable slot 0 (`lwz r11,0(rN)` + `bctrl`) == the virtual
    //     GuiComponent::Construct; mLargeEventIcon is a DIRECT call to
    //     BrnGui::IconComponent::Construct (0x824C6618) because that overload takes the
    //     extra state-identifier table argument (here null).
    //   * The four `stb 0` at mainmap+0x678..0x67B (0x824C66AC..0x824C66BC) are the
    //     committed MainMapComponent::SetStickMapToScreenEdges(false x4) inlined -- the
    //     setter is called by name, the console offsets are recorded only.
    //   * SetOwnerParameters takes NINE parameters, two of them on the stack, which
    //     Hex-Rays dropped: the outgoing parameter save area starts at sp+0x14 with
    //     8-byte slots, so sp+0x54 (`li r6,5` stored at 0x824C689C) is parameter 8 and
    //     sp+0x5C (`stw r26` == "flyByHud_mc" at 0x824C6894) is parameter 9. r6 is then
    //     reloaded with 16 for parameter 3. Its RETURN VALUE replaces mIconManagerOwnerId
    //     (`stw r3, 0x990(r31)` at 0x824C68BC).
    //   * The icon-display-type argument is 5 == E_ICON_DISPLAY_TYPE_COUNT, i.e. the
    //     one-past-the-end sentinel, not a real display set. Recorded as measured.
    //   * The debug print is the inlined StrStreamBase::operator<<(s32) (the "%d"/"0x%X"
    //     AppendFormat split at 0x824C6824/0x824C684C is that operator's print-mode
    //     branch); on the host it is the committed operator<< chain.
    //   * No float COMPARE anywhere in this body -- f31 only ever carries 0.0f -- so there
    //     is no NaN-polarity decision to make.
    //   * Every console byte offset quoted below is a 32-bit-ABI reference only; all
    //     member access is by name so the host's own layout applies.
    // ================================================================================
    void PreRaceFlyByState::OnEnter()
    {
        // X360 +0x1034. -1 means "no icon count seen yet", so the first UpdateIconManager
        // pass always counts as an increase and fires the map-scroll-end sound.
        miPreviousIconCount = -1;

        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // Announce the fly-by entry on the GUI-out channel (X360 record size 16).
        {
            GuiEventPreRaceFlyByEnter lEnter;
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEnter),
                KI_CHANNEL_GUI_OUT, static_cast<s32>(sizeof(lEnter)));
        }

        // Both asserts are the committed accessor path's own (non-fatal on the X360, so the
        // fetch below runs regardless -- exactly as the console does).
        CGS_ASSERT(mpStateInterface->GetAccessPointers() != 0,
                   "mpAccessPointers != NULL");  // CgsGuiStateInterface.h:344
        CgsGui::GuiAccessPointers* lpAccessPointers = mpStateInterface->GetAccessPointers();
        CGS_ASSERT(lpAccessPointers->GetGuiCache() != 0, "mpGuiCache");  // CgsGuiShared.h:201

        mpGuiCache     = lpAccessPointers->GetGuiCache();
        meCurrentState = E_PRERACE_UNLOADED;

        // ---- the components, all parented on the fly-by clip ---------------------------
        mEventName.Construct(KAC_EVENT_NAME_TEXTFIELD_NAME, mpStateInterface,
                             KAC_STATE_COMPONENT_NAME);
        mModeType.Construct(KAC_MODE_TYPE_TEXTFIELD_NAME, mpStateInterface,
                            KAC_STATE_COMPONENT_NAME);

        for (s32 liLine = 0; liLine < KI_MAX_LINES_DESCRIPTION_TEXT; ++liLine)
        {
            maEventDescriptionText[liLine].Construct(KAAC_EVENT_DESC_TEXTFIELD_NAMES[liLine],
                                                     mpStateInterface,
                                                     KAC_STATE_COMPONENT_NAME);
        }

        // No state-identifier table and no parent clip: the destination icon is addressed
        // by name and driven with the string-keyed SetState.
        mLargeEventIcon.Construct(KAC_LARGE_EVENT_ICON_NAME, mpStateInterface, 0, 0);
        mStateAnimator.Construct(KAC_STATE_ANIMATOR_NAME, mpStateInterface, 0);

        // ---- the state's own scalars ----------------------------------------------------
        mbEndRequestSent = false;   // X360 +0x980 (stb)
        mbDoMapPan       = false;   // X360 +0x981 (stb)
        mfTimeRemaining  = 0.0f;    // X360 +0x97C

        MainMapComponent::MainMapParameterBundle lMapParameters;
        lMapParameters.mv4ViewRect    = KV4_VIEW_RECT;
        lMapParameters.mv4PaddingRect = KV4_PADDING_RECT;
        lMapParameters.meMapType      = GuiEventRenderMainMap::E_MAPTYPE_PRERACE;  // X360 li 1
        mMainMapComponent.Construct(mpStateInterface, &lMapParameters);
        mMainMapComponent.Prepare();

        mpIconManager = 0;

        // The pre-race map is free to sit wherever the world centre puts it.
        mMainMapComponent.SetStickMapToScreenEdges(false, false, false, false);

        mfIconAnimationStartTime = 0.0f;
        mv2WorldCenterPoint.SetZero();   // X360 `stvx` of a zeroed vector at +0x1020

        // Stores in the console's order: the type word first, then the id (the id 0 means
        // "no large event icon resolved yet"; SetEventIconResource fills it in). X360 `li r10,4`
        // == E_GUI_RESOURCETYPE_APT.
        mLargeIconResource.meType = CgsGui::E_GUI_RESOURCETYPE_APT;
        mLargeIconResource.muId   = 0;

        mbHiddenDueToPause  = false;
        mIconManagerOwnerId = MapIconManager::E_PRERACE_FLYBY_MAP;

        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:271 (non-fatal)

        // X360 `stbx r24, mpGuiCache, 0xA015` -- the cache-side "a fly-by is running" latch.
        mpGuiCache->SetPreRaceFlyByActive(true);

        // The 15-case jumptable at 0x824C6744 IS IsMapApplicableToGameMode inlined (false for
        // modes 2,3,4,7,9,15,16); the method is the named face of it.
        if (IsMapApplicableToGameMode(static_cast<GSM::EGameModeType>(mpGuiCache->GetGameMode())))
        {
            mpIconManager = mpGuiCache->GetMapIconManager();
            CGS_ASSERT(mpIconManager != 0, "mpIconManager");   // cpp:277 (non-fatal)

            mpIconManager->SetIconsVisible(true);

            if ((CgsDev::Message::gxMessageFilterFlags & KX_MESSAGE_FILTER_BIT) != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "MAPICONMANAGER: PreRaceFlyBy is calling SetOwnerParameters with OwnerID "
                    << static_cast<s32>(mIconManagerOwnerId)
                    << ".\n";
            }

            // The manager hands back the owner id it actually granted, which is what the
            // release path later quotes -- hence the self-assignment through the call.
            mIconManagerOwnerId = mpIconManager->SetOwnerParameters(
                mpStateInterface,
                macSatNavIconBaseName,          // "SatNavIcon"
                KI_PRERACEMAP_NUMICONS,         // 16
                mIconManagerOwnerId,
                false,                          // lbUseRoadSigns
                false,                          // lbShowingDriveThrus
                false,                          // lbAllowDriveThruSelection
                GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_COUNT,   // X360 stack arg 8 == 5
                KAC_STATE_COMPONENT_NAME);      // X360 stack arg 9 == "flyByHud_mc"

            // Friend pokes: the X360 writes these three straight through the manager
            // (stb +0xAA1C, stw +0xAA04, stb +0xAA20) -- there is no setter for them.
            mpIconManager->mbRotateSatNav        = false;
            mpIconManager->meIconSizeMode        = MapIconManager::E_ICONSIZE_LARGE;
            mpIconManager->mbShowingPreRaceRoute = true;
        }
    }

    // ================================================================================
    //  OnLeave  @ 0x824C68F0  (cpp:565)
    //
    //  Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
    //  (disassembly in scratchpad/waveJ/asm_onleave.txt; asm arbitrated over Hex-Rays).
    //
    //  Tear the fly-by down: fade the sat-nav out on both the view and internal channels,
    //  drop the apt clips, stop observing, release the screens the mode pulled in, and
    //  hand the icon manager back.
    //
    //  Notes taken from the asm rather than the pseudocode:
    //   * The two sat-nav posts are the SAME record sent twice, on channel 41 then 42
    //     (0x824C6924 `li r5,0x29` / 0x824C695C `li r5,0x2A`), size 24: header
    //     { 12, 213, 12 } + { map type 0, 0.0f, a byte-zero word }. Hex-Rays's `v14`
    //     shuffle is just the compiler reusing the stack slots.
    //   * The two 20-byte channel-41 posts of { 8, 18, 12 } + { "", 2 } / { "", 1 } are
    //     CgsGui::StateInterface::PlayAptMovie inlined -- VERIFIED against the committed
    //     body (CgsGuiStateInterface.cpp:46), which posts exactly that record on channel
    //     41 with sizeof == 20. Called by name here. The X360's `unk_820046A7` is the
    //     shared empty-string sentinel, i.e. "".
    //   * `cmpwi cr6, r11, 0xA` + `bge` at 0x824C6B44 is a SIGNED compare and the ONLY
    //     guard on the per-gamemode screen index: the online modes (10..16) skip the
    //     unload, but E_MODE_NONE (-1) does NOT -- see the note at that call.
    //   * meCurrentState = -1 is stored between ClearExpectedAptComponentList and the
    //     icon-manager release (0x824C6B78); that order is kept.
    //   * The game mode is read from the cache twice (0x824C6A6C for the map predicate,
    //     0x824C6B40 for the screen index) -- both are the same accessor.
    //   * No float COMPARE in this body (the only float is the 0.0f fade), so there is no
    //     NaN-polarity decision to make.
    //   * Every console byte offset quoted is a 32-bit-ABI reference only; all member
    //     access is by name.
    // ================================================================================
    void PreRaceFlyByState::OnLeave()
    {
        // Fade the main sat-nav map out immediately (fade time 0.0f, show false). The same
        // record goes to the view layer (channel 41) and to the internal listeners (42);
        // both go through the StateInterface templates, which build the
        // GuiEventWrapper<T,channel> header the console stack-builds here. Posting the
        // struct straight onto the out-queue would ship a headerless 12-byte record --
        // GuiEventShowHideSatNav is deliberately the RAW 12-byte payload with no
        // CgsGui::GuiEvent<213> base (BrnGuiDemangledEventTypes.h:566).
        {
            GuiEventShowHideSatNav lHideSatNav;
            lHideSatNav.Construct(GuiEventShowHideSatNav::E_MAPTYPE_MAIN, false, 0.0f);

            mpStateInterface->OutputViewState(lHideSatNav);
            mpStateInterface->OutputInternalState(lHideSatNav);
        }

        // Level 2 with an empty name: stop whatever the fly-by had playing on that layer.
        mpStateInterface->PlayAptMovie("", 2);

        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // Streamed diagnostic (X360 builds it into CgsDev::Assert::gpcMessageBuffer through a
        // StrStream; the house idiom folds that to a local buffer). Non-fatal -- everything
        // below dereferences mpGuiCache anyway, exactly as the console does.
        if (mpGuiCache == 0)
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Gui Cache should be setup in the OnEnter";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                lacMessage,
                "..\\..\\..\\GameSource\\Gui/Flow/PreEvent/States/BrnPreRaceFlyBy.cpp",
                600);
            CgsDev::Assert::EndAssert();
        }

        // Same inlined predicate as OnEnter (the 15-case jumptable at 0x824C6A90).
        if (IsMapApplicableToGameMode(static_cast<GSM::EGameModeType>(mpGuiCache->GetGameMode())))
        {
            // Put the shared map back to whatever the rest of the GUI expects.
            mpGuiCache->RefreshMapState();
            mpStateInterface->PlayAptMovie("", 1);
        }

        // The large event icon is only loaded once SetEventIconResource has resolved one.
        if (mLargeIconResource.muId != 0)
        {
            mpGuiCache->UnloadResource(mLargeIconResource);
        }

        // MEASURED CONSOLE QUIRK, reproduced as-is: the guard is one-sided. The X360 emits
        // `cmpwi cr6, r11, 0xA` + `bge` (0x824C6B44) and nothing else, so an E_MODE_NONE (-1)
        // mode passes the test and indexes one tuple BEFORE maPerGamemodeScreens. Kept faithful
        // rather than silently hardened -- flag for the verify round if the host cares.
        const s32 liGameMode = mpGuiCache->GetGameMode();
        if (liGameMode < GSM::E_MODE_OFFLINE_COUNT)   // X360 signed `cmpwi 0xA` + `bge`
        {
            mpGuiCache->UnloadResource(maPerGamemodeScreens[liGameMode]);
        }

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_HUD);

        meCurrentState = E_PRERACE_INVALID;

        if (mpIconManager != 0)
        {
            if ((CgsDev::Message::gxMessageFilterFlags & KX_MESSAGE_FILTER_BIT) != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "MAPICONMANAGER: PreRaceFlyBy is calling ReleaseResources.\n";
            }

            mpIconManager->ReleaseResources(mpStateInterface, mIconManagerOwnerId);
        }

        // X360 `stbx r29, mpGuiCache, 0xA015` -- clear the latch OnEnter set.
        mpGuiCache->SetPreRaceFlyByActive(false);
    }
}
