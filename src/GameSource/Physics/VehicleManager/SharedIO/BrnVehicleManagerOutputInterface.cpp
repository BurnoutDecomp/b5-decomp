#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h" // VehicleManagerOutputInterface + TrafficCrashedEvent (via BrnVehicleEvents.h)
#include "GameShared/GameClasses/Core/CgsAssert.h"                                // CgsDev::Assert Begin/Fire/End + KI_MESSAGEBUFFERSIZE
#include "GameShared/GameClasses/Development/CgsStrStream.h"                       // CgsDev::StrStream (dynamic assert message)

namespace BrnPhysics
{
namespace Vehicle
{
    // @0x825C0658  VehicleManagerOutputInterface::AddCrashedTrafficEvent
    //   Queue a "physical-traffic vehicle crashed" event and return its slot index. The
    //   VolumeInstanceId's embedded entity word (high 32 bits) is the crashing vehicle's own entity
    //   id; lCrasherEntityID is the "other" party. If they are equal the vehicle crashed into
    //   itself -- a diagnostic-only dev-assert (non-gating: the event is queued either way). The
    //   event is always appended to the FIRST member, mCrashedTrafficEventQueue (offset 0), and the
    //   freshly-added slot index (miLength - 1) is returned.
    //
    //   The X360 builds the diagnostic with a CgsDev::StrStream and passes it to FireAssert --
    //   reproduced here per the committed streamed-assert precedent (CgsID.cpp), which keeps the
    //   file/line args for dynamically-built messages rather than collapsing to CGS_ASSERT.
    s32 VehicleManagerOutputInterface::AddCrashedTrafficEvent(VolumeInstanceId lVolumeInstanceID,
                                                              EntityId         lCrasherEntityID)
    {
        // srdi/cmplw: compare hi32(volumeInstanceId) against crasherEntityID.
        const u32 luTrafficEntityWord = static_cast<u32>(lVolumeInstanceID.muId >> 32);
        if (luTrafficEntityWord == lCrasherEntityID.muValue)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStream << "Traffic entity " << static_cast<s32>(lVolumeInstanceID.muId)
                    << " crashed into itself. Other=" << static_cast<s32>(lCrasherEntityID.muValue);
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessageBuffer,
                                       "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\physics\\vehiclemanager\\SharedIO/BrnVehicleOutputInterface.h",
                                       445);
            CgsDev::Assert::EndAssert();
        }

        // Always append the event.
        TrafficCrashedEvent lEvent;
        lEvent.mTrafficVolumeInstanceID = lVolumeInstanceID;
        lEvent.mCrasherEntityID         = lCrasherEntityID;
        mCrashedTrafficEventQueue.AddEvent(lEvent);

        // return miLength - 1 (index of the just-added event).
        return mCrashedTrafficEventQueue.GetLength() - 1;
    }

    // @0x825C0758 (25 insns) VehicleManagerOutputInterface::AddTrafficRemovedEvent
    // This function was ABSENT from
    //   .ida-exports/BURNOUT_X360_ARTIST.XEX (no per-function JSON); the body below is a direct
    //   transcription of a headless-idat dump taken for this wave, not an inference. DWARF
    //   BrnVehicleOutputInterface.h:217.
    //
    //   Three asserts in order, then one append. The queue seat is the console's +0x7A0 == 1952
    //   (`addi r3, r29, 0x7A0` @0x825C08B4), reached BY NAME here; the length/max reads at +0x7A8
    //   and +0x7A4 are the same member's miLength/miMaxLength.
    // The FIRST assert is the console's own capacity tripwire and it is NOT gating -- the
    //   append happens either way, exactly as shipped (25 slots, one frame).
    s32 VehicleManagerOutputInterface::AddTrafficRemovedEvent(EntityId     lRemovedVehicleEntityId,
                                                              ETrafficType leTrafficType)
    {
        if (mRemovedTrafficEventQueue.GetLength() >= mRemovedTrafficEventQueue.GetMaxLength())
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStream << "Trying to recycle more than " << mRemovedTrafficEventQueue.GetMaxLength()
                    << " traffic vehicles in a single frame";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessageBuffer,
                                       "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\physics\\vehiclemanager\\SharedIO/BrnVehicleOutputInterface.h",
                                       481);
            CgsDev::Assert::EndAssert();
        }

        // `srwi r11, r22, 24 ; cmplwi r11, 2` -- the owner byte of the 32-bit entity word.
        if ((lRemovedVehicleEntityId.muValue >> 24) != KU_ENTITYTYPE_TRAFFIC_VEHICLE)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStream << "Trying to remove non-traffic vehicle in AddTrafficRemovedEvent()";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessageBuffer,
                                       "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\physics\\vehiclemanager\\SharedIO/BrnVehicleOutputInterface.h",
                                       482);
            CgsDev::Assert::EndAssert();
        }

        CGS_ASSERT(leTrafficType < E_TRAFFIC_TYPE_COUNT, "leTrafficType < E_TRAFFIC_TYPE_COUNT"); // :484

        TrafficRemovedEvent lEvent;
        lEvent.mRemovedVehicleEntityId = lRemovedVehicleEntityId;
        lEvent.meTrafficType           = leTrafficType;
        mRemovedTrafficEventQueue.AddEvent(lEvent);

        return mRemovedTrafficEventQueue.GetLength() - 1;
    }

    // @0x827A9B20  VehicleManagerOutputInterface::operator=
    //   (dossier 'VehicleManagerOutputInt' is a truncated name.) For each of the seven
    //   EventQueue<T,N> members: reset the live count (X360 `*(this+offset) = 0`, the same
    //   store-8-zero BaseEventQueue<T>::Clear() does -- this+offset lands exactly on each queue's
    //   miLength field) then Append() the matching member from lOther onto it, i.e. "become a copy
    //   of lOther's live events" rather than a raw memberwise copy (which would also duplicate the
    //   per-instance mpEvents/miMaxLength backing-buffer bookkeeping the X360 body deliberately
    //   leaves alone -- matching the committed VehicleOutputInterface::operator= precedent above).
    //   The trailing VehicleGuiOutputMessages (@0x79C, 3 bools) and WheelFFSpring (@0x874, 2 floats)
    //   are plain block copies (not queues), reconstructed as named-member struct assignment (both
    //   are trivial PODs, so `=` reproduces the X360's byte/word copies exactly). Returns *this.
    //   Called by *::InputBuffer_PostPhysics::SetVehicleManagerOutputInterface and
    //   WorldModule::BridgePhysicsToOutput.
    VehicleManagerOutputInterface&
    VehicleManagerOutputInterface::operator=(const VehicleManagerOutputInterface& lOther)
    {
        mCrashedTrafficEventQueue.Clear();                                   // @+0x000
        mCrashedTrafficEventQueue.Append(lOther.mCrashedTrafficEventQueue);

        mSlammedTrafficEventQueue.Clear();                                   // @+0x150
        mSlammedTrafficEventQueue.Append(lOther.mSlammedTrafficEventQueue);

        mFineTrafficCrashedEventQueue.Clear();                               // @+0x2F0
        mFineTrafficCrashedEventQueue.Append(lOther.mFineTrafficCrashedEventQueue);

        mRaceCarCrashEventQueue.Clear();                                     // @+0x3A0
        mRaceCarCrashEventQueue.Append(lOther.mRaceCarCrashEventQueue);

        mRaceCarResetEventQueue.Clear();                                     // @+0x5B0
        mRaceCarResetEventQueue.Append(lOther.mRaceCarResetEventQueue);

        mCreateVehicleResultQueue.Clear();                                   // @+0x6C0
        mCreateVehicleResultQueue.Append(lOther.mCreateVehicleResultQueue);

        mTrafficTypeRequestQueue.Clear();                                    // @+0x750
        mTrafficTypeRequestQueue.Append(lOther.mTrafficTypeRequestQueue);

        mVehicleGuiOutputMessages = lOther.mVehicleGuiOutputMessages;        // @+0x79C (3 bools)

        mRemovedTrafficEventQueue.Clear();                                   // @+0x7A0
        mRemovedTrafficEventQueue.Append(lOther.mRemovedTrafficEventQueue);

        mWheelFFSpring = lOther.mWheelFFSpring;                              // @+0x874 (2 floats)

        return *this;
    }
}
}
