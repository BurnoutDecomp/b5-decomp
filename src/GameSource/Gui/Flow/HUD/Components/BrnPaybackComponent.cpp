// ============================================================================
// b5-decomp/src/GameSource/Gui/Flow/HUD/Components/BrnPaybackComponent.cpp
//
// BrnGui::PaybackComponent -- the in-race "payback available" HUD widget.
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   Construct                  @0x8242E3A8
//   Initialize                 @0x82428DF0
//   Update                     @0x8241FF38
//   UpdateState                @0x8241FE98
//   BeginAwardAnimation        @0x8243E148
//   ShowAvailableInstantly     @0x8241FF40
//   BecomeInvisible            @0x8241FFE8
//   RespondToTransitionComplete@0x8243DEB0
//   SetDisplayedIcon           @0x824116C8
//   TriggerAptAnimation        @0x82411758
//   ChooseRandomAward          @0x824119B0
//
// The X360 inlines the CgsDev Begin/Fire/EndAssert triples; per project convention they are
// reproduced with the house CGS_ASSERT (baked file/line discarded, message text preserved).
// The bound literals (leAwardType < 3, lePaybackType < 3) are the values the asm compares --
// this X360 build's E_AT_COUNT / E_PAYBACK_TYPE_COUNT == 3, a documented cross-TU delta from
// the committed 4-valued enums (see BrnPaybackManager.h) -- so the numeric bound is emitted
// verbatim while the assert string keeps the symbolic name.
// ============================================================================

#include "GameSource/Gui/Flow/HUD/Components/BrnPaybackComponent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // CgsGui::StateInterface::OutputGuiEvent
#include "GameSource/Gui/BrnGuiCache.h"                               // BrnGui::GuiCache (GetOnlinePlayerInfo)
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h" // InGamePlayerStatusData (mPlayerName / meActiveRaceCarIndex)

namespace BrnGui
{
// The embedded help-item instance name (X360 off_82F24984[0], asm-attested). DWARF cpp:54.
static const char* const msacHelpItemInstanceName = "PaybackButton";

// DWARF cpp:25/26. KU_NUM_RANDOM_TO_SHOW is consumed by EnterState (other TU); the delay is
// flt_8204C554 == 1.5 (asm-attested: RespondToTransitionComplete adds it to the current time).
static const u32 KU_NUM_RANDOM_TO_SHOW           = 4;
static const f32 KF_TIME_TO_DELAY_SHOWING_ICON   = 1.5f;

// The apt "TargetIconType" view-state string per award icon (X360 off_82F24978, DWARF cpp:29
// const char*[4]). off_82F24978[0] == "SlashDown" is asm-attested (SetDisplayedIcon @0x82411748);
// the remaining ids carry the EAwardTypes enum names (same convention the committed
// BrnNorthIndicator style-string table uses). Only ids 0..2 are reachable (SetDisplayedIcon
// bounds the index to < 3). FLAG: [1..3] are enum-name reconstructions, not asm-attested rodata.
static const char* const KAPC_AWARD_STRINGS[PaybackComponent::E_AT_COUNT] =
{
    "SlashDown",     // E_AT_SLASHDOWN    (asm-attested)
    "LockDown",      // E_AT_LOCKDOWN     (enum-name reconstruction)
    "TakeoverDown",  // E_AT_TAKEOVERDOWN (enum-name reconstruction)
    "EvilAxis",      // E_AT_EVIL_AXIS    (enum-name reconstruction)
};

// Payback-type -> award-icon map (X360 dword_8204B8A4, DWARF cpp:41 EAwardTypes[4]). Indexed by
// BrnNetwork::EPaybackType in ShowAvailableInstantly (bounded to < 3). FLAG: NO entry is
// asm-attested (the asm only shows the indexed load); reconstructed as the identity mapping
// implied by the parallel EPaybackType / EAwardTypes ordering (e.g. BOOST_LOCK -> LOCKDOWN).
static const PaybackComponent::EAwardTypes KAE_PAYBACKS_TO_AWARD_TYPE[PaybackComponent::E_AT_COUNT] =
{
    PaybackComponent::E_AT_SLASHDOWN,    // E_PAYBACK_TYPE_REVERSE_STEERING
    PaybackComponent::E_AT_LOCKDOWN,     // E_PAYBACK_TYPE_BOOST_LOCK
    PaybackComponent::E_AT_TAKEOVERDOWN, // E_PAYBACK_TYPE_AGGRESSORS_CONTROLS_AFFECTS_VICTIM
    PaybackComponent::E_AT_EVIL_AXIS,    // E_PAYBACK_TYPE_SIX_AXIS_STEERING (unreachable, bound < 3)
};

// FLAG: X360 Construct seeds mRandom from a process-wide construction counter kept in a .data
// word (dword_82F256EC) that it advances by 42 on every PaybackComponent construction, so
// successive payback widgets get distinct icon sequences. The DWARF names no symbol for this
// word; modelled here as a file-scope counter. Only PaybackComponent::Construct references it in
// the recovered call graph.
static u32 suPaybackConstructSeedCounter = 0;

namespace
{
    // FLAG: the fixed OutputGuiEvent record BeginAwardAnimation posts carries an event whose id
    // (370) is un-named in the DecFIGS DWARF and whose single payload byte the binary leaves
    // uninitialised. Modelled as a 1-byte GetEventType()==370 event so OutputGuiEvent reproduces
    // the exact {size=1, type=370, offset=12} / 16-byte record the asm builds.
    struct GuiEventPaybackBeginAward
    {
        u8  muPayload;
        s32 GetEventType() const { return 370; }
    };
}

// @0x8242E3A8
void PaybackComponent::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                                 const char* lpacParentName)
{
    CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);

    // Bring up the embedded help item ("PaybackButton"), parented under this component's name.
    mHelpItem.Construct(msacHelpItemInstanceName, lpStateInterface, GetName());

    // Prime the icon RNG (default-seed buffer fill) then re-seed from the per-construction
    // counter (the big inlined LCG block; de-inlined to Construct() + SetSeed(), as the
    // committed Random embedders do). The X360 sign-extends the 32-bit counter into the seed.
    suPaybackConstructSeedCounter += 42;
    mRandom.Construct();
    mRandom.SetSeed(static_cast<u64>(static_cast<s64>(static_cast<s32>(suPaybackConstructSeedCounter))));

    mePreviouslyShownPayback = E_AT_EVIL_AXIS;   // +0xD8 = 3
    mePaybackComponentState  = E_PCS_INVISIBLE;  // +0xD0 = 0
    mePaybackAnimationState  = E_PCA_INVISIBLE;  // +0xD4 = 0
}

// @0x82428DF0
void PaybackComponent::Initialize(GuiCache* lpGuiCache)
{
    mpGuiCache = lpGuiCache;                                                       // +0x8C
    mHelpItem.SetItem("ACTIVATE", ButtonIconComponent::E_PADBUTTON_BACK,
                      ButtonIconComponent::E_PADBUTTON_INVISIBLE);
    mePaybackComponentState = E_PCS_INVISIBLE;                                     // +0xD0 = 0
    mePaybackAnimationState = E_PCA_INVISIBLE;                                     // +0xD4 = 0
    AddOutputAptViewState("MainFrame", "Invisible", false);
    AddOutputAptViewState("IconFrame", "Blank", false);
}

// @0x8241FF38
void PaybackComponent::Update(f32 lfCurrentSysTime_Seconds)
{
    mfCurrentSysTime_Seconds = lfCurrentSysTime_Seconds;   // +0xC4
    UpdateState();
}

// @0x8241FE98
void PaybackComponent::UpdateState()
{
    if (mePaybackComponentState == E_PCS_PROPERICON
        && mbDelayingIcon
        && mfTimeStampOfMessageEnd < mfCurrentSysTime_Seconds)
    {
        mePaybackComponentState = E_PCS_ANIMATINGLEFT;   // +0xD0 = 3
        SetDisplayedIcon(meActualPayback);
        mePaybackAnimationState = E_PCA_GO_TO_SIDE;      // +0xD4 = 5
        AddOutputAptViewState("MainFrame", "AnimateLeft", false);
        AddOutputAptViewState("IconFrame", "Static", false);
    }
}

// @0x8243E148 -- post the fixed "begin payback award" GUI output event. The two parameters are
// accepted per the DWARF signature but the X360 body does not read them.
void PaybackComponent::BeginAwardAnimation(BrnNetwork::EPaybackType /*lePaybackType*/,
                                           ::EActiveRaceCarIndex /*leVictimRaceCarIndex*/)
{
    GuiEventPaybackBeginAward lEvent;   // payload left uninitialised, matching the binary
    mpStateInterface->OutputGuiEvent(lEvent);
}

// @0x8241FF40
void PaybackComponent::ShowAvailableInstantly(BrnNetwork::EPaybackType lePaybackType,
                                              ::EActiveRaceCarIndex leVictimRaceCarIndex)
{
    CGS_ASSERT(lePaybackType >= BrnNetwork::E_PAYBACK_TYPE_START,
               "lePaybackType >= BrnNetwork::E_PAYBACK_TYPE_START");
    CGS_ASSERT(lePaybackType < 3, "lePaybackType < BrnNetwork::E_PAYBACK_TYPE_COUNT");

    const EAwardTypes leAwardType = KAE_PAYBACKS_TO_AWARD_TYPE[lePaybackType];
    meVictimRaceCarIndex    = leVictimRaceCarIndex;   // +0xE0
    mePaybackComponentState = E_PCS_AVAILABLE;        // +0xD0 = 4
    meActualPayback         = leAwardType;            // +0xDC
    SetDisplayedIcon(leAwardType);
    TriggerAptAnimation(E_PCA_ATSIDE);                // 6
}

// @0x8241FFE8
void PaybackComponent::BecomeInvisible()
{
    mePaybackComponentState = E_PCS_INVISIBLE;   // +0xD0 = 0
    mePaybackAnimationState = E_PCA_INVISIBLE;   // +0xD4 = 0
    AddOutputAptViewState("MainFrame", "Invisible", false);
    AddOutputAptViewState("IconFrame", "Blank", false);
}

// @0x8243DEB0 -- drive the payback FSM one apt-transition step forward.
void PaybackComponent::RespondToTransitionComplete()
{
    switch (mePaybackComponentState)
    {
        case E_PCS_RANDOMICONS:
            if (mbFirstHalfOfRotationAnim)
            {
                mePaybackAnimationState = E_PCA_ROTATE_OUT;   // +0xD4 = 2
                AddOutputAptViewState("MainFrame", "Middle", false);
                AddOutputAptViewState("IconFrame", "RotateOut", false);
                mbFirstHalfOfRotationAnim = false;            // +0xC0 = 0
            }
            else if (muNumRandomLeftToShow)
            {
                const s32 liAward = ChooseRandomAward();
                SetDisplayedIcon(static_cast<EAwardTypes>(liAward));
                mePaybackAnimationState = E_PCA_ROTATE_IN;    // +0xD4 = 1
                AddOutputAptViewState("MainFrame", "Middle", false);
                AddOutputAptViewState("IconFrame", "RotateIn", false);
                mbFirstHalfOfRotationAnim = true;             // +0xC0 = 1
                --muNumRandomLeftToShow;                      // +0xCC
            }
            else
            {
                mePaybackComponentState = E_PCS_PROPERICON;   // +0xD0 = 2
                mbDelayingIcon = false;                       // +0xC1 = 0
                SetDisplayedIcon(meActualPayback);
                mePaybackAnimationState = E_PCA_ROTATE_IN;    // +0xD4 = 1
                AddOutputAptViewState("MainFrame", "Middle", false);
                AddOutputAptViewState("IconFrame", "RotateIn", false);
            }
            break;

        case E_PCS_PROPERICON:
            switch (mePaybackAnimationState)
            {
                case E_PCA_ROTATE_IN:
                    mePaybackAnimationState = E_PCA_BOUNCE;   // +0xD4 = 3
                    AddOutputAptViewState("MainFrame", "Middle", false);
                    AddOutputAptViewState("IconFrame", "Bounce", false);
                    break;
                case E_PCA_BOUNCE:
                    mePaybackAnimationState = E_PCA_WITHBIGTEXT;   // +0xD4 = 4
                    AddOutputAptViewState("ShowHeading", "true", false);
                    AddOutputAptViewState("MainFrame", "Middle", false);
                    AddOutputAptViewState("IconFrame", "Static", false);
                    break;
                case E_PCA_WITHBIGTEXT:
                    mbDelayingIcon = true;                                                  // +0xC1 = 1
                    mfTimeStampOfMessageEnd = mfCurrentSysTime_Seconds + KF_TIME_TO_DELAY_SHOWING_ICON; // +0xC8
                    break;
                default:
                    CGS_ASSERT(false, "false");   // cpp:275
                    break;
            }
            break;

        case E_PCS_ANIMATINGLEFT:
            mePaybackComponentState = E_PCS_AVAILABLE;   // +0xD0 = 4
            SetDisplayedIcon(meActualPayback);
            TriggerAptAnimation(E_PCA_ATSIDE);           // 6
            SendAwardTriggerableEvent();
            break;

        default:
            break;
    }
}

// @0x824116C8
void PaybackComponent::SetDisplayedIcon(EAwardTypes leAwardType)
{
    CGS_ASSERT(leAwardType >= E_AT_START, "leAwardType >= E_AT_START");   // cpp:337
    CGS_ASSERT(leAwardType < 3, "leAwardType < E_AT_COUNT");              // cpp:338 (asm bound == 3)
    AddOutputAptViewState("TargetIconType", KAPC_AWARD_STRINGS[leAwardType], false);
}

// @0x82411758
void PaybackComponent::TriggerAptAnimation(EPaybackComponentAnimations lePaybackComponentAnimation)
{
    const char* lpacMainFrameState = 0;
    const char* lpacIconFrameState = 0;

    mePaybackAnimationState = lePaybackComponentAnimation;   // +0xD4

    switch (lePaybackComponentAnimation)
    {
        case E_PCA_INVISIBLE:
            lpacMainFrameState = "Invisible";
            lpacIconFrameState = "Blank";
            break;
        case E_PCA_ROTATE_IN:
            lpacMainFrameState = "Middle";
            lpacIconFrameState = "RotateIn";
            break;
        case E_PCA_ROTATE_OUT:
            lpacMainFrameState = "Middle";
            lpacIconFrameState = "RotateOut";
            break;
        case E_PCA_BOUNCE:
            lpacMainFrameState = "Middle";
            lpacIconFrameState = "Bounce";
            break;
        case E_PCA_WITHBIGTEXT:
            lpacMainFrameState = "Middle";
            lpacIconFrameState = "Static";
            AddOutputAptViewState("ShowHeading", "true", false);
            break;
        case E_PCA_GO_TO_SIDE:
            lpacMainFrameState = "AnimateLeft";
            lpacIconFrameState = "Static";
            break;
        case E_PCA_ATSIDE:
        {
            CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:407
            lpacMainFrameState = "ShowText";
            lpacIconFrameState = "Static";
            AddOutputAptViewState("PaybackText", "Payback on", false);

            // Publish the victim's online name. The X360 walks the 8-entry online-player table
            // (maPlayerInfo[8] @ GuiCache+0xAC80, 312-byte InGamePlayerStatusData stride), matching
            // each record's meActiveRaceCarIndex (+276) against meVictimRaceCarIndex and reading its
            // mPlayerName (+256). The not-found path passes the raw (const char*)256 the binary
            // computes (v8 == 0, name = v8 + 256).
            const char* lpacPersonName = reinterpret_cast<const char*>(256);
            for (s32 liPlayer = 0; liPlayer < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liPlayer)
            {
                const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpRecord =
                    mpGuiCache->GetOnlinePlayerInfo(liPlayer);
                if (lpRecord->meActiveRaceCarIndex == meVictimRaceCarIndex)
                {
                    lpacPersonName = lpRecord->mPlayerName.macName;
                    break;
                }
            }
            AddOutputAptViewState("PersonName", lpacPersonName, false);

            mHelpItem.SetItem("ACTIVATE", ButtonIconComponent::E_PADBUTTON_BACK,
                              ButtonIconComponent::E_PADBUTTON_INVISIBLE);
            break;
        }
        default:
            CGS_ASSERT(false, "false");                        // cpp:418
            CGS_ASSERT(lpacMainFrameState != 0, "lpMainFrame != NULL");   // cpp:423
            CGS_ASSERT(lpacIconFrameState != 0, "lpIconFrame != NULL");   // cpp:424
            break;
    }

    AddOutputAptViewState("MainFrame", lpacMainFrameState, false);
    AddOutputAptViewState("IconFrame", lpacIconFrameState, false);
}

// @0x824119B0 -- pick a random award icon distinct from the last one shown.
s32 PaybackComponent::ChooseRandomAward()
{
    // X360 inlines a single raw LCG draw on mRandom (high-32 bits of the seed, reduced mod 3)
    // and re-rolls until it differs from mePreviouslyShownPayback. De-inlined to the public RNG
    // draw. FLAG: the divisor is the literal 3 the asm emits (the three randomisable award
    // icons, == SetDisplayedIcon's `< E_AT_COUNT` runtime bound), not the DWARF E_AT_COUNT (4).
    s32 liAward;
    do
    {
        liAward = static_cast<s32>(mRandom.RandomUInt() % 3u);
    }
    while (liAward == mePreviouslyShownPayback);

    mePreviouslyShownPayback = static_cast<EAwardTypes>(liAward);   // +0xD8
    return liAward;
}
}
