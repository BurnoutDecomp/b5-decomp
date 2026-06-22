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

    // ------------------------------------------------------------------------
    // PadOutputInformation is the per-pad input/output record the input module publishes for
    // each connected pad. GetPadInfo(port) returns &maPadOutputInformation[port]; the game's
    // controller bridges (BrnGame::BrnGameModule::BridgeControllerTo*) read this record by field.
    //
    // ADDITIVE GROW (was an opaque u8[932] placeholder). The fields named below are exactly the
    // ones the bridge funcs + SetButtonPressed + MapActionInfoToDebugController read, each pinned
    // to its X360 byte offset; untouched spans remain explicit u8 storage so the record stays
    // 932 bytes (0x3A4, the X360 stride `mulli r11, r22, 0x3A4`) and the maPadOutputInformation[7]
    // array offsets do not move. FLAG: the precise semantics of most fields (axis identities,
    // which action each ActionInfo slot is) are not DWARF/leak-attested -- they are named from
    // their bridge usage (analogue stick deflections, per-action {value,status} pairs, the
    // controller connection/state/user-index tail). Promote field names as more callers land.
    //
    // ActionInfo is the per-action {value, status} pair the bridges index as `&record.maActionInfo[k]`
    // (X360 `v15 = padRecord + 0x18; v15[2*k]`). The action-index tables map a GUI/world action id k
    // to a slot; status bit0 == held this frame, bit1 == pressed this frame, bit2 == released this
    // frame (the &1 / >>1&1 / >>2&1 tests across the bridges + SetButtonPressed). The block also holds
    // the two analogue stick deflections at its head (the >0.25 / >0.1 magnitude tests).
    struct ActionInfo
    {
        f32 mfValue;    // +0x00 analogue / axis magnitude for this action slot
        u32 muStatus;   // +0x04 bit0 held, bit1 pressed-this-frame, bit2 released-this-frame
    };

    struct PadOutputInformation
    {
        // ---- leading analogue axes ---------------------------------------------------------------
        // The two analogue sticks. The GUI bridge forwards {LX,LY} and {RX,RY} as GuiEventControllerAxis,
        // the world bridge maps LX through the steering response curve, and SetButtonPressed tests
        // |LX|>0.25 && |RY?|>0.25 for the both-sticks-deflected flag.
        f32 mfStickLX;          // +0x00
        f32 mfStickLY;          // +0x04
        f32 mfStickRX;          // +0x08
        f32 mfStickRY;          // +0x0C
        f32 mfAxis10;           // +0x10 (world: ShowtimeIntro steering source pre-curve)
        f32 mfAxis14;           // +0x14
        // ---- per-action {value,status} table @ +0x18 ---------------------------------------------
        // 96 entries (8-byte stride) spanning +0x18..+0x318. Indexed by-name via maActionInfo[k]; the
        // bridges read specific slots (k = 4,6,8,..,17) plus MapActionInfoToDebugController copies
        // maActionInfo[0..21] into the debug-controller image, and SetButtonPressed reads a sparse set
        // of slots from this same block (it receives &maActionInfo[0] == record+0x18 as its source).
        ActionInfo maActionInfo[(0x318 - 0x18) / sizeof(ActionInfo)]; // +0x18 .. +0x318 (96 entries)
        u8  maPad0x318[0x398 - 0x318];   // +0x318 unattested span up to the connection/state tail
        // ---- connection / controller-state / connected-this-scan tail ----------------------------
        u32  muConnectionWord;  // +0x398 (GetPadInfoForPlayer0: `*(PadInfo+920)`; 0 => use this pad)
        u32  meControllerState; // +0x39C (==2 test feeds the pressed-bit in ToGameState / ToWorld / ToGui)
        u8   mbDisconnected;    // +0x3A0 (GetPadInfo scan / ToGui: nonzero => pad not present this scan)
        u8   maPad0x3A1[0x3A4 - 0x3A1]; // +0x3A1 close the 932-byte (0x3A4) record
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
