// ===================================================================================
// BrnGui::CarSelectMain -- out-of-line bodies for the shared car-select screen state.
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   Construct @0x824BBC20, GetResourcesToLoad @0x824B55B8, AppendAptComponents @0x824B5380,
//   HandleControllerInput @0x824B5410, UpdateGuiCache @0x824B54A8, SetupCar @0x824B5548,
//   SetupCarNameComponent @0x824C0EB0, TriggerSetupCar @0x824C8E08.
//
// The remaining 8 functions (OnEnter @0x824C8920, OnLeave @0x824C8B78, Update @0x824DC9C0,
// ProcesssIncomingEvents @0x824D73D8, ExitCarSelection @0x824C8CB8, HandleLaunchedEvent
// @0x824C8EF0, HandleLeftGameEvent @0x824C91C8, GetResourcesToLoadForCarSelect @0x824B56C0)
// live in the wave-G partfiles BrnCarSelectMain_wG_01..03.cpp alongside this file. Their
// former blockers were resolved in wave G: the maiEventToObserve[21] / overlay-string rodata
// was dumped from the image (values in _wG_01), the vtable slot map was recovered from the
// dispatch displacements (see the header banner), and the far GuiCache members were carved
// as named members (BrnGuiCache.h +0x4B40/+0x4B4E/+0x4B70).
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnCarSelectMain.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"              // CgsID / CgsIDConvertToString
#include "GameShared/GameClasses/Core/CgsStringUtils.h"     // CgsCore::SPrintf
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"        // CgsLanguage::LanguageManager
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"         // CgsGui::GuiEvent<N> / GuiEventQueueLarge
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // StateInterface out-queue
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"             // BrnGui::GuiFlow (E_GUIFLOW_SCREEN)
#include "GameSource/Gui/BrnGuiCache.h"                     // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiWorldDataController.h"       // BrnGui::WorldDataController
#include "SharedClasses/DataLists/VehicleList.h"            // BrnResource::VehicleList
#include "SharedClasses/DataLists/VehicleListEntry.h"       // BrnResource::VehicleListEntry

// includes folded in from the BrnCarSelectMain_w*.cpp partfiles (2026-09-15)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"               // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                     // CgsGui::GuiAccessPointers
#include "GameSource/GameState/BrnGameStateSharedIO.h"                   // GsmIO::ECarSelectType
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStreamBase (debug print)

namespace BrnGui
{
    // The car-select trigger event posted to the state interface out-queue (X360 buffer
    // {8, 415, 16} + the selected car id, published on channel 40 as 24 bytes). Modelled as a
    // GuiEvent<415> carrying the id so the AddEvent header/id/size match the asm exactly.
    struct GuiEventTriggerCarSelect : public CgsGui::GuiEvent<415>
    {
        CgsID mCarId;
        explicit GuiEventTriggerCarSelect(CgsID lCarId)
            : CgsGui::GuiEvent<415>(8, 16)
            , mCarId(lCarId)
        {
        }
    };

    // ---- Construct @ 0x824BBC20 ---------------------------------------------------
    void CarSelectMain::Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm)
    {
        CGS_ASSERT(lpFsm != 0, "Invalid ScriptedFsm ptr");   // cpp:133

        CgsGui::State::Construct(liId, lpFsm);

        mpGuiCache            = 0;                 // +0x288
        mbCarChangeInProgress = false;            // +0x7C4
        meCurrentState        = E_CARSELECT_INVALID;   // +0x7CC = -1

        mGameDataEventReceiverQueue.Construct();  // +0x7D4 (buffer=+0x18, capacity 256, align 16, Clear)

        mCurrentSetupInfo.mCarId      = static_cast<CgsID>(-1);
        mCurrentSetupInfo.mbSelectable = true;
        mDesiredSetupInfo.mCarId      = static_cast<CgsID>(-1);
        mDesiredSetupInfo.mbSelectable = true;
    }

    // ---- GetResourcesToLoad @ 0x824B55B8 ------------------------------------------
    void CarSelectMain::GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                           u32* lpuNumberOfResources) const
    {
        CGS_ASSERT(lppResourceTuples != 0, "Invalid pointer");    // cpp:935
        CGS_ASSERT(lpuNumberOfResources != 0, "Invalid pointer"); // cpp:936

        *lppResourceTuples    = 0;
        *lpuNumberOfResources = 0;
    }

    // ---- AppendAptComponents @ 0x824B5380 -----------------------------------------
    void CarSelectMain::AppendAptComponents()
    {
        CGS_ASSERT(mpGuiCache, "lpGuiCache");   // cpp:502

        // The three always-present help/logo apt clips (X360 sub_824F87C0 == the name-taking
        // GuiCache::AppendExpectedAptComponent entry, flow 0).
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, "ManufacturerLogo_mc");
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, "HelpItemContinue_mc");
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, "HelpItemBack_mc");
    }

    // ---- HandleControllerInput @ 0x824B5410 ---------------------------------------
    void CarSelectMain::HandleControllerInput(const CgsModule::Event* lpEvent, s32 /*liController*/)
    {
        CGS_ASSERT(lpEvent != 0, "Invalid event in CarSelectMain::HandleControllerInput");  // cpp:535
    }

    // ---- UpdateGuiCache @ 0x824B54A8 ----------------------------------------------
    // The trailing event-id s32 is the DWARF declaration shape (cpp:544); the dispatcher
    // (ProcesssIncomingEvents, event 64) passes it like the other handlers and this body
    // ignores it.
    void CarSelectMain::UpdateGuiCache(const CgsModule::Event* lpCacheEvent, s32 /*liEventType*/)
    {
        // The cache-ready GUI event carries the acquired GuiCache pointer as its leading word.
        // External event blob: its layout is fixed by the event, not a reconstructable C++ class,
        // so the leading pointer is read positionally (X360 lwz r11,0(a2)).
        GuiCache* lpCache = *reinterpret_cast<GuiCache* const*>(lpCacheEvent);
        CGS_ASSERT(lpCache != 0, "lpCacheEvent->mpCachePointer");   // cpp:553

        mpGuiCache = lpCache;

        // (PC-BUILD GUARD RETIRED 2026-08-02: GuiModule::Construct now binds the module's own
        // WorldDataController into the cache and its acquire machine binds the vehicle list, so
        // the console's asserting accessor is restored.)
        mpVehicleList = mpGuiCache->GetWorldDataController()->GetVehicleList();
    }

    // ---- SetupCar @ 0x824B5548 ----------------------------------------------------
    // POINTER param per the DWARF declaration (cpp:577) -- CarSelectVehicle overloads it
    // with (const CarSetupInfo*, bool) as a distinct new virtual, pinning the base shape.
    void CarSelectMain::SetupCar(const CarSetupInfo* lpSetupInfo)
    {
        mDesiredSetupInfo = *lpSetupInfo;  // copies mCarId + mbSelectable (X360 two-qword store)

        CGS_ASSERT(mDesiredSetupInfo.mbSelectable, "mDesiredSetupInfo.mbSelectable");   // cpp:600

        mbCarChangeInProgress = true;
    }

    // ---- SetupCarNameComponent @ 0x824C0EB0 ---------------------------------------
    void CarSelectMain::SetupCarNameComponent(CgsID lSelectedCarId)
    {
        typedef CgsLanguage::LanguageManager LM;

        // cpp:880 -- the console's assert, RESTORED 2026-08-02: GuiCache::mpWorldDataController
        // is populated now, so mpVehicleList is the real list (see UpdateGuiCache above).
        CGS_ASSERT(mpVehicleList != 0, "mpVehicleList");   // cpp:880

        const BrnResource::VehicleList* lpVehicleList = mpVehicleList;
        const s32 liVehicleIndex = lpVehicleList->GetVehicleIndex(lSelectedCarId);

        const BrnResource::VehicleListEntry* lpVehicleData =
            (liVehicleIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liVehicleIndex);
        // cpp:883 -- ⚠️ ENTRY guard (shape (b) in BrnCarSelectVehicle.h): the console asserts
        // and then dereferences. The id reaching here is published by a stand-in
        // (BrnGameModule::PublishCarSelectionToGui) and CarSelectMain::Construct seeds it to
        // (CgsID)-1, so an unresolved car id is still reachable on this build.
        if (lpVehicleData == 0)
            return;

        char lacId[16];
        char lacCarText[32];
        char lacManText[80];

        // Car name: a livery variant (finish type 2) or a parentless car uses its own id,
        // otherwise the parent car's id.
        const CgsID lParentId = lpVehicleData->GetParentId();
        const CgsID lCarNameId =
            (lpVehicleData->GetLiveryType() == 2 || lParentId == 0) ? lSelectedCarId : lParentId;

        CgsIDConvertToString(lCarNameId, lacId);
        CgsCore::SPrintf(lacCarText, 31, "CAR_CAPS_%s", lacId);
        lacCarText[31] = 0;
        mCarName.SetLocalisedText(lacCarText, LM::E_FORMAT_ID_LOOKUP);

        // Manufacturer name: always keyed off the parent car's id when the car has one.
        const CgsID lManufacturerId = (lParentId != 0) ? lParentId : lSelectedCarId;

        CgsIDConvertToString(lManufacturerId, lacId);
        CgsCore::SPrintf(lacManText, 31, "CAR_MAN_CAPS_%s", lacId);
        lacManText[31] = 0;
        mManufacturerName.SetLocalisedText(lacManText, LM::E_FORMAT_ID_LOOKUP);
    }

    // ---- TriggerSetupCar @ 0x824C8E08 ---------------------------------------------
    void CarSelectMain::TriggerSetupCar()
    {
        mCurrentSetupInfo = mDesiredSetupInfo;   // commit the pending selection

        if (mCurrentSetupInfo.mbSelectable)
        {
            CGS_ASSERT(mCurrentSetupInfo.mCarId != 0, "Trying to select a car will null id");   // cpp:655

            GuiEventTriggerCarSelect lEvent(mCurrentSetupInfo.mCarId);
            mpStateInterface->GetOutputEventQueue()->AddEvent(&lEvent, 40, 24);
        }
    }

    // ================================================================================
    // The ten base virtuals with no out-of-line X360 symbol (2026-08-02).
    //
    // Until this wave nothing in the tree instantiated a CarSelectMain-derived state, so
    // the class's vtable was never emitted and these ten never had to link. Re-homing
    // BrnGui::CarSelectVehicle onto this class makes the base sub-object real, and MSVC
    // emits CarSelectMain's own vtable alongside it -- so every declared virtual now needs
    // a definition. None of them is dispatched on a live CarSelectVehicle (it overrides the
    // first six and the last four are empty on the console), and CarSelectMain itself is
    // never instantiated: BrnScreenFlow::Prepare only ever news the derived states.
    //
    //  * The FOUR event handlers below are EMPTY ON THE CONSOLE, proven not assumed: the
    //    derived vtable off_82075470 holds 0x8284CB38 in slots +0x48/+0x4C/+0x50/+0x54, and
    //    0x8284CB38 is a bare `blr` with 193 xrefs -- the image-wide ICF fold of an empty
    //    body, NOT _purecall. Making them `= 0` would route events 564/406/413/414 into
    //    _purecall on this exact screen.
    //  * GetResourcesToLoad() / GetNumberResourcesToLoad() return "no resources", which is
    //    exactly what the base's ATTESTED 2-argument sibling
    //    CarSelectMain::GetResourcesToLoad(ptr, count) @0x824B55B8 writes ({0, 0}).
    //  * IsLoading / PlayMovie / SetupComponents / HandleCarInfoResponseEvent are the
    //    neutral base behaviours (nothing pending, no movie, nothing to build, nothing to
    //    adopt). FLAG: the X360 emitted no body for these four -- the DWARF places them at
    //    cpp:954 / h:136 / cpp:511 / cpp:563 in the PS3 build, where the derived states also
    //    own the real work. Replace them if a CarSelectMain-derived state ever needs a
    //    non-trivial base default.
    // ================================================================================

    bool CarSelectMain::IsLoading() const
    {
        return false;
    }

    void CarSelectMain::PlayMovie()
    {
    }

    void CarSelectMain::SetupComponents()
    {
    }

    void CarSelectMain::HandleCarInfoResponseEvent(const CgsModule::Event* /*lpEvent*/,
                                                   s32 /*liEventType*/)
    {
    }

    // The four ICF-folded empty overrides (derived vtable slots +0x48..+0x54).
    void CarSelectMain::HandleCarAudioLoadComplete()
    {
    }

    void CarSelectMain::HandlePlayerInfoResponse(const CgsModule::Event* /*lpEvent*/,
                                                  s32 /*liEventType*/)
    {
    }

    void CarSelectMain::HandleUnlockedLiveryResponseEvent(const CgsModule::Event* /*lpEvent*/,
                                                           s32 /*liEventType*/)
    {
    }

    void CarSelectMain::HandlePlayerCarColourResponseEvent(const CgsModule::Event* /*lpEvent*/,
                                                            s32 /*liEventType*/)
    {
    }

    // The two 0-argument resource getters GetResourcesToLoadForCarSelect forwards to
    // (X360 slots +0x24 / +0x60).
    CgsGui::sResourceTuple* CarSelectMain::GetResourcesToLoad() const
    {
        return 0;
    }

    u32 CarSelectMain::GetNumberResourcesToLoad() const
    {
        return 0;
    }
}

// ============================================================================
// FOLDED FROM BrnCarSelectMain_wG_01.cpp (wave G) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===================================================================================
// BrnGui::CarSelectMain -- wave-G partfile 01: the enter/leave/accept lifecycle plus the
// class's four static tables.
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   OnEnter          @ 0x824C8920
//   OnLeave          @ 0x824C8B78
//   ExitCarSelection @ 0x824C8CB8
//
// The statics were read out of the decrypted image (headless IDA dump of the .data
// tables the three referencing bodies address):
//   maiEventToObserve[21]        @ 0x82065E80   (21 event ids, table order)
//   miNumEventsObserved          @ 0x82065ED4   (== 21, immediately after the table)
//   KPC_LAUNCH_FAILED_STRINGIDS  @ 0x82F26C94   (7 char* -- consumed by HandleLaunchedEvent)
//   KPAC_MODE_STRINGS            @ 0x82F26CB0   (17 char*, EGameModeType-indexed --
//                                                consumed by HandleLaunchingEvent)
//
// The sibling bodies live in BrnCarSelectMain.cpp and the other wave-G partfiles.
// ===================================================================================


namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) --------------------------
        const s32 KI_CHANNEL_GUI_OUT      = 40;   // GuiEventOut
        const s32 KI_CHANNEL_GUI_INTERNAL = 42;   // internal/HUD-component channel

        // ---- out-queue wire records (the GuiEventWrapper shape: { payload size, event
        //      type, payload offset } then the payload; same idiom as BrnInGame.cpp's
        //      anon namespace) --------------------------------------------------------

        // 16-byte command record { 1, N, 12, flag }. The payload is a single byte, so the
        // X360 size word is 1 and the record pads out to 16.
        template <s32 N>
        struct GuiCommandEvent16 : public CgsGui::GuiEvent<N>
        {
            u8 mu8Flag;    // +0x0C
            u8 maPad[3];   // +0x0D (the X360 never writes these; modelled zeroed)

            explicit GuiCommandEvent16(u8 lu8Flag = 0)
                : CgsGui::GuiEvent<N>(1, 12), mu8Flag(lu8Flag)
            {
                maPad[0] = maPad[1] = maPad[2] = 0;
            }
        };

        template <s32 N>
        void PostCommand16(CgsGui::StateInterface* lpStateInterface, s32 liChannel,
                           u8 lu8Flag = 0)
        {
            GuiCommandEvent16<N> lEvent(lu8Flag);
            lpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), liChannel,
                static_cast<s32>(sizeof(lEvent)));   // X360 record size 16
        }

        // The junkyard "activate car select" record (@0x824C8D38): { 8, 192, 12, 4, 1 },
        // channel 40, 20 bytes. BrnGuiDemangledEventTypes.h attests the id-192 payload as
        // 8 bytes.
        //
        // ⭐ THE TWO PAYLOAD WORDS ARE (ACTION, TYPE) -- RECOVERED 2026-08-02 from the consumer,
        // GameStateModule::ProcessGameEvents @0x823A0A18 case 94:
        //     word1 (`_R25[1]`) is the car-select TYPE   -- 1 junkyard, 2 online event;
        //     word0 (`*_R25`)   is the car-select ACTION -- 0 StartCarSelectState,
        //                                                   1 EnterModification,
        //                                                   4 ExitJunkyard.
        // So this record's { 4, 1 } is "exit the junkyard, junkyard flow". Its two siblings are
        // CarSelectVehicle::Update @0x824DCBF0 ({ 0, type }) and CarSelectLivery::Update
        // @0x824DFCD0 ({ 1, type }) -- both of which build the pair with ONE big-endian `std`,
        // so the word that lands in word0 is the __int64's HIGH dword. ⚠️ Modelling either as a
        // single u64 member SWAPS THE WORDS on a little-endian host (that bug was live in
        // CarSelectVehicle until this wave).
        struct GuiEventActivateCarSelect20 : public CgsGui::GuiEvent<192>
        {
            u32 muWord0;   // +0x0C -- the ACTION  (X360 stores 4 == ExitJunkyard)
            u32 muWord1;   // +0x10 -- the TYPE    (X360 stores 1 == E_CAR_SELECT_TYPE_JUNKYARD)

            GuiEventActivateCarSelect20(u32 luWord0, u32 luWord1)
                : CgsGui::GuiEvent<192>(8, 12), muWord0(luWord0), muWord1(luWord1) {}
        };

        // The always-posted accept record (@0x824C8D78): { 2, 536, 12, u8 1, u8 0 },
        // channel 40, 16 bytes. The X360 builds the two payload bytes separately and
        // pushes them out as one halfword -- modelled as the two bytes they are, NOT as a
        // 16-bit word (that value only reads back as 256 because the console is
        // big-endian). The trailing two bytes are never written by the X360.
        struct GuiEventCarSelectAccept16 : public CgsGui::GuiEvent<536>
        {
            u8 mu8Byte0;   // +0x0C (X360 stores 1)
            u8 mu8Byte1;   // +0x0D (X360 stores 0)
            u8 maPad[2];   // +0x0E (untouched by the X360; modelled zeroed)

            GuiEventCarSelectAccept16(u8 lu8Byte0, u8 lu8Byte1)
                : CgsGui::GuiEvent<536>(2, 12), mu8Byte0(lu8Byte0), mu8Byte1(lu8Byte1)
            {
                maPad[0] = maPad[1] = 0;
            }
        };
    }

    // ================================================================================
    // statics -- DWARF h:254/h:255 + cpp:79/cpp:90; values read from the X360 image.
    // ================================================================================

    // @0x82065E80 -- the events OnEnter registers for / OnLeave unregisters, in table
    // order: controller (6/7/8/5), online launching/launched (57/58), disconnect (44),
    // gui cache (64), car info (412), player info (406), car audio load (564/565/566),
    // unlocked livery (413), 81, overlay complete (189), left game (273), 83, 84, 271,
    // enter game (93).
    const s32 CarSelectMain::maiEventToObserve[21] =
    {
          6,   7,   8,   5,  57,  58,  44,  64, 412, 406, 564,
        565, 566, 413,  81, 189, 273,  83,  84, 271,  93,
    };

    // @0x82065ED4 -- sits immediately after the table.
    const s32 CarSelectMain::miNumEventsObserved = 21;

    // @0x82F26C94 -- launched-result -> overlay message string id. Entry 0 is the pooled
    // empty string (@0x820046A7); results 1..6 are the failure reasons HandleLaunchedEvent
    // feeds to the "CNOnlLchFail" overlay as message param 2.
    const char* const CarSelectMain::KPC_LAUNCH_FAILED_STRINGIDS[7] =
    {
        "",
        "ONLINE_LAUNCHED_FAILED_PLAYERS",
        "ONLINE_LAUNCHED_FAILED_CONNECTING",
        "ONLINE_LAUNCHED_FAILED_ROUTES",
        "ONLINE_LAUNCHED_FAILED_TEAMS",
        "ONLINE_LAUNCHED_FAILED_PLAYING",
        "ONLINE_LAUNCHED_FAILED_GENERAL",
    };

    // @0x82F26CB0 -- EGameModeType-indexed mode string ids (17 entries, matching
    // GsmIO::E_MODE_COUNT). Only the online modes 10..14 are populated in the image; the
    // offline slots 0..9 and the online free-burn-lobby / showtime slots 15..16 are null.
    // Consumed by HandleLaunchingEvent (out of this partfile's scope).
    // The table is EIGHTEEN slots, not seventeen. Re-dumped big-endian from the image:
    // [17] @0x82F26CF4 = 0x8205CBEC -> "ONLINE_GAME_OPTION_MODE_STUNT_COOP" is a LIVE
    // entry, and the table ends at [18] @0x82F26CF8 = 0x00000095, which is not a pointer.
    // HandleLaunchingEvent @0x824C9008 indexes this by EGameModeType, so declaring it [17]
    // lost a real mode string with no diagnostic.
    const char* const CarSelectMain::KPAC_MODE_STRINGS[18] =
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                     // 0..9   offline modes
        "ONLINE_GAME_OPTION_MODE_RACE",                   // 10  E_MODE_ONLINE_RACE
        "ONLINE_GAME_OPTION_MODE_ROAD_RAGE",              // 11  E_MODE_ONLINE_ROAD_RAGE
        "ONLINE_GAME_OPTION_MODE_STUNT",                  // 12  E_MODE_ONLINE_FUGITIVE
        "ONLINE_GAME_OPTION_MODE_BURNING_HOME_RUN",       // 13  E_MODE_ONLINE_BURNING_HOME_RUN
        "ONLINE_GAME_OPTION_MODE_STUNT_FREE_FOR_ALL",     // 14  E_MODE_ONLINE_FREE_BURN
        0,                                                // 15  E_MODE_ONLINE_FREE_BURN_LOBBY
        0,                                                // 16  E_MODE_ONLINE_SHOWTIME
        "ONLINE_GAME_OPTION_MODE_STUNT_COOP",             // 17  (0x8205CBEC)
    };

    // ---- OnEnter @ 0x824C8920 ------------------------------------------------------
    // Register the observed events, shut the HUD components down (command 148 on the
    // internal channel), clear the pending selection, latch the GuiCache, post the
    // car-select-entered command (405), Construct the five apt components, and latch
    // which car-select flow is running.
    void CarSelectMain::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // { 1, 148, 12, 0 } on channel 42: take the HUD components down.
        PostCommand16<148>(mpStateInterface, KI_CHANNEL_GUI_INTERNAL, 0);

        miMostRecentDropInId = 0;

        // The X360 inlines GetAccessPointers() (assert "mpAccessPointers != NULL",
        // CgsGuiStateInterface.h:344) and GetGuiCache() (assert "mpGuiCache",
        // CgsGuiShared.h:201) here; the accessor calls reproduce that assert sequence.
        GuiCache* lpGuiCache = mpStateInterface->GetAccessPointers()->GetGuiCache();

        // Both setup descriptors are cleared to the null car id -- the X360 writes only
        // the leading id qword of each, leaving mbSelectable as Construct left it.
        mCurrentSetupInfo.mCarId = 0;
        mDesiredSetupInfo.mCarId = 0;

        meCurrentState = E_CARSELECT_UNLOADED;
        mpGuiCache     = lpGuiCache;

        // { 1, 405, 12 } on channel 40. The X360 reuses the previous record's payload
        // slot without rewriting the flag byte (it still holds the zero the 148 record
        // put there), so the posted flag is 0.
        PostCommand16<405>(mpStateInterface, KI_CHANNEL_GUI_OUT, 0);

        // The five apt components, in the X360's Construct order. All are parented to the
        // movie root (null parent name).
        mManufacturerLogo.Construct("ManufacturerLogo_mc", mpStateInterface, 0);
        mManufacturerName.Construct("CarMan_mc",           mpStateInterface, 0);
        mCarName.Construct         ("CarName_mc",          mpStateInterface, 0);
        mHelpItemContinue.Construct("HelpItemContinue_mc", mpStateInterface, 0);
        mHelpItemBack.Construct    ("HelpItemBack_mc",     mpStateInterface, 0);

        // A SECOND, freshly re-walked access-pointers -> gui-cache chain (the X360 repeats
        // both asserts rather than reusing mpGuiCache), then the car-select-type latch.
        meCarSelectType =
            mpStateInterface->GetAccessPointers()->GetGuiCache()->GetCurrentCarSelectType();
    }

    // ---- OnLeave @ 0x824C8B78 ------------------------------------------------------
    // Unregister, invalidate the load state, clear the level-3 apt movie, drop the
    // expected-component list and post the car-select-left command (533).
    void CarSelectMain::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        meCurrentState = E_CARSELECT_INVALID;

        // The X360 inlines StateInterface::PlayAptMovie (type 18, channel 41, 20 bytes).
        // Its movie name is the pooled empty string @0x820046A7 (verified in the image:
        // the terminating NUL of "%s%s%s"), i.e. "clear level 3".
        mpStateInterface->PlayAptMovie("", 3);

        // cpp:213 -- the streamed-message form of the assert in the X360 image.
        CGS_ASSERT(mpGuiCache != 0, "Invalid GuiCache pointer here");

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

        // { 1, 533, 12 } on channel 40. The X360 leaves the payload byte at whatever the
        // apt-movie record left in that stack slot; it is modelled zeroed.
        PostCommand16<533>(mpStateInterface, KI_CHANNEL_GUI_OUT, 0);
    }

    // ---- ExitCarSelection @ 0x824C8CB8 ---------------------------------------------
    // Accept the current selection: the junkyard flow additionally posts the
    // activate-car-select event, then everybody posts the accept record and sends the
    // "ACCEPT" state event.
    void CarSelectMain::ExitCarSelection()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:616

        // GetCurrentCarSelectType() carries the "meCarSelectType >
        // GsmIO::E_CAR_SELECT_TYPE_NONE" assert (BrnGuiCache.h:4378) the X360 inlines here.
        if (mpGuiCache->GetCurrentCarSelectType() ==
            BrnGameState::GameStateModuleIO::E_CAR_SELECT_TYPE_JUNKYARD)
        {
            GuiEventActivateCarSelect20 lActivateEvent(4, 1);
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lActivateEvent),
                KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lActivateEvent)));   // X360 record size 20
        }

        GuiEventCarSelectAccept16 lAcceptEvent(1, 0);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lAcceptEvent),
            KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lAcceptEvent)));   // X360 record size 16

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "RG :: CSM : SendStateEvent( \"ACCEPT\" ) 2\n";

        SendStateEvent("ACCEPT");
    }
}

// ============================================================================
// FOLDED FROM BrnCarSelectMain_wG_02.cpp (wave G) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===================================================================================
// BrnGui::CarSelectMain -- wave-G partfile 02: the two online-overlay handlers.
//   HandleLaunchedEvent  @0x824C8EF0  (in-queue event 58)
//   HandleLeftGameEvent  @0x824C91C8  (in-queue event 273)
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode + asm). Both
// bodies build the same out-queue overlay records the rest of the SCREEN flow posts:
// the 304-byte GuiOverlayRequest wire (id 184) and the 24-byte GuiOverlayWaitFinish
// wire (id 188), published on channel 40 through
// mpStateInterface->GetOutputEventQueue()->AddEvent(). The X360 reaches those records
// either by inlining CgsGui::StateInterface::OutputGuiEvent<T> (HandleLaunchedEvent's
// construct-then-memcpy at 0x824C8FC0) or by calling the out-of-line instantiation
// (HandleLeftGameEvent's kick overlay at 0x824C93F4); both produce the identical
// { sizeof(payload), <event id>, 16, <pad>, payload } record on channel 40, so the
// reconstruction posts the wire record directly at every site (the committed
// CgsGui::StateInterface::OutputGuiEvent template in CgsGuiStateInterface.h queues the
// bare payload under channel == GetEventType() instead, which is NOT what the X360
// instantiation does -- it is not called here, and its header is frozen this wave).
//
// The kick-reason overlay table @0x82F26774 was read out of the image (see
// KAPC_KICK_REASON_OVERLAYS below); KPC_LAUNCH_FAILED_STRINGIDS @0x82F26C94 is the
// class static declared in the header and defined in BrnCarSelectMain_wG_01.cpp.
// ===================================================================================


namespace BrnGui
{
    namespace
    {
// (fold: an identical definition of KI_CHANNEL_GUI_OUT was dropped here -- this TU defines it once, above)

        // ---- in-queue payload views (the queue delivers the header-stripped payload;
        //      same idiom as BrnInGame.cpp's) -------------------------------------------

        // Event 58 (online "launched") payload: the X360 reads one word at +0x00 and uses
        // it both as the failure test and as the KPC_LAUNCH_FAILED_STRINGIDS index.
        struct GuiEventLaunchedPayload : public CgsModule::Event
        {
            s32 miResult;        // +0x00  (0 == launched OK; 1..6 select the failure string)
        };

        // Event 273 ("left game") payload: the reason word at +0x00 and, for the kicked
        // reason, the kick sub-reason at +0x04 (compared SIGNED against 0 and 5 by the
        // X360 assert -- Hex-Rays renders it as an unsigned `> 4u`, the asm is blt/blt).
        struct GuiEventLeftGamePayload : public CgsModule::Event
        {
            s32 miReason;        // +0x00
            s32 miKickReason;    // +0x04  (index into KAPC_KICK_REASON_OVERLAYS)
        };

        // ---- out-queue wire records (same shape as BrnInGame.cpp's / BrnPauseScreen's) --

        // The OutputGuiEvent<BrnGui::GuiOverlayRequest> record: { 288, 184, 16, <pad>, the
        // 288-byte request }, channel 40, 304 bytes. The header words are host expressions
        // (sizeof / alignment), NOT the console's baked 0x120 / 0x130 immediates.
        struct GuiOverlayRequestWire : public CgsGui::GuiEvent<184>
        {
            u32               muPad0C;    // +0x0C (payload is 16-aligned past the header)
            GuiOverlayRequest mRequest;   // +0x10
            GuiOverlayRequestWire()
                : CgsGui::GuiEvent<184>(static_cast<u32>(sizeof(GuiOverlayRequest)), 16)
                , muPad0C(0) {}
        };

        // The OutputGuiEvent<BrnGui::GuiOverlayWaitFinishRequest> record:
        // { 8, 188, 16, <pad>, the compressed overlay id }, channel 40, 24 bytes.
        struct GuiOverlayWaitFinishWire : public CgsGui::GuiEvent<188>
        {
            u32                         muPad0C;    // +0x0C (8-aligned payload)
            GuiOverlayWaitFinishRequest mRequest;   // +0x10
            GuiOverlayWaitFinishWire()
                : CgsGui::GuiEvent<188>(static_cast<u32>(sizeof(GuiOverlayWaitFinishRequest)), 16)
                , muPad0C(0) {}
        };

        // ---- the left-game reason / kick-reason vocabularies ------------------------

        // *a2 (the reason word) values this handler acts on. FLAG: the producer-side enum
        // name is not in the recovered DWARF slice; the roles come from the overlay ids.
        const s32 KI_LEFT_GAME_REASON_KICKED       = 2;   // -> the leave questions + kick overlay
        const s32 KI_LEFT_GAME_REASON_LOBBY_DELETED = 3;  // -> "CNLobbyDlted"

        // Kick-reason -> overlay id. Read from the image: off_82F26774 is a POINTER table of
        // 5 entries (0x82F26774..0x82F26784, the next qwords are zero), chased to their
        // strings -- { "OnKicked", "OnKickedGame", "OnKicked", "OnKickedComm", "OnKickedLost" }
        // (entries 0 and 2 share the same "OnKicked" literal @0x8205CBE0). FLAG: the table's
        // ORIGINAL name is not in the recovered DWARF slice; the assert bound (5) is the
        // X360's own `kick >= 5` test.
        const s32 KI_KICK_REASON_COUNT = 5;
        const char* const KAPC_KICK_REASON_OVERLAYS[KI_KICK_REASON_COUNT] =
        {
            "OnKicked",       // 0  @0x8205CBE0
            "OnKickedGame",   // 1  @0x8205CBD0
            "OnKicked",       // 2  @0x8205CBE0 (same literal as 0)
            "OnKickedComm",   // 3  @0x8205CBC0
            "OnKickedLost",   // 4  @0x8205CBB0
        };
    }

    // ---- HandleLaunchedEvent @ 0x824C8EF0 -----------------------------------------
    // In-queue event 58. On a non-zero launch result, raise the "CNOnlLchFail" overlay
    // carrying the result's ONLINE_LAUNCHED_FAILED_* string id as message param 2.
    void CarSelectMain::HandleLaunchedEvent(const CgsModule::Event* lpLaunchedEvent)
    {
        // cpp:686. The X360 streams this text into the assert buffer; the message is the
        // ORIGINAL copy-paste from OnlineGameRoomPlayerInfo (kept verbatim).
        CGS_ASSERT(lpLaunchedEvent != 0,
                   "Invalid event sent to OnlineGameRoomPlayerInfo::HandleLaunchedEvent");

        const GuiEventLaunchedPayload* lpPayload =
            reinterpret_cast<const GuiEventLaunchedPayload*>(lpLaunchedEvent);

        if (lpPayload->miResult != 0)
        {
            // The X360 builds the request in a scratch stack slot and memcpy's the 288
            // bytes into the record (the inlined OutputGuiEvent<GuiOverlayRequest> copy);
            // constructed in place here.
            GuiOverlayRequestWire lWire;
            lWire.mRequest.Construct("CNOnlLchFail");
            lWire.mRequest.AddMessageParam(2, KPC_LAUNCH_FAILED_STRINGIDS[lpPayload->miResult]);

            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lWire), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(GuiOverlayRequestWire)));
        }
    }

    // ---- HandleLeftGameEvent @ 0x824C91C8 -----------------------------------------
    // In-queue event 273. Raise the reason-specific overlay(s), then hand the flow the
    // "DISCONNECT" state event and drop the expected-apt-component list if the screen is
    // still loading.
    void CarSelectMain::HandleLeftGameEvent(const CgsModule::Event* lpLeftGameEvent)
    {
        // The X360 fires BOTH asserts back-to-back on the null path: the bare expression
        // text (cpp:755) then the streamed message (cpp:756, again the original
        // OnlineGameRoomPlayerInfo copy-paste).
        CGS_ASSERT(lpLeftGameEvent != 0, "lpLeftGameEvent");                            // cpp:755
        CGS_ASSERT(lpLeftGameEvent != 0,
                   "Invalid event sent to OnlineGameRoomPlayerInfo::HandleLeftGameEvent"); // cpp:756

        const GuiEventLeftGamePayload* lpPayload =
            reinterpret_cast<const GuiEventLeftGamePayload*>(lpLeftGameEvent);

        if (lpPayload->miReason == KI_LEFT_GAME_REASON_LOBBY_DELETED)
        {
            GuiOverlayRequestWire lWire;
            lWire.mRequest.Construct("CNLobbyDlted");

            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lWire), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(GuiOverlayRequestWire)));
        }
        else if (lpPayload->miReason == KI_LEFT_GAME_REASON_KICKED)
        {
            // Tear down the two outstanding "are you sure you want to leave" questions
            // before the kick overlay goes up.
            {
                GuiOverlayWaitFinishWire lWaitGame;
                lWaitGame.mRequest.Construct("CNOnlLvGmQn");
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lWaitGame), KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(GuiOverlayWaitFinishWire)));
            }
            {
                GuiOverlayWaitFinishWire lWaitChat;
                lWaitChat.mRequest.Construct("CNOnlLvChaQn");
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lWaitChat), KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(GuiOverlayWaitFinishWire)));
            }

            // cpp:780 -- SIGNED bound check (asm: blt on 0 then blt on 5). The X360 streams
            // "Invalid kick reason of " << miKickReason << "\n" into the assert buffer;
            // collapsed to the static text per project policy (the streamed value is the
            // kick reason below).
            CGS_ASSERT(lpPayload->miKickReason >= 0 && lpPayload->miKickReason < KI_KICK_REASON_COUNT,
                       "Invalid kick reason of ");

            GuiOverlayRequestWire lWire;
            lWire.mRequest.Construct(KAPC_KICK_REASON_OVERLAYS[lpPayload->miKickReason]);

            // The X360 calls the out-of-line OutputGuiEvent<GuiOverlayRequest>
            // instantiation here; it queues exactly this record on channel 40.
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lWire), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(GuiOverlayRequestWire)));
        }

        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "RG :: CSM : SendStateEvent( \"DISCONNECT\" ) 2\n";

        SendStateEvent("DISCONNECT");

        if (mpGuiCache != 0 &&
            (meCurrentState == E_CARSELECT_UNLOADED ||
             meCurrentState == E_CARSELECT_LOADING_COMPONENTS))
        {
            mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
        }
    }
}

// ============================================================================
// FOLDED FROM BrnCarSelectMain_wG_03.cpp (wave G) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===================================================================================
// BrnGui::CarSelectMain -- wave-G partfile 03: the per-frame pump.
//   b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnCarSelectMain_wG_03.cpp
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   GetResourcesToLoadForCarSelect @0x824B56C0
//   Update                         @0x824DC9C0
//   ProcesssIncomingEvents         @0x824D73D8   (the triple-s spelling is the original)
//
// Companion partfiles hold the rest of the class (BrnCarSelectMain.cpp and
// BrnCarSelectMain_wG_01/_02.cpp); the owning header is BrnCarSelectMain.h, whose banner
// carries the X360 vtable slot map every dispatch site below is derived from:
//   +0x24 GetResourcesToLoad() (0-arg)  +0x28 GetResourcesToLoadForCarSelect
//   +0x2C IsLoading  +0x30 PlayMovie  +0x34 AppendAptComponents  +0x38 SetupComponents
//   +0x3C HandleControllerInput  +0x40 UpdateGuiCache  +0x44 HandleCarInfoResponseEvent
//   +0x48 HandleCarAudioLoadComplete  +0x4C HandlePlayerInfoResponse
//   +0x50 HandleUnlockedLiveryResponseEvent  +0x54 HandlePlayerCarColourResponseEvent
//   +0x58 SetupCar  +0x5C ExitCarSelection  +0x60 GetNumberResourcesToLoad
// The x64 build lays out its own vtable; every call below is a plain by-name (virtual)
// call and the slot map only records WHICH member each X360 displacement resolved to.
// ===================================================================================


// NOTE on the event records: the canonical payload homes for events 93/406 are
// GameSource/Gui/BrnGuiDemangledEventTypes.h, but that header and
// GameSource/Gui/BrnGuiEventTypeDefs.h are MUTUALLY EXCLUSIVE by construction (both define
// GuiEventRunFsm / GuiAudioTriggerEvent / GuiOverlayWaitFinishRequest -- see the banner in
// the demangled header), and BrnGuiCache.h pulls BrnGuiEventTypeDefs.h in. So the two
// payload fields this TU reads are modelled here as local in-queue VIEWS, exactly as the
// sibling screen state does (BrnInGame.cpp "in-queue payload views").

namespace BrnGui
{
    namespace
    {
        // The state's inbound GUI queue. CgsGui::State holds it as an opaque
        // InputBuffer::GuiEventQueue*; the X360 drains it through
        // CgsModule::VariableEventQueue<18432,16>::GetFirstEvent / GetNextEvent / Clear
        // (the calls at 0x824D73F4 / 0x824D7D74 / 0x824D7D8C name the instantiation).
        // Same idiom as the sibling screen states (BrnInGame.cpp, BrnPauseScreen.cpp).
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // ---- observed event ids (the dispatch labels; the registered set is
        //      CarSelectMain::maiEventToObserve[21], defined in _wG_01) ------------------
        const s32 KI_EVENT_CONTROLLER_FIRST        = 5;    // 5..8: one id per controller port
        const s32 KI_EVENT_CONTROLLER_LAST         = 8;
        const s32 KI_EVENT_NETWORK_DISCONNECTED    = 44;
        const s32 KI_EVENT_ONLINE_LAUNCHING        = 57;
        const s32 KI_EVENT_ONLINE_LAUNCHED         = 58;
        const s32 KI_EVENT_GUI_CACHE               = 64;
        const s32 KI_EVENT_CAR_SELECT_START        = 81;   // GuiCarSelectStartEvent
        const s32 KI_EVENT_CAR_SELECT_EXIT         = 83;
        const s32 KI_EVENT_CAR_SELECT_ABORT        = 84;   // GuiCarSelectAbortEvent
        const s32 KI_EVENT_PREPARE_FOR_MODE_START  = 93;   // GuiEventPrepareForModeStart
        const s32 KI_EVENT_OVERLAY_COMPLETE        = 189;  // registered (maiEventToObserve[15]); handled as a no-op
        const s32 KI_EVENT_TO_CAR_SELECT           = 271;
        const s32 KI_EVENT_NETWORK_LEFT_GAME       = 273;  // GuiEventNetworkLeftGame
        const s32 KI_EVENT_PLAYER_INFO_RESPONSE    = 406;  // GuiPlayerInfoResponse
        const s32 KI_EVENT_CAR_SELECTION           = 412;  // GuiCarSelectionEvent
        const s32 KI_EVENT_CAR_UNLOCKED_LIVERY     = 413;  // GuiCarUnlockedLiveryEvent
        const s32 KI_EVENT_PLAYER_CAR_COLOUR       = 414;  // GuiPlayerCarColourResponse
        const s32 KI_EVENT_CAR_READY_TO_EXIT       = 564;  // GuiCarSelectReadyToExitEvent
        const s32 KI_EVENT_CAR_CHANGED_DROP_IN     = 565;  // GuiCarSelectionChangedDropIn
        const s32 KI_EVENT_CAR_CHANGED_ONLINE      = 566;  // GuiCarSelectionChangedOnline

        // The GuiCarSelectStartEvent payload word that means "go to online car select".
        const s32 KI_CAR_SELECT_START_ONLINE = 2;

        // CgsDev::Message log-category bit the three SendStateEvent traces are gated on.
        const u64 KU_MESSAGE_FILTER_DEFAULT = 1;

        // ---- in-queue payload views (see the include-set note above) -----------------
        // GuiPlayerInfoResponse (id 406, 64 bytes = the 12-byte CgsGui::GuiEvent<406>
        // header + a 52-byte payload). The only field this state reads is the responding
        // player's car id at +0x20 (X360 `ld r11, 0x20(r30)`); the rest stays opaque. The
        // 12-byte header + 20 opaque bytes put mCarId at +0x20 on x64 too (align 8, no
        // inserted padding).
        struct GuiPlayerInfoResponseView : public CgsGui::GuiEvent<406>
        {
            u8    maPayload0[20];   // +0x0C..+0x1F (opaque)
            CgsID mCarId;           // +0x20
            u8    maPayload1[24];   // +0x28..+0x3F (opaque)
        };

        // GuiEventPrepareForModeStart (id 93, 152 bytes = the 12-byte
        // CgsGui::GuiEvent<93> header + a 140-byte payload). The mode type is the first
        // payload word (X360 `lwz r11, 0xC(r30)`).
        struct GuiPrepareForModeStartView : public CgsGui::GuiEvent<93>
        {
            s32 meGameModeType;     // +0x0C (GsmIO::EGameModeType)
            u8  maPayload[136];     // +0x10..+0x97 (opaque)
        };
    }

    // ---- GetResourcesToLoadForCarSelect @ 0x824B56C0 -------------------------------
    // Pure forwarder onto the two 0-arg resource getters the derived car-select states
    // override (X360 vtable +0x24 / +0x60).
    void CarSelectMain::GetResourcesToLoadForCarSelect(const CgsGui::sResourceTuple** lppResourceTuples,
                                                       u32* lpuNumberOfResources) const
    {
        CGS_ASSERT(lppResourceTuples != 0, "Invalid pointer");    // cpp:955
        CGS_ASSERT(lpuNumberOfResources != 0, "Invalid pointer"); // cpp:956

        *lppResourceTuples    = GetResourcesToLoad();
        *lpuNumberOfResources = GetNumberResourcesToLoad();
    }

    // ---- Update @ 0x824DC9C0 -------------------------------------------------------
    // The load/interactive state machine. Every path -- including the two early bails and
    // the unhandled-state assert -- falls through to ProcesssIncomingEvents().
    void CarSelectMain::Update()
    {
        switch (meCurrentState)
        {
        case E_CARSELECT_UNLOADED:
        {
            // Nothing can be done until the cache-ready event (64) has landed; keep
            // pumping the queue until it does.
            if (!mpGuiCache)
                break;

            mpStateInterface->StopLoadingScreen();

            // The X360 pre-zeroes only the list pointer (stw of r10==0 into var_40); the
            // count slot is written by the callee.
            const CgsGui::sResourceTuple* lpResourceTuples = 0;
            u32 luNumberOfResources;
            GetResourcesToLoadForCarSelect(&lpResourceTuples, &luNumberOfResources);

            if (luNumberOfResources != 0)
            {
                CGS_ASSERT(lpResourceTuples != 0,
                           "resource count > 1, but resource list pointer is zero");   // cpp:246

                // Still streaming -- stay in this state and re-try next frame.
                if (!mpGuiCache->EnsureResourcesAreLoaded(lpResourceTuples, luNumberOfResources))
                    break;

                PlayMovie();
                mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
                AppendAptComponents();
            }

            meCurrentState = E_CARSELECT_LOADING_COMPONENTS;
            break;
        }

        case E_CARSELECT_LOADING_COMPONENTS:
            // "aquired" is the original spelling baked into the image.
            CGS_ASSERT(mpGuiCache != 0,
                       "Should have aquired GuiCache in the previous state");   // cpp:269

            if (mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
            {
                SetupComponents();
                meCurrentState = E_CARSELECT_VISIBLE_INTERACTIVE;
            }
            break;

        case E_CARSELECT_VISIBLE_INTERACTIVE:
            CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:280

            // Full 64-bit id compare (X360 ld/ld + cmpld): a pending selection is anything
            // whose desired car id differs from the committed one.
            if (mCurrentSetupInfo.mCarId != mDesiredSetupInfo.mCarId)
                TriggerSetupCar();
            break;

        default:
            // E_CARSELECT_INVALID (-1) and anything >= E_CARSELECT_COUNT land here.
            CGS_ASSERT(false, "Invalid, unhandled state");   // cpp:291
            break;
        }

        ProcesssIncomingEvents();
    }

    // ---- ProcesssIncomingEvents @ 0x824D73D8 ---------------------------------------
    // Drain the state's inbound GUI queue, dispatch the observed events, then clear it.
    // The loop aborts the moment a handler has invalidated the state (a state event that
    // takes the FSM elsewhere), leaving the remaining events for the Clear() below.
    void CarSelectMain::ProcesssIncomingEvents()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;

        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            if (meCurrentState == E_CARSELECT_INVALID)
                break;

            switch (liEventId)
            {
            case KI_EVENT_CONTROLLER_FIRST:
            case KI_EVENT_CONTROLLER_FIRST + 1:
            case KI_EVENT_CONTROLLER_FIRST + 2:
            case KI_EVENT_CONTROLLER_LAST:
                // Input is only accepted once the screen is up and interactive. The event
                // id doubles as the controller selector (the X360 leaves it live in r5).
                if (meCurrentState > E_CARSELECT_LOADING_COMPONENTS)
                    HandleControllerInput(lpEvent, liEventId);
                break;

            case KI_EVENT_NETWORK_DISCONNECTED:
                CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:431

                // Latch the error word the disconnect popup will show (0 when the event
                // carried no payload) -- GuiCache::SetDoDisconnectPopup, inlined here.
                mpGuiCache->SetDoDisconnectPopup(lpEvent);

                if ((CgsDev::Message::gxMessageFilterFlags & KU_MESSAGE_FILTER_DEFAULT) != 0)
                    *CgsDev::Log::gpDebugPrint << "RG :: CSM : SendStateEvent( \"DISCONNECT\" ) 1\n";
                SendStateEvent("DISCONNECT");
                break;

            case KI_EVENT_ONLINE_LAUNCHING:
                HandleLaunchingEvent(lpEvent);
                break;

            case KI_EVENT_ONLINE_LAUNCHED:
                HandleLaunchedEvent(lpEvent);
                break;

            case KI_EVENT_GUI_CACHE:
                UpdateGuiCache(lpEvent, liEventId);
                break;

            case KI_EVENT_CAR_SELECT_START:
                // GuiCarSelectStartEvent: the leading word selects the flow to enter.
                // External event blob -- its layout is fixed by the producer, so the
                // leading word is read positionally (X360 lwz r11, 0(r30)).
                if (*reinterpret_cast<const s32*>(lpEvent) == KI_CAR_SELECT_START_ONLINE)
                {
                    if ((CgsDev::Message::gxMessageFilterFlags & KU_MESSAGE_FILTER_DEFAULT) != 0)
                        *CgsDev::Log::gpDebugPrint << "RG :: CSM : SendStateEvent( \"TO_ON_CARSEL\" )\n";
                    SendStateEvent("TO_ON_CARSEL");
                }
                break;

            case KI_EVENT_CAR_SELECT_EXIT:
                ExitCarSelection();
                break;

            case KI_EVENT_CAR_SELECT_ABORT:
                // GuiCarSelectAbortEvent: a single leading byte (X360 lbz r11, 0(r30));
                // zero means "accept the current selection".
                if (*reinterpret_cast<const u8*>(lpEvent) == 0)
                {
                    if ((CgsDev::Message::gxMessageFilterFlags & KU_MESSAGE_FILTER_DEFAULT) != 0)
                        *CgsDev::Log::gpDebugPrint << "RG :: CSM : SendStateEvent( \"ACCEPT\" ) 1\n";
                    SendStateEvent("ACCEPT");
                }
                break;

            case KI_EVENT_PREPARE_FOR_MODE_START:
            {
                // GuiEventPrepareForModeStart: the mode type is the first payload word
                // after the 12-byte GuiEvent<93> header (X360 lwz r11, 0xC(r30)).
                const GuiPrepareForModeStartView* lpModeEvent =
                    reinterpret_cast<const GuiPrepareForModeStartView*>(lpEvent);
                const s32 liGameModeType = lpModeEvent->meGameModeType;

                // NOTE: no mpGuiCache null-check on this path in the X360 build.
                if ((liGameModeType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY ||
                     liGameModeType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME) &&
                    !mpGuiCache->IsStartingGameDueToPlayerJoin())
                {
                    SendStateEvent("ENTER_GAME");
                }
                break;
            }

            case KI_EVENT_OVERLAY_COMPLETE:
                // Registered but handled as a no-op. I re-dumped maiEventToObserve
                // @0x82065E80 big-endian: 21 words (the count at 0x82065ED4 is 21), and 189
                // IS index 15 of that set -- the sibling wG_01 table already lists it as
                // "overlay complete (189)". The X360 pivots on it at 0x824D7470
                // (cmpwi r5,0xBD / beq default), which is MSVC's three-way form for a real,
                // empty case. (414 is the value that is NOT registered.)
                break;

            case KI_EVENT_TO_CAR_SELECT:
                SendStateEvent("TO_CSELECT");
                break;

            case KI_EVENT_NETWORK_LEFT_GAME:
                HandleLeftGameEvent(lpEvent);
                break;

            case KI_EVENT_PLAYER_INFO_RESPONSE:
            {
                // GuiPlayerInfoResponse: adopt the car the responding player is on. The
                // committed id is copied straight into the desired slot (so no
                // TriggerSetupCar fires for it) and remembered as the drop-in car.
                const GuiPlayerInfoResponseView* lpResponse =
                    reinterpret_cast<const GuiPlayerInfoResponseView*>(lpEvent);

                mCurrentSetupInfo.mCarId = lpResponse->mCarId;
                mDesiredSetupInfo        = mCurrentSetupInfo;   // id + selectable (X360 two-qword copy)
                miMostRecentDropInId     = lpResponse->mCarId;

                HandlePlayerInfoResponse(lpEvent, liEventId);
                break;
            }

            case KI_EVENT_CAR_SELECTION:
                HandleCarInfoResponseEvent(lpEvent, liEventId);
                break;

            case KI_EVENT_CAR_UNLOCKED_LIVERY:
                HandleUnlockedLiveryResponseEvent(lpEvent, liEventId);
                break;

            case KI_EVENT_PLAYER_CAR_COLOUR:
                HandlePlayerCarColourResponseEvent(lpEvent, liEventId);
                break;

            case KI_EVENT_CAR_READY_TO_EXIT:
                HandleCarAudioLoadComplete();
                break;

            case KI_EVENT_CAR_CHANGED_DROP_IN:
            case KI_EVENT_CAR_CHANGED_ONLINE:
                // Both records lead with the newly selected car id (X360 ld r11, 0(r30));
                // the pending car change is satisfied by the notification itself.
                mbCarChangeInProgress = false;
                miMostRecentDropInId  = *reinterpret_cast<const CgsID*>(lpEvent);
                break;

            default:
                break;
            }
        }

        lpInQueue->Clear();
    }
}

namespace BrnGui
{
    // ---- HandleLaunchingEvent @ 0x824C9008 ----------------------------------------
    // In-queue event 57 (ProcesssIncomingEvents case 57, above). ONLINE-ONLY: it raises the
    // "CNOnlLchGmH" (this client is the lobby host) or "CNOnlLchGame" (a peer is launching,
    // + that player's name as message param 1) overlay, then adds the mode string
    // KPAC_MODE_STRINGS[event.mode] as message param 2 and queues the 288-byte
    // GuiOverlayRequest as event 184 on channel 40.
    //
    // ⭐ HOME-FILE CORRECTION. BrnCarSelectMain.h:145-149 said this body "is NOT part of this
    // TU's ledger scope (identity attributes it to another TU)". That attribution is wrong and
    // the function says so itself: its four asserts bake
    // "..\..\..\GameSource\Gui/Flow/Screen/States/BrnCarSelectMain.cpp" at lines 712/716/719/731,
    // and the DecFIGS DWARF places it at BrnCarSelectMain.cpp:690. This IS its home file, so the
    // body belongs in a CarSelectMain partfile -- next to its only caller.
    //
    // ⛔ NOT RECONSTRUCTED, and deliberately NOT a silent {}. The X360 body needs two things
    // this wave cannot attest: the unnamed GuiCache lookup sub_82482738(cache, event.miPlayerId)
    // and the +0x100 name field of whatever it returns (the committed GuiCache models
    // maLobbyPlayerInfo[8] at a 56-byte stride, which +0x100 does not fit, so the return type is
    // NOT LobbyPlayerStatusData and guessing it would be fabrication). Every reachable path here
    // is an online lobby launch; the offline junkyard car-select flow this TU was mounted for
    // never posts event 57. The assert is the tripwire: if this is ever reached, it says so
    // loudly instead of dropping the overlay on the floor.
    void CarSelectMain::HandleLaunchingEvent(const CgsModule::Event* lpLaunchingEvent)
    {
        // cpp:712 -- the X360's streamed text, verbatim (it is the ORIGINAL copy-paste from
        // OnlineGameRoomPlayerInfo, exactly like the sibling HandleLaunchedEvent/HandleLeftGameEvent).
        CGS_ASSERT(lpLaunchingEvent != 0,
                   "Invalid event sent to OnlineGameRoomPlayerInfo::HandleLaunchingEvent");
        CGS_ASSERT(lpLaunchingEvent != 0, "lpLaunchingEvent");   // cpp:716
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");               // cpp:719

        CGS_ASSERT(false,
                   "CarSelectMain::HandleLaunchingEvent (0x824C9008) is not reconstructed -- "
                   "the online launch overlay is missing. Recover sub_82482738's return type first.");
    }
}
