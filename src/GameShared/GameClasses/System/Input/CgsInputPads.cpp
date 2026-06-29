// CgsInput::InputPads -- construction, preparation, player/port bind reset, and the jolt/rumble
// force-feedback effect engine. Reconstructed from BURNOUT_X360_ARTIST.XEX (CgsInputPads.cpp,
// methods @0x828E72xx / 0x828EFxxx / 0x828DCxxx). The class layout + member offsets are pinned in
// CgsInputPads.h (offsetof static_asserts). GetDebugGamePad lives in CgsInputPads_GetDebugGamePad.cpp.
//
// Source path: d:\p4\b5_main\burnout\main\code\gameshared\gameclasses\system\input\CgsInputPads.cpp

#include "GameShared/GameClasses/System/Input/CgsInputPads.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstring>   // std::memset (Construct clears the action-down bitmap)

namespace CgsInput
{
    // Provisional home of the debug "force rumble on a disconnected pad" override (X360 byte_83085F80).
    // FLAGGED: a debug runtime toggle; default false (no immediate store seeds it -- it lives in BSS,
    // which is zero-initialised). Promote/relocate when the debug-input override TU lands.
    bool gbForceRumbleOnDisconnectedPad = false;

    // The total wall-clock duration of one ADSR jolt envelope = attack + decay + sustain + release.
    // UpdatePadRumble computes this inline (`(((env[3]+env[2])+env[1])+env[0])`) for both the low- and
    // high-frequency envelopes and ages an effect out once its time exceeds the larger of the two.
    static f32 GetTotalJoltEnvelopeDuration(const InputIO::JoltEnvelope& lEnvelope)
    {
        return ((lEnvelope.mfReleaseTime + lEnvelope.mfSustainTime) + lEnvelope.mfDecayTime) + lEnvelope.mfAttackTime;
    }

    // ------------------------------------------------------------------------------------------------
    // X360 0x828E7238 -- InitializePads.
    // Zero the per-port records ({miField00, mbField14} per record) and the six head words, then
    // construct the four physical pad devices (stride 0x1C4).
    // ------------------------------------------------------------------------------------------------
    void InputPads::InitializePads()
    {
        miHead70  = 0;
        mbHead74  = 0;
        miHead60  = 0;
        miHead64  = 0;
        miHead68  = 0;
        miHead6C  = 0;

        for (u32 luPortIndex = 0; luPortIndex < KU_NUMBER_OF_PADS; ++luPortIndex)
        {
            maPorts[luPortIndex].miField00 = 0;
            maPorts[luPortIndex].mbField14 = 0;
        }

        for (u32 luPadIndex = 0; luPadIndex < KU_NUMBER_OF_PADS; ++luPadIndex)
        {
            maPads[luPadIndex].Construct();
        }
    }

    // ------------------------------------------------------------------------------------------------
    // X360 0x828EFAF0 -- Construct.
    // Seed the per-player bind table (unbound), the per-port resolved-player ids (-1), the action
    // mapping storage (0xFF), the action-down bitmap (0), the override pointers (null), the jolt/rumble
    // time/priority/id tables, and the rumble enable flags, then InitializePads().
    // ------------------------------------------------------------------------------------------------
    void InputPads::Construct()
    {
        mbPrepared = false;   // +0xCFC = 0

        for (u32 luPadIndex = 0; luPadIndex < KU_NUMBER_OF_PADS; ++luPadIndex)
        {
            // per-player bind record + per-port resolved-player id + override ptr
            maPlayers[luPadIndex].mbBound = false;          // +1928 + 8*i
            maPlayers[luPadIndex].miPort  = -1;             // +1932 + 8*i
            maiPortToPlayer[luPadIndex]   = -1;             // +1960 + 4*i
            mauOverrideDeviceState[luPadIndex] = 0;          // X360 const DeviceState* (4-byte slot), null   // +3328 + 4*i

            // per-pad action mapping table: 0x70 bytes of 0xFF
            for (u32 luByte = 0; luByte < 0x70; ++luByte)
            {
                maaMappingStorage[luPadIndex][luByte] = 0xFF;
            }
        }

        // action-down bitmap: 2 * KU_NUMBER_OF_PADS * 112 bytes cleared (0x380 == 896)
        std::memset(maaabActionsDown, 0, sizeof(maaabActionsDown));
        miCurrentSetOfStates = 0;   // +2424

        // jolt + rumble effect timing tables: times 0.0, priorities/ids -1
        for (u32 luPort = 0; luPort < KU_NUMBER_OF_PADS; ++luPort)
        {
            for (u32 luIdx = 0; luIdx < KU_MAX_NUMBER_OF_JOLT_EFFECTS; ++luIdx)
            {
                mafJoltTime[luPort][luIdx]             = 0.0f;
                maiJoltEffectPriorities[luPort][luIdx] = -1;
            }
            for (u32 luIdx = 0; luIdx < KU_MAX_NUMBER_OF_RUMBLE_EFFECTS; ++luIdx)
            {
                mafRumbleTime[luPort][luIdx]              = 0.0f;
                maiRumbleIds[luPort][luIdx]               = -1;
                maiRumbleEffectPriorities[luPort][luIdx]  = -1;
            }
        }

        mbRumblePaused   = false;   // +0x10D0 = 0
        mbRumbleEnabled  = true;    // +0x10D1 = 1
        mbWheelFFEnabled = true;    // +0x10D2 = 1

        InitializePads();
    }

    // ------------------------------------------------------------------------------------------------
    // X360 0x828E72C0 -- Prepare.
    // Mark prepared, re-prime the jolt/rumble timing tables, then enable rumble + un-pause. Returns
    // mbPrepared. (The PS3 resource allocator is unused on X360.)
    // ------------------------------------------------------------------------------------------------
    bool InputPads::Prepare(rw::IResourceAllocator* lpInputGeneralAllocator)
    {
        (void)lpInputGeneralAllocator;

        if (!mbPrepared)
        {
            mbPrepared = true;
        }

        for (u32 luPort = 0; luPort < KU_NUMBER_OF_PADS; ++luPort)
        {
            for (u32 luIdx = 0; luIdx < KU_MAX_NUMBER_OF_JOLT_EFFECTS; ++luIdx)
            {
                mafJoltTime[luPort][luIdx]             = 0.0f;
                maiJoltEffectPriorities[luPort][luIdx] = -1;
            }
            for (u32 luIdx = 0; luIdx < KU_MAX_NUMBER_OF_RUMBLE_EFFECTS; ++luIdx)
            {
                mafRumbleTime[luPort][luIdx]             = 0.0f;
                maiRumbleIds[luPort][luIdx]              = -1;
                maiRumbleEffectPriorities[luPort][luIdx] = -1;
            }
        }

        mbRumbleEnabled = true;    // +0x10D1 = 1
        mbRumblePaused  = false;   // +0x10D0 = 0
        return mbPrepared;
    }

    // ------------------------------------------------------------------------------------------------
    // X360 0x828EFBF0 -- Destruct.
    // Reset all per-player bind records to unbound and all per-port resolved-player ids + the two head
    // words to zero/-1.
    // ------------------------------------------------------------------------------------------------
    void InputPads::Destruct()
    {
        for (u32 luPadIndex = 0; luPadIndex < KU_NUMBER_OF_PADS; ++luPadIndex)
        {
            maPlayers[luPadIndex].mbBound = false;
            maPlayers[luPadIndex].miPort  = -1;
            maiPortToPlayer[luPadIndex]   = -1;
        }

        for (u32 luPortIndex = 0; luPortIndex < KU_NUMBER_OF_PADS; ++luPortIndex)
        {
            maPorts[luPortIndex].miField00 = 0;
            maPorts[luPortIndex].mbField14 = 0;
        }

        miHead70 = 0;
        mbHead74 = 0;
    }

    // ------------------------------------------------------------------------------------------------
    // X360 0x828E7350 -- FillRawData.
    // For each control (28): take the per-control float and keep the running max into the per-action
    // raw-button array. For each axis (6): add the pad's axis value to the running axis accumulator and
    // clamp the result to [-1, 1].
    // ------------------------------------------------------------------------------------------------
    void InputPads::FillRawData(f32* lafNewRawData, f32* lafAxisData, s32 liPortIndex)
    {
        for (s32 liControlIndex = 0; liControlIndex < static_cast<s32>(CgsInput::KU_NUMBER_OF_CONTROLS); ++liControlIndex)
        {
            const f32 lfControlValue = maPads[liPortIndex].GetControlValue(static_cast<u32>(liControlIndex));
            if (lfControlValue > lafNewRawData[liControlIndex])
            {
                lafNewRawData[liControlIndex] = lfControlValue;
            }
        }

        for (s32 liAxisIndex = 0; liAxisIndex < static_cast<s32>(CgsInput::KU_NUMBER_OF_AXES); ++liAxisIndex)
        {
            const f32 lfAxisValue = maPads[liPortIndex].GetAxisValue(static_cast<u32>(liAxisIndex));
            f32 lfAccumulated = lafAxisData[liAxisIndex] + lfAxisValue;
            if (lfAccumulated < -1.0f)
            {
                lfAccumulated = -1.0f;
            }
            if (lfAccumulated > 1.0f)
            {
                lfAccumulated = 1.0f;
            }
            lafAxisData[liAxisIndex] = lfAccumulated;
        }
    }

    // ------------------------------------------------------------------------------------------------
    // X360 0x828E7618 -- UpdateJoltEnvelope.
    // ADSR-style envelope evaluated at lfTime:
    //   t < attack                          -> peak floor level (mfPeakSpeedValue)
    //   t < attack+decay                    -> lerp peak->sustain over the decay window
    //   t < attack+decay+sustain            -> sustain level (mfSustainSpeedValue)
    //   t >= attack+decay+sustain+release   -> 0
    //   otherwise (release)                 -> lerp sustain->0 over the release window
    // ------------------------------------------------------------------------------------------------
    f32 InputPads::UpdateJoltEnvelope(const InputIO::JoltEnvelope& lEnvelope, f32 lfTime)
    {
        CGS_ASSERT(lfTime >= 0.0f, "jolt envelope time must be non-negative");

        const f32 lfAttack  = lEnvelope.mfAttackTime;
        const f32 lfDecay   = lEnvelope.mfDecayTime;
        const f32 lfSustain = lEnvelope.mfSustainTime;
        const f32 lfRelease = lEnvelope.mfReleaseTime;
        const f32 lfPeak    = lEnvelope.mfPeakSpeedValue;
        const f32 lfLevel   = lEnvelope.mfSustainSpeedValue;

        if (lfTime < lfAttack)
        {
            return lfPeak;
        }
        if (lfTime < lfDecay + lfAttack)
        {
            return ((lfTime - lfAttack) / lfDecay) * (lfLevel - lfPeak) + lfPeak;
        }
        if (lfTime < (lfSustain + lfDecay) + lfAttack)
        {
            return lfLevel;
        }
        if (lfTime >= ((lfRelease + lfSustain) + lfDecay) + lfAttack)
        {
            return 0.0f;
        }
        return ((lfTime - ((lfSustain + lfDecay) + lfAttack)) / lfRelease) * (-lfLevel) + lfLevel;
    }

    // ------------------------------------------------------------------------------------------------
    // X360 0x828DC128 -- PlayJoltEvent.
    // Resolve the requesting player to a physical port, find a jolt slot whose priority is lower than
    // the request (or an empty one), and store the effect there.
    // ------------------------------------------------------------------------------------------------
    void InputPads::PlayJoltEvent(const InputIO::PlayJoltEffectEvent& lJoltEvent)
    {
        s32 liPort   = lJoltEvent.miPort;
        const s32 liPlayer = lJoltEvent.miPlayer;

        if (liPort == -1)
        {
            if (!maPlayers[liPlayer].mbBound)
            {
                return;
            }
            liPort = maPlayers[liPlayer].miPort;
        }

        u32 luJoltIndex = 0;
        while (lJoltEvent.miRumblePriority <= maiJoltEffectPriorities[liPort][luJoltIndex])
        {
            ++luJoltIndex;
            if (luJoltIndex >= KU_MAX_NUMBER_OF_JOLT_EFFECTS)
            {
                return;
            }
        }

        maJoltEffects[liPort][luJoltIndex]           = lJoltEvent.mJoltEffect;
        mafJoltTime[liPort][luJoltIndex]             = 0.0f;
        maiJoltEffectPriorities[liPort][luJoltIndex] = lJoltEvent.miRumblePriority;
    }

    // ------------------------------------------------------------------------------------------------
    // X360 0x828DC208 -- PlayRumbleEvent.
    // As PlayJoltEvent but for the rumble table, also storing the rumble id + volume.
    // ------------------------------------------------------------------------------------------------
    void InputPads::PlayRumbleEvent(const InputIO::PlayRumbleEffectEvent& lRumbleEvent)
    {
        s32 liPort   = lRumbleEvent.miPort;
        const s32 liPlayer = lRumbleEvent.miPlayer;

        if (liPort == -1)
        {
            if (!maPlayers[liPlayer].mbBound)
            {
                return;
            }
            liPort = maPlayers[liPlayer].miPort;
        }

        u32 luRumbleIndex = 0;
        while (lRumbleEvent.miRumblePriority <= maiRumbleEffectPriorities[liPort][luRumbleIndex])
        {
            ++luRumbleIndex;
            if (luRumbleIndex >= KU_MAX_NUMBER_OF_RUMBLE_EFFECTS)
            {
                return;
            }
        }

        maRumbleEffects[liPort][luRumbleIndex]           = lRumbleEvent.mJoltEffect;
        mafRumbleTime[liPort][luRumbleIndex]             = 0.0f;
        maiRumbleEffectPriorities[liPort][luRumbleIndex] = lRumbleEvent.miRumblePriority;
        maiRumbleIds[liPort][luRumbleIndex]              = lRumbleEvent.miRumbleId;
        mafRumbleVolume[liPort][luRumbleIndex]           = lRumbleEvent.mfRumbleVolume;
    }

    // ------------------------------------------------------------------------------------------------
    // X360 0x828DC318 -- ChangeVolumeRumbleEvent.
    // Resolve the player to a port, find the rumble slot matching the event's id, and update its volume.
    // ------------------------------------------------------------------------------------------------
    void InputPads::ChangeVolumeRumbleEvent(const InputIO::ChangeVolumeRumbleEffectEvent& lChangeVolumeRumbleEvent)
    {
        s32 liPort   = lChangeVolumeRumbleEvent.miPort;
        const s32 liPlayer = lChangeVolumeRumbleEvent.miPlayer;

        if (liPort == -1)
        {
            if (!maPlayers[liPlayer].mbBound)
            {
                return;
            }
            liPort = maPlayers[liPlayer].miPort;
        }

        u32 luRumbleIndex = 0;
        while (lChangeVolumeRumbleEvent.miRumbleId != maiRumbleIds[liPort][luRumbleIndex])
        {
            ++luRumbleIndex;
            if (luRumbleIndex >= KU_MAX_NUMBER_OF_RUMBLE_EFFECTS)
            {
                return;
            }
        }

        mafRumbleVolume[liPort][luRumbleIndex] = lChangeVolumeRumbleEvent.mfRumbleVolume;
    }

    // ------------------------------------------------------------------------------------------------
    // X360 0x828DC3C0 -- StopRumbleEvent.
    // Resolve the player to a port, find the rumble slot matching the id, and clear it (time 0, volume
    // 0, priority -1, id -1).
    // ------------------------------------------------------------------------------------------------
    void InputPads::StopRumbleEvent(const InputIO::StopRumbleEffectEvent& lStopRumbleEvent)
    {
        s32 liPort   = lStopRumbleEvent.miPort;
        const s32 liPlayer = lStopRumbleEvent.miPlayer;

        if (liPort == -1)
        {
            if (!maPlayers[liPlayer].mbBound)
            {
                return;
            }
            liPort = maPlayers[liPlayer].miPort;
        }

        u32 luRumbleIndex = 0;
        while (lStopRumbleEvent.miRumbleId != maiRumbleIds[liPort][luRumbleIndex])
        {
            ++luRumbleIndex;
            if (luRumbleIndex >= KU_MAX_NUMBER_OF_RUMBLE_EFFECTS)
            {
                return;
            }
        }

        mafRumbleTime[liPort][luRumbleIndex]             = 0.0f;
        mafRumbleVolume[liPort][luRumbleIndex]           = 0.0f;
        maiRumbleEffectPriorities[liPort][luRumbleIndex] = -1;
        maiRumbleIds[liPort][luRumbleIndex]              = -1;
    }

    // ------------------------------------------------------------------------------------------------
    // X360 0x828EFC58 -- UpdatePadRumble.
    // Tick every active jolt + rumble effect on one pad through its low/high-frequency envelopes,
    // accumulate the strongest left/right motor magnitudes, age out finished effects, then push the
    // resulting motor magnitudes (or, for a wheel device, the wheel FF spring) to the DeviceX360Pad.
    // liPlayer is unused on X360 (the index parameter that drives every table is liPort).
    // ------------------------------------------------------------------------------------------------
    void InputPads::UpdatePadRumble(s32 liPort, s32 liPlayer, f32 lfTimeStep)
    {
        (void)liPlayer;

        bool lbStopMotors    = true;
        f32  lfLeftMotorValue  = 0.0f;
        f32  lfRightMotorValue = 0.0f;

        if (!mbRumblePaused)
        {
            // ---- jolt effects ----
            for (u32 luJoltIndex = 0; luJoltIndex < KU_MAX_NUMBER_OF_JOLT_EFFECTS; ++luJoltIndex)
            {
                if (maiJoltEffectPriorities[liPort][luJoltIndex] > 0)
                {
                    InputIO::JoltEffect& lJoltEffect = maJoltEffects[liPort][luJoltIndex];

                    const f32 lfLeftEnv = UpdateJoltEnvelope(lJoltEffect.mLowFreqJoltData, mafJoltTime[liPort][luJoltIndex]);
                    if (lfLeftMotorValue < lfLeftEnv)
                    {
                        lfLeftMotorValue = lfLeftEnv;
                    }
                    const f32 lfRightEnv = UpdateJoltEnvelope(lJoltEffect.mHighFreqJoltData, mafJoltTime[liPort][luJoltIndex]);
                    if (lfRightMotorValue < lfRightEnv)
                    {
                        lfRightMotorValue = lfRightEnv;
                    }

                    const f32 lfNewTime = mafJoltTime[liPort][luJoltIndex] + lfTimeStep;
                    mafJoltTime[liPort][luJoltIndex] = lfNewTime;

                    f32 lfTotalDuration = GetTotalJoltEnvelopeDuration(lJoltEffect.mLowFreqJoltData);
                    const f32 lfHighDuration = GetTotalJoltEnvelopeDuration(lJoltEffect.mHighFreqJoltData);
                    if (lfTotalDuration <= lfHighDuration)
                    {
                        lfTotalDuration = lfHighDuration;
                    }

                    if (lfNewTime <= lfTotalDuration)
                    {
                        lbStopMotors = false;
                    }
                    else
                    {
                        mafJoltTime[liPort][luJoltIndex]             = 0.0f;
                        maiJoltEffectPriorities[liPort][luJoltIndex] = -1;
                    }
                }
            }

            // ---- rumble effects ----
            for (u32 luRumbleIndex = 0; luRumbleIndex < KU_MAX_NUMBER_OF_RUMBLE_EFFECTS; ++luRumbleIndex)
            {
                if (maiRumbleEffectPriorities[liPort][luRumbleIndex] > 0)
                {
                    InputIO::JoltEffect& lRumbleEffect = maRumbleEffects[liPort][luRumbleIndex];

                    const f32 lfLeftEnv = UpdateJoltEnvelope(lRumbleEffect.mLowFreqJoltData, mafRumbleTime[liPort][luRumbleIndex]);
                    const f32 lfVolume  = mafRumbleVolume[liPort][luRumbleIndex];
                    if (lfLeftMotorValue < lfVolume * lfLeftEnv)
                    {
                        lfLeftMotorValue = lfVolume * lfLeftEnv;
                    }
                    const f32 lfRightEnv = UpdateJoltEnvelope(lRumbleEffect.mHighFreqJoltData, mafRumbleTime[liPort][luRumbleIndex]);
                    if (lfRightMotorValue < lfVolume * lfRightEnv)
                    {
                        lfRightMotorValue = lfVolume * lfRightEnv;
                    }

                    const f32 lfNewTime = lfTimeStep + mafRumbleTime[liPort][luRumbleIndex];
                    mafRumbleTime[liPort][luRumbleIndex] = lfNewTime;

                    f32 lfTotalDuration = GetTotalJoltEnvelopeDuration(lRumbleEffect.mLowFreqJoltData);
                    const f32 lfHighDuration = GetTotalJoltEnvelopeDuration(lRumbleEffect.mHighFreqJoltData);
                    if (lfTotalDuration <= lfHighDuration)
                    {
                        lfTotalDuration = lfHighDuration;
                    }

                    if (lfNewTime > lfTotalDuration)
                    {
                        mafRumbleTime[liPort][luRumbleIndex] = 0.0f;
                    }
                    lbStopMotors = false;
                }
            }
        }

        // ---- decide whether to process this pad ----
        bool lbProcess;
        if (gbForceRumbleOnDisconnectedPad)
        {
            lbProcess = true;
        }
        else
        {
            lbProcess = maPads[liPort].IsConnected();
        }
        if (!lbProcess)
        {
            return;
        }

        DeviceX360Pad& lPad = maPads[liPort];

        if (lPad.GetDeviceType() == E_DEVICETYPE_WHEEL)
        {
            // Publish the wheel force-feedback spring (or a neutral {coeff=1, sat=0} default).
            if (!mbRumbleEnabled || mbRumblePaused || !mbWheelFFEnabled || maiPortToPlayer[liPort] == -1)
            {
                lPad.SetWheelFFSpring(1.0f, 0.0f);
            }
            else
            {
                lPad.SetWheelFFSpring(mWheelFFSpring.mfSpringCoefficient, mWheelFFSpring.mfSpringSaturation);
            }

            if (!lbStopMotors && mbRumbleEnabled && mbWheelFFEnabled)
            {
                f32 lfLeft  = (lfLeftMotorValue  < 0.0f) ? 0.0f : ((lfLeftMotorValue  > 1.0f) ? 1.0f : lfLeftMotorValue);
                f32 lfRight = (lfRightMotorValue < 0.0f) ? 0.0f : ((lfRightMotorValue > 1.0f) ? 1.0f : lfRightMotorValue);
                lPad.SetRumble(lfLeft, lfRight);
            }
            else
            {
                lPad.SetRumble(0.0f, 0.0f);
            }
        }
        else
        {
            if (lbStopMotors || !mbRumbleEnabled)
            {
                lPad.SetRumble(0.0f, 0.0f);
            }
            else
            {
                f32 lfLeft  = (lfLeftMotorValue  < 0.0f) ? 0.0f : ((lfLeftMotorValue  > 1.0f) ? 1.0f : lfLeftMotorValue);
                f32 lfRight = (lfRightMotorValue < 0.0f) ? 0.0f : ((lfRightMotorValue > 1.0f) ? 1.0f : lfRightMotorValue);
                lPad.SetRumble(lfLeft, lfRight);
            }
        }
    }
}
