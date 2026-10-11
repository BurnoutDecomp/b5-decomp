// Grouped numeric regression for the live Sound debug-menu tuning cells.
// The runner supplies the current production MusicStream/SpeedStreamControl method bodies
// and their real global initializers. Expectations below are independently pinned to ARTIST:
// MusicStream::UpdateVoiceParams @826BB6F0 writes filter slots 3/4/6/7; speed-stream
// UpdateParams @826BA2A0 uses threshold - width*0.75; UpdateDuckers @8269BA08 ramps around
// threshold +/- width/2 and converts each clamped input with fctiwz (truncate toward zero).
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

using f32 = float;
using s32 = std::int32_t;
using u32 = std::uint32_t;
using u8 = std::uint8_t;

namespace Test
{
    int giChecks = 0;
    int giFailures = 0;

    void Check(const char* apLabel, bool abCondition)
    {
        ++giChecks;
        if (!abCondition)
        {
            ++giFailures;
            std::printf("FAIL  %s\n", apLabel);
        }
        else
            std::printf("PASS  %s\n", apLabel);
    }

    bool Near(f32 afActual, f32 afExpected)
    {
        return std::fabs(afActual - afExpected) <= 0.00001f;
    }
}

#define CHECK(label, expression) Test::Check((label), (expression))
#define CGS_ASSERT(expression, message) do { (void)(expression); (void)(message); } while (0)

namespace CgsSound
{
namespace Playback
{
    struct Name
    {
        static u32 MakeHash(const char* apText)
        {
            u32 luHash = 2166136261u;
            while (*apText)
            {
                luHash ^= static_cast<u8>(*apText++);
                luHash *= 16777619u;
            }
            return luHash;
        }
    };
}

namespace Logic
{
    class VoiceWrapper
    {
    public:
        s32 miUpdateStage = 0;
        f32 mafParameters[8] = {};
        u32 mauParameterNames[8] = {};
        f32 mafGains[2] = {};
        u32 mauGainNames[2] = {};

        s32 GetUpdateStage() const { return miUpdateStage; }
        void SetParameter(s32 aiIndex, f32 afValue, const u32* apName)
        {
            if (aiIndex >= 0 && aiIndex < 8)
            {
                mafParameters[aiIndex] = afValue;
                mauParameterNames[aiIndex] = apName ? *apName : 0;
            }
        }
        void SetGain(u32 auIndex, f32 afGain, const u32* apName)
        {
            if (auIndex < 2)
            {
                mafGains[auIndex] = afGain;
                mauGainNames[auIndex] = apName ? *apName : 0;
            }
        }
    };

}

namespace Utils
{
    struct Curve
    {
        enum ECurveType { E_ONE_MINUS_EQPWR = 3 };
        static f32 GetOutput(f32 afInput, ECurveType) { return afInput; }
    };
}

namespace Diag
{
    void StreamDiagPrintf(const char*, ...) {}
}
}

namespace CgsDev
{
namespace Assert
{
    int BeginAssert() { return 0; }
    int FireAssert(const char*, const char*, int) { return 0; }
    void* EndAssert() { return nullptr; }
}
}

namespace BrnSound
{
namespace Logic
{
    class BrnEffectControl
    {
    public:
        s32 maiMixerInputs[4] = {};
        void SetMixerInputValue(s32 aiIndex, s32 aiValue)
        {
            if (aiIndex >= 0 && aiIndex < 4)
                maiMixerInputs[aiIndex] = aiValue;
        }
    };

    struct MusicCreateParams
    {
        u32 mSendName = 0;
        u32 mContentSpecName = 0;
        s32 miSendIndex = 0;
    };

    class MusicStream
    {
    public:
        enum EState { E_STOPPED = 0, E_PLAYING = 1, E_STOP_REQUESTED = 2,
                      E_FADING_OUT = 3, E_PAUSING = 4 };
        u32 mu32VoiceUpdateStage = 0;
        u32 mu32PrevVoiceUpdateStage = 0;
        MusicCreateParams mCreateParams;
        EState meState = E_PLAYING;
        f32 mfFadeTime = 0.0f;
        f32 mfFadeDuration = 0.25f;
        f32 mfVolume = 1.0f;
        f32 mfHighPassFrequency = 0.0f;
        f32 mfLowPassFrequency = 96000.0f;
        bool mbInternalPause = false;
        bool mbStreamPaused = false;
        u8 mu8OutputSlot = 0;
        int miDiagLastGainQ = -1;

        void UpdateVoiceParams(CgsSound::Logic::VoiceWrapper&, f32, f32);
    };
}

namespace Vehicles
{
namespace Engines
{
    template <typename T>
    struct DataPoint
    {
        T mCurrent{};
        T mPrevious{};
        T GetCurrent() const { return mCurrent; }
        void Update(T aValue) { mPrevious = mCurrent; mCurrent = aValue; }
        void Flush(T aValue) { mPrevious = aValue; mCurrent = aValue; }
    };

    struct PhysicsControl
    {
        struct PhysicsData
        {
            DataPoint<bool> IsCrashing;
            DataPoint<bool> IsBoosting;
            DataPoint<f32> mSpeedMPH;
        };

        PhysicsData mData;
        const PhysicsData& GetPhysicsData() const { return mData; }
    };
}

namespace Environment
{
    class SpeedStreamControl : public BrnSound::Logic::BrnEffectControl
    {
    public:
        BrnSound::Vehicles::Engines::PhysicsControl* mpPhysicsControl = nullptr;
        f32 mfSpeedHighTime = 0.0f;
        f32 mfBoostHighTime = 0.0f;
        BrnSound::Vehicles::Engines::DataPoint<bool> mSpeedStreamOn;
        BrnSound::Vehicles::Engines::DataPoint<bool> mBoostStreamOn;

        void UpdateParams(f32);
        void UpdateDuckers();
    };
}
}
}

// These are definitions copied from the real owning production files by the runner. Keeping
// them in this translation unit makes the default-value checks exercise the shipped cells.
#include "sound_stream_debug_menu.inc"

namespace
{
    void SetSpeed(BrnSound::Vehicles::Engines::PhysicsControl& arPhysics, f32 afSpeed)
    {
        arPhysics.mData.mSpeedMPH.Update(afSpeed);
        arPhysics.mData.IsCrashing.Update(false);
        arPhysics.mData.IsBoosting.Update(false);
    }

    void CheckMixer(const char* apLabel, const BrnSound::Vehicles::Environment::SpeedStreamControl& arControl,
                    s32 aiHigh, s32 aiLow, s32 aiCrash = 0, s32 aiBoost = 0)
    {
        CHECK(apLabel, arControl.maiMixerInputs[0] == aiHigh &&
                       arControl.maiMixerInputs[1] == aiLow &&
                       arControl.maiMixerInputs[2] == aiCrash &&
                       arControl.maiMixerInputs[3] == aiBoost);
    }
}

int main()
{
    using BrnSound::Logic::KF_MUSIC_HIGH_PASS_ORDER;
    using BrnSound::Logic::KF_MUSIC_HIGH_PASS_Q;
    using BrnSound::Logic::KF_MUSIC_LOW_PASS_ORDER;
    using BrnSound::Logic::KF_MUSIC_LOW_PASS_Q;
    using BrnSound::Vehicles::Environment::KF_SPEED_STREAM_CROSSFADE_WIDTH;
    using BrnSound::Vehicles::Environment::KF_SPEED_STREAM_THRESHOLD_MPH;

    CHECK("ARTIST music filter defaults are present",
          Test::Near(KF_MUSIC_HIGH_PASS_ORDER, 4.0f) &&
          Test::Near(KF_MUSIC_LOW_PASS_ORDER, 4.0f) &&
          Test::Near(KF_MUSIC_HIGH_PASS_Q, 0.95f) &&
          Test::Near(KF_MUSIC_LOW_PASS_Q, 0.99f));
    CHECK("ARTIST speed threshold and width defaults are present",
          Test::Near(KF_SPEED_STREAM_THRESHOLD_MPH, 105.0f) &&
          Test::Near(KF_SPEED_STREAM_CROSSFADE_WIDTH, 40.0f));

    BrnSound::Logic::MusicStream lrMusic;
    CgsSound::Logic::VoiceWrapper lrVoice;
    lrMusic.mCreateParams.mSendName = 0x12345678u;
    lrVoice.miUpdateStage = 6;
    lrMusic.UpdateVoiceParams(lrVoice, 0.75f, 0.0f);
    CHECK("music filter defaults reach voice slots 3, 4, 6, and 7",
          Test::Near(lrVoice.mafParameters[3], 4.0f) &&
          Test::Near(lrVoice.mafParameters[4], 0.95f) &&
          Test::Near(lrVoice.mafParameters[6], 4.0f) &&
          Test::Near(lrVoice.mafParameters[7], 0.99f));
    CHECK("voice update stage is latched current then previous",
          lrMusic.mu32VoiceUpdateStage == 6u && lrMusic.mu32PrevVoiceUpdateStage == 0u);

    KF_MUSIC_HIGH_PASS_ORDER = 1.25f;
    KF_MUSIC_HIGH_PASS_Q = 0.65f;
    KF_MUSIC_LOW_PASS_ORDER = 2.5f;
    KF_MUSIC_LOW_PASS_Q = 0.8f;
    lrVoice.miUpdateStage = 7;
    lrMusic.UpdateVoiceParams(lrVoice, 0.75f, 0.0f);
    CHECK("mutated music filter globals reach their four original voice slots",
          Test::Near(lrVoice.mafParameters[3], 1.25f) &&
          Test::Near(lrVoice.mafParameters[4], 0.65f) &&
          Test::Near(lrVoice.mafParameters[6], 2.5f) &&
          Test::Near(lrVoice.mafParameters[7], 0.8f));
    CHECK("music stage history advances on each voice update",
          lrMusic.mu32VoiceUpdateStage == 7u && lrMusic.mu32PrevVoiceUpdateStage == 6u);

    lrMusic.mfLowPassFrequency = 100.0f;
    lrMusic.mbInternalPause = true;
    lrMusic.UpdateVoiceParams(lrVoice, 0.75f, 0.0f);
    CHECK("low-pass frequency retains the original 300 Hz floor",
          Test::Near(lrMusic.mfLowPassFrequency, 300.0f) &&
          Test::Near(lrVoice.mafParameters[5], 300.0f));
    CHECK("either pause byte drives original PauseControl slot 1",
          Test::Near(lrVoice.mafParameters[1], 1.0f));

    BrnSound::Vehicles::Engines::PhysicsControl lrPhysics;
    BrnSound::Vehicles::Environment::SpeedStreamControl lrSpeed;
    lrSpeed.mpPhysicsControl = &lrPhysics;
    SetSpeed(lrPhysics, 65.0f); // below the 85 mph lower edge
    lrSpeed.UpdateDuckers();
    CheckMixer("speed mixer clamps below the crossfade window", lrSpeed, 0, 32767);

    SetSpeed(lrPhysics, 85.0f); // lower edge: high=0, low=1
    lrSpeed.UpdateDuckers();
    CheckMixer("speed mixer includes the lower edge", lrSpeed, 0, 32767);

    SetSpeed(lrPhysics, 105.0f); // midpoint of [85,125]
    lrSpeed.UpdateDuckers();
    CheckMixer("speed mixer midpoint truncates both half gains", lrSpeed, 16383, 16383);

    SetSpeed(lrPhysics, 125.0f); // upper edge: high=1, low=0
    lrSpeed.UpdateDuckers();
    CheckMixer("speed mixer includes the upper edge", lrSpeed, 32767, 0);

    SetSpeed(lrPhysics, 145.0f); // above the crossfade window
    lrSpeed.UpdateDuckers();
    CheckMixer("speed mixer clamps above the crossfade window", lrSpeed, 32767, 0);

    KF_SPEED_STREAM_THRESHOLD_MPH = 120.0f;
    KF_SPEED_STREAM_CROSSFADE_WIDTH = 20.0f;
    SetSpeed(lrPhysics, 110.0f); // new lower edge, not the default 85 mph edge
    lrSpeed.UpdateDuckers();
    CheckMixer("runtime threshold and width move both crossfade edges", lrSpeed, 0, 32767);
    SetSpeed(lrPhysics, 120.0f);
    lrSpeed.UpdateDuckers();
    CheckMixer("runtime threshold and width move the midpoint", lrSpeed, 16383, 16383);

    KF_SPEED_STREAM_CROSSFADE_WIDTH = 0.0f;
    SetSpeed(lrPhysics, 120.0f); // 0/0 in the high ramp; original fsel path selects 1
    lrSpeed.UpdateDuckers();
    CheckMixer("zero-width unordered high ramp follows ARTIST fsel clamps", lrSpeed, 32767, 0);
    SetSpeed(lrPhysics, 119.0f); // opposite infinities clamp to high=0, low=1
    lrSpeed.UpdateDuckers();
    CheckMixer("zero-width opposite-side infinities clamp to the lower side", lrSpeed, 0, 32767);

    SetSpeed(lrPhysics, std::numeric_limits<f32>::quiet_NaN());
    lrSpeed.UpdateDuckers();
    // ARTIST's high fsel chain maps the unordered ramp to 1. The separate fcmpu/bge
    // before the low ramp treats unordered as not-less-than and leaves that channel at 0.
    CheckMixer("unordered speed keeps ARTIST's high fsel result and skips the low ramp",
               lrSpeed, 32767, 0);

    KF_SPEED_STREAM_CROSSFADE_WIDTH = 20.0f;
    SetSpeed(lrPhysics, 120.0f);
    lrPhysics.mData.IsCrashing.Update(true);
    lrSpeed.mBoostStreamOn.Update(true);
    lrSpeed.UpdateDuckers();
    CheckMixer("crash and boost channels preserve their binary mixer values", lrSpeed,
               0, 32767, 32767, 32767);

    // ARTIST UpdateParams uses threshold - 0.75*width, so these live settings make the
    // onset exactly 105 mph. Exercise equality and the strict 0.5 s timer boundary.
    lrSpeed.mfSpeedHighTime = 1.0f;
    SetSpeed(lrPhysics, 105.0f);
    lrSpeed.UpdateParams(0.6f);
    CHECK("speed timer resets at the live threshold-minus-three-quarter-width onset",
          Test::Near(lrSpeed.mfSpeedHighTime, 0.0f) &&
          !lrSpeed.mSpeedStreamOn.GetCurrent());

    SetSpeed(lrPhysics, 105.25f);
    lrSpeed.UpdateParams(0.5f);
    CHECK("speed stream remains off at exactly 0.5 seconds",
          Test::Near(lrSpeed.mfSpeedHighTime, 0.5f) &&
          !lrSpeed.mSpeedStreamOn.GetCurrent());
    lrSpeed.UpdateParams(0.001f);
    CHECK("speed stream turns on only after the strict 0.5 second boundary",
          Test::Near(lrSpeed.mfSpeedHighTime, 0.501f) &&
          lrSpeed.mSpeedStreamOn.GetCurrent());

    std::printf("SoundStreamDebugMenu: %d checks, %d failures\n",
                Test::giChecks, Test::giFailures);
    return Test::giFailures ? 1 : 0;
}
