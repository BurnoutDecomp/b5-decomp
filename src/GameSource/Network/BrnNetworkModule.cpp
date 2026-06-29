// ============================================================================
// b5-decomp/src/GameSource/Network/BrnNetworkModule.cpp
// ============================================================================
// BrnNetwork::BrnNetworkModule -- constructor + the lock-guarded sub-object accessors.
//
// Source-of-truth: X360 ARTIST build (BURNOUT_X360_ARTIST.XEX). Function addresses:
//   0x827E5E10 BrnNetworkModule::BrnNetworkModule (ctor; EXECUTED in the boot-trace milestone)
//   0x82581700 GetVehicleDriverInputInterface         (this + 614704)
//   0x82542818 GetTrafficInputInterface               (this + 800320)
//   0x825428C0 GetTrafficOutputInterface (const)       (this + 800432)
//   0x82542968 GetCrashInputInterface                 (this + 800576)
//   0x82542A10 GetCrashOutputInterface (const)         (this + 816080)
//   0x82542C00 GetNetworkToGameStateInterface         (this + 818608)
//   0x82542CA8 GetGameStateToNetworkInterface         (this + 818064)
//   0x825817A8 GetPlayerVehicleControls               (this + 827880)
//   0x825426C8 GetTakedownEventInputQueue (const)      (this + 829496)
//   0x82581850 GetInputGuiEventQueue                  (this + 829832)
//   0x82542D50 GetNetworkEventQueue                   (this + 852392)
//   0x825818F8 IsSendUpdateMessageToBeShown (const)    (lbz this + 614664)
//   0x825819A0 IsRecvUpdateMessageToBeShown (const)    (lbz this + 614665)
//
// Every accessor guards on mbIsUpdating (X360 lbz this+0x228; "Can not use this function unless
// module is updating") then returns the address of its embedded sub-object. The sub-object member
// types live in other, not-yet-reconstructed modules and are modelled as offset-pinned opaque
// storage in the header; the accessors hand back a pointer typed as the (incomplete) interface,
// matching the committed BrnNetworkModuleIO.cpp pattern.

#include "GameSource/Network/BrnNetworkModule.h"

namespace BrnNetwork
{
    // X360 0x827E5E10. Stores the vtable, constructs the two base RWMutexes (+0x10/+0x118) and the
    // embedded BrnNetworkManager (+0x280), then zero-inits a set of scalar fields and stores -1 into
    // the +790352 field. The base/vtable/mutex construction is part of the opaque base region on
    // this minimal slice (no formal base here); value-initialising the modelled storage covers every
    // zero/0.0f default the ctor asm writes, and the single grounded non-zero default is applied
    // explicitly.
    BrnNetworkModule::BrnNetworkModule()
        : maPadToIsUpdating{}
        , mbIsUpdating(false)
        , maPadToManager{}
        , mNetworkManagerStorage{}
        , mbOutputSendUpdateMessages(false)     // X360 stbx 0 @ +614664
        , mbOutputRecvUpdateMessages(false)     // X360 +614665
        , mPadToVehicleDriver{}
        , mVehicleDriverInputInterfaceStorage{}
        , mField790352(-1)                       // X360 li r6,-1 ; stw r6,0x200(r8)  @ +790352
        , mVehicleDriverInputInterfaceStorageTail{}
        , mTrafficNetworkInputInterfaceStorage{}
        , mTrafficNetworkOutputInterfaceStorage{}
        , mCrashNetworkInputInterfaceStorage{}
        , mCrashNetworkOutputInterfaceStorage{}
        , mGameStateToNetworkInterfaceStorage{}
        , mNetworkToGameStateInterfaceStorage{}
        , mPlayerVehicleControlsStorage{}
        , mTakedownEventInputQueueStorage{}
        , mInputGuiEventQueueStorage{}
        , mNetworkEventQueueStorage{}
        , mGameEventQueueStorage{}
        , mOutputGuiEventQueueStorage{}
    {
    }

    // X360 0x82581700 -> &mVehicleDriverInputInterface @ +614704.
    BrnPhysics::Vehicle::VehicleDriverInputInterface* BrnNetworkModule::GetVehicleDriverInputInterface()
    {
        CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
        return reinterpret_cast<BrnPhysics::Vehicle::VehicleDriverInputInterface*>(
            &mVehicleDriverInputInterfaceStorage);
    }

    // X360 0x82542818 -> &mTrafficNetworkInputInterface @ +800320.
    BrnTraffic::BrnTrafficIO::TrafficNetworkInputInterface* BrnNetworkModule::GetTrafficInputInterface()
    {
        CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
        return reinterpret_cast<BrnTraffic::BrnTrafficIO::TrafficNetworkInputInterface*>(
            &mTrafficNetworkInputInterfaceStorage);
    }

    // X360 0x825428C0 -> &mTrafficNetworkOutputInterface @ +800432.
    const BrnTraffic::BrnTrafficIO::TrafficNetworkOutputInterface* BrnNetworkModule::GetTrafficOutputInterface() const
    {
        CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
        return reinterpret_cast<const BrnTraffic::BrnTrafficIO::TrafficNetworkOutputInterface*>(
            &mTrafficNetworkOutputInterfaceStorage);
    }

    // X360 0x82542968 -> &mCrashNetworkInputInterface @ +800576.
    BrnWorld::CrashIO::NetworkInputInterface* BrnNetworkModule::GetCrashInputInterface()
    {
        CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
        return reinterpret_cast<BrnWorld::CrashIO::NetworkInputInterface*>(
            &mCrashNetworkInputInterfaceStorage);
    }

    // X360 0x82542A10 -> &mCrashNetworkOutputInterface @ +816080.
    const BrnWorld::CrashIO::NetworkOutputInterface* BrnNetworkModule::GetCrashOutputInterface() const
    {
        CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
        return reinterpret_cast<const BrnWorld::CrashIO::NetworkOutputInterface*>(
            &mCrashNetworkOutputInterfaceStorage);
    }

    // X360 0x82542CA8 -> &mGameStateToNetworkInterface @ +818064.
    BrnNetworkModuleIO::GameStateToNetworkInterface* BrnNetworkModule::GetGameStateToNetworkInterface()
    {
        CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
        return reinterpret_cast<BrnNetworkModuleIO::GameStateToNetworkInterface*>(
            &mGameStateToNetworkInterfaceStorage);
    }

    // X360 0x82542C00 -> &mNetworkToGameStateInterface @ +818608.
    BrnNetworkModuleIO::NetworkToGameStateInterface* BrnNetworkModule::GetNetworkToGameStateInterface()
    {
        CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
        return reinterpret_cast<BrnNetworkModuleIO::NetworkToGameStateInterface*>(
            &mNetworkToGameStateInterfaceStorage);
    }

    // X360 0x825817A8 -> &mPlayerVehicleControls @ +827880.
    BrnWorld::PlayerVehicleControls* BrnNetworkModule::GetPlayerVehicleControls()
    {
        CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
        return reinterpret_cast<BrnWorld::PlayerVehicleControls*>(&mPlayerVehicleControlsStorage);
    }

    // X360 0x825426C8 -> &mTakedownEventInputQueue @ +829496.
    const BrnNetworkModuleIO::InputBuffer::TakedownEventQueue* BrnNetworkModule::GetTakedownEventInputQueue() const
    {
        CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
        return reinterpret_cast<const BrnNetworkModuleIO::InputBuffer::TakedownEventQueue*>(
            &mTakedownEventInputQueueStorage);
    }

    // X360 0x82581850 -> &mInputGuiEventQueue @ +829832.
    BrnNetworkModuleIO::InputBuffer::GuiEventQueue* BrnNetworkModule::GetInputGuiEventQueue()
    {
        CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
        return reinterpret_cast<BrnNetworkModuleIO::InputBuffer::GuiEventQueue*>(
            &mInputGuiEventQueueStorage);
    }

    // X360 0x82542D50 -> &mNetworkEventQueue @ +852392.
    BrnNetworkModuleIO::NetworkEventQueue* BrnNetworkModule::GetNetworkEventQueue()
    {
        CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
        return reinterpret_cast<BrnNetworkModuleIO::NetworkEventQueue*>(&mNetworkEventQueueStorage);
    }

    // X360 0x825818F8 -> lbz this+614664 (mbOutputSendUpdateMessages).
    bool BrnNetworkModule::IsSendUpdateMessageToBeShown() const
    {
        CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
        return mbOutputSendUpdateMessages;
    }

    // X360 0x825819A0 -> lbz this+614665 (mbOutputRecvUpdateMessages).
    bool BrnNetworkModule::IsRecvUpdateMessageToBeShown() const
    {
        CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
        return mbOutputRecvUpdateMessages;
    }
}
