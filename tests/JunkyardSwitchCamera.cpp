// Recording fixture for complete production ArbStateCarSelect switch arms.
// Shot identity is independent of the production ternary; the oracle is the
// ARTIST pointer loads documented in run_junkyard_switch_camera.py.
#include <cstdio>
using f32 = float;
using s32 = int;

struct Shot { int miIndex; };
namespace Camera {
struct BehaviourIceAnim {};
struct Camera { int mState_uFlags = 0; };
inline void EnsureEffectIsPlaying(Camera&, int&, const char*, float) {}
}
struct Behaviour {
    Shot* mpSelected = nullptr;
    int miChanges = 0;
    bool mbFinished = true;
    void ChangeMovie(Shot* lpShot, int&) { mpSelected = lpShot; ++miChanges; }
    bool HasFinishedOrFailed() { return mbFinished; }
    float GetTimeRemaining() { return 20.0f; }
    void BecomeSimilarTo(Camera::Camera&, int&) {}
    void ClearBaseFirstFrameGate() {}
    void SetParameters(Shot*) {}
    void SetTakeLooping(bool) {}
};
struct Handle {
    Behaviour mBehaviour;
    Camera::Camera mCamera;
    int miReleases = 0;
    Behaviour* GetBehaviour() { return &mBehaviour; }
    Camera::Camera& GetProducedCamera() { return mCamera; }
    Camera::Camera& GetCamera() { return mCamera; }
    int GetBehaviourHelperIndex() { return 0; }
    void Release() { ++miReleases; }
    template<class... A> void Prepare(A...) {}
};
struct Manager { template<class T, class... A> void NewBehaviour(const A&...) {} };
struct GameState {
    enum { E_JY_INACTIVE, E_JY_WAITING_FOR_AUDIO, E_JY_CAR_UNLOCK, E_JY_CAR_SELECT };
    int meJunkyardState = E_JY_CAR_SELECT;
    bool mbJunkyardPosJustChanged = false;
    bool mbJunkyardPosIsLeft = true;
    bool mbJunkyardSelectionChangedMessageReceivedThisFrame = false;
    bool mbJunkyardCarModActive = false;
    bool mbJunkyardPlayerRespawnedThisFrame = false;
    float mfPadInactiveTime = 1.0f;
};
struct SharedInfo {
    int mEffect = 0, mVehicles = 0;
    int* mpEffectInterface = &mEffect;
    int* mpAllVehicleData = &mVehicles;
    float mfTimestep = 0.125f;
};
struct State {
    enum { E_STATE_ACTIVE = 8, E_STATE_ROTATE_ABOUT_CAR, E_STATE_WAIT_FOR_CAR_DROP,
           E_STATE_IDLE, E_STATE_CAR_UNLOCK = 7 };
    int meState = E_STATE_ACTIVE;
    bool mbIsLeft = true;
    bool mbSafeToBeAttachedToCar = false;
    bool mbWaitingForCarToSpawn = false;
    bool mbWaitingForCarToTouchGround = false;
    bool mbWaitingToLookAtOriginalSelection = false;
    float mfTimeInState = 1.0f;
    float mfTimeWaitingToLookAtOriginalSelection = 0.0f;
    Shot mLeftToRight{1}, mRightToLeft{2}, mIdle{3};
    Shot* mpLeftToRight = &mLeftToRight;
    Shot* mpRightToLeft = &mRightToLeft;
    Shot* mpIdle = &mIdle;
    Handle mTransitionCam, mToGameplayInterpolater, mLookAroundCarCam, mIdleCam;
    Camera::Camera mCamera;
    GameState mGame;
    SharedInfo mShared;
    Manager mManager;
    int mResources = 0;
    Camera::Camera& GetNonConstCamera() { return mCamera; }
    void StartWaitForAudioMovie(SharedInfo&) {}
    void StartCarUnlockCam(SharedInfo&) {}
    void StartOutroMovie(SharedInfo&) {}
    void ReturnToActive() { meState = E_STATE_ACTIVE; }
    void Step();
};

#include "junkyard_switch_camera.inc"

int giChecks = 0, giFailures = 0;
void Check(bool lbPass, const char* lpName, int liState, int liSide) {
    ++giChecks;
    if (!lbPass) { ++giFailures; std::printf("FAIL %s state=%d oldLeft=%d\n", lpName, liState, liSide); }
}
int main() {
    const int laiStates[] = { State::E_STATE_ACTIVE, State::E_STATE_ROTATE_ABOUT_CAR, State::E_STATE_IDLE };
    for (int liState : laiStates) {
        for (int liSide = 0; liSide < 2; ++liSide) {
            State lState;
            lState.meState = liState;
            lState.mbIsLeft = liSide != 0;
            lState.mGame.mbJunkyardSelectionChangedMessageReceivedThisFrame = true;
            lState.Step();
            // New left means ShotList[2]; new right means ShotList[1].
            Shot* lpExpected = liSide ? &lState.mLeftToRight : &lState.mRightToLeft;
            Check(lState.mTransitionCam.mBehaviour.mpSelected == lpExpected, "selection take points to destination", liState, liSide);
            Check(lState.mTransitionCam.mBehaviour.miChanges == 1 && lState.mbIsLeft != (liSide != 0), "one toggle and one movie", liState, liSide);
            Check(lState.meState == State::E_STATE_WAIT_FOR_CAR_DROP && lState.mbWaitingForCarToSpawn, "wait for replacement spawn", liState, liSide);
            Check(lState.mfTimeInState == 0 && !lState.mbWaitingToLookAtOriginalSelection, "reset selection timing", liState, liSide);
            Check(lState.mToGameplayInterpolater.miReleases == 1, "release browse interpolator", liState, liSide);
            Check((lState.mCamera.mState_uFlags & 0x200000) != 0, "entering camera flag", liState, liSide);
            if (liState == State::E_STATE_IDLE)
                Check(lState.mIdleCam.miReleases == 1, "idle camera released", liState, liSide);

            State lModified;
            lModified.meState = liState;
            lModified.mbIsLeft = liSide != 0;
            lModified.mGame.mbJunkyardSelectionChangedMessageReceivedThisFrame = true;
            lModified.mGame.mbJunkyardCarModActive = true;
            lModified.Step();
            Check(lModified.mTransitionCam.mBehaviour.miChanges == 0 && lModified.mbIsLeft == (liSide != 0), "livery change does not turn camera", liState, liSide);

            State lPosition;
            lPosition.meState = liState;
            lPosition.mbIsLeft = liSide != 0;
            lPosition.mGame.mbJunkyardPosJustChanged = true;
            lPosition.mGame.mbJunkyardPosIsLeft = liSide == 0;
            lPosition.Step();
            Check(lPosition.mTransitionCam.mBehaviour.mpSelected == (liSide ? &lPosition.mLeftToRight : &lPosition.mRightToLeft), "position event has same destination convention", liState, liSide);
        }
    }
    for (int liSide = 0; liSide < 2; ++liSide) {
        State lBusy;
        lBusy.mbIsLeft = liSide != 0;
        lBusy.mGame.mbJunkyardSelectionChangedMessageReceivedThisFrame = true;
        lBusy.mTransitionCam.mBehaviour.mbFinished = false;
        lBusy.Step();
        Check(lBusy.mTransitionCam.mBehaviour.miChanges == 0, "active unfinished movie gate", 8, liSide);

        State lReturn;
        lReturn.meState = State::E_STATE_WAIT_FOR_CAR_DROP;
        lReturn.mbIsLeft = liSide != 0;
        lReturn.mbWaitingToLookAtOriginalSelection = true;
        lReturn.mfTimeWaitingToLookAtOriginalSelection = 2.0f;
        lReturn.Step();
        Check(lReturn.mTransitionCam.mBehaviour.mpSelected == (liSide ? &lReturn.mLeftToRight : &lReturn.mRightToLeft), "timeout take points to original selection", 10, liSide);
        Check(lReturn.mbIsLeft != (liSide != 0) && lReturn.mTransitionCam.mBehaviour.miChanges == 1, "timeout toggles once", 10, liSide);
        Check(lReturn.meState == State::E_STATE_ACTIVE && lReturn.mbWaitingForCarToSpawn && !lReturn.mbWaitingForCarToTouchGround, "timeout returns to active", 10, liSide);

        State lWaiting;
        lWaiting.meState = State::E_STATE_WAIT_FOR_CAR_DROP;
        lWaiting.mbWaitingToLookAtOriginalSelection = true;
        lWaiting.mfTimeWaitingToLookAtOriginalSelection = 1.0f;
        lWaiting.Step();
        Check(lWaiting.mfTimeWaitingToLookAtOriginalSelection == 1.125f && lWaiting.mTransitionCam.mBehaviour.miChanges == 0, "timeout waits below two seconds", 10, liSide);

        State lSpawn;
        lSpawn.meState = State::E_STATE_WAIT_FOR_CAR_DROP;
        lSpawn.mbIsLeft = liSide != 0;
        lSpawn.mGame.mbJunkyardPosIsLeft = liSide != 0;
        lSpawn.mGame.mbJunkyardPlayerRespawnedThisFrame = true;
        lSpawn.Step();
        Check(lSpawn.meState == State::E_STATE_ACTIVE && lSpawn.mbWaitingForCarToTouchGround && lSpawn.mTransitionCam.mBehaviour.miChanges == 0, "matching respawn keeps destination take", 10, liSide);
    }
    std::printf("JunkyardSwitchCamera: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures != 0;
}
