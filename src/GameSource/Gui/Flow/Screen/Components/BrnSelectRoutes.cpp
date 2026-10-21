// ===================================================================================
// BrnGui::SelectRoutes -- out-of-line bodies reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Route setup, scrolling, checkpoint editing, round accessors and APT lifecycle.
// The previously recovered getters, preset import and random generators are retained.
// ARTIST pseudocode/assembly defines behavior; DecFIGS defines declaration shape.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/Components/BrnSelectRoutes.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/Gui/BrnGuiCache.h"              // GuiCache (preset events, landmark info, online game mode)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"      // GuiEventUpdateSatNav::SatNavIconInfo
#include "GameSource/GameState/BrnGameStateSharedIO.h"   // SpecificGameModeEventInterface::Event
#include "GameSource/Gui/BrnGuiWorldDataController.h"  // WorldDataController::GetTotalNumberOfOnlineLandmarks
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"   // the selected-candidate bit sets
#include "GameShared/GameClasses/Numeric/CgsRandom.h"            // the local generator

#include "GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include <cstring>

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

    namespace
    {
        // ARTIST OutputGuiEvent<GuiAudioTriggerEvent> @82436890 posts this exact
        // wrapper on channel 40. The homed GuiAudioTriggerEvent has a legacy PC
        // header, so only its canonical 100-byte payload belongs in this record.
        struct GuiAudioTriggerWire
        {
            s32 miSize;
            s32 miType;
            s32 miOffset;
            GuiAudioTriggerWirePayload457 mPayload;
        };
        static_assert(sizeof(GuiAudioTriggerWire) == 112, "ARTIST GUI audio trigger wrapper");

        void PostMenuAudio(CgsGui::StateInterface* lpStateInterface, s32 liAction,
                           const char* lpacLabel)
        {
            GuiAudioTriggerEvent lAudio;
            lAudio.Construct(liAction, "", lpacLabel, "");
            GuiAudioTriggerWire lWire = { 100, 457, 12, {} };
            std::memcpy(&lWire.mPayload, lAudio.macComponent, sizeof(lWire.mPayload));
            lpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lWire), 40,
                static_cast<s32>(sizeof(lWire)));
        }

        // The three per-round stores repeated by ARTIST's insert/remove loops.
        // Other rounds in the same CheckpointData remain untouched.
        void CopyCheckpointRound(SelectRoutes::CheckpointData& lrDestination,
                                 const SelectRoutes::CheckpointData& lrSource, s32 liRound)
        {
            lrDestination.maeSelectedCounty[liRound] = lrSource.maeSelectedCounty[liRound];
            lrDestination.maStartLightTriggerID[liRound] = lrSource.maStartLightTriggerID[liRound];
            lrDestination.maLandmarkIndex[liRound] = lrSource.maLandmarkIndex[liRound];
        }
    }

    // ARTIST 82418BB8; DecFIGS BrnSelectRoutes.cpp:73.
    void SelectRoutes::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                                 const char* lpacParentName)
    {
        CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);
        miCurrentRound = 0;
        miStartItem = 0;
        mpGuiCache = 0;
        mMenuOptions.Construct("CheckpointItem", lpStateInterface,
                               KI_NUM_DISPLAYABLE_CHECKPOINTS, 0, 0xFFFFFFFFull);
        mUpArrowAnimator.Construct("ArrowUp_anim", lpStateInterface, 0);
        mDownArrowAnimator.Construct("ArrowDown_anim", lpStateInterface, 0);
        for (s32 liRound = 0; liRound < KI_MAX_ROUNDS; ++liRound)
            maiNumCheckpoints[liRound] = 1;
        for (s32 liCheckpoint = 0; liCheckpoint < KI_MAX_CHECKPOINTS; ++liCheckpoint)
        {
            for (s32 liRound = 0; liRound < KI_MAX_ROUNDS; ++liRound)
            {
                maCheckpointData[liCheckpoint].maeSelectedCounty[liRound] = BrnWorld::ECounty(0);
                maCheckpointData[liCheckpoint].maLandmarkIndex[liRound] = BrnGameState::LandmarkIndex(0);
                maCheckpointData[liCheckpoint].maStartLightTriggerID[liRound] = 0xFFFFFFFFu;
            }
        }
    }

    // ARTIST 82418CB8. Names are the component's own GuiComponent identities.
    void SelectRoutes::AppendExpectedAptComponent(GuiFlow leFlow, GuiCache* lpGuiCache)
    {
        mMenuOptions.AppendExpectedAptComponent(leFlow, lpGuiCache);
        lpGuiCache->AppendExpectedAptComponent(leFlow, mUpArrowAnimator.GetName());
        lpGuiCache->AppendExpectedAptComponent(leFlow, mDownArrowAnimator.GetName());
    }

    // ARTIST 82418D00 is a tail call through menu slot 5.
    void SelectRoutes::Update()
    {
        mMenuOptions.Update();
    }

    // ARTIST 82418D18: menu slot 6 is Clear, then both arrows become invisible.
    void SelectRoutes::Hide()
    {
        mMenuOptions.Clear();
        mUpArrowAnimator.AddOutputAptViewState("apt_Transition", "invisible", false);
        mDownArrowAnimator.AddOutputAptViewState("apt_Transition", "invisible", false);
    }

    // ARTIST 8242D728. The original only reveals the down arrow here.
    void SelectRoutes::Show()
    {
        mMenuOptions.SetupMenu(KI_NUM_DISPLAYABLE_CHECKPOINTS, false);
        miStartItem = 0;
        SetupMenuText();
        mDownArrowAnimator.AddOutputAptViewState("apt_Transition", "visible", false);
    }

    // ARTIST 82418D78. Burning Home Run has no separate finish-point row.
    SelectRoutes::EMenuItemType SelectRoutes::GetMenuItemType(s32 liItemIndex) const
    {
        if (liItemIndex == 0)
            return E_MENU_ITEM_TYPE_START_POINT;
        const s32 liCount = maiNumCheckpoints[miCurrentRound];
        if (mpGuiCache->GetOnlineGameMode() == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_BURNING_HOME_RUN)
        {
            if (liItemIndex == liCount && liCount < KI_MAX_CHECKPOINTS)
                return E_MENU_ITEM_TYPE_ADD_CHECKPOINT;
        }
        else
        {
            if ((liItemIndex == liCount && liCount < KI_MAX_CHECKPOINTS) ||
                (liItemIndex == liCount - 1 && liCount == KI_MAX_CHECKPOINTS))
                return E_MENU_ITEM_TYPE_FINISH_POINT;
            if (liItemIndex == liCount - 1 && liCount < KI_MAX_CHECKPOINTS)
                return E_MENU_ITEM_TYPE_ADD_CHECKPOINT;
        }
        if ((liItemIndex == liCount + 1 && liCount < KI_MAX_CHECKPOINTS) ||
            (liItemIndex == liCount && liCount == KI_MAX_CHECKPOINTS))
            return E_MENU_ITEM_TYPE_DONE;
        return E_MENU_ITEM_TYPE_CHECKPOINT;
    }

    // ARTIST 82426208. No matching preset returns index 0, as the original does.
    u32 SelectRoutes::GetStartPointEventIndex(s32 liRoundNumber) const
    {
        const u32 luCount = static_cast<u32>(mpGuiCache->GetNumPresetEvents());
        for (u32 luEvent = 0; luEvent < luCount; ++luEvent)
        {
            const PresetEvent* lpPreset = mpGuiCache->GetPresetEvent(static_cast<s32>(luEvent));
            const SatNavEventDisplayInfo* lpEventStart =
                mpGuiCache->GetPresetEventDisplayInfo(lpPreset->GetPositionLookupId());
            CGS_ASSERT(lpEventStart, "lpEventStart");
            if (mpGuiCache->GetPresetEvent(static_cast<s32>(luEvent))->GetPositionLookupId() ==
                maCheckpointData[0].maStartLightTriggerID[liRoundNumber])
                return luEvent;
        }
        return 0;
    }

    // ARTIST 824262F8. The row text references dynamically formatted localisation IDs.
    void SelectRoutes::SetupMenuText()
    {
        const s32 liCount = maiNumCheckpoints[miCurrentRound];
        for (s32 liRow = 0; liRow < KI_NUM_DISPLAYABLE_CHECKPOINTS; ++liRow)
        {
            const s32 liItem = miStartItem + liRow;
            const EMenuItemType leType = GetMenuItemType(liItem);
            char lacStringID[64];
            CgsCore::SPrintf(lacStringID, sizeof(lacStringID), "$ONLINE_SELECT_ROUTE_ADD_ROUND_%d", liRow);
            switch (leType)
            {
            case E_MENU_ITEM_TYPE_START_POINT:
                mpStateInterface->GetLanguageManager()->FormatAndAddText(
                    lacStringID + 1, "ONLINE_SELECT_ROUTE_START_POINT");
                break;
            case E_MENU_ITEM_TYPE_CHECKPOINT:
            {
                char lacCheckpoint[64];
                char lacTotal[64];
                CgsCore::SPrintf(lacCheckpoint, sizeof(lacCheckpoint), "%d", liItem);
                const s32 liTotal = mpGuiCache->GetOnlineGameMode() ==
                    BrnGameState::GameStateModuleIO::E_MODE_ONLINE_BURNING_HOME_RUN ? liCount - 1 : liCount - 2;
                CgsCore::SPrintf(lacTotal, sizeof(lacTotal), "%d", liTotal);
                mpStateInterface->GetLanguageManager()->FormatAndAddText(
                    lacStringID + 1, "ONLINE_SELECT_ROUTE_CHECKPOINT_NUM",
                    CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP, 2,
                    lacCheckpoint, CgsLanguage::LanguageManager::E_FORMAT_INTEGER,
                    lacTotal, CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
                break;
            }
            case E_MENU_ITEM_TYPE_ADD_CHECKPOINT:
                mpStateInterface->GetLanguageManager()->FormatAndAddText(
                    lacStringID + 1, "ONLINE_SELECT_ROUTE_ADD_ROUND");
                break;
            case E_MENU_ITEM_TYPE_FINISH_POINT:
                mpStateInterface->GetLanguageManager()->FormatAndAddText(
                    lacStringID + 1, "ONLINE_SELECT_ROUTE_FINISH_POINT");
                break;
            case E_MENU_ITEM_TYPE_DONE:
                mpStateInterface->GetLanguageManager()->FormatAndAddText(
                    lacStringID + 1, "ONLINE_GAME_OPTION_DONE");
                break;
            default:
            {
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lMessage(lacMessage, sizeof(lacMessage));
                lMessage << "invalid menu item type of " << static_cast<s32>(leType) << "\n";
                CGS_ASSERT(false, lMessage.GetBuffer());
                break;
            }
            }
            mMenuOptions.SetText(liRow, lacStringID);
            if ((liCount < KI_MAX_CHECKPOINTS && liItem <= liCount + 1) ||
                (liCount == KI_MAX_CHECKPOINTS && liItem < KI_MAX_CHECKPOINTS + 1))
                mMenuOptions.Enable(liRow);
            else
                mMenuOptions.Disable(liRow);
        }
    }

    // ARTIST 82431400: seed missing routes, retaining an explicitly selected preset.
    void SelectRoutes::Setup(const GuiEventNetworkGameParams* lpGameParams, GuiCache* lpGuiCache)
    {
        CGS_ASSERT(lpGameParams, "lpGameParams");
        CGS_ASSERT(lpGuiCache, "lpGuiCache");
        mpGuiCache = lpGuiCache;
        miStartItem = 0;
        const u32 luPresetCount = static_cast<u32>(mpGuiCache->GetNumPresetEvents());
        CGS_ASSERT(luPresetCount > 0, "There are no races to select from");
        CgsNumeric::Random lRandom;
        SeedFromGuiClock(lRandom, mpGuiCache);
        u32 luRandomPreset = 0;
        for (s32 liRound = 0; liRound < lpGameParams->miNumRounds; ++liRound)
        {
            if (mpGuiCache->GetOnlineGameMode() != BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE ||
                (liRound % 2) == 0)
                luRandomPreset = static_cast<u32>(lRandom.RandomInt(0, static_cast<s32>(luPresetCount) - 1));
            const PresetEvent* lpPreset = mpGuiCache->GetPresetEvent(static_cast<s32>(luRandomPreset));
            const GuiEventNetworkGameParams::Event& lrRequested = lpGameParams->maEvents[liRound];
            bool lbUseRequested = false;
            for (u32 luPreset = 0; luPreset < luPresetCount; ++luPreset)
            {
                if (mpGuiCache->GetPresetEvent(static_cast<s32>(luPreset))->GetEventId() ==
                    static_cast<u32>(lrRequested.GetEventID()))
                {
                    lbUseRequested = true;
                    break;
                }
            }
            maiNumCheckpoints[liRound] =
                (lbUseRequested ? lrRequested.GetNumLandmarks() : lpPreset->GetNumLandmarks()) + 1;
            for (s32 liCheckpoint = 0; liCheckpoint < maiNumCheckpoints[liRound]; ++liCheckpoint)
            {
                CheckpointData& lrCheckpoint = maCheckpointData[liCheckpoint];
                if (GetMenuItemType(liCheckpoint) == E_MENU_ITEM_TYPE_START_POINT)
                {
                    const u32 luStart = lbUseRequested ? lrRequested.GetTrafficLightTriggerId()
                                                       : lpPreset->GetPositionLookupId();
                    lrCheckpoint.maStartLightTriggerID[liRound] = luStart;
                    const SatNavEventDisplayInfo* lpEventStart = mpGuiCache->GetPresetEventDisplayInfo(luStart);
                    CGS_ASSERT(lpEventStart, "lpEventStart");
                    lrCheckpoint.maeSelectedCounty[liRound] = static_cast<BrnWorld::ECounty>(lpEventStart->muCounty);
                }
                else
                {
                    const BrnGameState::LandmarkIndex lLandmark = lbUseRequested
                        ? lrRequested.GetLandmark(liCheckpoint - 1) : lpPreset->GetLandmark(liCheckpoint - 1);
                    lrCheckpoint.maLandmarkIndex[liRound] = lLandmark;
                    GuiEventUpdateSatNav::SatNavIconInfo lInfo;
                    mpGuiCache->GetLandmarkInfoFromIndex(lLandmark, &lInfo);
                    lrCheckpoint.maeSelectedCounty[liRound] = lInfo.GetCounty();
                }
            }
        }
    }

    // ARTIST 82426810. If the add row lies outside the six visible slots, scroll to it.
    void SelectRoutes::HighlightAddCheckpoint()
    {
        const s32 liCount = maiNumCheckpoints[miCurrentRound];
        if (liCount == KI_MAX_CHECKPOINTS)
            return;
        s32 liHighlight = mpGuiCache->GetOnlineGameMode() ==
            BrnGameState::GameStateModuleIO::E_MODE_ONLINE_BURNING_HOME_RUN
                ? liCount - miStartItem : liCount - miStartItem - 1;
        if (liHighlight >= KI_NUM_DISPLAYABLE_CHECKPOINTS)
        {
            miStartItem = mpGuiCache->GetOnlineGameMode() ==
                BrnGameState::GameStateModuleIO::E_MODE_ONLINE_BURNING_HOME_RUN
                    ? liCount - 5 : liCount - 6;
            SetupMenuText();
            liHighlight = 5;
        }
        mMenuOptions.HighlightIndex(liHighlight);
    }

    // ARTIST 82435F18: changing an existing start/checkpoint through the original generators.
    bool SelectRoutes::SelectRandomLandmark(s32 liCheckpointItem, BrnWorld::ECounty leCounty)
    {
        if (liCheckpointItem == 0)
        {
            const u32 luStart = GenerateRandomStartPoint(miCurrentRound, leCounty);
            if ((luStart & 0xFFFF00u) == 0xFFFF00u || (luStart & 0xFFu) == 0xFFu)
            {
                CGS_ASSERT(false, "Couldn't find startpoint for county");
                return false;
            }
            const SatNavEventDisplayInfo* lpStart = mpGuiCache->GetPresetEventDisplayInfo(luStart);
            CGS_ASSERT(lpStart, "lpEventStart");
            maCheckpointData[0].maStartLightTriggerID[miCurrentRound] = lpStart->muLightTriggerId;
            maCheckpointData[0].maeSelectedCounty[miCurrentRound] = static_cast<BrnWorld::ECounty>(lpStart->muCounty);
            if (mpGuiCache->GetOnlineGameMode() == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE)
            {
                maCheckpointData[0].maStartLightTriggerID[miCurrentRound + 1] = lpStart->muLightTriggerId;
                maCheckpointData[0].maeSelectedCounty[miCurrentRound + 1] = static_cast<BrnWorld::ECounty>(lpStart->muCounty);
            }
        }
        else
        {
            const BrnGameState::LandmarkIndex lLandmark =
                liCheckpointItem == maiNumCheckpoints[miCurrentRound] - 1 &&
                mpGuiCache->GetOnlineGameMode() != BrnGameState::GameStateModuleIO::E_MODE_ONLINE_BURNING_HOME_RUN
                    ? GenerateRandomFinishPoint(miCurrentRound, liCheckpointItem, leCounty)
                    : GenerateRandomLandmark(miCurrentRound, liCheckpointItem, leCounty);
            if (static_cast<s32>(lLandmark) == -1)
            {
                CGS_ASSERT(false, "Couldn't find landmark for county");
                return false;
            }
            GuiEventUpdateSatNav::SatNavIconInfo lInfo;
            mpGuiCache->GetLandmarkInfoFromIndex(lLandmark, &lInfo);
            maCheckpointData[liCheckpointItem].maLandmarkIndex[miCurrentRound] =
                BrnGameState::LandmarkIndex(lInfo.GetLandmarkIndexHalf());
            maCheckpointData[liCheckpointItem].maeSelectedCounty[miCurrentRound] = lInfo.GetCounty();
            if (mpGuiCache->GetOnlineGameMode() == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE)
            {
                maCheckpointData[liCheckpointItem].maLandmarkIndex[miCurrentRound + 1] =
                    BrnGameState::LandmarkIndex(lInfo.GetLandmarkIndexHalf());
                maCheckpointData[liCheckpointItem].maeSelectedCounty[miCurrentRound + 1] = lInfo.GetCounty();
            }
        }
        return true;
    }

    // ARTIST 82483878; round is the argument, not miCurrentRound.
    void SelectRoutes::SetStartLightTriggerID(s32 liRoundNumber, u32 luStartLightTriggerID)
    {
        const SatNavEventDisplayInfo* lpStart = mpGuiCache->GetPresetEventDisplayInfo(luStartLightTriggerID);
        CGS_ASSERT(lpStart, "lpEventStart");
        maCheckpointData[0].maeSelectedCounty[liRoundNumber] = static_cast<BrnWorld::ECounty>(lpStart->muCounty);
        maCheckpointData[0].maStartLightTriggerID[liRoundNumber] = luStartLightTriggerID;
        if (mpGuiCache->GetOnlineGameMode() == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE)
        {
            maCheckpointData[0].maeSelectedCounty[liRoundNumber + 1] = static_cast<BrnWorld::ECounty>(lpStart->muCounty);
            maCheckpointData[0].maStartLightTriggerID[liRoundNumber + 1] = luStartLightTriggerID;
        }
    }

    // ARTIST 82489B18: a short route's finish menu row edits the preceding stored slot.
    void SelectRoutes::SetCurrentlySelectedCheckpointLandmark(s32 liRoundNumber,
                                                             BrnGameState::LandmarkIndex lLandmarkIndex)
    {
        GuiEventUpdateSatNav::SatNavIconInfo lInfo;
        mpGuiCache->GetLandmarkInfoFromIndex(lLandmarkIndex, &lInfo);
        const EMenuItemType leType = GetMenuItemType(GetCurrentlySelectedCheckpointIndex());
        if (leType == E_MENU_ITEM_TYPE_DONE || leType == E_MENU_ITEM_TYPE_ADD_CHECKPOINT)
        {
            CGS_ASSERT(false, "Checkpoint not selected!\n");
        }
        else
        {
            const bool lbPrevious = leType == E_MENU_ITEM_TYPE_FINISH_POINT &&
                                    maiNumCheckpoints[miCurrentRound] < KI_MAX_CHECKPOINTS;
            const s32 liCheckpoint = GetCurrentlySelectedCheckpointIndex() - (lbPrevious ? 1 : 0);
            maCheckpointData[liCheckpoint].maeSelectedCounty[liRoundNumber] = lInfo.GetCounty();
            maCheckpointData[liCheckpoint].maLandmarkIndex[liRoundNumber] = lLandmarkIndex;
            if (mpGuiCache->GetOnlineGameMode() == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE)
            {
                maCheckpointData[liCheckpoint].maeSelectedCounty[liRoundNumber + 1] = lInfo.GetCounty();
                maCheckpointData[liCheckpoint].maLandmarkIndex[liRoundNumber + 1] = lLandmarkIndex;
            }
        }
    }

    // ARTIST 82483920. Despite its name this shifts from the current selected row.
    void SelectRoutes::RemoveLastCheckpoint()
    {
        for (s32 liCheckpoint = GetCurrentlySelectedCheckpointIndex();
             liCheckpoint < maiNumCheckpoints[miCurrentRound] - 1; ++liCheckpoint)
        {
            CopyCheckpointRound(maCheckpointData[liCheckpoint], maCheckpointData[liCheckpoint + 1], miCurrentRound);
            if (mpGuiCache->GetOnlineGameMode() == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE)
                CopyCheckpointRound(maCheckpointData[liCheckpoint], maCheckpointData[liCheckpoint + 1], miCurrentRound + 1);
        }
        --maiNumCheckpoints[miCurrentRound];
        if (mpGuiCache->GetOnlineGameMode() == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE)
            --maiNumCheckpoints[miCurrentRound + 1];
        SetupMenuText();
    }


    // These header-inline owners are attested by OnlineSelectRoute's native callers.
    s32 SelectRoutes::GetNumberCheckpoints(s32 liRoundNumber) const
    {
        return maiNumCheckpoints[liRoundNumber];
    }

    u32 SelectRoutes::GetStartLightTriggerID(s32 liRoundNumber) const
    {
        return maCheckpointData[0].maStartLightTriggerID[liRoundNumber];
    }

    s32 SelectRoutes::GetCurrentRound() const
    {
        return miCurrentRound;
    }

    void SelectRoutes::SetCurrentRound(s32 liRoundNumber)
    {
        CGS_ASSERT(liRoundNumber >= 0, "liRound >= 0");
        CGS_ASSERT(static_cast<u32>(liRoundNumber) < KI_MAX_ROUNDS,
                   "((uint32_t)liRound) < BrnGameState::GameStateModuleIO::KU_MAX_ONLINE_ROUNDS_IN_MODE");
        miCurrentRound = liRoundNumber;
    }

    SelectRoutes::EMenuItemType SelectRoutes::GetCurrentMenuItemType() const
    {
        return GetMenuItemType(GetCurrentlySelectedCheckpointIndex());
    }

    // ARTIST 8243B040. State queues carry the controller payload without GuiEvent's
    // explicit PC queue header. Keep this boundary separate from the event producer.
    SelectRoutes::EChangedItem SelectRoutes::HandleControllerInput(const CgsModule::Event* lpControllerEvent)
    {
        struct ControllerButtonPayload
        {
            s32 miPadId;
            s32 miButtonId;
        };
        static_assert(sizeof(ControllerButtonPayload) == 8, "state-queue controller payload");
        ControllerButtonPayload lInput;
        std::memcpy(&lInput, lpControllerEvent, sizeof(lInput));
        EChangedItem leChanged = E_CHANGED_ITEM_COUNT;
        switch (lInput.miButtonId)
        {
        case 41: // up
        {
            bool lbMoved = mMenuOptions.HighlightPrevious();
            if (!lbMoved && miStartItem > 0)
            {
                --miStartItem;
                SetupMenuText();
                lbMoved = true;
            }
            if (lbMoved)
            {
                leChanged = E_CHANGED_ITEM_CHECKPOINT;
                mUpArrowAnimator.AddOutputAptViewState("apt_Transition", "animate", false);
                if (GetCurrentlySelectedCheckpointIndex() == 0)
                    mUpArrowAnimator.AddOutputAptViewState("apt_Transition", "invisible", false);
                mDownArrowAnimator.AddOutputAptViewState("apt_Transition", "visible", false);
                PostMenuAudio(mpStateInterface, 7, "MenuToggleDefault");
            }
            break;
        }
        case 42: // down
        {
            bool lbMoved = mMenuOptions.HighlightNext();
            if (!lbMoved)
            {
                const s32 liCount = maiNumCheckpoints[miCurrentRound];
                if ((liCount < KI_MAX_CHECKPOINTS && miStartItem + 6 < liCount + 2) ||
                    miStartItem + 6 < liCount + 1)
                {
                    ++miStartItem;
                    SetupMenuText();
                    lbMoved = true;
                }
            }
            if (lbMoved)
            {
                leChanged = E_CHANGED_ITEM_CHECKPOINT;
                mDownArrowAnimator.AddOutputAptViewState("apt_Transition", "animate", false);
                const s32 liCount = maiNumCheckpoints[miCurrentRound];
                if ((liCount < KI_MAX_CHECKPOINTS && GetCurrentlySelectedCheckpointIndex() == liCount + 1) ||
                    (liCount == KI_MAX_CHECKPOINTS && GetCurrentlySelectedCheckpointIndex() == KI_MAX_CHECKPOINTS))
                    mDownArrowAnimator.AddOutputAptViewState("apt_Transition", "invisible", false);
                mUpArrowAnimator.AddOutputAptViewState("apt_Transition", "visible", false);
                PostMenuAudio(mpStateInterface, 7, "MenuToggleDefault");
            }
            break;
        }
        case 49: // accept
        {
            const EMenuItemType leType = GetMenuItemType(GetCurrentlySelectedCheckpointIndex());
            if (leType == E_MENU_ITEM_TYPE_ADD_CHECKPOINT)
            {
                if (maiNumCheckpoints[miCurrentRound] < KI_MAX_CHECKPOINTS)
                {
                    for (s32 liCheckpoint = maiNumCheckpoints[miCurrentRound] - 1;
                         liCheckpoint >= GetCurrentlySelectedCheckpointIndex(); --liCheckpoint)
                    {
                        CopyCheckpointRound(maCheckpointData[liCheckpoint + 1], maCheckpointData[liCheckpoint], miCurrentRound);
                        if (mpGuiCache->GetOnlineGameMode() == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE)
                            CopyCheckpointRound(maCheckpointData[liCheckpoint + 1], maCheckpointData[liCheckpoint], miCurrentRound + 1);
                    }
                    ++maiNumCheckpoints[miCurrentRound];
                    if (mpGuiCache->GetOnlineGameMode() == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE)
                        ++maiNumCheckpoints[miCurrentRound + 1];
                    SelectRandomLandmark(GetCurrentlySelectedCheckpointIndex(), BrnWorld::E_COUNTY_COUNT);
                    SetupMenuText();
                    leChanged = E_CHANGED_ITEM_EDIT_NEW_CHECKPOINT;
                    if (maiNumCheckpoints[miCurrentRound] < KI_MAX_CHECKPOINTS)
                        mDownArrowAnimator.AddOutputAptViewState("apt_Transition", "visible", false);
                }
            }
            else if (leType == E_MENU_ITEM_TYPE_DONE)
                leChanged = E_CHANGED_ITEM_FINISHED_EDIT;
            else if (leType == E_MENU_ITEM_TYPE_START_POINT || leType == E_MENU_ITEM_TYPE_CHECKPOINT ||
                     leType == E_MENU_ITEM_TYPE_FINISH_POINT)
                leChanged = E_CHANGED_ITEM_EDIT_CHECKPOINT;
            PostMenuAudio(mpStateInterface, 4, "B5MenuItem");
            break;
        }
        case 51: // randomise current start/checkpoint/finish
        {
            const EMenuItemType leType = GetMenuItemType(GetCurrentlySelectedCheckpointIndex());
            if (leType == E_MENU_ITEM_TYPE_CHECKPOINT || leType == E_MENU_ITEM_TYPE_FINISH_POINT ||
                leType == E_MENU_ITEM_TYPE_START_POINT)
            {
                const s32 liCheckpoint = GetCurrentlySelectedCheckpointIndex() -
                    (leType == E_MENU_ITEM_TYPE_FINISH_POINT && maiNumCheckpoints[miCurrentRound] < KI_MAX_CHECKPOINTS ? 1 : 0);
                SelectRandomLandmark(liCheckpoint, BrnWorld::E_COUNTY_COUNT);
                leChanged = E_CHANGED_ITEM_LANDMARK;
            }
            break;
        }
        case 52: // remove a selected checkpoint when at least one other route point remains
        {
            if (maiNumCheckpoints[miCurrentRound] > 2 &&
                GetMenuItemType(GetCurrentlySelectedCheckpointIndex()) == E_MENU_ITEM_TYPE_CHECKPOINT)
            {
                for (s32 liCheckpoint = GetCurrentlySelectedCheckpointIndex();
                     liCheckpoint < maiNumCheckpoints[miCurrentRound] - 1; ++liCheckpoint)
                {
                    CopyCheckpointRound(maCheckpointData[liCheckpoint], maCheckpointData[liCheckpoint + 1], miCurrentRound);
                    if (mpGuiCache->GetOnlineGameMode() == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE)
                        CopyCheckpointRound(maCheckpointData[liCheckpoint], maCheckpointData[liCheckpoint + 1], miCurrentRound + 1);
                }
                --maiNumCheckpoints[miCurrentRound];
                if (mpGuiCache->GetOnlineGameMode() == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE)
                    --maiNumCheckpoints[miCurrentRound + 1];
                SetupMenuText();
                leChanged = E_CHANGED_ITEM_CHECKPOINT;
                const s32 liCount = maiNumCheckpoints[miCurrentRound];
                if ((liCount < KI_MAX_CHECKPOINTS && GetCurrentlySelectedCheckpointIndex() == liCount + 2) ||
                    (liCount == KI_MAX_CHECKPOINTS && GetCurrentlySelectedCheckpointIndex() == 18))
                    mDownArrowAnimator.AddOutputAptViewState("apt_Transition", "invisible", false);
            }
            break;
        }
        case 57: // insert after the current row, or at the add-checkpoint row
        {
            const EMenuItemType leType = GetMenuItemType(GetCurrentlySelectedCheckpointIndex());
            if (maiNumCheckpoints[miCurrentRound] < KI_MAX_CHECKPOINTS &&
                (leType == E_MENU_ITEM_TYPE_CHECKPOINT || leType == E_MENU_ITEM_TYPE_ADD_CHECKPOINT ||
                 leType == E_MENU_ITEM_TYPE_START_POINT))
            {
                for (s32 liCheckpoint = maiNumCheckpoints[miCurrentRound] - 1;
                     liCheckpoint >= GetCurrentlySelectedCheckpointIndex(); --liCheckpoint)
                {
                    CopyCheckpointRound(maCheckpointData[liCheckpoint + 1], maCheckpointData[liCheckpoint], miCurrentRound);
                    if (mpGuiCache->GetOnlineGameMode() == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE)
                        CopyCheckpointRound(maCheckpointData[liCheckpoint + 1], maCheckpointData[liCheckpoint], miCurrentRound + 1);
                }
                if (leType != E_MENU_ITEM_TYPE_ADD_CHECKPOINT)
                {
                    const s32 liNextVisibleRow = GetCurrentlySelectedCheckpointIndex() - miStartItem + 1;
                    if (liNextVisibleRow < KI_NUM_DISPLAYABLE_CHECKPOINTS)
                        mMenuOptions.HighlightIndex(liNextVisibleRow);
                    else
                        ++miStartItem;
                }
                ++maiNumCheckpoints[miCurrentRound];
                if (mpGuiCache->GetOnlineGameMode() == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE)
                    ++maiNumCheckpoints[miCurrentRound + 1];
                SelectRandomLandmark(GetCurrentlySelectedCheckpointIndex(), BrnWorld::E_COUNTY_COUNT);
                SetupMenuText();
                leChanged = E_CHANGED_ITEM_EDIT_NEW_CHECKPOINT;
                if (maiNumCheckpoints[miCurrentRound] == KI_MAX_CHECKPOINTS &&
                    GetCurrentlySelectedCheckpointIndex() == KI_MAX_CHECKPOINTS - 1)
                    mDownArrowAnimator.AddOutputAptViewState("apt_Transition", "invisible", false);
                else
                    mDownArrowAnimator.AddOutputAptViewState("apt_Transition", "visible", false);
            }
            break;
        }
        default:
            break;
        }
        return leChanged;
    }

}
