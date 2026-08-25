#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleTrafficIOInterfaces.h"

// BrnWorld::CrashIO::TrafficInputInterface
// Reconstructed from BURNOUT_X360_ARTIST.XEX (Construct @ 0x827609F0, operator= @ 0x827AA4E8).
// The crash module's per-frame traffic input view: three EventQueue<...,160> input queues plus
// three FastBitArray<601> per-traffic-vehicle bit masks. The EventQueue<T,160>::Construct and
// BaseEventQueue<T>::Clear/Append generics are committed inline (CgsEventQueue.h /
// CgsBaseEventQueue.h); the FastBitArray<601> ctor/copy is committed inline (CgsFastBitArray.h).
// This TU only wires those together for the interface aggregate.

namespace BrnWorld
{
namespace CrashIO
{

// Construct @ 0x827609F0.
//   Calls EventQueue<AddCrashingTrafficEvent,160>::Construct(this+0),
//   EventQueue<RemoveSlammedTrafficEvent,160>::Construct(this+0xA10),
//   EventQueue<RemoveCrashedTrafficEvent,160>::Construct(this+0xB5C) -- each points its base
//   queue at the inline maEvents buffer, sets miMaxLength = 160, miLength = 0.
//   Then zeroes the three FastBitArray<601> masks: the X360 emits 30 consecutive `std r11(0)`
//   from 0xCA8 to 0xD90 (3 masks * 10 u64 fields == 240 bytes), i.e. each mask's Construct().
void TrafficInputInterface::Construct()
{
    mAddCrashingTrafficEventQueue.Construct();
    mRemoveSlammedTrafficEventQueue.Construct();
    mRemoveCrashedTrafficEventQueue.Construct();

    mRenderingBits.Construct();
    mFarFromCameraBits.Construct();
    mPhysicalBits.Construct();
}

// operator= @ 0x827AA4E8.
//   For each of the three input queues: clear the destination queue's miLength (the X360
//   `stw 0, 8(queue)` before each Append) then Append every live event from the matching
//   source queue (BaseEventQueue<T>::Append @0x827A82D0/0x827A83B0 etc.). The destination is
//   assumed already Constructed (no buffer re-pointing). Then copy the three FastBitArray<601>
//   masks: the X360 emits three 10-qword `ld/std` copy loops starting at 0xCA8/0xCF8/0xD48,
//   i.e. each mask's memberwise (u64[10]) copy-assignment. Returns *this.
TrafficInputInterface& TrafficInputInterface::operator=(const TrafficInputInterface& lOther)
{
    mAddCrashingTrafficEventQueue.Clear();
    mAddCrashingTrafficEventQueue.Append(lOther.mAddCrashingTrafficEventQueue);

    mRemoveSlammedTrafficEventQueue.Clear();
    mRemoveSlammedTrafficEventQueue.Append(lOther.mRemoveSlammedTrafficEventQueue);

    mRemoveCrashedTrafficEventQueue.Clear();
    mRemoveCrashedTrafficEventQueue.Append(lOther.mRemoveCrashedTrafficEventQueue);

    mRenderingBits     = lOther.mRenderingBits;
    mFarFromCameraBits = lOther.mFarFromCameraBits;
    mPhysicalBits      = lOther.mPhysicalBits;

    return *this;
}


// ====================================================================================
// TrafficOutputInterface::Construct   [crash exit 2026-08-25]
//
// Declared alongside its sibling since this group landed and never defined -- nothing had
// ever constructed a CrashIO::OutputBuffer_PreScene, because that buffer had no Construct
// either (see BrnCrashModuleIO_OutputBuffer_PreScene.cpp).
//
// ⚠️ THERE IS NO OUT-OF-LINE X360 SYMBOL FOR THIS ONE. The console emits it INLINED into
// CrashIO::OutputBuffer_PreScene::Construct @0x827CE9E8, as two queue Constructs over the
// interface's own seats:
//   0x827CE9F8  CleanupTrafficEvent_160_::Construct(buffer + 8)         -> +0x000
//   0x827CEA04  NetworkTrafficCrashingEvent_160_::Construct(buffer+332) -> +0x510 - 8 + 8
// which is exactly "construct both member queues", the same shape TrafficInputInterface::
// Construct @0x827609F0 has for its three. Homed here beside that sibling.
// ====================================================================================
void TrafficOutputInterface::Construct()
{
    mCleanupTrafficEventQueue.Construct();
    mStartCrashingNetworkTrafficQueue.Construct();
}

}
}
