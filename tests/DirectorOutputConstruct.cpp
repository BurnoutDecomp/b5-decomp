// ARTIST OutputBuffer::Construct @0x8225C960: selective reset on reused IO-stack storage.
// The runner compiles the production constructor/accessor bodies and their real camera/base
// dependencies. Only the diagnostic sink is isolated; every tested reset runs production code.
#include <cstdio>
#include <cstring>
#include <type_traits>

#include "GameSource/Director/DirectorModule/BrnDirectorModuleIOOutputBuffer.hpp"

static unsigned suChecks = 0, suFailures = 0, suAsserts = 0;
static void Check(bool lbPass, const char* lpcMessage)
{
    ++suChecks;
    if (!lbPass)
    {
        ++suFailures;
        std::printf("FAIL %s\n", lpcMessage);
    }
}

namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcExpression, const char*, int)
{
    ++suAsserts;
    std::printf("ASSERT %s\n", lpcExpression);
    return 0;
}
void* EndAssert() { return nullptr; }
} }

#include "director_output_buffer.inc"

using BrnDirector::DirectorIO::OutputBuffer;
using BrnDirector::Camera::Camera;
using BrnReplays::ReplayIO::RequestInterface;

static_assert(std::is_same<decltype(OutputBuffer::mReplayRequestInterface),
                           RequestInterface>::value, "use the canonical replay interface");
static_assert(sizeof(RequestInterface) == 11 * sizeof(BrnReplays::BaseSerialiser*),
              "all eleven replay pointers widen on x64");

template <class Queue>
static void CheckQueue(Queue& lrQueue)
{
    Check(lrQueue.mbIsConstructed, "request queue is constructed");
    Check(lrQueue.GetLength() == 0, "reused queue has no stale events");
    Check(lrQueue.miBufferWritePos == lrQueue.miFirstEventOffset,
          "queue write cursor starts at the first record");
    const auto luFirstRecord = reinterpret_cast<uintptr_t>(lrQueue.macData)
                            + lrQueue.miFirstEventOffset;
    Check(luFirstRecord % 16 == 0, "queue record alignment follows its real host address");
}

static void CheckTimer(const CgsSystem::TimerRequests& lrTimer)
{
    Check(lrTimer.muFlags == 0 && !lrTimer.IsStartRequested() && !lrTimer.IsStopRequested()
          && !lrTimer.IsMultiplierRequested(), "timer request flags are cleared");
    Check(lrTimer.mfMultiplier == 1.0f, "timer multiplier resets to identity");
}

int main()
{
    struct GuardedBuffer
    {
        u64 mauBefore[2];
        OutputBuffer mBuffer;
        u64 mauAfter[2];
    } lStorage;

    // Poison the same allocation twice, with different stale data and valid bool sentinels.
    // The camera's independent Construct provides the full named-type reference, including
    // the camera fields that ARTIST intentionally leaves untouched.
    const unsigned char kauPoison[] = { 0xA5, 0x5A };
    for (const unsigned char luPoison : kauPoison)
    {
        std::memset(&lStorage, luPoison, sizeof(lStorage));
        auto& lrBuffer = lStorage.mBuffer;
        auto& lrDirector = lrBuffer.mDirectorOutputInterface;
        lrDirector.mbIntroCameraStartingToLookAtRival = true;
        lrDirector.muIntroCameraRivalryNumber = 0x12345678;
        lrDirector.mfIntroCameraSecsRemaining = 6.25f;
        lrDirector.mbStartRivalPresentation = true;
        lrDirector.mbFinishedPostRacePresentation = true;
        lrDirector.mbPauseRequest = true;
        lrDirector.mbPauseRequestState = (luPoison == 0xA5);
        lrBuffer.mbRequestHookEnumeration = true;
        lrBuffer.mbDirectorSettingsChanged = true;

        // Seed every host-width pointer with a non-null value before reuse.
        for (auto& lpSerialiser : lrBuffer.mReplayRequestInterface.mapSerialisers)
            lpSerialiser = reinterpret_cast<BrnReplays::BaseSerialiser*>(&lStorage);

        Camera lExpectedCamera;
        std::memcpy(&lExpectedCamera, &lrBuffer.mCameraOutput, sizeof(lExpectedCamera));
        lExpectedCamera.Construct();
        unsigned char lauCgsCamera[sizeof(lrBuffer.mCgsCamera)];
        unsigned char lauDirectorProfile[sizeof(lrBuffer.mDirectorInterface)];
        std::memcpy(lauCgsCamera, &lrBuffer.mCgsCamera, sizeof(lauCgsCamera));
        std::memcpy(lauDirectorProfile, &lrBuffer.mDirectorInterface, sizeof(lauDirectorProfile));

        lrBuffer.Construct();

        Check(lStorage.mauBefore[0] == lStorage.mauBefore[1]
              && lStorage.mauAfter[0] == lStorage.mauBefore[0]
              && lStorage.mauAfter[1] == lStorage.mauBefore[0], "constructor stays inside the allocation");
        Check(!lrBuffer.IsBufferLocked(), "base construct removes stale read/write locks");
        Check(std::memcmp(&lrBuffer.mCameraOutput, &lExpectedCamera, sizeof(Camera)) == 0,
              "embedded camera receives the complete original Construct");
        Check(lrBuffer.mCameraOutput.mfFOV == 90.0f
              && lrBuffer.mCameraOutput.mfAspectRatio == 1.7777778f,
              "camera defaults retain original FOV and aspect");
        Check(lrBuffer.mCameraOutput.mTransform.xAxis.x == 1.0f
              && lrBuffer.mCameraOutput.mTransform.yAxis.y == 1.0f
              && lrBuffer.mCameraOutput.mTransform.zAxis.z == 1.0f
              && lrBuffer.mCameraOutput.mTransform.wAxis.x == 0.0f,
              "camera transform is initialized even at the origin");
        Check(std::memcmp(&lrBuffer.mCgsCamera, lauCgsCamera, sizeof(lauCgsCamera)) == 0,
              "Construct preserves the unpublished graphics camera");
        Check(std::memcmp(lrBuffer.mDirectorInterface, lauDirectorProfile,
                          sizeof(lauDirectorProfile)) == 0, "Construct preserves director profile data");
        for (auto* lpSerialiser : lrBuffer.mReplayRequestInterface.mapSerialisers)
            Check(lpSerialiser == nullptr, "each replay serialiser slot is reset");
        Check(!lrDirector.mbIntroCameraStartingToLookAtRival && !lrDirector.mbStartRivalPresentation
              && !lrDirector.mbFinishedPostRacePresentation && !lrDirector.mbPauseRequest,
              "four director request flags are reset");
        Check(lrDirector.muIntroCameraRivalryNumber == 0x12345678
              && lrDirector.mfIntroCameraSecsRemaining == 6.25f,
              "director rival payload survives selective reset");
        Check(lrDirector.mbPauseRequestState == (luPoison == 0xA5),
              "director pause state survives selective reset");
        CheckQueue(lrBuffer.mResourceInterface.mRequestQueue);
        CheckQueue(lrBuffer.mVaultRequestInterface.mRequestQueue);
        CheckTimer(lrBuffer.mTimerRequestInterface.mGameTimer);
        CheckTimer(lrBuffer.mTimerRequestInterface.mSimTimer);
        Check(!lrBuffer.mbRequestHookEnumeration && !lrBuffer.mbDirectorSettingsChanged,
              "both GUI request latches are reset");

        lrBuffer.LockForWrite();
        Check(lrBuffer.GetDirectorOutputIn() == reinterpret_cast<u8*>(&lrDirector),
              "write accessor retains the whole-object director byte view");
        Check(lrBuffer.GetReplayRequestI() == reinterpret_cast<u8*>(&lrBuffer.mReplayRequestInterface),
              "write accessor retains the whole-object replay byte view");
        lrBuffer.UnlockForWrite();
        lrBuffer.LockForRead();
        Check(lrBuffer.GetDirectorOu() == reinterpret_cast<u8*>(&lrDirector),
              "read accessor retains the whole-object director byte view");
        Check(lrBuffer.GetReplayRe() == reinterpret_cast<u8*>(&lrBuffer.mReplayRequestInterface),
              "read accessor retains the whole-object replay byte view");
        Check(lrBuffer.GetCameraOutput() == &lrBuffer.mCameraOutput,
              "camera accessor uses the constructed embedded object");
        lrBuffer.UnlockForRead();
    }
    Check(suAsserts == 0, "fresh and reused payloads satisfy production base/queue assertions");
    std::printf("DirectorOutputConstruct: %u checks, %u failures\n", suChecks, suFailures);
    return suFailures ? 1 : 0;
}
