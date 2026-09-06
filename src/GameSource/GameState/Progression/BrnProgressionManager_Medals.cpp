// ============================================================================================
// b5-decomp/src/GameSource/GameState/Progression/BrnProgressionManager_Medals.cpp
// ============================================================================================
// [progression wave 2026-09-06, lane `medals`] THE LICENCE / RANK-UP CHAIN.
//
//   ProgressionManager::UpdatePlayerMedals        @0x8239FE50  (DWARF BrnProgressionManager.cpp:630)
//   ProgressionManager::CalculateRankFromMedalTotal @0x8237AB38 (DWARF :735, `int8_t (uint32_t) const`)
//   ProgressionManager::ClearMedalsOnRankUp       @0x823705D8  (DWARF :4247)
//   ProgressionManager::UnlockDefaultPlayerCars   @0x8237BF98  (DWARF :4323)
//   ProgressionManager::FixGameModeRanks          @0x82395CD8  (DWARF :579)
//   ProgressionManager::DEBUG_ClearMedals         @0x82366BF8  (DWARF :3175)
//
// WHY THE LICENCE NEVER ADVANCED. UpdatePlayerMedals is the ONLY producer of game action 200
// (E_ACTION_UPDATE_PLAYER_MEDALS -> GuiEventMedalUpdate 307) and the ONLY console caller of
// UnlockToProgressionRank in the whole XEX. It had no body, so all three of its seats were
// parked: OnEventFinishUpdateProfile's P6, PreWorldUpdate's `mbPlayerMedalsUpdateRequired` arm,
// and -- transitively -- ProgressionManager::Construct @0x8237A5F8, whose two boot stores
// (`stbx -2 -> +0x2096C` @0x8237A7A0 and `stbx 1 -> +0x20973` @0x8237A7B0) are what ARM that arm
// in the first place. Construct is not reconstructed either, so on PC nothing set the flag and
// the arm could not have run even with a body behind it. The result on screen: the medal HUD
// never refreshed, the rank-up car unlocks never fired, and Profile::mi8CurrentProgressionRank
// kept the -2 "rank not set" seed Profile::Construct wrote.
//
// A partfile, not BrnProgressionManager.cpp, for the reason the _PreWorldUpdate / _EventFinish /
// _Completion / _Unlocks / _Rivals partfiles exist: the console homes all six functions in
// BrnProgressionManager.cpp, which is why every assert below is fired through
// CgsDev::Assert::{Begin,Fire,End}Assert with that file's baked path and the binary's own line
// number rather than through CGS_ASSERT's __FILE__/__LINE__.
//
// Every member is reached BY NAME. The console offsets are quoted only to say which member each
// load lands on.
// ============================================================================================
#include "BrnProgressionManager.h"
#include "BrnProfile.h"

#include <stdlib.h>                                                     // getenv (the [medals] witness gate)

#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CgsDev::Assert::{Begin,Fire,End}Assert
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                 // CgsCore::SPrintf (the Q3 telemetry parameter)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // gpDebugPrint + gxMessageFilterFlags
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // GameActionQueue::AddEvent
#include "GameSource/GameState/BrnGameActions.h"                        // UpdatePlayerMedalsAction (200) / E_ACTION_SEND_TELEMETRY (228)
#include "GameSource/GameState/BrnGameStateModuleIO.h"                  // GameStateModuleIO::GameActionQueue
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h" // OnLicenseUpgrade (Q3)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"             // TelemetryData / E_TELEMETRY_EVENT_EARNED_LICENCE
#include "SharedClasses/Progression/BrnProgressionData.h"               // ProgressionData
#include "SharedClasses/Progression/BrnProgressionRankData.h"           // ProgressionRankData (the 112-byte per-rank record)
#include "SharedClasses/Progression/BrnRaceEventData.h"                 // EventJunction / RaceEventData

namespace BrnProgression
{

namespace GsmIO = BrnGameState::GameStateModuleIO;

namespace
{
    // The verbatim X360-baked source path this TU's asserts reference (identical spelling to
    // BrnProgressionManager.cpp's own KAC_PROGMGR_FILE -- the console homes these six functions
    // there, so their asserts must keep that file and the binary's line numbers).
    const char* const KAC_PROGMGR_FILE =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Progression/BrnProgressionManager.cpp";

    // IMAGE-CITED CONSTANT (big-endian dump of scratch/postfx_step9_final/envfix/work/image.bin,
    // file offset == VA - 0x82000000):
    //   flt_82029B70 = 0x7F7FFFFF == 3.40282347e+38f == FLT_MAX -- the seed
    //   Profile::GetCurrentCarTypeWithMinDistance's inlined min-finder starts from
    //   (UnlockToProgressionRank's shared rank tail, `lfs f13, flt_82029B70@l(r11)` @0x8239E0EC).
    // It is cited here rather than used here: the min-finder now lives in BrnProfile.h beside the
    // array it scans, and the constant is quoted on that body.

    // The licence rank at which the credits become selectable (`cmpwi cr6, r10, 5` @0x8239E170,
    // then `stbx 1 -> profile+0x1CD14` == Profile::mbHasUnlockedCredits @+118036).
    const s32 KI_RANK_THAT_UNLOCKS_CREDITS = 5;

    // The trophy type UpdatePlayerMedals evaluates on every call (`li r4, 0x16` @0x8239FF80).
    // 22 == BrnProgression::E_UNLOCKTYPE_* (the trophy table is data; OnTrophyUnlock @0x82389740
    // owns the "has this trophy's condition been met" walk, which is why the call is
    // unconditional here).
    const s32 KI_TROPHY_TYPE_MEDAL_TOTAL = 22;

    // The per-mode legs FixGameModeRanks fixes up, in the console's own order
    // (@0x82395CEC mode 0, @0x82395D6C mode 3, then the two INLINED legs for 7 and 8 --
    // see the banner on the body).
    const GsmIO::EGameModeType KAE_DIFFICULTY_SCALED_MODES[4] =
    {
        GsmIO::E_MODE_OFFLINE_RACE,   // 0 -> maiRankWinsPerOfflineGameMode[0], rank record +0x60
        GsmIO::E_MODE_ROAD_RAGE,      // 3 -> [3],                              rank record +0x62
        GsmIO::E_MODE_STUNT_ATTACK,   // 7 -> [7],                              rank record +0x61
        GsmIO::E_MODE_MARKED_MAN      // 8 -> [8],                              rank record +0x63
    };

    // [FLAG PC witness] the `[medals] ...` lines -- NOT IN THE X360 BINARY. Opt-in behind
    // BRN_PROGRESSION_MEDALS, capped, printed on every call of two functions that run a handful
    // of times per boot. tools/tests/cases/progression_medals.ps1 is the only consumer.
    bool MedalsDiagEnabled()
    {
        static const bool sbDiag = (getenv("BRN_PROGRESSION_MEDALS") != 0);
        return sbDiag && CgsDev::Log::gpDebugPrint != 0;
    }

    // One-shot park reporter (campaign house rule: a park must be visible, not silent).
    void ParkOnce(bool& lrbAlreadySaid, const char* lpcMessage)
    {
        if (lrbAlreadySaid)
        {
            return;
        }
        lrbAlreadySaid = true;
        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint << lpcMessage;
        }
    }
    bool gbSaidProfileReadyForDisplay = false;
    bool gbSaidNoAchievementManager   = false;
}

// --------------------------------------------------------------------------------------------
// ProgressionManager::CalculateRankFromMedalTotal  (X360 0x8237AB38)
// DWARF BrnProgressionManager.h:604 -- `int8_t CalculateRankFromMedalTotal(uint32_t) const;`
// DWARF locals (BrnProgressionManager.cpp:736/:737): `int8_t li8TestRank; int8_t li8RankCount;`
//
// "Which licence rank does this many medals buy?" Walk the per-rank medal thresholds up from the
// player's current rank until one is out of reach.
//   0x8237AB58..0x8237AB68  li8RankCount = (s8)(u8)muProgressionRankCount   (`clrlwi 24` + later `extsb`)
//   0x8237AB7C..0x8237ABAC  li8TestRank  = (mi8ProgressionRank == (s32)rankCount)
//                                          ? (s8)(rankCount - 1)            -- already off the end
//                                          : (s8)GetProgressionRank()       -- the clamped cache
//   0x8237ABD0..0x8237AC28  while (li8TestRank < li8RankCount):
//                             record = GetProgressionRankData(li8TestRank)  -- owns the :330 assert
//                             if (luMedalTotal < record->mu16MedalThresholdToNextRank) break;  (cmplw, UNSIGNED)
//                             li8TestRank = (s8)(li8TestRank + 1);          -- narrowed EVERY pass
// ⚠️ The `==` against the RAW rank count (not count-1) is the console's own, and it is NOT the
// same test as PlayerHasFinishedLastRank's -- GetProgressionRankNormalisedForCurrentRank carries
// the identical pair with the same note. Do not unify them.
// [!] The "luIndex < muProgressionRankCount" assert (BrnProgressionData.h:330) that the console
// fires at 0x8237ABE8 belongs to the INLINED GetProgressionRankData; routing through the accessor
// reproduces it exactly once, so it is not restated here.
// --------------------------------------------------------------------------------------------
s8 ProgressionManager::CalculateRankFromMedalTotal(u32 luMedalTotal) const
{
    const s8 li8RankCount =
        static_cast<s8>(static_cast<u8>(GetProgressionData()->GetProgressionRankCount()));

    s8 li8TestRank;
    if (static_cast<s32>(mi8ProgressionRank) ==
        static_cast<s32>(GetProgressionData()->GetProgressionRankCount()))
    {
        li8TestRank = static_cast<s8>(GetProgressionData()->GetProgressionRankCount() - 1u);
    }
    else
    {
        li8TestRank = static_cast<s8>(GetProgressionRank());
    }

    while (static_cast<s32>(li8TestRank) < static_cast<s32>(li8RankCount))
    {
        const ProgressionRankData* lpcRankData =
            GetProgressionData()->GetProgressionRankData(static_cast<u32>(static_cast<s32>(li8TestRank)));

        // `cmplw cr6, r24, r11` -- an UNSIGNED compare of the medal total against the u16 threshold.
        if (luMedalTotal < static_cast<u32>(lpcRankData->GetMedalThresholdToNextRank()))
        {
            break;
        }
        li8TestRank = static_cast<s8>(li8TestRank + 1);
    }

    return li8TestRank;
}

// --------------------------------------------------------------------------------------------
// ProgressionManager::ClearMedalsOnRankUp  (X360 0x823705D8)
// DWARF BrnProgressionManager.h:619 -- `void ClearMedalsOnRankUp();`
// DWARF locals/hints (:4247): `uint32_t luIndex; uint32_t luEventCount;` +
//   `const ProfileEvent* lpProfileEvent; const RaceEventData* lpEventData;` and the callee list
//   {Profile::GetEventCount, Profile::GetEvent, ProgressionData::FindOfflineEvent,
//    ProfileEvent::GetID, Profile::ResetEventMedals} -- which is exactly the asm, with
//   FindOfflineEvent and ResetEventMedals inlined.
//
// A rank-up wipes the MEDAL bits of every profile event so the new rank is earned again, but it
// keeps the "you have already won this" memory:
//   0x823705F0..0x82370610  `if ((s8)mi8ProgressionRank == (s32)muProgressionRankCount) return;`
//                           -- the same off-the-end test CalculateRankFromMedalTotal uses.
//   0x82370614..0x82370620  luEventCount = mProfile.GetEventCount()  (`lwz r21, 0x3E8(this)` ==
//                           Profile+0x278 == miEventCount; read ONCE, the loop bound)
//   0x82370654..0x82370678  Profile::GetEvent(luIndex)  -- owns the BrnProfile.h:3004 index assert
//   0x82370680..0x8237069C  the ":4369 lpProfileEvent" assert
//   0x823706A8..0x823706F0  FindOfflineEvent(lpProfileEvent->GetID()) inlined: scan the junction
//                           table (count +0x1C, base +0x18, stride 16) for a matching muID and
//                           take that junction's OFFLINE event slot (+0x04)
//   0x823706F0..0x82370704  the ":4372 lpEventData" assert
//   0x82370708..0x82370740  `flags &= (lpEventData->GetSpecialEventCarId() != 0) ? 0x11 : 0x21`
//                           (`ld r11, 0x10(event)`; 0x11 == DISCOVERED|WON_SPECIAL_EVENT_BEFORE,
//                            0x21 == DISCOVERED|WON_EVENT_BEFORE) == Profile::ResetEventMedals
// So FINISHED / RANK_WIN / NON_RANK_WIN are cleared and DISCOVERED plus the matching
// "won before" bit survive.
// --------------------------------------------------------------------------------------------
void ProgressionManager::ClearMedalsOnRankUp()
{
    if (static_cast<s32>(mi8ProgressionRank) ==
        static_cast<s32>(GetProgressionData()->GetProgressionRankCount()))
    {
        return;
    }

    const u32 luEventCount = mProfile.GetEventCount();

    for (u32 luIndex = 0; luIndex < luEventCount; ++luIndex)
    {
        const ProfileEvent* lpcProfileEvent = mProfile.GetEvent(luIndex);
        if (lpcProfileEvent == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpProfileEvent", KAC_PROGMGR_FILE, 4369);
            CgsDev::Assert::EndAssert();
            continue;   // [PC GUARD] the console reads through the null; GetEvent cannot return
                        // one on the host, so the guard is unreachable rather than divergent.
        }

        const RaceEventData* lpcEventData =
            GetProgressionData()->FindOfflineEvent(lpcProfileEvent->GetID());
        if (lpcEventData == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpEventData", KAC_PROGMGR_FILE, 4372);
            CgsDev::Assert::EndAssert();
        }

        // [PC GUARD] the console loads `0x10(lpEventData)` UNGUARDED right after that assert, so
        // a null there is an access violation on the console too. It cannot happen: the profile's
        // event records are minted by UnlockToProgressionRank from the junctions that HAVE an
        // offline event, so every id in the list resolves. A null is treated as "not a special
        // event" (mask 0x21) rather than dereferenced. DELETE-WHEN nothing else can add a
        // ProfileEvent whose id has no junction.
        const bool lbIsSpecialEvent = (lpcEventData != 0) && (lpcEventData->GetSpecialEventCarId() != 0);

        mProfile.ResetEventMedals(luIndex, lbIsSpecialEvent);
    }
}

// --------------------------------------------------------------------------------------------
// ProgressionManager::UnlockDefaultPlayerCars  (X360 0x8237BF98)
// DWARF BrnProgressionManager.h:679 -- `void UnlockDefaultPlayerCars();`
// DWARF local (:4323): `uint32_t luFreeCarIndex;`; hint callees {ProgressionData::GetPlayerCarId,
// ResourcePtr<ProgressionData>::operator->} -- Profile::FindCar is the inlined scan below.
//
// The rank-0 starting garage: every car id in PROGRESSION.DAT's player-car table becomes owned,
// and its unlock sequence is marked already-seen so the game does not play a reveal for a car the
// player has had since the first frame.
//   0x8237BFB8..0x8237BFC4  loop bound = muPlayerCarIdCount (+0x0C), re-read every pass (0x8237C0C0)
//   0x8237BFE4..0x8237C020  GetPlayerCarId(luFreeCarIndex) -- owns the :310 index assert; `ldx`,
//                           an 8-byte CgsID at the +0x08 table
//   0x8237C018..0x8237C064  Profile::FindCar inlined (count Profile+0x26C, base +0x280, stride
//                           0x18, `cmpld` on the record's 8-byte id)
//   0x8237C068..0x8237C070  on a MISS: AddCar(lCarId, 0 == E_UNLOCK_TYPE_UNLOCK)
//   0x8237C074..0x8237C0B0  a SECOND GetPlayerCarId (hence the second :310 assert in the asm) ->
//                           Profile::SetCarUnlockAlreadyShown
// ⚠️ The pseudocode's `if ( 24 * v10 + a1 + 368 == -640 )` is Hex-Rays rendering the console's
// `addi r11, r11, 0x280 / cmplwi r11, 0 / bne` -- a null test on the FOUND record's address, which
// can never be null. It is the else-arm of the found/not-found fork, nothing more.
// --------------------------------------------------------------------------------------------
void ProgressionManager::UnlockDefaultPlayerCars()
{
    for (u32 luFreeCarIndex = 0;
         luFreeCarIndex < GetProgressionData()->GetPlayerCarIdCount();
         ++luFreeCarIndex)
    {
        const CgsID lCarId = GetProgressionData()->GetPlayerCarId(luFreeCarIndex);

        if (mProfile.FindCar(lCarId) == 0)
        {
            AddCar(lCarId, CarData::E_UNLOCK_TYPE_UNLOCK);              // `li r5, 0`
        }

        mProfile.SetCarUnlockAlreadyShown(GetProgressionData()->GetPlayerCarId(luFreeCarIndex));
    }
}

// --------------------------------------------------------------------------------------------
// ProgressionManager::FixGameModeRanks  (X360 0x82395CD8)
// DWARF BrnProgressionManager.h:579 -- `void FixGameModeRanks();`
//
// The per-mode difficulty ladders are not allowed to lag more than TWO ranks behind the player's
// licence: for each of the four modes that have their own ladder, if the mode's derived rank is
// below (licence rank - 2), the mode's rank-win tally is raised to the threshold of that rank.
// It is what stops a player who ranked up on races from meeting rank-1 road rage at licence 5.
//
//   per leg: `lbz r11, 0x1E0(this)` == Profile+0x70 == mProfile.GetCurrentProgressionRank(),
//            `extsb` then `addi r11, r11, -2`                          -- the target rank
//            GetProgressionRankForGameMode(mode)                        -- the mode's own rank
//            `bge` -> skip when the mode is already at or above the target
//            else store GetRankThresholdForEvent(target, mode) into
//            Profile::maiRankWinsPerOfflineGameMode[mode] (this+0x36C + 4*mode).
//   leg 1 @0x82395CEC mode 0 -> +0x36C, rank record +0x60
//   leg 2 @0x82395D6C mode 3 -> +0x378, rank record +0x62
//   leg 3 @0x82395DD0 mode 7 -> +0x388, rank record +0x61
//   leg 4 @0x82395EE4 mode 8 -> +0x38C, rank record +0x63
//
// ⭐ INLINING REVERSAL. Legs 1 and 2 `bl GetProgressionRankForGameMode`; legs 3 and 4 carry that
// function OPEN-CODED (the same `liProgressionRank` climb from 1, the same `extsb`-narrowed rank
// count, and the same "liProgressionRank >= 0 && liProgressionRank < liNumRanks" assert with the
// same baked line 3802 -- which is that function's own assert, not this one's). The compiler
// folded the constant-mode switch away for the last two; the source called the same function four
// times, so it is written that way here. The threshold reads are likewise
// GetRankThresholdForEvent's own switch, folded to one byte load per leg.
//
// ⚠️ The target rank can be NEGATIVE (the profile's -2 seed gives -4). The console does not guard
// it and does not need to: the mode rank is never negative, so `modeRank < target` is false and
// the arm is skipped. Reproduced as shipped -- no invented clamp.
// --------------------------------------------------------------------------------------------
void ProgressionManager::FixGameModeRanks()
{
    s8 lai8ModeRanks[4] = { 0, 0, 0, 0 };

    for (s32 liLeg = 0; liLeg < 4; ++liLeg)
    {
        const GsmIO::EGameModeType leGameModeType = KAE_DIFFICULTY_SCALED_MODES[liLeg];

        const s8  li8ModeRank   = GetProgressionRankForGameMode(leGameModeType);
        const s32 liTargetRank  = static_cast<s32>(mProfile.GetCurrentProgressionRank()) - 2;

        lai8ModeRanks[liLeg] = li8ModeRank;

        if (static_cast<s32>(li8ModeRank) < liTargetRank)
        {
            mProfile.SetNumRankWinsForGameMode(GetRankThresholdForEvent(liTargetRank, leGameModeType),
                                               leGameModeType);
        }
    }

    // [FLAG PC witness] see MedalsDiagEnabled.
    if (MedalsDiagEnabled())
    {
        static s32 siPrintedFixRanks = 0;
        if (siPrintedFixRanks < 16)
        {
            ++siPrintedFixRanks;
            *CgsDev::Log::gpDebugPrint
                << "[medals] fixranks rank="  << static_cast<s32>(mProfile.GetCurrentProgressionRank())
                << " race="                   << static_cast<s32>(lai8ModeRanks[0])
                << " roadrage="               << static_cast<s32>(lai8ModeRanks[1])
                << " stunt="                  << static_cast<s32>(lai8ModeRanks[2])
                << " markedman="              << static_cast<s32>(lai8ModeRanks[3])
                << "\n";
        }
    }
}

// --------------------------------------------------------------------------------------------
// ProgressionManager::UpdatePlayerMedals  (X360 0x8239FE50)
// DWARF BrnProgressionManager.h:658 -- `void UpdatePlayerMedals(GameActionQueue*);`
// DWARF locals (:631-:638), used verbatim below: `SendPlayerMedalAction lSendPlayerMedalAction;
//   uint32_t luTotalWinsForCurrentRank, luRankWin, luNonRankWin, luSpecialEventsWonBefore;
//   int32_t liWinsForNextRank; int8_t li8TestRank, liTopRankIndex;`
// ⓘ NAME NOTE: the DWARF calls the 8-byte record `SendPlayerMedalAction`; this tree already
// models it as GameStateModuleIO::UpdatePlayerMedalsAction (BrnGameActions.h:1930, named after
// the producer). Same record, same four halfwords, same size -- not renamed here because that
// header belongs to another lane this wave.
//
// Count the profile's medals, turn the total into a licence rank, unlock up to it if it grew, and
// publish the four numbers the medal HUD shows.
//
//   0x8239FE64..0x8239FE8C  liTopRankIndex = (s8)(muProgressionRankCount - 1)
//   0x8239FE90              Profile::GetTotalWinCount(luRankWin, luNonRankWin, luSpecialEventsWonBefore)
//                           (r4/r5/r6 == the three out-params, in that order)
//   0x8239FEA4..0x8239FEC4  luTotalWinsForCurrentRank = luRankWin, PLUS luSpecialEventsWonBefore
//                           when the player is already on the TOP rank -- the special events only
//                           count toward the total once there is no next rank to earn.
//   0x8239FEC8..0x8239FEEC  li8TestRank = (mi8ProgressionRank == (s32)rankCount)
//                                         ? mi8ProgressionRank
//                                         : CalculateRankFromMedalTotal(luTotalWinsForCurrentRank)
//   0x8239FEF0..0x8239FF54  if (li8TestRank > mi8ProgressionRank):
//                             if (mi8ProgressionRank <= liTopRankIndex):
//                                 UnlockToProgressionRank(li8TestRank, lpGameActionQueue)
//                                 re-run GetTotalWinCount + the top-rank sum (the unlock CLEARS
//                                 medals, so every number below is the post-unlock one)
//                             if (mi8ProgressionRank > liTopRankIndex): the debug complaint
//   0x8239FF80..0x8239FF88  OnTrophyUnlock(22) -- UNCONDITIONAL, on every path
//   0x8239FF90..0x8239FFC8  the ":723 mi8CurrentProgressionRank >= 0" assert (`cmplwi 0x80`)
//   0x8239FF94..0x823A0010  the record: four halfwords at +0/+2/+4/+6
//   0x8239FFCC..0x823A0008  +6 == -1 when the rank has run off the end, else
//                           max(0, GetTotalWinsForNextRank() - luTotalWinsForCurrentRank)
//   0x823A001C              AddEvent(queue, &record, 200, 8)
//   0x823A0024              CheckForSpecialCarUnlocks()
//   0x823A0028..0x823A0034  mbPlayerMedalsUpdateRequired = false
// --------------------------------------------------------------------------------------------
void ProgressionManager::UpdatePlayerMedals(GsmIO::GameActionQueue* lpGameActionQueue)
{
    const s8 li8TopRankIndex =
        static_cast<s8>(GetProgressionData()->GetProgressionRankCount() - 1u);

    u32 luRankWin                = 0;
    u32 luNonRankWin             = 0;
    u32 luSpecialEventsWonBefore = 0;
    mProfile.GetTotalWinCount(luRankWin, luNonRankWin, luSpecialEventsWonBefore);

    u32 luTotalWinsForCurrentRank =
        (static_cast<s32>(mi8ProgressionRank) == static_cast<s32>(li8TopRankIndex))
            ? (luRankWin + luSpecialEventsWonBefore)
            : luRankWin;

    // The default is the cached byte itself (`lbz r4, 0(r30)` before the branch): a player whose
    // rank has already run off the end of the table is not re-derived from medals.
    s8 li8TestRank = mi8ProgressionRank;
    if (static_cast<s32>(mi8ProgressionRank) !=
        static_cast<s32>(GetProgressionData()->GetProgressionRankCount()))
    {
        li8TestRank = CalculateRankFromMedalTotal(luTotalWinsForCurrentRank);
    }

    if (static_cast<s32>(li8TestRank) > static_cast<s32>(mi8ProgressionRank))
    {
        bool lbProgressionAlreadyComplete =
            (static_cast<s32>(mi8ProgressionRank) > static_cast<s32>(li8TopRankIndex));

        if (!lbProgressionAlreadyComplete)
        {
            UnlockToProgressionRank(li8TestRank, lpGameActionQueue);

            // The unlock ran ClearMedalsOnRankUp, so every tally below has to be re-counted.
            mProfile.GetTotalWinCount(luRankWin, luNonRankWin, luSpecialEventsWonBefore);
            luTotalWinsForCurrentRank =
                (static_cast<s32>(mi8ProgressionRank) == static_cast<s32>(li8TopRankIndex))
                    ? (luRankWin + luSpecialEventsWonBefore)
                    : luRankWin;

            lbProgressionAlreadyComplete =
                (static_cast<s32>(mi8ProgressionRank) > static_cast<s32>(li8TopRankIndex));
        }

        if (lbProgressionAlreadyComplete &&
            (CgsDev::Message::gxMessageFilterFlags & 1) != 0 &&
            CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "***********************************!\n"
                   "Junction event progression already complete!\n"
                   "***********************************!\n";
        }
    }

    OnTrophyUnlock(KI_TROPHY_TYPE_MEDAL_TOTAL);

    // `cmplwi cr6, r11, 0x80 / blt` -- an UNSIGNED compare of the raw byte, i.e. "the signed byte
    // is not negative". The console fires this AFTER the unlock above, which is exactly why it
    // does not trip on the -2 boot seed.
    if (static_cast<u8>(mi8ProgressionRank) >= 0x80u)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mi8CurrentProgressionRank >= 0", KAC_PROGMGR_FILE, 723);
        CgsDev::Assert::EndAssert();
    }

    s16 li16WinsForNextRank;
    if (static_cast<s32>(mi8ProgressionRank) ==
        static_cast<s32>(GetProgressionData()->GetProgressionRankCount()))
    {
        li16WinsForNextRank = -1;                                       // `li r11, -1`
    }
    else
    {
        const s32 liWinsForNextRank =
            static_cast<s32>(GetTotalWinsForNextRank()) - static_cast<s32>(luTotalWinsForCurrentRank);
        li16WinsForNextRank = static_cast<s16>((liWinsForNextRank < 0) ? 0 : liWinsForNextRank);
    }

    GsmIO::UpdatePlayerMedalsAction lSendPlayerMedalAction;
    lSendPlayerMedalAction.mi16TotalWins      = static_cast<s16>(luRankWin);                // +0x00
    lSendPlayerMedalAction.mi16Field02        = static_cast<s16>(luNonRankWin);             // +0x02
    lSendPlayerMedalAction.mi16Field04        = static_cast<s16>(luSpecialEventsWonBefore); // +0x04
    lSendPlayerMedalAction.mi16WinsToNextRank = li16WinsForNextRank;                        // +0x06

    lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSendPlayerMedalAction),
                                GsmIO::E_ACTION_UPDATE_PLAYER_MEDALS,
                                static_cast<s32>(sizeof(lSendPlayerMedalAction)));   // li r5,0xC8 ; li r6,8

    CheckForSpecialCarUnlocks();

    mbPlayerMedalsUpdateRequired = false;                               // stbx 0 -> +0x20973

    // [FLAG PC witness] see MedalsDiagEnabled. `events=` is the profile's event-record count --
    // the number UnlockToProgressionRank(0)'s population produces and the number the
    // progression_medals case pins so the retired Prepare2 seam cannot have changed it.
    if (MedalsDiagEnabled())
    {
        static s32 siPrintedMedals = 0;
        if (siPrintedMedals < 16)
        {
            ++siPrintedMedals;
            *CgsDev::Log::gpDebugPrint
                << "[medals] total="   << static_cast<s32>(luTotalWinsForCurrentRank)
                << " rank="            << static_cast<s32>(mi8ProgressionRank)
                << " profileRank="     << static_cast<s32>(mProfile.GetCurrentProgressionRank())
                << " winsToNext="      << static_cast<s32>(li16WinsForNextRank)
                << " action200=1"
                << " events="          << static_cast<s32>(mProfile.GetEventCount())
                << "\n";
        }
    }
}

// --------------------------------------------------------------------------------------------
// ProgressionManager::DEBUG_ClearMedals  (X360 0x82366BF8)
// DWARF BrnProgressionManager.h:685 -- `void DEBUG_ClearMedals();`
// DWARF hints (:3175) name only the two request setters; the rest is inlined.
//
// The debug component's "start again from nothing" button (its four users are
// ProgressionDebugComponent::GetToOneWinBelowEliteLicenseBR / GetToOneWinBelowEliteLicense /
// GetToOneWinBelowBurnoutLicense / SkipToProgressionRank).
//   0x82366C08..0x82366C0C  Profile::DEBUG_ClearMedals()  (this+0x170)
//   0x82366C10..0x82366C3C  every rival record's state word (maRivals[i] +0x10, 0x38 stride,
//                           count Profile+0x274) back to 0 == RivalData::E_STATE_LOCKED
//   0x82366C60              mi8ProgressionRank = -2   -- the same "rank not set" seed
//                           Profile::Construct and ProgressionManager::Construct both write
//   0x82366C64/0x82366C68   RequestMedalUpdate() (+0x20973) / RequestUpdateRivals() (+0x20971)
// ⚠️ NO CALLER ON PC. ProgressionDebugComponent is not reconstructed, so this body exists to
// close the chain, not because anything reaches it. That is deliberate: it is four instructions
// of state and its absence is what would make the debug component's own reconstruction a partial.
// --------------------------------------------------------------------------------------------
void ProgressionManager::DEBUG_ClearMedals()
{
    mProfile.DEBUG_ClearMedals();

    for (s32 liRivalIndex = 0; liRivalIndex < mProfile.GetRivalCount(); ++liRivalIndex)
    {
        mProfile.GetRivalData(liRivalIndex)->meState = RivalData::E_STATE_LOCKED;
    }

    mi8ProgressionRank = -2;

    RequestMedalUpdate();
    RequestUpdateRivals();
}

// ============================================================================================
// ProgressionManager::UnlockToProgressionRankTail -- THE SHARED RANK TAIL (X360 0x8239E094..0x8239E1DC)
//
// [progression wave, lane `medals`] This is UnlockToProgressionRank's PARK Q4, landed. It lives
// here rather than in BrnProgressionManager.cpp because everything it needs
// (ClearMedalsOnRankUp) is in this partfile, and because the un-park at the call site is then a
// single line. It is NOT a console function -- the console falls into this code inline at the end
// of UnlockToProgressionRank -- so it is private and named for what it is.
//
//   0x8239E094..0x8239E0DC  write the CLAMPED rank back: the exact GetProgressionRank() ladder
//                           (raw byte >= 0x80 -> 0; else min(rank, rankCount - 1)), stored into
//                           mi8ProgressionRank.
//   0x8239E0E0..0x8239E12C  meLeastUsedCarType (+0x20968) = the index of the SMALLEST of the three
//                           Profile::mafCarTypes distances, seeded from FLT_MAX
//                           (flt_82029B70 == 0x7F7FFFFF) -- i.e. the car type the player has
//                           driven least. That is Profile::GetCurrentCarTypeWithMinDistance.
//   0x8239E130..0x8239E164  walk the cached rank up ONE AT A TIME to li8Rank, stopping early if it
//                           reaches the rank count. (Written as the console's loop, not as a
//                           min(): the loop re-reads the stored byte every pass.)
//   0x8239E168..0x8239E184  mirror it onto Profile::mi8CurrentProgressionRank, and at rank >= 5
//                           set Profile::mbHasUnlockedCredits (+0x1CD14).
//   0x8239E188..0x8239E19C  Profile::ResetCarTypeDistances()  (three `stw 0` over mafCarTypes)
//   0x8239E1A0              ClearMedalsOnRankUp()
//   0x8239E1A4..0x8239E1BC  Profile::ClearCurrentEventCompleteCounts() (18 words at Profile+0x108
//                           == maGameModeTypeAmountCompleted, `mtctr 0x12`)
//   0x8239E1C0..0x8239E1D0  mbHasJustRankedUp = true, but ONLY for a non-zero rank
//   0x8239E1D4..0x8239E1DC  a byte at this+0x207FF -- see the PARK below.
// ============================================================================================
void ProgressionManager::UnlockToProgressionRankTail(s8 li8Rank)
{
    // @0x8239E094..0x8239E0DC -- GetProgressionRank()'s ladder, open-coded by the console and
    // written back. Routed through the accessor: same value, same clamp, one owner.
    mi8ProgressionRank = static_cast<s8>(GetProgressionRank());

    // @0x8239E0E0..0x8239E12C
    meLeastUsedCarType = mProfile.GetCurrentCarTypeWithMinDistance();

    // @0x8239E130..0x8239E164
    while (static_cast<s32>(mi8ProgressionRank) < static_cast<s32>(li8Rank))
    {
        mi8ProgressionRank = static_cast<s8>(mi8ProgressionRank + 1);
        if (static_cast<s32>(mi8ProgressionRank) ==
            static_cast<s32>(GetProgressionData()->GetProgressionRankCount()))
        {
            break;
        }
    }

    // @0x8239E168..0x8239E184
    mProfile.SetCurrentProgressionRank(mi8ProgressionRank);
    if (static_cast<s32>(mi8ProgressionRank) >= KI_RANK_THAT_UNLOCKS_CREDITS)
    {
        mProfile.SetHasUnlockedCredits(true);
    }

    // @0x8239E188..0x8239E1BC
    mProfile.ResetCarTypeDistances();
    ClearMedalsOnRankUp();
    mProfile.ClearCurrentEventCompleteCounts();

    // @0x8239E1C0..0x8239E1D0 -- `cmpwi r21, 0 / beq` on the RANK ARGUMENT, so the rank-0
    // starting unlock is not a "you just ranked up".
    if (li8Rank != 0)
    {
        mbHasJustRankedUp = true;
    }

    // ⛔ PARK -- console: `stbx 1, this, 0x207FF` @0x8239E1DC.
    // WHAT THAT BYTE IS. mDebugComponent sits at this+133000 and mQueueOfTrophyCarUnLocks at
    // this+133128, so the store lands at mDebugComponent + 119 -- the LAST byte of
    // ProgressionDebugComponent, which the DecFIGS DWARF ends with eight adjacent bools
    // (mbShowProfile .. mbProfileReadyForDisplay at +112..+119) and for which it declares
    // `void SetProfileReadyForDisplay();`. So the console line is
    // `mDebugComponent.SetProfileReadyForDisplay();`.
    // WHY IT IS PARKED: this tree models mDebugComponent as an opaque DebugComponentSlot
    // (a vtable pointer plus 60 unmodelled bytes), so the flag has no name to reach, and
    // ProgressionDebugComponent is not reconstructed, registered or rendered on PC -- the flag
    // gates a debug HUD readout and nothing else. Poking byte 119 of an unmodelled blob to
    // reproduce a debug-overlay gate is exactly the raw-offset trap this campaign bans.
    // It is the ONLY reference to that offset in the whole XEX (verified by scanning every
    // export for 0x207FF / 133119), so nothing else observes it either.
    // DELETE-WHEN ProgressionDebugComponent has a reconstructed layout in this header.
    ParkOnce(gbSaidProfileReadyForDisplay,
             "[FLAG PC bring-up] ProgressionManager::UnlockToProgressionRank tail: "
             "mDebugComponent.SetProfileReadyForDisplay() (this+0x207FF) is NOT reproduced -- "
             "ProgressionDebugComponent's layout is not modelled and the component is not "
             "constructed on PC. Debug-overlay only.\n");
}

// ============================================================================================
// ProgressionManager::UnlockToProgressionRankLicenceUpgrade -- PARK Q3, landed
// (X360 0x8239E034..0x8239E090, the RANK-N arm of UnlockToProgressionRank).
//
// [progression wave, lane `medals`] Q3's stated blocker was "AchievementManagerBase::
// OnLicenseUpgrade @0x8235ADC8 lives in a TU that is not mounted". THAT IS NO LONGER TRUE:
// BrnGameStateAchievementManagerBase.cpp is mounted (tools/build/build_game_exe.bat, next to the
// X360 leaf; the rem block above it still argues for the old state and is stale), OnLicenseUpgrade
// is bodied there, and BrnNetworkSharedIO_Telemetry.cpp -- which owns TelemetryData::Construct and
// AddParameter(const char*) -- is mounted too. So the arm is landed.
//
//   0x8239E034..0x8239E044  mpAchievementManager (+0x20938)->OnLicenseUpgrade(li8Rank)  -- the RAW
//                           byte argument (`mr r4, r31`), which the callee takes as a uint8_t.
//   0x8239E048..0x8239E04C  `cmplwi r30, 0 / beq` -- the whole telemetry post is skipped when the
//                           GameActionQueue argument is null. THIS build's other caller
//                           (UpdatePlayerMedals) always passes a real one.
//   0x8239E050..0x8239E070  TelemetryData::Construct(22 == E_TELEMETRY_EVENT_EARNED_LICENCE)
//                           inlined (`stw 0x16, var_80` + `stb 0, var_7C`), and
//                           CgsCore::SPrintf(buf, 16, "%i", (s32)li8Rank)
//   0x8239E07C              TelemetryData::AddParameter(buf)
//   0x8239E080..0x8239E090  AddEvent(queue, &record, 228, 20)
// ============================================================================================
void ProgressionManager::UnlockToProgressionRankLicenceUpgrade(
        s8 li8Rank, GsmIO::GameActionQueue* lpGameActionQueue)
{
    if (mpAchievementManager != 0)
    {
        mpAchievementManager->OnLicenseUpgrade(static_cast<u8>(li8Rank));
    }
    else
    {
        // [FLAG PC bring-up] the console dereferences +0x20938 unguarded; Prepare2 installs it,
        // and a null one on a PC boot path would be an access violation rather than a missing
        // achievement. DELETE-WHEN ProgressionManager::Construct/Prepare2 always run first.
        ParkOnce(gbSaidNoAchievementManager,
                 "[FLAG PC bring-up] ProgressionManager::UnlockToProgressionRank(N): "
                 "mpAchievementManager (X360 +133432) is NULL -- the licence-upgrade achievement "
                 "was not awarded.\n");
    }

    if (lpGameActionQueue == 0)
    {
        return;                                                         // `cmplwi r30, 0 / beq`
    }

    char lacRankText[16];                                               // li r4, 0x10
    CgsCore::SPrintf(lacRankText, sizeof(lacRankText), "%i", static_cast<s32>(li8Rank));

    BrnNetwork::BrnNetworkModuleIO::TelemetryData lTelemetry;
    lTelemetry.Construct(BrnNetwork::E_TELEMETRY_EVENT_EARNED_LICENCE);  // li r11, 0x16
    lTelemetry.AddParameter(lacRankText);

    lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lTelemetry),
                                GsmIO::E_ACTION_SEND_TELEMETRY,
                                static_cast<s32>(sizeof(lTelemetry)));   // li r5,0xE4 ; li r6,0x14
}

}   // namespace BrnProgression
