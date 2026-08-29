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
