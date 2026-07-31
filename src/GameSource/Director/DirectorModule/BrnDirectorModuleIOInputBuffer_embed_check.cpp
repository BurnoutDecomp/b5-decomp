// Compile-gate embed check for BrnDirector::DirectorIO::InputBuffer.
// Instantiates the buffer by value (forcing the full member layout + all static_assert offset
// pins in _AssertLayout to be compiled) and references every reconstructed accessor so the
// declarations and definitions are type-checked together.

#include "GameSource/Director/DirectorModule/BrnDirectorModuleIO.h"

namespace
{
    void ExerciseInputBuffer()
    {
        BrnDirector::DirectorIO::InputBuffer lBuffer;

        // queries
        (void)lBuffer.GetContacts();
        (void)lBuffer.GetUsedRaceCars();
        (void)lBuffer.GetVehicleInfoArray();
        (void)lBuffer.GetControllerInfo();
        (void)lBuffer.GetTimerStatusInterface();
        (void)static_cast<const BrnDirector::DirectorIO::InputBuffer&>(lBuffer).GetVehicleInputInterface();
        (void)lBuffer.GetVehicleInputInterface();
        (void)lBuffer.GetHookEnumeration();
        (void)lBuffer.GetPlayerCarIndex();
        (void)lBuffer.GetShortcutMenuState();
        (void)lBuffer.HasGotHookEnumeration();
        (void)lBuffer.HasGotShortcutMenuEvent();
        (void)lBuffer.HasGotCrashNavShownEvent();
        (void)lBuffer.HasGotCrashNavHiddenEvent();
        (void)lBuffer.HasGotColourCalibrationShownEvent();
        (void)lBuffer.HasGotColourCalibrationHiddenEvent();

        // mutators
        const u32 luContactWord = 0;
        lBuffer.AppendContacts(&luContactWord);

        unsigned char laControllerBytes[224] = {};
        lBuffer.SetControllerInfo(laControllerBytes);

        unsigned char laHookBytes[404] = {};
        lBuffer.SetHookEnumeration(laHookBytes);

        BrnDirector::Camera::VehicleInfo lInfo;
        lBuffer.SetRaceCarInfo(0u, lInfo);

        Matrix44Affine lCentre;
        lBuffer.SetCrashingCentreOfMass(0u, lCentre);

        lBuffer.SetVehicleTeam(E_ACTIVE_RACE_CAR_INDEX_0, 1);
        lBuffer.SetShortcutMenuEvent(true);
        lBuffer.SetGotCrashNavShownEvent();
        lBuffer.SetGotCrashNavHiddenEvent();
        lBuffer.SetGotColourCalibrationShownEvent();
        lBuffer.SetGotColourCalibrationHiddenEvent();

        // the BridgeGuiToDirector command pairs
        lBuffer.SetStartNewProfileIntro();          (void)lBuffer.GetStartNewProfileIntro();
        lBuffer.SetStartGameIntroFlyby();           (void)lBuffer.GetStartGameIntroFlyby();
        lBuffer.SetStopGameIntroFlyby();            (void)lBuffer.GetStopGameIntroFlyby();
        lBuffer.SetRankUp(1);                       (void)lBuffer.GetRankUpThisFrame();
                                                    (void)lBuffer.GetRankUpNewRank();
        lBuffer.SetCameraType(0);                   (void)lBuffer.GetCameraType();
        lBuffer.SetEndOfCarSelect();                (void)lBuffer.GetEndOfCarSelect();
        lBuffer.SetDirectorProfileData(0);          (void)lBuffer.HasNewDirectorProfileData();
                                                    (void)lBuffer.GetDirectorProfileData();
        lBuffer.SetCarSelectionChangedThisFrame();  (void)lBuffer.GetCarSelectionChangedThisFrame();
        lBuffer.SetCarSelectTickerClosedThisFrame();(void)lBuffer.GetCarSelectTickerClosedThisFrame();
        lBuffer.SetLeftOnlinePostEvent();           (void)lBuffer.GetLeftOnlinePostEvent();
        lBuffer.SetEnteredOnlinePostEvent();        (void)lBuffer.GetEnteredOnlinePostEvent();
        lBuffer.SetFinishedOnlineEventLoading();    (void)lBuffer.GetFinishedOnlineEventLoading();
        lBuffer.SetStartedOnlineEventLoading();     (void)lBuffer.GetStartedOnlineEventLoading();
        lBuffer.SetStarting100PercentSequence();    (void)lBuffer.GetStarting100PercentSequence();
        lBuffer.SetFinished100PercentSequence();    (void)lBuffer.GetFinished100PercentSequence();
        (void)lBuffer.IsSimPaused();
    }
}

// Reference the exerciser so it is not pruned before instantiation.
void* gpDirectorInputBufferEmbedCheck = reinterpret_cast<void*>(&ExerciseInputBuffer);
