// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/RaceCarEntityModule/
//   BrnRaceCarEntityModuleIO_PreSceneAccessors.cpp
//
// Out-of-line bodies for the two OutputBuffer_PreScene accessors the X360 build
// emitted out-of-line and that the catch-all BrnRaceCarEntityModuleIO.cpp does NOT
// already define (it homes only the const GetRaceCarAIInterface(), DWARF :300):
//
//   * X360 0x822B52C0 -- OutputBuffer_PreScene::GetRaceCarAIInterface()  (NON-const,
//     DWARF :301). IDA mislabels the truncated mangled name as "...::G". Write-lock
//     accessor: status>>3 &1 (eStatusLockedForWrite) => IsBufferLockedForWriting(),
//     non-const, returns &mRaceCarAIInterface. The X360 epilogue spells the member as
//     this + 0xF0E80 (986752); reproduced here layout-derived as &member, NOT a raw
//     offset cast. Assert source line 301 ("Not locked for writing").
//
//   * X360 0x8279D7A0 -- OutputBuffer_PreScene::IsRequestingRivalUpdate() const
//     (DWARF :303). Read-lock accessor: status>>4 &1 (eStatusLockedForRead) =>
//     IsBufferLockedForReading(), const, returns mbRequestingRivalUpdate (X360 reads
//     the byte at this + 0xF5250 (1004112) via lbzx). Assert source line 303
//     ("Not locked for reading").
//
// CGS_ASSERT stamps __FILE__/__LINE__, so the X360-baked d:\p4 path/line are
// intentionally not reproduced.
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnWorld
{
namespace RaceCarEntityModuleIO
{

// X360 0x822B52C0 (W, :301) -- mutable race-car AI interface accessor (IDA "G").
// Pairs with the const overload at the identical +986752 offset (homed in the
// catch-all BrnRaceCarEntityModuleIO.cpp, X360 0x8279D6F8, :300).
OutputBuffer_PreScene::RaceCarAIInterface*
OutputBuffer_PreScene::GetRaceCarAIInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mRaceCarAIInterface;
}

// X360 0x8279D7A0 (R, :303) -- const "is the producer requesting a rival update?"
// flag read (mbRequestingRivalUpdate; X360 byte read at +1004112). The matching
// setter SetRequestingRivalUpdate(bool) (:304) is a separate (inlined) TU.
bool
OutputBuffer_PreScene::IsRequestingRivalUpdate() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mbRequestingRivalUpdate;
}

}
}
