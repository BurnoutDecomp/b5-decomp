#pragma once

// Deformation output-interface event payloads + the two per-frame output buffers
// (DeformationOutputInterface and DeformationOutputInterfaceForEntityModules).
// Reconstructed from the DecFIGS DWARF (member names/types) + the X360 spine. The
// event structs are 16-byte aligned to match the queues' inline-buffer alignment.
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationEvents.h"  // BrnCommonTypes, EBodyParts, the deformation events (+ DetachedPartRenderEvent)
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                          // CgsModule::EventQueue

namespace BrnPhysics
{
    namespace Deformation
    {
        struct DeformationState;   // fwd (mpDeformationState)

        enum EGlassState : s32
        {
            E_GLASS_STATE_INTACT  = 0,
            E_GLASS_STATE_CRACKED = 1,
            E_GLASS_STATE_SMASHED = 2,
            NUM_GLASS_STATES      = 3
        };

        // Output event: a detached part's current world transform.
        struct alignas(16) DetachedPartCurrentPositionEvent
        {
            Matrix44Affine mTransform;
            EntityId       mVehicleEntityId;
            EBodyParts     meType;
        };

        // Output event: a glass pane cracked or smashed. sizeof == 0xB1 (177) -> alignas(16) 192.
        // Proven by the X360 copy-assignment spine @0x82279838 (ten quadwords 0x00..0x90 + three
        // dwords 0xA0/0xA4/0xA8 + one f32 0xAC + one byte 0xB0). The implicit member-wise
        // operator= reproduces it (see BaseEventQueue<GlassSmashOrCrackEvent> instantiations).
        struct alignas(16) GlassSmashOrCrackEvent
        {
            Vector3        maCorners[4];          // +0x00 (64)
            Vector3        mNormal;               // +0x40 (16)
            Vector3        mLinearVelocity;       // +0x50 (16)
            Matrix44Affine mTransform;            // +0x60 (64)
            EntityId       mVehicleEntityId;      // +0xA0
            EBodyParts     meGlassPart;           // +0xA4
            EGlassState    meNewState;            // +0xA8
            f32            mfCrackAmount;         // +0xAC
            bool           mbDontPlaySmashEffect; // +0xB0
        };

        // One skinned-model scratch record (DWARF BrnDeformationOutputInterface.h:49). 8 bytes on
        // console: an EntityId dword + a Vector3Plus* scratch pointer. The ForEntityModules copy
        // moves it as one 8-byte unit per the asm.
        struct SkinData
        {
            EntityId    mEntityId;                // +0x00
            const void* mpSkinOffsets_Scratch;    // +0x04 (const rw::math::vpu::Vector3Plus* on console)
        };

        // -------------------------------------------------------------------------
        // DeformationOutputInterface  (DWARF BrnDeformationOutputInterface.h:132)
        //
        // Per-frame output buffer the deformation system fills. Member order + the two scalars
        // recovered from Construct @0x8228F1B0 and the copy-assignment @0x823C8900. The X360 byte
        // offsets (32-bit console pointers) are noted per member; every access is by-name, so the
        // absolute x64 offsets differ harmlessly.
        //   +0x0000 maBaseEntityIDs[28]  (EntityId, stride 4)
        //   +0x0070 mpDeformationState
        //   +0x0074 mJointedPartStateQueue            EventQueue<JointedPartStateEvent,50>
        //   +0x03A0 mDetachedPartNotificationQueue    EventQueue<DetachedPartNotificationEvent,50>
        //   +0x09F0 mBrokenJointNotificationQueue     EventQueue<BrokenJointNotificationEvent,10>
        //   +0x0B40 mDetachedPartCurrentPositionQueue EventQueue<DetachedPartCurrentPositionEvent,50>
        //   +0x1AF0 mGlassSmashOrCrackQueue           EventQueue<GlassSmashOrCrackEvent,20>
        //   +0x2A00 miNumLocatorOutputs
        //   +0x2A04 maLocatorData[28]    (VehicleLocatorOutput, stride 8)
        // -------------------------------------------------------------------------
        class DeformationOutputInterface
        {
        public:
            typedef CgsModule::EventQueue<JointedPartStateEvent, 50>            JointedPartStateQueue;
            typedef CgsModule::EventQueue<DetachedPartNotificationEvent, 50>    DetachedPartNotificationQueue;
            typedef CgsModule::EventQueue<BrokenJointNotificationEvent, 10>     BrokenJointNotificationQueue;
            typedef CgsModule::EventQueue<DetachedPartCurrentPositionEvent, 50> DetachedPartCurrentPositionQueue;
            typedef CgsModule::EventQueue<GlassSmashOrCrackEvent, 20>           GlassSmashOrCrackQueue;

            void Construct();

            // DWARF :137: hand-written copy assignment. X360 emits it out-of-line @ 0x823C8900;
            // the world/sound/effects PostPhysics IO buffers call it directly
            // (SetDeformationOutputInterface: `mDeformationOutputInterface = *lpInterface`).
            DeformationOutputInterface& operator=(const DeformationOutputInterface& lkrOther);

            // Members are public: committed consumers reach the queues by name (e.g.
            // BrnDeformableObject_GlassState.cpp: `lpOut->mGlassSmashOrCrackQueue.AddEventSafe(..)`),
            // matching the original committed header where the whole body was public.
            EntityId                         maBaseEntityIDs[28];               // +0x0000
            const DeformationState*          mpDeformationState;                // +0x0070
            JointedPartStateQueue            mJointedPartStateQueue;            // +0x0074
            DetachedPartNotificationQueue    mDetachedPartNotificationQueue;    // +0x03A0
            BrokenJointNotificationQueue     mBrokenJointNotificationQueue;     // +0x09F0
            DetachedPartCurrentPositionQueue mDetachedPartCurrentPositionQueue; // +0x0B40
            GlassSmashOrCrackQueue           mGlassSmashOrCrackQueue;           // +0x1AF0
            s32                              miNumLocatorOutputs;               // +0x2A00
            // VehicleLocatorOutput (DWARF :117: EntityId + const VehicleLocatorData*) is 8 bytes on
            // console and the copy-assignment moves it as one 8-byte unit; an 8-byte opaque slot
            // preserves the stride/layout until VehicleLocatorOutput is homed.
            struct LocatorOutputSlot { u64 mOpaque; };
            LocatorOutputSlot                maLocatorData[28];                 // +0x2A04
        };

        // -------------------------------------------------------------------------
        // DeformationOutputInterfaceForEntityModules  (DWARF BrnDeformationOutputInterface.h:233)
        //
        // Entity-module-facing per-frame output buffer. operator= (@0x827A96D0) copies the array
        // region through the asm byte offsets (interior element types not fully homed). Members
        // named per DWARF; X360 byte offsets noted.
        //
        // WHEEL-STATE STRIDE (coherence-critical): the asm advances the maWheelStates cursor by
        // 0x190 (400) per element, and miNumSkinnedModels lands at 0xF0 + 400*28 = 0x2CB0 exactly.
        // The committed WheelPhysicalStates home (BrnWheelPhysicalStates.h) is a u8-array struct of
        // sizeof 0x188 (392) -- so a by-name `WheelPhysicalStates maWheelStates[28]` would place
        // miNumSkinnedModels at 0x2BD0, breaking the layout. The DWARF WheelPhysicalStates
        // (WheelPhysicalState[4]{Matrix44Affine + 2xVector3} + 3x bool[4]) has alignment 16 from its
        // Matrix44Affine members, so its true sizeof rounds 392->400; the u8 home lost that. To keep
        // this container's layout exact WITHOUT depending on the home's (currently 392) sizeof, the
        // wheel-state array is declared as a 400-byte asm-authoritative slot. The operator= copies
        // each element by casting the slot to WheelPhysicalStates& and invoking its homed operator=.
        //   +0x0000 muNumEntries
        //   +0x0008 maBaseIDs[28]        (VolumeInstanceId u64, stride 8)
        //   +0x00F0 maWheelStates[28]    (WheelPhysicalStates, stride 0x190 = 400)
        //   +0x2CB0 miNumSkinnedModels
        //   +0x2CB4 maSkinData[28]       (SkinData, stride 8)
        //   +0x2D94 miNumLocatorOutputs
        //   +0x2D98 maLocatorData[28]    (VehicleLocatorOutput, stride 8)
        //   +0x2E80 mDetachedPartRenderQueue  EventQueue<DetachedPartRenderEvent,50>
        //   +0x3E30 mGlassSmashOrCrackQueue   EventQueue<GlassSmashOrCrackEvent,20>
        // -------------------------------------------------------------------------
        struct alignas(16) DeformationOutputInterfaceForEntityModules
        {
            typedef CgsModule::EventQueue<DetachedPartRenderEvent, 50> DetachedPartRenderQueue;

            void Construct();
            DeformationOutputInterfaceForEntityModules& operator=(const DeformationOutputInterfaceForEntityModules& lkrOther);  // X360 @0x827A96D0

        private:
            // 400-byte slot per wheel-state entry (asm stride 0x190); the operator= reinterprets each
            // slot as WheelPhysicalStates& to invoke the homed by-name copy.
            struct alignas(16) WheelStateSlot { u8 mOpaque[0x190]; };  // 400 bytes
            struct LocatorOutputSlot { u64 mOpaque; };

            u32                     muNumEntries;                 // +0x0000
            u64                     maBaseIDs[28];                // +0x0008 (VolumeInstanceId)
            WheelStateSlot          maWheelStates[28];            // +0x00F0 (stride 0x190 = 400)
            s32                     miNumSkinnedModels;           // +0x2CB0
            SkinData                maSkinData[28];               // +0x2CB4
            s32                     miNumLocatorOutputs;          // +0x2D94
            LocatorOutputSlot       maLocatorData[28];            // +0x2D98
            DetachedPartRenderQueue mDetachedPartRenderQueue;     // +0x2E80
            DeformationOutputInterface::GlassSmashOrCrackQueue mGlassSmashOrCrackQueue; // +0x3E30
        };
    }
}
