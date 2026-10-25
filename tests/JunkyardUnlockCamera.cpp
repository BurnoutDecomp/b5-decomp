#include <cstdio>
using u32 = unsigned int;
constexpr int KI_CAMERA_STATE_JUNKYARD = 0x400000;
constexpr float KF_HOOK_BLEND = 1, KF_TAKE_PARAMETRIC_TIME_START = 0;
const char* const KPC_HOOK_FADE_OUT_BLACK_JUNK = "FadeOutBlack_Junk";
struct CameraData { int mState_uFlags = 0; };
namespace Camera {
int giFades = 0;
void EnsureEffectIsPlaying(CameraData&, int&, const char*, float) { ++giFades; }
}
struct Behaviour {
    float mfParametricTime = 1;
    int miRewinds = 0;
    void SetControllerParametricTime0To1(float time) { mfParametricTime = time; ++miRewinds; }
};
struct Handle {
    Behaviour mBehaviour;
    CameraData mCamera;
    int miReleases = 0;
    Behaviour* GetBehaviour() { return &mBehaviour; }
    CameraData& GetProducedCamera() { return mCamera; }
    void Release() { ++miReleases; }
};
struct GameState {
    enum { E_JY_CAR_UNLOCK = 3, E_JY_CAR_SELECT = 4 };
    int meJunkyardState = E_JY_CAR_UNLOCK;
    bool mbJunkyardCarUnlockTickedClosedThisFrame = false;
    bool mbNewCarUnlockedThisFrame = false;
};
struct SharedInfo { int mEffect = 0; int* mpEffectInterface = &mEffect; };
struct Fixture {
    enum { E_STATE_INTRO = 5, E_STATE_CAR_UNLOCK = 7 };
    int meState = E_STATE_CAR_UNLOCK, miNewMovies = 0;
    bool mbFadingOutCarUnlockMovie = false, mbHasCarsToUnlock = true;
    float mfTimeInState = 7;
    unsigned char maImpactEffect[20]{};
    Handle mTransitionCam, mCarUnlockCam, mIntroNoNewCars, mToGameplayInterpolater;
    CameraData mCamera;
    GameState mGame;
    SharedInfo mShared;
    CameraData& GetNonConstCamera() { return mCamera; }
    void StartCarUnlockCam(SharedInfo&) { ++miNewMovies; }
    void Step();
};
#include "junkyard_unlock_camera.inc"
int main() {
    int checks = 0, failures = 0;
    auto check = [&](bool pass, const char* name) {
        ++checks;
        if (!pass) { ++failures; std::printf("FAIL %s\n", name); }
    };
    Fixture f;
    f.Step();
    check(f.meState == 7 && f.mIntroNoNewCars.mBehaviour.miRewinds == 0,
          "live unlock keeps its camera");
    check((f.mCamera.mState_uFlags & KI_CAMERA_STATE_JUNKYARD) != 0, "junkyard flag preserved");
    f.mGame.mbJunkyardCarUnlockTickedClosedThisFrame = true; f.Step();
    check(f.mbFadingOutCarUnlockMovie && Camera::giFades == 1, "ticker completion fades the reveal");
    f.mGame.mbJunkyardCarUnlockTickedClosedThisFrame = false;
    f.mGame.meJunkyardState = GameState::E_JY_CAR_SELECT;
    f.Step();
    check(f.mIntroNoNewCars.mBehaviour.miRewinds == 1, "normal intro is rewound after the reveal");
    check(f.mIntroNoNewCars.mBehaviour.mfParametricTime == 0, "normal intro restarts at frame zero");
    check(f.mTransitionCam.mBehaviour.miRewinds == 0 &&
          f.mTransitionCam.mBehaviour.mfParametricTime == 1, "browse camera remains parked at the end");
    check(f.meState == Fixture::E_STATE_INTRO && f.mfTimeInState == 0, "handoff to intro resets timer");
    check(!f.mbHasCarsToUnlock, "unlock latch cleared");
    check(f.mCarUnlockCam.miReleases == 1, "reveal camera released");
    check(f.mToGameplayInterpolater.miReleases == 1, "previous interpolation released");
    Fixture next; next.mGame.mbNewCarUnlockedThisFrame = true; next.Step();
    check(next.miNewMovies == 1 && next.meState == 7, "next car starts its own reveal");
    check(next.mIntroNoNewCars.mBehaviour.miRewinds == 0, "intro waits until all reveals finish");
    std::printf("JunkyardUnlockCamera: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
