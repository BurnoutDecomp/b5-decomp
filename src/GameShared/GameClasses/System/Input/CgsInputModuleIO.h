#pragma once

// Input-module IO event payloads (the subset the boot-path event queues embed).
// Reconstructed from the DecFIGS DWARF. The input events derive from an empty
// per-module Event base (the CgsModule event-queue convention — see e.g.
// BrnContactSpyEvents.h).
#include "types.hpp"
#include "GameShared/GameClasses/System/Input/CgsInputTypes.h"   // CgsInput::EBindResult, EUnbindResult
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"           // CgsModule::IOBuffer (lock state; status byte @ +0)
#include "GameShared/GameClasses/Module/CgsEventQueue.h"         // CgsModule::EventQueue<T,N>

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

    // ------------------------------------------------------------------------
    // IO payload buffers (subset). MINIMAL-SLICE NOTE (follows BrnGameStateModuleIO.h
    // precedent): only the members the lock-guarded Get* accessors touch are modelled,
    // each pinned to its exact X360 byte offset. The lock-guarded accessors assert the
    // IOBuffer lock (read bit4 / write bit3) then return &member-at-offset. Tail/untouched
    // members (PadOutputInformation[7], the PreWorld rumble queues + Timer/bool flags,
    // WheelFFSpring) are left to each buffer's own full-reconstruction TU.
    // ------------------------------------------------------------------------

    // PadMapping is not committed yet (its full layout drags in the un-homed ActionMapping[34]).
    // PostWorldInputBuffer only returns a pointer to its EventQueue<PadMapping,7>, so an incomplete
    // forward declaration suffices for the pointer-typed accessor and the member is held as raw
    // aligned storage of the correct width-anchor (it is the last modelled member, so its exact
    // size does not move any touched member). Promote to the real type when PadMapping lands.
    struct PadMapping;

    // ---- PostWorldInputBuffer (DWARF CgsInputModuleIO.h:776) ----------------
    //   GetBindRequestQueue() const -> X360 0x828E6A88, read-lock, this+4
    //   GetPadMappingQueue()  const -> X360 0x828E6BD8, read-lock, this+156
    struct PostWorldInputBuffer : public CgsModule::IOBuffer
    {
        typedef CgsModule::EventQueue<BaseInputEvent, 8> BindRequestQueue;    // 76B
        typedef CgsModule::EventQueue<BaseInputEvent, 8> UnBindRequestQueue;  // 76B
        typedef CgsModule::EventQueue<PadMapping,     7> PadMappingQueue;

        const BindRequestQueue*   GetBindRequestQueue() const;   // 0x828E6A88
        const UnBindRequestQueue* GetUnBindRequestQueue() const; // declared-only
        const PadMappingQueue*    GetPadMappingQueue() const;    // 0x828E6BD8

    private:
        BindRequestQueue   mBindRequestQueue;    // @ +4   .. +80
        UnBindRequestQueue mUnBindRequestQueue;  // @ +80  .. +156
        // PadMappingQueue mPadMappingQueue @ +156 -- raw storage (PadMapping un-homed; last
        // modelled member, so its exact size does not affect the offsets above).
        u8                 mPadMappingQueueStorage[12 + 7 * (sizeof(BaseInputEvent) + 4)]; // @ +156
        // CgsInput::Device::WheelFFSpring mWheelFFSpring; // tail member -- own TU
    };

    // PadOutputInformation is the per-pad output record (rumble/jolt/force-feedback state
    // the input module publishes back to each connected pad). Its full field layout is its
    // own reconstruction TU; here only its X360 byte width is needed so OutputBuffer's
    // maPadOutputInformation[7] array lands at the right offset and GetPadInfo()'s
    // `&a1[932*a2 + 296]` element math reproduces exactly. Width 932B (0x3A4) is taken from
    // the X360 stride (`mulli r11, r22, 0x3A4`). Promote to the real fielded type when homed.
    struct PadOutputInformation
    {
        u8 mRawStorage[932]; // X360 sizeof == 0x3A4; HONEST placeholder until fielded TU
    };

    // ---- OutputBuffer (DWARF CgsInputModuleIO.h:844) ------------------------
    //   GetBindResultQueue()      const -> X360 0x823B1038, read-lock,  this+4
    //   GetUnbindResultQueue()    const -> X360 0x823B10E0, read-lock,  this+112
    //   GetPadDisconnectedQueue() const -> X360 0x823B1188, read-lock,  this+220
    //   GetUnbindResultQueue()          -> X360 0x828E6DD0, write-lock, this+112
    //   GetPadDisconnectedQueue()       -> X360 0x828E6E78, write-lock, this+220
    //   GetPadInfo(pad)           const -> X360 0x823B1230, read-lock,  this+296+932*pad
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        typedef CgsModule::EventQueue<BindResult,     8> BindResultQueue;      // 108B (BindResult=12B)
        typedef CgsModule::EventQueue<UnBindResult,   8> UnBindResultQueue;    // 108B (UnBindResult=12B)
        typedef CgsModule::EventQueue<BaseInputEvent, 8> PadDisconnectedQueue; // 76B

        const BindResultQueue*      GetBindResultQueue() const;      // 0x823B1038
        const UnBindResultQueue*    GetUnbindResultQueue() const;    // 0x823B10E0 (DWARF spells it "Unbind")
        const PadDisconnectedQueue* GetPadDisconnectedQueue() const; // 0x823B1188
        BindResultQueue*            GetBindResultQueue();            // declared-only
        UnBindResultQueue*          GetUnbindResultQueue();          // 0x828E6DD0
        PadDisconnectedQueue*       GetPadDisconnectedQueue();       // 0x828E6E78

        // X360 0x823B1230 - read-lock accessor returning the iPort-th pad output record.
        const PadOutputInformation* GetPadInfo(s32 iPort) const;     // 0x823B1230

    private:
        BindResultQueue      mBindResultQueue;          // @ +4   .. +112
        UnBindResultQueue    mUnBindResultQueue;        // @ +112 .. +220
        PadDisconnectedQueue mPadDisconnectedQueue;     // @ +220 .. +296
        PadOutputInformation maPadOutputInformation[7]; // @ +296 .. +6820
    };

    // ---- PreWorldInputBuffer (DWARF CgsInputModuleIO.h:603) -----------------
    //   GetPlayJoltEffectEventQueue() const -> X360 0x828E6740, read-lock, this+4
    struct PreWorldInputBuffer : public CgsModule::IOBuffer
    {
        typedef CgsModule::EventQueue<PlayJoltEffectEvent, 4> PlayJoltEffectEventQueue; // PlayJoltEffectEvent=60B

        const PlayJoltEffectEventQueue* GetPlayJoltEffectEventQueue() const; // 0x828E6740

    private:
        PlayJoltEffectEventQueue mPlayJoltEffectEventQueue; // @ +4
        // remaining rumble queues (PlayRumble/ChangeVolume/Stop), TimerStatusInterface,
        // mbPauseRumble, mbEnableRumble, mbForceFeedback -- own TU.
    };
}
}
