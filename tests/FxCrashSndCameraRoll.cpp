// FX-CRASHSND item 1 (crash parity 2026-09-24): the reset-on-track sting flood.
//
// The console keeps ONE camera-state edge per real change: MainDirector::Update @0x82274070
// rolls the published camera's PREVIOUS flag set to last frame's published CURRENT set just
// before carrying the camera over (DWARF CameraState::CopyFlagsToPrevious, inlined):
//   0x82275074  ori r10, r10, 0x3050 ; this + 0x33050 == mLastCamera(+0x32F10).mState(+0x138).mFlags
//   0x82275088  ldx r11, r30, r10    ; read BEFORE the operator= below
//   0x8227508C  std r11, var_3C8     ; lCamera(sp+0xC0).mState.mPreviousFlags (sp+0x208)
//   0x82275090  bl  Camera::operator=(this + 0x32F10, &lCamera)
// Every HasChanged consumer of the published camera keys on that roll -- among them
// CameraControl::UpdateParams @0x826F6540, which posts FxMessage_ResetOnTrack (type 5,
// 0x826F6AB4 li 5) when E_FLAG_RACING_GAMEPLAY_CAMERA (bit 3, rlwinm 0,28,28) CHANGED
// (0x826F6A28) and is SET (0x826F6A44) while E_FLAG_IS_PICTURE_PARADISE (bit 14, rlwinm
// 0,17,17) did NOT change (0x826F6A7C).
//
// run_fxcrashsnd_camera_roll.py extracts the PRODUCTION text into three includes:
//   fxcrashsnd_camerastate.inc  CameraState::SetFlag / ClearFlag / HasChanged (BrnCameraState.cpp)
//   fxcrashsnd_director_tail.inc  MainDirector::Update from the end of the slomo gate through
//                                 `mLastCamera = lCamera;` (BrnMainDirector.cpp)
//   fxcrashsnd_consumers.inc    the reset-on-track / camera-photo post conditions and the crash
//                               snapshot condition of CameraControl::UpdateParams, verbatim
// and this fixture replays the director's per-frame camera pipeline: a fresh stack camera
// (Camera::Construct -> CameraState::Clear @0x82220950: both frame sets = bit 0), the
// arbitrator's operator= copy of the owning state's camera (or LABEL_100's copy of mLastCamera
// when no player car is live), the extracted tail, then the consumers on the published camera.
// The owning state's camera carries a deliberately STALE previous set -- the console never
// publishes it, because the tail overwrites it.
#include "GameSource/Director/Camera/BrnCameraState.h"

#include <cstdio>
#include <vector>

static unsigned guAsserts = 0;

namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++guAsserts; return 0; }
void* EndAssert() { return nullptr; }
} }

namespace BrnDirector { namespace Camera {
#include "fxcrashsnd_camerastate.inc"
} }

using BrnDirector::Camera::CameraState;

// ---- fixtures ---------------------------------------------------------------------------
// The camera: only the state sub-object matters here. Camera::operator= @0x82233A80 copies all
// three CameraState qwords (0x82233B48..0x82233B5C), which the defaulted copy reproduces.
struct CameraFixture
{
    CameraState mState;
    CameraState&       GetState()       { return mState; }
    const CameraState& GetState() const { return mState; }
};

struct DirectorFixture
{
    CameraFixture mLastCamera;

    // The production tail of MainDirector::Update, with lCamera the frame's stack camera.
    void Tail(CameraFixture& lCamera)
    {
#include "fxcrashsnd_director_tail.inc"
    }
};

#include "fxcrashsnd_consumers.inc"

// ---- helpers ----------------------------------------------------------------------------
static u64 Head(const CameraFixture& lrCamera)     { return lrCamera.mState.mHeadFlags.GetBitField(0); }
static u64 Current(const CameraFixture& lrCamera)  { return lrCamera.mState.mCurrentFlags.GetBitField(0); }
static u64 Previous(const CameraFixture& lrCamera) { return lrCamera.mState.mPreviousFlags.GetBitField(0); }

static void Set(CameraFixture& lrCamera, u64 lu64Head, u64 lu64Current, u64 lu64Previous)
{
    lrCamera.mState.mHeadFlags.SetBitField(0, lu64Head);
    lrCamera.mState.mCurrentFlags.SetBitField(0, lu64Current);
    lrCamera.mState.mPreviousFlags.SetBitField(0, lu64Previous);
}

// Camera::Construct -> CameraState::Construct -> Clear (@0x82220950): std 0 to both frame sets,
// then `ori 1` on both. The head set is the validity account (zero for a fresh camera).
static CameraFixture FreshCamera()
{
    CameraFixture lCamera;
    Set(lCamera, 0, 1, 1);
    return lCamera;
}

static const u64 KU64_VALID     = 1ull << CameraState::E_FLAG_VALID;
static const u64 KU64_RACING    = 1ull << CameraState::E_FLAG_RACING_GAMEPLAY_CAMERA;
static const u64 KU64_CRASH     = 1ull << CameraState::E_FLAG_CRASH_CAMERA;
static const u64 KU64_PARADISE  = 1ull << CameraState::E_FLAG_IS_PICTURE_PARADISE;
static const u64 KU64_JUMPPHOTO = 1ull << CameraState::E_FLAG_JUMP_PHOTO;
static const u64 KU64_HEAD      = 0x5;   // an arbitrary validity-account pattern the tail must keep

struct FrameSpec
{
    bool mbLive;         // a live player car: the arbitrator copies the owning state's camera
    u64  mu64Current;    // the owning state's camera current set this frame
};

struct Run
{
    std::vector<int> maResetFrames;
    std::vector<int> maPhotoFrames;
    std::vector<int> maCrashSnapshotFrames;
    int miPreviousChainMismatches = 0;   // published previous != last frame's published current
    int miCarryMismatches         = 0;   // mLastCamera != published camera after the tail
    int miTailTouchedHeadOrCurrent = 0;  // the tail changed head / current of the frame camera
    int miCarriedChangedFlags     = 0;   // HasChanged(any flag) on a carried (not-live) frame
};

static Run Simulate(const std::vector<FrameSpec>& laFrames, u64 lu64StalePrevious)
{
    Run lRun;
    DirectorFixture lDirector;
    lDirector.mLastCamera = FreshCamera();          // MainDirector construct: mLastCamera.Construct()
    CameraFixture lStateCamera;                      // the arbitrator state's own camera
    u64 lu64LastPublishedCurrent = Current(lDirector.mLastCamera);

    for (int liFrame = 0; liFrame < static_cast<int>(laFrames.size()); ++liFrame)
    {
        const FrameSpec& lrSpec = laFrames[liFrame];
        Set(lStateCamera, KU64_HEAD, lrSpec.mu64Current, lu64StalePrevious);

        CameraFixture lCamera = FreshCamera();       // Camera lCamera; lCamera.Construct();
        if (lrSpec.mbLive)
            lCamera = lStateCamera;                  // UpdateArbitrator: operator= of the state camera
        else
            lCamera = lDirector.mLastCamera;         // LABEL_100: carry last frame's camera

        const u64 lu64HeadBefore    = Head(lCamera);
        const u64 lu64CurrentBefore = Current(lCamera);
        lDirector.Tail(lCamera);

        if (Head(lCamera) != lu64HeadBefore || Current(lCamera) != lu64CurrentBefore)
            ++lRun.miTailTouchedHeadOrCurrent;
        if (Head(lDirector.mLastCamera) != Head(lCamera) ||
            Current(lDirector.mLastCamera) != Current(lCamera) ||
            Previous(lDirector.mLastCamera) != Previous(lCamera))
            ++lRun.miCarryMismatches;
        if (Previous(lCamera) != lu64LastPublishedCurrent)
            ++lRun.miPreviousChainMismatches;
        lu64LastPublishedCurrent = Current(lCamera);

        // The published camera (OutputBuffer::SetCameraOutput(lCamera)) is what the sound
        // module's RootInputBuffer copies and CameraControl::UpdateParams reads.
        const CameraState& lrPublished = lCamera.GetState();
        if (ResetOnTrackEdge(lrPublished))  lRun.maResetFrames.push_back(liFrame);
        if (CameraPhotoEdge(lrPublished))   lRun.maPhotoFrames.push_back(liFrame);
        if (CrashSnapshotEdge(lrPublished)) lRun.maCrashSnapshotFrames.push_back(liFrame);
        if (!lrSpec.mbLive)
        {
            for (u32 luFlag = 0; luFlag < CameraState::KU_NUM_FLAGS; ++luFlag)
                if (lrPublished.HasChanged(luFlag))
                    ++lRun.miCarriedChangedFlags;
        }
    }
    return lRun;
}

static void Append(std::vector<FrameSpec>& laFrames, int liCount, bool lbLive, u64 lu64Current)
{
    for (int i = 0; i < liCount; ++i)
        laFrames.push_back(FrameSpec{ lbLive, lu64Current });
}

static unsigned guChecks = 0;
static unsigned guFailures = 0;

static void Check(bool lbPassed, const char* lpcLabel, long long liGot, long long liWant)
{
    ++guChecks;
    if (!lbPassed)
        ++guFailures;
    std::printf("%s  %s (got %lld, want %lld)\n", lbPassed ? "PASS" : "FAIL", lpcLabel, liGot, liWant);
}

static void CheckEq(long long liGot, long long liWant, const char* lpcLabel)
{
    Check(liGot == liWant, lpcLabel, liGot, liWant);
}

static long long At(const std::vector<int>& laFrames, size_t luIndex)
{
    return luIndex < laFrames.size() ? laFrames[luIndex] : -1;
}

int main()
{
    // ---- S1: a car placement. A crash/reset camera for 10 frames, then the gameplay camera
    // for 50. The console posts ONE reset-on-track sting (the frame the gameplay camera comes
    // back) and sets/clears the crash snapshot once each way.
    {
        std::vector<FrameSpec> laFrames;
        Append(laFrames, 10, true, KU64_VALID | KU64_CRASH);
        Append(laFrames, 50, true, KU64_VALID | KU64_RACING);
        const Run lRun = Simulate(laFrames, 0);
        CheckEq(static_cast<long long>(lRun.maResetFrames.size()), 1, "S1 placement: reset-on-track posts");
        CheckEq(At(lRun.maResetFrames, 0), 10, "S1 placement: the post lands on the gameplay-camera entry frame");
        CheckEq(static_cast<long long>(lRun.maCrashSnapshotFrames.size()), 2, "S1 placement: crash snapshot edges (on, off)");
        CheckEq(At(lRun.maCrashSnapshotFrames, 1), 10, "S1 placement: the crash snapshot clears on frame 10");
        CheckEq(lRun.miPreviousChainMismatches, 0, "S1 published previous set == last frame's published current set");
        CheckEq(lRun.miCarryMismatches, 0, "S1 mLastCamera == the published camera after the tail");
        CheckEq(lRun.miTailTouchedHeadOrCurrent, 0, "S1 the tail leaves the head and current sets alone");
    }

    // ---- S2: two separate gameplay-camera entries -> exactly two stings.
    {
        std::vector<FrameSpec> laFrames;
        Append(laFrames, 5,  true, KU64_VALID);
        Append(laFrames, 10, true, KU64_VALID | KU64_RACING);
        Append(laFrames, 5,  true, KU64_VALID);
        Append(laFrames, 10, true, KU64_VALID | KU64_RACING);
        const Run lRun = Simulate(laFrames, 0);
        CheckEq(static_cast<long long>(lRun.maResetFrames.size()), 2, "S2 two entries: reset-on-track posts");
        CheckEq(At(lRun.maResetFrames, 0), 5, "S2 two entries: first post frame");
        CheckEq(At(lRun.maResetFrames, 1), 20, "S2 two entries: second post frame");
    }

    // ---- S3: entering Picture Paradise raises bit 14 on the same frame -> no sting (the third
    // term, 0x826F6A7C); a later gameplay re-entry with bit 14 steady -> one sting.
    {
        std::vector<FrameSpec> laFrames;
        Append(laFrames, 3, true, KU64_VALID);
        Append(laFrames, 5, true, KU64_VALID | KU64_RACING | KU64_PARADISE);
        Append(laFrames, 1, true, KU64_VALID | KU64_PARADISE);
        Append(laFrames, 4, true, KU64_VALID | KU64_RACING | KU64_PARADISE);
        const Run lRun = Simulate(laFrames, 0);
        CheckEq(static_cast<long long>(lRun.maResetFrames.size()), 1, "S3 picture paradise: reset-on-track posts");
        CheckEq(At(lRun.maResetFrames, 0), 9, "S3 picture paradise: the post is the steady-bit-14 re-entry");
    }

    // ---- S4: frames with no live player car carry mLastCamera (LABEL_100): nothing changes.
    {
        std::vector<FrameSpec> laFrames;
        Append(laFrames, 5, true,  KU64_VALID | KU64_RACING);
        Append(laFrames, 5, false, 0);
        const Run lRun = Simulate(laFrames, 0);
        CheckEq(static_cast<long long>(lRun.maResetFrames.size()), 1, "S4 carried frames: total reset-on-track posts");
        CheckEq(At(lRun.maResetFrames, 0), 0, "S4 carried frames: the one post is the live entry");
        CheckEq(lRun.miCarriedChangedFlags, 0, "S4 carried frames: no flag reads as changed");
    }

    // ---- S5: the camera-photo sting (JUMP_PHOTO rising edge, 0x826F6970/0x826F6994) is one-shot too.
    {
        std::vector<FrameSpec> laFrames;
        Append(laFrames, 2,  true, KU64_VALID);
        Append(laFrames, 10, true, KU64_VALID | KU64_JUMPPHOTO);
        const Run lRun = Simulate(laFrames, 0);
        CheckEq(static_cast<long long>(lRun.maPhotoFrames.size()), 1, "S5 jump photo: camera-photo posts");
        CheckEq(At(lRun.maPhotoFrames, 0), 2, "S5 jump photo: post frame");
    }

    // ---- S6: the state camera's own previous set is never published: an all-ones stale set
    // must not swallow the sting either.
    {
        std::vector<FrameSpec> laFrames;
        Append(laFrames, 2, true, KU64_VALID);
        Append(laFrames, 8, true, KU64_VALID | KU64_RACING);
        const Run lRun = Simulate(laFrames, (1ull << CameraState::KU_NUM_FLAGS) - 1);
        CheckEq(static_cast<long long>(lRun.maResetFrames.size()), 1, "S6 stale all-ones previous: reset-on-track posts");
        CheckEq(At(lRun.maResetFrames, 0), 2, "S6 stale all-ones previous: post frame");
        CheckEq(lRun.miPreviousChainMismatches, 0, "S6 published previous set == last frame's published current set");
    }

    // ---- S7: the tail's own stores, one call (0x82275088 read of mLastCamera BEFORE 0x82275090).
    {
        DirectorFixture lDirector;
        Set(lDirector.mLastCamera, 0x3, 0x0808, 0x0007);
        CameraFixture lCamera;
        Set(lCamera, 0xA, 0x1234, 0x0F0F);
        lDirector.Tail(lCamera);
        CheckEq(static_cast<long long>(Previous(lCamera)), 0x0808, "S7 lCamera previous = old mLastCamera current");
        CheckEq(static_cast<long long>(Current(lCamera)), 0x1234, "S7 lCamera current kept");
        CheckEq(static_cast<long long>(Head(lCamera)), 0xA, "S7 lCamera head kept");
        CheckEq(static_cast<long long>(Previous(lDirector.mLastCamera)), 0x0808, "S7 mLastCamera previous = old mLastCamera current");
        CheckEq(static_cast<long long>(Current(lDirector.mLastCamera)), 0x1234, "S7 mLastCamera current = lCamera current");
        CheckEq(static_cast<long long>(Head(lDirector.mLastCamera)), 0xA, "S7 mLastCamera head = lCamera head");
    }

    CheckEq(guAsserts, 0, "no CameraState index assert fired");

    std::printf("FxCrashSndCameraRoll: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
