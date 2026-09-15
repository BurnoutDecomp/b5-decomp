// ===================================================================================
// BrnGui::CrashNavMap -- the BASE partfile of the crash-nav map screen state.
//
// Wave J landed nineteen CrashNavMap:: bodies as the eight BrnCrashNavMap_wJ_0*.cpp
// partfiles and never landed a base. This file is that base. It carries the four
// remaining EXPORTED bodies, the two ICF-folded empty input virtuals, and the class's
// own two statics:
//
//   CrashNavMap::CrashNavMap()          @0x825114B8   (vptr-only ctor -- see below)
//   CrashNavMap::OnLeave()              @0x824CB440   (vtable slot 1)
//   CrashNavMap::PlaceCursorOnPlayer()  @0x824BF6F0   (DWARF cpp:2136)
//   CrashNavMap::SetFilterFromPanel()   @0x824CC0E0   (DWARF cpp:1574)
//   CrashNavMap::HandleCrashNavInputPressed / ...Released  -- vtable slots 9 / 10, both
//                                       @0x8284CB38 (a shared `blr`; see the note below)
//   CrashNavMap::maResourcesToLoad[7]   @0x82F26E84
//   CrashNavMap::muNumResourcesToLoad   @0x82F26EBC (== 7)
//
// MOUNT AS A SET. The eight wJ partfiles and this file share `mauComponentHashIds`
// (defined with external linkage in wJ_04, `extern`-declared in wJ_02) and this file's
// two statics feed wJ_03's CheckForLoadComplete and the wave-H OnlineGameRoomPlayerInfo
// TU. Mounting any subset silently mis-wires the expected-component set.
//
// -----------------------------------------------------------------------------------
// TWO HEADER CORRECTIONS MADE THIS WAVE (applied to BrnCrashNavMap.h in the same edit):
//
// (1) THE BASE INPUT VIRTUALS ARE EMPTY, AND THE ADDRESSES THE HEADER CARRIED WERE
//     INSIDE Construct. The committed header annotated
//     `HandleCrashNavInputPressed @0x824B6798` / `...Released @0x824B67B8`. Neither is a
//     function entry: 0x824B6798 disassembles to `addi r29,r29,4` and 0x824B67B8 to
//     `addi r3,r31,0x6E0` -- the icon-name loop tail and the CrashNavPanel::StoreSettings
//     call of Construct @0x824B6660 (raw image bytes 3BBD0004 / 387F06E0), and neither has
//     an .ida-exports .json. The REAL slots come from the class vtable at off_82076664
//     (the pointer this file's ctor stores at +0):
//         slot 0  +0x00  0x824CB158  OnEnter
//         slot 1  +0x04  0x824CB440  OnLeave
//         slot 2  +0x08  0x824DD6D8  Update
//         slot 6  +0x18  0x824B6660  Construct
//         slot 9  +0x24  0x8284CB38  HandleCrashNavInputPressed
//         slot10  +0x28  0x8284CB38  HandleCrashNavInputReleased
//         slot11  +0x2C  0x824B67E0  AppendExpectedAptComponents
//         slot12  +0x30  0x824D8C40  SetupComponents
//     0x8284CB38 is one instruction, `4E800020` == `blr`, and it is ICF-folded across the
//     whole image (IDA names it CgsSceneManager::CgsCollision::BaseCollisionGenerator::
//     Destruct; the same pointer also fills slots 5, 7 and 19 of this vtable). So the BASE
//     pair really are empty virtuals -- the real input maps are the DERIVED
//     CrashNavMapMain::HandleCrashNavInput{Pressed,Released} @0x824CCAE8 / @0x824CCD90.
//     They are bodied empty here, which is store-for-store faithful.
//
// (2) OnLeave is @0x824CB440, not @0x824CB4B8. The header's number is 0x78 into the
//     function (`li r6, 0x18`, the second AddEvent's size argument). 0x824CB440 is the
//     vtable slot AND the .json export. Likewise PlaceCursorOnPlayer is @0x824BF6F0, not
//     "@0x824CBAA8 region" (0x824CBAA8 is inside UpdateIconManager @0x824CBA70, which is
//     merely one of its three callers).
//
// -----------------------------------------------------------------------------------
// FIVE DWARF METHODS WITH NO BODY ANYWHERE -- DECLARED, NOT INVENTED:
//   UpdateGuiCache(const CgsModule::Event*)   DWARF cpp:589
//   SetMapPanelState(MapState)                DWARF cpp:1525
//   ZoomIn() / ZoomOut() / ZoomUpdate()       DWARF cpp:2232 / :2249 / :2266
// None has an X360 symbol (scratch/func_index.tsv has 23 `CrashNavMap::` rows and none of
// these), none is reached from any of the nineteen landed wJ bodies, and none is inlined
// at a site this wave can pin. The nearest thing to evidence cuts the other way: the
// cache hand-off Update performs in its id-64 arm asserts at cpp:330/:339, i.e. inside
// Update (cpp:274), NOT inside UpdateGuiCache (cpp:589) -- so that arm is not the folded
// UpdateGuiCache, it is source written inline in Update. Bodies for the five are
// UNRECOVERABLE on this image; they stay declaration-only, and their declarations in
// BrnCrashNavMap.h now carry that reason. FLAG.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (the per-address listings under
// .ida-exports/BURNOUT_X360_ARTIST.XEX/), with the raw image consulted for the vtable, the
// rodata tables and the two `vperm` masks. X360 offsets appear only in comments; the host
// layout is name-based.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"
#include <cstdlib>   // getenv ([cnav-diag])

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT + Begin/Fire/EndAssert
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStream (the streamed default-arm assert)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEventWrapper
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"  // sResourceTuple / E_GUI_RESOURCETYPE_APT
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface (out-queue, PlayAptMovie, OutputViewState)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // BrnGui::GuiEventFilterEventIcons
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiFlow / GuiAudioTriggerEvent / SatNavIconInfo
#include "GameSource/Gui/Flow/Screen/Components/BrnCrashNavPanel.h"       // BrnGui::CrashNavPanel
#include "GameSource/Gui/Flow/Screen/Components/BrnCursor.h"              // BrnGui::GuiCursor
#include "GameSource/Gui/SatNav/BrnGuiTracker.h"                          // BrnGui::GuiTracker::ClearTracker
#include "GameSource/Gui/SatNav/BrnMainMap.h"                             // BrnGui::MainMapComponent
#include "GameSource/Gui/SatNav/BrnMapIconManager.h"                      // BrnGui::MapIconManager (+ the embedded RoadSignIconManager)
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"  // InGamePlayerStatusData
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"                         // BrnGui::MapTransform

#include <cstring>   // std::memcpy (the GuiEventFilterEventIcons payload word)

// includes folded in from the BrnCrashNavMap_w*.cpp partfiles (2026-09-15)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"       // GuiComponent::GetName
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Containers/CgsHash.h"                    // CgsHash::CalculateHash
#include "GameSource/Gui/BrnGuiWorldDataController.h"                     // GetEventInfoFromEventId
#include "SharedClasses/Progression/BrnRaceEventData.h"                   // BrnProgression::RaceEventData
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h" // CgsGui::GuiEventControllerAxis
#include "rw/math/vpu/vector2_operation.h"                  // rw::math::vpu::Dot (MagnitudeSquared)

// LobbyNameCmp @0x82B10050 -- the DirtySock collation-table string compare. Declared
// per-cpp exactly as BrnCrashNavMap_wJ_08.cpp and CgsServerInterfaceGames.cpp:47 do.
extern "C" s32 LobbyNameCmp(const char* lpcA, const char* lpcB);

namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) ------------------------
        const s32 KI_CHANNEL_GUI_OUT      = 40;   // `li r5,0x28`
        const s32 KI_CHANNEL_VIEW_STATE   = 41;   // `li r5,0x29`
        const s32 KI_CHANNEL_GUI_INTERNAL = 42;   // `li r5,0x2A`

        // OnLeave's apt-movie stop: the SAME level word CheckForLoadComplete starts the
        // screen's movie with (`li r11, 3` @0x824CB478), with an EMPTY movie name --
        // r28 == &unk_820046A7, and the image byte at 0x820046A7 is 0x00, i.e. "".
        const s32  KI_APT_MOVIE_LEVEL     = 3;
        const char KPC_NO_APT_MOVIE[]     = "";

        // The map-scroll-end cue OnLeave fires when the debouncer is still latched
        // scrolling. X360 off_82F26E7C; same string wJ_03's UpdateSoundEvents posts.
        const char KPC_SOUND_MAP_SCROLL_END[] = "CodeMapScrollEnd";

        // The GuiAudioTriggerEvent action every map cue carries (`li r4, 7`). FLAG: the
        // action enum's name is not in the recovered DWARF slice -- same note wJ_03 makes.
        const s32 KI_AUDIO_ACTION_MAP_CUE = 7;

        // HACK_FindABetterPlaceForMe_SetActiveLandmarksByEventID's animation parameter at
        // this call site (flt_82001C98 == 1.0f, loaded into f1 @0x824CB5D8 -- the PPC float
        // arg SKIPS its GPR slot, which is why Hex-Rays drops the trailing bool).
        const f32  KF_LANDMARK_ANIM_T     = 1.0f;

        // The two apt view-state names PlaceCursorOnPlayer drives on the button-prompt
        // animation. They are entries [6] and [7] of the SAME @0x82F26EC8 table wJ_05's
        // UpdateButtonPrompts reproduces in full -- the X360 reaches them as off_82F26EE0 /
        // off_82F26EE4, which are &table[6] / &table[7]. Only the two this body needs are
        // repeated here; a partial table indexed by the enum would be a trap.
        const char KPC_PROMPTS_ONLINE_RIVAL[]       = "FreeburnLobbyRival";        // [6]
        const char KPC_PROMPTS_ONLINE_PAUSE_RIVAL[] = "FreeburnLobbyPauseRival";   // [7]

        // GuiCache::maPlayerInfo[8] -- the online-roster walk bound (`cmpwi r11, 8`
        // @0x824BF998).
        const s32 KI_MAX_ONLINE_PLAYERS = 8;

        // The X360 assert-site file string, verbatim (aGamesourceGuiF_63).
        const char KAC_ASSERT_FILE[] =
            "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnCrashNavMap.cpp";

        // ---- out-queue payload views -------------------------------------------------

        // GuiEventShowHideSatNav (wire id 213, payload 12). Copied verbatim from
        // BrnCrashNavMap_wJ_03.cpp, which posts the same record with the SHOW values; this
        // file posts the HIDE pair. The homed BrnGuiDemangledEventTypes.h models the type as
        // an empty struct, so the three payload words are named generically. FLAG: word
        // roles not recovered; the receiving CustomRendererManager keys event 213 by a
        // sub-mode word (0 == MainMap, 1 == SatNav) and a renderable flag.
        //
        // The X360 leaves maPad as stack residue here (`lwz r26, var_158` picks up the 12
        // written for the PREVIOUS record's payload-offset word and stores it whole at
        // payload+8, so the flag byte -- the big-endian FIRST byte -- is the 0 the `stb`
        // put there and the three pad bytes carry 0,0,0x0C). Modelled zeroed, same as wJ_03.
        struct GuiEventShowHideSatNavPayload
        {
            s32  miSubMode;   // +0x00  X360 stores 0     (`stw r29`)
            f32  mfValue;     // +0x04  X360 stores 0.0f  (`stfs f31`, flt_82001CC0)
            bool mbFlag;      // +0x08  X360 stores 0     (`stb r29`)
            u8   maPad[3];    // +0x09  stack residue on the console; modelled zeroed

            GuiEventShowHideSatNavPayload(s32 liSubMode, f32 lfValue, bool lbFlag)
                : miSubMode(liSubMode), mfValue(lfValue), mbFlag(lbFlag)
            {
                maPad[0] = maPad[1] = maPad[2] = 0;
            }

            s32 GetEventType() const { return 213; }
        };

        // The record the inlined OutputGuiEvent<BrnGui::GuiAudioTriggerEvent> builds:
        // { 100, 457, 12, the 100-byte payload }, channel 40, 112 bytes. Verbatim from
        // BrnCrashNavMap_wJ_03.cpp -- the committed PC GuiAudioTriggerEvent already carries
        // the 12-byte queue header, but its GuiEvent<201> base seeds the payload-size word 0
        // and the id 201 while the X360 wire words are 100 and 457.
        const u32 KU_WIRE_ID_AUDIO_TRIGGER = 457;

        struct GuiAudioTriggerWire : public GuiAudioTriggerEvent
        {
            GuiAudioTriggerWire()
            {
                muHeader0 = static_cast<u32>(sizeof(GuiAudioTriggerEvent) -
                                             sizeof(CgsGui::GuiEvent<201>));
                muEventType = KU_WIRE_ID_AUDIO_TRIGGER;
            }
        };

        static_assert(sizeof(GuiAudioTriggerWire) == 112, "audio trigger record is 112 bytes");

        // Walk the cache's eight online-player records looking for the one driving the given
        // active-race-car index. Same helper (and the same known divergence) as
        // BrnCrashNavMap_wJ_08.cpp's: the X360 inlines the walk, striding maPlayerInfo by 312
        // (`addi r10,r10,0x138` @0x824BF994) and comparing the index word at record+276
        // WITHOUT touching the record pointer, then re-deriving &maPlayerInfo[i] and
        // null-checking THAT once. The committed accessor is the only handle on a record, so
        // the compare has to go through a non-null pointer, which makes a null record CONTINUE
        // the search where the X360 ENDS it. The two differ only if two slots carry the same
        // active-race-car index and the first one's record is null. (Duplicated rather than
        // shared because wJ_08 keeps its copy in an anonymous namespace; fold the two together
        // if the partfiles are ever concatenated.)
        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData*
        FindOnlinePlayerByActiveRaceCarIndex(const GuiCache* lpGuiCache,
                                             s32 liActiveRaceCarIndex)
        {
            for (s32 liSlot = 0; liSlot < KI_MAX_ONLINE_PLAYERS; ++liSlot)
            {
                const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerInfo =
                    lpGuiCache->GetOnlinePlayerInfo(liSlot);

                if (lpPlayerInfo != 0 &&
                    static_cast<s32>(lpPlayerInfo->meActiveRaceCarIndex) == liActiveRaceCarIndex)
                {
                    return lpPlayerInfo;
                }
            }
            return 0;
        }
    }

    // ================================================================================
    //  STATICS
    //
    //  maResourcesToLoad @0x82F26E84 -- seven { gGuiResourceIdentifier index, type }
    //  tuples, read big-endian out of the image; muNumResourcesToLoad @0x82F26EBC == 7,
    //  the very next word (which is why the table's length is not a guess). Type 4 is
    //  E_GUI_RESOURCETYPE_APT for all seven. Names are the gGuiResourceIdentifier rows
    //  (BrnGuiCache.cpp:45) at those indices.
    //
    //  Consumers: wJ_03's CheckForLoadComplete (EnsureResourcesAreLoaded) and the wave-H
    //  OnlineGameRoomPlayerInfo TU's UnloadMapResources, which unloads THIS base's table --
    //  which is why the pair is protected rather than file-local.
    // ================================================================================
    const CgsGui::sResourceTuple CrashNavMap::maResourcesToLoad[7] =
    {
        { 132u, CgsGui::E_GUI_RESOURCETYPE_APT },   // BrnCrashNavMapMain
        {  83u, CgsGui::E_GUI_RESOURCETYPE_APT },
        {  48u, CgsGui::E_GUI_RESOURCETYPE_APT },   // CrashNavPanel
        {  70u, CgsGui::E_GUI_RESOURCETYPE_APT },
        {  72u, CgsGui::E_GUI_RESOURCETYPE_APT },
        {  49u, CgsGui::E_GUI_RESOURCETYPE_APT },   // CrashNavLegend
        {  50u, CgsGui::E_GUI_RESOURCETYPE_APT },   // CrashNavBorough
    };

    const u32 CrashNavMap::muNumResourcesToLoad = 7u;

    // ================================================================================
    //  CrashNavMap::CrashNavMap  @ 0x825114B8
    //
    //  The compiler-generated default constructor, and NOTHING ELSE. Every store the
    //  X360 body makes is a vptr: this+0 (the class vtable off_82076664), the embedded
    //  MainMapComponent's at +96, the CrashNavLegend's at +22960 and its five sub-clip
    //  components at +0x94/+0x1BC/+0x250/+0x2E4/+0x378/+0x40C from there, the GuiCursor's
    //  at +24160 and the two AnimationComponents' at +24404 / +24544 -- plus two real
    //  subobject constructor calls, MapManager::MapManager on the MainMapComponent's
    //  embedded tile working set (r3 = this+96+0x8C) and CrashNavPanel::CrashNavPanel on
    //  this+1760. In C++ all of that IS member subobject construction, so the body is
    //  empty; writing the vptr stores by hand would be a fork of the ABI.
    //
    //  NOT A SINGLE SCALAR MEMBER IS INITIALISED HERE. The screen's cold-start values are
    //  Construct's job (BrnCrashNavMap_wJ_04.cpp @0x824B6660) and its per-visit reset is
    //  OnEnter's; between placement in the state pool and Construct, every scalar below
    //  mMainMapComponent holds pool residue on the console. Reproduced faithfully -- do
    //  NOT "helpfully" add an initialiser list here, or the pool-reuse behaviour the two
    //  callers (BrnScreenFlow::Prepare @0x82523E50 and OnlineGameRoomPlayerInfo's own ctor
    //  @0x82514F08) depend on changes shape.
    // ================================================================================
    CrashNavMap::CrashNavMap()
    {
    }

    // ================================================================================
    //  CrashNavMap::OnLeave  @ 0x824CB440  (vtable slot 1, DWARF cpp:418)
    //
    //  Tear the map screen down: stop the screen's apt movie, tell both the view and the
    //  internal channel to hide the sat-nav with no fade, fire the scroll-end cue if the
    //  debouncer was still latched, park the cursor at the origin, hand the cache's
    //  expected-component bookkeeping back, and release the shared map-icon manager.
    //
    //  Called by CrashNavMapMain::OnLeave @0x824CCA98, CrashNavMapEvent::OnLeave
    //  @0x824CC790 and three OnlineGameRoomPlayerInfo sites.
    // ================================================================================
    void CrashNavMap::OnLeave()
    {
        CgsGui::GuiStackEventQueue::GuiEventQueueLarge* lpOutQueue =
            mpStateInterface->GetOutputEventQueue();

        // Record { 8, 18, 12, "", 3 } on channel 41, 20 bytes -- exactly what
        // StateInterface::PlayAptMovie builds (GuiEventPlayAptMovie is GuiEvent<18>(8,12)).
        // The name pointer is &unk_820046A7 == the empty string, i.e. "play nothing at
        // level 3", the same level CheckForLoadComplete started "BrnCrashNavMapMain" at.
        mpStateInterface->PlayAptMovie(KPC_NO_APT_MOVIE, KI_APT_MOVIE_LEVEL);

        // The X360 clears the loaded latch in the middle of building the next record
        // (`stb r29, 0x44(r31)` @0x824CB4DC); kept where the asm has it.
        mbIsScreenLoaded = false;

        // The hide pair. Same record shape as CheckForLoadComplete's show pair -- payload
        // { sub-mode 0, 0.0f, false } -- posted at 24 bytes on channel 41 and then again on
        // channel 42. Sizes are host sizeof expressions, never the console's 12 / 24.
        GuiEventShowHideSatNavPayload lShowHideSatNav(0, 0.0f, false);

        CgsGui::GuiEventWrapper<GuiEventShowHideSatNavPayload, KI_CHANNEL_VIEW_STATE>
            lViewStateRecord(lShowHideSatNav);
        lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lViewStateRecord),
                             lViewStateRecord.GetChannel(),
                             static_cast<s32>(sizeof(lViewStateRecord)));

        CgsGui::GuiEventWrapper<GuiEventShowHideSatNavPayload, KI_CHANNEL_GUI_INTERNAL>
            lInternalStateRecord(lShowHideSatNav);
        lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lInternalStateRecord),
                             lInternalStateRecord.GetChannel(),
                             static_cast<s32>(sizeof(lInternalStateRecord)));

        // Falling edge of the map-scroll cue: if UpdateSoundEvents left the debouncer
        // latched, close the sound out on the way through the door (`lbz r11, 0x6140`).
        if (mSoundData.mbPrevIsScrolling)
        {
            GuiAudioTriggerWire lScrollEnd;
            lScrollEnd.Construct(KI_AUDIO_ACTION_MAP_CUE, "", KPC_SOUND_MAP_SCROLL_END, "");
            lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lScrollEnd),
                                 KI_CHANNEL_GUI_OUT,
                                 static_cast<s32>(sizeof(lScrollEnd)));
        }

        // Park the cursor. The X360 assembles a zero quad on the stack and passes it in v1
        // with r4 == 0, i.e. SetPosition({0,0,0,0}, /*clamp*/false).
        const Vector2 lv2CursorOrigin = { 0.0f, 0.0f, 0.0f, 0.0f };
        mCursor.SetPosition(lv2CursorOrigin, false);

        if (mpGuiCache != 0)
        {
            // The in-event sign-colouring gate byte (cache +0x4B4A) doubles as "an event is
            // live": while it is set, re-latch the active-landmark set for the current event
            // at animation parameter 1.0 on the way out, so whatever comes next inherits a
            // finished set rather than a half-animated one. The X360 compares the byte
            // against 1, not against zero.
            if (mpGuiCache->GetInEventColouringGate())
            {
                mpGuiCache->HACK_FindABetterPlaceForMe_SetActiveLandmarksByEventID(
                    mpGuiCache->GetEventID(), KF_LANDMARK_ANIM_T, true);
            }

            mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
            mpGuiCache->ClearExpectedControlledAptComponentList();
        }

        if (mpIconManager != 0)
        {
            // `stwx r29, mpIconManager, 0xAA08` -- the filter goes back to "show everything"
            // for the next owner. No setter links (SetIconFilter is one of the inline
            // one-liners the compiler folded), so the state writes the member directly
            // through `friend struct CrashNavMap`, exactly as ResetIconManager does.
            mpIconManager->meIconFilterMode = MapIconManager::E_ICONFILTER_ALL;

            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "MAPICONMANAGER: CrashNavMap is calling ReleaseResources.\n";
            }

            mpIconManager->ReleaseResources(
                mpStateInterface,
                static_cast<MapIconManager::OwnerId>(mIconManagerOwnerId));
        }

        mIconManagerOwnerId = 0;
        mbIsExiting         = true;
    }

    // ================================================================================
    //  CrashNavMap::HandleCrashNavInputPressed   (vtable slot 9,  @0x8284CB38)
    //  CrashNavMap::HandleCrashNavInputReleased  (vtable slot 10, @0x8284CB38)
    //
    //  Both slots point at the same one-instruction `blr` the linker ICF-folded across the
    //  whole image -- the base does nothing with controller input. Update (wJ_08) still
    //  dispatches through vtable +0x24 / +0x28 so the derived screens can override; see the
    //  header-correction note in this file's banner for how the slots were read.
    // ================================================================================
    void CrashNavMap::HandleCrashNavInputPressed(const CgsModule::Event* lpEvent)
    {
        (void)lpEvent;
    }

    void CrashNavMap::HandleCrashNavInputReleased(const CgsModule::Event* lpEvent)
    {
        (void)lpEvent;
    }

    // ================================================================================
    //  CrashNavMap::PlaceCursorOnPlayer  @ 0x824BF6F0  (DWARF cpp:2136)
    //
    //  Snap the map cursor onto the local player's car and make the player the selected
    //  icon. Returns whether it actually did so -- false only when the cache has not
    //  arrived, which is what the callers (UpdateIconManager, CrashNavMapMain::Update,
    //  OnlineGameRoomPlayerInfo::Update) test.
    //
    //  When the screen is in rival-selection mode the player also becomes the selected
    //  "rival": the local car id goes into the hover slot, the player's name is looked up
    //  from the online roster, and -- only if that name actually CHANGED -- the button
    //  prompts transition to the rival flavour of the online prompt set.
    // ================================================================================
    bool CrashNavMap::PlaceCursorOnPlayer()
    {
        bool lbPlacedOnPlayer = false;

        if (mpGuiCache != 0)
        {
            // The cache's player block. The X360 forms `r28 = mpGuiCache + 0x4AE0` and
            // asserts THAT against zero -- an address-of-member that can never be null, so
            // the assert is decorative on both platforms. Reproduced because the console
            // has it. +0x4AE0 is the head of the block SatNavComponent views as GuiPlayerInfo
            // and GuiCache exposes as GetWorldCameraPosition(); the two other lanes this body
            // reads are the local car id (+0x4AF0) and the active-race-car index (+0x4B00),
            // both of which have their own committed accessors.
            const Vector4& lrv4PlayerWorldPos = mpGuiCache->GetWorldCameraPosition();
            CGS_ASSERT(&lrv4PlayerWorldPos != 0, "lpPlayerInfo");   // cpp:2183 (non-fatal)

            // -----------------------------------------------------------------
            // World -> device, then onto the cursor.
            //
            // INLINING REVERSED. The X360 emits ~90 VMX instructions here and ONE call,
            // `bl MapTransform::Transform` @0x824503C0 (the two-matrix overload, which takes
            // its point in v1 and a Matrix33 copy by hidden reference in r3 / r4). What the
            // VMX block builds is:
            //   v1  = vperm(playerPos, playerPos, mask@0x82CDA450). The mask bytes read from
            //         the image are {0,1,2,3, 24,25,26,27, 0,1,2,3, 0,1,2,3} == (x, z, x, x),
            //         i.e. MapTransform::Flatten's world-XZ lane pick.
            //   r3  = MakeCoordSpaceFromRect(mMainMapComponent.mv4WorldRect)   -- the rect at
            //         state+0x670 == component+1552, splat lane2-lane0 / lane3-lane1 into the
            //         (w,0,0)/(0,h,0)/(x,y,1) rows BrnMapUtils.cpp:103 spells out.
            //   r4  = MakeCoordSpaceFromRect(mMainMapComponent.mv4ViewRect) composed with
            //         MapTransform::smm33DeviceSpace (unk_82FB3050, loaded at +0/+0x10/+0x20
            //         and folded in by the vmulfp128/vmaddfp chain) -- the view rect is the
            //         one at state+0x680 == component+1568.
            // Affine composition is associative, so `Transform(p, from, viewSpace*device)`
            // is written below as `Transform(Transform(p, from, viewSpace), device)`: two
            // public calls with the same result, and the same two-step idiom
            // MainMapComponent::Construct already uses for its own display rect. Composing
            // Matrix33s directly is not an option -- BrnMapUtils.cpp's Multiply33 is a
            // file-local helper, not part of MapTransform's surface.
            //
            // The two rects are private members of MainMapComponent reached through its
            // `friend struct CrashNavMap`; its GetWorldRect()/GetViewRect() accessors are
            // header-inline on the console and body-less here, and which member each reads
            // is not attested (BrnMainMap.h says so at their declarations).
            // -----------------------------------------------------------------
            const Vector2 lv2PlayerMapPoint = { lrv4PlayerWorldPos.x, lrv4PlayerWorldPos.z,
                                                0.0f, 0.0f };

            const Matrix33 lm33MapSpace =
                MapTransform::MakeCoordSpaceFromRect(mMainMapComponent.mv4WorldRect);
            const Matrix33 lm33ViewSpace =
                MapTransform::MakeCoordSpaceFromRect(mMainMapComponent.mv4ViewRect);

            const Vector2 lv2ViewPoint =
                MapTransform::Transform(lv2PlayerMapPoint, lm33MapSpace, lm33ViewSpace);
            const Vector2 lv2DevicePoint =
                MapTransform::Transform(lv2ViewPoint, MapTransform::GetDeviceSpace());

            mCursor.SetPosition(lv2DevicePoint, true);

            // The player's icon becomes the locked icon, and the sound debouncer's previous
            // position is re-seeded from the same lane so the snap does not read as a scroll
            // next frame (`lvx128 v0, r0, r28` twice, into +0x6100 and +0x6130).
            mLockedIconInfo.SetIconType(
                GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_PLAYER_CAR);
            mLockedIconInfo.SetPositionLane(lrv4PlayerWorldPos);
            mSoundData.mPrevCurPos = Vector3{ lrv4PlayerWorldPos.x, lrv4PlayerWorldPos.y,
                                              lrv4PlayerWorldPos.z, lrv4PlayerWorldPos.w };

            lbPlacedOnPlayer = true;

            // `cmplwi r9, 1` on mbSelectRivals -- an explicit compare against 1.
            if (mbSelectRivals)
            {
                mbLocalPlayerSelected = true;

                // Scheduled below the store above by the X360 (the cache is dereferenced
                // either way); non-fatal, so the position is behaviourally irrelevant.
                CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:2208

                // Name the player from the online roster, matched on the local player's
                // active-race-car index. "Changed" means: the roster row's name differs from
                // the one already latched, or -- on the no-row path -- a non-empty latched
                // name is being cleared.
                const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpRosterRow =
                    FindOnlinePlayerByActiveRaceCarIndex(
                        mpGuiCache, mpGuiCache->GetPlayerActiveRaceCarIndex());

                bool lbPlayerNameChanged = false;
                if (lpRosterRow != 0)
                {
                    lbPlayerNameChanged =
                        (LobbyNameCmp(mPlayerName.macName,
                                      lpRosterRow->mPlayerName.macName) != 0);
                    mPlayerName.Construct(lpRosterRow->mPlayerName.macName);
                }
                else
                {
                    lbPlayerNameChanged  = (mPlayerName.macName[0] != '\0');
                    mPlayerName.macName[0] = '\0';   // X360 `stb r27, 0x60C8`
                }

                if (lbPlayerNameChanged)
                {
                    // INLINING REVERSED, exactly as wJ_05's UpdateButtonPrompts documents:
                    // the X360 emits AddOutputAptViewState("apt_Transition", <name>, false)
                    // on the button-prompt component, but the clip name and the flag are
                    // constants at every site because they belong to the callee --
                    // AnimationComponent::Run (DWARF BrnAnimationComponent.h:64, one
                    // const char* parameter). Note the console does NOT latch
                    // meNavigationButtonsState here; it only runs the transition.
                    //
                    // The offline screen type falls through with no transition at all.
                    if (meScreenType == E_SCREEN_TYPE_ONLINE)
                    {
                        mButtonPromptsAnimation.Run(KPC_PROMPTS_ONLINE_RIVAL);
                    }
                    else if (meScreenType == E_SCREEN_TYPE_ONLINE_FROM_PAUSE)
                    {
                        mButtonPromptsAnimation.Run(KPC_PROMPTS_ONLINE_PAUSE_RIVAL);
                    }
                }

                // `ld r11, 0x10(r28)` / `std r11, 0x60B8(r31)` -- the whole 8-byte CgsID.
                // Reached on BOTH the named and the un-named paths.
                mHoveringRivalId = mpGuiCache->GetLocalPlayerCarId();
            }
        }

        // The console re-tests the return value rather than folding this into the branch
        // above; the three hover slots are cleared only when the cursor actually moved.
        if (lbPlacedOnPlayer)
        {
            muHoveredEventID    = 0;
            mHoveredDriveThruID = 0;
            mpLockedIconName    = 0;
        }

        return lbPlacedOnPlayer;
    }

    // ================================================================================
    //  CrashNavMap::SetFilterFromPanel  @ 0x824CC0E0  (DWARF cpp:1574)
    //
    //  Re-key the whole map from whatever sub-panel the crash-nav panel is showing: which
    //  icon classes are drawn, which are selectable, which icon type the locked-icon record
    //  reports, and -- for the events panel -- the game-mode filter the icon renderer is
    //  given. Every arm drops the route tracker, zeroes the manager's used-icon count and
    //  clears the hovered / inspected event, then the shared tail pushes the five flags at
    //  the manager and repaints the button prompts.
    //
    //  Callers: SetupComponents (wJ_02), and Update (wJ_08) on both the one-shot
    //  component set-up and every frame the panel reports its filter changed.
    // ================================================================================
    void CrashNavMap::SetFilterFromPanel()
    {
        // `lwz r27, 0x770(r31)` == mCrashNavPanel (state +0x6E0) + 0x90 == mePanelType,
        // which is what the header-inline GetPanelActiveFilterMode returns.
        const CrashNavPanel::PanelType lePanelType = mCrashNavPanel.GetPanelActiveFilterMode();

        switch (lePanelType)
        {
        case CrashNavPanel::E_PANEL_EVENT:
            mbDrawDriveThrus   = true;    // stb 1, +0x6082
            mbUseRoadSigns     = false;   // stb 0, +0x6081
            mbSelectRivals     = false;   // stb 0, +0x6080
            mbSelectDriveThrus = true;    // stb 1, +0x6083

            {
                // The only arm that publishes a filter event. GuiEventFilterEventIcons is
                // wire id 557 with a FOUR-BYTE payload (the homed catalogue type models it
                // as `u8 maData[4]`), and the X360 stores the game-mode word straight into
                // it (`stw r3, var_50` @0x824CC148) before handing the object to
                // OutputViewState. Copied in by name rather than by a fork of the payload
                // type; FLAG: the catalogue entry has no field names.
                GuiEventFilterEventIcons lFilterEvent;
                const s32 liGameModeType =
                    static_cast<s32>(mCrashNavPanel.GetPanelActiveGameModeType());
                std::memcpy(lFilterEvent.maData, &liGameModeType, sizeof(liGameModeType));
                mpStateInterface->OutputViewState(lFilterEvent);
            }

            // Direct member writes through `friend struct CrashNavMap` -- the DWARF's
            // setters for this cluster (SetSelectedCheckpointInMenu, SetSelectedJunctionID,
            // SetShowingCrashNavRouteInMenu, SetIconFilter) are all inline one-liners the
            // compiler folded, so none of them has a body to link against.
            mpIconManager->miSelectedCheckpoint     = 0;   // stwx 0, +0xAA14
            mpIconManager->muSelectedJunctionID     = 0;   // stwx 0, +0xAA10
            mpIconManager->mbShowingCrashNavRoute   = false;  // stbx 0, +0xAA21
            mpIconManager->meIconFilterMode         = MapIconManager::E_ICONFILTER_ALL;  // stwx 0, +0xAA08

            meEventIconDisplayType = GuiEventEnableSatNavIcons::E_ICON_DISPLAY_TYPE_OFFLINE_EVENTS;  // stw 0, +0x6084

            mpGuiCache->GetGuiTracker()->ClearTracker();
            mpIconManager->miNumUsedIcons = 0;            // stw 0, +0x990

            muHoveredEventID    = 0;                       // stw 0, +0x6094
            muInspectingEventID = 0;                       // stw 0, +0x609C
            mLockedIconInfo.SetIconType(
                GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_JUNCTION);   // stb 5, +0x6128
            break;

        case CrashNavPanel::E_PANEL_DRIVETHRU:
            mbDrawDriveThrus   = true;    // stb 1, +0x6082
            mbUseRoadSigns     = false;
            mbSelectRivals     = false;
            mbSelectDriveThrus = true;
            meEventIconDisplayType =
                GuiEventEnableSatNavIcons::E_ICON_DISPLAY_TYPE_COUNT;   // stw 5, +0x6084

            mpGuiCache->GetGuiTracker()->ClearTracker();
            mpIconManager->miNumUsedIcons = 0;

            muHoveredEventID    = 0;
            muInspectingEventID = 0;
            mLockedIconInfo.SetIconType(
                GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_JUNKYARD);   // stb 7, +0x6128
            mpIconManager->meIconFilterMode = MapIconManager::E_ICONFILTER_ALL;
            break;

        case CrashNavPanel::E_PANEL_ROADSIGN:
            mbDrawDriveThrus   = false;   // stb 0, +0x6082
            mbUseRoadSigns     = true;    // stb 1, +0x6081
            mbSelectRivals     = false;
            mbSelectDriveThrus = false;
            meEventIconDisplayType =
                GuiEventEnableSatNavIcons::E_ICON_DISPLAY_TYPE_COUNT;

            mpGuiCache->GetGuiTracker()->ClearTracker();
            mpIconManager->miNumUsedIcons = 0;

            muHoveredEventID    = 0;
            muInspectingEventID = 0;
            mLockedIconInfo.SetIconType(
                GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_ROADSIGN);   // stb 13, +0x6128

            // The ONE arm that narrows the filter: `stwx 8, mpIconManager, 0xAA08`. FLAG
            // consumer-named -- MapIconManager::IconFilterMode has no enumerator recovered
            // for 8, so the literal stands with its offset rather than a fabricated name.
            mpIconManager->meIconFilterMode =
                static_cast<MapIconManager::IconFilterMode>(8);
            mpIconManager->mbShowingCrashNavRoute = false;   // stbx 0, +0xAA21

            // The road-sign sub-manager is embedded at manager+0x7090 and private; reached
            // through MapIconManager's `friend struct CrashNavMap` for the same reason as
            // the flag tail above. GetPanelActiveRoadRuleType is the panel's inlined
            // RoadPanel::GetCurrentRule (BrnStreetData::ScoreType in the DWARF, kept s32 by
            // the committed CrashNavPanel header).
            mpIconManager->mRoadSignIconManager.SetRoadIconFilter(
                mCrashNavPanel.GetPanelActiveRoadRuleType());
            break;

        case CrashNavPanel::E_PANEL_RIVALS:
            mbSelectRivals     = true;    // stb 1, +0x6080
            mbUseRoadSigns     = false;
            mbDrawDriveThrus   = true;    // stb 1, +0x6082
            mbSelectDriveThrus = false;
            meEventIconDisplayType =
                GuiEventEnableSatNavIcons::E_ICON_DISPLAY_TYPE_COUNT;

            mpGuiCache->GetGuiTracker()->ClearTracker();
            mpIconManager->miNumUsedIcons = 0;

            muHoveredEventID    = 0;
            muInspectingEventID = 0;
            mLockedIconInfo.SetIconType(
                GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_RIVAL);   // stb 3, +0x6128
            mpIconManager->meIconFilterMode = MapIconManager::E_ICONFILTER_ALL;
            break;

        default:
            {
                // The X360 composes this into CgsDev::Assert::gpcMessageBuffer through a
                // StrStream (Begin first, then the two `operator<<` calls with the panel
                // type between them), then fires it. Non-fatal -- the shared tail runs
                // regardless, which is why the switch has no early return.
                CgsDev::Assert::BeginAssert();
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "Unknown selection in panel (" << static_cast<s32>(lePanelType)
                           << ") \n";
                CgsDev::Assert::FireAssert(lacMessage, KAC_ASSERT_FILE, 1720);   // li r5, 0x6B8
                CgsDev::Assert::EndAssert();
            }
            break;
        }

        // ---- the shared tail (0x824CC390..0x824CC3E8) -------------------------------
        // SetUseEventIcons' third parameter is a FLOAT and travels in f1; on the PPC ABI it
        // SKIPS its GPR slot, which is exactly why r6 is never written before the call and
        // why Hex-Rays invents a garbage `a4` there. The DWARF row settles the shape:
        // `void SetUseEventIcons(GuiEventDrawEventIcons::EIconDisplayType, StateInterface*,
        //  float32_t, uint32_t*, int32_t)` -- so the observed r7 == 0 and r8 == 0 are the
        // last two arguments and flt_82065668 == 0.5f is the float.
        // [DIAG] NOT IN THE X360 BINARY -- [cnav-diag] the resolved filter.
        if (getenv("BRN_SATNAV_DIAG") != 0 && CgsDev::Log::gpDebugPrint != 0)
            *CgsDev::Log::gpDebugPrint << "[cnav-diag] SetFilterFromPanel: panel=" << static_cast<s32>(lePanelType)
                << " displayType=" << static_cast<s32>(meEventIconDisplayType)
                << " iconFilter=" << static_cast<s32>(mpIconManager->meIconFilterMode)
                << " roadSigns=" << (mbUseRoadSigns ? 1 : 0) << " driveThrus=" << (mbDrawDriveThrus ? 1 : 0) << "\n";
        mpIconManager->SetUseEventIcons(
            static_cast<GuiEventDrawEventIcons::EIconDisplayType>(meEventIconDisplayType),
            mpStateInterface, 0.5f, 0, 0);
        mpIconManager->SetUseRoadSigns(mbUseRoadSigns, mpStateInterface);
        mpIconManager->SetShowDrivethrus(mbDrawDriveThrus);
        mpIconManager->SetAllowRivalSelection(mbSelectRivals);
        mpIconManager->SetAllowDriveThruSelection(mbSelectDriveThrus);

        UpdateButtonPrompts();
    }
}

// ============================================================================
// FOLDED FROM BrnCrashNavMap_wJ_01.cpp (wave J) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===================================================================================
// BrnGui::CrashNavMap -- wave-J partfile 01: the three panel-repaint switches.
//   UpdateEventInfoPanel @0x824B73B0  (cpp:2091 assert site)
//   UpdateDrivethru      @0x824B7158  (cpp:2000 assert site)
//   UpdateRival          @0x824B7258  (cpp:2047 assert site)
//
// All three bodies are landed. Every arm of all three calls a BrnGui::CrashNavPanel
// setter; those five declarations were filed as wave-J shared-header requests and have
// since been applied, so the bodies below compile against the committed
// b5-decomp/src/GameSource/Gui/Flow/Screen/Components/BrnCrashNavPanel.h:
//
//     void SetRivalPanelData();                                              // @0x8243AAC8
//     void SetRivalPanelData(CgsID lRivalCarId);                             // @0x8243AB68
//     void SetRivalPanelData(const CgsNetwork::PlayerName*, CgsID);          // @0x8243ABF0
//     void SetDrivethruPanelData(CgsID lDriveThruId);                        // @0x8243A8F0
//     void SetEventPanelData(u32, const ChallengedEventScore*, bool);        // @0x8243A878
//
// The last one takes THREE parameters on X360 where the PS3 DWARF has two. Its middle
// pointer is the score-override record wave-J group 8 measured (20 bytes: an s32 override
// word plus 16 reserved bytes; only word 0 is ever read, by EventPanel::SetEventData
// @0x82430D70) -- the three functions here all pass it as NULL.
//
// Compile-verified with the group's include set; the enum spellings, the out-of-line
// GuiEventUpdateSatNav::SatNavIconInfo::GetIconType() call and the CgsDev::StrStream
// assert chain all check out.
// ===================================================================================


namespace BrnGui
{
    // ----------------------------------------------------- UpdateDrivethru @0x824B7158
    //
    // Repaint the crash-nav panel while the screen is in its DRIVE-THROUGH sub-mode. Same
    // shape as UpdateEventInfoPanel minus the event arm -- and note the X360 compiles this
    // one as a plain if/else chain, NOT a jump table.
    //
    // Read from the asm rather than the pseudocode:
    //  * `addi r28, r31, 0x6100` + `bl BrnGui__GuiEventUpdateSatNav__SatNavI` is an
    //    out-of-line call on mLockedIconInfo (X360 +24832 == 0x6100). SatNavI @0x823A6B30
    //    is SatNavIconInfo::GetIconType(): it sign-extends the icon-type byte @+0x28 and
    //    fires the two range asserts (BrnGuiEventTypeDefs.h:1911/1912) before returning.
    //    It is a real `bl`, so the inline GetIconTypeByte() accessor would DROP those
    //    asserts -- call GetIconType().
    //  * `addi r3, r31, 0x6E0` on both panel arms is &mCrashNavPanel (X360 +1760); the
    //    Hex-Rays rendering drops that implicit `this` on the drive-through arm.
    //  * `ld r4, 0x60A8(r31)` = mHoveredDriveThruID (X360 +24744), one 8-byte CgsID in one
    //    register.
    //  * `addi r11, r3, -7` + `cmplwi r11, 5` + `bgt` is the compiler's strength-reduced
    //    form of the range test 7 <= type <= 12; de-optimised back to the written range
    //    below (AGENTS.md "strength reduction reversal"). The two forms agree for every
    //    value GetIconType() can return, because it asserts the type is >= 0.
    //
    // X360-LITERAL AUDIT: 0x6100 / 0x6E0 / 0x60A8 are the console offsets of
    // mLockedIconInfo / mCrashNavPanel / mHoveredDriveThruID. None is reproduced as a
    // number below -- every access is by member name, console values in comments only.
    void CrashNavMap::UpdateDrivethru()
    {
        const GuiEventUpdateSatNav::SatNavIconInfo::SatNavIconType leIconType =
            mLockedIconInfo.GetIconType();

        if (leIconType == GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_PLAYER_CAR)
        {
            // The local player's own car icon -> the "player" face of the rival panel.
            mCrashNavPanel.SetRivalPanelData();
        }
        else if (leIconType >= GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_JUNKYARD &&
                 leIconType <= GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_TIRE_SHOP)
        {
            // Any of the six drive-through / junkyard shop icons (types 7..12).
            mCrashNavPanel.SetDrivethruPanelData(mHoveredDriveThruID);
        }
        else
        {
            // Non-fatal, and the panel is left untouched. The X360 opens the assert FIRST
            // and only then builds the message, because it streams into the mutex-guarded
            // global CgsDev::Assert::gpcMessageBuffer; the committed convention
            // (BrnArbStateDriveThru.cpp) uses a stack buffer, so the ordering is cosmetic
            // here and is kept to match the asm.
            CgsDev::Assert::BeginAssert();

            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);

            // The X360 re-reads the icon type here (a second `bl SatNavI` at 0x824B7200)
            // rather than reusing leIconType -- kept. The three string literals are
            // verbatim rodata; there is no space before "while", so the composed message
            // really does read "...of type 3while updating drivethrus".
            lStream << "Should not be snapped to an icon of type "
                    << static_cast<s32>(mLockedIconInfo.GetIconType())
                    << "while updating drivethrus\n";

            CgsDev::Assert::FireAssert(lacMessageBuffer,
                                       "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnCrashNavMap.cpp",
                                       2000);   // `li r5, 0x7D0`
            CgsDev::Assert::EndAssert();
        }
    }
}

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT machinery
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStream (streamed assert)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // SatNavIconInfo::SatNavIconType

namespace BrnGui
{
    // ------------------------------------------------- UpdateEventInfoPanel @0x824B73B0
    //
    // Repaint the crash-nav panel for whatever the cursor is currently snapped to while the
    // screen is in its EVENT sub-mode. One jump-table switch on the locked icon's type.
    //
    // Read from the asm rather than the pseudocode:
    //  * `addi r28, r31, 0x6100` then `bl BrnGui__GuiEventUpdateSatNav__SatNavI` is an
    //    out-of-line call on mLockedIconInfo (X360 +24832 == 0x6100). SatNavI @0x823A6B30
    //    is SatNavIconInfo::GetIconType(): it sign-extends the icon-type byte @+0x28 and
    //    fires two range asserts ("leIconType >= 0" / "leIconType < E_SATNAVICON_MAX",
    //    BrnGuiEventTypeDefs.h:1911/1912) before returning it. It is a real `bl`, so the
    //    inline GetIconTypeByte() accessor would DROP those asserts -- call GetIconType().
    //  * `addi r3, r31, 0x6E0` on every panel arm is &mCrashNavPanel (X360 +1760 == 0x6E0);
    //    the Hex-Rays rendering drops that implicit `this` on the two-argument arms.
    //  * case 5 loads `lwz r4, 0x6094(r31)` = muHoveredEventID (X360 +24724), `li r5, 0`
    //    (the null score-override pointer) and `li r6, 1` (lbEventsToChooseFrom = true).
    //  * cases 7-12 load `ld r4, 0x60A8(r31)` = mHoveredDriveThruID (X360 +24744) -- an
    //    8-byte CgsID in ONE register, hence the single-argument call.
    //  * the switch is `cmplwi r3, 0xC` + `bgt default`, an UNSIGNED bound, so a negative
    //    icon type also lands in the default arm. C++ switch/default reproduces that.
    //
    // X360-LITERAL AUDIT: 0x6100 / 0x6E0 / 0x6094 / 0x60A8 are the console offsets of
    // mLockedIconInfo / mCrashNavPanel / muHoveredEventID / mHoveredDriveThruID. None of
    // them is reproduced as a number below -- every access is by member name and the
    // console values live in these comments only.
    void CrashNavMap::UpdateEventInfoPanel()
    {
        switch (mLockedIconInfo.GetIconType())
        {
        // ---- the local player's own car icon -> the "player" face of the rival panel ----
        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_PLAYER_CAR:
            mCrashNavPanel.SetRivalPanelData();
            break;

        // ---- a junction (= an event) icon -> the event panel, no score override ---------
        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_JUNCTION:
            mCrashNavPanel.SetEventPanelData(muHoveredEventID, NULL, true);
            break;

        // ---- any of the six drive-through / junkyard shop icons ------------------------
        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_JUNKYARD:
        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_CAR_PARK:
        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_BODYSHOP:
        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_GAS_STATION:
        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_PAINT_SHOP:
        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_TIRE_SHOP:
            mCrashNavPanel.SetDrivethruPanelData(mHoveredDriveThruID);
            break;

        // ---- every other icon type is a bug in the caller ------------------------------
        default:
            {
                // Non-fatal, and the panel is left untouched (BeginAssert / FireAssert /
                // EndAssert, then straight to the epilogue). The X360 opens the assert
                // FIRST and only then builds the message, because it streams into the
                // mutex-guarded global CgsDev::Assert::gpcMessageBuffer; the committed
                // convention (BrnArbStateDriveThru.cpp) uses a stack buffer instead, so the
                // ordering is cosmetic here and is kept to match the asm.
                CgsDev::Assert::BeginAssert();

                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);

                // The X360 re-reads the icon type here (a second `bl SatNavI` at
                // 0x824B74C4) rather than reusing the switch value -- kept. The three
                // string literals are verbatim rodata; note there is no space before
                // "while", so the composed message really does read "...of type 6while
                // updating events".
                lStream << "Should not be snapped to an icon of type "
                        << static_cast<s32>(mLockedIconInfo.GetIconType())
                        << "while updating events\n";

                CgsDev::Assert::FireAssert(lacMessageBuffer,
                                           "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnCrashNavMap.cpp",
                                           2091);   // `li r5, 0x82B`
                CgsDev::Assert::EndAssert();
            }
            break;
        }
    }
}

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT machinery
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStream (streamed assert)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // SatNavIconInfo::SatNavIconType

namespace BrnGui
{
    // --------------------------------------------------------- UpdateRival @0x824B7258
    //
    // Repaint the crash-nav panel while the screen is in its RIVAL sub-mode: which of the
    // three rival-panel faces to show is decided by the locked icon's type.
    //
    // Read from the asm rather than the pseudocode:
    //  * `addi r28, r31, 0x6100` + `bl BrnGui__GuiEventUpdateSatNav__SatNavI` is an
    //    out-of-line call on mLockedIconInfo (X360 +24832 == 0x6100). SatNavI @0x823A6B30
    //    is SatNavIconInfo::GetIconType(): it sign-extends the icon-type byte @+0x28 and
    //    fires the two range asserts (BrnGuiEventTypeDefs.h:1911/1912) before returning.
    //    It is a real `bl`, so the inline GetIconTypeByte() accessor would DROP those
    //    asserts -- call GetIconType().
    //  * `addi r3, r31, 0x6E0` is &mCrashNavPanel (X360 +1760) on ALL THREE panel arms,
    //    including the two the dossier renders as free functions `sub_8243AB68(...)` /
    //    `sub_8243ABF0(...)` -- Hex-Rays dropped the implicit `this` because those two
    //    targets are unnamed in the IDA database.
    //  * `ld r5/r4, 0x60B8(r31)` = mHoveringRivalId (X360 +24760), one 8-byte CgsID in one
    //    register; the pseudocode's `(*(a1+24760), *(a1+24764))` word pair is an artifact.
    //  * `addi r4, r31, 0x60C8` = &mPlayerName (X360 +24776), passed by pointer.
    //  * case 12 (E_SATNAVICON_TIRE_SHOP) jumps straight to the epilogue at 0x824B73A8 --
    //    a deliberate no-op arm, not a fall-through into the assert. MoveCursor parks type
    //    12 in mLockedIconInfo while the map is panning (spec section 4), which is why the
    //    rival path has to tolerate it silently.
    //  * the switch is `cmplwi r3, 0xC` + `bgt default`, an UNSIGNED bound, so a negative
    //    icon type also lands in the default arm. C++ switch/default reproduces that.
    //
    // X360-LITERAL AUDIT: 0x6100 / 0x6E0 / 0x60B8 / 0x60C8 are the console offsets of
    // mLockedIconInfo / mCrashNavPanel / mHoveringRivalId / mPlayerName. None is reproduced
    // as a number below -- every access is by member name, console values in comments only.
    void CrashNavMap::UpdateRival()
    {
        switch (mLockedIconInfo.GetIconType())
        {
        // ---- the local player's own car icon -> the "player" face of the panel ---------
        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_PLAYER_CAR:
            mCrashNavPanel.SetRivalPanelData();
            break;

        // ---- a networked rival: the panel gets the cached lobby name as well -----------
        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_NETWORKRIVAL:
            mCrashNavPanel.SetRivalPanelData(&mPlayerName, mHoveringRivalId);
            break;

        // ---- an offline rival: the car id alone identifies it --------------------------
        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_RIVAL:
            mCrashNavPanel.SetRivalPanelData(mHoveringRivalId);
            break;

        // ---- the panning placeholder type: leave the panel exactly as it is ------------
        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_TIRE_SHOP:
            break;

        // ---- every other icon type is a bug in the caller ------------------------------
        default:
            {
                // Non-fatal, and the panel is left untouched. The X360 opens the assert
                // FIRST and only then builds the message, because it streams into the
                // mutex-guarded global CgsDev::Assert::gpcMessageBuffer; the committed
                // convention (BrnArbStateDriveThru.cpp) uses a stack buffer, so the
                // ordering is cosmetic here and is kept to match the asm.
                CgsDev::Assert::BeginAssert();

                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);

                // The X360 re-reads the icon type here (a second `bl SatNavI` at
                // 0x824B7368) rather than reusing the switch value -- kept. The three
                // string literals are verbatim rodata; there is no space before "while",
                // so the composed message really does read "...of type 4while updating
                // rivals".
                lStream << "Should not be snapped to an icon of type "
                        << static_cast<s32>(mLockedIconInfo.GetIconType())
                        << "while updating rivals\n";

                CgsDev::Assert::FireAssert(lacMessageBuffer,
                                           "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnCrashNavMap.cpp",
                                           2047);   // `li r5, 0x7FF`
                CgsDev::Assert::EndAssert();
            }
            break;
        }
    }
}

// ============================================================================
// FOLDED FROM BrnCrashNavMap_wJ_02.cpp (wave J) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===================================================================================
// BrnGui::CrashNavMap -- wave-J partfile 02: the apt-component registration pair and the
// icon-manager (re)acquisition.
//   AppendExpectedAptComponents @0x824B67E0  (cpp:537,  virtual, vtable slot +44)
//   SetupComponents             @0x824D8C40  (cpp:556,  virtual, vtable slot +48)
//   ResetIconManager            @0x824B6BE0  (cpp:1408, Update's id-64 arm)
//
// All three bodies are landed, read store-for-store off the raw X360 assembly with
// Hex-Rays arbitrated against it. The shared-header declarations this group filed requests
// for -- GuiCache::AppendExpectedAptComponentList / GetMapIconManager, MapIconManager::
// AppendExpectedAptComponents / SetupComponent / SetOwnerParameters and its
// meIconFilterMode / mbRotateSatNav / meIconSizeMode members (reached through
// `friend struct CrashNavMap`), CrashNavPanel::AppendExpectedAptComponents / SetupComponent
// and GuiEventDrawEventIcons::EIconDisplayType -- have all since been applied, so the
// bodies below spell them as the committed headers do.
//
// The nine-parameter SetOwnerParameters signature is verbatim from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Gui/SatNav/BrnMapIconManager.h:191) and matches
// the X360 call site argument for argument, including the two stack parameters Hex-Rays
// dropped. GameSource/Gui/Flow/PreEvent/States/BrnPreRaceFlyBy_wJ_02.cpp is the other
// consumer of GetMapIconManager / SetOwnerParameters / E_ICON_DISPLAY_TYPE_COUNT; it is
// landed and compiles (selfcheck STATUS=pass, 2026-08-03). An earlier revision of this
// banner claimed that file "does not compile today either" -- that was wrong when written
// (the file was then a comment-only stub) and is wrong now; the claim is retracted, along
// with the cross-file ledger warning it was attached to.
//
// Everything else in the reconstruction -- the enum spellings, the GameStateModuleIO
// game-mode compares, the CgsDev::StrStream assert chain, the gpDebugPrint stream chain,
// the id-64 payload view and the GuiComponent::GetName() calls -- checks out clean under
// the compile gate.
// ===================================================================================


namespace BrnGui
{
    namespace
    {
        // ---- shared group-2 file statics (include ONCE when consolidating) ------------

        // The log-category bit every debug print in this TU is gated on
        // (`ld` + `clrldi r11,r11,63`).
        const u64 KX_MESSAGE_FILTER_BIT = 1;

        // The crash-nav map's per-icon apt component budget (X360 `li r6,0x32` at both
        // 0x824B67F8 and 0x824B6D60).
        const s32 KI_CRASHNAVMAP_NUMICONS = 50;

    }

    // X360 .bss @0x82FB4890 (DWARF BrnCrashNavMap.cpp:45, uint32_t[50]). Construct fills it
    // with CalculateHash(SPrintf("%s%d", macSatNavIconBaseName, i)) for i in [0, 50).
    //
    // DECLARED, NOT DEFINED, HERE. The single object lives in the partfile that owns
    // Construct (BrnCrashNavMap_wJ_04.cpp); wave J lands the partfiles as separate
    // translation units, so a second (anonymous-namespace) definition here would be a
    // private, never-written copy and this function would register 50 zeros.
    extern u32 mauComponentHashIds[];

    // ================================================================================
    //  AppendExpectedAptComponents  @ 0x824B67E0  (cpp:537, vtable slot +44)
    //
    //  Tell the cache every apt component this screen is waiting on before it will call
    //  itself loaded: the 50 sat-nav icon clips (as one pre-hashed list), whatever the
    //  shared icon manager and the crash-nav panel need, and the screen's own two
    //  animation clips.
    // ================================================================================
    void CrashNavMap::AppendExpectedAptComponents()
    {
        // The 50 hashes Construct precomputed ("SatNavIcon0".."SatNavIcon49"). This runs
        // BEFORE the mpIconManager assert on the console; the order is kept.
        mpGuiCache->AppendExpectedAptComponentList(E_GUIFLOW_SCREEN, mauComponentHashIds,
                                                   KI_CRASHNAVMAP_NUMICONS);
        // [DIAG] NOT IN THE X360 BINARY -- [cnav-diag] the expected icon hash range.
        if (getenv("BRN_SATNAV_DIAG") != 0 && CgsDev::Log::gpDebugPrint != 0)
            *CgsDev::Log::gpDebugPrint << "[cnav-diag] expected SatNavIcon0 hash " << mauComponentHashIds[0]
                << " .. SatNavIcon49 hash " << mauComponentHashIds[KI_CRASHNAVMAP_NUMICONS - 1]
                << " anims '" << mTitleButtonsAnimation.GetName() << "' '" << mButtonPromptsAnimation.GetName() << "'\n";

        // cpp:537 -- non-fatal on the X360; the manager is dereferenced either way,
        // exactly as the console does.
        CGS_ASSERT(mpIconManager != 0, "mpIconManager");

        if ((CgsDev::Message::gxMessageFilterFlags & KX_MESSAGE_FILTER_BIT) != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "MAPICONMANAGER: CrashNavMap is calling AppendExpectedAptComponents.\n";
        }

        mpIconManager->AppendExpectedAptComponents();
        mCrashNavPanel.AppendExpectedAptComponents(E_GUIFLOW_SCREEN, mpGuiCache);

        // X360 `addi r5, r31, 0x5F58` / `0x5FE4` == the two AnimationComponents' macName
        // buffers (component +4), i.e. the components' own names.
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                               mTitleButtonsAnimation.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                               mButtonPromptsAnimation.GetName());
    }
}

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT / Begin/Fire/EndAssert
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStream (the streamed assert)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event
#include "GameSource/GameState/BrnGameStateSharedIO.h"                    // GSM::EGameModeType
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiEventDrawEventIcons
#include "GameSource/Gui/SatNav/BrnMapIconManager.h"                      // BrnGui::MapIconManager

namespace BrnGui
{
    namespace GSM = BrnGameState::GameStateModuleIO;

    namespace
    {
        // ---- shared group-2 file statics (keep ONE copy when consolidating) ------------

        // TU static, rodata (DWARF macSatNavIconBaseName): the stem the 50 per-icon apt
        // component names are built from, and the base name the manager is handed.
        const char macSatNavIconBaseName[] = "SatNavIcon";

// (fold: an identical definition of KAC_ASSERT_FILE was dropped here -- this TU defines it once, above)
    }

    // ================================================================================
    //  ResetIconManager  @ 0x824B6BE0  (cpp:1408)
    //
    //  Update's id-64 (GuiCache arrived) arm: latch the shared MapIconManager out of the
    //  cache that just turned up and (re)claim it for the crash-nav map, handing it this
    //  screen's icon-clip base name, capacity, selection policy and -- only while no real
    //  game mode is running -- its event-icon display set. When the manager actually
    //  changes hands, reset the three display knobs the previous owner may have moved.
    // ================================================================================
    void CrashNavMap::ResetIconManager(const CgsModule::Event* lpEvent)
    {
        // The in-queue hands the state the HEADER-STRIPPED payload; id 64's payload is a
        // bare GuiCache pointer (X360 `lwz r11,0(r26)`).
        struct GuiCachePayload : public CgsModule::Event
        {
            GuiCache* mpGuiCache;   // +0x00
        };
        const GuiCachePayload* lpPayload = static_cast<const GuiCachePayload*>(lpEvent);

        if (lpPayload->mpGuiCache == 0)
        {
            // Streamed diagnostic: the X360 composes it into CgsDev::Assert::
            // gpcMessageBuffer through a StrStream, then fires it (Begin first, exactly as
            // ordered here). Non-fatal -- the payload cache is dereferenced below anyway.
            CgsDev::Assert::BeginAssert();
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Invalid cache in GenericHudState::Update";
            CgsDev::Assert::FireAssert(lacMessage, KAC_ASSERT_FILE, 1408);
            CgsDev::Assert::EndAssert();
        }

        // The X360 re-reads the PAYLOAD's cache here, and stores the manager before it
        // tests it.
        mpIconManager = lpPayload->mpGuiCache->GetMapIconManager();
        CGS_ASSERT(mpIconManager != 0, "mpIconManager");   // cpp:1411 (non-fatal)

        // ...whereas the display-set gate reads the STATE's own cache (`lwz r11,0x48`),
        // which Update latched from this same event just before calling in.
        GuiEventDrawEventIcons::EIconDisplayType leIconDisplayType =
            static_cast<GuiEventDrawEventIcons::EIconDisplayType>(meEventIconDisplayType);

        const s32 liGameMode = mpGuiCache->GetGameMode();
        if (liGameMode != GSM::E_MODE_NONE &&
            liGameMode != GSM::E_MODE_ONLINE_FREE_BURN_LOBBY)
        {
            // Any actually-running mode gets the one-past-the-end sentinel, i.e. no
            // event-icon display set at all.
            leIconDisplayType = GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_COUNT;
        }

        const u32 luPreviousOwnerId = mIconManagerOwnerId;

        if ((CgsDev::Message::gxMessageFilterFlags & KX_MESSAGE_FILTER_BIT) != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "MAPICONMANAGER: CrashNavMap is calling SetOwnerParameters with OwnerID "
                << static_cast<s32>(MapIconManager::E_CRASHNAV_MAP)
                << "(E_CRASHNAV_MAP).\n";
        }

        // The manager hands back the owner id it actually granted, which is what the
        // release path later quotes -- hence the assignment through the call.
        mIconManagerOwnerId = mpIconManager->SetOwnerParameters(
            mpStateInterface,
            macSatNavIconBaseName,              // X360 "SatNavIcon"
            KI_CRASHNAVMAP_NUMICONS,            // X360 `li r6,0x32` == 50
            MapIconManager::E_CRASHNAV_MAP,     // X360 `li r7,2`
            mbUseRoadSigns,                     // X360 lbz +0x6081
            mbDrawDriveThrus,                   // X360 lbz +0x6082
            mbSelectDriveThrus,                 // X360 lbz +0x6083
            leIconDisplayType,                  // X360 stack parameter 8 (sp+0x54)
            0);                                 // X360 stack parameter 9 (sp+0x5C) == NULL

        if (mIconManagerOwnerId != luPreviousOwnerId)
        {
            // Console stores in this order: stb +0xAA1C, stw +0xAA08, stw +0xAA04. There is
            // no setter for any of the three -- the state writes them directly.
            mpIconManager->mbRotateSatNav   = false;
            mpIconManager->meIconFilterMode = MapIconManager::E_ICONFILTER_ALL;
            mpIconManager->meIconSizeMode   = MapIconManager::E_ICONSIZE_LARGE;
        }
    }
}

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameSource/Gui/SatNav/BrnMapIconManager.h"                      // BrnGui::MapIconManager

namespace BrnGui
{
    // ================================================================================
    //  SetupComponents  @ 0x824D8C40  (cpp:556, vtable slot +48)
    //
    //  Once the cache reports every expected component initialised, hand the freshly
    //  created apt objects to the pieces that drive them, then re-apply the panel's
    //  current icon filter to the manager.
    // ================================================================================
    void CrashNavMap::SetupComponents()
    {
        // cpp:556 -- non-fatal on the X360 (the manager is dereferenced regardless).
        CGS_ASSERT(mpIconManager != 0, "mpIconManager");

        mpIconManager->SetupComponent();
        mCrashNavPanel.SetupComponent();
        SetFilterFromPanel();
    }
}

// ============================================================================
// FOLDED FROM BrnCrashNavMap_wJ_03.cpp (wave J) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===================================================================================
// BrnGui::CrashNavMap -- wave-J partfile 03: the map-scroll sound debouncer.
//
// Group 3 of the wave-J CrashNavMap keystone is
//   CheckForLoadComplete @0x824CB660  (BrnCrashNavMap.cpp:576 assert)
//   UpdateSoundEvents    @0x824CB8B0
//   UpdateCursorStatus   @0x824CB770
//
// All three bodies are landed; the shared-header declarations they waited on have since
// been applied.
//
// The DecFIGS DWARF splits the map-scroll sound debouncer out of UpdateSoundEvents as its
// own struct, CrashNavMapSoundData, with Construct/Prepare/Update (BrnCrashNavMap.h:61/70/
// 80), and the X360 folded all three inline. UpdateSoundEvents below therefore calls them
// by name rather than reproducing the folded code. NONE of the three has a body anywhere
// in b5-decomp/src yet -- they are declared in the owning BrnCrashNavMap.h and are
// LINK-TIME EXTERNALS reported by this wave, not defined here and not defined by partfile
// 04 either (an earlier revision of both banners claimed one file or the other had landed
// them; neither had).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX @0x824CB660 / @0x824CB770 / @0x824CB8B0 (the
// per-address assembly under .ida-exports/BURNOUT_X360_ARTIST.XEX/). X360 offsets appear
// only in comments; the host layout is name-based.
// ===================================================================================


namespace BrnGui
{
    namespace
    {
// (fold: an identical definition of KI_CHANNEL_VIEW_STATE was dropped here -- this TU defines it once, above)
// (fold: an identical definition of KI_CHANNEL_GUI_INTERNAL was dropped here -- this TU defines it once, above)

        // The apt movie this screen plays once its resources are in, and the level the
        // X360 passes with it (`li r5, 3`; same KI_APT_MOVIE_LEVEL the wave-H
        // OnlineGameRoomPlayerInfo partfile 02 uses).
        const char KPC_CRASHNAV_APT_MOVIE[] = "BrnCrashNavMapMain";   // X360 off_82F27AF0
// (fold: an identical definition of KI_APT_MOVIE_LEVEL was dropped here -- this TU defines it once, above)

        // The float in the ShowHideSatNav payload. MEASURED: X360 flt_82065668 == 0.5f
        // (headless IDA dump, scratchpad/waveJ/prfb_init.txt line 146 and
        // scratchpad/waveJ/g03_flt.txt). FLAG consumer-named -- the wave-J spec calls it
        // KF_MAP_FADE_IN_TIME; the rodata word carries no DWARF symbol of its own.
        const f32 KF_MAP_FADE_IN_TIME = 0.5f;

// (fold: an identical definition of GuiEventShowHideSatNavPayload was dropped here -- this TU defines it once, above)
    }

    // --------------------------------------------- CheckForLoadComplete @ 0x824CB660
    // Per-frame poll while the screen is coming up: once the cache reports every one of
    // this screen's seven resources loaded, start the screen's apt movie, re-arm the
    // expected-component bookkeeping for the component set the derived screen declares,
    // and tell the view and internal channels to bring the sat-nav up (a half-second
    // fade). Runs once -- mbIsScreenLoaded latches it -- and never while the screen is
    // already tearing down.
    void CrashNavMap::CheckForLoadComplete()
    {
        // cpp:576 -- non-fatal on the X360; the cache pointer is dereferenced either way
        // below.
        CGS_ASSERT(mpGuiCache, "mpGuiCache");

        if (mbIsScreenLoaded || mbIsExiting)
        {
            return;
        }

        if (!mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
        {
            return;
        }

        mpStateInterface->PlayAptMovie(KPC_CRASHNAV_APT_MOVIE, KI_APT_MOVIE_LEVEL);

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
        mpGuiCache->ClearExpectedControlledAptComponentList();

        // Virtual, vtable byte +0x2C: the derived screen appends its own component set.
        AppendExpectedAptComponents();

        mbItemsLoaded    = false;
        mbIsScreenLoaded = true;

        // The X360 stack-builds the OutputViewState / OutputInternalState wrapper record
        // inline: { payload size 12, type 213, payload offset 12, payload } posted at 24
        // bytes on channel 41 and then again on channel 42. Sizes are host sizeof
        // expressions, never the console's baked 12 / 24 immediates.
        CgsGui::GuiStackEventQueue::GuiEventQueueLarge* lpOutQueue =
            mpStateInterface->GetOutputEventQueue();

        GuiEventShowHideSatNavPayload lShowHideSatNav(0, KF_MAP_FADE_IN_TIME, true);

        CgsGui::GuiEventWrapper<GuiEventShowHideSatNavPayload, KI_CHANNEL_VIEW_STATE>
            lViewStateRecord(lShowHideSatNav);
        lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lViewStateRecord),
                             lViewStateRecord.GetChannel(),
                             static_cast<s32>(sizeof(lViewStateRecord)));

        CgsGui::GuiEventWrapper<GuiEventShowHideSatNavPayload, KI_CHANNEL_GUI_INTERNAL>
            lInternalStateRecord(lShowHideSatNav);
        lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lInternalStateRecord),
                             lInternalStateRecord.GetChannel(),
                             static_cast<s32>(sizeof(lInternalStateRecord)));
    }
}

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"

#include "GameSource/Gui/Flow/Screen/Components/BrnCursor.h"   // BrnGui::GuiCursor

namespace BrnGui
{
    // ------------------------------------------------ UpdateCursorStatus @ 0x824CB770
    // Reconcile the cursor with what the icon pass decided is under it. When nothing is
    // hovered any more the cursor drops its icon lock and goes back to the un-snapped
    // display state; either way it is re-activated, this frame's hovered set becomes next
    // frame's "last frame" set, and the cursor publishes itself.
    void CrashNavMap::UpdateCursorStatus()
    {
        // Did any part of the hovered set move since last frame?
        bool lbHoveredSetChanged = false;
        if (muHoveredEventID != muHoveredEventIDLastFrame
            || mHoveredDriveThruID != mHoveredDriveThruIDLastFrame
            || mHoveringRivalId != mHoveringRivalIdLastFrame
            || mpLockedIconName != mpLockedIconNameLastFrame)
        {
            lbHoveredSetChanged = true;
        }

        // Is anything hovered at all? (The X360 tests the local-player flag byte
        // against 1 rather than against zero -- `cmplwi r10, 1` at 0x824CB808.)
        bool lbAnythingHovered = false;
        if (muHoveredEventID != 0
            || mHoveredDriveThruID != 0
            || mHoveringRivalId != 0
            || mpLockedIconName != 0
            || mbLocalPlayerSelected)
        {
            lbAnythingHovered = true;
        }

        // See the BRANCH SHAPE note in the banner: the X360 branches on
        // lbHoveredSetChanged first, but both arms run this same test and share the
        // release block, so the change flag does not affect the outcome.
        (void)lbHoveredSetChanged;

        if (!lbAnythingHovered)
        {
            mCursor.muLockedToIndex = GuiCursor::KU_INVALID_SNAP_INDEX;
            if (mCursor.meDisplayState != GuiCursor::E_DISPLAY_ACTIVE_UNSNAP)
            {
                mCursor.meDisplayState = GuiCursor::E_DISPLAY_ACTIVE_UNSNAP;
            }
        }

        mCursor.SetActive();

        muHoveredEventIDLastFrame    = muHoveredEventID;
        mHoveredDriveThruIDLastFrame = mHoveredDriveThruID;
        mHoveringRivalIdLastFrame    = mHoveringRivalId;
        mpLockedIconNameLastFrame    = mpLockedIconName;

        mCursor.Update();
    }
}

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface (out-queue)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiAudioTriggerEvent
#include "GameSource/Gui/Flow/Screen/Components/BrnCursor.h"              // BrnGui::GuiCursor::GetPosition
#include "GameSource/Gui/SatNav/BrnMainMap.h"                             // MainMapComponent::IsZooming
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"                         // BrnGui::MapTransform

namespace BrnGui
{
    namespace
    {
// (fold: an identical definition of KI_CHANNEL_GUI_OUT was dropped here -- this TU defines it once, above)

// (fold: an identical definition of KI_AUDIO_ACTION_MAP_CUE was dropped here -- this TU defines it once, above)

        // The three sound labels, from the TU's own rodata pointer table. DWARF names and
        // cpp lines are from the wave-J spec section 1 (BrnCrashNavMap.cpp:40-42):
        //   KPC_SOUND_MAP_MOVE_START   X360 off_82F26E74
        //   KPC_SOUND_MAP_SCROLL_START X360 off_82F26E78
        //   KPC_SOUND_MAP_SCROLL_END   X360 off_82F26E7C
        // CONDUCTOR NOTE: these are TU statics, not file-local ones -- the fourth member
        // of the same table, KPC_SOUND_MAP_VIEW_EVENT (off_82F26E80,
        // "CodeMapViewEvent"), is used by group 06's UpdateEvent. Hoist all four into the
        // TU statics block rather than leaving a copy in each partfile.
        const char KPC_SOUND_MAP_MOVE_START[]   = "CodeMapMoveStart";
        const char KPC_SOUND_MAP_SCROLL_START[] = "CodeMapScrollStart";
// (fold: an identical definition of KPC_SOUND_MAP_SCROLL_END was dropped here -- this TU defines it once, above)

// (fold: an identical definition of KU_WIRE_ID_AUDIO_TRIGGER was dropped here -- this TU defines it once, above)

// (fold: an identical definition of GuiAudioTriggerWire was dropped here -- this TU defines it once, above)

        static_assert(sizeof(GuiAudioTriggerWire) == 112, "audio trigger record is 112 bytes");
    }

    // ------------------------------------------------- UpdateSoundEvents @ 0x824CB8B0
    // Edge-trigger the map's move/scroll audio. Whether the map counts as scrolling
    // depends on the cursor mode: while the player is picking icons the caller (the main
    // map update) has already worked it out and passes it in -- unless the map is
    // mid-zoom, during which no scroll cue is wanted -- and while the player is panning
    // it is measured directly from how far the cursor has travelled in world space.
    // The start pair fires on the rising edge and the end cue on the falling edge; the
    // debouncer's latch at the tail is what makes both edges detectable next frame.
    void CrashNavMap::UpdateSoundEvents(bool lbIsScrolling)
    {
        const Vector3 lv3CursorWorldPos = MapTransform::DeviceToWorld(mCursor.GetPosition());

        bool lbIsMapScrolling = false;

        if (meCursorMode == E_CURSORMODE_SELECTING_ICONS)
        {
            if (!mMainMapComponent.IsZooming())
            {
                lbIsMapScrolling = lbIsScrolling;
            }
        }
        else if (meCursorMode == E_CURSORMODE_PANNING)
        {
            lbIsMapScrolling = mSoundData.Prepare(lv3CursorWorldPos);
        }

        if (lbIsMapScrolling)
        {
            if (!mSoundData.mbPrevIsScrolling)
            {
                GuiAudioTriggerWire lMoveStart;
                lMoveStart.Construct(KI_AUDIO_ACTION_MAP_CUE, "", KPC_SOUND_MAP_MOVE_START, "");
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lMoveStart), KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(lMoveStart)));

                GuiAudioTriggerWire lScrollStart;
                lScrollStart.Construct(KI_AUDIO_ACTION_MAP_CUE, "", KPC_SOUND_MAP_SCROLL_START, "");
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lScrollStart), KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(lScrollStart)));
            }
        }
        else if (mSoundData.mbPrevIsScrolling)
        {
            GuiAudioTriggerWire lScrollEnd;
            lScrollEnd.Construct(KI_AUDIO_ACTION_MAP_CUE, "", KPC_SOUND_MAP_SCROLL_END, "");
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lScrollEnd), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lScrollEnd)));
        }

        mSoundData.Update(lv3CursorWorldPos, lbIsMapScrolling);
    }
}

// ============================================================================
// FOLDED FROM BrnCrashNavMap_wJ_04.cpp (wave J) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===================================================================================
// BrnGui::CrashNavMap -- wave-J partfile 04.
//
//   Construct @0x824B6660  (cpp:133 assert site)
//   OnEnter   @0x824CB158
//
// Both bodies are COMPLETE reconstructions walked store-for-store off the raw X360
// assembly (the Hex-Rays listing for OnEnter is badly garbled and was cross-check only),
// and both are landed. The declarations they waited on have since been applied:
// CrashNavPanel::StoreSettings (@0x82418708) and its virtual Construct (@0x82425C60,
// slot 0) with the DWARF's `: public CgsGui::GuiComponent` base; GuiCursor::Construct
// (@0x82416690) / SetAlwaysSnap (@0x82416CA0) / GetPosition / the position-lane writer;
// and GuiEventUpdateSatNav::SatNavIconInfo::SetPositionLane.
//
// CrashNavMapSoundData::Construct is NOT defined here (nor in partfile 03, which owns
// Prepare/Update's call sites): all three are declared in BrnCrashNavMap.h and are
// LINK-TIME EXTERNALS this wave reports rather than defines. An earlier revision of this
// banner said Construct "ships" here -- it does not.
//
// ONE CORRECTION FOR THE WAVE RECORD: the group brief and the Hex-Rays listing both say
// mbUseRoadSigns is set true. The raw asm sets mbUseRoadSigns (+24705) FALSE and
// mbDrawDriveThrus (+24706) TRUE, in both Construct (0x824B66E4/0x824B66E8) and OnEnter
// (0x824CB3C0/0x824CB3C4). The name-to-offset mapping is corroborated independently by
// ResetIconManager @0x824B6D4C..0x824B6D5C, which loads +0x6081/+0x6082/+0x6083 into
// r8/r9/r10 as MapIconManager::SetOwnerParameters' (useRoadSigns, drawDriveThrus,
// selectDriveThrus) argument triple.
// ===================================================================================


namespace BrnGui
{
    namespace
    {
        // DWARF BrnCrashNavMap.h:221 -- the sat-nav icon component count. It is a
        // class-scope constant in the original header; the committed recon header does not
        // declare it, so it is file-local here. (Header finding, not a fabrication: the
        // value 50 is the `li r8, 0x32` of OnEnter and the `li r6, 0x32` of
        // ResetIconManager, and the DWARF prints "= 50".)
        const s32 KAC_CRASHNAVMAP_NUMICONS = 50;

// (fold: an identical definition of macSatNavIconBaseName was dropped here -- this TU defines it once, above)

        // X360 rodata aSD_1, the SPrintf format the loop passes (r5). Hex-Rays mislabels
        // variadic args here; the raw asm is unambiguous: r3=buffer, r4=16, r5="%s%d",
        // r6=macSatNavIconBaseName, r7=the loop index.
        const char KAC_ICON_NAME_FORMAT[5] = "%s%d";

    }

    // DWARF BrnCrashNavMap.cpp:45 -- uint32_t[50], X360 .bss @0x82FB4890 (the loop bound is
    // pinned by dword_82FB4958 - 0x82FB4890 == 0xC8 == 50*4). In the original this is a
    // file-scope object of the single BrnCrashNavMap.cpp, written here by Construct and
    // read by AppendExpectedAptComponents.
    //
    // ONE OBJECT, NOT ONE PER PARTFILE. Wave J lands BrnCrashNavMap.cpp as eight separate
    // translation units, so an anonymous-namespace definition would give each partfile its
    // own private array -- Construct would fill this file's copy and
    // AppendExpectedAptComponents (BrnCrashNavMap_wJ_02.cpp) would register 50 zeros from a
    // different one, and the screen would wait on the wrong expected-component set. It is
    // therefore defined ONCE, here (the partfile that owns Construct), with external
    // linkage; wJ_02 carries the matching `extern` declaration. Fold both back into the
    // anonymous namespace if the partfiles are ever concatenated into one .cpp.
    u32 mauComponentHashIds[KAC_CRASHNAVMAP_NUMICONS];

    // ------------------------------------------------------------- Construct @0x824B6660
    // Bring the crash-nav map screen state up: chain to the GUI state base, put every
    // scalar member in its cold-start value, pre-hash the 50 sat-nav icon component names
    // the apt movie will be asked for, and zero the scroll/sound bookkeeping.
    void CrashNavMap::Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm)
    {
        CGS_ASSERT(lpFsm != 0, "lpFsm");                                   // cpp:133

        CgsGui::State::Construct(liId, lpFsm);

        // The store walk, in raw-asm order (0x824B66B8..0x824B674C).
        meEventIconDisplayType       = GuiEventEnableSatNavIcons::E_ICON_DISPLAY_TYPE_COUNT;  // 5
        mfInspectingEventTime        = 0.0f;                               // flt_82001CC0
        mIconManagerOwnerId          = 0;
        mpIconManager                = 0;
        mbUseRoadSigns               = false;                              // stb 0, +0x6081
        mbDrawDriveThrus             = true;                               // stb 1, +0x6082
        mbSelectDriveThrus           = false;                              // stb 0, +0x6083
        mbItemsLoaded                = false;
        mpLockedIconName             = 0;
        mpLockedIconNameLastFrame    = 0;
        meTitleButtonsState          = E_VISIBLE_ANIMATION_STATES_COUNT;   // 2
        muHoveredEventID             = 0;
        muInspectingEventID          = 0;
        mHoveredDriveThruID          = 0;                                  // std (8-byte CgsID)
        mHoveringRivalId             = 0;                                  // std (8-byte CgsID)
        meCursorMode                 = E_CURSORMODE_NONE;
        meScreenType                 = E_SCREEN_TYPE_OFFLINE;
        meNavigationButtonsState     = E_BUTTON_PROMPT_ANIMATION_STATES_COUNT;  // 12
        mbLocalPlayerSelected        = false;
        muHoveredEventIDLastFrame    = 0;
        mHoveredDriveThruIDLastFrame = 0;
        mHoveringRivalIdLastFrame    = 0;
        mPlayerName.macName[0]       = '\0';                               // stb 0, +0x60C8

        // Pre-hash "SatNavIcon0".."SatNavIcon49" into the TU's component-id table. The
        // X360 measures the string with an inline `while (*p++);` and hands the length
        // (excluding the NUL) to the container CRC-32; CalculateHash takes a mutable
        // char* but never writes, so the buffer is passed straight through.
        for (s32 liIcon = 0; liIcon < KAC_CRASHNAVMAP_NUMICONS; ++liIcon)
        {
            char lacIconName[16];
            CgsCore::SPrintf(lacIconName, sizeof(lacIconName), KAC_ICON_NAME_FORMAT,
                             macSatNavIconBaseName, liIcon);
            mauComponentHashIds[liIcon] = CgsContainers::CgsHash::CalculateHash(
                lacIconName, static_cast<int>(std::strlen(lacIconName)));
        }

        mv2MapScrollVelocity.SetZero();          // stvx128 of a zero quad at +0x6070
        mCrashNavPanel.StoreSettings(true);      // @0x82418708
        mSoundData.Construct();                  // inlined by the X360: zero quad at +0x6130 + zero byte at +0x6140
    }
}

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"

#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEventWrapper
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"       // GuiComponent::Construct
#include "GameSource/Gui/SatNav/BrnMainMap.h"                             // MainMapComponent + bundle
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // EIconDisplayType / SatNavIconInfo
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"                         // MapTransform::DeviceToWorld

namespace BrnGui
{
    namespace
    {
        // DWARF BrnCrashNavMap.h:221 -- see the group-4 Construct park for provenance.

        // DWARF BrnCrashNavMap.cpp:34/:36/:37 (char[10]/[17]/[18]) and cpp:90/:91 -- the
        // apt component names this state constructs. All five are X360 rodata strings.
        const char mCursorName[10]           = "cursor_mc";
        const char macCrashNavPanelName[17]  = "CrashNavPanel_mc";
        const char macCrashNavLegendName[18] = "CrashNavLegend_mc";
        const char KAC_TITLE_BUTTONS_ANIMATION_COMPONENT[22]  = "TitleButtonsAnimation";
        const char KAC_BUTTON_PROMPTS_ANIMATION_COMPONENT[17] = "ButtonsAnimation";

        // The AddEvent channel word (`li r5, 0x28`).
        const s32 KI_CHANNEL_GUI_EVENT_OUT = 40;

        // The reference device rect OnEnter installs on the cursor (cursor +0xD0).
        const f32 KF_CURSOR_RECT_LEFT   = 0.0f;      // flt_82001CC0
        const f32 KF_CURSOR_RECT_TOP    = 0.0f;      // flt_82001CC0
        const f32 KF_CURSOR_RECT_RIGHT  = 1280.0f;   // flt_82066040
        const f32 KF_CURSOR_RECT_BOTTOM = 720.0f;    // flt_8201A7A4

        // The cursor's construction parameters (f1/f2/f3 of GuiCursor::Construct).
        const f32 KF_CURSOR_MOVEMENT_SCALAR = 1.0f;  // flt_82001C98, f30
        const f32 KF_CURSOR_START_X         = 0.0f;  // flt_82001CC0, f31
        const f32 KF_CURSOR_START_Y         = 0.0f;  // flt_82001CC0, f31

        // Out-queue payload view for the wire event this state posts on entry. Wire id 555
        // has no homed type; the record is {payload size 1, id 555, payload offset 12} and
        // the single payload byte is never written by the X360 (no stb targets it), so it
        // is modelled zero-initialised. FLAG consumer-named: neither the field's role nor
        // the event's name is recovered.
        struct GuiEventUnnamed555Payload
        {
            bool mbFlag;   // +0x00 -- left as stack garbage by the X360; posted zeroed here

            GuiEventUnnamed555Payload() : mbFlag(false) {}

            s32 GetEventType() const { return 555; }
        };
    }

    // ---------------------------------------------------------------- OnEnter @0x824CB158
    // Entering the crash-nav map screen: build and construct the main map view, announce
    // the entry on the GUI out-queue, construct the cursor and the four apt components,
    // then reset every piece of per-visit selection/hover state and seed the map-move
    // sound debouncer from the cursor's current world position.
    void CrashNavMap::OnEnter()
    {
        // The X360 assembles this bundle in three stack quads at 0xF0+var_70 and passes
        // its address in r5.
        MainMapComponent::MainMapParameterBundle lParameters;
        lParameters.mv4ViewRect.x    = 0.0f;
        lParameters.mv4ViewRect.y    = 0.16972221f;
        lParameters.mv4ViewRect.z    = 1.0f;
        lParameters.mv4ViewRect.w    = 0.83888888f;
        lParameters.mv4PaddingRect.x = 0.4f;
        lParameters.mv4PaddingRect.y = 0.22805555f;
        lParameters.mv4PaddingRect.z = 0.9375f;
        lParameters.mv4PaddingRect.w = 0.78333336f;
        lParameters.meMapType        = GuiEventRenderMainMap::E_MAPTYPE_MAINMAP;

        mMainMapComponent.Construct(mpStateInterface, &lParameters);
        mMainMapComponent.Prepare();
        mMainMapComponent.SetZoom(MainMapComponent::E_ZOOMFACTOR_MEDIUM, 0.0f, false);
        mMainMapComponent.SetStickMapToScreenEdges(false, false, false, false);

        // The X360 stack-builds the wrapper record inline: { payload size 1, type 555,
        // payload offset 12 } posted at 16 bytes on channel 40.
        GuiEventUnnamed555Payload lEnteredEvent;
        CgsGui::GuiEventWrapper<GuiEventUnnamed555Payload, KI_CHANNEL_GUI_EVENT_OUT>
            lEnteredRecord(lEnteredEvent);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lEnteredRecord),
            lEnteredRecord.GetChannel(),
            static_cast<s32>(sizeof(lEnteredRecord)));

        meCursorMode = E_CURSORMODE_NONE;
        mCursor.Construct(mCursorName, mpStateInterface,
                          KF_CURSOR_MOVEMENT_SCALAR, KF_CURSOR_START_X, KF_CURSOR_START_Y, 0);
        // The X360 does not call a setter here: it INLINES GuiCursor::SetBounds as a single
        // 16-byte store into the cursor's mv4BoundsRect lane -- `li r11, 0x5F30;
        // lvx128 v0, r0, r10; stvx128 v0, r31, r11` @0x824CB2B8..0x824CB2C8, where
        // 0x5F30 == mCursor (state +0x5E60) + 0xD0. The stack quad it loads was filled at
        // 0x824CB198..0x824CB214 with {flt_82001CC0, flt_82001CC0, flt_82066040,
        // flt_8201A7A4} == {0, 0, 1280, 720}, i.e. {left, top, right, bottom}.
        // SetBounds is the DWARF's own name for that writer (BrnCursor.h:212).
        const Vector4 lv4CursorBounds = { KF_CURSOR_RECT_LEFT, KF_CURSOR_RECT_TOP,
                                          KF_CURSOR_RECT_RIGHT, KF_CURSOR_RECT_BOTTOM };
        mCursor.SetBounds(lv4CursorBounds);
        mCursor.SetAlwaysSnap(true);

        // The four apt components, each through vtable slot 0 with a NULL parent name.
        mCrashNavPanel.Construct(macCrashNavPanelName, mpStateInterface, 0);
        mCrashNavLegend.Construct(macCrashNavLegendName, mpStateInterface, 0);
        mTitleButtonsAnimation.Construct(KAC_TITLE_BUTTONS_ANIMATION_COMPONENT, mpStateInterface, 0);
        mButtonPromptsAnimation.Construct(KAC_BUTTON_PROMPTS_ANIMATION_COMPONENT, mpStateInterface, 0);

        // The per-visit reset, in raw-asm store order (0x824CB360..0x824CB420). It repeats
        // most of Construct's cold-start walk plus the members Construct leaves alone.
        mfInspectingEventTime    = 0.0f;                                   // flt_82001CC0
        meNavigationButtonsState = E_BUTTON_PROMPT_ANIMATION_STATES_COUNT; // 12
        meTitleButtonsState      = E_VISIBLE_ANIMATION_STATES_COUNT;       // 2
        miSatNavIconsToLoad      = KAC_CRASHNAVMAP_NUMICONS;               // li r8, 0x32
        mLockedIconInfo.SetIconType(GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_PLAYER_CAR);  // stb 0, +0x6128
        mbItemsLoaded            = false;
        mpGuiCache               = 0;
        meMapState               = E_MAPSTATE_PANEL;
        mbIsCursorLockedToIcon   = false;
        mbIsScreenLoaded         = false;
        mpIconManager            = 0;
        mbSelectRivals           = false;                                  // stb 0, +0x6080
        mbUseRoadSigns           = false;                                  // stb 0, +0x6081
        mbDrawDriveThrus         = true;                                   // stb 1, +0x6082
        mbSelectDriveThrus       = false;                                  // stb 0, +0x6083
        meEventIconDisplayType   = GuiEventEnableSatNavIcons::E_ICON_DISPLAY_TYPE_COUNT;  // 5
        mbIsInEvent              = false;
        mbIsExiting              = false;
        meScreenType             = E_SCREEN_TYPE_OFFLINE;
        mi8CurrentEventIndex     = 0;
        meWaitforData            = E_WAITFOR_NONE;                         // 3
        mpLockedIconName         = 0;
        muHoveredEventID         = 0;
        muInspectingEventID      = 0;
        mLockedIconInfo.SetPositionLane(Vector4());                        // stvx128 zero, +0x6100
        mHoveredDriveThruID      = 0;
        mHoveringRivalId         = 0;
        mbLocalPlayerSelected    = false;
        mPlayerName.macName[0]   = '\0';
        mv2MapScrollVelocity.SetZero();                                    // stvx128 zero, +0x6070
        mv2WorldCentrePoint.SetZero();                                     // stvx128 zero, +0x60F0

        // Seed the map-move/scroll debouncer with the cursor's world position and clear
        // the "was scrolling" latch. GuiCursor::GetPosition() returns Vector2 BY VALUE (DWARF h:334).
        mSoundData.Update(MapTransform::DeviceToWorld(mCursor.GetPosition()), false);
    }
}

// ============================================================================
// FOLDED FROM BrnCrashNavMap_wJ_05.cpp (wave J) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===================================================================================
// BrnGui::CrashNavMap -- wave-J partfile 05: the button-prompt state machine and the
// road-rule scoreboard builder.
//   UpdateButtonPrompts @0x824B68B8  (cpp:1010 / 1015 / 1016 assert sites)
//   UpdateRoadRule      @0x824B6DC0  (cpp:1747 / 1861 assert sites)
//
// Both bodies are landed. The shared-header declarations this group filed requests for
// have since been applied, so what follows is spelled the way the committed headers spell
// it. Where the wave-J request and the DWARF disagreed, the DWARF won:
//
//   * The panel-type word at CrashNavPanel+0x90 is `PanelType mePanelType`, read through
//     `PanelType GetPanelActiveFilterMode()` (DWARF BrnCrashNavPanel.h:239 / h:314), NOT
//     the `EShowingMode`/`GetPanelType()` spelling the request invented -- and the enum
//     has SEVEN values (E_PANEL_EVENT/DRIVETHRU/ROADSIGN/RIVALS/SELECTABLE_COUNT/GENERIC/
//     COUNT, DWARF h:69), not the two the request asked to add. DecFIGS lists exactly one
//     GetPanelActiveFilterMode call for this function (dwarfdump/_compile/
//     BrnGuiScreenUnity.cpp:3317), so the panel type is read ONCE per arm into a local --
//     the X360 sank that load into all three arms.
//   * `void SetRoadPanelData(const char*, RoadPanelData&)` takes a NON-const reference
//     (DWARF BrnCrashNavPanel.h, BrnCrashNavPanel.cpp:675).
//   * RoadPanelData / PanelBox carry the DWARF names (mPanels, mabPlayerBestScore,
//     mPlayerScores, mNames, mScores, KI_ROADRULE_COUNT, KI_PANEL_TEXT_LENGTH -- DWARF
//     BrnRoadPanel.h:52..85); the earlier draft had consumer-named every one of them.
//   * `bool IsRoadRuleFriendSelected() const` -- the `const` is DWARF-attested
//     (BrnCrashNavPanel.cpp:1152), not inferred.
//   * `ChallengeData::CompareScores` is static: the X360 function @0x82676640 takes no
//     `this` at all -- its whole body uses r3 as the comparator-table index and shifts
//     r4/r5 down before tail-calling.
//   * GuiCache's road-rule-friend-scores gate at +0x4B50 is read with **lbz** at both
//     sites, so the one-byte carve does not disturb mbOnlineMatchRanked @+0x4B51.
//
// The measurement dumps this wave produced live at scratchpad/waveJ/asm_updateroadrule.txt
// (the raw UpdateRoadRule listing Hex-Rays mangles), scratchpad/waveJ/g05_wJ.txt (the 0.001
// scale float, the CompareScores body, RoadPanelData/PanelBox/SetRoadPanelData),
// scratchpad/waveJ/g05_strs.txt (the strings IDA truncated) and
// scratchpad/waveJ/g05_panel.txt (the CrashNavPanel setters).
// ===================================================================================


namespace BrnGui
{
    namespace
    {
        // @0x82F26EC0 -- the apt view-state name per EVisibleAnimationStates, dumped
        // big-endian (scratchpad/waveJ/crashnav_consts.txt). DWARF BrnCrashNavMap.cpp:93.
        const char* const KAPC_VISIBLE_ANIMATION_STATES[CrashNavMap::E_VISIBLE_ANIMATION_STATES_COUNT] =
        {
            "visible",     // E_VISIBLE_ANIMATION_STATES_VISIBLE
            "invisible",   // E_VISIBLE_ANIMATION_STATES_INVISIBLE
        };

        // @0x82F26EC8 -- the apt view-state name per EButtonPromptAnimationStates, dumped
        // big-endian (same dump). DWARF BrnCrashNavMap.cpp:99.
        const char* const KAPC_BUTTON_PROMPT_ANIMATION_STATES[CrashNavMap::E_BUTTON_PROMPT_ANIMATION_STATES_COUNT] =
        {
            "OfflineNone",                 //  0 OFFLINE_NONE_SELECTED
            "OfflineEvent",                //  1 OFFLINE_EVENT
            "RoadScoresOffline",           //  2 ROADS_OFFLINE_SCORES
            "RoadScoresOnline",            //  3 ROADS_ONLINE_SCORES
            "FreeburnLobby",               //  4 ONLINE
            "FreeburnLobbyPause",          //  5 ONLINE_PAUSE
            "FreeburnLobbyRival",          //  6 ONLINE_RIVAL
            "FreeburnLobbyPauseRival",     //  7 ONLINE_PAUSE_RIVAL
            "RoadScoresOfflineX360",       //  8 ROADS_OFFLINE_SCORES_X360
            "RoadScoresOfflineBack",       //  9 ROADS_OFFLINE_SCORES_BACK
            "RoadScoresOnlineBack",        // 10 ROADS_ONLINE_SCORES_BACK
            "RoadScoresOfflineX360Back",   // 11 ROADS_OFFLINE_SCORES_X360_BACK
        };

        // The two GuiEventSetRoadRuleScoreMode::ERoadPanelModes values this body branches
        // on (X360 `cmpwi r3, 1` @0x824B6B30 / `cmpwi r3, 0` @0x824B6B74). That enum is
        // catalogued but not reconstructed (BrnGuiDemangledEventTypes.h:172 carries only
        // the wire id 330 and an opaque 4-byte payload), and BrnGuiCache.cpp:1046 attests
        // only E_ROAD_PANEL_MODE_COUNT == 2, so the two members are un-named here.
        //
        // MEASURED, NOT EXPLAINED: score mode 1 selects the "RoadScoresOffline*" prompt
        // sets and score mode 0 selects the "RoadScoresOnline*" ones -- which is the
        // OPPOSITE polarity to BrnRoadPanel.h's KI_ROAD_PANEL_MODE_ONLINE == 1 (that
        // constant comes from RoadPanel::GetSelectedFriendName's own assert, so both
        // readings are attested). Flagged rather than reconciled; nothing here depends on
        // which name is right.
        const s32 KI_ROAD_PANEL_SCORE_MODE_OFFLINE_PROMPTS = 1;
        const s32 KI_ROAD_PANEL_SCORE_MODE_ONLINE_PROMPTS  = 0;

        const char* const KPC_ASSERT_FILE =
            "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnCrashNavMap.cpp";
    }

    // ------------------------------------------------- UpdateButtonPrompts @0x824B68B8
    //
    // Choose the title-bar visibility state and the button-prompt set for the map screen,
    // and push them at the two animation components whenever either changes. Pure state
    // machine: no side effect other than the two latched members and the two apt
    // transitions.
    //
    // The decision is (screen type) x (panel type) x (what the cursor has selected), with
    // the road-rule scoreboard arm additionally keyed on the panel's score mode, a cache
    // gate byte and whether a friend row is selected. The three screen types share the
    // road-scores sub-tree's SHAPE but not its constants -- the offline and online screens
    // land on the plain prompt sets (2 / 3 / 8) and the from-pause screen on the "Back"
    // flavours (9 / 10 / 11) -- so the X360 has two copies of it and so does this.
    void CrashNavMap::UpdateButtonPrompts()
    {
        // `li r28, 2` / `li r29, 0xC` at 0x824B68CC. Both start at their COUNT sentinel;
        // the post-switch asserts below fire on any arm that fails to set them.
        EVisibleAnimationStates      leTitleButtonsState      = E_VISIBLE_ANIMATION_STATES_COUNT;
        EButtonPromptAnimationStates leNavigationButtonsState = E_BUTTON_PROMPT_ANIMATION_STATES_COUNT;

        switch (meScreenType)   // lwz +0x60DC
        {
        case E_SCREEN_TYPE_OFFLINE:   // loc_824B6B84
            {
                leTitleButtonsState = E_VISIBLE_ANIMATION_STATES_VISIBLE;   // li r28, 0

                const CrashNavPanel::PanelType lePanelType = mCrashNavPanel.GetPanelActiveFilterMode();   // lwz +0x770

                if (lePanelType == CrashNavPanel::E_PANEL_EVENT)
                {
                    // The X360 computes this with cntlzw/extrwi/xori -- the compiler's
                    // branchless form of `muHoveredEventID != 0`, de-optimised back.
                    leNavigationButtonsState = (muHoveredEventID != 0)
                        ? E_BUTTON_PROMPT_ANIMATION_STATES_OFFLINE_EVENT
                        : E_BUTTON_PROMPT_ANIMATION_STATES_OFFLINE_NONE_SELECTED;
                }
                else if (lePanelType == CrashNavPanel::E_PANEL_ROADSIGN &&
                         (mpLockedIconName != NULL ||          // lwz  +0x608C
                          mbLocalPlayerSelected ||             // lbz  +0x60D8
                          meCursorMode == E_CURSORMODE_PANNING))   // lwz +0x5F50, cmpwi 4
                {
                    // loc_824B6B24 -- the plain (non-"Back") road-scores prompt sets.
                    if (mCrashNavPanel.GetRoadPanelScoreMode() == KI_ROAD_PANEL_SCORE_MODE_OFFLINE_PROMPTS)
                    {
                        leNavigationButtonsState =
                            (mpGuiCache->AreRoadRuleFriendScoresAvailable() &&
                             mCrashNavPanel.IsRoadRuleFriendSelected())
                                ? E_BUTTON_PROMPT_ANIMATION_STATES_ROADS_OFFLINE_SCORES_X360
                                : E_BUTTON_PROMPT_ANIMATION_STATES_ROADS_OFFLINE_SCORES;
                    }
                    else if (mCrashNavPanel.GetRoadPanelScoreMode() == KI_ROAD_PANEL_SCORE_MODE_ONLINE_PROMPTS)
                    {
                        leNavigationButtonsState = E_BUTTON_PROMPT_ANIMATION_STATES_ROADS_ONLINE_SCORES;
                    }
                    // Any other score mode leaves the COUNT sentinel in place and trips
                    // the cpp:1016 assert below (X360: `bne loc_824B6928`).
                }
                else
                {
                    leNavigationButtonsState = E_BUTTON_PROMPT_ANIMATION_STATES_OFFLINE_NONE_SELECTED;
                }
            }
            break;

        case E_SCREEN_TYPE_ONLINE:   // loc_824B6A98
            {
                leTitleButtonsState = E_VISIBLE_ANIMATION_STATES_INVISIBLE;   // li r28, 1

                const CrashNavPanel::PanelType lePanelType = mCrashNavPanel.GetPanelActiveFilterMode();

                if (lePanelType == CrashNavPanel::E_PANEL_ROADSIGN)
                {
                    if (mpLockedIconName != NULL || mbLocalPlayerSelected ||
                        meCursorMode == E_CURSORMODE_PANNING)
                    {
                        // Shares loc_824B6B24 with the offline screen: same constants.
                        if (mCrashNavPanel.GetRoadPanelScoreMode() == KI_ROAD_PANEL_SCORE_MODE_OFFLINE_PROMPTS)
                        {
                            leNavigationButtonsState =
                                (mpGuiCache->AreRoadRuleFriendScoresAvailable() &&
                                 mCrashNavPanel.IsRoadRuleFriendSelected())
                                    ? E_BUTTON_PROMPT_ANIMATION_STATES_ROADS_OFFLINE_SCORES_X360
                                    : E_BUTTON_PROMPT_ANIMATION_STATES_ROADS_OFFLINE_SCORES;
                        }
                        else if (mCrashNavPanel.GetRoadPanelScoreMode() == KI_ROAD_PANEL_SCORE_MODE_ONLINE_PROMPTS)
                        {
                            leNavigationButtonsState = E_BUTTON_PROMPT_ANIMATION_STATES_ROADS_ONLINE_SCORES;
                        }
                    }
                    else
                    {
                        leNavigationButtonsState = E_BUTTON_PROMPT_ANIMATION_STATES_OFFLINE_NONE_SELECTED;
                    }
                }
                else if (lePanelType == CrashNavPanel::E_PANEL_RIVALS)   // loc_824B6AB8
                {
                    // A rival is "named" either because the player-name buffer is filled
                    // in or because the locked icon is a networked rival.
                    leNavigationButtonsState =
                        (mPlayerName.macName[0] != '\0' ||
                         mLockedIconInfo.GetIconType() ==
                             GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_NETWORKRIVAL)
                            ? E_BUTTON_PROMPT_ANIMATION_STATES_ONLINE_RIVAL
                            : E_BUTTON_PROMPT_ANIMATION_STATES_ONLINE;
                }
                else
                {
                    // MEASURED (`li r29, 5` @0x824B6AB0): the ONLINE screen's fall-through
                    // is the *_PAUSE prompt set, the same constant the ONLINE_FROM_PAUSE
                    // screen uses (@0x824B69CC), even though its own rival arm just above
                    // uses the non-pause 4. Transcribed as found; not "corrected".
                    leNavigationButtonsState = E_BUTTON_PROMPT_ANIMATION_STATES_ONLINE_PAUSE;
                }
            }
            break;

        case E_SCREEN_TYPE_ONLINE_FROM_PAUSE:   // loc_824B69B4
            {
                leTitleButtonsState = E_VISIBLE_ANIMATION_STATES_INVISIBLE;

                const CrashNavPanel::PanelType lePanelType = mCrashNavPanel.GetPanelActiveFilterMode();

                if (lePanelType == CrashNavPanel::E_PANEL_ROADSIGN)   // loc_824B6A08
                {
                    if (mpLockedIconName != NULL || mbLocalPlayerSelected ||
                        meCursorMode == E_CURSORMODE_PANNING)
                    {
                        // loc_824B6A38 -- the "Back" flavours of the same three states.
                        if (mCrashNavPanel.GetRoadPanelScoreMode() == KI_ROAD_PANEL_SCORE_MODE_OFFLINE_PROMPTS)
                        {
                            leNavigationButtonsState =
                                (mpGuiCache->AreRoadRuleFriendScoresAvailable() &&
                                 mCrashNavPanel.IsRoadRuleFriendSelected())
                                    ? E_BUTTON_PROMPT_ANIMATION_STATES_ROADS_OFFLINE_SCORES_X360_BACK
                                    : E_BUTTON_PROMPT_ANIMATION_STATES_ROADS_OFFLINE_SCORES_BACK;
                        }
                        else if (mCrashNavPanel.GetRoadPanelScoreMode() == KI_ROAD_PANEL_SCORE_MODE_ONLINE_PROMPTS)
                        {
                            leNavigationButtonsState = E_BUTTON_PROMPT_ANIMATION_STATES_ROADS_ONLINE_SCORES_BACK;
                        }
                    }
                    else
                    {
                        leNavigationButtonsState = E_BUTTON_PROMPT_ANIMATION_STATES_OFFLINE_NONE_SELECTED;
                    }
                }
                else if (lePanelType == CrashNavPanel::E_PANEL_RIVALS)   // loc_824B69D4
                {
                    leNavigationButtonsState =
                        (mPlayerName.macName[0] != '\0' ||
                         mLockedIconInfo.GetIconType() ==
                             GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_NETWORKRIVAL)
                            ? E_BUTTON_PROMPT_ANIMATION_STATES_ONLINE_PAUSE_RIVAL
                            : E_BUTTON_PROMPT_ANIMATION_STATES_ONLINE_PAUSE;
                }
                else
                {
                    leNavigationButtonsState = E_BUTTON_PROMPT_ANIMATION_STATES_ONLINE_PAUSE;
                }
            }
            break;

        default:
            // `cmplwi cr6, r11, 3` + `bge` -- anything from 3 up (unsigned) gets here and
            // leaves BOTH locals at their COUNT sentinel, which is why the X360 emits the
            // two asserts below unconditionally on this path.
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Unknown screen type!\n", KPC_ASSERT_FILE, 1010);   // li r5, 0x3F2
            CgsDev::Assert::EndAssert();
            break;
        }

        if (E_VISIBLE_ANIMATION_STATES_COUNT == leTitleButtonsState)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("E_VISIBLE_ANIMATION_STATES_COUNT != leTitleButtonsState",
                                       KPC_ASSERT_FILE, 1015);   // li r5, 0x3F7
            CgsDev::Assert::EndAssert();
        }

        if (E_BUTTON_PROMPT_ANIMATION_STATES_COUNT == leNavigationButtonsState)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("E_BUTTON_PROMPT_ANIMATION_STATES_COUNT != leNavigationButtonsState",
                                       KPC_ASSERT_FILE, 1016);   // li r5, 0x3F8, loc_824B6928
            CgsDev::Assert::EndAssert();
        }

        // Latch + push. Both comparisons are SIGNED (`cmpw`), and the index used to pick
        // the view-state string is the NEW value, stored first (stw then lwzx).
        //
        // INLINING REVERSED. The X360 emits `AddOutputAptViewState("apt_Transition",
        // <view state>, false)` at both sites (0x824B6978 / 0x824B69A8, with r4 =
        // aAptTransition_1 and `li r6, 0` hoisted above the branch at 0x824B6950), but the
        // clip name and the immediate flag are constants at EVERY call site precisely
        // because they belong to the callee: DecFIGS lists this function's callees as
        // `AnimationComponent::Run` TWICE and does not mention AddOutputAptViewState at all
        // (dwarfdump/_compile/BrnGuiScreenUnity.cpp:3315-3316), and DWARF
        // BrnAnimationComponent.h:64 gives Run exactly one `const char*` parameter. So the
        // source called Run and the compiler folded its one-line body in. Run has no X360
        // symbol of its own -- it is a LINK-TIME EXTERNAL that grows a body with the
        // AnimationComponent TU.
        if (leTitleButtonsState != meTitleButtonsState)   // lwz +0x60E0
        {
            meTitleButtonsState = leTitleButtonsState;
            mTitleButtonsAnimation.Run(   // r3 = this + 0x5F54
                KAPC_VISIBLE_ANIMATION_STATES[leTitleButtonsState]);
        }

        if (leNavigationButtonsState != meNavigationButtonsState)   // lwz +0x60E4
        {
            meNavigationButtonsState = leNavigationButtonsState;
            mButtonPromptsAnimation.Run(   // r3 = this + 0x5FE0
                KAPC_BUTTON_PROMPT_ANIMATION_STATES[leNavigationButtonsState]);
        }
    }
}

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CgsDev::Assert machinery
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStream (streamed assert)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event
#include "GameSource/GameState/BrnCgsPlayerName.h"                        // CgsNetwork::PlayerName
#include "GameSource/Gui/Flow/Screen/Components/BrnCrashNavPanel.h"       // CrashNavPanel::SetRoadPanelData
#include "GameSource/Gui/Flow/Screen/Components/BrnRoadPanel.h"           // BrnGui::RoadPanelData
#include "SharedClasses/StreetData/BrnChallengeData.h"                    // ChallengeData::CompareScores / ScoreType

namespace BrnGui
{
    namespace
    {
        // ---- in-queue payload view (the state's input queue hands the header-stripped
        // payload, per the wave-H/I convention) ----
        //
        // Event 334 (road-rule scoreboard data). Field placement is measured off this
        // function's own reads; the shape corroborates the PS3 DWARF GuiEventRoadRuleData
        // = GuiEvent<330>{ CgsID; RoadRuleType[2] }.
        //
        // NOTE: BrnGuiDemangledEventTypes.h:148 catalogues id 334 as a 12-byte header plus
        // a 76-byte opaque payload. The reads here span 88 bytes of payload (rule 1's
        // mRivalId is the last 8 bytes at payload +80), so that catalogue entry is 12
        // bytes short -- the same kind of mismatch the wave-J spec already recorded for
        // id 332. The asm wins; this view stays file-local.
        struct GuiEventRoadRuleDataPayload : public CgsModule::Event
        {
            struct RoadRuleType
            {
                // [0] the current road ruler's (rival's) score, [1] the local player's
                // own score, [2] the selected friend's score. Indices are measured from
                // which value feeds which PanelBox field, below.
                s32                    maiScores[3];   // rule +0x00 / +0x04 / +0x08
                CgsNetwork::PlayerName mFriendName;    // rule +0x0C
                CgsID                  mRivalId;       // rule +0x20 (ld / cmpldi)
            };

            CgsID        mRoadID;                                        // payload +0x00
            RoadRuleType maRules[BrnStreetData::E_SCORE_TYPE_COUNT];     // payload +0x08, 40-byte stride
        };

        // @0x820662C4 -- which scoreboard box each score type is drawn in. DWARF
        // BrnCrashNavMap.cpp:47. Dumped big-endian: { 0, 1 }
        // (scratchpad/waveJ/crashnav_consts.txt).
        const s32 KAI_ROAD_RULE_DISPLAY_POS[BrnStreetData::E_SCORE_TYPE_COUNT] = { 0, 1 };

        // @0x8204D370 -- the one numeric format this function uses, for every score of
        // both score types.
        const char* const KPC_SCORE_FORMAT = "%3.3f";

        // @0x8206796C -- the road-ruler name line: the locked road-sign icon's name run
        // through the language table.
        const char* const KPC_ROAD_RULER_NAME_FORMAT = "$RULERQ_%s";

        // @0x82025D48 -- the placeholder drawn for an absent friend / absent road ruler.
        const char* const KPC_NO_ENTRY = "-";

        // f31, loaded once at 0x824B6E28 from flt_82013F90 (0x3A83126F). Road-rule TIME
        // scores are stored in milliseconds and shown in seconds.
        // FLAG consumer-named: the float is a bare rodata literal with no DWARF symbol.
        const f32 KF_MILLISECONDS_TO_SECONDS = 0.001f;

        // `li r4, 0x40` at every SPrintf call site in this function.
        const s32 KI_SCORE_TEXT_LEN = 64;

        // KPC_ASSERT_FILE is already defined by the UpdateButtonPrompts block above --
        // this partfile concatenates two functions that each carried their own
        // file-local block; one definition per TU is enough.
    }

    // ------------------------------------------------------- UpdateRoadRule @0x824B6DC0
    //
    // Turn an incoming road-rule scoreboard payload into the two-row panel the crash-nav
    // map shows for a selected road sign: one row per score type (TIME then CRASH), each
    // row holding the local player's score plus the friend row and the road-ruler row,
    // and a "you have beaten this" flag for each of those two.
    //
    // The loop index doubles as the BrnStreetData::ScoreType -- it is what the X360 hands
    // CompareScores as its comparator-table index (see banner note (3)) -- which is also
    // why only iteration 0 rescales its scores from milliseconds to seconds.
    void CrashNavMap::UpdateRoadRule(const CgsModule::Event* lpEvent)
    {
        if (lpEvent == NULL)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpRoadRule", KPC_ASSERT_FILE, 1747);   // li r5, 0x6D3
            CgsDev::Assert::EndAssert();
        }

        const GuiEventRoadRuleDataPayload* const lpRoadRule =
            static_cast<const GuiEventRoadRuleDataPayload*>(lpEvent);

        RoadPanelData lRoadPanelData;
        lRoadPanelData.Construct();   // @0x824B6E08

        for (s32 liScoreType = 0; liScoreType < BrnStreetData::E_SCORE_TYPE_COUNT; ++liScoreType)
        {
            // `cmpwi r28, 2` at 0x824B711C -- the loop really does run over the two score
            // types, and maRules / KAI_ROAD_RULE_DISPLAY_POS are both sized by them.
            const GuiEventRoadRuleDataPayload::RoadRuleType& lRule = lpRoadRule->maRules[liScoreType];

            bool lbScore1Beaten = false;   // r24 -- the friend row
            bool lbScore2Beaten = false;   // r25 -- the road-ruler row

            char lacName1[KI_SCORE_TEXT_LEN];
            char lacName2[KI_SCORE_TEXT_LEN];
            char lacYourScore[KI_SCORE_TEXT_LEN];
            char lacScore1[KI_SCORE_TEXT_LEN];
            char lacScore2[KI_SCORE_TEXT_LEN];

            s32 liScore1 = 0;   // r27 -- the friend's score, 0 when there is no friend row
            s32 liScore2 = 0;   // r30 -- the road ruler's score, 0 when there is none

            // -------- row 1: the selected friend --------
            if (lRule.maiScores[2] > 0)
            {
                // The name is passed to SPrintf as the FORMAT string, not as a "%s"
                // argument -- exactly as the X360 does it (banner note above).
                CgsCore::SPrintf(lacName1, KI_SCORE_TEXT_LEN, lRule.mFriendName.macName);
                liScore1 = lRule.maiScores[2];
                lacName1[KI_SCORE_TEXT_LEN - 1] = '\0';

                if (lRule.maiScores[1] > 0 &&
                    BrnStreetData::ChallengeData::CompareScores(
                        static_cast<BrnStreetData::ScoreType>(liScoreType),
                        lRule.maiScores[1], lRule.maiScores[2]) <= 0)
                {
                    // CompareScores returns <= 0 when the first score is the better one
                    // (the polarity every committed caller uses), so the local player has
                    // matched or beaten the friend.
                    lbScore1Beaten = true;
                }
            }
            else
            {
                // No friend score on record: any score of the player's own counts.
                if (lRule.maiScores[1] > 0)
                {
                    lbScore1Beaten = true;
                }

                CgsCore::SPrintf(lacName1, KI_SCORE_TEXT_LEN, KPC_NO_ENTRY);
                liScore1 = 0;
                lacName1[KI_SCORE_TEXT_LEN - 1] = '\0';
            }

            // -------- row 2: the current road ruler --------
            if (lRule.mRivalId != 0)
            {
                CgsCore::SPrintf(lacName2, KI_SCORE_TEXT_LEN, KPC_ROAD_RULER_NAME_FORMAT, mpLockedIconName);
                liScore2 = lRule.maiScores[0];
                lacName2[KI_SCORE_TEXT_LEN - 1] = '\0';

                if (lRule.maiScores[1] > 0 &&
                    BrnStreetData::ChallengeData::CompareScores(
                        static_cast<BrnStreetData::ScoreType>(liScoreType),
                        lRule.maiScores[1], lRule.maiScores[0]) <= 0)
                {
                    lbScore2Beaten = true;
                }
            }
            else
            {
                if (lRule.maiScores[1] > 0)
                {
                    lbScore2Beaten = true;
                }

                CgsCore::SPrintf(lacName2, KI_SCORE_TEXT_LEN, KPC_NO_ENTRY);
                liScore2 = 0;
                lacName2[KI_SCORE_TEXT_LEN - 1] = '\0';
            }

            // -------- the three score strings --------
            if (liScoreType == BrnStreetData::E_SCORE_TYPE_TIME)
            {
                // Times are held in milliseconds; "%3.3f" of value/1000 renders seconds.
                CgsCore::SPrintf(lacYourScore, KI_SCORE_TEXT_LEN, KPC_SCORE_FORMAT,
                                 static_cast<f32>(lRule.maiScores[1]) * KF_MILLISECONDS_TO_SECONDS);
                CgsCore::SPrintf(lacScore1, KI_SCORE_TEXT_LEN, KPC_SCORE_FORMAT,
                                 static_cast<f32>(liScore1) * KF_MILLISECONDS_TO_SECONDS);
                CgsCore::SPrintf(lacScore2, KI_SCORE_TEXT_LEN, KPC_SCORE_FORMAT,
                                 static_cast<f32>(liScore2) * KF_MILLISECONDS_TO_SECONDS);
            }
            else
            {
                // Same float format, no rescale (X360: the fcfid/frsp chain without the
                // fmuls). Not an integer format -- see the banner.
                CgsCore::SPrintf(lacYourScore, KI_SCORE_TEXT_LEN, KPC_SCORE_FORMAT,
                                 static_cast<f32>(lRule.maiScores[1]));
                CgsCore::SPrintf(lacScore1, KI_SCORE_TEXT_LEN, KPC_SCORE_FORMAT,
                                 static_cast<f32>(liScore1));
                CgsCore::SPrintf(lacScore2, KI_SCORE_TEXT_LEN, KPC_SCORE_FORMAT,
                                 static_cast<f32>(liScore2));
            }

            lacScore2[KI_SCORE_TEXT_LEN - 1] = '\0';
            lacScore1[KI_SCORE_TEXT_LEN - 1] = '\0';

            if (lacName1[0] == '\0')
            {
                // The X360 opens the assert FIRST and streams into the global
                // CgsDev::Assert::gpcMessageBuffer (sized by dword_82F32264); the
                // committed convention -- and the wave-J sibling parked files -- use a
                // stack buffer instead, which is cosmetic. The message carries no streamed
                // value, but the X360 still builds it through the StrStream sink, so the
                // stream form is kept.
                CgsDev::Assert::BeginAssert();

                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStream << "Name for the first player not set";

                CgsDev::Assert::FireAssert(lacMessageBuffer, KPC_ASSERT_FILE, 1861);   // li r5, 0x745
                CgsDev::Assert::EndAssert();
            }

            lRoadPanelData.mPanels[KAI_ROAD_RULE_DISPLAY_POS[liScoreType]].Construct(
                lacYourScore, lacName1, lacScore1, lacName2, lacScore2,
                lbScore1Beaten, lbScore2Beaten);
        }

        // The panel only takes the payload while a road sign is actually locked on and
        // this screen flavour draws road signs at all.
        if (mpLockedIconName != NULL && mbUseRoadSigns)
        {
            mCrashNavPanel.SetRoadPanelData(mpLockedIconName, lRoadPanelData);
        }
    }
}

// ============================================================================
// FOLDED FROM BrnCrashNavMap_wJ_06.cpp (wave J) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===================================================================================
// BrnGui::CrashNavMap -- wave-J partfile 06: the "inspect an event" pair.
//
//   CalculateEventZoomFactor @0x824BF4B0
//   UpdateEvent              @0x824CC3F8
//
// Both bodies are landed. They were reconstructed in full from the raw X360 assembly
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x824CC3F8.json and /0x824BF4B0.json, field
// `assembly`; copies at scratchpad/waveJ/asm_g06_*.txt) and originally parked because
// seven names they use were declared nowhere in b5-decomp/src. Those declarations have
// since been applied:
//
//   BrnMapIconManager.h  -- `friend struct CrashNavMap;` (miNumUsedIcons was already
//                           committed but private) plus the three DWARF-attested members
//                           muSelectedJunctionID / miSelectedCheckpoint /
//                           mbShowingCrashNavRoute (X360 +0xAA10 / +0xAA14 / +0xAA21).
//   BrnGuiCache.h        -- GuiCache::GetLandmarkInfoFromID(CgsID, SatNavIconInfo*) const;
//                           GuiCache::HACK_FindABetterPlaceForMe_SetActiveLandmarksByEventID
//                           (u32, f32, bool); and SatNavEventDisplayInfo's +0x14 junction
//                           id word (split out of the existing 8-byte pad).
//   BrnRaceEventData.h   -- RaceEventData::GetCheckpointCount() and a COMPLETE
//                           RaceEventData::CheckpointData carrying GetLandmarkId().
//
// CORRECTION TO THE MEASUREMENT THIS BANNER USED TO QUOTE. It said the probe against the
// committed headers produced "five C2039, one C2248, one C2027". Re-measured from OUTSIDE
// the probe directory -- MSVC resolves a quoted include relative to the INCLUDING FILE's
// directory before any /I, so a probe.cpp sitting inside scratchpad/waveJ/probe_g06/ binds
// to that directory's own shadow headers and never tests the committed ones at all -- the
// real figure was C2039 x10 across SEVEN distinct names (mbShowingCrashNavRoute,
// miSelectedCheckpoint, muSelectedJunctionID, GetJunctionID, GetCheckpointCount,
// GetLandmarkInfoFromID, HACK_FindABetterPlaceForMe_SetActiveLandmarksByEventID) plus one
// C2248 and one C2027. The blocking SET was right; only the count was understated.
//
// Nothing else about these two functions is outstanding: every constant they read was
// dumped from the image with headless IDA this wave (scratchpad/waveJ/g05_consts.txt,
// crashnav_sinit.txt, crashnav_floats2.txt) and every argument list was arbitrated from
// the assembly, including the two PPC float-ABI call sites (MainMapComponent::SetZoom and
// HACK_FindABetterPlaceForMe_SetActiveLandmarksByEventID, where the float argument skips
// its gpr slot) and the fsel progress clamp's NaN polarity.
// ===================================================================================


namespace BrnGui
{
    namespace
    {
        // TU statics, DWARF BrnCrashNavMap.cpp:70 / :71. X360 .data @0x82FB4AC0 and
        // @0x82FB4D10, both written by the same runtime static initialiser shape
        // (0x82C54D20 / 0x82C54D60) from flt_8206B494 == 638.0 and flt_8206B490 ==
        // 349.79999 with lanes 2/3 zeroed. They hold the SAME value in the shipped build.
        const Vector2 K_CRASHNAV_LONG_DISPLAY_RECT = { 638.0f, 349.8f, 0.0f, 0.0f };
        const Vector2 K_CRASHNAV_TALL_DISPLAY_RECT = { 638.0f, 349.8f, 0.0f, 0.0f };
    }

    // ================================================================================
    //  CalculateEventZoomFactor  @ 0x824BF4B0
    //
    //  Frame the whole of the event the player is inspecting: take the bounding box of
    //  the event-start marker plus every checkpoint landmark on the 2D map plane, centre
    //  the main map on it, and return the zoom factor that makes that box fit the
    //  crash-nav display rect.
    // ================================================================================
    f32 CrashNavMap::CalculateEventZoomFactor()
    {
        const SatNavEventDisplayInfo* lpEventStart =
            mpGuiCache->GetProfileEventDisplayInfo(muInspectingEventID);

        // GetWorldDataController() is X360-inlined here and carries its own
        // "mpWorldDataController" assert (BrnGuiCache.h:2324) -- do not repeat it.
        const BrnProgression::RaceEventData* lpRaceEventData =
            mpGuiCache->GetWorldDataController()->GetEventInfoFromEventId(muInspectingEventID);

        // cpp:2120 / cpp:2121 -- both non-fatal on the console; both pointers are used
        // regardless, exactly as below.
        CGS_ASSERT(lpEventStart, "lpEventStart");
        CGS_ASSERT(lpRaceEventData, "lpRaceEventData");

        // Seed the bounding box with the event start marker. The console does the
        // world -> map-plane swizzle with one vperm (see the mask note in the banner);
        // lanes 2/3 are the redundant copy of lane 0 that mask produces.
        Vector2 lv2Min;
        lv2Min.x = lpEventStart->mv3Position.x;
        lv2Min.y = lpEventStart->mv3Position.z;
        lv2Min.z = lpEventStart->mv3Position.x;
        lv2Min.w = lpEventStart->mv3Position.x;

        Vector2 lv2Max = lv2Min;

        // The count is re-read every pass on the console; GetCheckpointCount() in the
        // condition keeps that. GetCheckpointData carries the inlined bounds assert.
        for (s32 liCheckpointIndex = 0;
             liCheckpointIndex < lpRaceEventData->GetCheckpointCount();
             ++liCheckpointIndex)
        {
            GuiEventUpdateSatNav::SatNavIconInfo lIconInfo;
            mpGuiCache->GetLandmarkInfoFromID(
                lpRaceEventData->GetCheckpointData(liCheckpointIndex)->GetLandmarkId(),
                &lIconInfo);

            const Vector4& lrv4IconPosition = lIconInfo.GetPositionLane();

            Vector2 lv2Point;
            lv2Point.x = lrv4IconPosition.x;
            lv2Point.y = lrv4IconPosition.z;
            lv2Point.z = lrv4IconPosition.x;
            lv2Point.w = lrv4IconPosition.x;

            // vminfp / vmaxfp, written out per lane.
            lv2Min.x = (lv2Min.x < lv2Point.x) ? lv2Min.x : lv2Point.x;
            lv2Min.y = (lv2Min.y < lv2Point.y) ? lv2Min.y : lv2Point.y;
            lv2Min.z = (lv2Min.z < lv2Point.z) ? lv2Min.z : lv2Point.z;
            lv2Min.w = (lv2Min.w < lv2Point.w) ? lv2Min.w : lv2Point.w;

            lv2Max.x = (lv2Max.x > lv2Point.x) ? lv2Max.x : lv2Point.x;
            lv2Max.y = (lv2Max.y > lv2Point.y) ? lv2Max.y : lv2Point.y;
            lv2Max.z = (lv2Max.z > lv2Point.z) ? lv2Max.z : lv2Point.z;
            lv2Max.w = (lv2Max.w > lv2Point.w) ? lv2Max.w : lv2Point.w;
        }

        // (max + min) * 0.5 -- see the vrefp note in the banner.
        Vector2 lv2Centre;
        lv2Centre.x = (lv2Max.x + lv2Min.x) * 0.5f;
        lv2Centre.y = (lv2Max.y + lv2Min.y) * 0.5f;
        lv2Centre.z = (lv2Max.z + lv2Min.z) * 0.5f;
        lv2Centre.w = (lv2Max.w + lv2Min.w) * 0.5f;
        mMainMapComponent.SetDesiredWorldCentre(lv2Centre);

        // A box that is taller than it is wide gets framed against the tall rect.
        const f32 lfWidth  = lv2Max.x - lv2Min.x;
        const f32 lfHeight = lv2Max.y - lv2Min.y;
        const Vector2 lv2DisplayRect = (lfHeight > lfWidth) ? K_CRASHNAV_TALL_DISPLAY_RECT
                                                            : K_CRASHNAV_LONG_DISPLAY_RECT;

        // X360 flt_82F27384 == 1.7777778, the 16:9 display aspect (a shared rodata scalar).
        return MapTransform::CalculateZoomFactor(lv2Min, lv2Max, lv2DisplayRect,
                                                 16.0f / 9.0f);
    }
}

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT / Begin/Fire/EndAssert
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStream (the streamed assert)
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiAudioTriggerEvent
#include "GameSource/Gui/SatNav/BrnGuiTracker.h"                          // GuiTracker::ClearTracker
#include "GameSource/Gui/SatNav/BrnMapIconManager.h"                      // BrnGui::MapIconManager

namespace BrnGui
{
    namespace
    {
// (fold: an identical definition of KI_CHANNEL_GUI_OUT was dropped here -- this TU defines it once, above)

        // GuiAudioTriggerEvent::meAction values for the map's view-event cue (X360
        // `li r4, 8` on the first inspecting frame, `li r4, 9` on the tear-down).
        // FLAG: the action enum's name is not in the recovered DWARF slice, so the
        // measured literals stand in -- same convention as BrnOnlineGameOptions_wI_02.cpp.
        const s32 KI_AUDIO_ACTION_VIEW_EVENT_START = 8;
        const s32 KI_AUDIO_ACTION_VIEW_EVENT_STOP  = 9;

        // TU static, DWARF BrnCrashNavMap.cpp:43 -- rodata pointer off_82F26E80.
        const char KPC_SOUND_MAP_VIEW_EVENT[] = "CodeMapViewEvent";

// (fold: an identical definition of KAC_ASSERT_FILE was dropped here -- this TU defines it once, above)

// (fold: an identical definition of KU_WIRE_ID_AUDIO_TRIGGER was dropped here -- this TU defines it once, above)

// (fold: an identical definition of GuiAudioTriggerWire was dropped here -- this TU defines it once, above)

        // Record-size pin. The payload is pointer-free, so the X360's AddEvent size
        // immediate must survive unchanged on the x64 host.
        static_assert(sizeof(GuiAudioTriggerWire) == 112, "audio trigger record is 112 bytes");
    }

    // ================================================================================
    //  UpdateEvent  @ 0x824CC3F8  (cpp:1890 / cpp:1959 asserts)
    //
    //  The per-frame event half of UpdateMainMap: refresh the info panel for whatever the
    //  cursor is on, and drive the "inspecting an event" mode -- on the first inspecting
    //  frame zoom the map onto the whole event and switch the icon manager into
    //  route-display, then every frame reveal the route's landmarks in proportion to how
    //  long the player has been inspecting. Leaving inspection tears all of that down.
    // ================================================================================
    void CrashNavMap::UpdateEvent()
    {
        // cpp:1890 -- non-fatal on the X360; the cache is dereferenced either way.
        CGS_ASSERT(mpGuiCache, "mpGuiCache");

        UpdateEventInfoPanel();

        switch (meCursorMode)
        {
            // The four non-inspecting modes share one jump-table arm.
            case E_CURSORMODE_NONE:
            case E_CURSORMODE_SELECTING_ICONS:
            case E_CURSORMODE_ZOOMEDOUT:
            case E_CURSORMODE_PANNING:
                // A non-zero stamp means we were inspecting last frame and have just left.
                if (mfInspectingEventTime != 0.0f)
                {
                    // No null check here on the console -- see the banner.
                    mpIconManager->mbShowingCrashNavRoute = false;
                    mpIconManager->miSelectedCheckpoint   = 0;
                    mpIconManager->muSelectedJunctionID   = 0;
                    mpIconManager->miNumUsedIcons         = 0;

                    mpGuiCache->GetGuiTracker()->ClearTracker();

                    mfInspectingEventTime = 0.0f;
                    muInspectingEventID   = 0;

                    GuiAudioTriggerWire lAudio;
                    lAudio.Construct(KI_AUDIO_ACTION_VIEW_EVENT_STOP, "",
                                     KPC_SOUND_MAP_VIEW_EVENT, "");
                    mpStateInterface->GetOutputEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lAudio),
                        KI_CHANNEL_GUI_OUT, static_cast<s32>(sizeof(lAudio)));
                }
                break;

            case E_CURSORMODE_INSPECTING_ICONS:
                if (muInspectingEventID != 0)
                {
                    // A zero stamp means this is the first frame of the inspection.
                    if (mfInspectingEventTime == 0.0f)
                    {
                        const f32 lfZoomFactor = CalculateEventZoomFactor();
                        mMainMapComponent.SetZoom(MainMapComponent::E_ZOOMFACTOR_CUSTOM,
                                                  lfZoomFactor, false);

                        if (mpIconManager != 0)
                        {
                            mpIconManager->mbShowingCrashNavRoute = true;
                            mpIconManager->miSelectedCheckpoint   = 0;
                            mpIconManager->muSelectedJunctionID   =
                                mpGuiCache->GetProfileEventDisplayInfo(muInspectingEventID)
                                    ->muJunctionId;   // lwz record+0x14 @0x824CC594
                        }

                        mfInspectingEventTime = mpGuiCache->GetTime();

                        GuiAudioTriggerWire lAudio;
                        lAudio.Construct(KI_AUDIO_ACTION_VIEW_EVENT_START, "",
                                         KPC_SOUND_MAP_VIEW_EVENT, "");
                        mpStateInterface->GetOutputEventQueue()->AddEvent(
                            reinterpret_cast<const CgsModule::Event*>(&lAudio),
                            KI_CHANNEL_GUI_OUT, static_cast<s32>(sizeof(lAudio)));
                    }

                    // The two fsel instructions, transcribed operand for operand -- see
                    // the NaN-polarity note in the banner.
                    const f32 lfElapsed  = mpGuiCache->GetTime() - mfInspectingEventTime;
                    const f32 lfFloored  = (-lfElapsed >= 0.0f) ? 0.0f : lfElapsed;
                    const f32 lfProgress = ((1.0f - lfFloored) >= 0.0f) ? lfFloored : 1.0f;

                    mpGuiCache->HACK_FindABetterPlaceForMe_SetActiveLandmarksByEventID(
                        muInspectingEventID, lfProgress, false);
                }
                break;

            default:
            {
                // Streamed diagnostic: the X360 composes it into
                // CgsDev::Assert::gpcMessageBuffer through a StrStream, then fires it.
                // Non-fatal; the frame simply does nothing for the unknown mode.
                CgsDev::Assert::BeginAssert();
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "Unhandled cursor mode "
                           << static_cast<s32>(meCursorMode)
                           << " in CrashNavMap::UpdateEvent\n";
                CgsDev::Assert::FireAssert(lacMessage, KAC_ASSERT_FILE, 1959);
                CgsDev::Assert::EndAssert();
                break;
            }
        }
    }
}

// ============================================================================
// FOLDED FROM BrnCrashNavMap_wJ_07.cpp (wave J) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===================================================================================
// BrnGui::CrashNavMap -- wave-J partfile 07: the map pan / scroll pair.
//   UpdateMainMap @0x824D8CB0  (BrnCrashNavMap.cpp:611-621)
//   MoveCursor    @0x824BF100  (BrnCrashNavMap.cpp:1432-1502, assert cpp:1470)
//
// Both bodies are landed, reconstructed instruction by instruction from the raw X360 asm
// (Hex-Rays renders UpdateMainMap as inline-asm blocks and flags MoveCursor "local variable
// allocation has failed" -- it drops the float arguments of both the SetDelta call and the
// tail SetPosition). Six callees were declared nowhere under b5-decomp/src when this group
// ran; those declarations have since been applied and the bodies compile against them:
//
//   BrnCursor.h  -- GuiCursor::GetPosition() (DWARF h:334, BY VALUE),
//                   SetPosition(Vector2, bool)      (h:230, @0x82428AD8),
//                   SetDelta(f32, f32, f32)         (h:100, @0x82416750),
//                   FindClosestSnapIndex(Vector2*, u32) (h:119, @0x824168B8).
//   BrnMapIconManager.h -- MapIconManager::GetSatNavIconPositions(Vector2*, s32*)
//                   (h:165, @0x8250A708), now public.
//   BrnMainMap.h -- an exposure for the single main-map world rect the X360 keeps at .data
//                   0x82FB31F0 (written by MainMapComponent Construct / Prepare /
//                   CalculatePositionedWorldRect, read by MoveCursor and by
//                   RoadSignIconManager::SetupComponent; measured this wave,
//                   scratchpad/waveJ/g07_rect.txt).
//   BrnCrashNavPanel.h -- CrashNavPanel::SetRivalPanelData() (@0x8243AAC8), the
//                   no-argument face one arm of UpdateMainMap needs.
//
// Everything else checked out under the compile gate: the member names, the CursorMode /
// SatNavIconType / EIconDisplayType enumerator spellings, MapTransform::Flatten /
// Unflatten / DeviceToWorld / WorldToDevice overload resolution, rw::math::vpu::Dot over
// Vector2, and the stripped GuiEventControllerAxis payload view.
//
// FINDING for the header owner (comment-only, no code impact): the class banner in
// BrnCrashNavMap.h documents mfMapPanningStopTime at X360 +24900, but every access in
// MoveCursor is at 0x6150 == 24912 (`stfs f1, 0x6150(r30)` @0x824BF1EC, `lfs f13,
// 0x6150(r30)` @0x824BF3F0) -- 24880 + 32, i.e. mSoundData's 16-byte Vector3 lane plus its
// 16-byte-aligned bool tail. The member ORDER is right; only the documented offset is off.
// ===================================================================================


namespace BrnGui
{
    namespace
    {
        // Axis ids as emitted by the controller->GUI bridge (see the banner).
        const s32 KI_AXIS_LEFT_STICK      = 0;
        const s32 KI_AXIS_RIGHT_STICK     = 1;
        const s32 KI_AXIS_ALTERNATE_STICK = 2;

        // Pan / snap constants. Names are DWARF-attested (BrnCrashNavMap.h:32/35/38/80/83);
        // values are the measured big-endian rodata words.
        const f32 KF_PAN_INPUT_FILTER_FACTOR     = 0.40000001f;  // flt_820662EC
        const f32 KF_PAN_Y_BORDER                = 200.0f;       // flt_820662F0
        const f32 FK_PAN_CURSOR_MOVEMENT_RADIUS  = 60.0f;        // flt_820662F4
        const f32 KF_PANNING_STOP_RESETTIME      = 0.20000000f;  // flt_820662F8
        const f32 KF_MAX_SNAP_SCREEN_DISTANCE    = 2500.0f;      // flt_820662FC

        // DWARF BrnCrashNavMap.cpp:1452 -- the on-stack snap-target list this screen hands
        // to the cursor. Corroborated by the X360 frame (see the banner).
        const s32 KI_SNAP_LIST_SIZE = 289;

        // CgsGui::GuiEventControllerAxis, minus the 12-byte GuiEvent header -- the in-queue
        // hands the state the stripped payload. Identical view to the one the committed
        // sibling BrnCrashNavEnterOnline_wI_05.cpp carries for the same event.
        struct ControllerAxisPayload : public CgsModule::Event
        {
            s32 miAxis;    // +0x00
            f32 mfXAxis;   // +0x04
            f32 mfYAxis;   // +0x08
        };
    }

    // --------------------------------------------------------------- MoveCursor @0x824BF100
    //
    // Feed one analogue-stick sample into the crash-nav map. Which of the two things it does
    // depends on the stick: the right stick (axis 1) PANS the map -- the cursor is pushed
    // toward a point offset from the map centre and the world position it lands on is clamped
    // to the map's world rect -- while the left stick / alternate pair (axes 0 and 2) simply
    // hand the raw delta to the cursor, and, if the map is already panning, decide when to
    // drop back out of panning.
    //
    // Panning is left in two ways: the player deflects one of the other sticks (immediate),
    // or the pan stick has been idle for KF_PANNING_STOP_RESETTIME and the cursor has drifted
    // within snapping range of an icon.
    //
    // The whole function is inert while an event is being inspected.
    void CrashNavMap::MoveCursor(const CgsModule::Event* lpEvent)
    {
        // `cmpwi cr6, r11, 2` + `beq` straight to the epilogue.
        if (meCursorMode == E_CURSORMODE_INSPECTING_ICONS)
        {
            return;
        }

        // X360 BrnCrashNavMap.cpp:1470. Non-fatal: the payload is dereferenced immediately
        // afterwards either way. The message names the DERIVED screen (CrashNavMapMain) even
        // though the code lives in the base -- kept verbatim.
        CGS_ASSERT(lpEvent != 0, "Invalid event in CrashNavMapMain::MoveCursor");

        // cpp:1432.
        const ControllerAxisPayload* lpAxisData =
            reinterpret_cast<const ControllerAxisPayload*>(lpEvent);

        if (lpAxisData->miAxis == KI_AXIS_LEFT_STICK ||
            lpAxisData->miAxis == KI_AXIS_ALTERNATE_STICK)
        {
            if (meCursorMode == E_CURSORMODE_PANNING)
            {
                // Any deflection on a non-pan stick drops panning immediately.
                bool lbStopPanning = (lpAxisData->mfXAxis != 0.0f) ||
                                     (lpAxisData->mfYAxis != 0.0f);

                // Otherwise the pan stick has to have been idle for the reset time before the
                // cursor is even allowed to look for something to snap back to.
                if (!lbStopPanning &&
                    mfMapPanningStopTime + KF_PANNING_STOP_RESETTIME < mpGuiCache->GetTime())
                {
                    // cpp:1452-1454.
                    Vector2 lSnapList[KI_SNAP_LIST_SIZE];
                    s32     liNumIcons = 0;
                    mpIconManager->GetSatNavIconPositions(lSnapList, &liNumIcons);

                    const u32 luNearestIcon =
                        mCursor.FindClosestSnapIndex(lSnapList, static_cast<u32>(liNumIcons));

                    // `vsubfp` of the winning snap location against the cursor lane, then the
                    // 2-lane squared magnitude -- no square root is taken on either side, the
                    // threshold is already squared (50 device units).
                    //
                    // FAITHFUL, and deliberately unguarded: `slwi r10, r3, 4` @0x824BF444
                    // indexes the list with whatever FindClosestSnapIndex returned, with no
                    // test against its KU_INVALID_SNAP_INDEX (0xFFFFFFFF) sentinel and no
                    // test on liNumIcons. The X360 really does read lSnapList[-1] when the
                    // list is empty; the surrounding reset-time gate is what keeps that from
                    // being reachable in practice. Do not "fix" it without evidence.
                    const Vector2 lv2CursorPos = mCursor.GetPosition();
                    Vector2 lv2ToSnapLocation;
                    lv2ToSnapLocation.x = lSnapList[luNearestIcon].x - lv2CursorPos.x;
                    lv2ToSnapLocation.y = lSnapList[luNearestIcon].y - lv2CursorPos.y;
                    lv2ToSnapLocation.z = 0.0f;   // vsubfp is all-lane; z/w are Vector2 padding
                    lv2ToSnapLocation.w = 0.0f;

                    lbStopPanning = KF_MAX_SNAP_SCREEN_DISTANCE >
                                    rw::math::vpu::Dot(lv2ToSnapLocation, lv2ToSnapLocation);
                }

                if (lbStopPanning)
                {
                    meCursorMode = E_CURSORMODE_SELECTING_ICONS;
                }
            }
            else
            {
                // Not panning: the stick just moves the cursor. See the banner for the PPC
                // float-argument ABI note that recovers these three arguments.
                mCursor.SetDelta(lpAxisData->mfXAxis,
                                 lpAxisData->mfYAxis,
                                 mpGuiCache->GetTime());
            }
        }
        else if (lpAxisData->miAxis == KI_AXIS_RIGHT_STICK)
        {
            // Any deflection on the pan stick ENTERS panning and drops every hover latch, so
            // the panel and the button prompts stop advertising whatever was under the cursor.
            if (lpAxisData->mfXAxis != 0.0f || lpAxisData->mfYAxis != 0.0f)
            {
                meCursorMode           = E_CURSORMODE_PANNING;
                mfMapPanningStopTime   = mpGuiCache->GetTime();
                muHoveredEventID       = 0;
                mHoveredDriveThruID    = 0;
                mHoveringRivalId       = 0;
                mpLockedIconName       = 0;
                mbLocalPlayerSelected  = false;

                // `stb r11, 0x6128(r30)` with r11 == 12: a bare byte store, i.e. the setter is
                // inlined here and its range assert does not fire. Parking the "tire shop"
                // type in the locked-icon record is what makes the panel-repaint switches
                // (UpdateRival / UpdateDrivethru) take their silent no-op arm while panning.
                mLockedIconInfo.SetIconType(
                    GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_TIRE_SHOP);

                UpdateButtonPrompts();
            }

            // Re-read: the block above may have just entered panning, and a pan sample that
            // did NOT enter it still has to be integrated while panning is already active.
            if (meCursorMode == E_CURSORMODE_PANNING)
            {
                // cpp:1493. The map centre is a point on the world XZ plane; Unflatten lifts
                // it to (x, 0, z) and WorldToDevice projects it back onto the screen. `li r3,
                // 0` @0x824BF228 is WorldToDevice's `lbClamp` argument -- the vector travels
                // in v1, so r3 is the FIRST scalar parameter, not a `this`.
                const Vector2 lv2MapCentre =
                    MapTransform::WorldToDevice(MapTransform::Unflatten(mv2WorldCentrePoint), false);

                // cpp:1494. The Y axis is inverted (`fmuls f0, f13, -1.0f`) because device
                // space grows downward.
                Vector2 lv2ControllerOffset;
                lv2ControllerOffset.x = lpAxisData->mfXAxis;
                lv2ControllerOffset.y = lpAxisData->mfYAxis * -1.0f;
                lv2ControllerOffset.z = 0.0f;
                lv2ControllerOffset.w = 0.0f;

                // cpp:1495. The cursor chases a point FK_PAN_CURSOR_MOVEMENT_RADIUS device
                // units from the map centre in the stick's direction, with a first-order
                // filter so the motion eases instead of snapping.
                const Vector2 lv2CursorPos = mCursor.GetPosition();
                Vector2 lv2CursorPosToMoveTo;
                lv2CursorPosToMoveTo.x =
                    (lv2ControllerOffset.x * FK_PAN_CURSOR_MOVEMENT_RADIUS + lv2MapCentre.x -
                     lv2CursorPos.x) * KF_PAN_INPUT_FILTER_FACTOR + lv2CursorPos.x;
                lv2CursorPosToMoveTo.y =
                    (lv2ControllerOffset.y * FK_PAN_CURSOR_MOVEMENT_RADIUS + lv2MapCentre.y -
                     lv2CursorPos.y) * KF_PAN_INPUT_FILTER_FACTOR + lv2CursorPos.y;
                lv2CursorPosToMoveTo.z = 0.0f;
                lv2CursorPosToMoveTo.w = 0.0f;

                // cpp:1501/1502. Round-trip through world space so the cursor can be held
                // inside the map's own bounds: x against the rect outright, z inset by
                // KF_PAN_Y_BORDER at both ends so the cursor never reaches the top/bottom
                // edge of the map.
                Vector3 lv3CursorWorldPos = MapTransform::DeviceToWorld(lv2CursorPosToMoveTo);
                // The X360 reads the world rect as a PROCESS-WIDE quad: `lis/addi r11,
                // flt_82FB31F0; lvx128 v0, r0, r11` @0x824BF2EC..0x824BF2FC -- an absolute
                // .data address, not `this`. That global is written only by
                // MainMapComponent::Construct/Prepare/CalculatePositionedWorldRect (see the
                // FLAG in BrnMainMap.h), and on this screen the component that wrote it IS
                // mMainMapComponent, whose DWARF instance member mv4WorldRect holds the same
                // quad. So the console's global read is spelled here as the DWARF-attested
                // instance accessor on that component; it is the same value, by name.
                const Vector4 lv4WorldRect = mMainMapComponent.GetWorldRect();

                // Four `fsel`s, transcribed sign for sign (see the NaN note in the banner).
                // fsel(a, b, c) == (a >= 0.0f) ? b : c.
                f32 lfClampedX =
                    (lv4WorldRect.x - lv3CursorWorldPos.x >= 0.0f) ? lv4WorldRect.x
                                                                   : lv3CursorWorldPos.x;
                f32 lfClampedZ =
                    ((lv4WorldRect.y + KF_PAN_Y_BORDER) - lv3CursorWorldPos.z >= 0.0f)
                        ? (lv4WorldRect.y + KF_PAN_Y_BORDER)
                        : lv3CursorWorldPos.z;

                lfClampedX = (lv4WorldRect.z - lfClampedX >= 0.0f) ? lfClampedX
                                                                   : lv4WorldRect.z;
                lfClampedZ = ((lv4WorldRect.w - KF_PAN_Y_BORDER) - lfClampedZ >= 0.0f)
                                 ? lfClampedZ
                                 : (lv4WorldRect.w - KF_PAN_Y_BORDER);

                lv3CursorWorldPos.x = lfClampedX;
                lv3CursorWorldPos.z = lfClampedZ;

                // `li r4, 0` -- second argument false; the vector goes in v1.
                mCursor.SetPosition(MapTransform::WorldToDevice(lv3CursorWorldPos, false), false);
            }
        }
    }
}

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"

#include "GameSource/Gui/BrnGuiEventTypeDefs.h"        // SatNavIconInfo / EIconDisplayType
#include "GameSource/Gui/SatNav/BrnMainMap.h"          // MainMapComponent::Update / SetDesiredWorldCentre
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"      // BrnGui::MapTransform
#include "rw/math/vpu/vector2_operation.h"             // rw::math::vpu::Dot (Vector2)

#include <cmath>                                       // sqrtf (the de-optimised vrsqrtefp chain)

namespace BrnGui
{
    namespace
    {
        // Scroll integrator constants. Names are DWARF-attested (BrnCrashNavMap.h:59/62/65);
        // values are the measured big-endian rodata words (see the banner).
        const f32 KF_MAP_SCROLL_TOLERANCE = 10.0f;          // flt_82065B68
        const f32 KF_MAP_SCROLL_ACCEL     = 0.029999999f;   // flt_820662E0 (0x3CF5C28F)
        const f32 KF_MAX_SCROLL_SPEED     = 0.80000001f;    // unk_820662E4 (0x3F4CCCCD),
                                                            // negated form flt_8200D564
    }

    // ----------------------------------------------------------- UpdateMainMap @0x824D8CB0
    //
    // The per-frame map driver: pull the map view toward the cursor while the player is
    // selecting or panning, hand the resulting scroll state to the audio debouncer, advance
    // the map component, refresh the icons and the cursor, then repaint whichever panel face
    // the current cursor mode owns.
    //
    // The scroll model is a spring-less integrator: once the cursor sits further than
    // KF_MAP_SCROLL_TOLERANCE (10 world units) from the map centre, the centre-to-cursor
    // vector feeds an acceleration into mv2MapScrollVelocity every frame and the map is asked
    // to chase mv2WorldCentrePoint + that velocity. Inside the tolerance the velocity is
    // dropped to zero outright (no decay) and the map stops.
    void CrashNavMap::UpdateMainMap()
    {
        // cpp:611. Also the argument UpdateSoundEvents needs, which is why it is hoisted
        // out of the branch (`li r30, 0` @0x824D8CC8, `li r30, 1` @0x824D8DD8).
        bool lbMapIsScrolling = false;

        // `lwz r11, 0x5F50(r31)` + two cmpwi: only the two cursor modes that let the player
        // drive the map scroll it.
        if (meCursorMode == E_CURSORMODE_SELECTING_ICONS ||
            meCursorMode == E_CURSORMODE_PANNING)
        {
            // cpp:618 / cpp:621. The cursor lives in device space; the map centre lives in
            // the (x,z) world plane, so the cursor is projected through DeviceToWorld and
            // flattened before the two can be subtracted.
            const Vector2 lv2CursorPos = mCursor.GetPosition();
            const Vector2 lv2CursorWorld = MapTransform::Flatten(MapTransform::DeviceToWorld(lv2CursorPos));

            Vector2 lv2MapCentreToCursorWorld;
            lv2MapCentreToCursorWorld.x = lv2CursorWorld.x - mv2WorldCentrePoint.x;
            lv2MapCentreToCursorWorld.y = lv2CursorWorld.y - mv2WorldCentrePoint.y;
            lv2MapCentreToCursorWorld.z = 0.0f;   // vsubfp is all-lane; z/w are Vector2 padding
            lv2MapCentreToCursorWorld.w = 0.0f;   // and are never read back (types.h)

            // cpp:616. The X360 spells this vrsqrtefp + two Newton-Raphson steps + a
            // zero-length vsel; sqrtf of the 2-lane dot product is the sanctioned scalar
            // de-optimisation and yields the same value (including 0 for a zero delta).
            const f32 lfMagnitude =
                sqrtf(rw::math::vpu::Dot(lv2MapCentreToCursorWorld, lv2MapCentreToCursorWorld));

            if (lfMagnitude > KF_MAP_SCROLL_TOLERANCE)
            {
                // `vmaddfp v0, v9, v13, v0` -- velocity += delta * accel, then stored back
                // BEFORE the clamp (@0x824D8DF4), so the magnitude below is taken from the
                // freshly accumulated velocity.
                mv2MapScrollVelocity.x += lv2MapCentreToCursorWorld.x * KF_MAP_SCROLL_ACCEL;
                mv2MapScrollVelocity.y += lv2MapCentreToCursorWorld.y * KF_MAP_SCROLL_ACCEL;

                // cpp:617. vmaxfp(a,b) = a > b ? a : b, vminfp(a,b) = a < b ? a : b -- kept in
                // the console's operand order. The lower bound can never bite (a magnitude is
                // non-negative) but the X360 emits it, so it stays.
                f32 lfClampedMagnitude =
                    sqrtf(rw::math::vpu::Dot(mv2MapScrollVelocity, mv2MapScrollVelocity));
                if (-KF_MAX_SCROLL_SPEED > lfClampedMagnitude)
                {
                    lfClampedMagnitude = -KF_MAX_SCROLL_SPEED;
                }
                if (KF_MAX_SCROLL_SPEED < lfClampedMagnitude)
                {
                    lfClampedMagnitude = KF_MAX_SCROLL_SPEED;
                }

                // See the banner: the velocity is scaled BY the clamped magnitude, not by
                // clamped/magnitude. Measured, not inferred.
                mv2MapScrollVelocity.x *= lfClampedMagnitude;
                mv2MapScrollVelocity.y *= lfClampedMagnitude;

                // `stvx128 v0, r31, 0x6B0` -- the store lands on mMainMapComponent's own
                // mv2DesiredCentre (component-relative +1616), i.e. the inlined setter.
                Vector2 lv2DesiredCentre;
                lv2DesiredCentre.x = mv2WorldCentrePoint.x + mv2MapScrollVelocity.x;
                lv2DesiredCentre.y = mv2WorldCentrePoint.y + mv2MapScrollVelocity.y;
                lv2DesiredCentre.z = 0.0f;
                lv2DesiredCentre.w = 0.0f;
                mMainMapComponent.SetDesiredWorldCentre(lv2DesiredCentre);

                lbMapIsScrolling = true;
            }
            else
            {
                // `stvx128 v12, r31, 0x6070` with v12 == vspltisw 0: the whole lane, not a
                // decay.
                mv2MapScrollVelocity.SetZero();
            }
        }

        UpdateSoundEvents(lbMapIsScrolling);

        // The map component consumes the current centre and returns the interpolated one
        // (sret in the asm: `addi r3, r1, <buf>` + `addi r4, r31, 0x60` with the argument
        // lane in v1, then the returned lane is stored straight back).
        mv2WorldCentrePoint = mMainMapComponent.Update(mv2WorldCentrePoint);

        UpdateIconManager();
        UpdateCursorStatus();

        // The panel-repaint dispatch. Three cursor modes own a panel face; NONE and PANNING
        // leave the panel to UpdateIconManager.
        if (meCursorMode == E_CURSORMODE_SELECTING_ICONS ||
            meCursorMode == E_CURSORMODE_INSPECTING_ICONS ||
            meCursorMode == E_CURSORMODE_ZOOMEDOUT)
        {
            // `lwz r11, 0x6084` == 5 (the display-type sentinel) OR `lwz r11, 0x48` == 0
            // (no cache yet) routes away from the event panel. Kept in the console's order.
            if (meEventIconDisplayType == GuiEventEnableSatNavIcons::E_ICON_DISPLAY_TYPE_COUNT ||
                mpGuiCache == 0)
            {
                // Each of the three flags is `lbz` + `cmplwi ..., 1`, i.e. compared against
                // exactly 1; the members are bool, so the plain test is equivalent.
                if (mbSelectDriveThrus)
                {
                    UpdateDrivethru();
                }
                else if (mbSelectRivals)
                {
                    UpdateRival();
                }
                else if (mbUseRoadSigns &&
                         mpLockedIconName == 0 &&
                         mLockedIconInfo.GetIconType() ==
                             GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_PLAYER_CAR)
                {
                    // Road-sign screens with nothing locked fall back to the player face of
                    // the rival panel. GetIconType is a real `bl` @0x824D8F38 (it fires the
                    // two range asserts), so the inline GetIconTypeByte() accessor would drop
                    // them -- call GetIconType().
                    mCrashNavPanel.SetRivalPanelData();
                }
            }
            else
            {
                UpdateEvent();
            }
        }
    }
}

// ============================================================================
// FOLDED FROM BrnCrashNavMap_wJ_08.cpp (wave J) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===================================================================================
// BrnGui::CrashNavMap -- wave-J partfile 08: the frame pump and the icon-hover resolver.
//   Update            @0x824DD6D8  (the state machine's per-frame virtual)
//   UpdateIconManager @0x824CBA70  (cpp:1119 region)
//
// Both bodies are read store-for-store off the raw X360 assembly
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x824DD6D8.json and /0x824CBA70.json, dumped to
// scratchpad/waveJ/asm_g08_update.txt and asm_g08_uim.txt), with Hex-Rays arbitrated
// against the asm everywhere the two disagree.
//
// RECONCILIATION PASS (2026-08-03). The wave-J shared-header requests this group filed
// have since been applied by the header owner, so the two bodies are landed here and the
// names are spelled the way the committed headers now spell them. What changed against
// the parked drafts:
//
//   * GuiCursor::IsAlwaysSnap()  ->  GuiCursor::GetAlwaysSnap()
//     The request had invented the `Is` spelling. DWARF
//     references/DecFIGS/dwarfdump/GameSource/Gui/Flow/Screen/Components/BrnCursor.h:348
//     declares `bool GetAlwaysSnap();` (non-const) and that is what
//     b5-decomp/src/GameSource/Gui/Flow/Screen/Components/BrnCursor.h:201 now carries.
//     GuiCursor::GetPosition() likewise returns Vector2 BY VALUE (DWARF h:334), which is
//     what MapTransform::DeviceToWorld(Vector2) takes, so that call site is unchanged.
//
//   * GuiEventUpdateSatNav::SatNavI(...)  ->  SatNavIconInfo::GetIconType()
//     `SatNavI` was IDA's TRUNCATED symbol for 0x823A6B30, not a function. That body is
//     `lbz r11, 0x28(r3); extsb r31, r11` guarded by the two CGS_ASSERTs from
//     "..\..\..\GameSource\Gui/BrnGuiEventTypeDefs.h" (`li r5,0x777`/`0x778`), i.e. it
//     reads +0x28 of ITS ARGUMENT -- and at 0x824CBEFC / 0x824CBFC0 that argument is the
//     SatNavIconInfo* returned by GetDriveThroughOrJunkyardAtIndex, not the event. DWARF
//     BrnGuiEventTypeDefs.h:1731 names it `SatNavIconType GetIconType() const;` and the
//     committed BrnGuiEventTypeDefs.h:199 already declares it. Both drive-through arms
//     now read the icon's own type, with no cast.
//
// STILL BLOCKED -- ONE STATEMENT, TWO MISSING DECLARATIONS (UpdateIconManager, below).
// Measured, not guessed, from 0x824CBAC4..0x824CBAEC:
//
//     lwz   r10, 0x4C(r31)          ; r10 = mpIconManager
//     lfs   f13, 0x6C0(r31)         ; CrashNavMap+0x6C0 == mMainMapComponent(+96) + 1632
//     ori   r9,  r11, 0xA198        ; r9  = 0xA198
//     lfs   f0,  flt_82F259E0       ; 3500.0f  (scratchpad/waveJ/crashnav_consts.txt)
//     fdivs f0,  f0, f13
//     stfsx f0,  r10, r9            ; mpIconManager + 0xA198 <- 3500.0f / zoom
//
//   (a) The DESTINATION, mpIconManager + 0xA198, has no member in the committed
//       b5-decomp/src/GameSource/Gui/SatNav/BrnMapIconManager.h -- it falls in the
//       "[further selection/flag state: not modelled here]" gap between mRoadSignIconManager
//       (+0x7090) and mpGuiCache (+0xA9F8). It has NO DWARF row either: the DecFIGS
//       MapIconManager has no float member at all, and a repo-wide grep of
//       .ida-exports/BURNOUT_X360_ARTIST.XEX for the immediate 0xA198 matches exactly ONE
//       function -- this one -- so the field has no other writer and no reader I can point
//       at to name it. It therefore stays consumer-named and UNLANDED rather than invented.
//   (b) The SOURCE, MainMapComponent::mfWorldZoomScaleFactor (DWARF BrnMainMap.h:220,
//       X360 comp+1632), is `private` in the committed BrnMainMap.h with no accessor and
//       no `friend struct CrashNavMap`. The DWARF has no getter for it either.
//
// So this one store needs a header edit that is not mine to make (the parallel case,
// MapIconManager's flag tail, was solved by `friend struct CrashNavMap` + named members --
// BrnMapIconManager.h:233). The statement is kept in place, spelled the way it would be
// once those two declarations exist, and this partfile is reported as still_blocked
// rather than having the store deleted or faked. Everything else in both bodies was
// compile-verified against the committed headers with that single statement elided
// (scratchpad/waveJ/probe_CrashNavMap_8_recon/) -- zero further diagnostics.
//
// LINK-TIME EXTERNALS (cl /c cannot see these; reported, not fabricated):
//   LobbyNameCmp @0x82B10050, MapIconManager::{UpdateSatNavInfo, SetIconsVisible, Update,
//   GetSatNavIconPositions, GetRivalIconAtIndex, GetRoadSignNameAtIndex, GetEventIDAtIndex,
//   GetDriveThroughAndJunkyardCount, GetDriveThroughOrJunkyardAtIndex, SetRoadRuleBatchData},
//   CrashNavPanel::{Update, RecEvent, ShowBlank, SetEventPanelData}, GuiCursor::
//   {UpdateToSnapLocations, SetPosition}, MainMapComponent::{RecvEvent, IsZooming},
//   MapTransform::{WorldToDevice, DeviceToWorld}, SatNavIconInfo::{SetIconType, GetIconType},
//   GuiCache::{AreAllAptComponentsInitialised, GetWorldCameraPosition, GetOnlinePlayerInfo,
//   GetProfileEventDisplayInfo}.
// ===================================================================================


// LobbyNameCmp @0x82B10050 -- the DirtySock collation-table string compare (it skips
// characters whose collation class is 1). No header in the tree declares it and no body
// exists here, so it is declared per-cpp exactly as
// GameSource/.../CgsServerInterfaceGames.cpp:47 does. LINK-TIME EXTERNAL: reported, not
// fabricated.
extern "C" s32 LobbyNameCmp(const char* lpcA, const char* lpcB);

namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) ------------------------
        // Measured from the three template instantiations this TU calls:
        //   OutputGuiEvent<GuiEventRoadRuleDataRequest>        @0x82476EE8  `li r5,0x28`
        //   OutputGuiEvent<GuiEventChallengedEventDataRequest> @0x824C2F50  `li r5,0x28`
        //   OutputViewState<GuiEventSetHoveredEventIcon>       @0x824C2EE8  `li r5,0x29`
        const s32 KI_CHANNEL_GUI_EVENT  = 40;   // GuiEventOut
// (fold: an identical definition of KI_CHANNEL_VIEW_STATE was dropped here -- this TU defines it once, above)

        // ---- the in-queue wire ids Update dispatches on -----------------------------
        // (the jump-table base is `addi r11, r28, -6` @0x824DD774, and the three
        //  above-199 ids come from the `cmpwi` chain at 0x824DD9A8..0x824DD9BC)
        const s32 KI_EVENT_CONTROLLER_INPUT_PRESSED       = 6;
        const s32 KI_EVENT_CONTROLLER_INPUT_RELEASED      = 7;
        const s32 KI_EVENT_CONTROLLER_AXIS                = 8;
        const s32 KI_EVENT_GUI_CACHE                      = 64;
        const s32 KI_EVENT_UPDATE_SATNAV                  = 199;   // 0xC7
        const s32 KI_EVENT_CHALLENGED_EVENT_DATA_RESPONSE = 332;   // 0x14C
        const s32 KI_EVENT_ROAD_RULE_DATA                 = 334;   // 0x14E
        const s32 KI_EVENT_ROAD_RULE_BATCH_DATA_RESPONSE  = 344;   // 0x158

        // The wire id the screen posts back once it has accepted the GuiCache
        // (Update's id-64 arm; `li r19, 0x148` @0x824DD764).
        const s32 KI_EVENT_CRASHNAV_CACHE_ACCEPTED = 328;

        // The wire id of the hover-set publication (`li r11, 0x22F` inside
        // OutputViewState<GuiEventSetHoveredEventIcon> @0x824C2F34).
        const s32 KI_EVENT_SET_HOVERED_EVENT_ICON = 559;

        // The icons are authored against this reference world scale; UpdateIconManager
        // divides it by the map's live zoom factor. X360 flt_82F259E0, measured 3500.0f
        // (scratchpad/waveJ/crashnav_consts.txt) -- UpdateIconManager is its only reader.
        const f32 KF_EVENT_ICON_REFERENCE_SCALE = 3500.0f;

// (fold: an identical definition of KI_MAX_ONLINE_PLAYERS was dropped here -- this TU defines it once, above)

        // Capacity of UpdateIconManager's stack snap-location buffer. NOT a recovered
        // source constant -- the X360 encodes no immediate for it. DERIVED: the buffer
        // starts at sp+0x90 inside a 0x12E0-byte frame and the lanes are 16 bytes, so
        // (0x12E0 - 0x90) / 16 == 293 is the array's upper bound. Flagged rather than
        // rounded to a plausible-looking number.
        const s32 KI_MAX_SNAP_LOCATIONS = 293;

// (fold: an identical definition of KAC_ASSERT_FILE was dropped here -- this TU defines it once, above)

        // The state input queue: CgsGui::State::mpInGuiEventQueue is declared as an
        // opaque InputBuffer::GuiEventQueue*, and the X360 calls
        // VariableEventQueue<18432,16>::GetFirstEvent/GetNextEvent on it. Same typedef +
        // reinterpret_cast as every other committed GUI state (BrnBootAttract.cpp:15,
        // BrnCarSelectVehicle.cpp:54, ...).
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // ---- in-queue payload views -------------------------------------------------
        // The queue hands the state the HEADER-STRIPPED payload, so a typed event pointer
        // would mislead by 12 bytes; these are the wH_00 / BrnInGame.cpp idiom.

        // id 64: a bare GuiCache pointer (X360 `lwz r11, 0(r29)` -- a console u32, a real
        // pointer on the host).
        struct GuiCachePayload : public CgsModule::Event
        {
            GuiCache* mpGuiCache;   // +0x00
        };

        // id 332 (GuiEventChallengedEventDataResponse). Offsets measured at
        // 0x824DDA48..0x824DDA6C: the leading field is read with a 64-bit `ld` and
        // compared with `cmpld` against the zero-extended muHoveredEventID, so it is one
        // 8-byte id and NOT two words (Hex-Rays' `v5[1]` is the low half only). The
        // handler then lifts the +0x08 word and memcpy's the sixteen bytes at +0x0C.
        // FLAG consumer-named: only the leading id and the +0x08 score word have a
        // recovered role -- see BrnCrashNavMap_08_Update.cpp's banner.
        struct GuiEventChallengedEventDataResponsePayload : public CgsModule::Event
        {
            u64 mu64EventID;          // +0x00  (`ld` / `cmpld`)
            u32 muScoreOverride;      // +0x08  (`lwz`, becomes the panel record's word 0)
            u8  maReserved_0C[16];    // +0x0C..+0x1B (`memcpy` 16 into the panel record)
        };

        // ---- out-queue payload views ------------------------------------------------

        // Wire id 328, posted by Update's id-64 arm. The X360 builds { 1, 328, 12 } and
        // AddEvent's 16 bytes on channel 40 -- and NEVER stores the payload byte itself
        // (there is no `stb` to the record's +12 anywhere in the function), so on the
        // console it ships whatever was on the stack. Modelled zeroed. The size word is
        // sizeof(payload) == 1, NOT 4.
        struct GuiEventCrashNavCacheAcceptedPayload
        {
            bool mbFlag;   // +0x00  never written by the X360; role unrecovered

            GuiEventCrashNavCacheAcceptedPayload() : mbFlag(false) {}
            s32 GetEventType() const { return KI_EVENT_CRASHNAV_CACHE_ACCEPTED; }
        };

        // Wire id 559, posted by UpdateIconManager (both exits). Layout measured from the
        // OutputViewState<GuiEventSetHoveredEventIcon> body @0x824C2EE8 (three `ld`/`std`
        // pairs = 24 payload bytes, record offset word 0x10) cross-checked against the two
        // build sites (0x824CBB5C.. and 0x824CC094..), which fill +0x00 / +0x08 / +0x10 /
        // +0x14 from mHoveredDriveThruID / mHoveringRivalId / muHoveredEventID /
        // mpLockedIconName.
        //
        // NOT the homed BrnGui::GuiEventSetHoveredEventIcon: that catalogue entry models
        // the type as GuiEvent<559> + maPayload[12], i.e. only twelve bytes of actual
        // payload, which cannot cover the measured +0x00..+0x17 writes. Flagged for a
        // future catalogue sweep; kept file-local here rather than widening a shared catalogue
        // entry on one consumer's evidence.
        //
        // HOST WIDTH: the fourth field is a char POINTER. It is a 4-byte word on the X360
        // (payload 24, record 40) and 8 bytes on the x64 host (payload 32, record 48).
        // Never write those console numbers as literals -- the poster takes sizeof().
        struct alignas(8) GuiEventSetHoveredEventIconPayload
        {
            CgsID       mHoveredDriveThruId;   // +0x00
            CgsID       mHoveringRivalId;      // +0x08
            u32         muHoveredEventId;      // +0x10
            const char* mpcLockedIconName;     // +0x14 on X360; +0x18 on the host
            s32 GetEventType() const { return KI_EVENT_SET_HOVERED_EVENT_ICON; }
        };

        // ---- lane conversions -------------------------------------------------------
        // The X360 moves whole 16-byte lanes between the icon records, MapTransform and
        // the cursor with lvx128/stvx128, so all four floats travel. Vector2/Vector3/
        // Vector4 are distinct PODs on the host, hence these lane-for-lane copies. They
        // are transliteration plumbing, not recovered functions.
        inline Vector3 LaneAsVector3(const Vector4& lv4Lane)
        {
            const Vector3 lv3Lane = { lv4Lane.x, lv4Lane.y, lv4Lane.z, lv4Lane.w };
            return lv3Lane;
        }

        inline Vector4 LaneAsVector4(const Vector3& lv3Lane)
        {
            const Vector4 lv4Lane = { lv3Lane.x, lv3Lane.y, lv3Lane.z, lv3Lane.w };
            return lv4Lane;
        }

        // ---- shared helpers ---------------------------------------------------------

        // Publish the current hover set to the view. The X360 stack-builds the
        // OutputViewState wrapper record and calls
        // VariableEventQueue<65536,16>::AddEvent(iface + 0xC, &record, 41, sizeof(record))
        // -- which is exactly what GuiEventWrapper<T, 41> + GetOutputEventQueue()->AddEvent
        // produces (the wave-H BrnOnlineGameRoomPlayerInfo_wH_09.cpp precedent). Written
        // as a helper because UpdateIconManager emits the identical sequence twice.
        //
        // (CgsGui::StateInterface declares OutputGuiEvent<T> but not OutputViewState<T>;
        //  the missing member is a pre-existing tree gap -- CgsGuiStateInterface_
        //  OutputViewState_Inst.cpp names a member that does not exist -- and is reported,
        //  not worked around here.)
        void PostSetHoveredEventIcon(CgsGui::StateInterface* lpStateInterface,
                                     CgsID       lHoveredDriveThruId,
                                     CgsID       lHoveringRivalId,
                                     u32         luHoveredEventId,
                                     const char* lpcLockedIconName)
        {
            GuiEventSetHoveredEventIconPayload lPayload;
            lPayload.mHoveredDriveThruId = lHoveredDriveThruId;
            lPayload.mHoveringRivalId    = lHoveringRivalId;
            lPayload.muHoveredEventId    = luHoveredEventId;
            lPayload.mpcLockedIconName   = lpcLockedIconName;

            CgsGui::GuiEventWrapper<GuiEventSetHoveredEventIconPayload, KI_CHANNEL_VIEW_STATE>
                lRecord(lPayload);
            lpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lRecord),
                lRecord.GetChannel(),
                static_cast<s32>(sizeof(lRecord)));
        }

// (fold: an identical definition of FindOnlinePlayerByActiveRaceCarIndex was dropped here -- this TU defines it once, above)
    }

    // ================================================================================
    //  Update  @ 0x824DD6D8  (the state machine's per-frame virtual)
    //
    //  The screen's per-frame pump: reset the shared icon manager's used-icon count,
    //  drain the state in-queue (controller input, the map cursor axis, the GuiCache
    //  hand-off, sat-nav icon refreshes, road-rule and challenged-event responses),
    //  giving the main map and the crash-nav panel a look at every event on the way
    //  past, then run the map itself and finish the one-shot component set-up as soon
    //  as the cache reports every expected apt component initialised.
    // ================================================================================
    void CrashNavMap::Update()
    {
        // X360 `stw r10(=0), 0x990(r11)` -- the count is rebuilt from scratch by
        // UpdateIconManager later in the frame.
        if (mpIconManager != 0)
            mpIconManager->miNumUsedIcons = 0;

        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liEventSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize))
        {
            // [DIAG] NOT IN THE X360 BINARY -- [cnav-diag] the input events the map screen drains.
            if ((liEventId == 6 || liEventId == 7) && getenv("BRN_SATNAV_DIAG") != 0 && CgsDev::Log::gpDebugPrint != 0)
                *CgsDev::Log::gpDebugPrint << "[cnav-diag] map input event " << liEventId << " action "
                    << *reinterpret_cast<const s32*>(reinterpret_cast<const u8*>(lpEvent) + 4) << "\n";
            switch (liEventId)
            {
            case KI_EVENT_CONTROLLER_INPUT_PRESSED:
                // vtable +0x24 -- the derived screens' input maps.
                HandleCrashNavInputPressed(lpEvent);
                break;

            case KI_EVENT_CONTROLLER_INPUT_RELEASED:
                // vtable +0x28.
                HandleCrashNavInputReleased(lpEvent);
                break;

            case KI_EVENT_CONTROLLER_AXIS:
                // The cursor cannot move before the cache has arrived (MoveCursor
                // dereferences it), so the X360 gates the call on mpGuiCache.
                if (mpGuiCache != 0)
                    MoveCursor(lpEvent);
                break;

            case KI_EVENT_GUI_CACHE:
                {
                    // The in-queue hands the state the HEADER-STRIPPED payload; id 64's
                    // payload is a bare GuiCache pointer (X360 `lwz r11, 0(r29)` -- a
                    // console u32, a real pointer on the host).
                    const GuiCachePayload* lpPayload =
                        static_cast<const GuiCachePayload*>(lpEvent);

                    if (lpPayload->mpGuiCache == 0)
                    {
                        // Streamed diagnostic; the message names GenericHudState::Update
                        // (copy-paste in the original source) -- kept verbatim. Non-fatal.
                        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                        CgsDev::StrStream lStrStream(lacMessageBuffer,
                                                     CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                        lStrStream << "Invalid cache in GenericHudState::Update";
                        CgsDev::Assert::BeginAssert();
                        CgsDev::Assert::FireAssert(lStrStream.GetBuffer(),
                                                   KAC_ASSERT_FILE, 330);
                        CgsDev::Assert::EndAssert();
                    }

                    // Only the FIRST cache event is acted on: once mpGuiCache is latched
                    // the whole arm is skipped (`lwz r11,0x48` / `bne` @0x824DD918).
                    if (mpGuiCache == 0)
                    {
                        mpGuiCache = lpPayload->mpGuiCache;
                        ResetIconManager(lpEvent);

                        CGS_ASSERT(mpIconManager != 0, "NULL != mpIconManager");  // cpp:339

                        mpIconManager->SetIconsVisible(true);

                        // The X360 stack-builds the OutputGuiEvent wrapper record inline:
                        // { payload size 1, type 328, payload offset 12 } posted at 16
                        // bytes on channel 40. It never writes the payload byte itself.
                        GuiEventCrashNavCacheAcceptedPayload lCacheAccepted;
                        CgsGui::GuiEventWrapper<GuiEventCrashNavCacheAcceptedPayload,
                                                KI_CHANNEL_GUI_EVENT> lRecord(lCacheAccepted);
                        mpStateInterface->GetOutputEventQueue()->AddEvent(
                            reinterpret_cast<const CgsModule::Event*>(&lRecord),
                            lRecord.GetChannel(),
                            static_cast<s32>(sizeof(lRecord)));
                    }
                }
                break;

            case KI_EVENT_UPDATE_SATNAV:
                if (mpIconManager != 0)
                {
                    mpIconManager->UpdateSatNavInfo(
                        reinterpret_cast<const GuiEventUpdateSatNav*>(lpEvent));
                }
                break;

            case KI_EVENT_CHALLENGED_EVENT_DATA_RESPONSE:
                {
                    CGS_ASSERT(lpEvent != 0, "lpEventData");   // cpp:375 (non-fatal)

                    const GuiEventChallengedEventDataResponsePayload* lpResponse =
                        static_cast<const GuiEventChallengedEventDataResponsePayload*>(lpEvent);

                    // 64-bit compare (see the ASM note in the banner): the response is
                    // only relevant while the cursor is still on the event that asked
                    // for it. muHoveredEventID widens to 64 bits for the compare exactly
                    // as the X360's `mr r10, r30` after an `lwz` does.
                    const u32 luHoveredEventId = muHoveredEventID;
                    if (lpResponse->mu64EventID == luHoveredEventId)
                    {
                        CrashNavPanel::ChallengedEventScore lScore;
                        lScore.muScoreOverride = lpResponse->muScoreOverride;
                        std::memcpy(lScore.maReserved_04, lpResponse->maReserved_0C,
                                    sizeof(lScore.maReserved_04));

                        mCrashNavPanel.SetEventPanelData(luHoveredEventId, &lScore, true);
                    }
                }
                break;

            case KI_EVENT_ROAD_RULE_DATA:
                UpdateRoadRule(lpEvent);
                break;

            case KI_EVENT_ROAD_RULE_BATCH_DATA_RESPONSE:
                CGS_ASSERT(lpEvent != 0, "lpRoadRules");            // cpp:362 (non-fatal)
                CGS_ASSERT(mpIconManager != 0, "mpIconManager");    // cpp:363 (non-fatal)
                mpIconManager->SetRoadRuleBatchData(
                    reinterpret_cast<const GuiEventRoadRuleBatchDataResponse*>(lpEvent));
                break;

            default:
                break;
            }

            // EVERY event -- handled or not -- is also offered to the map component and
            // to the panel. A panel that answers true has changed its icon filter.
            mMainMapComponent.RecvEvent(lpEvent, liEventId);
            if (mCrashNavPanel.RecEvent(lpEvent, liEventId, liEventSize))
                SetFilterFromPanel();
        }

        // `lbz r11, 0x44(r31)` / `cmplwi cr6, r11, 1` -- an explicit compare against 1,
        // not a plain zero test.
        if (mbIsScreenLoaded)
            UpdateMainMap();

        CheckForLoadComplete();

        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:417 (non-fatal)

        // One-shot: the frame the cache first reports every expected apt component
        // initialised, wire the components up and paint the button prompts.
        if (!mbItemsLoaded && mbIsScreenLoaded &&
            mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
        {
            mbItemsLoaded = true;   // stored BEFORE the virtual call, per the asm
            // [DIAG] NOT IN THE X360 BINARY -- [cnav-diag] the one-shot wiring frame.
            if (getenv("BRN_SATNAV_DIAG") != 0 && CgsDev::Log::gpDebugPrint != 0)
                *CgsDev::Log::gpDebugPrint << "[cnav-diag] CrashNavMap items loaded -> SetupComponents/SetFilterFromPanel\n";
            SetupComponents();      // vtable +0x30
            SetFilterFromPanel();
            UpdateButtonPrompts();
        }

        mCrashNavPanel.Update();
    }

    // ================================================================================
    //  UpdateIconManager  @ 0x824CBA70  (cpp:1119 region)
    //
    //  Drive the shared map-icon manager for this frame and resolve what the cursor is
    //  hovering over. Feeds the manager the current zoom-derived icon scale and event
    //  context, then -- unless the map is mid-zoom with the cursor already locked --
    //  collects every on-screen icon position, snaps the cursor to the nearest one and
    //  latches the hovered thing (local player / rival / road sign / drive-through /
    //  event) into the state's hover members. Whatever the outcome, it publishes the
    //  hover set to the view so the renderer can highlight it.
    // ================================================================================
    void CrashNavMap::UpdateIconManager()
    {
        // The whole body is inside `if (mpIconManager)` on the X360 (0x824CBA8C jumps
        // straight to the epilogue); an early return reads better and is identical.
        if (mpIconManager == 0)
            return;

        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:1123 (non-fatal)

        // Icons are authored at a reference world scale; the manager needs the ratio
        // against the map's current zoom. X360: `lfs f13, mMainMapComponent+1632` /
        // `lfs f0, flt_82F259E0` (measured 3500.0f -- scratchpad/waveJ/crashnav_consts.txt,
        // and UpdateIconManager is that constant's ONLY reader) / `fdivs` / `stfsx`.
        //
        // The destination +0xA198 is NOT a MapIconManager member: it is
        // RoadSignIconManager::mfZoomFactor (DWARF BrnRoadSignIconManager.h:273) inside the
        // embedded mRoadSignIconManager, which BrnMapIconManager.h documents at its
        // SetZoomFactor declaration -- the X360 fully inlined that forwarding call, leaving
        // only the single stfsx. Going through the declared method restores the real call.
        // The source is MainMapComponent::mfWorldZoomScaleFactor (DWARF BrnMainMap.h:220,
        // X360 comp+1632); it is private with no DWARF accessor, so this class is a friend.
        mpIconManager->SetZoomFactor(
            KF_EVENT_ICON_REFERENCE_SCALE / mMainMapComponent.mfWorldZoomScaleFactor);

        // Two more straight member stores (`stb ...,0x998` and `stbx ...,0xAA18`); the
        // X360 re-loads mpIconManager before each one.
        mpIconManager->mi8CurrentEventIndex   = mi8CurrentEventIndex;
        mpIconManager->mbIsDisplayingEventInfo = mbIsInEvent;

        if (mbItemsLoaded)
            mpIconManager->Update();

        // While the map is animating a zoom the cursor must not re-snap -- it stays glued
        // to whatever it was already locked onto, re-projected each frame. Panning is
        // excluded because the cursor is being driven by the stick there.
        if (mMainMapComponent.IsZooming() && mbIsCursorLockedToIcon &&
            meCursorMode != E_CURSORMODE_PANNING)
        {
            mCursor.SetPosition(
                MapTransform::WorldToDevice(LaneAsVector3(mLockedIconInfo.GetPositionLane()),
                                            false),
                false);

            PostSetHoveredEventIcon(mpStateInterface, mHoveredDriveThruID, mHoveringRivalId,
                                    muHoveredEventID, mpLockedIconName);
            return;   // the X360 tail-returns here; no UpdateButtonPrompts on this path
        }

        // X360 r25: set once the snapped-icon block is entered and cleared again by the
        // road-sign arm (which owns mpLockedIconName). Every other arm therefore drops the
        // road-sign name at the end of the block. Modelled as a named bool in place of the
        // decompiler's v9 / LABEL_57 goto web.
        bool lbClearLockedIconName = false;

        switch (meCursorMode)
        {
        case E_CURSORMODE_NONE:
            // First frame on the map: park the cursor on the player and start selecting.
            PlaceCursorOnPlayer();
            meCursorMode = E_CURSORMODE_SELECTING_ICONS;
            break;

        case E_CURSORMODE_SELECTING_ICONS:
        case E_CURSORMODE_ZOOMEDOUT:
            {
                Vector2 lav2IconPositions[KI_MAX_SNAP_LOCATIONS];
                s32     liNumIcons = 0;
                mpIconManager->GetSatNavIconPositions(lav2IconPositions, &liNumIcons);

                const GuiCursor::SnapResults lSnapResults =
                    mCursor.UpdateToSnapLocations(lav2IconPositions,
                                                  static_cast<u32>(liNumIcons), false);

                if (liNumIcons == 0)
                {
                    // Nothing on the map at all.
                    if (meEventIconDisplayType !=
                        GuiEventEnableSatNavIcons::E_ICON_DISPLAY_TYPE_OFFLINE_EVENTS)
                    {
                        mCrashNavPanel.ShowBlank();
                    }
                    else
                    {
                        muHoveredEventID = 0;
                        mCrashNavPanel.SetEventPanelData(0, 0, false);
                    }
                }
                else if (lSnapResults.luLockedIndex ==
                         GuiCursor::SnapResults::KI_INVALID_LOCK_INDEX)
                {
                    // The cursor drifted off every icon. Only an unsnapped cursor lets go.
                    if (!mCursor.GetAlwaysSnap() && mbIsCursorLockedToIcon)
                    {
                        mbIsCursorLockedToIcon = false;
                        mpLockedIconName       = 0;
                        muHoveredEventID       = 0;
                    }
                }
                else if (!mbIsCursorLockedToIcon || mCursor.GetAlwaysSnap())
                {
                    const s32 liSnappedIndex = static_cast<s32>(lSnapResults.luLockedIndex);

                    lbClearLockedIconName  = true;
                    mbIsCursorLockedToIcon = true;

                    if (liSnappedIndex == liNumIcons - 1)
                    {
                        // GetSatNavIconPositions always appends the local player's icon
                        // last, so the final index is the player.
                        mLockedIconInfo.SetIconType(
                            GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_PLAYER_CAR);
                        mLockedIconInfo.SetPositionLane(mpGuiCache->GetWorldCameraPosition());
                        mbLocalPlayerSelected = true;
                        muHoveredEventID      = 0;
                        mHoveredDriveThruID   = 0;
                        mHoveringRivalId      = mpGuiCache->GetLocalPlayerCarId();

                        // The X360 schedules this test below the stores above (the cache
                        // is dereferenced by the lvx either way); non-fatal, so the
                        // position is behaviourally irrelevant -- kept where the asm has it.
                        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:1222

                        // Name the player from the online roster, matched on the local
                        // player's active-race-car index.
                        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData*
                            lpPlayerInfo = FindOnlinePlayerByActiveRaceCarIndex(
                                mpGuiCache, mpGuiCache->GetPlayerActiveRaceCarIndex());

                        if (lpPlayerInfo != 0)
                            mPlayerName.Construct(lpPlayerInfo->mPlayerName.macName);
                        else
                            mPlayerName.macName[0] = '\0';   // X360 `stb r30, 0x60C8`
                    }
                    else if (mbSelectRivals)
                    {
                        mbLocalPlayerSelected = false;

                        const GuiEventUpdateSatNav::SatNavIconInfo* lpRivalIcon =
                            mpIconManager->GetRivalIconAtIndex(liSnappedIndex);

                        mLockedIconInfo.SetPositionLane(lpRivalIcon->GetPositionLane());

                        // A networked rival (an online match is being started) gets the
                        // network-rival icon and a real gamertag; anyone else is a plain
                        // offline rival with no name.
                        if (mpGuiCache->IsOnlineStartInProgress())
                        {
                            mLockedIconInfo.SetIconType(
                                GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_NETWORKRIVAL);

                            CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:1252

                            const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData*
                                lpPlayerInfo = FindOnlinePlayerByActiveRaceCarIndex(
                                    mpGuiCache, lpRivalIcon->GetActiveRaceCarIndex());

                            if (lpPlayerInfo != 0)
                            {
                                // MEASURED: the X360 calls the collation-table compare and
                                // DISCARDS its answer -- there is no branch on r3 between
                                // this call and the Construct below (0x824CBE18..0x824CBE24).
                                // Kept because the binary makes the call.
                                LobbyNameCmp(mPlayerName.macName,
                                             lpPlayerInfo->mPlayerName.macName);
                                mPlayerName.Construct(lpPlayerInfo->mPlayerName.macName);
                            }
                            else
                            {
                                mPlayerName.macName[0] = '\0';
                            }
                        }
                        else
                        {
                            mLockedIconInfo.SetIconType(
                                GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_RIVAL);
                            mPlayerName.macName[0] = '\0';
                        }

                        mHoveringRivalId = lpRivalIcon->GetCgsId();
                    }
                    else if (mbUseRoadSigns)
                    {
                        // The road-sign arm owns mpLockedIconName, so it opts out of the
                        // end-of-block clear (X360 `mr r25, r30` -- unconditionally, before
                        // the lookup).
                        lbClearLockedIconName = false;

                        const char* lpcRoadSignName =
                            mpIconManager->GetRoadSignNameAtIndex(liSnappedIndex);

                        // Pointer identity, not strcmp: the manager hands back the same
                        // interned string for the same sign (X360 `cmplw`).
                        if (mpLockedIconName != lpcRoadSignName)
                        {
                            mpLockedIconName      = lpcRoadSignName;
                            muHoveredEventID      = 0;
                            mHoveredDriveThruID   = 0;
                            mHoveringRivalId      = 0;
                            mbLocalPlayerSelected = false;

                            // Ask the game side for this road's rule scores. The road id is
                            // the sign's name parsed as a decimal and SIGN-extended to 64
                            // bits (X360 `extsw`).
                            GuiEventRoadRuleDataRequest lRequest;
                            const s64 li64RoadId =
                                static_cast<s64>(std::atoi(lpcRoadSignName));
                            std::memcpy(lRequest.maData, &li64RoadId, sizeof(li64RoadId));

                            CgsGui::GuiEventWrapper<GuiEventRoadRuleDataRequest,
                                                    KI_CHANNEL_GUI_EVENT> lRecord(lRequest);
                            mpStateInterface->GetOutputEventQueue()->AddEvent(
                                reinterpret_cast<const CgsModule::Event*>(&lRecord),
                                lRecord.GetChannel(),
                                static_cast<s32>(sizeof(lRecord)));

                            // A road sign has no world record of its own, so the "locked"
                            // position is wherever the cursor is sitting.
                            mLockedIconInfo.SetPositionLane(
                                LaneAsVector4(MapTransform::DeviceToWorld(mCursor.GetPosition())));
                            mLockedIconInfo.SetIconType(
                                GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_ROADSIGN);
                        }
                    }
                    else if (meEventIconDisplayType ==
                             GuiEventEnableSatNavIcons::E_ICON_DISPLAY_TYPE_COUNT)
                    {
                        // No event icons are being drawn at all, so every index in the set
                        // is a drive-through / junkyard -- no count check needed.
                        if (mbSelectDriveThrus)
                        {
                            const GuiEventUpdateSatNav::SatNavIconInfo* lpIcon =
                                mpIconManager->GetDriveThroughOrJunkyardAtIndex(liSnappedIndex);

                            muHoveredEventID      = 0;
                            mHoveringRivalId      = 0;
                            mbLocalPlayerSelected = false;
                            mHoveredDriveThruID   = lpIcon->GetCgsId();

                            mLockedIconInfo.SetPositionLane(lpIcon->GetPositionLane());
                            // @0x824CBEFC: `bl SatNavIconInfo::GetIconType` on the icon the
                            // manager just handed back (IDA truncates the symbol to
                            // "SatNavI"); the byte lands in mLockedIconInfo's own +0x28.
                            mLockedIconInfo.SetIconType(lpIcon->GetIconType());
                        }
                        else
                        {
                            PlaceCursorOnPlayer();
                        }
                    }
                    else if (mbSelectDriveThrus &&
                             liSnappedIndex < mpIconManager->GetDriveThroughAndJunkyardCount())
                    {
                        // Drive-throughs occupy the front of the index space. (Byte-identical
                        // instruction sequence to the arm above -- the X360 emitted it twice,
                        // at 0x824CBED4 and 0x824CBF98; kept written out twice so no
                        // unattested helper has to be added to the class.)
                        const GuiEventUpdateSatNav::SatNavIconInfo* lpIcon =
                            mpIconManager->GetDriveThroughOrJunkyardAtIndex(liSnappedIndex);

                        muHoveredEventID      = 0;
                        mHoveringRivalId      = 0;
                        mbLocalPlayerSelected = false;
                        mHoveredDriveThruID   = lpIcon->GetCgsId();

                        mLockedIconInfo.SetPositionLane(lpIcon->GetPositionLane());
                        // @0x824CBFC0 -- the second call site of the same accessor.
                        mLockedIconInfo.SetIconType(lpIcon->GetIconType());
                    }
                    else
                    {
                        const u32 luEventId =
                            mpIconManager->GetEventIDAtIndex(liSnappedIndex);

                        if (luEventId != muHoveredEventID)
                        {
                            // Newly hovered event: ask the game side for its challenge
                            // scores. The id is ZERO-extended to 64 bits (X360 `clrldi`).
                            GuiEventChallengedEventDataRequest lRequest;
                            const u64 lu64EventId = luEventId;
                            std::memcpy(lRequest.maData, &lu64EventId, sizeof(lu64EventId));

                            CGS_ASSERT(mpStateInterface != 0, "mpStateInterface");  // cpp:1321

                            CgsGui::GuiEventWrapper<GuiEventChallengedEventDataRequest,
                                                    KI_CHANNEL_GUI_EVENT> lRecord(lRequest);
                            mpStateInterface->GetOutputEventQueue()->AddEvent(
                                reinterpret_cast<const CgsModule::Event*>(&lRecord),
                                lRecord.GetChannel(),
                                static_cast<s32>(sizeof(lRecord)));
                        }

                        muHoveredEventID      = luEventId;
                        mHoveredDriveThruID   = 0;
                        mHoveringRivalId      = 0;
                        mbLocalPlayerSelected = false;

                        // The display record's leading 16-byte lane is the junction's
                        // world position (X360 `lvx128 v0, r0, r3` on the returned
                        // pointer, @0x824CBF7C). The committed record names it mv3Position.
                        mLockedIconInfo.SetPositionLane(LaneAsVector4(
                            mpGuiCache->GetProfileEventDisplayInfo(luEventId)->mv3Position));
                        mLockedIconInfo.SetIconType(
                            GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_JUNCTION);
                    }

                    if (lbClearLockedIconName)
                        mpLockedIconName = 0;
                }
            }
            break;

        case E_CURSORMODE_INSPECTING_ICONS:
            // The inspect camera owns the map; nothing to re-snap, just re-publish.
            break;

        case E_CURSORMODE_PANNING:
            mCrashNavPanel.ShowBlank();
            break;

        default:
            {
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessageBuffer,
                                             CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "Unhandled cursor mode "
                           << static_cast<s32>(meCursorMode)
                           << " in CrashNavMap::UpdateIconManager()\n";
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), KAC_ASSERT_FILE, 1374);
                CgsDev::Assert::EndAssert();
            }
            break;
        }

        PostSetHoveredEventIcon(mpStateInterface, mHoveredDriveThruID, mHoveringRivalId,
                                muHoveredEventID, mpLockedIconName);

        if (mbItemsLoaded)
            UpdateButtonPrompts();
    }

// -----------------------------------------------------------------------------------
// NOTE: the drive-through latch appears TWICE above. That is deliberate: the X360 emitted
// two byte-identical copies (0x824CBED4 and 0x824CBF98), and folding them into a shared
// private helper would mean minting a member of CrashNavMap that neither the X360 ledger
// nor the DecFIGS DWARF attests. Written out twice, this body needs no addition to the
// owning header at all.
// -----------------------------------------------------------------------------------

}
