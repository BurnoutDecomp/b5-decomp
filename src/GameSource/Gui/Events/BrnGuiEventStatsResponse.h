// BrnGuiEventStatsResponse.h
// Home of BrnGui::GuiEventStatsResponse -- the GUI event payload carrying the player's whole
// game-stats block (GUI event 436, 432 bytes).
//
// ⭐⭐⭐ [pause-stats wave 2026-08-29] THIS RECORD WAS "OPAQUE" AND IT WAS NOT. It stood in
// BrnGuiDemangledEventTypes.h as `GuiEvent<436> { u8 maPayload[420]; }`, and both of its
// consumers (CrashNavDriverDetails::HandleStatData, CrashNavStats) read it through file-local
// byte cursors. The DecFIGS DWARF names EVERY field
// (BrnGuiEventTypeDefs.h:4950..:5056, `struct GuiEventStatsResponse : GuiEvent<431>`), and the
// X360 layout is that member list with ONE change: the three CgsIDs are hoisted to the FRONT.
//
// THE HOIST IS PROVEN, NOT ASSUMED. Take the DWARF order verbatim and the three 8-byte ids land
// at +0x98/+0xA0/+0xA8 and the six district arrays at +0x138..+0x1B0 (sizeof 0x1B0). Take them
// at the front and the ids land at +0x00/+0x08/+0x10 and the arrays at +0x134..+0x1AC (sizeof
// still 0x1B0, the CgsIDs forcing 8-byte alignment). The producer settles it three times over --
// BrnGameModule::TranslateGameActionsToGuiEvents case 180 @0x823EC8A0:
//   * opens with THREE `ld`/`std` pairs writing the record at +0x00, +0x08 and +0x10;
//   * computes its six district cursors as event+0x134 / +0x148 / +0x15C / +0x170 / +0x184 /
//     +0x198 (`subf rN, r31, <local>` then `stwx rM, rN, r28` with r28 walking
//     GameStats+0x120+4i) -- and the SOURCE of each is the GameStats grid slot whose DWARF name
//     matches the destination's DWARF name at that offset, all six of them
//     (current[BILLBOARD]->mBillboardStunts, current[JUMP]->mJumpStunts,
//      current[SMASH]->mSmashStunts, max[BILLBOARD]->mMaxBillboardStunts,
//      max[JUMP]->mMaxJumpStunts, max[SMASH]->mMaxSmashStunts);
//   * writes +0xF4 and +0xF8 with `stfs` (the only two float stores in the arm) -- exactly where
//     the front-hoisted order puts mfBestAirtime / mfBestSpin, and it sources them from
//     GameStats' maFloatValues[BEST_AIRTIME] / [BEST_SPIN].
// And the CONSUMER agrees independently: HandleStatData @0x824B8618 reads its "percentage" field
// at +0xC4 (== miPercentageComplete here) and formats it E_FORMAT_PERCENTAGE, reads +0x20 and
// formats it E_FORMAT_HOURS_MINUTES_SECONDS (== miTimePlayed), and walks its district cursor
// from +0x134 with the same +20/+40/+60/+80/+100 strides.
//
// ⓘ THE EVENT ID IS THE X360's, NOT THE DWARF's: the PS3 DWARF declares
// `GuiEventStatsResponse : GuiEvent<431>`, while AddGuiEvent<GuiEventStatsResponse> @0x823D71D8
// posts `AddEvent(queue, &event, 436, 432)` -- and 436 is what CrashNavDriverDetails'
// maiEventToObserve table (.rdata @0x820664F0) registers. Same +5 GUI-event drift the rest of
// this band carries (438 vs the DWARF's 433 for the rank response next door).
//
// ⓘ NO GuiEvent<N> HEADER ON THE WIRE. AddGuiEvent queues the object from offset 0 at its full
// 432 bytes and the producer's first store is a data field, so the record derives from the empty
// CgsModule::Event exactly as GuiEventRankProgressResponse does.

#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                      // CgsID (u64)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::Event base

namespace BrnGui
{
    // The game-stats response GUI event. Member NAMES and ORDER are the DecFIGS DWARF's
    // (BrnGuiEventTypeDefs.h:4950..:5056); the three CgsIDs' POSITION and every byte offset are
    // the X360 producer's and consumer's, and they agree (see the banner).
    struct GuiEventStatsResponse : public CgsModule::Event
    {
        // AddGuiEvent<GuiEventStatsResponse> @0x823D71D8 -> AddEvent(&event, 436, 432).
        s32 GetEventType() const { return 436; }

        enum { KI_NUM_DISTRICTS = 5 };   // BrnWorld::E_COUNTY_VALID_COUNT

        // ---- the three car/rival ids (DWARF :5000..:5003, hoisted to the front on X360) ----
        CgsID mFaveCarId;                              // +0x000
        CgsID mForgottenCarId;                         // +0x008
        CgsID mGreatestRivalId;                        // +0x010

        s32 miDistanceOnline;                          // +0x018
        s32 miDistanceOffline;                         // +0x01C
        s32 miTimePlayed;                              // +0x020
        s32 miCarsCollected;                           // +0x024
        s32 miCarsTotal;                               // +0x028
        s32 miPowerParkingBest;                        // +0x02C
        s32 miPowerParkingBest_BetweenOtherPlayers;    // +0x030  (sic -- DWARF spelling)
        s32 miAllMedalsEarned;                         // +0x034
        s32 miAllMedalsTotal;                          // +0x038
        s32 miEventMedalsEarned;                       // +0x03C
        s32 miEventMedalsTotal;                        // +0x040
        s32 miRoadRuleMedalsEarned;                    // +0x044
        s32 miRoadRuleMedalsTotal;                     // +0x048
        s32 miRoadRules;                               // +0x04C
        s32 mRoadsRuledTotal;                          // +0x050
        s32 miDrivers;                                 // +0x054
        s32 miDriversTot;                              // +0x058
        s32 miGolds;                                   // +0x05C
        s32 miSilvers;                                 // +0x060
        s32 miBronzes;                                 // +0x064
        s32 miJumps;                                   // +0x068
        s32 miJumpTot;                                 // +0x06C
        s32 miSmashes;                                 // +0x070
        s32 miSmashTot;                                // +0x074
        s32 miStunts;                                  // +0x078  (this build: BILLBOARDS)
        s32 miStuntTot;                                // +0x07C
        s32 miSignatureTDs;                            // +0x080
        s32 miSignatureTDsTot;                         // +0x084
        s32 miTotalTakedowns;                          // +0x088
        s32 miStandardTakedowns;                       // +0x08C
        s32 miVerticalTakedowns;                       // +0x090
        s32 miTBoneTakedowns;                          // +0x094
        s32 miAftertouchTakedowns;                     // +0x098
        s32 miCarTakedowns;                            // +0x09C
        s32 miVanTakedowns;                            // +0x0A0
        s32 miBusTakedowns;                            // +0x0A4
        s32 miBigRigTakedowns;                         // +0x0A8
        s32 mRoadsRuledTime;                           // +0x0AC
        s32 mRoadsRuledCrash;                          // +0x0B0
        s32 mRoadsRuledComplete;                       // +0x0B4
        s32 mNumberOfRoads;                            // +0x0B8
        s32 miWinsToNextRank;                          // +0x0BC
        s32 miCarsToShutdown;                          // +0x0C0
        s32 miPercentageComplete;                      // +0x0C4
        s32 miDriveThrusFound;                         // +0x0C8
        s32 miRacesWon;                                // +0x0CC
        s32 miRoadRagesWon;                            // +0x0D0
        s32 miMarkedManWon;                            // +0x0D4
        s32 miChallengesWon;                           // +0x0D8  (this build: BURNING ROUTES)
        s32 miStuntRunsWon;                            // +0x0DC
        s32 miBestShowtime;                            // +0x0E0
        s32 miBestRoadRageTakedownCount;               // +0x0E4
        s32 miBestBoostChain;                          // +0x0E8
        s32 miBestDrift;                               // +0x0EC
        s32 miBestOncoming;                            // +0x0F0
        f32 mfBestAirtime;                             // +0x0F4  (`stfs` -- a REAL float)
        f32 mfBestSpin;                                // +0x0F8  (`stfs` -- a REAL float)
        s32 miBestNumBarrelRolls;                      // +0x0FC
        s32 miHighestStuntScore;                       // +0x100
        s32 miEventsFound;                             // +0x104
        s32 miTotalEvents;                             // +0x108
        s32 miBodyShopsFound;                          // +0x10C
        s32 miGasStationsFound;                        // +0x110
        s32 miPaintShopsFound;                         // +0x114
        s32 miJunkYardsFound;                          // +0x118
        s32 miBodyShopsTotal;                          // +0x11C
        s32 miGasStationsTotal;                        // +0x120
        s32 miPaintShopsTotal;                         // +0x124
        s32 miJunkYardsTotal;                          // +0x128
        s32 miTotalDriveThrus;                         // +0x12C
        s32 miTotalDriveThrusFound;                    // +0x130

        // The six per-district columns the panel's three "x of y" rows are built from.
        s32 mBillboardStunts[KI_NUM_DISTRICTS];        // +0x134
        s32 mJumpStunts[KI_NUM_DISTRICTS];             // +0x148
        s32 mSmashStunts[KI_NUM_DISTRICTS];            // +0x15C
        s32 mMaxBillboardStunts[KI_NUM_DISTRICTS];     // +0x170
        s32 mMaxJumpStunts[KI_NUM_DISTRICTS];          // +0x184
        s32 mMaxSmashStunts[KI_NUM_DISTRICTS];         // +0x198
        // -> the last field ends at +0x1AC; the CgsIDs give the record 8-byte alignment, so
        //    sizeof is 0x1B0 == 432, which is what AddGuiEvent posts. The four trailing pad
        //    bytes are never written by the producer or read by either consumer.
    };

    // AddGuiEvent<GuiEventStatsResponse> @0x823D71D8: `li r6, 0x1B0 / li r5, 0x1B4`.
    static_assert(sizeof(GuiEventStatsResponse) == 432,
                  "X360 AddGuiEvent<GuiEventStatsResponse> posts 432 bytes (id 436)");
}
