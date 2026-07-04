#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h"
#include "GameShared/GameClasses/Physics/Deformation/BrnWheelPhysicalStates.h"  // WheelPhysicalStates::operator= (ForEntityModules copy)

// BrnPhysics::Deformation::DeformationOutputInterface / ...ForEntityModules.
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX. This TU owns:
//   Construct  @ 0x8228F1B0  -- construct the five fixed-capacity event queues + clear the two scalars
//   operator=  @ 0x823C8900  -- DeformationOutputInterface copy assignment
//   operator=  @ 0x827A96D0  -- DeformationOutputInterfaceForEntityModules copy assignment

namespace BrnPhysics
{
namespace Deformation
{
    // DeformationOutputInterface::Construct  @ 0x8228F1B0. Constructs the five fixed-capacity
    // deformation event queues (each points itself at its inline buffer, sets the capacity, and
    // clears the live count) and clears the two scalars the asm zeroes: mpDeformationState (+0x70)
    // and miNumLocatorOutputs (+0x2A00). The X360 pseudocode additionally re-zeroes each queue's
    // length immediately after constructing it; that is redundant (Construct already cleared it)
    // and is omitted.
    void DeformationOutputInterface::Construct()
    {
        mBrokenJointNotificationQueue.Construct();
        mDetachedPartNotificationQueue.Construct();
        mJointedPartStateQueue.Construct();
        mGlassSmashOrCrackQueue.Construct();
        mDetachedPartCurrentPositionQueue.Construct();

        mpDeformationState  = 0;   // X360 +0x70 cleared
        miNumLocatorOutputs = 0;   // X360 +0x2A00 cleared
    }

    // BrnPhysics::Deformation::DeformationOutputInterface::operator=  @ 0x823C8900.
    //
    // Hand-written copy assignment (DWARF BrnDeformationOutputInterface.h:137). Reconstructed
    // store-for-store from BURNOUT_X360_ARTIST.XEX; the sound/effects/world PostPhysics IO buffers
    // call it directly (SetDeformationOutputInterface / SetDeformationInterface).
    //
    // Copies the fixed-count entity-id array + the deformation-state pointer + the locator
    // count/array member-wise, then for each of the five event queues clears the destination length
    // and Append()s all live source events. Append order follows the asm.
    DeformationOutputInterface& DeformationOutputInterface::operator=(const DeformationOutputInterface& lkrOther)
    {
        // maBaseEntityIDs[28] -- EntityId, 4 bytes each (lwzx/stw loop, stride 4).
        for (s32 liIndex = 0; liIndex < 28; ++liIndex)
        {
            maBaseEntityIDs[liIndex] = lkrOther.maBaseEntityIDs[liIndex];
        }

        // miNumLocatorOutputs is hoisted before the loop by the asm; maLocatorData[28] copied 8 bytes/entry.
        miNumLocatorOutputs = lkrOther.miNumLocatorOutputs;
        for (s32 liIndex = 0; liIndex < 28; ++liIndex)
        {
            maLocatorData[liIndex] = lkrOther.maLocatorData[liIndex];
        }

        // mpDeformationState pointer copy (asm dest+0x70 = src+0x70, after the loop).
        mpDeformationState = lkrOther.mpDeformationState;

        // Five event queues: clear destination length, then Append all live source events.
        // Order per asm: DetachedPartNotification, JointedPartState, BrokenJoint, GlassSmash, DetachedPartCurrentPosition.
        mDetachedPartNotificationQueue.Clear();
        mDetachedPartNotificationQueue.Append(lkrOther.mDetachedPartNotificationQueue);

        mJointedPartStateQueue.Clear();
        mJointedPartStateQueue.Append(lkrOther.mJointedPartStateQueue);

        mBrokenJointNotificationQueue.Clear();
        mBrokenJointNotificationQueue.Append(lkrOther.mBrokenJointNotificationQueue);

        mGlassSmashOrCrackQueue.Clear();
        mGlassSmashOrCrackQueue.Append(lkrOther.mGlassSmashOrCrackQueue);

        mDetachedPartCurrentPositionQueue.Clear();
        mDetachedPartCurrentPositionQueue.Append(lkrOther.mDetachedPartCurrentPositionQueue);

        return *this;
    }

    // BrnPhysics::Deformation::DeformationOutputInterfaceForEntityModules::operator=  @ 0x827A96D0.
    //
    // Hand-written copy assignment (DWARF BrnDeformationOutputInterface.h:238). Reconstructed
    // store-for-store from BURNOUT_X360_ARTIST.XEX. The world/traffic PostPhysics IO buffers call
    // it directly (SetDeformationOutputInterfaceForEntityModules: `*lpDest = *lpSource`).
    //
    // The struct interior element types (WheelPhysicalStates is a MINIMAL FLAGGED home; SkinData /
    // VehicleLocatorOutput interiors are not fully homed), so the array region is copied through the
    // asm-authoritative BYTE OFFSETS -- the same un-homed-interior idiom the rest of this TU uses.
    // Every store matches the asm exactly:
    //
    //   +0x0000  muNumEntries                       (dword)
    //   +0x0008  maBaseIDs[28]        stride 8       (VolumeInstanceId, u64)
    //   +0x00F0  maWheelStates[28]    stride 0x190   (WheelPhysicalStates::operator=, 400)
    //   +0x2CB0  miNumSkinnedModels                 (dword)
    //   +0x2CB4  maSkinData[28]       stride 8       (SkinData: EntityId + ptr word)
    //   +0x2D94  miNumLocatorOutputs                (dword)
    //   +0x2D98  maLocatorData[28]    stride 8       (VehicleLocatorOutput)
    //   +0x2E80  mDetachedPartRenderQueue           (clear miLength + Append)
    //   +0x3E30  mGlassSmashOrCrackQueue            (clear miLength + Append)
    DeformationOutputInterfaceForEntityModules&
    DeformationOutputInterfaceForEntityModules::operator=(const DeformationOutputInterfaceForEntityModules& lkrOther)
    {
        char*       lpDest = reinterpret_cast<char*>(this);
        const char* lpSrc  = reinterpret_cast<const char*>(&lkrOther);

        // Byte offsets the X360 spine indexes with (asm-authoritative).
        static const u32 KU_NUM_ENTRIES_OFFSET   = 0x0000;
        static const u32 KU_BASE_IDS_OFFSET      = 0x0008;   // VolumeInstanceId maBaseIDs[28]
        static const u32 KU_WHEEL_STATES_OFFSET  = 0x00F0;   // WheelPhysicalStates maWheelStates[28]
        static const u32 KU_WHEEL_STATES_STRIDE  = 0x0190;   // 400 (WheelPhysicalStates, alignment 16)
        static const u32 KU_NUM_SKINNED_OFFSET   = 0x2CB0;   // miNumSkinnedModels
        static const u32 KU_SKIN_DATA_OFFSET     = 0x2CB4;   // SkinData maSkinData[28]
        static const u32 KU_NUM_LOCATORS_OFFSET  = 0x2D94;   // miNumLocatorOutputs
        static const u32 KU_LOCATOR_DATA_OFFSET  = 0x2D98;   // VehicleLocatorOutput maLocatorData[28]
        static const u32 KU_DETACHED_QUEUE_OFF   = 0x2E80;   // mDetachedPartRenderQueue
        static const u32 KU_GLASS_QUEUE_OFF      = 0x3E30;   // mGlassSmashOrCrackQueue
        static const u32 KU_QUEUE_LENGTH_OFF     = 0x0008;   // BaseEventQueue::miLength within a queue
        static const s32 KI_ENTRY_COUNT          = 28;

        // muNumEntries; then the two array counters the asm hoists before the loop.
        *reinterpret_cast<u32*>(lpDest + KU_NUM_ENTRIES_OFFSET) =
            *reinterpret_cast<const u32*>(lpSrc + KU_NUM_ENTRIES_OFFSET);
        *reinterpret_cast<u32*>(lpDest + KU_NUM_SKINNED_OFFSET) =
            *reinterpret_cast<const u32*>(lpSrc + KU_NUM_SKINNED_OFFSET);
        *reinterpret_cast<u32*>(lpDest + KU_NUM_LOCATORS_OFFSET) =
            *reinterpret_cast<const u32*>(lpSrc + KU_NUM_LOCATORS_OFFSET);

        for (s32 liIndex = 0; liIndex < KI_ENTRY_COUNT; ++liIndex)
        {
            // maBaseIDs[i] -- one 64-bit VolumeInstanceId (ldx/std 8 bytes).
            *reinterpret_cast<u64*>(lpDest + KU_BASE_IDS_OFFSET + 8u * liIndex) =
                *reinterpret_cast<const u64*>(lpSrc + KU_BASE_IDS_OFFSET + 8u * liIndex);

            // maWheelStates[i] -- WheelPhysicalStates::operator= (homed by-name copy).
            WheelPhysicalStates&       lrDestWheel =
                *reinterpret_cast<WheelPhysicalStates*>(lpDest + KU_WHEEL_STATES_OFFSET + KU_WHEEL_STATES_STRIDE * liIndex);
            const WheelPhysicalStates& lkrSrcWheel =
                *reinterpret_cast<const WheelPhysicalStates*>(lpSrc + KU_WHEEL_STATES_OFFSET + KU_WHEEL_STATES_STRIDE * liIndex);
            lrDestWheel = lkrSrcWheel;

            // maSkinData[i] -- 8 bytes (EntityId + scratch ptr word).
            *reinterpret_cast<u64*>(lpDest + KU_SKIN_DATA_OFFSET + 8u * liIndex) =
                *reinterpret_cast<const u64*>(lpSrc + KU_SKIN_DATA_OFFSET + 8u * liIndex);

            // maLocatorData[i] -- 8 bytes.
            *reinterpret_cast<u64*>(lpDest + KU_LOCATOR_DATA_OFFSET + 8u * liIndex) =
                *reinterpret_cast<const u64*>(lpSrc + KU_LOCATOR_DATA_OFFSET + 8u * liIndex);
        }

        // mDetachedPartRenderQueue: clear dest length, then Append all live source events.
        {
            auto& lrDestQueue =
                *reinterpret_cast<CgsModule::EventQueue<DetachedPartRenderEvent, 50>*>(lpDest + KU_DETACHED_QUEUE_OFF);
            const auto& lkrSrcQueue =
                *reinterpret_cast<const CgsModule::EventQueue<DetachedPartRenderEvent, 50>*>(lpSrc + KU_DETACHED_QUEUE_OFF);
            *reinterpret_cast<s32*>(lpDest + KU_DETACHED_QUEUE_OFF + KU_QUEUE_LENGTH_OFF) = 0;  // stw 0, 8(queue)
            lrDestQueue.Append(lkrSrcQueue);
        }

        // mGlassSmashOrCrackQueue: clear dest length, then Append.
        {
            auto& lrDestQueue =
                *reinterpret_cast<CgsModule::EventQueue<GlassSmashOrCrackEvent, 20>*>(lpDest + KU_GLASS_QUEUE_OFF);
            const auto& lkrSrcQueue =
                *reinterpret_cast<const CgsModule::EventQueue<GlassSmashOrCrackEvent, 20>*>(lpSrc + KU_GLASS_QUEUE_OFF);
            *reinterpret_cast<s32*>(lpDest + KU_GLASS_QUEUE_OFF + KU_QUEUE_LENGTH_OFF) = 0;
            lrDestQueue.Append(lkrSrcQueue);
        }

        return *this;
    }
}
}
