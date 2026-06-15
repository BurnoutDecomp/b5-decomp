#pragma once

// Input-module IO event payloads (the subset the boot-path event queues embed).
// Reconstructed from the DecFIGS DWARF. The input events derive from an empty
// per-module Event base (the CgsModule event-queue convention — see e.g.
// BrnContactSpyEvents.h).
#include "types.hpp"
#include "GameShared/GameClasses/System/Input/CgsInputTypes.h"   // CgsInput::EBindResult, EUnbindResult

namespace CgsInput
{
namespace InputIO
{
    struct Event {};   // empty base of the input event hierarchy

    struct BaseInputEvent : public Event
    {
        s32 miPlayer;
        s32 miPort;
    };

    struct BaseRumbleEvent : public BaseInputEvent
    {
        s32 miRumblePriority;
    };

    struct JoltEnvelope
    {
        f32 mfAttackTime;
        f32 mfDecayTime;
        f32 mfSustainTime;
        f32 mfReleaseTime;
        f32 mfPeakSpeedValue;
        f32 mfSustainSpeedValue;
    };

    struct JoltEffect
    {
        JoltEnvelope mLowFreqJoltData;
        JoltEnvelope mHighFreqJoltData;
    };

    struct BindResult : public BaseInputEvent
    {
        CgsInput::EBindResult meResultCode;
    };

    struct UnBindResult : public BaseInputEvent
    {
        CgsInput::EUnbindResult meResultCode;
    };

    struct ChangeVolumeRumbleEffectEvent : public BaseInputEvent
    {
        JoltEffect mJoltEffect;
        s32        miRumbleId;
        f32        mfRumbleVolume;
    };

    struct PlayJoltEffectEvent : public BaseRumbleEvent
    {
        JoltEffect mJoltEffect;
    };

    struct PlayRumbleEffectEvent : public PlayJoltEffectEvent
    {
        s32 miRumbleId;
        f32 mfRumbleVolume;
    };

    struct StopRumbleEffectEvent : public BaseInputEvent
    {
        s32 miRumbleId;
    };
}
}
