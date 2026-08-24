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
    // ⭐⭐ REWRITTEN BY NAME 2026-08-24 (deform-land wave). The previous body copied through the
    // CONSOLE byte offsets (+0x2CB4 skin data stride 8, +0x2D98 locators stride 8, queue length
    // at queue+8) via reinterpret_cast on `this` -- but the HOST layout diverges from the console
    // at exactly those seats: SkinData/VehicleLocatorOutput carry widened 8-byte pointers
    // (16-byte host stride, different array bases) and BaseEventQueue's miLength sits at host +12
    // (mpEvents is 8 bytes), so the old `*(queue + 8) = 0` was zeroing miMaxLength. Latent only
    // because the bridge legs that invoke this copy were parked; they landed this wave. Console
    // store order (asm @0x827A96D0) preserved:
    //   muNumEntries; miNumSkinnedModels; miNumLocatorOutputs (hoisted);
    //   per entry: maBaseIDs[i] (8B), maWheelStates[i] (WheelPhysicalStates::operator=),
    //              maSkinData[i], maLocatorData[i];
    //   mDetachedPartRenderQueue: clear length + Append; mGlassSmashOrCrackQueue: same.
    DeformationOutputInterfaceForEntityModules&
    DeformationOutputInterfaceForEntityModules::operator=(const DeformationOutputInterfaceForEntityModules& lkrOther)
    {
        static const s32 KI_ENTRY_COUNT = 28;

        muNumEntries        = lkrOther.muNumEntries;
        miNumSkinnedModels  = lkrOther.miNumSkinnedModels;
        miNumLocatorOutputs = lkrOther.miNumLocatorOutputs;

        for (s32 liIndex = 0; liIndex < KI_ENTRY_COUNT; ++liIndex)
        {
            // maBaseIDs[i] -- one 64-bit VolumeInstanceId (ldx/std 8 bytes).
            maBaseIDs[liIndex] = lkrOther.maBaseIDs[liIndex];

            // maWheelStates[i] -- the 400-byte slot viewed as the homed WheelPhysicalStates to
            // invoke its by-name operator= (the slot idiom the member banner documents).
            *reinterpret_cast<WheelPhysicalStates*>(&maWheelStates[liIndex]) =
                *reinterpret_cast<const WheelPhysicalStates*>(&lkrOther.maWheelStates[liIndex]);

            // maSkinData[i] / maLocatorData[i] -- one 8-byte unit each on console; member-wise
            // struct copies here (the pointers are 8 bytes on the host).
            maSkinData[liIndex]    = lkrOther.maSkinData[liIndex];
            maLocatorData[liIndex] = lkrOther.maLocatorData[liIndex];
        }

        // Queues: clear destination length (`stw 0, 8(queue)` on console == Clear()), then Append.
        mDetachedPartRenderQueue.Clear();
        mDetachedPartRenderQueue.Append(lkrOther.mDetachedPartRenderQueue);

        mGlassSmashOrCrackQueue.Clear();
        mGlassSmashOrCrackQueue.Append(lkrOther.mGlassSmashOrCrackQueue);

        return *this;
    }

    // DeformationOutputInterfaceForEntityModules::Construct -- the console has NO standalone
    // symbol for it: PhysicsModuleIO::OutputBuffer::Construct @0x825ABB10 constructs this
    // member's two queues IN LINE (EventQueue<DetachedPartRenderEvent,50>::Construct at
    // buffer+171552 == this interface's +0x2E80 seat, EventQueue<GlassSmashOrCrackEvent,20>::
    // Construct at buffer+175568 == +0x3E30). The three counters live in BSS-zeroed storage on
    // console; cleared explicitly here because the host object can sit on reused heap.
    void DeformationOutputInterfaceForEntityModules::Construct()
    {
        muNumEntries        = 0;
        miNumSkinnedModels  = 0;
        miNumLocatorOutputs = 0;
        mDetachedPartRenderQueue.Construct();
        mGlassSmashOrCrackQueue.Construct();
    }
}
}
