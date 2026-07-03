#pragma once

// ============================================================================
// b5-decomp/src/GameSource/Effects/SharedIO/BrnEffectsModuleIO_InputBuffer.h
//
// BrnEffects::EffectsIO::InputBuffer -- the per-frame input aggregate the game
// bridges (BrnGame::BrnGameModule::BridgeEntityToEffects / DoUpdate_Effects) fill
// before the effects module updates (BrnEffects::EffectsModule::Update). It embeds
// several sub-interfaces/queues BY VALUE; each guarded accessor takes the buffer
// lock, then returns &member (getters) or block-copies into &member (setters); the
// two queue setters additionally clear the destination queue's live count first.
//
// Lock bit per the X360 asm (reproduced verbatim, whichever bit is tested):
//   read-lock  (status>>4 &1) => IsBufferLockedForReading()  -> "Not locked for reading\n"
//   write-lock (status>>3 &1) => IsBufferLockedForWriting()  -> "Not locked for writing\n"
// (These Effects lock strings carry the trailing \n per the X360 rodata.)
//
// IDIOM: OPAQUE BYTE IMAGE (mirrors the committed BrnAIModuleIO_OutputBuffer /
// BrnCrashModuleIO opaque-offset idiom). The interior sub-interfaces are large opaque
// payloads whose full field layout is owned by their own ledger TUs; most members have
// only their return offset attested (not their size or the gaps between them), so no
// named members are declared -- each accessor reinterprets its member at the attested
// byte offset. This whole class is opaque-image for internal coherence (16 of 19
// accessors are attested only by offset); the 3 EffectsIO-bare accessors folded in here
// (GetGameActionQueue const/mutable @0xA690, GetTriangleCacheInterface const @0xE400)
// use the SAME idiom so the class does not mix named + opaque members.
//
// FULL member byte offsets are X360-attested (accessor bodies at the addresses noted
// per member below).
//
// VERIFIER-CORRECTED OFFSETS (three members in the recon had the addis+1/-imm high
// bit dropped -- these are the asm-authoritative values):
//   mActiveRaceCarInterface       @ +0x01140 (Set XMemCpy 0x28F0=10480)  0x823BA490
//   mVehiclePhysicalStateQueue    @ +0x03A30 (Set clear+Append)          0x823C96B8
//   mCameraInput                  @ +0x07A00 (Get 0x8227D940 / Set operator= 0x823C9770)
//   mTimerStatusInterface         @ +0x07B60 (Get 0x8227D9E8 / Set 48-byte 0x823BA548)
//   mContactSpyInterface          @ +0x07B90 (Get 0x8227DA90 / Set 1 word 0x823BA658)
//   mDeformationInterface         @ +0x07BA0 (Get 0x8227DB38 / Set operator= 0x823C9820)
//   mGameActionQueue              @ +0x0A690 (Get 0x8227DBE0 R / 0x823BA708 W)   [EffectsIO-bare]
//   mEffectsEnvironmentInterface  @ +0x0DAA0 (Set 16-byte Vector2 0x823BA868)    [was 0x7AA0]
//   mReplayStatusInterface        @ +0x0DAB0 (Set operator= 0x823BA7B0)          [was 0x7AB0]
//   mTriangleCacheInterface       @ +0x0E400 (Get 0x8227DD30 R / Set 1 word 0x823BA928)
//   mAudioEffectsMessageQueue     @ +0x0E404 (Get 0x8227DDD8 / Set memcpy 0x90=144 0x823BA9E0)
//   mPropVFXLocatorQueue          @ +0x0E0D0 (Set clear+Append 0x823C98D0)       [was 0x10D0]

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"   // CgsModule::IOBuffer

namespace BrnEffects
{
namespace EffectsIO
{
    // Minimal BaseEventQueue view for the two queue setters: the setter only clears the
    // live count (miLength @ +8) and calls the queue's Append. The concrete element type
    // (PhysicalTrafficState / PropVFXLocatorEvent) is owned by another TU, so a view is used.
    struct InputBufferBaseEventQueueView
    {
        void* mpHead;   // +0
        void* mpTail;   // +4
        s32   miLength; // +8  (X360: stw 0, 8(queue) resets this before Append)
        void Append(const InputBufferBaseEventQueueView& lrSource);
    };

    struct InputBuffer : public CgsModule::IOBuffer
    {
        // Typed opaque stand-ins for the two EffectsIO-bare accessor return handles (only
        // the return offset is attested; kept opaque like every other member).
        struct GameActionQueue          { u8 maOpaquePayload[16];    };  // @ +0xA690  DWARF :173 (foreign)
        struct InTriangleCacheInterface { u8 maOpaquePayload[0x400]; };  // @ +0xE400  DWARF :177 (foreign)

        // ---- X360-attested member byte offsets (see file header for evidence) ----
        enum EMemberOffset
        {
            KU_ACTIVE_RACE_CAR_INTERFACE_OFFSET     = 0x01140,
            KU_VEHICLE_PHYSICAL_STATE_QUEUE_OFFSET  = 0x03A30,
            KU_CAMERA_INPUT_OFFSET                  = 0x07A00,
            KU_TIMER_STATUS_INTERFACE_OFFSET        = 0x07B60,
            KU_CONTACT_SPY_INTERFACE_OFFSET         = 0x07B90,
            KU_DEFORMATION_INTERFACE_OFFSET         = 0x07BA0,
            KU_GAME_ACTION_QUEUE_OFFSET             = 0x0A690, // EffectsIO-bare (Get R/W)
            KU_EFFECTS_ENVIRONMENT_INTERFACE_OFFSET = 0x0DAA0, // asm addis+1/-0x2560
            KU_REPLAY_STATUS_INTERFACE_OFFSET       = 0x0DAB0, // asm addis+1/-0x2550 (a1+55984)
            KU_PROP_VFX_LOCATOR_QUEUE_OFFSET        = 0x0E0D0, // asm addis+1/-0x1F30
            KU_TRIANGLE_CACHE_INTERFACE_OFFSET      = 0x0E400,
            KU_AUDIO_EFFECTS_MESSAGE_QUEUE_OFFSET   = 0x0E404, // asm addis+1/-0x1BFC
        };

        // Payload sizes the X360 setters block-copy (attested by memcpy/XMemCpy counts,
        // or by the sub-object's own attested span for the operator=-copied members).
        enum EMemberSize
        {
            KU_ACTIVE_RACE_CAR_INTERFACE_SIZE     = 0x28F0, // 10480 (XMemCpy count)
            KU_AUDIO_EFFECTS_MESSAGE_QUEUE_SIZE   = 0x90,   // 144   (memcpy count)
            KU_EFFECTS_ENVIRONMENT_INTERFACE_SIZE = 0x10,   // 16    (DWARF EffectsEnvironmentInterface = one Vector2; two 8-byte runs)
            KU_TIMER_STATUS_INTERFACE_SIZE        = 0x30,   // 48    (two 24-byte field runs)
            // operator=-copied sub-objects: copy span owned by the foreign type's TU; the
            // effects setter only proves the destination offset. NOMINAL below.
            KU_CAMERA_INPUT_SIZE                  = 0x10,   // NOMINAL -- Camera operator= span not attested here
            KU_DEFORMATION_INTERFACE_SIZE         = 0x10,   // NOMINAL -- Deformation operator= span not attested here
            KU_REPLAY_STATUS_INTERFACE_SIZE       = 0x10,   // NOMINAL -- StatusInterface operator= span not attested here
        };

        // ---- getters (read-lock: status bit 4) ----
        const GameActionQueue*          GetGameActionQueue() const;      // 0x8227DBE0 :138 (EffectsIO-bare)
        GameActionQueue*                GetGameActionQueue();            // 0x823BA708 :137 (EffectsIO-bare, write-lock)
        const InTriangleCacheInterface* GetTriangleCacheInterface() const; // 0x8227DD30 :152 (EffectsIO-bare)
        const void* GetCameraInput() const;          // 0x8227D940 :119
        const void* GetTimerStatusInterface() const; // 0x8227D9E8 :123
        const void* GetContactSpyInterface() const;  // 0x8227DA90 :127
        const void* G() const;                       // 0x8227DB38 :131 (returns &mDeformationInterface)
        const void* GetAud() const;                  // 0x8227DDD8 :156 (returns &mAudioEffectsMessageQueue)

        // ---- setters (write-lock: status bit 3) ----
        void SetActiveRaceCarInterface(const void* lpInterface);      // 0x823BA490 :109
        void SetVehiclePhysicalStateQueue(const void* lpQueue);       // 0x823C96B8 :113
        void SetCameraInput(const void* lpCameraInput);               // 0x823C9770 :117
        void SetTimerStatusInterface(const void* lpTimer);            // 0x823BA548 :121
        void SetContactSpyInterface(const void* lpInterface);         // 0x823BA658 :125
        void SetDeformationInterface(const void* lpInterface);        // 0x823C9820 :129
        void SetReplayStatusInterface(const void* lpStatus);          // 0x823BA7B0 :141
        void SetEffectsEnvironmentInterface(const void* lpEnv);       // 0x823BA868 :143
        void SetPropVFXLocatorQueue(const void* lpQueue);             // 0x823C98D0 :147
        void SetTriangleCacheInterface(const void* lpInterface);      // 0x823BA928 :151
        void SetAudioEffectsMessageQueue(const void* lpQueue);        // 0x823BA9E0 :154

    private:
        // Raw byte accessor over the member image (this points at the IOBuffer status byte @+0).
        u8*       MemberImage()       { return reinterpret_cast<u8*>(this); }
        const u8* MemberImage() const { return reinterpret_cast<const u8*>(this); }

        // Backing storage laid out after the IOBuffer base. Accessors index from `this` (the
        // IOBuffer status byte @+0), so the object need only reach the last member's tail:
        // mAudioEffectsMessageQueue @ 0xE404 + 144 = 0xE494, rounded up to 0xE4A0. The lowest
        // touched member is mActiveRaceCarInterface @ 0x1140; over-reserving the base is safe.
        u8 maImage[0xE4A0];
    };
}
}
