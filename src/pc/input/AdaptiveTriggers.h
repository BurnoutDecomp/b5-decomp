#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// FLAG PC-platform leaf: optional controller presentation. These are copied host
// observations and tuning, never members of a recovered vehicle or input record.
namespace CgsInput::AdaptiveTriggers
{
    enum class Preset { Off, Light, Standard, Immersive };
    enum class Character { Speed, Stunt, Power, Heavy };
    enum class Cue
    {
        Off, Pedal, Engine, Launch, Shift, Limiter, Wheelspin, GripRecovery,
        BoostStart, Boost, BoostEnd, Chain, Brake, BrakeSkid, DriftStart, Drift,
        Surface, Takeoff, Airborne, Landing, Scrape, Impact, Wreck, Reverse,
        Showtime, Bounce, BoostedBounce, Reward, Count
    };
    enum Event : std::uint32_t
    {
        E_EVENT_NONE = 0, E_EVENT_IMPACT = 1, E_EVENT_TRAFFIC_CHECK = 2,
        E_EVENT_BOUNCE = 4, E_EVENT_BOOSTED_BOUNCE = 8, E_EVENT_NEAR_MISS = 16,
        E_EVENT_STUNT = 32, E_EVENT_CHAIN = 64, E_EVENT_BOOST_AWARD = 128
    };
    enum class Mode : std::uint8_t { Off = 0x05, Feedback = 0x21, Vibration = 0x26 };

    inline float Finite(float lfValue, float lfDefault = 0.0f)
    {
        return std::isfinite(lfValue) ? lfValue : lfDefault;
    }
    inline float Unit(float lfValue) { return std::clamp(Finite(lfValue), 0.0f, 1.0f); }

    struct CueTuning
    {
        float mfStrength = 1.0f, mfFrequency = 1.0f, mfDuration = 1.0f;
        int miStartPosition = -1; // -1 preserves the effect's normal engagement zone.
    };

    struct Settings
    {
        Preset mePreset = Preset::Immersive;
        float mfResistance = 1.0f;
        float mfTexture = 1.0f;
        float mfLeftStrength = 1.0f, mfRightStrength = 1.0f;
        std::array<CueTuning, static_cast<unsigned>(Cue::Count)> maCues{};
        bool mbEnabled = true;
        bool mbArcadeCues = true;
        bool mbDiagnostics = false;
        bool mbSystemFeedbackEnabled = true;

        float GetScale() const
        {
            if (!mbEnabled || !mbSystemFeedbackEnabled) return 0.0f;
            switch (mePreset)
            {
                case Preset::Light: return 0.45f;
                case Preset::Standard: return 0.7f;
                case Preset::Immersive: return 1.0f;
                default: return 0.0f;
            }
        }
    };

    struct Sample
    {
        std::uint64_t muVehicle = 0;
        std::uint32_t muEvents = 0;
        std::uint32_t muChain = 0;
        std::uint64_t muBoostAward = 0;
        Character meCharacter = Character::Speed;
        int miGear = 0;
        unsigned muGroundedWheels = 0;
        float mfGas = 0, mfBrake = 0, mfHandBrake = 0;
        float mfSpeedMPH = 0, mfRPM = 0, mfUpShiftRPM = 0;
        float mfDrivenSlip = 0, mfBrakeSkid = 0, mfDrift = 0;
        float mfAirTime = 0, mfVerticalSpeed = 0;
        float mfSurface = 0, mfImpact = 0, mfScrape = 0;
        bool mbActive = false, mbReset = false, mbReverse = false;
        bool mbDriving = true;
        bool mbBoosting = false, mbDrifting = false, mbCrashing = false;
        bool mbShowtime = false, mbLimiter = false;
    };

    struct Effect
    {
        Mode meMode = Mode::Off;
        Cue meCue = Cue::Off;
        std::array<std::uint8_t, 10> mauStrength{};
        std::uint8_t muFrequency = 0;
        unsigned muPriority = 0;

        bool IsOff() const { return meMode == Mode::Off; }
    };
    struct Output { Effect mLeft, mRight; };

    inline const char* GetCueName(Cue leCue)
    {
        static constexpr const char* KAC_NAMES[] = {
            "off", "pedal", "engine", "launch", "shift", "limiter", "wheelspin", "grip-recovery",
            "boost-start", "boost", "boost-end", "chain", "brake", "brake-skid", "drift-start", "drift",
            "surface", "takeoff", "airborne", "landing", "scrape", "impact", "wreck", "reverse",
            "showtime", "bounce", "boosted-bounce", "reward"
        };
        const unsigned luCue = static_cast<unsigned>(leCue);
        return luCue < static_cast<unsigned>(Cue::Count) ? KAC_NAMES[luCue] : "invalid";
    }

    inline const char* GetCueKey(Cue leCue)
    {
        static constexpr const char* KAC_KEYS[] = {
            "Off", "Pedal", "Engine", "Launch", "Shift", "Limiter", "Wheelspin", "GripRecovery",
            "BoostStart", "Boost", "BoostEnd", "Chain", "Brake", "BrakeSkid", "DriftStart", "Drift",
            "Surface", "Takeoff", "Airborne", "Landing", "Scrape", "Impact", "Wreck", "Reverse",
            "Showtime", "Bounce", "BoostedBounce", "Reward"
        };
        const unsigned luCue = static_cast<unsigned>(leCue);
        return luCue < static_cast<unsigned>(Cue::Count) ? KAC_KEYS[luCue] : "Invalid";
    }

    class Controller
    {
    public:
        static constexpr std::uint64_t KU_STALE_MS = 500;

        void Configure(const Settings& lrSettings)
        {
            mSettings = lrSettings;
            mSettings.mfResistance = Unit(mSettings.mfResistance);
            mSettings.mfTexture = Unit(mSettings.mfTexture);
            mSettings.mfLeftStrength = Unit(mSettings.mfLeftStrength);
            mSettings.mfRightStrength = Unit(mSettings.mfRightStrength);
            for (auto& lrCue : mSettings.maCues)
            {
                lrCue.mfStrength = std::clamp(Finite(lrCue.mfStrength, 1.0f), 0.0f, 2.0f);
                lrCue.mfFrequency = std::clamp(Finite(lrCue.mfFrequency, 1.0f), 0.25f, 2.0f);
                lrCue.mfDuration = std::clamp(Finite(lrCue.mfDuration, 1.0f), 0.0f, 2.0f);
                lrCue.miStartPosition = std::clamp(lrCue.miStartPosition, -1, 9);
            }
            Reset();
        }

        void Reset()
        {
            mbValid = false;
            mbSpinning = false;
            mfSlip = 0;
            muLastReward = 0;
            maPulses = {};
        }

        void Submit(Sample lSample, std::uint64_t luNow)
        {
            lSample.mfGas = Unit(lSample.mfGas);
            lSample.mfBrake = Unit(lSample.mfBrake);
            lSample.mfHandBrake = Unit(lSample.mfHandBrake);
            lSample.mfRPM = std::clamp(Finite(lSample.mfRPM), 0.0f, 30000.0f);
            lSample.mfSpeedMPH = std::clamp(Finite(lSample.mfSpeedMPH), -1000.0f, 1000.0f);
            lSample.mfDrivenSlip = Unit(lSample.mfDrivenSlip);
            lSample.mfBrakeSkid = Unit(lSample.mfBrakeSkid);
            lSample.mfDrift = Unit(lSample.mfDrift);
            lSample.mfSurface = Unit(lSample.mfSurface);
            lSample.mfImpact = Unit(lSample.mfImpact);
            lSample.mfScrape = Unit(lSample.mfScrape);
            lSample.mfAirTime = (std::max)(0.0f, Finite(lSample.mfAirTime));
            lSample.mfVerticalSpeed = std::clamp(Finite(lSample.mfVerticalSpeed), -1000.0f, 1000.0f);
            lSample.muGroundedWheels = (std::min)(lSample.muGroundedWheels, 4u);
            if (!lSample.mbActive || lSample.mbReset)
            {
                Reset();
                return;
            }
            if (mbValid && (luNow < muLastSample || luNow - muLastSample > KU_STALE_MS ||
                lSample.muVehicle != mSample.muVehicle || lSample.mbShowtime != mSample.mbShowtime))
                Reset();

            if (mbValid)
            {
                const float lfStep = static_cast<float>(luNow - muLastSample) * 0.001f;
                mfSlip += (lSample.mfDrivenSlip - mfSlip) * (1.0f - std::exp(-lfStep * 15.0f));
                const bool lbSpinning = lSample.muGroundedWheels && lSample.mfGas > 0.1f &&
                    mfSlip > (mbSpinning ? 0.12f : 0.25f);
                if (mbSpinning && !lbSpinning && lSample.muGroundedWheels && lSample.mbDriving && mSample.mbDriving)
                    Start(Cue::GripRecovery, luNow, 110, 40, 0.25f, lSample.mbReverse ? 1 : 2);
                mbSpinning = lbSpinning;
                if (lSample.mbDriving != mSample.mbDriving)
                    for (auto& lrPulse : maPulses) if (lrPulse.muPriority <= 65) lrPulse = {};
                if (!lSample.mbShowtime && lSample.mbCrashing && !mSample.mbCrashing)
                    Start(Cue::Wreck, luNow, 300, 100, 0.85f, 3);
                if (!lSample.mbCrashing || lSample.mbShowtime)
                {
                    if (lSample.mbDriving && mSample.mbDriving)
                    {
                        if (lSample.miGear != mSample.miGear && lSample.miGear > 0 &&
                            mSample.miGear > 0 && lSample.mfGas > 0.1f)
                            Start(Cue::Shift, luNow, 130, 35, 0.35f, lSample.mbReverse ? 1 : 2);
                        if (lSample.mfGas > 0.65f && mSample.mfGas < 0.3f &&
                            std::fabs(lSample.mfSpeedMPH) < 15.0f && lSample.muGroundedWheels)
                            Start(Cue::Launch, luNow, 220, 40, 0.45f, lSample.mbReverse ? 1 : 2);
                        if (lSample.mbBoosting != mSample.mbBoosting)
                            Start(lSample.mbBoosting ? Cue::BoostStart : Cue::BoostEnd,
                                luNow, lSample.mbBoosting ? 180 : 120, 45,
                                lSample.mbBoosting ? 0.55f : 0.25f, lSample.mbReverse ? 1 : 2);
                        if (lSample.mbDrifting && !mSample.mbDrifting)
                            Start(Cue::DriftStart, luNow, 140, 55, 0.4f, 3);
                    }
                    if (!lSample.muGroundedWheels && mSample.muGroundedWheels)
                        Start(Cue::Takeoff, luNow, 90, 65, 0.0f, 3);
                    if (lSample.muGroundedWheels && !mSample.muGroundedWheels && mSample.mfAirTime > 0.08f)
                        Start(Cue::Landing, luNow, 230, 85,
                            std::clamp(std::fabs(mSample.mfVerticalSpeed) / 14.0f, 0.2f, 1.0f), 3);
                    if (lSample.mfImpact > 0.12f &&
                        (mSample.mfImpact <= 0.12f || lSample.mfImpact > mSample.mfImpact + 0.15f))
                        Start(Cue::Impact, luNow, 180, 90, lSample.mfImpact, 3);
                    if (lSample.muEvents & E_EVENT_IMPACT)
                        Start(Cue::Impact, luNow, 180, 90, (std::max)(lSample.mfImpact, 0.45f), 3);
                    if (lSample.muEvents & E_EVENT_TRAFFIC_CHECK)
                        Start(Cue::Impact, luNow, 150, 90, (std::max)(lSample.mfImpact, 0.35f), 3);
                    if (lSample.mbShowtime && (lSample.muEvents & E_EVENT_BOUNCE))
                        Start((lSample.muEvents & E_EVENT_BOOSTED_BOUNCE) ? Cue::BoostedBounce : Cue::Bounce,
                            luNow, 220, 95, (lSample.muEvents & E_EVENT_BOOSTED_BOUNCE) ? 0.85f : 0.55f, 3);
                    if (((lSample.muEvents & E_EVENT_CHAIN) &&
                        (!(mSample.muEvents & E_EVENT_CHAIN) || lSample.muChain != mSample.muChain)) ||
                        (lSample.mbBoosting && lSample.muChain > mSample.muChain))
                        Start(Cue::Chain, luNow, 250, 48, 0.5f, lSample.mbReverse ? 1 : 2);
                    if (mSettings.mbArcadeCues && (luNow - muLastReward >= 300) &&
                        ((lSample.muEvents & (E_EVENT_NEAR_MISS | E_EVENT_STUNT | E_EVENT_BOOST_AWARD)) ||
                        (lSample.muBoostAward != 0 && lSample.muBoostAward != mSample.muBoostAward)))
                    {
                        Start(Cue::Reward, luNow, 75, 29, 0.18f, lSample.mbReverse ? 1 : 2);
                        muLastReward = luNow;
                    }
                }
            }
            else
            {
                // First frame after a lifecycle reset establishes state; it must
                // not replay an old collision, shift, reward or boost-start.
                mfSlip = lSample.mfDrivenSlip;
                mbSpinning = lSample.muGroundedWheels && lSample.mfGas > 0.1f && mfSlip > 0.25f;
            }
            mSample = lSample;
            muLastSample = luNow;
            mbValid = true;
        }

        Output Resolve(std::uint64_t luNow) const
        {
            Output lOutput;
            if (!mbValid || !mSettings.GetScale() || luNow < muLastSample ||
                luNow - muLastSample > KU_STALE_MS)
                return lOutput;

            const bool lbTexture = mSettings.mfTexture > 0;
            const unsigned luDrive = mSample.mbReverse ? 1 : 2;
            const float lfPower = mSample.meCharacter == Character::Power ? 1.15f :
                mSample.meCharacter == Character::Heavy ? 1.25f : 1.0f;
            const float lfFrequency = mSample.meCharacter == Character::Speed ? 1.15f :
                mSample.meCharacter == Character::Heavy ? 0.65f :
                mSample.meCharacter == Character::Power ? 0.8f : 1.0f;
            if (mSample.mbDriving && (!mSample.mbCrashing || mSample.mbShowtime))
            {
                Effect lDrive = Make(Cue::Pedal, 20, 0.12f * lfPower, 0, 2, true);
                if (mSample.mbShowtime)
                    lDrive = Make(Cue::Showtime, 20, 0.08f, 0, 2);
                else if (!mSample.muGroundedWheels)
                    lDrive = Make(Cue::Airborne, 30, 0.06f, 12 + mSample.mfRPM * 0.004f, 1);
                else if (mSample.mfGas > 0.04f)
                    Apply(lDrive, Make(mSample.mbReverse ? Cue::Reverse : Cue::Engine, 25,
                        (0.1f + 0.06f * mSample.mfGas) * lfPower,
                        (12 + mSample.mfRPM * 0.006f) * lfFrequency, 1, true));
                if (lbTexture && mSample.muGroundedWheels && mSample.mbBoosting)
                    Apply(lDrive, Make(Cue::Boost, 45, 0.3f * lfPower, 72 * lfFrequency, 1));
                if (lbTexture && mSample.muGroundedWheels && mSample.mbDrifting)
                    Apply(lDrive, Make(Cue::Drift, 55, 0.16f + 0.25f * mSample.mfDrift,
                        23 + 12 * mSample.mfDrift + Irregular(luNow, 0), 1));
                if (lbTexture && mbSpinning && mSample.muGroundedWheels)
                    Apply(lDrive, Make(Cue::Wheelspin, 60, 0.2f + 0.35f * mfSlip,
                        32 + 22 * mfSlip + Irregular(luNow, 17), 1));
                if (lbTexture && mSample.mbLimiter && mSample.mfGas > 0.1f)
                    Apply(lDrive, Make(Cue::Limiter, 62, 0.35f, 18, 1));
                if (luDrive == 1) lOutput.mLeft = lDrive; else lOutput.mRight = lDrive;

                if (mSample.muGroundedWheels && !mSample.mbShowtime)
                {
                    Effect lBrake = Make(Cue::Brake, 30,
                        (0.16f + 0.4f * mSample.mfBrake) * lfPower, 0, 2, true);
                    if (lbTexture && (mSample.mfBrake > 0.1f || mSample.mfHandBrake > 0.1f) && mSample.mfBrakeSkid > 0.18f)
                        Apply(lBrake, Make(Cue::BrakeSkid, 65, 0.25f + 0.4f * mSample.mfBrakeSkid,
                            28 + Irregular(luNow, 91), 1));
                    if (luDrive == 1) lOutput.mRight = lBrake; else lOutput.mLeft = lBrake;
                }
                if (lbTexture && mSample.muGroundedWheels)
                {
                    const float lfSpeed = std::fabs(mSample.mfSpeedMPH);
                    if (mSample.mfSurface > 0.06f && lfSpeed > 3.0f)
                        ApplyBoth(lOutput, Make(Cue::Surface, 28, mSample.mfSurface * 0.35f,
                            12 + (std::min)(lfSpeed, 200.0f) * 0.2f + Irregular(luNow, 43), 1));
                    if (mSample.mfScrape > 0.05f && lfSpeed > 2.0f)
                        ApplyBoth(lOutput, Make(Cue::Scrape, 70, 0.2f + 0.4f * mSample.mfScrape,
                            45 + Irregular(luNow, 73), 1));
                }
            }

            for (const auto& lrPulse : maPulses)
            {
                if ((!lbTexture && lrPulse.meCue != Cue::Takeoff) || (!mSample.mbDriving && lrPulse.muPriority <= 65) || !lrPulse.muDuration || luNow < lrPulse.muStart || luNow - lrPulse.muStart >= lrPulse.muDuration)
                    continue;
                const float lfPhase = static_cast<float>(luNow - lrPulse.muStart) / lrPulse.muDuration;
                Effect lEffect;
                if (lrPulse.meCue == Cue::Takeoff || (lrPulse.meCue == Cue::Shift && lfPhase < 0.25f) ||
                    (lrPulse.meCue == Cue::Chain && lfPhase > 0.32f && lfPhase < 0.58f))
                {
                    lEffect.meCue = lrPulse.meCue;
                    lEffect.muPriority = lrPulse.muPriority;
                }
                else
                {
                    const float lfEnvelope = lrPulse.meCue == Cue::Chain ? 1.0f :
                        (std::min)(1.0f, (1.0f - lfPhase) * 2.0f);
                    lEffect = Make(lrPulse.meCue, lrPulse.muPriority, lrPulse.mfStrength * lfEnvelope,
                        lrPulse.meCue == Cue::Wreck ? 20.0f : lrPulse.meCue == Cue::Landing ? 28.0f :
                        lrPulse.meCue == Cue::BoostedBounce ? 44.0f : 38.0f, 1);
                }
                if (lrPulse.muMask & 1) Apply(lOutput.mLeft, lEffect);
                if (lrPulse.muMask & 2) Apply(lOutput.mRight, lEffect);
            }
            ScaleTrigger(lOutput.mLeft, mSettings.mfLeftStrength);
            ScaleTrigger(lOutput.mRight, mSettings.mfRightStrength);
            return lOutput;
        }

    private:
        struct Pulse
        {
            Cue meCue = Cue::Off;
            std::uint64_t muStart = 0;
            unsigned muDuration = 0, muPriority = 0, muMask = 0;
            float mfStrength = 0;
        };
        Settings mSettings;
        Sample mSample;
        std::array<Pulse, static_cast<unsigned>(Cue::Count)> maPulses{};
        std::uint64_t muLastSample = 0;
        std::uint64_t muLastReward = 0;
        float mfSlip = 0;
        bool mbValid = false, mbSpinning = false;

        void Start(Cue leCue, std::uint64_t luNow, unsigned luDuration, unsigned luPriority,
            float lfStrength, unsigned luMask)
        {
            const auto& lrTuning = mSettings.maCues[static_cast<unsigned>(leCue)];
            if (lrTuning.mfStrength <= 0.0f || lrTuning.mfDuration <= 0.0f) return;
            luDuration = static_cast<unsigned>(std::round(luDuration * lrTuning.mfDuration));
            maPulses[static_cast<unsigned>(leCue)] = {leCue, luNow, luDuration, luPriority, luMask, Unit(lfStrength)};
        }
        static float Irregular(std::uint64_t luNow, unsigned luSeed)
        {
            // Deterministic texture, independent of frame count and global RNG.
            const std::uint32_t luTick = static_cast<std::uint32_t>(luNow / 47) + luSeed;
            return static_cast<float>((luTick * 1664525u + 1013904223u) % 11u);
        }
        static void Apply(Effect& lrCurrent, const Effect& lrNext)
        {
            if (lrNext.muPriority >= lrCurrent.muPriority) lrCurrent = lrNext;
        }
        static void ApplyBoth(Output& lrOutput, const Effect& lrEffect)
        {
            Apply(lrOutput.mLeft, lrEffect);
            Apply(lrOutput.mRight, lrEffect);
        }
        static void ScaleTrigger(Effect& lrEffect, float lfScale)
        {
            if (lfScale <= 0.0f) { lrEffect = {}; return; }
            for (auto& luStrength : lrEffect.mauStrength)
                if (luStrength) luStrength = static_cast<std::uint8_t>((std::max)(1.0f, std::round(luStrength * lfScale)));
        }
        Effect Make(Cue leCue, unsigned luPriority, float lfStrength, float lfFrequency,
            unsigned luStart, bool lbProgressive = false) const
        {
            const auto& lrTuning = mSettings.maCues[static_cast<unsigned>(leCue)];
            if (lrTuning.mfStrength <= 0.0f) return {};
            lfStrength *= lrTuning.mfStrength;
            lfFrequency *= lrTuning.mfFrequency;
            if (lrTuning.miStartPosition >= 0) luStart = static_cast<unsigned>(lrTuning.miStartPosition);
            if (lfFrequency > 0 && mSettings.mfTexture <= 0 && leCue != Cue::Engine &&
                leCue != Cue::Reverse && leCue != Cue::Airborne) return {};
            Effect lEffect;
            lEffect.meCue = leCue;
            lEffect.muPriority = luPriority;
            const bool lbTexture = lfFrequency > 0 && mSettings.mfTexture > 0;
            const float lfScale = mSettings.GetScale() *
                (lbTexture ? mSettings.mfTexture : mSettings.mfResistance);
            const float lfForce = Unit(lfStrength * lfScale);
            if (lfForce <= 0.0f) return lEffect;
            lEffect.meMode = lbTexture ? Mode::Vibration : Mode::Feedback;
            lEffect.muFrequency = lbTexture ? static_cast<std::uint8_t>(std::clamp(Finite(lfFrequency), 1.0f, 120.0f)) : 0;
            bool lbAny = false;
            for (unsigned luZone = (std::min)(luStart, 9u); luZone < 10; ++luZone)
            {
                const float lfRamp = lbProgressive ? 0.4f + 0.6f * (luZone - luStart) / (9.0f - (std::min)(luStart, 8u)) : 1.0f;
                const unsigned luStrength = (std::max)(1u, static_cast<unsigned>(std::round(lfForce * lfRamp * 8.0f)));
                lEffect.mauStrength[luZone] = static_cast<std::uint8_t>((std::min)(luStrength, 8u));
                lbAny |= luStrength != 0;
            }
            if (!lbAny) lEffect.meMode = Mode::Off;
            return lEffect;
        }
    };
}
