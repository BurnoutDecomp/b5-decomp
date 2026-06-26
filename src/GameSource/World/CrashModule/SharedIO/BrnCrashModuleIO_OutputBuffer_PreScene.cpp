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
        static_assert(offsetof(OutputBuffer_PreScene, mTrafficOutputInterface) == 0x8,
                      "mTrafficOutputInterface @0x8");
        static_assert(offsetof(OutputBuffer_PreScene, mVehicleInputInterface) == 0x670,
                      "mVehicleInputInterface @0x670");
        static_assert(offsetof(OutputBuffer_PreScene, mRaceCarOutputInterface) == 0x231D0,
                      "mRaceCarOutputInterface @0x231D0");
    }

    // +0x8 -- traffic interface.
    const OutputBuffer_PreScene::TrafficOutputInterfaceStorage*
    OutputBuffer_PreScene::GetTrafficOutputInterface() const   // 0x827A23E0 read-lock
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mTrafficOutputInterface;
    }

    OutputBuffer_PreScene::TrafficOutputInterfaceStorage*
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
    const OutputBuffer_PreScene::RaceCarOutputInterfaceStorage*
    OutputBuffer_PreScene::GetRaceCarOutputInterface() const   // 0x827A2530 read-lock
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mRaceCarOutputInterface;
    }

    OutputBuffer_PreScene::RaceCarOutputInterfaceStorage*
    OutputBuffer_PreScene::GetRaceCarOutputInterface()         // 0x827BB720 write-lock
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mRaceCarOutputInterface;
    }
}
}
