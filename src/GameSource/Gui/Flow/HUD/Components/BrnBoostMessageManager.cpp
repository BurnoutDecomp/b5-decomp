#include "GameSource/Gui/Flow/HUD/Components/BrnBoostMessageManager.h"

#include <cstdarg>

#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"       // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // StateInterface + mOutEventQueue
#include "GameShared/GameClasses/Language/CgsLanguageManager.h" // LanguageManager::FormatTextV
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"         // the recovered event payloads
#include "GameSource/Gui/BrnGuiCache.h"                       // GuiCache::GetPlayerActiveRaceCarIndex

// ===================================================================================
// BrnGui::BoostMessageManager -- reconstructed from BURNOUT_X360_ARTIST.XEX.
// DWARF primary file GameSource/Gui/Flow/HUD/Components/BrnBoostMessageManager.{h,cpp}.
//
// Bodied here (23 functions -- the 16 ledger TU functions PLUS the seven whose ledger
// rows were mis-homed to CgsStrStream.h with no committed body anywhere):
//   Construct 0x82420298   Prepare 0x82428E80    RecvEvent 0x824204E8
//   AddMessage 0x8242E7B8  Update 0x8243E198     FindMessageSlot 0x82428F80
//   GetFreeSlot 0x82429088 GetNumActiveSlots 0x82429100
//   UpdateStunts 0x8242ECD0              UpdateVehicleImpacts 0x8242EA48
//   UpdateBarrelRolls 0x8242EC20         UpdateHandBrake 0x8242E900
//   UpdateAir 0x824378C8                 UpdateOncoming 0x8242EDC0
//   UpdateDrift 0x8242EFA8               UpdateSpin 0x8242F198
//   UpdateTailgating 0x8242F2E0          UpdateChecking 0x8242F828
//   UpdateStuntsJumpsAndSmashes 0x8242F4C8
//   UpdateNearMiss 0x8242F958            HandleOnCompletedStunt 0x82411CC8
//   HandleOnInProgressStunt 0x82411C70
//
// The PS3 build's separate UpdateTakdowns / UpdateLandingsCheck / UpdateSignatureStunts /
// UpdateShortcuts / UpdateCrashEscape / UpdateBoosting bodies are folded into the X360
// call graph this file reproduces (takedowns/signature/shortcuts inline in Update, the
// landings check at its tail, crash escape inside UpdateNearMiss) -- see the per-site
// comments.
//
// FLOAT-ARG ABI NOTE: every AddMessage / SetMessage / Refresh callsite below passes the
// time-to-live as the C++ argument in the position the DecFIGS declaration gives it; the
// console's r6/r7 register skips after each float argument fall out of that declaration
// and need no attention at source level.
// ===================================================================================

namespace
{
    // X360 rodata constants (values read from the decrypted image at the cited addresses).
    const f32 KF_ZERO                           = 0.0f;                    // flt_82001CC0
    const f32 KF_MESSAGE_TTL_STANDARD           = 2.0f;                    // flt_82001D9C
    const f32 KF_MESSAGE_TTL_SHORT              = 0.5f;                    // flt_82001DA0
    const f32 KF_MESSAGE_TTL_CHECKING           = 1.0f;                    // flt_82001C98
    const f32 KF_MESSAGE_TTL_STUNT_FAMILY       = 3.0f;                    // flt_8204BEC8
    const f32 KF_MESSAGE_TTL_CLEANLANDING       = 2.5f;                    // flt_82005548
    const f32 KF_MESSAGE_TTL_BARREL_ROLL        = 5.0f;                    // flt_8204C54C
    const f32 KF_MESSAGE_TTL_CRASH_ESCAPE       = 4.0f;                    // flt_82052E64
    const f32 KF_MESSAGE_TTL_NEAR_MISS          = 5.0f;                    // flt_82014A4C
    const f32 KF_AIR_MINIMUM_TIME               = 0.25f;                   // flt_82003F40
    const f32 KF_ONCOMING_DISPLAY_THRESHOLD_M   = 50.0f;                   // flt_820138DC
    const f32 KF_TAILGATING_DISPLAY_THRESHOLD_M = 10.0f;                   // flt_82004A20
    const f32 KF_DEGREES_PER_RADIAN             = 57.295780181884766f;     // flt_8204B608
    // The handbrake/spin "settled" window is +/- FLT_EPSILON (flt_8204B630 upper,
    // flt_82002514 lower): the producer resets its live angle once a trick completes,
    // so an angle back inside epsilon means the turn/spin is over.
    const f32 KF_ANGLE_SETTLE_EPSILON           = 1.1920928955078125e-07f;

    // macMessageSlotNameBase (DWARF h:258) -- the "%s%d"/"%s_%s%d" slot-name base.
    const char macMessageSlotNameBase[] = "Slot";

    // mpacMessageTypeStrings (DWARF h:259) @X360 0x82F249D0 -- one string id per
    // MessageType; the table was dumped verbatim from the decrypted image. These are
    // loc-string IDS resolved through the language manager, not display literals.
    const char* const mpacMessageTypeStrings[BrnGui::BoostMessageManager::E_MESSAGETYPE_MAX] =
    {
        "GOODBOOSTING",              //  0
        "ONCOMING",                  //  1
        "AIR",                       //  2
        "DRIFT",                     //  3
        "FLATSPIN",                  //  4
        "TWOWHEELS",                 //  5
        "TAILGATING",                //  6
        "TRAFFICCHECK",              //  7
        "NEARMISS",                  //  8
        "STUNT",                     //  9
        "SHORTCUT",                  // 10
        "BOOST_SHOWTIME_CAR",        // 11
        "BOOST_SHOWTIME_VAN",        // 12
        "BOOST_SHOWTIME_TRUCK",      // 13
        "BOOST_SHOWTIME_BUS",        // 14
        "BOOST_SHOWTIME_BIGRIG",     // 15
        "BOOST_SHOWTIME_LIMO",       // 16
        "BOOST_SHOWTIME_TAXI",       // 17
        "BOOST_SHOWTIME_TARGETVEHICLE", // 18
        "NCARS",                     // 19
        "OVERHEADSIGN",              // 20
        "TRADINGPAINT",              // 21
        "NUDGE",                     // 22
        "SLAM",                      // 23
        "SHUNT",                     // 24
        "BOOSTSLAM",                 // 25
        "BOOSTSHUNT",                // 26
        "GRINDING",                  // 27
        "RUBBING",                   // 28
        "CRASHESCAPE",               // 29
        "SIGNATURESTUNT",            // 30
        "BARRELROLL",                // 31
        "EBRAKETURN",                // 32
        "HUD_BOOST_TAKEDOWN",        // 33
        "CLEANLANDING",              // 34
        "SUCCESSFULLANDING",         // 35
        "HUD_BOOST_JUMPS",           // 36
        "HUD_BOOST_BILLBOARDS",      // 37 (the billboards tally -- see the enum note)
        "HUD_BOOST_SMASHES",         // 38
    };

    // KA_VEHICLE_SCORE_CATEGORY_TO_MESSAGE_TYPE (DWARF cpp:85) @X360 dword_82F24A6C --
    // VehicleScoreCategory -> the E_MESSAGETYPE_*_HIT family. Read from the image.
    const s32 KA_VEHICLE_SCORE_CATEGORY_TO_MESSAGE_TYPE[8] = { 11, 12, 13, 14, 15, 16, 17, 18 };
}

namespace BrnGui
{

// ===================================================================================
// Construct  @ 0x82420298  (cpp:119/:120 asserts)
//
// Asserts the name and interface, runs the base Construct (interface store + mAptRef
// invalidate), zeroes the slot-array count word, then builds the three slots by
// default-Constructing a temporary and Append-ing it (the console's own shape: the temp
// is stack var_F0, the array receives a 0x44-byte copy per Append). The SPrintf'd
// "Slot<i>" name IS computed each iteration and then discarded -- the shipped body hands
// the slot Construct a NULL name; reproduced verbatim.
// ===================================================================================
void BoostMessageManager::Construct(const char* lacName, CgsGui::StateInterface* lpStateInterface,
                                    const char* /*lpacParentName*/)
{
    CGS_ASSERT(lacName != 0, "Invalid name");                                   // cpp:119
    CGS_ASSERT(lpStateInterface != 0, "Invalid state interface");               // cpp:120 (+ base h:113)

    BrnFlaptComponent::Construct(lpStateInterface);

    mSlots.Clear();                                             // `stw r28, 0x18C(r31)` @0x824203CC

    for (s32 liIndex = 0; liIndex < KI_MAX_BOOSTMESSAGE_SLOTS; ++liIndex)
    {
        char lacSlotName[64];
        CgsCore::SPrintf(lacSlotName, 64, "%s%d", macMessageSlotNameBase, liIndex);
        (void)lacSlotName;   // computed and dropped by the shipped body @0x824203DC..EC

        BoostMessageSlot lTempSlot;
        lTempSlot.Construct(0, lpStateInterface, 0);
        mSlots.Append(lTempSlot);
    }

    // The seed storm @0x8242041C..DC, store for store. NOTE what is NOT here:
    // meCurrentBoostType (+0x0C), mfAirTime/mfPrevAirTime (+0x50/+0x54),
    // miHitVehicleScore (+0xB4) are left UNINITIALISED by the console ctor.
    mbChecking                    = false;
    miCheckingCount               = 0;
    mbNearMiss                    = false;
    mbIsCrashEscape               = false;
    miNearMissCount               = 0;
    mbStuntDone                   = false;
    miStuntChain                  = 0;
    mSignatureStuntId             = K_SIGNATURE_STUNT_INVALID;  // `std r10, 0x28` (-1,-1)
    mShortcutId                   = 0;                          // `std r11, 0x30`
    mfDriftingDist                = KF_ZERO;
    mfPrevDriftDist               = KF_ZERO;
    mfSpinAngle                   = KF_ZERO;
    mfPrevSpinAngle               = KF_ZERO;
    mfOncomingDist                = KF_ZERO;
    mfPrevOncomingDist            = KF_ZERO;
    mfTailDist                    = KF_ZERO;
    mfPrevTailDist                = KF_ZERO;
    mbBarrelRollCompleted         = false;
    miNumberOfCompletedBarrelRolls= 0;
    mfCompletedBarrelRollAngle    = KF_ZERO;
    mbAirSpinInProgress           = false;
    mfInProgressAirSpinAngle      = KF_ZERO;
    mfPrevAirSpinAngle            = KF_ZERO;
    mbHandBreakTurnCompleted      = false;
    mfHandBrakeTurnAngle          = KF_ZERO;
    mfPrevHandBrakeTurnAngle      = KF_ZERO;
    mbCleanLandingComplete        = false;
    mbSuccessfulLandingComplete   = false;
    mbImpactEventRecorded         = false;
    meImpactType                  = 0;
    mbPlayerDoesTakedown          = false;
    mbJumpMessagePending          = false;
    miJumpCurrentCount            = -1;
    miJumpTotalCount              = -1;
    mbSmashesMessagePending       = false;
    miSmashesCurrentCount         = -1;
    miSmashesTotalCount           = -1;
    mbBillBoardsMessagePending    = false;
    miBillBoardsCurrentCount      = -1;
    miBillBoardsTotalCount        = -1;
    meHitVehicleCategory          = 8;                          // == E_VEHICLESCORE_COUNT ("none")
    miHitVehicleCount             = -1;
    mbHitOverheadSign             = false;
}

// ===================================================================================
// Prepare  @ 0x82428E80
//
// The base Prepare body is INLINED on the console (FindComponent + the two-word
// mAptRef copy + the mpMovieClipInst assert + ResetTimeline); calling the reconstructed
// out-of-line base reproduces it exactly. Each slot then prepares under the composite
// "<manager>_<Slot><i>" key ("%s_%s%d").
// ===================================================================================
void BoostMessageManager::Prepare(const char* lacName, const BrnFlapt::FileRef& lFile)
{
    CGS_ASSERT(lacName != 0, "lacName != NULL");

    BrnFlaptComponent::Prepare(lacName, lFile, 0);

    for (s32 liIndex = 0; liIndex < KI_MAX_BOOSTMESSAGE_SLOTS; ++liIndex)
    {
        char lacSlotName[128];
        CgsCore::SPrintf(lacSlotName, 128, "%s_%s%d", lacName, macMessageSlotNameBase, liIndex);
        mSlots.GetItem(liIndex).Prepare(lacSlotName, lFile);        // @0x82428F30..74
    }
}

// ===================================================================================
// FindMessageSlot  @ 0x82428F80  -- the live slot showing leMessageType, or NULL.
// ===================================================================================
BoostMessageSlot* BoostMessageManager::FindMessageSlot(MessageType leMessageType)
{
    CGS_ASSERT(leMessageType >= 0 && leMessageType < E_MESSAGETYPE_MAX,
               "Invalid message type");                                     // cpp:1780

    for (u32 luIndex = 0; luIndex < KI_MAX_BOOSTMESSAGE_SLOTS; ++luIndex)
    {
        BoostMessageSlot& lrSlot = mSlots.GetItem(luIndex);
        if (!lrSlot.IsInUse())
            continue;
        if (lrSlot.GetMessageId() == leMessageType)
            return &lrSlot;
    }
    return 0;
}

// ===================================================================================
// GetFreeSlot  @ 0x82429088  -- an idle AND settled slot, or NULL.
// ===================================================================================
BoostMessageSlot* BoostMessageManager::GetFreeSlot()
{
    for (u32 luIndex = 0; luIndex < KI_MAX_BOOSTMESSAGE_SLOTS; ++luIndex)
    {
        BoostMessageSlot& lrSlot = mSlots.GetItem(luIndex);
        if (!lrSlot.IsInUse() && !lrSlot.IsInTransition())
            return &lrSlot;
    }
    return 0;
}

// ===================================================================================
// GetNumActiveSlots  @ 0x82429100
// ===================================================================================
s32 BoostMessageManager::GetNumActiveSlots() const
{
    s32 liCount = 0;
    for (u32 luIndex = 0; luIndex < KI_MAX_BOOSTMESSAGE_SLOTS; ++luIndex)
    {
        if (mSlots.GetItem(luIndex).IsInUse())
            ++liCount;
    }
    return liCount;
}

// ===================================================================================
// AddMessage  @ 0x8242E7B8  (cpp:477 assert)
//
// Stage a message onto the stack: find a free slot, refuse when any transitioning slot
// still sits BELOW the top row (its shuffle would race the new entry), shuffle every
// live slot up one row, then hand the text to the free slot with the CURRENT boost type
// as the tint. Returns whether the message went live (callers latch their pending flags
// on this).
// ===================================================================================
bool BoostMessageManager::AddMessage(MessageType leMessageType, const char* lpcText,
                                     f32 lfTimeToLive, s32 liBoostAmount)
{
    CGS_ASSERT(lpcText != 0, "lpMessageText");                                  // cpp:477

    BoostMessageSlot* lpMessageSlot = GetFreeSlot();
    if (lpMessageSlot == 0)
        return false;

    bool lbCanShuffle = true;
    for (u32 luIndex = 0; luIndex < KI_MAX_BOOSTMESSAGE_SLOTS; ++luIndex)
    {
        BoostMessageSlot& lrSlot = mSlots.GetItem(luIndex);
        if (!lrSlot.IsInUse())
            continue;
        if (!lrSlot.IsInTransition())
            continue;
        if (lrSlot.GetSlotPosition() < 3)
        {
            lbCanShuffle = false;                       // a mid-shuffle slot blocks the add
            break;
        }
    }
    if (!lbCanShuffle)
        return false;

    for (u32 luIndex = 0; luIndex < KI_MAX_BOOSTMESSAGE_SLOTS; ++luIndex)
    {
        BoostMessageSlot& lrSlot = mSlots.GetItem(luIndex);
        if (lrSlot.IsInUse())
            lrSlot.ShuffleUp();
    }

    lpMessageSlot->SetMessage(lpcText, leMessageType, lfTimeToLive,
                              static_cast<s32>(meCurrentBoostType), liBoostAmount);
    return true;
}

// ===================================================================================
// RecvEvent  @ 0x824204E8  -- the event latch. Switch ids are X360 wire ids.
// ===================================================================================
void BoostMessageManager::RecvEvent(const CgsModule::Event* lpEvent, s32 liEventType,
                                    GuiCache* lpGuiCache)
{
    CGS_ASSERT(lpEvent != 0, "Invalid event pointer");                          // cpp:243

    switch (liEventType)
    {
        case 206:   // GuiEventBoostInfo -- the boost-type word drives the slot/item tint
        {
            const GuiEventBoostInfo* lpBoostInfo = reinterpret_cast<const GuiEventBoostInfo*>(lpEvent);
            meCurrentBoostType = static_cast<s32>(lpBoostInfo->meBoostType);    // +0x0C @0x82420C74
            break;
        }

        case 218:   // GuiEventBoostBarStuntInfo -- {current,total,type} tally latch
        {
            const GuiEventBoostBarStuntInfo* lpStuntInfo =
                reinterpret_cast<const GuiEventBoostBarStuntInfo*>(lpEvent);
            CGS_ASSERT(lpStuntInfo != 0, "lpGuiEventBoostBarStuntInfo != NULL"); // cpp:292
            if (lpStuntInfo->meStuntType == 0)
            {
                mbJumpMessagePending  = true;                                   // @0x82420A04
                miJumpCurrentCount    = lpStuntInfo->miCurrentCount;
                miJumpTotalCount      = lpStuntInfo->miTotalCount;
            }
            if (lpStuntInfo->meStuntType == 1)
            {
                mbSmashesMessagePending = true;                                 // @0x82420A24
                miSmashesCurrentCount   = lpStuntInfo->miCurrentCount;
                miSmashesTotalCount     = lpStuntInfo->miTotalCount;
            }
            if (lpStuntInfo->meStuntType == 2)
            {
                mbBillBoardsMessagePending = true;                              // @0x82420A44
                miBillBoardsCurrentCount   = lpStuntInfo->miCurrentCount;
                miBillBoardsTotalCount     = lpStuntInfo->miTotalCount;
            }
            break;
        }

        case 364:   // GuiSoftTakedownEvent -- "the local player performed a takedown"
        {
            const GuiSoftTakedownEvent* lpSoftTakedownEvent =
                reinterpret_cast<const GuiSoftTakedownEvent*>(lpEvent);
            CGS_ASSERT(lpSoftTakedownEvent != 0, "lpGuiSoftTakedownEvent != NULL"); // cpp:336
            CGS_ASSERT(lpGuiCache != 0, "lpGuiCache != NULL");                      // cpp:337
            if (lpSoftTakedownEvent->meAggressorIndex ==
                static_cast<EActiveRaceCarIndex>(lpGuiCache->GetPlayerActiveRaceCarIndex()))
            {
                mbPlayerDoesTakedown = true;                                    // @0x82420B2C
            }
            break;
        }

        case 365:   // GuiImpactEvent -- impact kind + "aggressor is the local player"
        {
            const GuiImpactEvent* lpGuiImpactEvent = reinterpret_cast<const GuiImpactEvent*>(lpEvent);
            CGS_ASSERT(lpGuiImpactEvent != 0, "lpGuiImpactEvent != NULL");      // cpp:321
            CGS_ASSERT(lpGuiCache != 0, "lpGuiCache != NULL");                  // cpp:322
            if (lpGuiImpactEvent->meAggressorActiveRaceCarIndex ==
                static_cast<EActiveRaceCarIndex>(lpGuiCache->GetPlayerActiveRaceCarIndex()))
            {
                mbImpactEventRecorded = true;                                   // @0x82420ABC
                meImpactType          = static_cast<s32>(lpGuiImpactEvent->meImpactType);
            }
            break;
        }

        case 367:   // GuiStuntEvent -- stunt chain completed this frame
        {
            const GuiStuntEvent* lpStuntEvent = reinterpret_cast<const GuiStuntEvent*>(lpEvent);
            miStuntChain = lpStuntEvent->miChainCount;                          // @0x824208BC
            mbStuntDone  = true;
            break;
        }

        case 368:   // GuiSignatureStuntEvent -- park the id until Update can post it
        {
            const GuiSignatureStuntEvent* lpSignatureEvent =
                reinterpret_cast<const GuiSignatureStuntEvent*>(lpEvent);
            // Console guard is the FULL qword test (`ld r11, 0x28(r25); cmpdi cr6,
            // r11, -1` @0x824208D4) -- the pseudocode's dword-only rendering was a
            // Hex-Rays artifact. The compare below matches the asm exactly.
            CGS_ASSERT(mSignatureStuntId == K_SIGNATURE_STUNT_INVALID,
                       "Triggering a new Signature stunt whilst one already waiting to"
                       " appear. Will miss one of these messages unless we implement a queue."); // cpp:258
            mSignatureStuntId = lpSignatureEvent->mId;                          // @0x82420918
            break;
        }

        case 382:   // GuiOffenceShortcutEvent -- park the shortcut id
        {
            const GuiOffenceShortcutEvent* lpShortcutEvent =
                reinterpret_cast<const GuiOffenceShortcutEvent*>(lpEvent);
            mShortcutId = lpShortcutEvent->mShortcutId;                         // @0x82420928
            break;
        }

        case 383:   // GuiTrafficCheckEvent
        {
            const GuiTrafficCheckEvent* lpCheckingEvent =
                reinterpret_cast<const GuiTrafficCheckEvent*>(lpEvent);
            miCheckingCount = lpCheckingEvent->miCount;                        // @0x82420BD8
            mbChecking      = true;
            break;
        }

        case 384:   // GuiNearMissEvent -- count latch + the crash-escape discriminator
        {
            const GuiNearMissEvent* lpNearMissEvent = reinterpret_cast<const GuiNearMissEvent*>(lpEvent);
            if (lpNearMissEvent->meNearMissType == 2 || lpNearMissEvent->meNearMissType == 3)
            {
                mbNearMiss      = false;                                        // @0x82420C04
                mbIsCrashEscape = true;
            }
            else
            {
                mbIsCrashEscape  = false;
                mbNearMiss       = true;
                miNearMissCount  = lpNearMissEvent->miCount;
            }
            break;
        }

        case 385:   // GuiDriftingEvent
        {
            const GuiDriftingEvent* lpDriftEvent = reinterpret_cast<const GuiDriftingEvent*>(lpEvent);
            CGS_ASSERT(lpDriftEvent != 0, "lpDriftEvent");                      // cpp:273
            mfDriftingDist = lpDriftEvent->mfDistance;                          // @0x8242095C
            break;
        }

        case 386:   // GuiSpinningEvent
        {
            const GuiSpinningEvent* lpSpinEvent = reinterpret_cast<const GuiSpinningEvent*>(lpEvent);
            CGS_ASSERT(lpSpinEvent != 0, "lpSpinEvent");                        // cpp:282
            CGS_ASSERT(lpSpinEvent->mfSpinAngle >= 0.0f,
                       "lpSpinEvent->mfSpinAngle >= 0.0f");                     // cpp:284
            mfSpinAngle = lpSpinEvent->mfSpinAngle;                             // @0x824209C0
            break;
        }

        case 387:   // GuiInAirEvent -- the CURRENT jump's air time
        {
            const GuiInAirEvent* lpAirEvent = reinterpret_cast<const GuiInAirEvent*>(lpEvent);
            mfAirTime = lpAirEvent->mfCurrentJumpAirTime;                       // @0x82420BB8
            break;
        }

        case 388:   // GuiOncomingEvent
        {
            const GuiOncomingEvent* lpOncomingEvent = reinterpret_cast<const GuiOncomingEvent*>(lpEvent);
            mfOncomingDist = lpOncomingEvent->mfDistance;                       // @0x82420B38
            break;
        }

        case 389:   // GuiTailgatingEvent
        {
            const GuiTailgatingEvent* lpTailgatingEvent =
                reinterpret_cast<const GuiTailgatingEvent*>(lpEvent);
            mfTailDist = lpTailgatingEvent->mfDistance;                         // @0x82420BC8
            break;
        }

        case 390:   // GuiCompletedStuntEvent
        {
            const GuiCompletedStuntEvent* lpCompletedStuntEvent =
                reinterpret_cast<const GuiCompletedStuntEvent*>(lpEvent);
            CGS_ASSERT(lpCompletedStuntEvent != 0, "lpGuiCompletedStuntEvent != NULL"); // cpp:366
            HandleOnCompletedStunt(lpCompletedStuntEvent);
            break;
        }

        case 391:   // GuiInProgressStuntEvent
        {
            const GuiInProgressStuntEvent* lpInProgressStuntEvent =
                reinterpret_cast<const GuiInProgressStuntEvent*>(lpEvent);
            CGS_ASSERT(lpInProgressStuntEvent != 0, "lpGuiInProgressStuntEvent != NULL"); // cpp:357
            HandleOnInProgressStunt(lpInProgressStuntEvent);
            break;
        }

        case 394:   // GuiHitVehicleEvent -- showtime hit category + earned score
        {
            const GuiHitVehicleEvent* lpHitEvent = reinterpret_cast<const GuiHitVehicleEvent*>(lpEvent);
            meHitVehicleCategory = static_cast<s32>(lpHitEvent->meVehicleScoreCategory); // @0x82420C34
            miHitVehicleScore    = lpHitEvent->miVehicleBaseScore + lpHitEvent->miVehicleChainBonus;
            break;
        }

        case 400:   // GuiHUDMessageSignSmashed -- overhead sign down
        {
            mbHitOverheadSign = true;                                           // @0x82420C64
            break;
        }

        case 401:   // GuiHUDMessageCrushCombo -- the showtime consumer reads the single
                    // word as the vehicles-hit COUNT for this showtime run
        {
            const GuiHUDMessageCrushCombo* lpCrushCombo =
                reinterpret_cast<const GuiHUDMessageCrushCombo*>(lpEvent);
            miHitVehicleCount = lpCrushCombo->miCrushComboCount;                // @0x82420C54
            break;
        }

        default:
            break;
    }
}

// ===================================================================================
// HandleOnInProgressStunt  @ 0x82411C70 -- mask bits off GuiInProgressStuntEvent.
//   bit2 (air spin): raise the in-progress latch, convert radians -> degrees.
//   bit1 (handbrake): raise the latch, store the live degrees directly.
// ===================================================================================
void BoostMessageManager::HandleOnInProgressStunt(const GuiInProgressStuntEvent* lpEvent)
{
    const u32 luMask = lpEvent->muStuntActionInProgress;
    if (luMask == 0)
        return;

    if ((luMask & 4) != 0)
    {
        mbHandBreakTurnCompleted   = true;                                       // @0x82411C8C
        mfHandBrakeTurnAngle       = lpEvent->mfInProgressHandbreakTurnAngle;
    }
    if ((luMask & 2) != 0)
    {
        mbAirSpinInProgress        = true;                                       // @0x82411CA8
        mfInProgressAirSpinAngle   = lpEvent->mfInProgressAirSpinAngle * KF_DEGREES_PER_RADIAN;
    }
}

// ===================================================================================
// HandleOnCompletedStunt  @ 0x82411CC8 -- mask bits off GuiCompletedStuntEvent.
//   bit0 (barrel roll): latch the roll count + completed angle.
//   bit3: clean landing. bit4: successful landing = NOT already clean-flagged.
// ===================================================================================
void BoostMessageManager::HandleOnCompletedStunt(const GuiCompletedStuntEvent* lpEvent)
{
    const u32 luMask = lpEvent->muStuntActionComplete;
    if (luMask == 0)
        return;

    if ((luMask & 1) != 0)
    {
        mbBarrelRollCompleted          = true;                                   // @0x82411CE4
        miNumberOfCompletedBarrelRolls = lpEvent->miCompletedBarrelRolls;
        mfCompletedBarrelRollAngle     = lpEvent->mfCompletedBarrelRollAngle;
    }
    if ((luMask & 8) != 0)
    {
        mbCleanLandingComplete = true;                                           // @0x82411D08
    }
    if ((luMask & 16) != 0)
    {
        mbSuccessfulLandingComplete = !mbCleanLandingComplete;                   // @0x82411D1C
    }
}

// ===================================================================================
// UpdateStunts  @ 0x8242ECD0 -- refresh or post the STUNT message for the finished chain.
// ===================================================================================
void BoostMessageManager::UpdateStunts()
{
    if (!mbStuntDone)
        return;

    BoostMessageSlot* lpSlot = FindMessageSlot(E_MESSAGETYPE_STUNT);
    if (lpSlot != 0)
    {
        if (miStuntChain > 1)
        {
            char lacChain[128];                                                 // "%s_X"
            CgsCore::SPrintf(lacChain, 128, "%s_X", mpacMessageTypeStrings[E_MESSAGETYPE_STUNT]);
            lpSlot->SetMessageText(lacChain, miStuntChain);                     // @0x8242ED50
        }
        else
        {
            lpSlot->SetMessageText(mpacMessageTypeStrings[E_MESSAGETYPE_STUNT], -1);
        }
        lpSlot->Refresh(KF_MESSAGE_TTL_STUNT_FAMILY, true);                     // @0x8242ED68
        mbStuntDone = false;
    }
    else
    {
        if (AddMessage(E_MESSAGETYPE_STUNT, mpacMessageTypeStrings[E_MESSAGETYPE_STUNT],
                       KF_MESSAGE_TTL_STUNT_FAMILY, -1))                        // @0x8242ED90
        {
            mbStuntDone = false;
        }
    }
}

// ===================================================================================
// UpdateVehicleImpacts  @ 0x8242EA48 -- post the impact-kind message, then ALWAYS drop
// the recorded latch. NOTE the console asymmetry reproduced in case 6: the pre-check
// asks for BARREL_ROLL (31) while the posted message is BOOST_SHUNT (26).
// ===================================================================================
void BoostMessageManager::UpdateVehicleImpacts()
{
    if (!mbImpactEventRecorded)
        return;

    switch (meImpactType)
    {
        case 1:     // E_IMPACT_TRADING_PAINT
            if (FindMessageSlot(E_MESSAGETYPE_TRADING_PAINT) == 0)
                AddMessage(E_MESSAGETYPE_TRADING_PAINT,
                           mpacMessageTypeStrings[E_MESSAGETYPE_TRADING_PAINT],
                           KF_MESSAGE_TTL_STANDARD, -1);
            break;
        case 2:     // E_IMPACT_NUDGE
            if (FindMessageSlot(E_MESSAGETYPE_NUDGE) == 0)
                AddMessage(E_MESSAGETYPE_NUDGE,
                           mpacMessageTypeStrings[E_MESSAGETYPE_NUDGE],
                           KF_MESSAGE_TTL_STANDARD, -1);
            break;
        case 3:     // E_IMPACT_SLAM
            if (FindMessageSlot(E_MESSAGETYPE_SLAM) == 0)
                AddMessage(E_MESSAGETYPE_SLAM,
                           mpacMessageTypeStrings[E_MESSAGETYPE_SLAM],
                           KF_MESSAGE_TTL_STANDARD, -1);
            break;
        case 4:     // E_IMPACT_SHUNT
            if (FindMessageSlot(E_MESSAGETYPE_SHUNT) == 0)
                AddMessage(E_MESSAGETYPE_SHUNT,
                           mpacMessageTypeStrings[E_MESSAGETYPE_SHUNT],
                           KF_MESSAGE_TTL_STANDARD, -1);
            break;
        case 5:     // E_IMPACT_BOOST_SLAM
            if (FindMessageSlot(E_MESSAGETYPE_BOOST_SLAM) == 0)
                AddMessage(E_MESSAGETYPE_BOOST_SLAM,
                           mpacMessageTypeStrings[E_MESSAGETYPE_BOOST_SLAM],
                           KF_MESSAGE_TTL_STANDARD, -1);
            break;
        case 6:     // E_IMPACT_BOOST_SHUNT -- pre-checks 31, posts 26 (console truth)
            if (FindMessageSlot(E_MESSAGETYPE_BARREL_ROLL) == 0)
                AddMessage(E_MESSAGETYPE_BOOST_SHUNT,
                           mpacMessageTypeStrings[E_MESSAGETYPE_BOOST_SHUNT],
                           KF_MESSAGE_TTL_STANDARD, -1);
            break;
        case 7:     // E_IMPACT_GRINDING
            if (FindMessageSlot(E_MESSAGETYPE_GRINDING) == 0)
                AddMessage(E_MESSAGETYPE_GRINDING,
                           mpacMessageTypeStrings[E_MESSAGETYPE_GRINDING],
                           KF_MESSAGE_TTL_STANDARD, -1);
            break;
        case 8:     // E_IMPACT_RUBBING
            if (FindMessageSlot(E_MESSAGETYPE_RUBBING) == 0)
                AddMessage(E_MESSAGETYPE_RUBBING,
                           mpacMessageTypeStrings[E_MESSAGETYPE_RUBBING],
                           KF_MESSAGE_TTL_STANDARD, -1);
            break;
        default:
            break;
    }

    mbImpactEventRecorded = false;                                              // @0x8242EC00
    meImpactType          = 0;
}

// ===================================================================================
// UpdateBarrelRolls  @ 0x8242EC20 -- post BARRELROLL (or "_X" with the count), reset.
// ===================================================================================
void BoostMessageManager::UpdateBarrelRolls()
{
    if (mbBarrelRollCompleted)
    {
        if (miNumberOfCompletedBarrelRolls > 1)
        {
            char lacRolls[128];                                                 // "%s_X"
            CgsCore::SPrintf(lacRolls, 128, "%s_X",
                             mpacMessageTypeStrings[E_MESSAGETYPE_BARREL_ROLL]);
            AddMessage(E_MESSAGETYPE_BARREL_ROLL, lacRolls,
                       KF_MESSAGE_TTL_BARREL_ROLL, miNumberOfCompletedBarrelRolls);
        }
        else
        {
            AddMessage(E_MESSAGETYPE_BARREL_ROLL,
                       mpacMessageTypeStrings[E_MESSAGETYPE_BARREL_ROLL],
                       KF_MESSAGE_TTL_BARREL_ROLL, -1);
        }
    }

    mfCompletedBarrelRollAngle     = KF_ZERO;                                    // @0x8242ECAC
    mbBarrelRollCompleted          = false;
    miNumberOfCompletedBarrelRolls = 0;
}

// ===================================================================================
// UpdateHandBrake  @ 0x8242E900 -- drive the live "EBRAKETURN <degrees>" counter while
// the turn runs; when the producer's angle settles back into +/- epsilon the turn is
// over and BOTH angles silently reset (no message). Mirrors UpdateSpin exactly.
// ===================================================================================
void BoostMessageManager::UpdateHandBrake()
{
    const bool lbTurnOver =
        (mfHandBrakeTurnAngle <= KF_ANGLE_SETTLE_EPSILON) &&
        (mfHandBrakeTurnAngle >= -KF_ANGLE_SETTLE_EPSILON);

    const f32 lfReset = KF_ZERO;                                                 // flt_82001CC0
    if (lbTurnOver)                                                              // @0x8242EA34
    {
        mfPrevHandBrakeTurnAngle = lfReset;
        mfHandBrakeTurnAngle     = lfReset;
        return;
    }

    BoostMessageSlot* lpSlot = FindMessageSlot(E_MESSAGETYPE_HANDBRAKE_TURN);
    if (lpSlot != 0)
    {
        if (mfHandBrakeTurnAngle > mfPrevHandBrakeTurnAngle)
        {
            // Still rotating: rewrite the counter text (truncated degrees) and extend.
            const int liDegrees = static_cast<int>(mfHandBrakeTurnAngle);       // fctiwz @0x8242E98C
            lpSlot->SetMessageText(mpacMessageTypeStrings[E_MESSAGETYPE_HANDBRAKE_TURN],
                                   liDegrees);
            lpSlot->Refresh(KF_MESSAGE_TTL_STANDARD, false);
            mfPrevHandBrakeTurnAngle = mfHandBrakeTurnAngle;
            mfHandBrakeTurnAngle     = lfReset;
        }
        else
        {
            lpSlot->Refresh(KF_MESSAGE_TTL_SHORT, true);                        // @0x8242E9D8
            mfPrevHandBrakeTurnAngle = mfHandBrakeTurnAngle;
            mfHandBrakeTurnAngle     = lfReset;
        }
    }
    else
    {
        if (AddMessage(E_MESSAGETYPE_HANDBRAKE_TURN,
                       mpacMessageTypeStrings[E_MESSAGETYPE_HANDBRAKE_TURN],
                       KF_MESSAGE_TTL_SHORT, -1))                               // @0x8242EA0C
        {
            mfPrevHandBrakeTurnAngle = mfHandBrakeTurnAngle;
        }
        mfHandBrakeTurnAngle = lfReset;
    }
}

// ===================================================================================
// UpdateSpin  @ 0x8242F198 -- the FLATSPIN twin of UpdateHandBrake (same epsilons, same
// arms) over the air-spin angle pair.
// ===================================================================================
void BoostMessageManager::UpdateSpin()
{
    const bool lbSpinOver =
        (mfInProgressAirSpinAngle <= KF_ANGLE_SETTLE_EPSILON) &&
        (mfInProgressAirSpinAngle >= -KF_ANGLE_SETTLE_EPSILON);

    const f32 lfReset = KF_ZERO;
    if (lbSpinOver)                                                              // @0x8242F2CC
    {
        mfPrevAirSpinAngle   = lfReset;
        mfInProgressAirSpinAngle = lfReset;
        return;
    }

    BoostMessageSlot* lpSlot = FindMessageSlot(E_MESSAGETYPE_SPIN);
    if (lpSlot != 0)
    {
        if (mfInProgressAirSpinAngle > mfPrevAirSpinAngle)
        {
            const int liDegrees = static_cast<int>(mfInProgressAirSpinAngle);   // fctiwz @0x8242F224
            lpSlot->SetMessageText(mpacMessageTypeStrings[E_MESSAGETYPE_SPIN], liDegrees);
            lpSlot->Refresh(KF_MESSAGE_TTL_STANDARD, false);
            mfPrevAirSpinAngle       = mfInProgressAirSpinAngle;
            mfInProgressAirSpinAngle = lfReset;
        }
        else
        {
            lpSlot->Refresh(KF_MESSAGE_TTL_SHORT, true);                        // @0x8242F270
            mfPrevAirSpinAngle       = mfInProgressAirSpinAngle;
            mfInProgressAirSpinAngle = lfReset;
        }
    }
    else
    {
        if (AddMessage(E_MESSAGETYPE_SPIN, mpacMessageTypeStrings[E_MESSAGETYPE_SPIN],
                       KF_MESSAGE_TTL_SHORT, -1))                               // @0x8242F2A4
        {
            mfPrevAirSpinAngle       = mfInProgressAirSpinAngle;
        }
        mfInProgressAirSpinAngle = lfReset;
    }
}

// ===================================================================================
// UpdateOncoming  @ 0x8242EDC0 -- the distance-trio template, ONCOMING flavour:
//   dist <= 0                        -> reset both, done
//   no slot                          -> post at SHORT ttl; latch prev on success
//   prev <= 0 (first contact frame)  -> refresh(true), latch prev = current
//   current <= threshold             -> refresh(false), reset current only
//   growing past the threshold       -> rebuild "<ONCOMING> <metres>" text, refresh at
//                                       the function-static STANDARD floor, latch+reset
// The console lazily initialises a per-flavour static TTL pair (@flt_82FB2DE0/dword_82FB2DE4,
// guard bit 0) to KF_MESSAGE_TTL_STANDARD on first call -- modelled as a C++ local static.
// ===================================================================================
void BoostMessageManager::UpdateOncoming()
{
    static const f32 sfUpdatedTextRefreshTtl = KF_MESSAGE_TTL_STANDARD;   // flt_82FB2DE0

    const f32 lfReset = KF_ZERO;
    if (mfOncomingDist <= lfReset)                                               // @0x8242EF94
    {
        mfPrevOncomingDist = lfReset;
        mfOncomingDist     = lfReset;
        return;
    }

    BoostMessageSlot* lpSlot = FindMessageSlot(E_MESSAGETYPE_ONCOMING);
    if (lpSlot == 0)
    {
        if (AddMessage(E_MESSAGETYPE_ONCOMING, mpacMessageTypeStrings[E_MESSAGETYPE_ONCOMING],
                       KF_MESSAGE_TTL_SHORT, -1))                               // @0x8242EF6C
        {
            mfPrevOncomingDist = mfOncomingDist;
        }
        mfOncomingDist = lfReset;
        return;
    }

    if (mfPrevOncomingDist <= lfReset)                                           // @0x8242EF28
    {
        lpSlot->Refresh(KF_MESSAGE_TTL_SHORT, true);
        mfPrevOncomingDist = mfOncomingDist;
        mfOncomingDist     = lfReset;
        return;
    }

    if (mfOncomingDist > KF_ONCOMING_DISPLAY_THRESHOLD_M)                        // @0x8242EE58
    {
        // "<ONCOMING> GENERAL_SEPARATOR <distance>m" via the positional formatter; the
        // number renders through E_FORMAT_SMALL_DISTANCE (17).
        CgsLanguage::LanguageManager* lpLanguageManager = mpStateInterface->GetLanguageManager();
        char lacDistance[32];
        const int liDistance = static_cast<int>(mfOncomingDist);                // fctiwz @0x8242EE68
        CgsCore::SPrintf(lacDistance, 32, "%d", liDistance);
        char lacComposed[256];
        lpLanguageManager->FormatTextV(lacComposed, 256, "GENERAL_SEPARATOR",
                                       CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP, 2,
                                       mpacMessageTypeStrings[E_MESSAGETYPE_ONCOMING],
                                       CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
                                       lacDistance,
                                       CgsLanguage::LanguageManager::E_FORMAT_SMALL_DISTANCE);
        lpSlot->SetMessageText(lacComposed, -1);                                // @0x8242EED8
        lpSlot->Refresh(sfUpdatedTextRefreshTtl, false);
        mfPrevOncomingDist = mfOncomingDist;
        mfOncomingDist     = lfReset;
    }
    else                                                                         // @0x8242EF04
    {
        lpSlot->Refresh(KF_MESSAGE_TTL_SHORT, false);
        mfOncomingDist = lfReset;
    }
}

// ===================================================================================
// UpdateDrift  @ 0x8242EFA8 -- the distance trio, DRIFT flavour. Same template; the
// mid arms differ from ONCOMING: cur<=prev ALSO takes the refresh(true)+latch arm, and
// the threshold gate sits under cur>prev.
// ===================================================================================
void BoostMessageManager::UpdateDrift()
{
    static const f32 sfUpdatedTextRefreshTtl = KF_MESSAGE_TTL_STANDARD;   // flt_82FB2DE8

    const f32 lfReset = KF_ZERO;
    if (mfDriftingDist <= lfReset)                                               // @0x8242F184
    {
        mfPrevDriftDist = lfReset;
        mfDriftingDist  = lfReset;
        return;
    }

    BoostMessageSlot* lpSlot = FindMessageSlot(E_MESSAGETYPE_DRIFT);
    if (lpSlot == 0)
    {
        if (AddMessage(E_MESSAGETYPE_DRIFT, mpacMessageTypeStrings[E_MESSAGETYPE_DRIFT],
                       KF_MESSAGE_TTL_SHORT, -1))                               // @0x8242F15C
        {
            mfPrevDriftDist = mfDriftingDist;
        }
        mfDriftingDist = lfReset;
        return;
    }

    if (mfPrevDriftDist <= lfReset || mfDriftingDist <= mfPrevDriftDist)         // @0x8242F118
    {
        lpSlot->Refresh(KF_MESSAGE_TTL_SHORT, true);
        mfPrevDriftDist = mfDriftingDist;
        mfDriftingDist  = lfReset;
        return;
    }

    if (mfDriftingDist > KF_ONCOMING_DISPLAY_THRESHOLD_M)                        // same rodata
    {
        CgsLanguage::LanguageManager* lpLanguageManager = mpStateInterface->GetLanguageManager();
        char lacDistance[32];
        const int liDistance = static_cast<int>(mfDriftingDist);                // fctiwz @0x8242F058
        CgsCore::SPrintf(lacDistance, 32, "%d", liDistance);
        char lacComposed[256];
        lpLanguageManager->FormatTextV(lacComposed, 256, "GENERAL_SEPARATOR",
                                       CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP, 2,
                                       mpacMessageTypeStrings[E_MESSAGETYPE_DRIFT],
                                       CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
                                       lacDistance,
                                       CgsLanguage::LanguageManager::E_FORMAT_SMALL_DISTANCE);
        lpSlot->SetMessageText(lacComposed, -1);                                // @0x8242F0C8
        lpSlot->Refresh(sfUpdatedTextRefreshTtl, false);
        mfPrevDriftDist = mfDriftingDist;
        mfDriftingDist  = lfReset;
    }
    else                                                                         // @0x8242F0F4
    {
        lpSlot->Refresh(KF_MESSAGE_TTL_SHORT, false);
        mfDriftingDist = lfReset;
    }
}

// ===================================================================================
// UpdateTailgating  @ 0x8242F2E0 -- the distance trio, TAILGATING flavour (threshold 10).
// ===================================================================================
void BoostMessageManager::UpdateTailgating()
{
    static const f32 sfUpdatedTextRefreshTtl = KF_MESSAGE_TTL_STANDARD;   // flt_82FB2DF0

    const f32 lfReset = KF_ZERO;
    if (mfTailDist <= lfReset)                                                   // @0x8242F4B4
    {
        mfPrevTailDist = lfReset;
        mfTailDist     = lfReset;
        return;
    }

    BoostMessageSlot* lpSlot = FindMessageSlot(E_MESSAGETYPE_TAILGATING);
    if (lpSlot == 0)
    {
        if (AddMessage(E_MESSAGETYPE_TAILGATING, mpacMessageTypeStrings[E_MESSAGETYPE_TAILGATING],
                       KF_MESSAGE_TTL_SHORT, -1))                               // @0x8242F48C
        {
            mfPrevTailDist = mfTailDist;
        }
        mfTailDist = lfReset;
        return;
    }

    if (mfPrevTailDist <= lfReset)      // @0x8242F448 -- prev<=0 ONLY; the
                                        // cur<=prev disjunction is DRIFT's alone
    {
        mfPrevTailDist = mfTailDist;
        mfTailDist     = lfReset;
        return;
    }

    if (mfTailDist > KF_TAILGATING_DISPLAY_THRESHOLD_M)                          // @0x8242F378
    {
        CgsLanguage::LanguageManager* lpLanguageManager = mpStateInterface->GetLanguageManager();
        char lacDistance[32];
        const int liDistance = static_cast<int>(mfTailDist);                    // fctiwz @0x8242F388
        CgsCore::SPrintf(lacDistance, 32, "%d", liDistance);
        char lacComposed[256];
        lpLanguageManager->FormatTextV(lacComposed, 256, "GENERAL_SEPARATOR",
                                       CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP, 2,
                                       mpacMessageTypeStrings[E_MESSAGETYPE_TAILGATING],
                                       CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
                                       lacDistance,
                                       CgsLanguage::LanguageManager::E_FORMAT_SMALL_DISTANCE);
        lpSlot->SetMessageText(lacComposed, -1);                                // @0x8242F3F8
        lpSlot->Refresh(sfUpdatedTextRefreshTtl, false);
        mfPrevTailDist = mfTailDist;
        mfTailDist     = lfReset;
    }
    else                                                                         // @0x8242F424
    {
        lpSlot->Refresh(KF_MESSAGE_TTL_SHORT, false);
        mfTailDist = lfReset;
    }
}

// ===================================================================================
// UpdateChecking  @ 0x8242F828 -- TRAFFICCHECK refresh/post; the count>1 variant gets
// the "_X" plural and the count as its parameter. Console static pair flt_82FB2DF8/
// dword_82FB2DFC initialised from flt_82001C98.
// ===================================================================================
void BoostMessageManager::UpdateChecking()
{
    static const f32 sfCountedRefreshTtl = KF_MESSAGE_TTL_CHECKING;       // flt_82FB2DF8

    if (!mbChecking)                                                             // @0x8242F950
        return;

    BoostMessageSlot* lpSlot = FindMessageSlot(E_MESSAGETYPE_TRAFFIC_CHECK);
    if (lpSlot != 0)
    {
        if (miCheckingCount > 1)
        {
            char lacChecks[128];                                                // "%s_X"
            CgsCore::SPrintf(lacChecks, 128, "%s_X",
                             mpacMessageTypeStrings[E_MESSAGETYPE_TRAFFIC_CHECK]);
            lpSlot->SetMessageText(lacChecks, miCheckingCount);                 // @0x8242F8C0
            lpSlot->Refresh(sfCountedRefreshTtl, true);
        }
        else
        {
            lpSlot->SetMessageText(mpacMessageTypeStrings[E_MESSAGETYPE_TRAFFIC_CHECK], -1);
            lpSlot->Refresh(KF_MESSAGE_TTL_CHECKING, true);                     // @0x8242F908
        }
        mbChecking = false;
    }
    else
    {
        if (AddMessage(E_MESSAGETYPE_TRAFFIC_CHECK,
                       mpacMessageTypeStrings[E_MESSAGETYPE_TRAFFIC_CHECK],
                       KF_MESSAGE_TTL_CHECKING, -1))                            // @0x8242F938
        {
            mbChecking = false;
        }
    }
}

// ===================================================================================
// UpdateNearMiss  @ 0x8242F958 -- NEARMISS refresh/post, plus the CRASH-ESCAPE arm
// (this is where the PS3's empty-bodied UpdateCrashEscape logic actually lives on X360):
// when the near-miss latch is clear but the crash-escape latch is set, post CRASHESCAPE
// once no such message is live.
// Console statics: flt_82FB2E04 (add ttl) + flt_82FB2E00 (counted-refresh ttl), both
// seeded from flt_82014A4C under guard bits 0/1 of dword_82FB2E08.
// ===================================================================================
void BoostMessageManager::UpdateNearMiss()
{
    static const f32 sfAddTtl         = KF_MESSAGE_TTL_NEAR_MISS;          // flt_82FB2E04
    static const f32 sfCountedRefreshTtl = KF_MESSAGE_TTL_NEAR_MISS;      // flt_82FB2E00

    if (!mbNearMiss)                                                             // @0x8242FA94
    {
        if (mbIsCrashEscape == 1)                                                // @0x8242FAA0
        {
            if (FindMessageSlot(E_MESSAGETYPE_CRASH_ESCAPE) == 0)
            {
                AddMessage(E_MESSAGETYPE_CRASH_ESCAPE,
                           mpacMessageTypeStrings[E_MESSAGETYPE_CRASH_ESCAPE],
                           KF_MESSAGE_TTL_CRASH_ESCAPE, -1);                    // @0x8242FAD4
            }
        }
        return;
    }

    BoostMessageSlot* lpSlot = FindMessageSlot(E_MESSAGETYPE_NEARMISS);
    if (lpSlot != 0)
    {
        if (miNearMissCount > 1)
        {
            char lacMisses[128];                                                // "%s_X"
            CgsCore::SPrintf(lacMisses, 128, "%s_X",
                             mpacMessageTypeStrings[E_MESSAGETYPE_NEARMISS]);
            lpSlot->SetMessageText(lacMisses, miNearMissCount);                 // @0x8242FA14
            lpSlot->Refresh(sfCountedRefreshTtl, true);
        }
        else
        {
            lpSlot->SetMessageText(mpacMessageTypeStrings[E_MESSAGETYPE_NEARMISS], -1);
            lpSlot->Refresh(sfAddTtl, true);                                    // @0x8242FA44
        }
        mbNearMiss = false;
    }
    else
    {
        if (AddMessage(E_MESSAGETYPE_NEARMISS, mpacMessageTypeStrings[E_MESSAGETYPE_NEARMISS],
                       sfAddTtl, -1))                                           // @0x8242FA74
        {
            mbNearMiss = false;
        }
    }
}

// ===================================================================================
// UpdateStuntsJumpsAndSmashes  @ 0x8242F4C8 -- the three collectible tallies. Each block:
// compose "HUD_BOOST_COLLECTABLE" with the family string + current/total counts, post or
// live-update it, and drop the pending latch only on success. All three use
// KF_MESSAGE_TTL_STANDARD. The composed form is FormatTextV(out,128,"HUD_BOOST_COLLECTABLE",
// ID_LOOKUP, 3, <family>, ID_LOOKUP, <cur str>, INTEGER, <total str>, INTEGER).
// ===================================================================================
void BoostMessageManager::UpdateStuntsJumpsAndSmashes()
{
    const f32 lfTtl = KF_MESSAGE_TTL_STANDARD;                                   // flt_82001D9C

    // ---- JUMPS ----
    if (mbJumpMessagePending)                                                    // @0x8242F50C
    {
        CgsLanguage::LanguageManager* lpLanguageManager = mpStateInterface->GetLanguageManager();
        char lacCurrent[32];
        CgsCore::SPrintf(lacCurrent, 32, "%d", miJumpCurrentCount);
        char lacTotal[32];
        CgsCore::SPrintf(lacTotal, 32, "%d", miJumpTotalCount);
        char lacComposed[128];
        lpLanguageManager->FormatTextV(lacComposed, 128, "HUD_BOOST_COLLECTABLE",
                                       CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP, 3,
                                       "HUD_BOOST_JUMPS",
                                       CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
                                       lacCurrent, CgsLanguage::LanguageManager::E_FORMAT_INTEGER,
                                       lacTotal, CgsLanguage::LanguageManager::E_FORMAT_INTEGER);

        BoostMessageSlot* lpSlot = FindMessageSlot(E_MESSAGETYPE_JUMPS);
        if (lpSlot == 0)
        {
            const bool lbPosted = AddMessage(E_MESSAGETYPE_JUMPS, lacComposed, lfTtl, -1);
            mbJumpMessagePending = !lbPosted;                                   // @0x8242F5C4
        }
        else if (!lpSlot->IsInTransition() && lpSlot->IsInUse())
        {
            lpSlot->SetMessageText(lacComposed, -1);                            // @0x8242F5F0
            lpSlot->Refresh(lfTtl, true);
            mbJumpMessagePending = false;                                       // @0x8242F604
        }
    }

    // ---- SMASHES ----
    if (mbSmashesMessagePending)                                                // @0x8242F614
    {
        CgsLanguage::LanguageManager* lpLanguageManager = mpStateInterface->GetLanguageManager();
        char lacCurrent[32];
        CgsCore::SPrintf(lacCurrent, 32, "%d", miSmashesCurrentCount);
        char lacTotal[32];
        CgsCore::SPrintf(lacTotal, 32, "%d", miSmashesTotalCount);
        char lacComposed[128];
        lpLanguageManager->FormatTextV(lacComposed, 128, "HUD_BOOST_COLLECTABLE",
                                       CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP, 3,
                                       "HUD_BOOST_SMASHES",
                                       CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
                                       lacCurrent, CgsLanguage::LanguageManager::E_FORMAT_INTEGER,
                                       lacTotal, CgsLanguage::LanguageManager::E_FORMAT_INTEGER);

        BoostMessageSlot* lpSlot = FindMessageSlot(E_MESSAGETYPE_SMASHES);
        if (lpSlot == 0)
        {
            const bool lbPosted = AddMessage(E_MESSAGETYPE_SMASHES, lacComposed, lfTtl, -1);
            mbSmashesMessagePending = !lbPosted;                                // @0x8242F6C8
        }
        else if (!lpSlot->IsInTransition() && lpSlot->IsInUse())
        {
            lpSlot->SetMessageText(lacComposed, -1);                            // @0x8242F6F4
            lpSlot->Refresh(lfTtl, true);
            mbSmashesMessagePending = false;                                    // @0x8242F708
        }
    }

    // ---- BILLBOARDS ---- (message type 37 -- see the enum's runtime note)
    if (mbBillBoardsMessagePending)                                             // @0x8242F718
    {
        CgsLanguage::LanguageManager* lpLanguageManager = mpStateInterface->GetLanguageManager();
        char lacCurrent[32];
        CgsCore::SPrintf(lacCurrent, 32, "%d", miBillBoardsCurrentCount);
        char lacTotal[32];
        CgsCore::SPrintf(lacTotal, 32, "%d", miBillBoardsTotalCount);
        char lacComposed[128];
        lpLanguageManager->FormatTextV(lacComposed, 128, "HUD_BOOST_COLLECTABLE",
                                       CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP, 3,
                                       "HUD_BOOST_BILLBOARDS",
                                       CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
                                       lacCurrent, CgsLanguage::LanguageManager::E_FORMAT_INTEGER,
                                       lacTotal, CgsLanguage::LanguageManager::E_FORMAT_INTEGER);

        BoostMessageSlot* lpSlot = FindMessageSlot(E_MESSAGETYPE_STUNTS);
        if (lpSlot == 0)
        {
            const bool lbPosted = AddMessage(E_MESSAGETYPE_STUNTS, lacComposed, lfTtl, -1);
            mbBillBoardsMessagePending = !lbPosted;                             // @0x8242F7CC
        }
        else if (!lpSlot->IsInTransition() && lpSlot->IsInUse())
        {
            lpSlot->SetMessageText(lacComposed, -1);                            // @0x8242F800
            lpSlot->Refresh(lfTtl, true);
            mbBillBoardsMessagePending = false;                                 // @0x8242F814
        }
    }
}

// ===================================================================================
// UpdateShowtime  @ 0x8242FAE0 -- the showtime/crash-mode ticker: per-category hit
// message, the vehicles-hit counter, the overhead-sign flag. Every posted message uses
// KF_MESSAGE_TTL_STUNT_FAMILY; results gate ONLY their own latch reset.
// ===================================================================================
void BoostMessageManager::UpdateShowtime()
{
    if (meHitVehicleCategory != 8)                                              // "none" sentinel
    {
        CGS_ASSERT(meHitVehicleCategory >= 0, "meHitVehicleCategory >= 0");     // cpp:1722
        CGS_ASSERT(static_cast<u32>(meHitVehicleCategory) <
                   sizeof(KA_VEHICLE_SCORE_CATEGORY_TO_MESSAGE_TYPE) /
                   sizeof(KA_VEHICLE_SCORE_CATEGORY_TO_MESSAGE_TYPE[0]),
                   "(uint32_t) meHitVehicleCategory < sizeof(...)");            // cpp:1723

        const MessageType leCategoryMessage =
            static_cast<MessageType>(KA_VEHICLE_SCORE_CATEGORY_TO_MESSAGE_TYPE[meHitVehicleCategory]);
        const char* lpcCategoryString = mpacMessageTypeStrings[leCategoryMessage];

        CgsLanguage::LanguageManager* lpLanguageManager = mpStateInterface->GetLanguageManager();
        char lacComposed[255];
        lpLanguageManager->FormatTextFromInt(lacComposed, 255, lpcCategoryString,
                                             CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
                                             miHitVehicleScore,
                                             CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
        if (AddMessage(leCategoryMessage, lacComposed, KF_MESSAGE_TTL_STUNT_FAMILY, -1))
        {
            meHitVehicleCategory = 8;                                           // @0x8242FBC8
        }
    }

    if (miHitVehicleCount > 0)                                                  // @0x8242FBD0
    {
        CgsLanguage::LanguageManager* lpLanguageManager = mpStateInterface->GetLanguageManager();
        char lacComposed[255];
        lpLanguageManager->FormatTextFromInt(lacComposed, 255, "BOOST_SHOWTIME_VEHICLES_HIT",
                                             CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
                                             miHitVehicleCount,
                                             CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
        if (AddMessage(E_MESSAGETYPE_N_CARS_HIT, lacComposed, KF_MESSAGE_TTL_STUNT_FAMILY, -1))
        {
            miHitVehicleCount = 0;                                              // @0x8242FC34
        }
    }

    if (mbHitOverheadSign)                                                      // @0x8242FC38
    {
        if (AddMessage(E_MESSAGETYPE_OVERHEAD_SIGN,
                       mpacMessageTypeStrings[E_MESSAGETYPE_OVERHEAD_SIGN],
                       KF_MESSAGE_TTL_STUNT_FAMILY, -1))
        {
            mbHitOverheadSign = false;                                          // @0x8242FC68
        }
    }
}

// ===================================================================================
// UpdateAir  @ 0x824378C8 -- the AIR timer. While airborne past 0.25s it posts/refreshes
// the live seconds counter (updated only in whole-1.0s steps); when no slot is free it
// falls back to posting training event 58 (the exact wrapper record the console builds:
// {size=4, type=572, offset=12, payload 58}).
// Console statics: flt_82FB2E0C (text-arm refresh ttl, seeded KF_MESSAGE_TTL_STANDARD
// under guard dword_82FB2E10 bit 0).
// ===================================================================================
void BoostMessageManager::UpdateAir()
{
    static const f32 sfUpdatedTextRefreshTtl = KF_MESSAGE_TTL_STANDARD;   // flt_82FB2E0C

    if (mfAirTime <= KF_AIR_MINIMUM_TIME)                                        // @0x82437ABC
    {
        mfPrevAirTime = KF_ZERO;
        mfAirTime     = KF_ZERO;
        return;
    }

    BoostMessageSlot* lpSlot = FindMessageSlot(E_MESSAGETYPE_AIR);
    if (lpSlot != 0)
    {
        if (mfPrevAirTime <= KF_ZERO)                                            // @0x82437A28
        {
            // f1 was pre-loaded with flt_82001C98 (1.0) BEFORE the null check @0x82437948
            lpSlot->Refresh(KF_MESSAGE_TTL_CHECKING, true);
            mfPrevAirTime = mfAirTime;
            mfAirTime     = KF_ZERO;
            return;
        }

        if (mfAirTime > KF_MESSAGE_TTL_CHECKING)                                 // step gate @0x8243795C
        {
            CgsLanguage::LanguageManager* lpLanguageManager = mpStateInterface->GetLanguageManager();
            char lacSeconds[32];
            CgsCore::SPrintf(lacSeconds, 32, "%.2f", mfAirTime);                // f1->double @0x8243797C
            char lacComposed[256];
            lpLanguageManager->FormatTextV(lacComposed, 256, "GENERAL_SEPARATOR",
                                           CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP, 2,
                                           mpacMessageTypeStrings[E_MESSAGETYPE_AIR],
                                           CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
                                           lacSeconds,
                                           CgsLanguage::LanguageManager::E_FORMAT_SECONDS_HUNDREDTHS);
            lpSlot->SetMessageText(lacComposed, -1);                            // @0x824379E0
            lpSlot->Refresh(sfUpdatedTextRefreshTtl, false);
            mfPrevAirTime = mfAirTime;
            mfAirTime     = KF_ZERO;
        }
        else                                                                     // @0x82437A0C
        {
            lpSlot->Refresh(KF_MESSAGE_TTL_CHECKING, false);
            mfAirTime = KF_ZERO;
        }
    }
    else                                                                         // @0x82437A4C
    {
        if (AddMessage(E_MESSAGETYPE_AIR, mpacMessageTypeStrings[E_MESSAGETYPE_AIR],
                       KF_MESSAGE_TTL_CHECKING, -1))   // f1 still flt_82001C98 @0x82437A4C
        {
            mfPrevAirTime = mfAirTime;
        }

        // No room on the stack: request training message 58 instead (GUI event id 572).
        // The console stacks the GuiEventWrapper-shaped record {4, 572, 12, payload} and
        // AddEvents 16 bytes on channel 40 -- byte-for-byte the record documented on the
        // committed GuiEventRequestTraining home.
        struct RequestTrainingWrapper
        {
            s32 miOutEventSize;
            s32 miOutEventType;
            s32 miOutEventOffset;
            s32 meTrainingType;
        } lRequest;
        lRequest.miOutEventSize   = 4;                                          // @0x82437A9C
        lRequest.miOutEventType   = 572;                                        // 0x23C
        lRequest.miOutEventOffset = 12;
        lRequest.meTrainingType   = 58;                                         // 0x3A @0x82437AA0
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest), 40, 16);      // @0x82437AA8

        mfAirTime = KF_ZERO;                                                    // @0x82437AAC
    }
}

// ===================================================================================
// Update  @ 0x8243E198 -- the per-frame pump.
//   1. Tick all three slots; note whether any is mid-transition; bitmask the stack rows
//      currently occupied.
//   2. With nothing transitioning, compact: find the first free row below the active
//      count and ShuffleDown everything sitting above it.
//   3. Showtime mode runs ONLY UpdateShowtime. Normal mode runs the whole hint chain:
//      jumps/smashes/billboards tallies, takedown, signature stunt, stunts, shortcut,
//      air, oncoming, drift, tailgating, near miss (+crash escape), traffic check,
//      barrel rolls, flat spin, handbrake, clean landing, vehicle impacts.
// ===================================================================================
void BoostMessageManager::Update(f32 lfTimeStep, bool lbShowtimeMode)
{
    u64 luOccupiedRows = 0;
    bool lbAnyTransitioning = false;

    for (u32 luIndex = 0; luIndex < KI_MAX_BOOSTMESSAGE_SLOTS; ++luIndex)        // @0x8243E210
    {
        BoostMessageSlot& lrSlot = mSlots.GetItem(luIndex);
        lrSlot.Update(lfTimeStep);

        if (lrSlot.IsInTransition())
            lbAnyTransitioning = true;

        if (!lrSlot.IsInUse())
            continue;

        const s32 liPosition = lrSlot.GetSlotPosition();
        CGS_ASSERT(liPosition < 3, "Array index out of bounds");                // cpp:222
        luOccupiedRows |= (1ull << liPosition);                                 // @0x8243E34C
    }

    if (!lbAnyTransitioning)                                                    // @0x8243E380
    {
        const s32 liActiveCount = GetNumActiveSlots();
        if (liActiveCount > 0)
        {
            // First unoccupied row below the active count (rows are kept compacted).
            s32 liTargetRow = 0;                                                // @0x8243E3A8
            while (liTargetRow < liActiveCount)
            {
                CGS_ASSERT(liTargetRow < 3, "invalid index");
                if ((luOccupiedRows & (1ull << liTargetRow)) == 0)
                    break;
                ++liTargetRow;
            }

            if (liTargetRow < liActiveCount && (luOccupiedRows & (1ull << liTargetRow)) == 0)
            {
                for (u32 luIndex = 0; luIndex < KI_MAX_BOOSTMESSAGE_SLOTS; ++luIndex)
                {
                    BoostMessageSlot& lrSlot = mSlots.GetItem(luIndex);
                    if (lrSlot.IsInUse() && lrSlot.GetSlotPosition() > liTargetRow)
                        lrSlot.ShuffleDown();                                   // @0x8243E518
                }
            }
        }
    }

    if (lbShowtimeMode)                                                         // @0x8243E528
    {
        UpdateShowtime();
        return;
    }

    UpdateStuntsJumpsAndSmashes();

    // ---- takedown (the PS3 UpdateTakdowns body, folded here on X360) ----
    if (mbPlayerDoesTakedown)                                                   // @0x8243E53C
    {
        if (FindMessageSlot(E_MESSAGETYPE_TAKEDOWN) == 0)
        {
            const bool lbPosted =
                AddMessage(E_MESSAGETYPE_TAKEDOWN,
                           mpacMessageTypeStrings[E_MESSAGETYPE_TAKEDOWN],
                           KF_MESSAGE_TTL_STANDARD, -1);                        // @0x8243E57C
            mbPlayerDoesTakedown = !lbPosted;
        }
    }

    // ---- signature stunt (the PS3 UpdateSignatureStunts body, folded here) ----
    if (mSignatureStuntId != K_SIGNATURE_STUNT_INVALID)                         // qword cmp @0x8243E594
    {
        char lacStuntId[13];
        CgsIDConvertToString(mSignatureStuntId, lacStuntId);                    // @0x8243E5A0
        char lacComposed[128];                                                  // "%s %s"
        CgsCore::SPrintf(lacComposed, 128, "%s %s",
                         mpacMessageTypeStrings[E_MESSAGETYPE_SIGNATURE_STUNT], lacStuntId);
        if (AddMessage(E_MESSAGETYPE_SIGNATURE_STUNT, lacComposed,
                       KF_MESSAGE_TTL_STANDARD, -1))                            // @0x8243E5D4
        {
            mSignatureStuntId = K_SIGNATURE_STUNT_INVALID;
        }
    }

    UpdateStunts();

    // ---- shortcut (the PS3 UpdateShortcuts body, folded here) ----
    if (mShortcutId != 0)                                                       // @0x8243E5F4
    {
        BoostMessageSlot* lpSlot = FindMessageSlot(E_MESSAGETYPE_SHORTCUT);
        if (lpSlot != 0)
        {
            lpSlot->RaiseTimeToLiveFloor(KF_MESSAGE_TTL_STUNT_FAMILY);          // @0x8243E650
        }
        else
        {
            AddMessage(E_MESSAGETYPE_SHORTCUT, mpacMessageTypeStrings[E_MESSAGETYPE_SHORTCUT],
                       KF_MESSAGE_TTL_STUNT_FAMILY, -1);                        // @0x8243E62C
        }
        mShortcutId = 0;                                                        // @0x8243E654
    }

    UpdateAir();
    UpdateOncoming();
    UpdateDrift();
    UpdateTailgating();
    UpdateNearMiss();
    UpdateChecking();
    UpdateBarrelRolls();
    UpdateSpin();
    UpdateHandBrake();

    // ---- clean landing (the PS3 UpdateLandingsCheck body, folded here) ----
    if (mbCleanLandingComplete)                                                 // @0x8243E6A0
    {
        AddMessage(E_MESSAGETYPE_CLEANLANDING,
                   mpacMessageTypeStrings[E_MESSAGETYPE_CLEANLANDING],
                   KF_MESSAGE_TTL_CLEANLANDING, -1);                            // result ignored
    }
    mbCleanLandingComplete       = false;                                       // @0x8243E6CC
    mbSuccessfulLandingComplete  = false;

    UpdateVehicleImpacts();
}

} // namespace BrnGui
