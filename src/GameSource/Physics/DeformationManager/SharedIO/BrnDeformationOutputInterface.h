#pragma once

// Deformation output-interface event payloads (the subset the event queues embed).
// Reconstructed from the DecFIGS DWARF (member names/types) + the X360 spine. The
// event structs are 16-byte aligned to match the queues' inline-buffer alignment.
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationEvents.h"  // BrnCommonTypes, EBodyParts, the deformation events
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                          // CgsModule::EventQueue

namespace BrnPhysics
{
    namespace Deformation
    {
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

        // Output event: a glass pane cracked or smashed.
        struct alignas(16) GlassSmashOrCrackEvent
        {
            Vector3        maCorners[4];
            Vector3        mNormal;
            Vector3        mLinearVelocity;
            Matrix44Affine mTransform;
            EntityId       mVehicleEntityId;
            EBodyParts     meGlassPart;
            EGlassState    meNewState;
            f32            mfCrackAmount;
            bool           mbDontPlaySmashEffect;
        };

        // The deformation output interface: a per-frame buffer of fixed-capacity event
        // queues the deformation system fills. Member order recovered from Construct
        // @0x8228F1B0; the X360 byte offsets it writes are noted per member (they hold on
        // the 32-bit console; this PC build is x64 so the absolute offsets differ — the
        // queues are accessed by name, not offset). The leading region and two scalar
        // flags are only partially recovered (padded placeholder).
        class DeformationOutputInterface
        {
        public:
            void Construct();

            u8  maPad0[112];     // +0     unrecovered leading members (placeholder)
            u32 muClearedFlag0;  // +112   X360 a1[28]   — cleared on Construct
            CgsModule::EventQueue<JointedPartStateEvent, 50>            mJointedPartStateQueue;            // X360 +116
            CgsModule::EventQueue<DetachedPartNotificationEvent, 50>    mDetachedPartNotificationQueue;    // X360 +928
            CgsModule::EventQueue<BrokenJointNotificationEvent, 10>     mBrokenJointNotificationQueue;     // X360 +2544
            CgsModule::EventQueue<DetachedPartCurrentPositionEvent, 50> mDetachedPartCurrentPositionQueue; // X360 +2880
            CgsModule::EventQueue<GlassSmashOrCrackEvent, 20>           mGlassSmashOrCrackQueue;           // X360 +6896
            u32 muClearedFlag1;  // X360 a1[2688] — cleared on Construct
        };
    }
}
