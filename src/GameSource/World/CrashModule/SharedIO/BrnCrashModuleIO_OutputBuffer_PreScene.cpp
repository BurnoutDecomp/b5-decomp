#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleIO.h"

#include <cstddef>   // offsetof

// BrnWorld::CrashIO::OutputBuffer_PreScene members, reconstructed from
// BURNOUT_X360_ARTIST.XEX. The pre-scene output buffer exposes three embedded interface
// members, each via a const (read-lock) and a non-const (write-lock) getter that return the
// same member address:
//   mTrafficOutputInterface  @ +0x8      read 0x827A23E0 (line 130) / write 0x827BB5D0 (line 131)
//   mVehicleInputInterface  @ +0x670    read 0x827A2488 (line 133) / write 0x827BB678 (line 134)
//   mRaceCarOutputInterface  @ +0x231D0  read 0x827A2530 (line 136) / write 0x827BB720 (line 137)
//
// Each const getter tests the read-lock bit (`lbz r11,0(this); extrwi r11,r11,1,27` == bit 4 ==
// IsBufferLockedForReading()) and on failure streams "Not locked for reading\n"; each non-const
// getter tests the write-lock bit (`extrwi r11,r11,1,28` == bit 3 == IsBufferLockedForWriting())
// and streams "Not locked for writing\n". Each returns `this + offset` (the +0x231D0 offset is
// computed by the asm as `addis r3,this,2; addi r3,r3,0x31D0`). The streamed-message asserts map
// to the house CGS_ASSERT (the trailing "\n" is dropped from the stringized condition).

namespace BrnWorld
{
namespace CrashIO
{
    void OutputBuffer_PreScene::_AssertLayout()
    {
        // ⚠️ [crash exit 2026-08-25] The three console OFFSET asserts that stood here are gone,
        // and deliberately. They only held while all three members were 1-byte opaque storage
        // padded into position; the two committed interfaces are align-16 aggregates full of
        // EventQueues that WIDEN on the host, so 0x8 / 0x670 / 0x231D0 are console facts a
        // 64-bit host cannot reproduce. Asserting them would force the fake padding back in --
        // and that padding is exactly what kept this buffer opaque and the crash-exit path
        // blocked. The ORDER is what is load-bearing: the bridges take member ADDRESSES from the
        // accessors below, never a literal offset. Same trade InputBuffer_PostPhysics already
        // makes; its members carry the same "align-16, widens on host" note.
        static_assert(offsetof(OutputBuffer_PreScene, mTrafficOutputInterface)
                        < offsetof(OutputBuffer_PreScene, mVehicleInputInterface),
                      "DWARF order: traffic (X360 +0x8) precedes vehicle (X360 +0x670)");
        static_assert(offsetof(OutputBuffer_PreScene, mVehicleInputInterface)
                        < offsetof(OutputBuffer_PreScene, mRaceCarOutputInterface),
                      "DWARF order: vehicle (X360 +0x670) precedes race car (X360 +0x231D0)");
    }

    // +0x8 -- traffic interface.
    const TrafficOutputInterface*
    OutputBuffer_PreScene::GetTrafficOutputInterface() const   // 0x827A23E0 read-lock
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mTrafficOutputInterface;
    }

    TrafficOutputInterface*
    OutputBuffer_PreScene::GetTrafficOutputInterface()         // 0x827BB5D0 write-lock
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mTrafficOutputInterface;
    }

    // +0x670 -- vehicle interface.
    const OutputBuffer_PreScene::VehicleInputInterfaceStorage*
    OutputBuffer_PreScene::GetVehicleInputInterface() const   // 0x827A2488 read-lock
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mVehicleInputInterface;
    }

    OutputBuffer_PreScene::VehicleInputInterfaceStorage*
    OutputBuffer_PreScene::GetVehicleInputInterface()         // 0x827BB678 write-lock
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mVehicleInputInterface;
    }

    // +0x231D0 -- race-car interface.
    const RaceCarOutputInterface*
    OutputBuffer_PreScene::GetRaceCarOutputInterface() const   // 0x827A2530 read-lock
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mRaceCarOutputInterface;
    }

    RaceCarOutputInterface*
    OutputBuffer_PreScene::GetRaceCarOutputInterface()         // 0x827BB720 write-lock
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mRaceCarOutputInterface;
    }

    // ====================================================================================
    // OutputBuffer_PreScene::Construct   X360 0x827CE9E8
    //
    //   *this = 1                                                -- IOBuffer::Construct
    //   CleanupTrafficEvent_160_::Construct(this + 8)         \__ == TrafficOutputInterface
    //   NetworkTrafficCrashingEvent_160_::Construct(this + 332) /   ::Construct over +0x8
    //   VehicleInputInterface::Construct(this + 1648)            -- mVehicleInputInterface
    //   RaceCarCrashCompleteEvent_10_::Construct(this + 143824)  -- mRaceCarOutputInterface's
    //                                                               queue; 143824 == 0x231D0
    //   *(this + 143826) = 0                                     -- a trailing byte in the same
    //                                                               interface (2 past the queue
    //                                                               base; not modelled)
    //
    // ⭐ THE +0x231D0 CONSTRUCT IS THE ONE THAT MATTERS: it is the crash-complete ring that
    // ResetRaceCarFromCrashIndex AddEvent()s into and that
    // RaceCarEntityModule::ProcessRaceCarCrashCompleteEvents drains. 143824 is also, exactly, the
    // number the previous wave's "OUT OF BOUNDS @143824" measurement was reporting against the
    // phantom OutputBuffer_PostScene -- it was this member's own offset all along.
    //
    // ⛔ mVehicleInputInterface IS NOT CONSTRUCTED HERE, and that is not an omission: this tree
    // still models it as `VehicleInputInterfaceStorage { unsigned char maBytes[1] }` (see the type
    // banner). There is nothing to construct until it is promoted to the real type, and nothing
    // in the reconstructed crash module writes it. [FLAG] DELETE-WHEN it is promoted.
    // ====================================================================================
    void OutputBuffer_PreScene::Construct()
    {
        CgsModule::IOBuffer::Construct();

        mTrafficOutputInterface.Construct();
        mRaceCarOutputInterface.GetRaceCarCrashCompleteEventQueue()->Construct();
    }

}
}
