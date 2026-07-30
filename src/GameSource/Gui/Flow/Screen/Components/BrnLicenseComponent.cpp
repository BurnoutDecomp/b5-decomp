// ============================================================================
// GameSource/Gui/Flow/Screen/Components/BrnLicenseComponent.cpp
//
// BrnGui::LicenseComponent -- the driver-licence screen component (see the header for the
// address table and the DWARF-authoritative member layout).
//
// Reconstructed store-for-store / call-for-call from BURNOUT_X360_ARTIST.XEX. Member
// access is BY NAME; every literal, assert line number and apt state identifier below was
// read out of the image (the state identifiers are the inline .rdata literals at
// 0x8204AE94 "idle" / 0x8204A780 "transIn" / 0x8204A75C "transOut" / 0x8204B4F8
// "invisible" / 0x82050604 "addWin" / 0x82053AF8 "upgradedTransIn" / 0x82053304
// "upgradedTransOut" / 0x82053AD8 "upgradedIdle" / 0x820505F4 "upgradedAddWins" /
// 0x82053AE8 "upgradePending").
//
// THE INTRO PATH (the only one live on PC today) is:
//   SetCachePointer -> SetPlayerInfo(name, false,false, 0,0, false,false)
//     -> EnsureResourcesAreLoaded -> OnLoad -> ShowLicense(false)
//     -> HandleAptTransitionTriggers -> ReleaseResources
// The upgrade half of the state machine (states 6..15: ShowUpgradedLicense / RankUp /
// AddWin / Update's timed cases) belongs to CompletedGame / InstantResults and is
// reproduced but not reachable from the intro.
//
// TWO FLAG'd PC-platform boundaries live in the anonymous namespace below -- both are GUI
// CACHE reads, neither is a behaviour invention, and each names the console function it
// stands in for. See their comments.
// ============================================================================

#include "GameSource/Gui/Flow/Screen/Components/BrnLicenseComponent.h"

#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache (resources + time step)
#include "GameSource/Gui/BrnGuiShared.h"                                  // gGuiResourceIdentifier (resource name table)
#include "GameSource/GameState/Progression/BrnProfile.h"                  // BrnProgression::Profile
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::PlayAptMovie / GetOutputEventQueue / GetLanguageManager
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h" // CgsGui::GuiEventAptTriggerPayload
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"           // CgsLanguage::LanguageManager::FormatDateString
#include "GameShared/GameClasses/System/Timer/PS3/CgsDateAndTimePS3.h"    // CgsSystem::DateAndTime
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SnPrintf
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStream (the streamed asserts)
#include "GameShared/GameClasses/Development/AssertSystem/CgsAssertManager.h" // CgsDev::Assert::Begin/Fire/EndAssert

#include <cstddef>   // offsetof (the x64 payload-offset words of the wire records)
#include <cstring>   // std::strcmp (the X360 inlines the apt-trigger name compares)

namespace BrnGui
{
    // ================================================================================
    // File-scope statics. Names + array widths from the DecFIGS DWARF
    // (BrnLicenseComponent.cpp:30..:67); values read from the X360 image.
    // ================================================================================

    // cpp:30 -- flt @0x8204CC88 region; Construct seeds mfRequiredWinsTickUpRate /
    // mfTimeToNextWinIncrement with it and Update/RankUp/ShowUpgradedLicense clamp to it.
    static const f32 KF_REQUIRED_WINS_TICK_UP_RATE = 0.08f;

    // cpp:32/:42/:43 @0x82F25360 / 0x82F25390 / 0x82F25398 -- the licence artwork bundles.
    // The ids index gGuiResourceIdentifier: 97..102 = "B5LicenseRank0".."B5LicenseRank5",
    // 103 = "B5LicenseElite", 104 = "B5LicenseEliteFinal"; the type word is 4 (APT).
    static const CgsGui::sResourceTuple KA_LICENSE_RESOURCES_AVAILABLE[LicenseComponent::KI_MAX_RANK + 1] =
    {
        {  97u, CgsGui::E_GUI_RESOURCETYPE_APT },   // B5LicenseRank0
        {  98u, CgsGui::E_GUI_RESOURCETYPE_APT },   // B5LicenseRank1
        {  99u, CgsGui::E_GUI_RESOURCETYPE_APT },   // B5LicenseRank2
        { 100u, CgsGui::E_GUI_RESOURCETYPE_APT },   // B5LicenseRank3
        { 101u, CgsGui::E_GUI_RESOURCETYPE_APT },   // B5LicenseRank4
        { 102u, CgsGui::E_GUI_RESOURCETYPE_APT },   // B5LicenseRank5
    };
    static const CgsGui::sResourceTuple KA_ELITE_LICENSE_RESOURCE =
        { 103u, CgsGui::E_GUI_RESOURCETYPE_APT };            // B5LicenseElite
    static const CgsGui::sResourceTuple KA_FINISHED_ELITE_LICENSE_RESOURCE =
        { 104u, CgsGui::E_GUI_RESOURCETYPE_APT };            // B5LicenseEliteFinal

    // cpp:45 @0x82F253A0 -- the total number of licence bundles (6 ranks + elite + finished).
    static const s32 KI_NUM_LICENSE_RESOURCES = 8;

    // h:207 -- the cap SetPlayerInfo asserts miNumResourcesToLoad against.
    static const s32 KI_MAX_REQUIRED_RANKS = 2;

    // cpp:48..:67 -- localisation string ids / apt sub-component names (widths are the DWARF's).
    static const char KAC_UPGRADE_STRINGID[16]             = "LICENSE_UPGRADE";
    static const char KAC_ONE_UPGRADE_STRINGID[25]         = "LICENSE_UPGRADE_SINGULAR";
    static const char KAC_ELITE_UPGRADE_STRINGID[22]       = "ELITE_LICENSE_UPGRADE";
    static const char KAC_ONE_ELITE_UPGRADE_STRINGID[31]   = "ELITE_LICENSE_UPGRADE_SINGULAR";
    static const char KAC_PERCENTAGE_COMPLETE_STRINGID[24] = "LICENSE_UPGRADE_PERCENT";
    static const char KAC_DIRT_CONTROL_VAR[14]             = "apt_dirtLevel";
    // cpp:58/:59 @0x8204CD4C / 0x8204CD54. Present in .rdata and DWARF-declared, but NO
    // recovered LicenseComponent body references either -- every apt state this TU pushes is
    // an inline literal. Kept because they are this TU's own attested statics.
    static const char KAC_RANK_BASE_TEMPLATE[7]            = "rank%d";
    static const char KAC_RANK_UP_BASE_TEMPLATE[9]         = "rankUp%d";
    static const char KAC_PLAYERNAME_TEXTFIELD_NAME[11]    = "playerName";
    static const char KAC_NEXT_RANK_TEXTFIELD_NAME[14]     = "playerUpgrade";
    static const char KAC_DATE_ISSUED_TEXTFIELD_NAME[17]   = "IssuedOnText_cpt";
    static const char KAC_COMPLETIONMONTHFIELD_NAME[14]    = "MonthText_cpt";
    static const char KAC_COMPLETIONDATEFIELD_NAME[13]     = "DateText_cpt";
    static const char KAC_COMPLETIONYEARFIELD_NAME[13]     = "YearText_cpt";

    namespace
    {
        // ---- apt state identifiers (inline .rdata literals on the console) ---------
        const char KAC_STATE_IDLE[]               = "idle";
        const char KAC_STATE_TRANSIN[]            = "transIn";
        const char KAC_STATE_TRANSOUT[]           = "transOut";
        const char KAC_STATE_INVISIBLE[]          = "invisible";
        const char KAC_STATE_ADDWIN[]             = "addWin";
        const char KAC_STATE_UPGRADEPENDING[]     = "upgradePending";
        const char KAC_STATE_UPGRADED_TRANSIN[]   = "upgradedTransIn";
        const char KAC_STATE_UPGRADED_TRANSOUT[]  = "upgradedTransOut";
        const char KAC_STATE_UPGRADED_IDLE[]      = "upgradedIdle";
        const char KAC_STATE_UPGRADED_ADDWINS[]   = "upgradedAddWins";

        // The empty string the console passes as `&unk_820046A7` (unmounting a movie /
        // blanking a text field).
        const char KAC_EMPTY_STRING[] = "";

        // @0x82053B70 -- pushed as LITERAL text (SetText, not SetLocalisedText): the field
        // resolves the leading '$' itself.
        const char KAC_UPGRADE_PENDING_TEXT[] = "$LICENSE_UPGRADE_PENDING";

        // The apt level the licence's OWN movie is mounted at (OnLoad, Update case 9,
        // HandleAptTransitionTriggers case 6 and ReleaseResources all pass 4; the photo booth
        // uses 5 and the intro screen movie itself 3).
        const s32 KI_LICENSE_APT_LEVEL = 4;

        // Queue channels the X360 AddEvent calls name.
        const s32 KI_CHANNEL_GUI_OUT    = 40;   // GuiEventOut     (OutputGuiEvent<T>)
        const s32 KI_CHANNEL_VIEW_STATE = 41;   // GuiOutViewState (OutputViewState<T>)

        // The LanguageManager::ParameterFormatType literals the X360 passes: 9 for a database
        // string id, 12 for a plain integer field, 11 / 13 for the two positional-parameter
        // slots.
        const CgsLanguage::LanguageManager::ParameterFormatType KE_FORMAT_ID_LOOKUP =
            static_cast<CgsLanguage::LanguageManager::ParameterFormatType>(9);
        const CgsLanguage::LanguageManager::ParameterFormatType KE_FORMAT_INTEGER_FIELD =
            static_cast<CgsLanguage::LanguageManager::ParameterFormatType>(12);
        const CgsLanguage::LanguageManager::ParameterFormatType KE_FORMAT_PARAM_WINS =
            static_cast<CgsLanguage::LanguageManager::ParameterFormatType>(11);
        const CgsLanguage::LanguageManager::ParameterFormatType KE_FORMAT_PARAM_PERCENT =
            static_cast<CgsLanguage::LanguageManager::ParameterFormatType>(13);

        // @0x82F27E8C -- the same month string-id table BrnIntro.cpp carries (SetPlayerInfo
        // indexes it for the 100%-completion date).
        const s32 KI_NUM_MONTH_STRINGIDS = 12;
        const char* const KAPC_MONTH_STRINGIDS[KI_NUM_MONTH_STRINGIDS] =
        {
            "MONTH_SHORT_01", "MONTH_SHORT_02", "MONTH_SHORT_03", "MONTH_SHORT_04",
            "MONTH_SHORT_05", "MONTH_SHORT_06", "MONTH_SHORT_07", "MONTH_SHORT_08",
            "MONTH_SHORT_09", "MONTH_SHORT_10", "MONTH_SHORT_11", "MONTH_SHORT_12",
        };

        // ---- BrnGui::GuiEventNetworkOutputPlayerTexture (id 264) -------------------
        // Same record BrnPhotoBoothComponent.cpp builds: { payloadBytes, type, payloadOffset }
        // + { mode, playerIndex }, channel 40, console size 20. Payload is two words with no
        // pointers, so the x64 record has the same shape -- but the header words are still
        // DERIVED from the real layout rather than stamped with the console literals.
        // The licence card's own mode literal is 5 (StartOutputtingGamerpic @0x8243CC30 packs
        // {5, -1}); StopOutputtingGamerpic @0x8243CD00 packs {0, -1}, the shared "off" mode
        // the photo booth also uses.
        enum EPlayerTextureMode
        {
            E_PLAYERTEXTURE_OFF             = 0,   // StopOutputtingGamerpic
            E_PLAYERTEXTURE_LICENCE_GAMERPIC = 5,  // StartOutputtingGamerpic
        };

        struct GuiEventNetworkOutputPlayerTextureRecord : public CgsGui::GuiEvent<264>
        {
            s32 meMode;          // payload +0x00
            s32 miPlayerIndex;   // payload +0x04 (both emit sites pass -1)

            GuiEventNetworkOutputPlayerTextureRecord(s32 leMode, s32 liPlayerIndex)
                : CgsGui::GuiEvent<264>(), meMode(leMode), miPlayerIndex(liPlayerIndex)
            {
                const size_t luOffset = offsetof(GuiEventNetworkOutputPlayerTextureRecord, meMode);
                muHeader0 = static_cast<u32>(sizeof(*this) - luOffset);   // X360 8
                muHeader2 = static_cast<u32>(luOffset);                   // X360 12
            }
        };

        // ---- BrnGui::GuiEventNetworkPlayerImage (id 258) ---------------------------
        // { texture, index }, channel 41, console size 20. The first payload word is a
        // POINTER, so the x64 payload is wider than the console's 8 -- both header words are
        // derived. The licence card passes index 1 (the photo booth passes 0).
        struct GuiEventNetworkPlayerImageRecord : public CgsGui::GuiEvent<258>
        {
            const CgsNetwork::NetworkTexture* mpTexture;   // payload +0x00
            s32                               miIndex;     // payload +0x08 x64 (+0x04 console)

            GuiEventNetworkPlayerImageRecord(const CgsNetwork::NetworkTexture* lpTexture, s32 liIndex)
                : CgsGui::GuiEvent<258>(), mpTexture(lpTexture), miIndex(liIndex)
            {
                const size_t luOffset = offsetof(GuiEventNetworkPlayerImageRecord, mpTexture);
                muHeader0 = static_cast<u32>(sizeof(*this) - luOffset);   // X360 8
                muHeader2 = static_cast<u32>(luOffset);                   // X360 12
            }
        };

        // ---- the 16-byte GuiEvent<N> command record { 1, N, 12 } + one flag byte ----
        // Byte-identical to BrnIntro.cpp's helper (SetPlayerInfo posts id 435 with it).
        template <s32 N>
        struct GuiCommandEvent16 : public CgsGui::GuiEvent<N>
        {
            u8 mu8Flag;
            u8 maPad[3];
            GuiCommandEvent16(u8 lu8Flag = 0) : CgsGui::GuiEvent<N>(1, 12), mu8Flag(lu8Flag)
            { maPad[0] = maPad[1] = maPad[2] = 0; }
        };

        template <s32 N>
        void PostCommand16(CgsGui::StateInterface* lpInterface, s32 liChannel, u8 lu8Flag = 0)
        {
            GuiCommandEvent16<N> lEvent(lu8Flag);
            lpInterface->GetOutputEventQueue()->AddEvent(&lEvent, liChannel,
                                                         static_cast<s32>(sizeof(lEvent)));
        }

        void PostPlayerTextureEvent(CgsGui::StateInterface* lpStateInterface, s32 leMode)
        {
            GuiEventNetworkOutputPlayerTextureRecord lRecord(leMode, -1);
            lpStateInterface->GetOutputEventQueue()->AddEvent(&lRecord, KI_CHANNEL_GUI_OUT,
                                                             static_cast<s32>(sizeof(lRecord)));
        }

        // ================================================================================
        // FLAG'd PC-platform GUI-CACHE boundaries. Both stand in for reads the console makes
        // straight through BrnGui::GuiCache; each names the console callee it replaces and
        // returns the console's own answer for the state the PC cache is actually in.
        // ================================================================================

        // X360: `mpGuiCache->GetWorldDataController()->GetRequiredWinsInRank(liRank)`
        // (BrnGui::WorldDataController::GetRequiredWinsInRank @0x82428740 -- rank -1 returns 0,
        // otherwise it asserts the controller is READY and reads the loaded ProgressionData's
        // per-rank required-wins word).
        //
        // The GUI cache's WorldDataController (GuiCache +0x4064) is the GUI's front end onto
        // the streamed progression resource. NOTHING ON PC POPULATES IT YET -- the pointer is
        // never assigned in this tree, and GuiCache::GetWorldDataController() would fire its
        // own "mpWorldDataController" assert and hand back NULL. Until the GUI-side
        // world/progression acquisition lands (BrnGuiWorldDataController's Prepare state
        // machine), report the console's own no-rank-data answer: 0, which is exactly what
        // GetRequiredWinsInRank itself returns for an unknown rank. With 0 the three consumers
        // behave as they do for a brand-new profile -- ShowLicense picks
        // E_LICENSE_SHOWING_NORMAL, SetPlayerInfo leaves miWinsInCurrentRank at 0, and
        // UpdateDirt clamps to the console's 0.01 dirt ceiling.
        // FLAG PC-platform leaf.
        s32 CacheGetRequiredWinsInRank(GuiCache* /*lpGuiCache*/, s32 /*liRank*/) { return 0; }

        // X360: the u16 at `mpGuiCache + 0xB874` that RankUp @0x8243C918 and Update's
        // E_LICENSE_UPGRADING_ADDING_REQUIRED_WINS case read as the wins needed in the rank
        // being upgraded INTO (`lhzx r6, mpGuiCache, 0xB874`). The byte still sits inside
        // BrnGuiCache.h's mPad_B865[19] hole ("un-modelled sat-nav/landmark words") and has no
        // DWARF name, so it is NOT carved here -- guessing a member name is worse than naming
        // the boundary. Reached only from the UPGRADE half of the state machine
        // (CompletedGame / InstantResults), which nothing on PC enters today.
        // FLAG PC-platform leaf.
        s32 CacheGetUpgradeRequiredWins(GuiCache* /*lpGuiCache*/) { return 0; }
    }

    // ================================================================================
    // @0x8241A610 (cpp:91) -- VIRTUAL (overrides CgsGui::GuiComponent::Construct).
    // ================================================================================
    void LicenseComponent::Construct(const char* lpacName,
                                     CgsGui::StateInterface* lpStateInterface,
                                     const char* lpacParentName)
    {
        // IconComponent::Construct(name, stateInterface, /*stateIdentifiers*/ NULL, parentName).
        IconComponent::Construct(lpacName, lpStateInterface, 0, lpacParentName);

        mfRequiredWinsTickUpRate = KF_REQUIRED_WINS_TICK_UP_RATE;   // +0x7BC
        mpGuiCache               = 0;                               // +0xA8
        mfTimeToNextWinIncrement = KF_REQUIRED_WINS_TICK_UP_RATE;   // +0x7C4
        mpProfile                = 0;                               // +0xAC
        mbAtTopRank              = false;                           // +0x7A0
        mbElite                  = false;                           // +0x7A1
        mfTimeToShowNextRank     = 0.0f;                            // +0x7C0
        mbFinishedGame           = false;                           // +0x7A2
        miCurrentRank            = -1;                              // +0x7A4
        miWinsInCurrentRank      = -1;                              // +0x7A8
        miPercentComplete        = 0;                               // +0x7AC
        mbShowUpgradePending     = true;                            // +0x7B8
        mbShowPoints             = true;                            // +0x7B9
        mbVisible                = false;                           // +0x7B0
        mbHiding                 = false;                           // +0x7B1
        meCurrentLicenseState    = E_LICENSE_CONSTRUCTED;           // +0x7B4
        mbForceCentred           = false;                           // +0x7BA

        maLicenseResourcesToLoad[0].muId   = 0;                                  // +0x94
        maLicenseResourcesToLoad[0].meType = CgsGui::E_GUI_RESOURCETYPE_START;   // +0x98
        maLicenseResourcesToLoad[1].muId   = 0;                                  // +0x9C
        maLicenseResourcesToLoad[1].meType = CgsGui::E_GUI_RESOURCETYPE_START;   // +0xA0
        miNumResourcesToLoad               = 0;                                  // +0xA4

        // The six embedded fields are Constructed (virtual) parented on this component's name.
        mPlayerNameTextField.Construct(KAC_PLAYERNAME_TEXTFIELD_NAME, lpStateInterface, GetName());
        mNextRankTextField.Construct(KAC_NEXT_RANK_TEXTFIELD_NAME, lpStateInterface, GetName());
        mDateIssuedTextField.Construct(KAC_DATE_ISSUED_TEXTFIELD_NAME, lpStateInterface, GetName());
        mCompletionMonthField.Construct(KAC_COMPLETIONMONTHFIELD_NAME, lpStateInterface, GetName());
        mCompletionDateField.Construct(KAC_COMPLETIONDATEFIELD_NAME, lpStateInterface, GetName());
        mCompletionYearField.Construct(KAC_COMPLETIONYEARFIELD_NAME, lpStateInterface, GetName());

        mbShowingProfilePicture = false;                            // +0x7C8
    }

    // ================================================================================
    // @0x8241A790 (cpp:143) -- this component plus its three VISIBLE text fields. The three
    // completion fields are deliberately absent (the console registers four names).
    // ================================================================================
    void LicenseComponent::AppendExpectedAptComponent(GuiFlow leFlow)
    {
        CGS_ASSERT(mpGuiCache != 0, "NULL != mpGuiCache");                         // cpp:145
        CGS_ASSERT((E_GUIFLOW_FIRST <= leFlow) && (E_GUIFLOW_COUNT > leFlow),
                   "(E_GUIFLOW_FIRST <= leFlow) && (E_GUIFLOW_COUNT > leFlow)");   // cpp:147

        mpGuiCache->AppendExpectedAptComponent(leFlow, GetName());
        mpGuiCache->AppendExpectedAptComponent(leFlow, mPlayerNameTextField.GetName());
        mpGuiCache->AppendExpectedAptComponent(leFlow, mNextRankTextField.GetName());
        mpGuiCache->AppendExpectedAptComponent(leFlow, mDateIssuedTextField.GetName());
    }

    // ================================================================================
    // @0x82440AC0 (cpp:167) -- the selected rank's bundle has arrived: mount its movie by
    // NAME at level 4, then decide between the profile's own licence picture and the Xbox
    // gamer picture.
    // ================================================================================
    void LicenseComponent::OnLoad()
    {
        CGS_ASSERT(meCurrentLicenseState == E_LICENSE_DATA_SUPPLIED,
                   "E_LICENSE_DATA_SUPPLIED == meCurrentLicenseState");   // cpp:169

        mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[maLicenseResourcesToLoad[0].muId],
                                       KI_LICENSE_APT_LEVEL);

        mbShowingProfilePicture = (mpProfile->GetPlayerLicencePicture() != 0);
        if (!mbShowingProfilePicture)
            StartOutputtingGamerpic();

        meCurrentLicenseState = E_LICENSE_FIRST_RESOURCE_LOADED;
    }

    // ================================================================================
    // @0x82440BC0 (cpp:200) -- unmount the movie, stop the gamerpic feed, hand the tuples
    // back. Note the unload is the CACHE's UnloadResources (the whole array), not the
    // per-tuple Ensure* the upgrade path uses.
    // ================================================================================
    void LicenseComponent::ReleaseResources()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:202

        mpStateInterface->PlayAptMovie(KAC_EMPTY_STRING, KI_LICENSE_APT_LEVEL);

        if (!mbShowingProfilePicture)
            StopOutputtingGamerpic();

        if (meCurrentLicenseState != E_LICENSE_RESOURCES_UNLOADED)
        {
            mpGuiCache->UnloadResources(maLicenseResourcesToLoad,
                                        static_cast<u32>(miNumResourcesToLoad));
            meCurrentLicenseState = E_LICENSE_RESOURCES_UNLOADED;
        }
    }

    // ================================================================================
    // @0x8241A848 (cpp:232) -- an apt clip reloaded and dropped the text it was showing:
    // re-push that field's own cached string. The console open-codes six strcmp chains
    // against each field's macName, then one against this component's own name.
    // ================================================================================
    bool LicenseComponent::HandleAptLoadTriggers(const CgsGui::GuiEventAptTriggerPayload* lpAptTrigger)
    {
        CGS_ASSERT(lpAptTrigger != 0, "lpAptTrigger");                                   // cpp:234
        CGS_ASSERT(lpAptTrigger->meEventType == CgsGui::GuiEventAptTrigger::E_APT_EVENT_ONLOAD,
                   "CgsGui::GuiEventAptTrigger::E_APT_EVENT_ONLOAD == lpAptTrigger->meEventType"); // cpp:235

        const char* lpacName = lpAptTrigger->mpacComponentName;

        if (std::strcmp(mPlayerNameTextField.GetName(), lpacName) == 0)
        {
            mPlayerNameTextField.SetText(mPlayerNameTextField.GetText());
            return true;
        }
        if (std::strcmp(mNextRankTextField.GetName(), lpacName) == 0)
        {
            mNextRankTextField.SetText(mNextRankTextField.GetText());
            return true;
        }
        if (std::strcmp(mDateIssuedTextField.GetName(), lpacName) == 0)
        {
            mDateIssuedTextField.SetText(mDateIssuedTextField.GetText());
            return true;
        }
        if (std::strcmp(mCompletionMonthField.GetName(), lpacName) == 0)
        {
            mCompletionMonthField.SetText(mCompletionMonthField.GetText());
            return true;
        }
        if (std::strcmp(mCompletionDateField.GetName(), lpacName) == 0)
        {
            mCompletionDateField.SetText(mCompletionDateField.GetText());
            return true;
        }
        if (std::strcmp(mCompletionYearField.GetName(), lpacName) == 0)
        {
            mCompletionYearField.SetText(mCompletionYearField.GetText());
            return true;
        }
        if (std::strcmp(GetName(), lpacName) == 0)
        {
            if (meCurrentLicenseState == E_LICENSE_UPGRADING_NEW_LICENSE_INITIALISING)
                meCurrentLicenseState = E_LICENSE_UPGRADING_NEW_LICENSE_WAITING;
            return true;
        }
        return false;
    }

    // ================================================================================
    // @0x8243BE20 (cpp:294) -- a transition finished ON THIS COMPONENT: advance the state
    // machine. A trigger for any other component returns false without touching anything;
    // an unexpected state fires the streamed assert and still reports the trigger consumed
    // (the console sets its result to 1 before the switch and every arm falls through to it).
    // ================================================================================
    bool LicenseComponent::HandleAptTransitionTriggers(const CgsGui::GuiEventAptTriggerPayload* lpAptTrigger)
    {
        CGS_ASSERT(lpAptTrigger != 0, "lpAptTrigger");                                   // cpp:296
        CGS_ASSERT(lpAptTrigger->meEventType == CgsGui::GuiEventAptTrigger::E_APT_EVENT_TRANSITION_COMPLETE,
                   "CgsGui::GuiEventAptTrigger::E_APT_EVENT_TRANSITION_COMPLETE == lpAptTrigger->meEventType"); // cpp:297

        if (std::strcmp(GetName(), lpAptTrigger->mpacComponentName) != 0)
            return false;

        switch (meCurrentLicenseState)
        {
        case E_LICENSE_SHOWING_NORMAL:
            break;

        case E_LICENSE_SHOWING_UPGRADE_PENDING:
            SetState(mbForceCentred ? KAC_STATE_UPGRADED_IDLE : KAC_STATE_UPGRADEPENDING);
            mNextRankTextField.SetText(KAC_EMPTY_STRING);
            UpdateDirt();
            break;

        case E_LICENSE_SHOWING_TRANSOUT:
            mbHiding              = false;
            mbVisible             = false;
            meCurrentLicenseState = E_LICENSE_FIRST_RESOURCE_LOADED;
            break;

        case E_LICENSE_UPGRADING_OLD_LICENSE_LEAVING:
            mpStateInterface->PlayAptMovie(KAC_EMPTY_STRING, KI_LICENSE_APT_LEVEL);
            mpGuiCache->EnsureResourceIsUnloaded(maLicenseResourcesToLoad[0]);
            meCurrentLicenseState = E_LICENSE_FIRST_RESOURCE_UNLOADING;
            break;

        case E_LICENSE_UPGRADING_NEW_LICENSE_ARRIVING:
            if (mbShowPoints)
            {
                SetState(KAC_STATE_UPGRADED_ADDWINS);
                meCurrentLicenseState = E_LICENSE_UPGRADING_ADDING_REQUIRED_WINS;
                UpdateDirt();
            }
            else
            {
                SetState(KAC_STATE_UPGRADED_IDLE);
                meCurrentLicenseState = E_LICENSE_UPGRADING_DONE;
            }
            break;

        case E_LICENSE_UPGRADING_DONE:
            mbHiding  = false;
            mbVisible = false;
            break;

        default:
        {
            // STREAMED on the console (LABEL default of @0x8243BE20).
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "This component should not generate a trans complete in this state ("
                       << static_cast<s32>(meCurrentLicenseState) << ")";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);   // cpp:381
            CgsDev::Assert::EndAssert();
            break;
        }
        }

        return true;
    }

    // ================================================================================
    // @0x8243C0B8 (cpp:399) -- the per-frame pump. Only the four streaming / timed upgrade
    // states do anything; every other state falls through the jump table's default.
    // ================================================================================
    void LicenseComponent::Update()
    {
        switch (meCurrentLicenseState)
        {
        case E_LICENSE_FIRST_RESOURCE_UNLOADING:
            if (mpGuiCache->EnsureResourceIsUnloaded(maLicenseResourcesToLoad[0]))
            {
                CGS_ASSERT(miNumResourcesToLoad >= 2, "miNumResourcesToLoad >= 2");   // cpp:408
                mpGuiCache->EnsureResourceIsLoaded(maLicenseResourcesToLoad[1]);
                meCurrentLicenseState = E_LICENSE_SECOND_RESOURCE_LOADING;
            }
            break;

        case E_LICENSE_SECOND_RESOURCE_LOADING:
            CGS_ASSERT(miNumResourcesToLoad == 2, "miNumResourcesToLoad == 2");       // cpp:419
            if (mpGuiCache->EnsureResourceIsLoaded(maLicenseResourcesToLoad[1]))
            {
                mpStateInterface->PlayAptMovie(
                    gGuiResourceIdentifier[maLicenseResourcesToLoad[1].muId], KI_LICENSE_APT_LEVEL);
                meCurrentLicenseState = E_LICENSE_UPGRADING_NEW_LICENSE_INITIALISING;
            }
            break;

        case E_LICENSE_UPGRADING_NEW_LICENSE_WAITING:
            mfTimeToShowNextRank -= mpGuiCache->GetTimeStep();
            if (mfTimeToShowNextRank <= 0.0f)
            {
                SetState(KAC_STATE_UPGRADED_TRANSIN);
                UpdateDirt();
                mbHiding              = false;
                mbVisible             = true;
                meCurrentLicenseState = E_LICENSE_UPGRADING_NEW_LICENSE_ARRIVING;
            }
            break;

        case E_LICENSE_UPGRADING_ADDING_REQUIRED_WINS:
            if (mfTimeToNextWinIncrement <= 0.0f)
            {
                const s32 liRequiredWins = CacheGetUpgradeRequiredWins(mpGuiCache);
                if (liRequiredWins == 1)
                {
                    // NOTE the console's own asymmetry: the SINGULAR branch tests
                    // miCurrentRank == KI_MAX_RANK, the plural branch tests mbAtTopRank.
                    mNextRankTextField.SetLocalisedText(
                        miCurrentRank == KI_MAX_RANK ? KAC_ONE_ELITE_UPGRADE_STRINGID
                                                     : KAC_ONE_UPGRADE_STRINGID,
                        KE_FORMAT_ID_LOOKUP);
                }
                else
                {
                    char lacWins[128];
                    CgsCore::SnPrintf(lacWins, 128, "%d", liRequiredWins);
                    lacWins[127] = 0;

                    const char* lapacParams[1] = { lacWins };
                    const CgsLanguage::LanguageManager::ParameterFormatType laeFormats[1] =
                        { KE_FORMAT_PARAM_WINS };
                    mNextRankTextField.SetLocalisedText(
                        mbAtTopRank ? KAC_ELITE_UPGRADE_STRINGID : KAC_UPGRADE_STRINGID,
                        KE_FORMAT_ID_LOOKUP, 1, lapacParams, laeFormats);
                }

                UpdateDirt();

                if (miWinsInCurrentRank != 0)
                {
                    mfTimeToNextWinIncrement += mfRequiredWinsTickUpRate;
                    --miWinsInCurrentRank;
                }
                else
                {
                    SetState(KAC_STATE_UPGRADED_IDLE);
                    mbHiding              = false;
                    meCurrentLicenseState = E_LICENSE_UPGRADING_DONE;
                }
            }
            // The console subtracts the frame step AFTER the branch, unconditionally.
            mfTimeToNextWinIncrement -= mpGuiCache->GetTimeStep();
            break;

        default:
            break;
        }
    }

    // ================================================================================
    // @0x8243C380 (cpp:551) -- everything the card displays, plus the rank resource choice.
    // ================================================================================
    void LicenseComponent::SetPlayerInfo(const char* lpcPlayerName, bool lbElite,
                                         bool lbFinishedGame, s32 liRank,
                                         s32 liPointsToNextRank, bool lbShowUpgradePending,
                                         bool lbShowPoints)
    {
        CGS_ASSERT(lpcPlayerName != 0, "lpcPlayerName");                        // cpp:553
        CGS_ASSERT(lpcPlayerName[0] != '\0', "lpcPlayerName[0]!='\\0'");        // cpp:554
        CGS_ASSERT(liPointsToNextRank >= 0, "liPointsToNextRank >= 0");         // cpp:555
        // "if you have finished the game you must also be elite" (the X360 tests
        // lbFinishedGame && !lbElite).
        CGS_ASSERT(!lbFinishedGame || lbElite, "lbElite");                      // cpp:560

        mbFinishedGame       = lbFinishedGame;
        mbElite              = lbElite;
        mbShowUpgradePending = lbShowUpgradePending;
        mbShowPoints         = lbShowPoints;
        mbAtTopRank          = (liRank == KI_MAX_RANK);

        // The name is passed to SnPrintf as the FORMAT string (the console emits
        // SnPrintf(buf, 128, lpcPlayerName)); it is a language-database string id here, which
        // is why the field then resolves it as one.
        char lacName[128];
        CgsCore::SnPrintf(lacName, 128, lpcPlayerName);
        lacName[127] = 0;
        mPlayerNameTextField.SetLocalisedText(lacName, KE_FORMAT_ID_LOOKUP);

        if (mpProfile->GetHaveSet100PercentCompletedDate())
        {
            const CgsSystem::DateAndTime lCompletedDate = mpProfile->Get100PercentCompletedDate();

            const s32 liMonth = lCompletedDate.GetMonth() - 1;
            CGS_ASSERT(liMonth >= 0, "liMonth >= 0");                                    // cpp:585
            CGS_ASSERT(liMonth < KI_NUM_MONTH_STRINGIDS, "liMonth < KI_NUM_MONTH_STRINGIDS"); // cpp:586

            mCompletionMonthField.SetLocalisedText(KAPC_MONTH_STRINGIDS[liMonth], KE_FORMAT_ID_LOOKUP);
            mCompletionDateField.SetLocalisedText(lCompletedDate.GetDay(), KE_FORMAT_INTEGER_FIELD);
            mCompletionYearField.SetLocalisedText(lCompletedDate.GetYear(), KE_FORMAT_INTEGER_FIELD);
        }
        else
        {
            // The console blanks each field's macText in place then re-pushes the apt data.
            mCompletionMonthField.ClearText();
            mCompletionMonthField.OutputAptData();
            mCompletionDateField.ClearText();
            mCompletionDateField.OutputAptData();
            mCompletionYearField.ClearText();
            mCompletionYearField.OutputAptData();
        }

        if (mbShowPoints)
        {
            if (mbElite)
            {
                // The 16-byte { 1, 435, 12 } command record on the GUI-out channel.
                PostCommand16<435>(mpStateInterface, KI_CHANNEL_GUI_OUT);
            }
            else if (liPointsToNextRank <= 1)
            {
                if (mbShowUpgradePending)
                {
                    mNextRankTextField.SetText(KAC_UPGRADE_PENDING_TEXT);
                }
                else
                {
                    mNextRankTextField.SetLocalisedText(
                        mbAtTopRank ? KAC_ONE_ELITE_UPGRADE_STRINGID : KAC_ONE_UPGRADE_STRINGID,
                        KE_FORMAT_ID_LOOKUP);
                }
            }
            else
            {
                char lacPoints[128];
                CgsCore::SnPrintf(lacPoints, 128, "%d", liPointsToNextRank);
                lacPoints[127] = 0;

                mNextRankTextField.SetLocalisedText(
                    mbAtTopRank ? KAC_ELITE_UPGRADE_STRINGID : KAC_UPGRADE_STRINGID,
                    KE_FORMAT_ID_LOOKUP, 1, lacPoints, KE_FORMAT_PARAM_WINS);
            }
        }
        else
        {
            mNextRankTextField.ClearText();
            mNextRankTextField.OutputAptData();
        }

        if (mbFinishedGame)
        {
            maLicenseResourcesToLoad[0] = KA_FINISHED_ELITE_LICENSE_RESOURCE;
            miNumResourcesToLoad        = 1;
            miCurrentRank               = -1;
            miWinsInCurrentRank         = 0;
        }
        else if (mbElite)
        {
            maLicenseResourcesToLoad[0] = KA_ELITE_LICENSE_RESOURCE;
            maLicenseResourcesToLoad[1] = KA_FINISHED_ELITE_LICENSE_RESOURCE;
            miNumResourcesToLoad        = 2;
            miCurrentRank               = -1;
            miWinsInCurrentRank         = 0;
        }
        else
        {
            s32 liWins = CacheGetRequiredWinsInRank(mpGuiCache, liRank) - liPointsToNextRank;
            if (liWins <= 0)
                liWins = 0;

            miWinsInCurrentRank         = liWins;
            miCurrentRank               = liRank;
            maLicenseResourcesToLoad[0] = KA_LICENSE_RESOURCES_AVAILABLE[liRank];
            miNumResourcesToLoad        = 1;
            maLicenseResourcesToLoad[1] = (liRank >= KI_MAX_RANK)
                                              ? KA_ELITE_LICENSE_RESOURCE
                                              : KA_LICENSE_RESOURCES_AVAILABLE[liRank + 1];
            ++miNumResourcesToLoad;
        }

        CGS_ASSERT(miNumResourcesToLoad <= KI_MAX_REQUIRED_RANKS,
                   "miNumResourcesToLoad <= KI_MAX_REQUIRED_RANKS");   // cpp:697

        UpdateDirt();
        meCurrentLicenseState = E_LICENSE_DATA_SUPPLIED;
    }

    // ================================================================================
    // @0x824277D0 (cpp:716) -- both pushes are IMMEDIATE (the third arg is 1). The console
    // prints through an 11-char buffer and NULs index 11.
    // ================================================================================
    void LicenseComponent::SetPosition(Vector2 lv2Position)
    {
        char lacValue[12];

        CgsCore::SnPrintf(lacValue, 11, "%f", lv2Position.x);
        lacValue[11] = 0;
        AddOutputAptViewState("_x", lacValue, true);

        CgsCore::SnPrintf(lacValue, 11, "%f", lv2Position.y);
        lacValue[11] = 0;
        AddOutputAptViewState("_y", lacValue, true);
    }

    // ================================================================================
    // @0x82440C98 (cpp:743) -- bring the card on screen.
    // ================================================================================
    void LicenseComponent::ShowLicense(bool lbForceCentred)
    {
        CGS_ASSERT(meCurrentLicenseState == E_LICENSE_FIRST_RESOURCE_LOADED,
                   "E_LICENSE_FIRST_RESOURCE_LOADED == meCurrentLicenseState");   // cpp:745

        mbForceCentred = lbForceCentred;
        SetState(lbForceCentred ? KAC_STATE_UPGRADED_TRANSIN : KAC_STATE_TRANSIN);

        mPlayerNameTextField.SetText(mPlayerNameTextField.GetText());
        mNextRankTextField.SetText(mNextRankTextField.GetText());
        UpdateDirt();

        mbHiding = false;

        bool lbOneWinFromUpgrade = false;
        if (!mbElite)
        {
            // X360: the GuiCache::GetWorldDataController assert (BrnGuiCache.h:2324) is
            // inlined here, then GetRequiredWinsInRank(miCurrentRank).
            if ((CacheGetRequiredWinsInRank(mpGuiCache, miCurrentRank) - miWinsInCurrentRank) == 1)
                lbOneWinFromUpgrade = true;
        }

        meCurrentLicenseState = (lbOneWinFromUpgrade && mbShowUpgradePending)
                                    ? E_LICENSE_SHOWING_UPGRADE_PENDING
                                    : E_LICENSE_SHOWING_NORMAL;

        CGS_ASSERT(mpProfile != 0, "mpProfile");   // cpp:787

        mbShowingProfilePicture = (mpProfile->GetPlayerLicencePicture() != 0);
        if (!mbShowingProfilePicture)
            StartOutputtingGamerpic();

        mbVisible = true;
    }

    // ================================================================================
    // @0x8243C7E0 (cpp:814) -- begin the rank-upgrade presentation.
    // ================================================================================
    void LicenseComponent::ShowUpgradedLicense(f32 lfWinTickUpDuration, bool lbShowPoints)
    {
        mbShowPoints = lbShowPoints;

        mpStateInterface->PlayAptMovie(KAC_EMPTY_STRING, KI_LICENSE_APT_LEVEL);
        mpGuiCache->EnsureResourceIsUnloaded(maLicenseResourcesToLoad[0]);

        mfTimeToShowNextRank = 0.0f;

        if (miCurrentRank == KI_MAX_RANK)
        {
            mbElite = true;
        }
        else if (!mbElite && !mbFinishedGame)
        {
            ++miCurrentRank;
            if (miCurrentRank == KI_MAX_RANK)
                mbAtTopRank = true;
        }

        mfRequiredWinsTickUpRate =
            ((miWinsInCurrentRank * KF_REQUIRED_WINS_TICK_UP_RATE) <= lfWinTickUpDuration)
                ? KF_REQUIRED_WINS_TICK_UP_RATE
                : (lfWinTickUpDuration / static_cast<f32>(miWinsInCurrentRank));
        mfTimeToNextWinIncrement = mfRequiredWinsTickUpRate;

        meCurrentLicenseState = E_LICENSE_FIRST_RESOURCE_UNLOADING;
    }

    // ================================================================================
    // @0x82434998 (cpp:868) -- play the transition-out frame.
    // ================================================================================
    void LicenseComponent::HideLicense()
    {
        if (meCurrentLicenseState > E_LICENSE_SHOWING_UPGRADE_PENDING)
        {
            if (meCurrentLicenseState <= E_LICENSE_UPGRADING_DONE)
                SetState(KAC_STATE_UPGRADED_TRANSOUT);
        }
        else
        {
            SetState(mbForceCentred ? KAC_STATE_UPGRADED_TRANSOUT : KAC_STATE_TRANSOUT);
            meCurrentLicenseState = E_LICENSE_SHOWING_TRANSOUT;
        }

        UpdateDirt();
        mbHiding = true;
    }

    // ================================================================================
    // @0x82434A30 (cpp:904) -- one more win in the current rank.
    // ================================================================================
    void LicenseComponent::AddWin()
    {
        if (mbFinishedGame || mbElite)
            return;

        const ELicenseStates leState = meCurrentLicenseState;
        ++miWinsInCurrentRank;

        if (leState != E_LICENSE_SHOWING_NORMAL)
            return;

        const s32 liWinsLeft =
            CacheGetRequiredWinsInRank(mpGuiCache, miCurrentRank) - miWinsInCurrentRank;

        if (liWinsLeft < 2)
        {
            mNextRankTextField.SetLocalisedText(
                mbAtTopRank ? KAC_ONE_ELITE_UPGRADE_STRINGID : KAC_ONE_UPGRADE_STRINGID,
                KE_FORMAT_ID_LOOKUP);
        }
        else
        {
            char lacWins[128];
            CgsCore::SnPrintf(lacWins, 128, "%d", liWinsLeft);
            lacWins[127] = 0;

            const char* lapacParams[1] = { lacWins };
            const CgsLanguage::LanguageManager::ParameterFormatType laeFormats[1] =
                { KE_FORMAT_PARAM_WINS };
            mNextRankTextField.SetLocalisedText(
                mbAtTopRank ? KAC_ELITE_UPGRADE_STRINGID : KAC_UPGRADE_STRINGID,
                KE_FORMAT_ID_LOOKUP, 1, lapacParams, laeFormats);
        }

        SetState(mbForceCentred ? KAC_STATE_UPGRADED_ADDWINS : KAC_STATE_ADDWIN);
        UpdateDirt();
        mbHiding = false;
    }

    // ================================================================================
    // @0x8241AAD8 (cpp:988) -- reveal the wins line.
    // ================================================================================
    void LicenseComponent::ShowScore()
    {
        if (!mbShowPoints && mbVisible)
            SetState(mbForceCentred ? KAC_STATE_UPGRADED_ADDWINS : KAC_STATE_ADDWIN);

        mbShowPoints = true;
    }

    // ================================================================================
    // @0x8243C918 (cpp:1019) -- the animated licence swap.
    // ================================================================================
    void LicenseComponent::RankUp(f32 lfTimeToNextWinIncrement, f32 lfTimeToShowNextRank,
                                  bool lbShowPoints)
    {
        if (mbFinishedGame)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Shouldn't be ranking up on the final licence.";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);   // cpp:1021
            CgsDev::Assert::EndAssert();
        }
        if (static_cast<u32>(miCurrentRank) >= 6u && !mbElite)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Invalid rank to rank up from (" << miCurrentRank << ")";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);   // cpp:1022
            CgsDev::Assert::EndAssert();
        }

        mbShowPoints = lbShowPoints;

        if (mbVisible)
        {
            SetState(mbForceCentred ? KAC_STATE_UPGRADED_TRANSOUT : KAC_STATE_TRANSOUT);
            UpdateDirt();
            mbHiding              = false;
            meCurrentLicenseState = E_LICENSE_UPGRADING_OLD_LICENSE_LEAVING;
        }
        else
        {
            mpStateInterface->PlayAptMovie(KAC_EMPTY_STRING, KI_LICENSE_APT_LEVEL);
            meCurrentLicenseState = E_LICENSE_FIRST_RESOURCE_UNLOADING;
        }

        if (!mbAtTopRank)
        {
            const s32 liRequiredWins = CacheGetUpgradeRequiredWins(mpGuiCache);

            // The console computes mbAtTopRank from the OLD rank (== KI_MAX_RANK - 1) and then
            // increments, so the flag names the rank being moved INTO.
            mbAtTopRank = (miCurrentRank == KI_MAX_RANK - 1);
            ++miCurrentRank;
            miWinsInCurrentRank = liRequiredWins;

            mfRequiredWinsTickUpRate =
                ((liRequiredWins * KF_REQUIRED_WINS_TICK_UP_RATE) <= lfTimeToNextWinIncrement)
                    ? KF_REQUIRED_WINS_TICK_UP_RATE
                    : (lfTimeToNextWinIncrement / static_cast<f32>(liRequiredWins));
            mfTimeToNextWinIncrement = mfRequiredWinsTickUpRate;
        }

        mfTimeToShowNextRank = lfTimeToShowNextRank;
    }

    // ================================================================================
    // @0x82440E38 (cpp:1095) -- show/hide without moving the presentation state on.
    // ================================================================================
    void LicenseComponent::SetVisible(bool lbVisible)
    {
        if (lbVisible == mbVisible)
            return;

        if (lbVisible)
        {
            mPlayerNameTextField.SetText(mPlayerNameTextField.GetText());
            mNextRankTextField.SetText(mNextRankTextField.GetText());

            if (mbFinishedGame || mbElite)
            {
                SetState(KAC_STATE_IDLE);
                mbHiding = false;
            }
            else
            {
                SetRank(miCurrentRank);
            }

            UpdateDirt();

            CGS_ASSERT(mpProfile != 0, "mpProfile");   // cpp:1122

            mbShowingProfilePicture = (mpProfile->GetPlayerLicencePicture() != 0);
            if (!mbShowingProfilePicture)
                StartOutputtingGamerpic();
        }
        else
        {
            SetState(KAC_STATE_INVISIBLE);
            const bool lbWasShowingProfilePicture = mbShowingProfilePicture;
            mbHiding = false;
            if (!lbWasShowingProfilePicture)
                StopOutputtingGamerpic();
        }

        mbVisible = lbVisible;
    }

    // ================================================================================
    // @0x824B31E8 (BrnLicenseComponent.h:366 owns the assert line)
    // ================================================================================
    void LicenseComponent::SetCachePointer(GuiCache* lpGuiCache)
    {
        CGS_ASSERT(lpGuiCache != 0, "NULL != lpGuiCache");   // h:366
        mpGuiCache = lpGuiCache;
    }

    // ================================================================================
    // @0x824B3248 (BrnLicenseComponent.h:384 owns the assert line) -- only rebuild the date
    // field when the profile actually changes.
    // ================================================================================
    void LicenseComponent::SetProfilePointer(BrnProgression::Profile* lpProfile)
    {
        CGS_ASSERT(lpProfile != 0, "NULL != lpProfile");   // h:384

        if (mpProfile != lpProfile)
        {
            mpProfile = lpProfile;

            const CgsSystem::DateAndTime lLicenceDate = mpProfile->GetLicenceIssuedDate();
            CgsLanguage::LanguageManager* lpLanguageManager = mpStateInterface->GetLanguageManager();

            const s32 liYear  = lLicenceDate.GetYear();
            const s32 liMonth = lLicenceDate.GetMonth();
            const s32 liDay   = lLicenceDate.GetDay();

            char lacDateString[112];   // X360 sp+0x60 local (the formatter is capped at 64)
            lpLanguageManager->FormatDateString(lacDateString, liDay, liMonth, liYear, 64);
            mDateIssuedTextField.SetText(lacDateString);
        }
    }

    // ================================================================================
    // @0x8243CB90 (cpp:1198) -- keep the licence picture flowing to the view.
    // ================================================================================
    void LicenseComponent::SendPlayerPictureEvent()
    {
        if (mpProfile != 0 && mbShowingProfilePicture)
        {
            GuiEventNetworkPlayerImageRecord lRecord(mpProfile->GetPlayerLicencePicture(), 1);
            mpStateInterface->GetOutputEventQueue()->AddEvent(&lRecord, KI_CHANNEL_VIEW_STATE,
                                                             static_cast<s32>(sizeof(lRecord)));
        }
    }

    // ================================================================================
    // @0x8241AB48 (cpp:1274)
    // ================================================================================
    void LicenseComponent::SetPercentageComplete(s32 liPercentageComplete)
    {
        const bool lbElite = mbElite;
        miPercentComplete  = liPercentageComplete;

        if (lbElite)
        {
            char lacPercent[128];
            CgsCore::SnPrintf(lacPercent, 128, "%d", liPercentageComplete);
            lacPercent[127] = 0;

            mNextRankTextField.SetLocalisedText(KAC_PERCENTAGE_COMPLETE_STRINGID,
                                                KE_FORMAT_ID_LOOKUP, 1, lacPercent,
                                                KE_FORMAT_PARAM_PERCENT);
        }
    }

    // ================================================================================
    // @0x824B3300 (BrnLicenseComponent.h:420/:421 own the assert lines) -- the "first
    // resource" state group [8, 15) streams tuple slot 1; every other state streams slot 0.
    // ================================================================================
    bool LicenseComponent::EnsureResourcesAreLoaded()
    {
        CGS_ASSERT(mpGuiCache != 0, "NULL != mpGuiCache");   // h:420

        if (meCurrentLicenseState == E_LICENSE_FIRST_RESOURCE_UNLOADED)
        {
            // STREAMED on the console.
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Do not know which resource we need to load at this point! In state "
                          "E_LICENSE_FIRST_RESOURCE_UNLOADED \n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);   // h:421
            CgsDev::Assert::EndAssert();
        }

        const s32 liIndex = (meCurrentLicenseState >= E_LICENSE_FIRST_RESOURCE_UNLOADED &&
                             meCurrentLicenseState < E_LICENSE_UPGRADING_DONE)
                                ? 1
                                : 0;

        return mpGuiCache->EnsureResourceIsLoaded(maLicenseResourcesToLoad[liIndex]);
    }

    // ================================================================================
    // @0x824B33F8 (BrnLicenseComponent.h:448/:449 own the assert lines)
    // ================================================================================
    bool LicenseComponent::EnsureResourcesAreUnloaded()
    {
        CGS_ASSERT(mpGuiCache != 0, "NULL != mpGuiCache");   // h:448
        CGS_ASSERT(meCurrentLicenseState == E_LICENSE_RESOURCES_UNLOADED,
                   "E_LICENSE_RESOURCES_UNLOADED == meCurrentLicenseState");   // h:449

        return mpGuiCache->EnsureResourcesAreUnloaded(maLicenseResourcesToLoad,
                                                      static_cast<u32>(miNumResourcesToLoad));
    }

    // ================================================================================
    // @0x8242D898 (cpp:1160) -- push the card's "dirt" control variable. Elite / finished
    // licences carry no dirt at all (the whole body is gated off).
    //
    // The value is `min(miWinsInCurrentRank / requiredWins, 0.01f)` -- the console computes
    // 0.01f - ratio and fsel's between the ratio and 0.01f on its sign, i.e. it CLAMPS to
    // 0.01f. Reproduced as the console has it.
    // ================================================================================
    void LicenseComponent::UpdateDirt()
    {
        if (mbFinishedGame || mbElite)
            return;

        CGS_ASSERT(miWinsInCurrentRank >= 0, "miWinsInCurrentRank >= 0");   // cpp:1167

        const s32 liRequiredWins = CacheGetRequiredWinsInRank(mpGuiCache, miCurrentRank);

        const f32 lfRatio = static_cast<f32>(miWinsInCurrentRank) / static_cast<f32>(liRequiredWins);
        const f32 lfCeiling = 0.01f;                       // flt_82002138
        const f32 lfDirt = ((lfCeiling - lfRatio) >= 0.0f) ? lfRatio : lfCeiling;

        char lacDirt[128];
        CgsCore::SnPrintf(lacDirt, 128, "%f", lfDirt);
        lacDirt[127] = 0;

        AddOutputAptViewState(KAC_DIRT_CONTROL_VAR, lacDirt, false);
    }

    // ================================================================================
    // @0x8241A4B8 (BrnLicenseComponent.h:343 owns the assert line) -- the range check is
    // UNSIGNED on the console (`cmplwi rank, 6`), so a negative rank trips it too.
    // ================================================================================
    void LicenseComponent::SetRank(s32 liRank)
    {
        if (static_cast<u32>(liRank) >= static_cast<u32>(KI_MAX_RANK + 1))
        {
            // STREAMED on the console.
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Invalid rank supplied (" << liRank << ")";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                lStrStream.GetBuffer(),
                "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gui\\flow\\screen\\components\\BrnLicenseComponent.h",
                343);
            CgsDev::Assert::EndAssert();
        }

        SetState(KAC_STATE_IDLE);
        miCurrentRank = liRank;
        mbHiding      = false;
    }

    // ================================================================================
    // @0x8243CC30 (cpp:1231/:1232) / @0x8243CD00 -- the gamer-picture texture feed.
    // ================================================================================
    void LicenseComponent::StartOutputtingGamerpic()
    {
        CGS_ASSERT(mpProfile != 0, "mpProfile");                                     // cpp:1231
        CGS_ASSERT(mbShowingProfilePicture == false, "mbShowingProfilePicture == false"); // cpp:1232

        PostPlayerTextureEvent(mpStateInterface, E_PLAYERTEXTURE_LICENCE_GAMERPIC);
    }

    void LicenseComponent::StopOutputtingGamerpic()
    {
        PostPlayerTextureEvent(mpStateInterface, E_PLAYERTEXTURE_OFF);
    }
}
