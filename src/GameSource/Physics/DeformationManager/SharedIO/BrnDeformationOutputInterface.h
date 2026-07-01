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

            // DWARF :154 (own TU): hand-written copy assignment. X360 emits it
            // out-of-line @ 0x823C8900; the world buffer's SetDeformationOutputInterface
            // @ 0x827AA658 calls it directly (`mDeformationOutputInterface = *lpInterface`).
            // Declared here so callers reverse the X360 call; bodied by this type's own TU.
            // (The DWARF also declares Append(const DeformationOutputInterface*) at :157
            // and Clear() at :160 -- left undeclared until an X360 body attests them.)
            DeformationOutputInterface& operator=(const DeformationOutputInterface& lrOther);

            u8  maPad0[112];     // +0     unrecovered leading members (placeholder)
            u32 muClearedFlag0;  // +112   X360 a1[28]   — cleared on Construct
            CgsModule::EventQueue<JointedPartStateEvent, 50>            mJointedPartStateQueue;            // X360 +116
            CgsModule::EventQueue<DetachedPartNotificationEvent, 50>    mDetachedPartNotificationQueue;    // X360 +928
            CgsModule::EventQueue<BrokenJointNotificationEvent, 10>     mBrokenJointNotificationQueue;     // X360 +2544
            CgsModule::EventQueue<DetachedPartCurrentPositionEvent, 50> mDetachedPartCurrentPositionQueue; // X360 +2880
            CgsModule::EventQueue<GlassSmashOrCrackEvent, 20>           mGlassSmashOrCrackQueue;           // X360 +6896
            u32 muClearedFlag1;  // X360 a1[2688] — cleared on Construct
        };

        // ====================================================================
        // MINIMAL SLICE for the RaceCarEntityModuleIO IO-buffer unlock; full layout
        // reconstructed by DeformationOutputInterfaceForEntityModules's own TU (DWARF home
        // BrnDeformationOutputInterface.h:233). Size 256 (NOMINAL — not byte-verified, grown
        // by own TU).
        //
        // DWARF (BrnDeformationOutputInterface.h:233) gives the full member list:
        //   uint32_t muNumEntries; VolumeInstanceId maBaseIDs[28]; WheelPhysicalStates maWheelStates[28];
        //   int32_t miNumSkinnedModels; SkinData maSkinData[28]; int32_t miNumLocatorOutputs;
        //   VehicleLocatorOutput maLocatorData[28];
        //   EventQueue<DetachedPartRenderEvent,50> mDetachedPartRenderQueue;
        //   DeformationOutputInterface::GlassSmashOrCrackQueue mGlassSmashOrCrackQueue;
        // WheelPhysicalStates/SkinData/VehicleLocatorOutput/DetachedPartRenderEvent are not yet
        // in-tree (full reconstruction cascades), so collapsed to an opaque reserved blob here.
        // alignas(16): carries Matrix44Affine-bearing locator arrays + two EventQueues (SIMD/queue).
        // Embedded BY VALUE in RaceCarEntityModuleIO::InputBuffer_PostPhysics (IO header :539);
        // accessed only by-name (Get/SetDeformationOutputInterfaceForEntityModules return &member),
        // so byte-exact size is not required for the IO-buffer unlock.
        struct alignas(16) DeformationOutputInterfaceForEntityModules
        {
            unsigned char maReserved[256];
        };
    }
}
