// ===================================================================================
// BrnGui::RivalMapPanel  -- implementation
//   class:BrnGui::RivalMapPanel
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   Construct                   @0x8243A1C0   (DWARF cpp:48)
//   AppendExpectedAptComponents @0x82417738   (cpp:89)
//   SetPlayerData               @0x824177C8   (cpp:115)
//   SetRivalData(CgsID)         @0x82430430   (cpp:168)
//   SetRivalData(name,id,cache) @0x82430888   (cpp:240; ledger-unnamed sub_82430888)
//   TransitionIn                @0x82417900   (cpp:323)
//   TransitionOut               @0x824179B0   (cpp:355)
//   StorePlayerInfo             @0x824176C0
//   SetState                    -- the panel's face onto IconComponent::SetState(const char*)
//
// Member access is BY NAME (the gate compiles 64-bit); the guest offsets quoted in the
// comments are the proof, not the mechanism. See the header banner for the full member run.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/Components/BrnRivalMapPanel.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf, LobbyNameCmp
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsIDConvertToString
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"           // ParameterFormatType
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // VariableEventQueue::AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiWorldDataController.h"                     // BrnGui::WorldDataController
#include "SharedClasses/DataLists/VehicleList.h"                          // BrnResource::VehicleList
#include "SharedClasses/DataLists/VehicleListEntry.h"                     // BrnResource::VehicleListEntry

#include <cstring>   // memcpy, memset

namespace BrnGui
{
    typedef CgsLanguage::LanguageManager LM;

    // The four child text fields' apt clip names. IMAGE-CITED: image.bin (file offset ==
    // VA - 0x82000000, big-endian) -- the table Construct walks lives at VA 0x82F25170 and
    // holds four pointers, 0x820494B4/0x820494A8/0x8204949C/0x82049490, which resolve to the
    // strings below. Construct's loop bound is the NEXT table (off_82F25180, the event-filter
    // options), which is what fixes the count at four.
    static const char* const KAPC_TEXTFIELD_NAMES[RivalMapPanel::E_TEXTFIELD_COUNT] =
    {
        "RivalName",
        "RivalText1",
        "RivalText2",
        "RivalText3",
    };

    // Apt clip name + parked state of the embedded car-image icon (@0x8243A230 / @0x8243A250).
    static const char KAC_CAR_ICON_NAME[]  = "rivalCarIcon_cpt";
    static const char KAC_STATE_INVISIBLE[] = "invisible";

    // The named apt states this panel pushes (all through sub_824E2B90 ==
    // IconComponent::SetState(const char*)).
    static const char KAC_STATE_PLAYER[]           = "player";
    static const char KAC_STATE_RIVAL[]            = "rival";
    static const char KAC_STATE_TRANS_IN_PLAYER[]  = "transInPlayer";
    static const char KAC_STATE_TRANS_IN_RIVAL[]   = "transInRival";
    static const char KAC_STATE_TRANS_OUT_PLAYER[] = "transOutPlayer";
    static const char KAC_STATE_TRANS_OUT_RIVAL[]  = "transOutRival";

    // The build's shared empty-string literal (X360 &unk_820046A7).
    static const char KAC_EMPTY_STRING[] = "";

    // The scratch capacities the console hard-codes: SPrintf is always called with 32 and the
    // stack buffer's byte 31 is cleared right after (`stb 0, var_XX+31`).
    static const s32 KI_SCRATCH_LEN = 32;

    // Localisation keys the panel formats for the rival's car.
    static const char KAC_CAR_CAPS_KEY_FORMAT[] = "CAR_CAPS_%s";   // -> the car-name text field
    static const char KAC_CAR_STATE_FORMAT[]    = "CAR_%s";        // -> the car-image icon state

    // ---------------------------------------------------------------------------------
    // The {1, 435, 12} "setup done" command Construct posts on the state interface's output
    // queue as channel 40, record size 16 (`v9 = {1, 0x1B3, 12}; AddEvent(si + 12, v9, 40, 16)`
    // @0x8243A298..0x8243A2B4). Modelled exactly like the committed sibling in
    // BrnCrashNavStats.cpp, which builds the SAME record for the SAME queue.
    // FLAG: 435 is the X360 WIRE id. The DWARF `CgsGui::GuiEvent<N>` template id for this event
    // is not derivable from the call site and differs from the wire id elsewhere in the tree
    // (see the "GuiEvent<450>; X360 id 455" note in BrnGuiEventTypeDefs.h) -- but the wire id is
    // what this tree's own consumers compare against (CrashNavPanel::RecEvent tests 436/438),
    // so the wire id is the self-consistent choice here.
    // ---------------------------------------------------------------------------------
    struct GuiEventSetupDone : public CgsGui::GuiEvent<435>
    {
        GuiEventSetupDone() : CgsGui::GuiEvent<435>(1, 12) {}
    };
    static const s32 KI_OUT_CHANNEL_GUI_EVENT = 40;   // the AddEvent channel (OutputGuiEvent)
    static const s32 KI_SETUP_DONE_RECORD_SIZE = 16;  // `li r6, 0x10`

    // ---------------------------------------------------------------------------------
    // File-local reader over the OPAQUE cached player-stats response event. Same boundary
    // idiom (and the same event record) as the committed CrashNavStats::HandleStatData:
    // BrnGuiDemangledEventTypes.h models GuiEventStatsResponse as an opaque 432-byte payload,
    // so the three fields SetPlayerData shows are read by their asm-proven byte offsets.
    // ---------------------------------------------------------------------------------
    namespace RivalStats
    {
        static s32 Word(const u8* lpcBlob, u32 luOffset)
        {
            return *reinterpret_cast<const s32*>(lpcBlob + luOffset);
        }

        // event +0x1C / +0x20 / +0xC4 -- the three words SetPlayerData formats
        // (`lwz r6, 0x60C/0x610/0x6B4(panel)` against mPlayerStats @+0x5F0).
        static const u32 KU_OFFSET_DISTANCE   = 0x1C;
        static const u32 KU_OFFSET_TIME       = 0x20;
        static const u32 KU_OFFSET_PERCENTAGE = 0xC4;
    }

    // @ 0x8243A1C0 -------------------------------------------------------------------
    void RivalMapPanel::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                                  const char* lpacParentName)
    {
        // Base IconComponent construct: no state-identifier table for this panel
        // (`li r6, 0` @0x8243A1D0).
        IconComponent::Construct(lpacName, lpStateInterface, 0, lpacParentName);

        // The four text fields, each parented under this panel's own name. The console
        // dispatches each through the field's vtable slot 0 (`lwz r11,0(r29); bctrl`).
        for (s32 liField = 0; liField < E_TEXTFIELD_COUNT; ++liField)
        {
            maTextfields[liField].Construct(KAPC_TEXTFIELD_NAMES[liField],
                                            lpStateInterface, GetName());
        }

        // The car-image icon: no state table either, then parked invisible.
        mCarImageIcon.Construct(KAC_CAR_ICON_NAME, lpStateInterface, 0, GetName());
        mCarImageIcon.SetState(KAC_STATE_INVISIBLE);

        mPlayerName.macName[0] = 0;               // stb 0, 0x5C8
        meCurrentRivalType     = E_RIVAL_TYPE_COUNT;  // stw 4, 0x5D8
        mRivalID               = 0;               // std 0, 0x5E0
        mOnlineRivalID         = 0;               // std 0, 0x5E8
        std::memset(maStatsResponse, 0, KI_STATS_RESPONSE_SIZE);   // memset(+0x5F0, 0, 432)
        mbHasPlayerInfo        = false;           // stb 0, 0x7A0
        mbActive               = false;           // stb 0, 0x7A1

        // Ask the GUI back-end for the player-stats snapshot this panel draws from; the answer
        // arrives as the id-436 GuiEventStatsResponse CrashNavPanel::RecEvent forwards to
        // StorePlayerInfo.
        GuiEventSetupDone lSetupEvent;
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lSetupEvent),
            KI_OUT_CHANNEL_GUI_EVENT, KI_SETUP_DONE_RECORD_SIZE);
    }

    // @ 0x82417738 -------------------------------------------------------------------
    void RivalMapPanel::AppendExpectedAptComponents(GuiFlow leFlow, GuiCache* lpGuiCache)
    {
        CGS_ASSERT(lpGuiCache != 0, "lpGuiCache");   // cpp:91

        lpGuiCache->AppendExpectedAptComponent(leFlow, GetNameHash());            // lwz +0x84

        for (s32 liField = 0; liField < E_TEXTFIELD_COUNT; ++liField)             // li r31, 4
        {
            lpGuiCache->AppendExpectedAptComponent(leFlow, maTextfields[liField].GetNameHash());
        }

        lpGuiCache->AppendExpectedAptComponent(leFlow, mCarImageIcon.GetNameHash());  // lwz +0x5B8
    }

    // @ 0x824177C8 -------------------------------------------------------------------
    // Repaint the panel from the CACHED stats response. Nothing happens unless the panel is
    // already showing; the console pushes the "player" state first and only then asserts.
    void RivalMapPanel::SetPlayerData(GuiCache* lpGuiCache)
    {
        if (!mbActive)   // lbz 0x7A1
        {
            return;
        }

        SetState(KAC_STATE_PLAYER);

        CGS_ASSERT(meCurrentRivalType == E_RIVAL_TYPE_OFFLINE_PLAYER,
                   "E_RIVAL_TYPE_OFFLINE_PLAYER == meCurrentRivalType");   // cpp:127
        CGS_ASSERT(lpGuiCache != 0, "NULL != lpGuiCache");                 // cpp:128

        if (!mbHasPlayerInfo)   // lbz 0x7A0 -- no response cached yet, nothing to draw
        {
            return;
        }

        char lacScratch[KI_SCRATCH_LEN];

        // Each value is formatted as a plain "%d" and then handed to the field's LOCALISED
        // setter, which re-formats it under the given ParameterFormatType.
        CgsCore::SPrintf(lacScratch, KI_SCRATCH_LEN, "%d",
                         RivalStats::Word(maStatsResponse, RivalStats::KU_OFFSET_PERCENTAGE));
        lacScratch[KI_SCRATCH_LEN - 1] = 0;
        maTextfields[E_TEXTFIELD_TEXT1].SetLocalisedText(lacScratch, LM::E_FORMAT_PERCENTAGE);  // li r5, 13

        CgsCore::SPrintf(lacScratch, KI_SCRATCH_LEN, "%d",
                         RivalStats::Word(maStatsResponse, RivalStats::KU_OFFSET_DISTANCE));
        lacScratch[KI_SCRATCH_LEN - 1] = 0;
        maTextfields[E_TEXTFIELD_TEXT3].SetLocalisedText(lacScratch, LM::E_FORMAT_AUTO_DISTANCE_LONG); // li r5, 16

        CgsCore::SPrintf(lacScratch, KI_SCRATCH_LEN, "%d",
                         RivalStats::Word(maStatsResponse, RivalStats::KU_OFFSET_TIME));
        lacScratch[KI_SCRATCH_LEN - 1] = 0;
        maTextfields[E_TEXTFIELD_TEXT2].SetLocalisedText(lacScratch, LM::E_FORMAT_HOURS_MINUTES_SECONDS); // li r5, 1

        // The name row is blanked (`stb 0, 0xA4(this + 0x94)` -- macText[0]) and re-pushed, and
        // the car image is hidden: in offline-player mode there is no rival car to show.
        maTextfields[E_TEXTFIELD_RIVAL_NAME].ClearText();
        maTextfields[E_TEXTFIELD_RIVAL_NAME].OutputAptData();
        mCarImageIcon.SetState(KAC_STATE_INVISIBLE);
    }

    // ---------------------------------------------------------------------------------
    // Shared by both SetRivalData overloads: resolve a car id to the id whose CAR_* artwork
    // should actually be shown. A livery variant (livery type != 2) displays under its PARENT
    // car's id; everything else displays under its own.
    // X360 (@0x824307D0.. and @0x82430C78..):
    //     lbz r11, 0xE9(entry)     ; muLiveryType
    //     cmplwi r11, 2 ; beq      -> keep the requested id
    //     ld  r11, 8(entry)        ; mParentId
    //     cmpldi r11, 0 ; beq      -> keep the requested id
    //     mr  <id>, r11            ; otherwise display the parent
    // ---------------------------------------------------------------------------------
    static CgsID ResolveDisplayCarId(const BrnResource::VehicleListEntry* lpVehicleData,
                                     CgsID lRequestedCarId)
    {
        if (lpVehicleData == 0)
        {
            return lRequestedCarId;
        }
        if (lpVehicleData->GetLiveryType() == 2)
        {
            return lRequestedCarId;
        }
        const CgsID lParentId = lpVehicleData->GetParentId();
        return (lParentId != 0) ? lParentId : lRequestedCarId;
    }

    // @ 0x82430430 -------------------------------------------------------------------
    void RivalMapPanel::SetRivalData(CgsID lRivalId)
    {
        CGS_ASSERT(meCurrentRivalType == E_RIVAL_TYPE_OFFLINE_RIVAL,
                   "E_RIVAL_TYPE_OFFLINE_RIVAL == meCurrentRivalType");   // cpp:170

        if (mRivalID == lRivalId)   // ld 0x5E0 / cmpld -- unchanged, nothing to do
        {
            return;
        }

        const bool lbWasActive = mbActive;   // lbz 0x7A1, read BEFORE the store
        mRivalID = lRivalId;                 // std 0x5E0

        if (lbWasActive)
        {
            SetState(KAC_STATE_RIVAL);
        }

        CGS_ASSERT(mpStateInterface != 0, "mpStateInterface");                          // cpp:184
        CGS_ASSERT(mpStateInterface->GetAccessPointers() != 0,
                   "mpStateInterface->GetAccessPointers()");                            // cpp:185
        CGS_ASSERT(mpStateInterface->GetAccessPointers()->GetGuiCache() != 0,
                   "mpStateInterface->GetAccessPointers()->GetGuiCache()");             // cpp:186
        CGS_ASSERT(mpStateInterface->GetAccessPointers()->GetGuiCache()->GetWorldDataController() != 0,
                   "mpStateInterface->GetAccessPointers()->GetGuiCache()->GetWorldDataController()"); // cpp:187
        CGS_ASSERT(mpStateInterface->GetAccessPointers()->GetGuiCache()
                       ->GetWorldDataController()->GetVehicleList() != 0,
                   "mpStateInterface->GetAccessPointers()->GetGuiCache()->GetWorldDataController()->GetVehicleList()"); // cpp:188

        const BrnResource::VehicleList* const lpVehicleList =
            mpStateInterface->GetAccessPointers()->GetGuiCache()
                ->GetWorldDataController()->GetVehicleList();

        // `GetVehicleIndex` answers < 0 when the id is not in the list; the console then leaves
        // the entry pointer NULL and falls into the assert (no early out).
        const s32 liVehicleIndex = lpVehicleList->GetVehicleIndex(mRivalID);
        const BrnResource::VehicleListEntry* const lpVehicleData =
            (liVehicleIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liVehicleIndex);
        CGS_ASSERT(lpVehicleData != 0, "lpVehicleData");   // cpp:190

        const CgsID lDisplayCarId = ResolveDisplayCarId(lpVehicleData, mRivalID);

        char lacCarId[KI_SCRATCH_LEN];
        char lacScratch[KI_SCRATCH_LEN];

        CgsIDConvertToString(lDisplayCarId, lacCarId);
        lacCarId[KI_SCRATCH_LEN - 1] = 0;

        CgsCore::SPrintf(lacScratch, KI_SCRATCH_LEN, KAC_CAR_CAPS_KEY_FORMAT, lacCarId);
        lacScratch[KI_SCRATCH_LEN - 1] = 0;
        maTextfields[E_TEXTFIELD_RIVAL_NAME].SetLocalisedText(lacScratch, LM::E_FORMAT_ID_LOOKUP);

        CgsCore::SPrintf(lacScratch, KI_SCRATCH_LEN, KAC_CAR_STATE_FORMAT, lacCarId);
        lacScratch[KI_SCRATCH_LEN - 1] = 0;
        mCarImageIcon.SetState(lacScratch);

        // The three stat rows have no meaning for an offline rival: blank and re-push each.
        maTextfields[E_TEXTFIELD_TEXT1].ClearText();
        maTextfields[E_TEXTFIELD_TEXT1].OutputAptData();
        maTextfields[E_TEXTFIELD_TEXT2].ClearText();
        maTextfields[E_TEXTFIELD_TEXT2].OutputAptData();
        maTextfields[E_TEXTFIELD_TEXT3].ClearText();
        maTextfields[E_TEXTFIELD_TEXT3].OutputAptData();
    }

    // @ 0x82430888 -------------------------------------------------------------------
    void RivalMapPanel::SetRivalData(const CgsNetwork::PlayerName* lpName, CgsID lRivalId,
                                     GuiCache* lpGuiCache)
    {
        CGS_ASSERT(meCurrentRivalType == E_RIVAL_TYPE_ONLINE_RIVAL,
                   "E_RIVAL_TYPE_ONLINE_RIVAL == meCurrentRivalType");   // cpp:242
        CGS_ASSERT(lRivalId != 0, "kCGSID_NULL != lRivalCarId");         // cpp:243
        CGS_ASSERT(lpGuiCache != 0, "lpGuiCache");                       // cpp:244

        if (lRivalId == 0)   // the console gates the whole body on the id, assert or not
        {
            return;
        }

        const bool lbWasActive = mbActive;
        mOnlineRivalID = lRivalId;   // std 0x5E8

        if (lbWasActive)
        {
            SetState(KAC_STATE_RIVAL);
        }

        CGS_ASSERT(mpStateInterface != 0, "mpStateInterface");                          // cpp:258
        CGS_ASSERT(mpStateInterface->GetAccessPointers() != 0,
                   "mpStateInterface->GetAccessPointers()");                            // cpp:259
        CGS_ASSERT(mpStateInterface->GetAccessPointers()->GetGuiCache() != 0,
                   "mpStateInterface->GetAccessPointers()->GetGuiCache()");             // cpp:260
        CGS_ASSERT(mpStateInterface->GetAccessPointers()->GetGuiCache()->GetWorldDataController() != 0,
                   "mpStateInterface->GetAccessPointers()->GetGuiCache()->GetWorldDataController()"); // cpp:261
        CGS_ASSERT(mpStateInterface->GetAccessPointers()->GetGuiCache()
                       ->GetWorldDataController()->GetVehicleList() != 0,
                   "mpStateInterface->GetAccessPointers()->GetGuiCache()->GetWorldDataController()->GetVehicleList()"); // cpp:262

        const BrnResource::VehicleList* const lpVehicleList =
            mpStateInterface->GetAccessPointers()->GetGuiCache()
                ->GetWorldDataController()->GetVehicleList();

        const s32 liVehicleIndex = lpVehicleList->GetVehicleIndex(mOnlineRivalID);
        const BrnResource::VehicleListEntry* const lpVehicleData =
            (liVehicleIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liVehicleIndex);
        CGS_ASSERT(lpVehicleData != 0, "lpVehicleData");   // cpp:264

        const CgsID lDisplayCarId = ResolveDisplayCarId(lpVehicleData, mOnlineRivalID);

        char lacCarId[KI_SCRATCH_LEN];
        char lacScratch[KI_SCRATCH_LEN];

        CgsIDConvertToString(lDisplayCarId, lacCarId);
        lacCarId[KI_SCRATCH_LEN - 1] = 0;

        CgsCore::SPrintf(lacScratch, KI_SCRATCH_LEN, KAC_CAR_CAPS_KEY_FORMAT, lacCarId);
        lacScratch[KI_SCRATCH_LEN - 1] = 0;
        maTextfields[E_TEXTFIELD_RIVAL_NAME].SetLocalisedText(lacScratch, LM::E_FORMAT_ID_LOOKUP);

        CgsCore::SPrintf(lacScratch, KI_SCRATCH_LEN, KAC_CAR_STATE_FORMAT, lacCarId);
        lacScratch[KI_SCRATCH_LEN - 1] = 0;
        mCarImageIcon.SetState(lacScratch);

        // Rows 2 and 3 are blanked; row 1 carries the rival's NAME (see below).
        maTextfields[E_TEXTFIELD_TEXT2].ClearText();
        maTextfields[E_TEXTFIELD_TEXT2].OutputAptData();
        maTextfields[E_TEXTFIELD_TEXT3].ClearText();
        maTextfields[E_TEXTFIELD_TEXT3].OutputAptData();

        // Only re-adopt the name when it actually changed (LobbyNameCmp answers 0 on a match,
        // and the console SKIPS the whole tail then -- @0x82430D18/@0x82430D20).
        if (LobbyNameCmp(mPlayerName.GetPlayerName(), lpName->GetPlayerName()) != 0)
        {
            mPlayerName.Construct(lpName->GetPlayerName());

            // An empty stored name displays as the build's shared "" literal; otherwise the
            // stored name itself is pushed (the console reads the FIRST byte of mPlayerName to
            // pick the arm, @0x82430D30).
            if (mPlayerName.macName[0] == 0)
            {
                maTextfields[E_TEXTFIELD_TEXT1].SetText(KAC_EMPTY_STRING);
            }
            else
            {
                maTextfields[E_TEXTFIELD_TEXT1].SetText(mPlayerName.GetPlayerName());
            }
        }
    }

    // @ 0x82417900 -------------------------------------------------------------------
    void RivalMapPanel::TransitionIn(ERivalType leRivalType)
    {
        CGS_ASSERT(static_cast<s32>(leRivalType) >= 0 && leRivalType < E_RIVAL_TYPE_COUNT,
                   "(0 <= leRivalType) && (E_RIVAL_TYPE_COUNT > leRivalType)");   // cpp:325

        // Already showing this exact rival type -> nothing to animate.
        if (mbActive && meCurrentRivalType == static_cast<s32>(leRivalType))
        {
            return;
        }

        // Type 0 (the offline PLAYER) is the only "player" flavour; every other type animates
        // in as a rival (`cmpwi r30, 0` @0x82417964).
        SetState((leRivalType != E_RIVAL_TYPE_OFFLINE_PLAYER) ? KAC_STATE_TRANS_IN_RIVAL
                                                              : KAC_STATE_TRANS_IN_PLAYER);
        meCurrentRivalType = static_cast<s32>(leRivalType);   // stw 0x5D8
        mbActive           = true;                            // stb 1, 0x7A1
    }

    // @ 0x824179B0 -------------------------------------------------------------------
    void RivalMapPanel::TransitionOut()
    {
        // The OUT animation is chosen from the type currently showing (unconditional -- there
        // is no active-flag gate on this side).
        SetState((meCurrentRivalType != E_RIVAL_TYPE_OFFLINE_PLAYER) ? KAC_STATE_TRANS_OUT_RIVAL
                                                                    : KAC_STATE_TRANS_OUT_PLAYER);
        meCurrentRivalType = E_RIVAL_TYPE_COUNT;   // stw 4, 0x5D8
        mbActive           = false;                // stb 0, 0x7A1
    }

    // @ 0x824176C0 -------------------------------------------------------------------
    void RivalMapPanel::StorePlayerInfo(const void* lpStatsResponseEvent)
    {
        CGS_ASSERT(lpStatsResponseEvent != 0, "NULL != lpStatsResponseEvent");   // @0x824176E4

        std::memcpy(maStatsResponse, lpStatsResponseEvent, KI_STATS_RESPONSE_SIZE); // @0x82417710
        mbHasPlayerInfo = true;                                                      // @0x82417718 (stb 1)
    }

    // The panel's own SetState face -- a one-line forward to the inherited IconComponent
    // entry point (sub_824E2B90), which is what every console call site on this object is.
    void RivalMapPanel::SetState(const char* lpacStateIdentifier)
    {
        IconComponent::SetState(lpacStateIdentifier);
    }
}
