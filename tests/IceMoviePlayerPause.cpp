#include <cstdio>
#include <initializer_list>

using s32 = int;

// The production Update runs against a scheduling fixture. Behaviours are skipped
// during pause unless their manager registration requests updates during pause.
namespace BrnDirector
{
namespace Camera
{
    struct Camera { int miFrame = 0; };
    struct BehaviourInterpolate
    {
        int miMode = 0;
        float mfElapsed = 0.0f, mfDuration = 0.0f;
        bool mbSetup = false;
        bool HasFinished() const { return mbSetup && mfElapsed >= mfDuration; }
        void SetInterpolationMode(int liMode) { miMode = liMode; }
        void SetParameters(const void*) {}
        void SetupDuration(float lfDuration) { mfDuration = lfDuration; }
        void SetupCameraAFromCamera(const Camera&) {}
        void SetupCameraBFromHelper(int, const struct BehaviourManager&) {}
        void Setup() { mbSetup = true; }
    };
    struct BehaviourManager;
    struct Handle
    {
        bool mbAllocated = false, mbUpdatesDuringPause = false;
        BehaviourInterpolate mBehaviour;
        Camera mCamera;
        BehaviourManager* mpManager = nullptr;
        bool IsAllocated() const { return mbAllocated; }
        BehaviourInterpolate* GetBehaviour() { return &mBehaviour; }
        Camera GetProducedCamera() const { return mCamera; }
        BehaviourManager* GetManager() { return mpManager; }
    };
    struct BehaviourManager
    {
        int miAllocations = 0;
        void NewBehaviourInterpolate(Handle& lrHandle, void*, const void*, int)
        {
            ++miAllocations;
            lrHandle.mbAllocated = true;
            lrHandle.mpManager = this;
        }
        void SetBehaviourUpdatesDuringPause(Handle& lrHandle, bool lbUpdate)
        {
            lrHandle.mbUpdatesDuringPause = lbUpdate;
        }
        void ReleaseBehaviour(Handle& lrHandle) { lrHandle.mbAllocated = false; }
        void Tick(Handle& lrHandle, bool lbPaused)
        {
            if (!lrHandle.IsAllocated() || (lbPaused && !lrHandle.mbUpdatesDuringPause)) return;
            const float lfWorldStep = lbPaused ? 0.0f : 0.25f;
            lrHandle.mBehaviour.mfElapsed += lrHandle.mBehaviour.miMode == 2 ? 0.25f : lfWorldStep;
            ++lrHandle.mCamera.miFrame;
        }
    };
}
struct ICEWrapper
{
    Camera::Camera mCamera;
    Camera::Camera* GetCamera() { return &mCamera; }
    bool IsPlayingMovie() const { return true; }
};
struct IceMovie { bool GetShouldFlash() const { return false; } };
struct Playlist
{
    struct Indices { int GetItem(int liIndex) const { return liIndex; } } mMoviePoolIndicies;
    IceMovie mMoviePool[1];
    int GetMovieCount() const { return 1; }
};
struct ICEMoviePlayer
{
    bool mbIsPlaying = true, mbHasReachedEnd = false, mbInterpolateIn = false;
    bool mbInterpolateOutNow = true, mbInterpolateOutUpdatesDuringPause = false;
    bool mbFirstFrameOfPlaying = false, mbIsLooping = true, mbShouldInterpolateOut = true;
    Camera::Handle mOutInterpolator, mInInterpolator;
    Camera::Camera mCamera;
    ICEWrapper* mpICEWrapper = nullptr;
    const void* mpInterpolateOutParams = nullptr;
    float mfInterpolateOutDuration = 0.5f;
    int mToBehaviourHelperIndex = 1, miCurrentMovie = 0;
    Playlist mPlaylist;
    void ApplyFlashHookToCamera() {}
    void StartCurrentMovie() {}
    void Update(Camera::BehaviourManager& lrManager);
};
}

#include "ice_movie_player_pause.inc"

int main()
{
    int liChecks = 0, liFailures = 0;
    const auto Check = [&](bool lbPass, const char* lpcLabel)
    {
        ++liChecks;
        if (!lbPass) { ++liFailures; std::printf("FAIL: %s\n", lpcLabel); }
    };
    for (bool lbRequested : {false, true})
    {
        BrnDirector::Camera::BehaviourManager lManager;
        BrnDirector::ICEWrapper lWrapper;
        BrnDirector::ICEMoviePlayer lPlayer;
        lPlayer.mpICEWrapper = &lWrapper;
        lPlayer.mbInterpolateOutUpdatesDuringPause = lbRequested;
        lPlayer.Update(lManager);
        Check(lPlayer.mOutInterpolator.IsAllocated(), "exit blend allocated");
        Check(lPlayer.mOutInterpolator.mbUpdatesDuringPause == lbRequested, "pause registration follows request");
        for (int liTick = 0; liTick < 3; ++liTick)
        {
            lManager.Tick(lPlayer.mOutInterpolator, true);
            lPlayer.Update(lManager);
        }
        Check(lPlayer.mbHasReachedEnd == lbRequested, "requested exit completes while paused");
        Check(lManager.miAllocations == 1, "exit blend is allocated once");
        if (!lbRequested)
        {
            for (int liTick = 0; liTick < 3; ++liTick)
            {
                lManager.Tick(lPlayer.mOutInterpolator, false);
                lPlayer.Update(lManager);
            }
            Check(lPlayer.mbHasReachedEnd, "ordinary exit completes when simulation resumes");
        }
    }
    std::printf("IceMoviePlayerPause: %d checks, %d failures\n", liChecks, liFailures);
    return liFailures != 0;
}
