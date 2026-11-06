from pathlib import Path
import hashlib, json

ROOT = Path('D:/codex-native/racecar-containment-sol-r1')
HEADER = ROOT/'src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h'
raw = HEADER.read_bytes()
assert hashlib.sha256(raw).hexdigest() == 'aaa20134c9c2d4b49009e09322308c221e4860412ab5aa6976d97fc7695e1329'
s = raw.decode('utf-8-sig')
before = s[s.index('    // FLAG: opaque leading state.'):s.index('\n};',s.index('    // FLAG: opaque leading state.'))]
methods = '''    void ProcessPlayerVehicleInput(f32 lfTimeStep,
        const RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpInput,
        RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput);
    void UpdateBoost(f32 lfTimeStep,
        const RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpInput,
        RaceCarEntityModuleIO::GameEventQueue* lpEventQueue);
    s32 GetDamagedCarCount() const;
    void ProcessTakedownEvents(const RaceCarEntityModuleIO::TakedownEventQueue* lpQueue);
    void UpdateActiveCars(f32 lfTimeStep, f32 lfTimeStepMultiplier,
        f32 lfAcceleration, f32 lfBraking,
        RaceCarEntityModuleIO::GameEventQueue* lpGameEvents,
        BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInput);
    void SendAddedForCollisionStateToPhysics(BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInput);
    void StorePlayerRoutePortalPositions(const RaceCarEntityModuleIO::InputBuffer_PostPhysics* lpInput);

    // Native containment follows DecFIGS declaration order, with ARTIST accesses
    // closing each constituent boundary. Existing source spellings are retained.
    // The leading ModuleSingleBuffered base is the actual native owner (0x228).
    f32 mfClippedTime;                              // ARTIST +0x228
    s32 mePrepareStage;                             // +0x22C
    s32 miPrepareCarIndex;                          // +0x230, meReleaseStage in DWARF
    s32 meLoadGlobalResourcesStage;                 // +0x234, meInitialLoadStage
    s32 miExpectedResponseCount;                    // +0x238, miResourceCount
    CgsResource::ResourceHandle mDistrictMapResourceHandle; // +0x23C
    BrnWorld::WorldRegion mCurrentWorldRegion;       // +0x244
    RaceCar maRaceCars[E_GLOBAL_RACE_CAR_INDEX_COUNT];       // +0x250
    ActiveRaceCar maActiveRaceCars[E_ACTIVE_RACE_CAR_INDEX_COUNT]; // +0x1A60
    CgsContainers::BitArray<8u> mabResetThisFrame;    // +0x100E0
    CgsModule::EventReceiverQueue<4096, 16> mReceiverQueue; // +0x100E8
    RaceCarStreamer mRaceCarStreamer;               // +0x11100
    PlaceOnTrackManager mPlaceOnTrackManager;       // +0x17850
    BoostManager mBoostManager;                    // +0x17890
    RaceCarEntityModuleDebugComponent mRCEntityModuleDebugComponent; // +0x17CF0
    BoostDebugComponent mBoostDebugComponent;       // +0x17E50
    NearMissManager mNearMissManager;               // +0x17E68
    AirTimeManager mAirTimeManager;                 // +0x180D8
    TrafficCheckManager mTrafficCheckManager;       // +0x180E8
    CrashPlayManager mCrashPlayManager;             // +0x180F0
    PowerParkingManager mPowerParkingManager;       // +0x18250
    f32 mfCurrentTailgateDuration;                  // +0x182F0
    EActiveRaceCarIndex meIndexOfCarPlayerIsTailgating; // +0x182F4
    EActiveRaceCarIndex mePlayerActiveRaceCarIndex;  // +0x182F8
    CgsWorld::WorldMap2D mWorldMap2D;                // +0x18300
    Attrib::Gen::surfacelist mSurfaceList;           // +0x18330
    s32 miOpponentCount;                           // +0x18340
    bool mbIsInGameMode;
    bool mbIsInOnlineGameMode;
    bool mbOnlineModeJustFinished;
    bool mbCarSelectAllowedInGameMode;
    bool mbWaitingForStreaming;
    bool mbDisplayPlayerCarPosition;
    bool mbRivalDebug;
    bool mbRenderCarsDuringCrash;
    bool mbRenderWheels;
    bool mbSixaxisSteeringEnabled;
    bool mbPaybackSixaxisSteering;
    bool mbRenderRaceCarCoronas;
    bool mbSpawnAIBehindStartGrid;
    bool mbPlayerRollsOnEventStart;
    bool mbPlayerDonutsOnEventStart;
    bool mbUseRankUpAISpawnSeparation;
    bool mbModeStartedPlaying;
    u64 mxGameModeFlags;                           // +0x18358
    bool mabCarSelectWaitForStreaming[8];           // +0x18360
    BrnGameState::GameStateModuleIO::EGameModeType meGameModeType; // +0x18368
    BrnPhysics::Vehicle::AggressiveDrivingFlags mAggressiveDrivingFlags; // +0x1836C
    bool mbSendStreamingComplete;
    BrnProgression::ETrainingType mePendingTrainingRequestQueue[KI_TRAINING_REQUEST_QUEUE_SIZE];
    s32 miPendingRequestCount;
    f32 mfTimeStep;                                // +0x18398, mfSimTimerTimeStep
    f32 mfIntroTimer;
    f32 mfSimTime;
    f32 mfTimeStepMultiplier;
    PlayerVehicleControls mPlayerVehicleControls;   // +0x183A8
    BrnNetwork::EPaybackType meActivePaybackType;
    EActiveRaceCarIndex meActivePaybackAggressor;
    Matrix44Affine mCameraTransform;               // +0x183F0
    bool mbDEBUGPlayerUsingReset;
    const BrnResource::VehicleList* mpVehicleList;
    const BrnResource::WheelList* mpWheelList;
    CgsResource::ResourcePtr<BrnWorld::GlobalColourPalette> mCarColoursResource;
    CgsNumeric::Random mDeterministicRandom;        // +0x18460
    CgsNumeric::Random mNonDeterministicRandom;     // +0x18490
    f32 mfLastPlayerCarSpeed;
    f32 mfRandomBoostTime;
    bool mbRandomBoostOn;
    f32 mfResetOnWaterHeight;
    s32 miPlayerBaseDeformationTypeMirror;
    s32 miPlayerBaseDeformationTypeSaved;
    f32 mfPlayerBaseDeformAmountMirror;
    bool mbPlayerBaseDeformRequestPending;
    f32 mfPlayerBaseDeformAmountSaved;
    StoredStompeeData mStoredStompees[E_ACTIVE_RACE_CAR_INDEX_COUNT]; // +0x184F0
    s32 miStoredStompeeCount;                       // +0x185F0
    bool mbInReplay;                               // +0x185F4
    bool mbReplayRestored;                         // +0x185F5

    // These are genuine declarations, not an accepted replay layout. The native
    // serialiser starts at 0x18600; its extent/alignment is still under recovery.
    BrnReplays::RaceCarEntitySerialiser mReplaySerialiser;
    Matrix44Affine mReplayTransform;
    bool mbInvalidatedRaceCars;
    bool mabRaceCarsRendered[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    bool mbInCarSelectScreen;
    bool mbInCarModScreen;
    s32 meCarSelectResetType;
    bool mbCarSelectDontStreamAudio;
    bool mbHACK_ExitingCarSelectWaitForAudio;
    RaceCarNeedsHidingEventRecord mHidingEvents[8];
    s32 miHidingEvents;
    bool mbOncomingTimerActive;
    f32 mfOncomingNoClueTimer;
    Vector2 mPlayersCurrentRouteNodePosition;
    Vector2 mPlayersNextRouteNodePosition;
    bool DEBUG_mbSpawnDebugRaceCarThisFrame;
    bool DEBUG_mbCleanDebugRaceCarThisFrame;
    bool DEBUG_mbDebugRaceCarIsActive;
    Vector3 DEBUG_mDebugRaceCarPosition;
    Vector3 DEBUG_mDebugRaceCarDirection;
    f32 DEBUG_mfDebugRaceCarVelocity;
    RaceCar* DEBUG_mpDebugRaceCar;
    bool DEBUG_mbOverrideDamage;
    f32 DEBUG_mfVehicleScratchAmount;
    f32 DEBUG_mfVehicleDustAmount;
    f32 DEBUG_mfVehicleCrumpleAmount;
    f32 DEBUG_mfSelfIlluminationR;
    f32 DEBUG_mfSelfIlluminationG;
    f32 DEBUG_mfSelfIlluminationB;
    bool DEBUG_mbOverrideCarColor;
    f32 DEBUG_mfOverridePaintColorR;
    f32 DEBUG_mfOverridePaintColorG;
    f32 DEBUG_mfOverridePaintColorB;
    f32 DEBUG_mfOverridePearlColorR;
    f32 DEBUG_mfOverridePearlColorG;
    f32 DEBUG_mfOverridePearlColorB;
    bool DEBUG_mbOverrideCarPalette;
    s32 DEBUG_miPaletteIndex;
    s32 DEBUG_miColourIndex;
    EActiveRaceCarIndex maActiveRaceCarForPlayerScoringIndex[BrnGameState::GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT];
    bool mbSetRaceCarEliminated;
    EActiveRaceCarIndex mCarToCrashActiveIndex;
    s32 miPreSceneUpdatePM;
    s32 miPostSceneUpdatePM;
    s32 miPrePhysicsUpdatePM;
    s32 miPostPhysicsUpdatePM;
'''
s = s.replace(before,methods)
s = s.replace('class RaceCarEntityModule\n{','class RaceCarEntityModule : public CgsModule::ModuleSingleBuffered\n{')
needle = '#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"'
s = s.replace(needle,'#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"\n#include "GameSource/Replays/Serialisers/BrnReplayRaceCarEntitySerialiser.h"\n'+needle)
banner_end = s.index('#include "GameShared/GameClasses/Containers/CgsArray.h"')
s = '#pragma once\n\n// Native RaceCar module containment; ARTIST supplies offsets and DecFIGS\n// supplies declaration order. Replay-tail extent remains a private open gate.\n\n'+s[banner_end:]
HEADER.write_bytes(s.replace('\r\n','\n').replace('\n','\r\n').encode('utf-8'))
print(json.dumps({'header':str(HEADER),'before':hashlib.sha256(raw).hexdigest(),'after':hashlib.sha256(HEADER.read_bytes()).hexdigest()},indent=2))
