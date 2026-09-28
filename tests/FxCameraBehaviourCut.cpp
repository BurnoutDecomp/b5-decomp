// ARTIST MainDirector::Update @0x822745D0..0x82274628 compares Camera +0x50
// with a static previous pointer and ORs flag 6 when they differ. It must retain
// an already requested sub-take cut, and always update the previous pointer.
// The generated include is extracted from production, not a restated branch.
#include <cstdint>
#include <cstdio>

namespace Camera
{
    struct Behaviour {};
    struct CameraState
    {
        enum { E_FLAG_NEW_THIS_FRAME = 6 };
        uint64_t flags = 0;
        int writes = 0;
        bool IsFlagSet(unsigned bit) const { return (flags & (uint64_t(1) << bit)) != 0; }
        void SetFlag(unsigned bit, bool value)
        {
            ++writes;
            if (value) flags |= uint64_t(1) << bit;
            else flags &= ~(uint64_t(1) << bit);
        }
    };
    struct Camera
    {
        const Behaviour* mpDebugInfoBehaviour = 0;
        CameraState mState;
    };
}

static void Publish(Camera::Camera& lCamera)
{
#include "fx_camera_behaviour_cut.inc"
}

static int checks, failures;
static void Check(bool ok, const char* label)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL %s\n", label); }
}

int main()
{
    Camera::Behaviour chase, countdown, crash;
    Camera::Camera camera;
    constexpr uint64_t cut = uint64_t(1) << 6;
    constexpr uint64_t other = (uint64_t(1) << 13) | (uint64_t(1) << 40) | 2;
    Publish(camera);
    Check(camera.mState.flags == 0, "initial null camera remains uncut");
    camera.mpDebugInfoBehaviour = &chase;
    camera.mState.flags = other;
    Publish(camera);
    Check(camera.mState.flags == (other | cut), "first producing behaviour is a cut");
    Check(camera.mState.writes == 1, "one cut store");
    camera.mState.flags = other;
    Publish(camera);
    Check(camera.mState.flags == other, "same behaviour does not force a cut");
    camera.mpDebugInfoBehaviour = &countdown;
    Publish(camera);
    Check(camera.mState.flags == (other | cut), "countdown first frame resets inertia");
    camera.mState.flags = other;
    Publish(camera);
    Check(camera.mState.flags == other, "countdown can smooth after its first frame");
    camera.mpDebugInfoBehaviour = &crash;
    camera.mState.flags = other | cut;
    int before = camera.mState.writes;
    Publish(camera);
    Check(camera.mState.flags == (other | cut), "existing sub-take cut survives transition");
    Check(camera.mState.writes == before, "existing cut is not rewritten");
    camera.mState.flags = other;
    Publish(camera);
    Check(camera.mState.flags == other, "already-cut transition still latches the pointer");
    camera.mpDebugInfoBehaviour = 0;
    Publish(camera);
    Check(camera.mState.flags == (other | cut), "transition to null is also a cut");
    Camera::Camera second;
    second.mpDebugInfoBehaviour = &chase;
    Publish(second);
    Check(second.mState.flags == cut, "another output observes the shared last pointer");
    second.mState.flags = 0;
    Publish(second);
    Check(second.mState.flags == 0, "unchanged second output does not recut");
    std::printf("FxCameraBehaviourCut: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
