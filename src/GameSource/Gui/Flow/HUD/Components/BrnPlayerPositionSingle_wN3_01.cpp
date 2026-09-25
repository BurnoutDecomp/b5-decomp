// ============================================================================
// GameSource/Gui/Flow/HUD/Components/BrnPlayerPositionSingle_wN3_01.cpp
//
// PlayerPositionSingleComponent::Update -- push one position-table row into a bar: the row
// type (and the name, on a type or race-car change), the colour animator, the headset and
// live-revenge icons, and the value (marking it for RenderValue). Its one caller is
// PlayerPositionTableComponent::DisplayData.
// ============================================================================
#include "GameSource/Gui/Flow/HUD/Components/BrnPlayerPositionSingle.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::OutputGuiEvent
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiAudioTriggerEvent

namespace BrnGui
{
    namespace
    {
        // The bar clip's frame labels, indexed by PlayerTypes.
        const char* const KAPC_PLAYER_TYPE_LABELS[E_PLAYERTYPES_COUNT] =
        {
            "Basic", "FBCOff", "FBCNotActive", "FBCInProg", "FBCDone", "FBCTotal", "newPlayer", "invisible",
        };
        // The headset icon labels, indexed by HeadsetStatus.
        const char* const KAPC_HEADSET_LABELS[E_HEADSETSTATUS_COUNT] =
        {
            "invisible", "off", "on", "active",
        };
        // The live-revenge icon labels, indexed by RevengeStatus.
        const char* const KAPC_REVENGE_LABELS[E_REVENGESTATUS_COUNT] =
        {
            "markedMan", "up", "down", "invisible",
        };
        // The colour animator frames, indexed by the player colour (EGuiPlayerColours).
        const char* const KAPC_PLAYER_COLOUR_LABELS[12] =
        {
            "Invisible", "Yellow", "Red", "Blue", "Pink", "Green",
            "Orange", "Purple", "Cyan", "White", "Gray", "Black",
        };
    }

    void PlayerPositionSingleComponent::Update(PlayerPositionSingleData* lpData)
    {
        CGS_ASSERT(lpData != 0, "lpPlayerData");
        CGS_ASSERT(lpData->mePlayerType >= 0, "Player Type is invalid : ");
        CGS_ASSERT(lpData->mePlayerType < E_PLAYERTYPES_COUNT, "Player Type is invalid : ");
        CGS_ASSERT(lpData->meHeadsetStatus >= 0, "Headset Status is invalid : ");
        CGS_ASSERT(lpData->meHeadsetStatus < E_HEADSETSTATUS_COUNT, "Headset Status is invalid : ");
        CGS_ASSERT(lpData->meRevengeStatus >= 0, "Revenge Status is invalid : ");
        CGS_ASSERT(lpData->meRevengeStatus < E_REVENGESTATUS_COUNT, "Revenge Status is invalid : ");

        if (lpData->meActiveRaceCarIndex != meActiveRaceCarIndex || lpData->mePlayerType != mePlayerType)
        {
            mePlayerType         = lpData->mePlayerType;
            meActiveRaceCarIndex = lpData->meActiveRaceCarIndex;
            mAptRef.GotoAndPlayLabel(KAPC_PLAYER_TYPE_LABELS[mePlayerType]);
            if (mePlayerType == E_PLAYERTYPES_FREEBURN_CHALLENGE_COMPLETE)
            {
                GuiAudioTriggerEvent lAudio;
                lAudio.Construct(6, "", "CodeTickboxChallengeAchieved", "");
                mpStateInterface->OutputGuiEvent<GuiAudioTriggerEvent>(lAudio);
            }
            mName.SetLocalisedText(lpData->mPlayerName.macName, lpData->meNameType);
            mbValueChanged = true;
        }

        if (lpData->mePlayerColour != mePlayerColour)
        {
            mePlayerColour = lpData->mePlayerColour;
            mPlayerTextColourAnimator.Run(KAPC_PLAYER_COLOUR_LABELS[lpData->mePlayerColour]);
        }

        if (lpData->meHeadsetStatus != meHeadsetStatus)
        {
            meHeadsetStatus = lpData->meHeadsetStatus;
            mAudio.GotoAndPlayLabel(KAPC_HEADSET_LABELS[meHeadsetStatus]);
        }

        if (lpData->meRevengeStatus != meRevengeStatus)
        {
            meRevengeStatus = lpData->meRevengeStatus;
            mRevenge.GotoAndPlayLabel(KAPC_REVENGE_LABELS[meRevengeStatus]);
        }

        if (mfValue != lpData->mfTableValue)
        {
            mfValue        = lpData->mfTableValue;
            mbValueChanged = true;
        }
    }
}
