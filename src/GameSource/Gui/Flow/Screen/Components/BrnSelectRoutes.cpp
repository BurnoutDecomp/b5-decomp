// ===================================================================================
// BrnGui::SelectRoutes -- out-of-line bodies reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// This slice covers these of the component's functions:
//   GetCurrentlySelectedCheckpointIndex     @ 0x82418B90
//   GetCheckpointLandmark                   @ 0x824837B8
//   GetCurrentlySelectedCheckpointLandmark  @ 0x824899E8
//   SetRouteFromPresetEvent
//   GenerateRandomLandmark / GenerateRandomFinishPoint / GenerateRandomStartPoint
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/Components/BrnSelectRoutes.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/Gui/BrnGuiCache.h"              // GuiCache (preset events, landmark info, online game mode)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"      // GuiEventUpdateSatNav::SatNavIconInfo
#include "GameSource/GameState/BrnGameStateSharedIO.h"   // SpecificGameModeEventInterface::Event
#include "GameSource/Gui/BrnGuiWorldDataController.h"  // WorldDataController::GetTotalNumberOfOnlineLandmarks
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"   // the selected-candidate bit sets
#include "GameShared/GameClasses/Numeric/CgsRandom.h"            // the local generator

#include <cmath>   // sqrtf (the candidate distance test)

namespace BrnGui
{
    // ---- GetCurrentlySelectedCheckpointIndex @ 0x82418B90 -------------------------
    // Absolute menu index = first-checkpoint item + highlighted row; 0 when unselected.
    s32 SelectRoutes::GetCurrentlySelectedCheckpointIndex() const
    {
        if (mMenuOptions.GetHighlightedIndex() == -1)
            return 0;

        return miStartItem + mMenuOptions.GetHighlightedIndex();
    }

    // ---- GetCheckpointLandmark @ 0x824837B8 ---------------------------------------
    BrnGameState::LandmarkIndex SelectRoutes::GetCheckpointLandmark(s32 liRoundNumber,
                                                                    s32 liCheckpointIndex) const
    {
        CGS_ASSERT(liCheckpointIndex > 0, "liCheckpointIndex > 0");
        CGS_ASSERT(liCheckpointIndex < KI_MAX_CHECKPOINTS, "liCheckpointIndex < KI_MAX_CHECKPOINTS");
        CGS_ASSERT(liCheckpointIndex < maiNumCheckpoints[liRoundNumber],
                   "liCheckpointIndex < maiNumCheckpoints[ liRoundNumber ]");

        return maCheckpointData[liCheckpointIndex].maLandmarkIndex[liRoundNumber];
    }

    // ---- GetCurrentlySelectedCheckpointLandmark @ 0x824899E8 ----------------------
    // A checkpoint row reports its own checkpoint. The finish-point row reports the current
    // checkpoint when the round is full, otherwise the previous one; any other row has none.
    BrnGameState::LandmarkIndex
    SelectRoutes::GetCurrentlySelectedCheckpointLandmark(s32 liRoundNumber) const
    {
        bool lbUsePreviousCheckpoint = false;

        if (GetMenuItemType(GetCurrentlySelectedCheckpointIndex()) != E_MENU_ITEM_TYPE_CHECKPOINT)
        {
            if (GetMenuItemType(GetCurrentlySelectedCheckpointIndex()) != E_MENU_ITEM_TYPE_FINISH_POINT)
                return BrnGameState::LandmarkIndex(0);

            // The fullness test indexes maiNumCheckpoints by the miCurrentRound MEMBER
            // (asm lwz r11,0x190C(r31)), not the incoming round param.
            if (maiNumCheckpoints[miCurrentRound] < KI_MAX_CHECKPOINTS)
                lbUsePreviousCheckpoint = true;
        }

        const s32 liCheckpoint = GetCurrentlySelectedCheckpointIndex();

        return lbUsePreviousCheckpoint
                   ? GetCheckpointLandmark(liRoundNumber, liCheckpoint - 1)
                   : GetCheckpointLandmark(liRoundNumber, liCheckpoint);
    }

    // ---- SetRouteFromPresetEvent --------------------------------------------------
    // Take the preset event's route for the current round: one checkpoint per landmark plus
    // the start. Checkpoint 0 is the start (its traffic-light trigger and that event start's
    // county); every other checkpoint takes the event's landmark and the landmark's county.
    // In online Road Rage the next round gets the same checkpoint count and counties (the
    // landmarks themselves are only written for the current round).
    void SelectRoutes::SetRouteFromPresetEvent(s32 liPresetEventID)
    {
        const BrnGameState::GameStateModuleIO::SpecificGameModeEventInterface::Event* lpEvent =
            mpGuiCache->GetPresetEventFromEventID(liPresetEventID);
        const s32 liRound = miCurrentRound;

        maiNumCheckpoints[liRound] = lpEvent->GetNumLandmarks() + 1;
        if (mpGuiCache->GetOnlineGameMode() == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE)
        {
            maiNumCheckpoints[liRound + 1] = lpEvent->GetNumLandmarks() + 1;
        }

        for (s32 liCheckpoint = 0; liCheckpoint < maiNumCheckpoints[liRound]; ++liCheckpoint)
        {
            CheckpointData& lrCheckpoint = maCheckpointData[liCheckpoint];

            if (liCheckpoint == 0)
            {
                lrCheckpoint.maStartLightTriggerID[liRound] = lpEvent->GetTrafficLightTriggerId();

                const SatNavEventDisplayInfo* lpEventStart =
                    mpGuiCache->GetPresetEventDisplayInfo(lrCheckpoint.maStartLightTriggerID[liRound]);
                CGS_ASSERT(lpEventStart, "lpEventStart");

                lrCheckpoint.maeSelectedCounty[liRound] = static_cast<BrnWorld::ECounty>(lpEventStart->muCounty);
                if (mpGuiCache->GetOnlineGameMode() == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE)
                {
                    lrCheckpoint.maeSelectedCounty[liRound + 1] = static_cast<BrnWorld::ECounty>(lpEventStart->muCounty);
                }
            }
            else
            {
                lrCheckpoint.maLandmarkIndex[liRound] = lpEvent->GetLandmark(liCheckpoint - 1);

                GuiEventUpdateSatNav::SatNavIconInfo lLandmarkInfo;
                mpGuiCache->GetLandmarkInfoFromIndex(lrCheckpoint.maLandmarkIndex[liRound], &lLandmarkInfo);

                lrCheckpoint.maeSelectedCounty[liRound] = lLandmarkInfo.GetCounty();
                if (mpGuiCache->GetOnlineGameMode() == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE)
                {
                    lrCheckpoint.maeSelectedCounty[liRound + 1] = lLandmarkInfo.GetCounty();
                }
            }
        }
    }

    namespace
    {
        // Candidates for the first checkpoint (or, for the start point, the start) must lie at
        // least this far from the other end of that first leg.
        const f32 KF_MIN_FIRST_LEG_DISTANCE = 200.0f;

        // The candidate bit sets: one bit per event start / per online landmark slot.
        const u32 KU_MAX_EVENT_STARTS      = 175;
        const u32 KU_MAX_LANDMARK_SLOTS    = 512;

        // The generators are reseeded from the GUI clock, in hundredths of a second.
        const f32 KF_SEED_TIME_SCALE = 100.0f;

        // Fresh generator: the default construction, then the clock seed. The float-to-integer
        // conversion is 64-bit and only its low word is kept.
        void SeedFromGuiClock(CgsNumeric::Random& lrRandom, const GuiCache* lpGuiCache)
        {
            lrRandom.Construct();
            const s64 liTicks = static_cast<s64>(lpGuiCache->GetTime() * KF_SEED_TIME_SCALE);
            lrRandom.SetSeed(static_cast<u64>(static_cast<u32>(liTicks)));
        }

        // Three-lane distance (the height axis takes part).
        template <typename TA, typename TB>
        f32 Distance3(const TA& lrA, const TB& lrB)
        {
            const f32 lfX = lrA.x - lrB.x;
            const f32 lfY = lrA.y - lrB.y;
            const f32 lfZ = lrA.z - lrB.z;
            return sqrtf(lfX * lfX + lfY * lfY + lfZ * lfZ);
        }
    }

    // ---- GenerateRandomLandmark ---------------------------------------------------
    // Gather every online landmark in the county (E_COUNTY_COUNT == anywhere) that is not
    // already the previous or the next checkpoint of the round and, for the first checkpoint,
    // lies far enough from the round's start; then pick one of them at random. The generator is
    // seeded from the GUI clock and then advanced once per round number, so successive rounds
    // pick differently.
    BrnGameState::LandmarkIndex SelectRoutes::GenerateRandomLandmark(s32 liRoundNumber,
                                                                     s32 liCheckpointItem,
                                                                     BrnWorld::ECounty leCounty)
    {
        const SatNavEventDisplayInfo* lpEventStart =
            mpGuiCache->GetPresetEventDisplayInfo(maCheckpointData[0].maStartLightTriggerID[miCurrentRound]);
        CGS_ASSERT(lpEventStart, "lpEventStart");
        const Vector3 lv3StartPosition = lpEventStart->mv3Position;

        CGS_ASSERT(liCheckpointItem > 0, "liCheckpointItem > 0");

        BrnGameState::LandmarkIndex lLandmark(-1);

        CGS_ASSERT(mpGuiCache, "mpGuiCache");
        CgsNumeric::Random lRandom;
        SeedFromGuiClock(lRandom, mpGuiCache);

        const s32 liNumLandmarks = mpGuiCache->GetWorldDataController()->GetTotalNumberOfOnlineLandmarks();

        CgsContainers::FastBitArray<KU_MAX_LANDMARK_SLOTS> lSelectedLandmarks;
        lSelectedLandmarks.Construct();

        for (s32 liDraw = liRoundNumber; liDraw > 0; --liDraw)
        {
            lRandom.RandomInt(0, liNumLandmarks - 1);
        }

        s32 liNumSelected = 0;
        s32 liLandmarkSlot = 0;
        for (s32 liIndex = 0; liIndex < liNumLandmarks; ++liIndex)
        {
            GuiEventUpdateSatNav::SatNavIconInfo lLandmarkInfo;
            mpGuiCache->GetOnlineLandmarkInfoAtPositionInList(liIndex, &lLandmarkInfo);

            if (lLandmarkInfo.GetIconType() != GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_LANDMARK)
            {
                continue;
            }

            const BrnWorld::ECounty leLandmarkCounty = lLandmarkInfo.GetCounty();
            if (leLandmarkCounty == leCounty || leCounty == BrnWorld::E_COUNTY_COUNT)
            {
                const s32 liLandmarkIndex = lLandmarkInfo.GetLandmarkIndexHalf();

                const bool lbIsPreviousCheckpoint =
                    liCheckpointItem != 1 &&
                    static_cast<s32>(maCheckpointData[liCheckpointItem - 1].maLandmarkIndex[liRoundNumber]) == liLandmarkIndex;
                const bool lbIsNextCheckpoint =
                    !lbIsPreviousCheckpoint &&
                    liCheckpointItem != maiNumCheckpoints[miCurrentRound] - 1 &&
                    static_cast<s32>(maCheckpointData[liCheckpointItem + 1].maLandmarkIndex[liRoundNumber]) == liLandmarkIndex;

                if (!lbIsPreviousCheckpoint && !lbIsNextCheckpoint &&
                    (liCheckpointItem > 1 ||
                     Distance3(lLandmarkInfo.GetPositionLane(), lv3StartPosition) >= KF_MIN_FIRST_LEG_DISTANCE))
                {
                    ++liNumSelected;
                    lSelectedLandmarks.SetBit(static_cast<u32>(liLandmarkSlot));
                }
            }

            ++liLandmarkSlot;
        }

        CGS_ASSERT(!lSelectedLandmarks.IsZero(), "!lSelectedLandmarks.IsZero()");

        const s32 liPick = lRandom.RandomInt(0, liNumSelected - 1);

        s32 liCandidate = 0;
        for (CgsContainers::FastBitArray<KU_MAX_LANDMARK_SLOTS>::Iterator lIt = lSelectedLandmarks.Begin();
             lIt != lSelectedLandmarks.End();
             ++lIt)
        {
            if (liCandidate == liPick)
            {
                GuiEventUpdateSatNav::SatNavIconInfo lLandmarkInfo;
                mpGuiCache->GetOnlineLandmarkInfoAtPositionInList(lIt.GetIndex(), &lLandmarkInfo);
                lLandmark = BrnGameState::LandmarkIndex(lLandmarkInfo.GetLandmarkIndexHalf());
                break;
            }
            ++liCandidate;
        }

        return lLandmark;
    }

    // ---- GenerateRandomFinishPoint ------------------------------------------------
    // GenerateRandomLandmark over the online finish points instead of the online landmarks.
    BrnGameState::LandmarkIndex SelectRoutes::GenerateRandomFinishPoint(s32 liRoundNumber,
                                                                        s32 liCheckpointItem,
                                                                        BrnWorld::ECounty leCounty)
    {
        const SatNavEventDisplayInfo* lpEventStart =
            mpGuiCache->GetPresetEventDisplayInfo(maCheckpointData[0].maStartLightTriggerID[miCurrentRound]);
        CGS_ASSERT(lpEventStart, "lpEventStart");
        const Vector3 lv3StartPosition = lpEventStart->mv3Position;

        CGS_ASSERT(liCheckpointItem > 0, "liCheckpointItem > 0");

        BrnGameState::LandmarkIndex lLandmark(-1);

        CGS_ASSERT(mpGuiCache, "mpGuiCache");
        CgsNumeric::Random lRandom;
        SeedFromGuiClock(lRandom, mpGuiCache);

        const s32 liNumFinishPoints = static_cast<s32>(mpGuiCache->GetNumOnlineFinishPoints());

        CgsContainers::FastBitArray<KU_MAX_LANDMARK_SLOTS> lSelectedLandmarks;
        lSelectedLandmarks.Construct();

        for (s32 liDraw = liRoundNumber; liDraw > 0; --liDraw)
        {
            lRandom.RandomInt(0, liNumFinishPoints - 1);
        }

        s32 liNumSelected = 0;
        s32 liLandmarkSlot = 0;
        for (s32 liIndex = 0; liIndex < liNumFinishPoints; ++liIndex)
        {
            GuiEventUpdateSatNav::SatNavIconInfo lFinishInfo;
            mpGuiCache->GetOnlineFinishPoint(liIndex, &lFinishInfo);

            if (lFinishInfo.GetIconType() != GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_LANDMARK)
            {
                continue;
            }

            const BrnWorld::ECounty leFinishCounty = lFinishInfo.GetCounty();
            if (leFinishCounty == leCounty || leCounty == BrnWorld::E_COUNTY_COUNT)
            {
                const s32 liLandmarkIndex = lFinishInfo.GetLandmarkIndexHalf();

                const bool lbIsPreviousCheckpoint =
                    liCheckpointItem != 1 &&
                    static_cast<s32>(maCheckpointData[liCheckpointItem - 1].maLandmarkIndex[liRoundNumber]) == liLandmarkIndex;
                const bool lbIsNextCheckpoint =
                    !lbIsPreviousCheckpoint &&
                    liCheckpointItem != maiNumCheckpoints[miCurrentRound] - 1 &&
                    static_cast<s32>(maCheckpointData[liCheckpointItem + 1].maLandmarkIndex[liRoundNumber]) == liLandmarkIndex;

                if (!lbIsPreviousCheckpoint && !lbIsNextCheckpoint &&
                    (liCheckpointItem > 1 ||
                     Distance3(lFinishInfo.GetPositionLane(), lv3StartPosition) >= KF_MIN_FIRST_LEG_DISTANCE))
                {
                    ++liNumSelected;
                    lSelectedLandmarks.SetBit(static_cast<u32>(liLandmarkSlot));
                }
            }

            ++liLandmarkSlot;
        }

        CGS_ASSERT(!lSelectedLandmarks.IsZero(), "!lSelectedLandmarks.IsZero()");

        const s32 liPick = lRandom.RandomInt(0, liNumSelected - 1);

        s32 liCandidate = 0;
        for (CgsContainers::FastBitArray<KU_MAX_LANDMARK_SLOTS>::Iterator lIt = lSelectedLandmarks.Begin();
             lIt != lSelectedLandmarks.End();
             ++lIt)
        {
            if (liCandidate == liPick)
            {
                GuiEventUpdateSatNav::SatNavIconInfo lFinishInfo;
                mpGuiCache->GetOnlineFinishPoint(lIt.GetIndex(), &lFinishInfo);
                lLandmark = BrnGameState::LandmarkIndex(lFinishInfo.GetLandmarkIndexHalf());
                break;
            }
            ++liCandidate;
        }

        return lLandmark;
    }

    // ---- GenerateRandomStartPoint -------------------------------------------------
    // Gather every event start in the county (E_COUNTY_COUNT == anywhere) at least the
    // first-leg distance away from the round's first checkpoint, pick one at random (same
    // clock seed and per-round advance as the landmark pickers) and return its traffic-light
    // trigger id.
    u32 SelectRoutes::GenerateRandomStartPoint(s32 liRoundNumber, BrnWorld::ECounty leCounty)
    {
        u32 luStartLightTriggerID = static_cast<u32>(-1);

        CGS_ASSERT(mpGuiCache, "mpGuiCache");

        GuiEventUpdateSatNav::SatNavIconInfo lFirstCheckpointInfo;
        mpGuiCache->GetLandmarkInfoFromIndex(maCheckpointData[1].maLandmarkIndex[miCurrentRound],
                                             &lFirstCheckpointInfo);
        const Vector4 lv4FirstCheckpoint = lFirstCheckpointInfo.GetPositionLane();

        CgsNumeric::Random lRandom;
        SeedFromGuiClock(lRandom, mpGuiCache);

        const u32 luNumEventStarts = mpGuiCache->GetNumEventStarts();

        CgsContainers::FastBitArray<KU_MAX_EVENT_STARTS> lSelectedEvents;
        lSelectedEvents.Construct();

        for (s32 liDraw = liRoundNumber; liDraw > 0; --liDraw)
        {
            lRandom.RandomUInt(0, luNumEventStarts - 1);
        }

        u32 luNumSelected = 0;
        for (u32 luIndex = 0; luIndex < luNumEventStarts; ++luIndex)
        {
            const SatNavEventDisplayInfo* lpEventStart = mpGuiCache->GetEventStart(luIndex);
            CGS_ASSERT(lpEventStart, "lpEventStart");

            if ((static_cast<s32>(lpEventStart->muCounty) == static_cast<s32>(leCounty) ||
                 leCounty == BrnWorld::E_COUNTY_COUNT) &&
                Distance3(lpEventStart->mv3Position, lv4FirstCheckpoint) >= KF_MIN_FIRST_LEG_DISTANCE)
            {
                ++luNumSelected;
                lSelectedEvents.SetBit(luIndex);
            }
        }

        CGS_ASSERT(!lSelectedEvents.IsZero(), "!lSelectedEvents.IsZero()");

        const u32 luPick = lRandom.RandomUInt(0, luNumSelected - 1);

        u32 luCandidate = 0;
        for (CgsContainers::FastBitArray<KU_MAX_EVENT_STARTS>::Iterator lIt = lSelectedEvents.Begin();
             lIt != lSelectedEvents.End();
             ++lIt)
        {
            if (luCandidate == luPick)
            {
                const SatNavEventDisplayInfo* lpEventStart =
                    mpGuiCache->GetEventStart(static_cast<u32>(lIt.GetIndex()));
                CGS_ASSERT(lpEventStart, "lpEventStart");
                luStartLightTriggerID = lpEventStart->muLightTriggerId;
                break;
            }
            ++luCandidate;
        }

        return luStartLightTriggerID;
    }
}
