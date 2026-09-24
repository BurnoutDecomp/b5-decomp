// CgsInput::InputPads -- construction, preparation, player/port bind reset, and the jolt/rumble
// force-feedback effect engine. Reconstructed from BURNOUT_X360_ARTIST.XEX (CgsInputPads.cpp,
// methods @0x828E72xx / 0x828EFxxx / 0x828DCxxx). The class layout + member offsets are pinned in
// CgsInputPads.h (offsetof static_asserts). GetDebugGamePad lives in CgsInputPads_GetDebugGamePad.cpp.
//
// Source path: d:\p4\b5_main\burnout\main\code\gameshared\gameclasses\system\input\CgsInputPads.cpp

#include "GameShared/GameClasses/System/Input/CgsInputPads.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"   // CgsDev::StrStream (BindPlayerToPort's bound asserts)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"     // [DIAG] CgsDev::Log::gpDebugPrint (UpdatePadRumble witness)
#include "rw/math/fpu/scalar_operation.h"                      // rw::math::fpu::Clamp (UpdatePadRumble / FillRawData)

#include <cstdlib>   // [DIAG] std::getenv (BRN_RUMBLE_DIAG)
#include <cstring>   // std::memset (Construct clears the action-down bitmap)

namespace CgsInput
{
    // ------------------------------------------------------------------------------------------------
    // DWARF CgsInputPads.cpp:901 -- GetTotalJoltEnvelopeDuration. The total wall-clock length of one
    // ADSR envelope, summed the way UpdatePadRumble's inlined copies sum it (0x828EFD28..0x828EFD48):
    // ((release + sustain) + decay) + attack.
    // ------------------------------------------------------------------------------------------------
    f32 InputPads::GetTotalJoltEnvelopeDuration(const InputIO::JoltEnvelope& lJoltEnvelope)
    {
        return ((lJoltEnvelope.mfReleaseTime + lJoltEnvelope.mfSustainTime) + lJoltEnvelope.mfDecayTime)
             + lJoltEnvelope.mfAttackTime;
    }

    // ------------------------------------------------------------------------------------------------
    // DWARF CgsInputPads.cpp:878 -- GetTotalJoltDuration (locals lfLeftJoltTime / lfRightJoltTime). The
    // longer of the two envelopes: UpdatePadRumble's `fcmpu f0,f13 ; bgt keep ; fmr f0,f13`
    // (0x828EFD60..0x828EFD68) keeps the LEFT time only when it is strictly greater.
    // ------------------------------------------------------------------------------------------------
    f32 InputPads::GetTotalJoltDuration(const InputIO::JoltEffect& lJoltEffect)
    {
        const f32 lfLeftJoltTime  = GetTotalJoltEnvelopeDuration(lJoltEffect.mLowFreqJoltData);
        const f32 lfRightJoltTime = GetTotalJoltEnvelopeDuration(lJoltEffect.mHighFreqJoltData);
        return (lfLeftJoltTime > lfRightJoltTime) ? lfLeftJoltTime : lfRightJoltTime;
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
            // DWARF cpp :411 rw::math::fpu::Clamp<float> -- the X360's `fsel f0,f13,f31,f0 ;
            // fsel f0,f13,f0,f30` pair (-1 then +1), which the fpu Clamp reproduces (NaN -> +1).
            lafAxisData[liAxisIndex] = rw::math::fpu::Clamp(lafAxisData[liAxisIndex] + lfAxisValue, -1.0f, 1.0f);
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
        CGS_ASSERT(lfTime >= 0.0f, "lfTime >= 0.0f");                         // cpp :843 (0x34B)

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
    // Resolve the player to a port, find the rumble slot matching the event's id, update its volume --
    // AND replace its envelope with the event's: after the `stfsx f0` volume store (0x828DC3A4) the body
    // tail-calls memcpy(&maRumbleEffects[port][slot], &event.mJoltEffect (r4+8), 0x30) at 0x828DC398..
    // 0x828DC3B8 (`addi r4,r4,8 ; li r5,0x30 ; ... ; b memcpy`). FX-RUMBLE3 2026-09-24: the envelope copy
    // was missing, so a surface rumble whose volume changed kept playing its FIRST surface's envelope.
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
        maRumbleEffects[liPort][luRumbleIndex] = lChangeVolumeRumbleEvent.mJoltEffect;
    }

    // ------------------------------------------------------------------------------------------------
    // X360 0x828DBEF0 -- BindPlayerToPort (DWARF CgsInputPads.h:88; cpp :427, two StrStream locals).
    //   player > 3 (cmplwi, unsigned): "Player out of bounds. 0 <= " << player << " < " << 4 (:432) -> 3
    //   port   > 3:                    "Port out of bounds. 0 <= "   << port   << " < " << 4 (:437) -> 4
    //   player already bound: OK when maPorts[port] already names this player, else
    //                         "Player is already bound to another port. Unbind first" (:448) -> 1
    //   port already bound (maPorts[port] != -1): "Port is already bound. Unbind first" (:453) -> 2
    //   else maPlayers[player] = {bound, port} (`stw port,4 ; stb 1,0`), maPorts[port] = player -> 0.
    // Console caller: InputModule::ProcessBindRequestQueue @0x828EF1F0 (the InputPostWorld bind chain).
    // ------------------------------------------------------------------------------------------------
    EBindResult InputPads::BindPlayerToPort(s32 liPlayer, s32 liPort)
    {
        if (static_cast<u32>(liPlayer) > KU_NUMBER_OF_PADS - 1)
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Player out of bounds. 0 <= " << liPlayer << " < " << static_cast<s32>(KU_NUMBER_OF_PADS) << "\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessage,
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\input\\CgsInputPads.cpp", 432);
            CgsDev::Assert::EndAssert();
            return E_BINDRESULTINVALIDPLAYER;
        }
        if (static_cast<u32>(liPort) > KU_NUMBER_OF_PADS - 1)
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Port out of bounds. 0 <= " << liPort << " < " << static_cast<s32>(KU_NUMBER_OF_PADS) << "\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessage,
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\input\\CgsInputPads.cpp", 437);
            CgsDev::Assert::EndAssert();
            return E_BINDRESULTINVALIDPORT;
        }

        InputPlayer& lrPlayer = maPlayers[liPlayer];
        if (lrPlayer.mbBound)
        {
            if (maiPortToPlayer[liPort] == liPlayer)
            {
                return E_BINDRESULTOK;
            }
            CGS_ASSERT(false, "Player is already bound to another port. Unbind first");   // :448
            return E_BINDRESULTPLAYERALREADYBOUND;
        }
        if (maiPortToPlayer[liPort] != -1)
        {
            CGS_ASSERT(false, "Port is already bound. Unbind first");                     // :453
            return E_BINDRESULTPORTALREADYBOUND;
        }

        lrPlayer.miPort           = liPort;
        lrPlayer.mbBound          = true;
        maiPortToPlayer[liPort]   = liPlayer;
        return E_BINDRESULTOK;
    }

    // ------------------------------------------------------------------------------------------------
    // DWARF CgsInputPads.cpp:688 -- UpdateRumble, as InputModule::ProcessRumbleRequests @0x828FFE50
    // inlines it (0x829000C4..0x829000F0): the three flag stores in the console's order (+0x10D0 pause,
    // +0x10D2 wheel force feedback, +0x10D1 enable), then UpdatePadRumble(port, maPorts[port] (the
    // `lwz r5,0(r29)` walk from +0x7A8), lfTimeStep) for each of the four ports.
    // ------------------------------------------------------------------------------------------------
    void InputPads::UpdateRumble(f32 lfTimeStep, bool lbPauseRumble, bool lbEnableRumble, bool lbEnableWheelForceFeedback)
    {
        mbRumblePaused   = lbPauseRumble;
        mbWheelFFEnabled = lbEnableWheelForceFeedback;
        mbRumbleEnabled  = lbEnableRumble;
        for (s32 liPort = 0; liPort < static_cast<s32>(KU_NUMBER_OF_PADS); ++liPort)
        {
            UpdatePadRumble(liPort, maiPortToPlayer[liPort], lfTimeStep);
        }
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
    // X360 0x828EFC58 -- UpdatePadRumble (DWARF cpp :713; locals lbStopMotors / lfLeftMotorValue /
    // lfRightMotorValue, lfJolt*MotorValue, lfRumble*MotorValue; rw::math::fpu::Clamp x4).
    // Re-read instruction by instruction 2026-09-24 (FX-RUMBLE3):
    //   0x828EFC80  mbRumblePaused -> skip both effect walks (the motors stay stopped)
    //   jolts   0x828EFC98..0x828EFD90  priority > 0 (signed): left/right = the larger of the running
    //           value and each envelope (`fcmpu ; bge skip`), time += step, then EXPIRE when the new
    //           time is strictly past the longer envelope (`fcmpu f12,f0 ; ble keep`: time 0.0,
    //           priority -1) else keep the motors running
    //   rumbles 0x828EFD94..0x828EFEB4  the same with the envelope scaled by the slot volume, the time
    //           WRAPS to 0.0 past the envelope (no expiry -- a rumble loops until it is stopped), and any
    //           active rumble keeps the motors running
    //   0x828EFEB8..0x828EFEE4  DeviceX360Pad::IsConnected() inlined (autotest byte, else +0x10) gates the
    //           rest; the wheel arm publishes the FF spring (or {1.0, 0.0}) then the motors; both arms
    //           Clamp each motor to [0,1] with the fsel pair and call SetRumble, or SetRumble(0,0).
    // liPlayer is unused (r5 is never read), as on the console.
    // ------------------------------------------------------------------------------------------------
    void InputPads::UpdatePadRumble(s32 liPort, s32 liPlayer, f32 lfTimeStep)
    {
        (void)liPlayer;

        bool lbStopMotors      = true;
        f32  lfLeftMotorValue  = 0.0f;
        f32  lfRightMotorValue = 0.0f;

        if (!mbRumblePaused)
        {
            for (u32 liJoltIndex = 0; liJoltIndex < KU_MAX_NUMBER_OF_JOLT_EFFECTS; ++liJoltIndex)
            {
                if (maiJoltEffectPriorities[liPort][liJoltIndex] > 0)
                {
                    const InputIO::JoltEffect& lJoltEffect = maJoltEffects[liPort][liJoltIndex];

                    const f32 lfJoltLeftMotorValue = UpdateJoltEnvelope(lJoltEffect.mLowFreqJoltData, mafJoltTime[liPort][liJoltIndex]);
                    if (lfLeftMotorValue < lfJoltLeftMotorValue)
                    {
                        lfLeftMotorValue = lfJoltLeftMotorValue;
                    }
                    const f32 lfJoltRightMotorValue = UpdateJoltEnvelope(lJoltEffect.mHighFreqJoltData, mafJoltTime[liPort][liJoltIndex]);
                    if (lfRightMotorValue < lfJoltRightMotorValue)
                    {
                        lfRightMotorValue = lfJoltRightMotorValue;
                    }

                    mafJoltTime[liPort][liJoltIndex] = mafJoltTime[liPort][liJoltIndex] + lfTimeStep;
                    if (mafJoltTime[liPort][liJoltIndex] > GetTotalJoltDuration(lJoltEffect))
                    {
                        mafJoltTime[liPort][liJoltIndex]             = 0.0f;
                        maiJoltEffectPriorities[liPort][liJoltIndex] = -1;
                    }
                    else
                    {
                        lbStopMotors = false;
                    }
                }
            }

            for (u32 liRumbleIndex = 0; liRumbleIndex < KU_MAX_NUMBER_OF_RUMBLE_EFFECTS; ++liRumbleIndex)
            {
                if (maiRumbleEffectPriorities[liPort][liRumbleIndex] > 0)
                {
                    const InputIO::JoltEffect& lRumbleEffect = maRumbleEffects[liPort][liRumbleIndex];

                    const f32 lfRumbleLeftMotorValue = mafRumbleVolume[liPort][liRumbleIndex]
                        * UpdateJoltEnvelope(lRumbleEffect.mLowFreqJoltData, mafRumbleTime[liPort][liRumbleIndex]);
                    if (lfLeftMotorValue < lfRumbleLeftMotorValue)
                    {
                        lfLeftMotorValue = lfRumbleLeftMotorValue;
                    }
                    const f32 lfRumbleRightMotorValue = mafRumbleVolume[liPort][liRumbleIndex]
                        * UpdateJoltEnvelope(lRumbleEffect.mHighFreqJoltData, mafRumbleTime[liPort][liRumbleIndex]);
                    if (lfRightMotorValue < lfRumbleRightMotorValue)
                    {
                        lfRightMotorValue = lfRumbleRightMotorValue;
                    }

                    mafRumbleTime[liPort][liRumbleIndex] = lfTimeStep + mafRumbleTime[liPort][liRumbleIndex];
                    if (mafRumbleTime[liPort][liRumbleIndex] > GetTotalJoltDuration(lRumbleEffect))
                    {
                        mafRumbleTime[liPort][liRumbleIndex] = 0.0f;
                    }
                    lbStopMotors = false;
                }
            }
        }

        // [DIAG] BRN_RUMBLE_DIAG -- NOT IN THE X360 BINARY. The motor-REQUEST witness (FX-RUMBLE3): one
        // line per port-frame on which an effect is driving the motors (budgeted), BEFORE the connected
        // test -- so a run on a box with no pad still shows the jolt arriving here. The PC motor leaf
        // (CgsInputPadsPC.cpp XInputSetState) adds its own line when a pad takes it.
        {
            static const bool sbPadDiag = (std::getenv("BRN_RUMBLE_DIAG") != 0);
            static s32        siPadDiagLines = 0;
            const s32         KI_PAD_DIAG_MAX_LINES = 96;
            if (sbPadDiag && !lbStopMotors && siPadDiagLines < KI_PAD_DIAG_MAX_LINES && CgsDev::Log::gpDebugPrint != 0)
            {
                ++siPadDiagLines;
                *CgsDev::Log::gpDebugPrint
                    << "[rumble] pad-request port=" << liPort
                    << " left=" << lfLeftMotorValue << " right=" << lfRightMotorValue
                    << " jolt{prio=" << maiJoltEffectPriorities[liPort][0] << "," << maiJoltEffectPriorities[liPort][1]
                    << " t=" << mafJoltTime[liPort][0] << "," << mafJoltTime[liPort][1]
                    << "} rumble{prio=" << maiRumbleEffectPriorities[liPort][0] << "," << maiRumbleEffectPriorities[liPort][1]
                    << "} enabled=" << static_cast<s32>(mbRumbleEnabled ? 1 : 0)
                    << " connected=" << static_cast<s32>(maPads[liPort].IsConnected() ? 1 : 0)
                    << " type=" << static_cast<s32>(maPads[liPort].GetDeviceType()) << "\n";
            }
        }

        if (!maPads[liPort].IsConnected())
        {
            return;
        }

        DeviceX360Pad& lPad = maPads[liPort];

        if (lPad.GetDeviceType() == Device::E_WHEEL_DEVICE_TYPE)
        {
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
                lPad.SetRumble(rw::math::fpu::Clamp(lfLeftMotorValue, 0.0f, 1.0f),
                               rw::math::fpu::Clamp(lfRightMotorValue, 0.0f, 1.0f));
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
                lPad.SetRumble(rw::math::fpu::Clamp(lfLeftMotorValue, 0.0f, 1.0f),
                               rw::math::fpu::Clamp(lfRightMotorValue, 0.0f, 1.0f));
            }
        }
    }
}
