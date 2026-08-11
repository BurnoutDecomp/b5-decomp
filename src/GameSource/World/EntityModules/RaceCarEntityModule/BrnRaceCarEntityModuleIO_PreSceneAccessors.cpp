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

// X360 0x8279D500 (R, :288) -- const active-race-car output-interface accessor of
// OutputBuffer_PreScene. Read-lock ((status>>4)&1, eStatusLockedForRead) =>
// IsBufferLockedForReading(). X360 epilogue == this + 0xEA9C0 (960960); reproduced
// by-name as &mActiveRaceCarOutputInterface. Pairs with the non-const overload (:289,
// X360 0x822B5020) at the identical member offset (in the catch-all IO.cpp).
const RCEntityActiveRaceCarOutputInterface*
OutputBuffer_PreScene::GetActiveRaceCarOutputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mActiveRaceCarOutputInterface;
}

// X360 0x8279D3B0 (R, :282) -- const vehicle-input-interface accessor of
// OutputBuffer_PreScene. Read-lock ((status>>4)&1, eStatusLockedForRead) =>
// IsBufferLockedForReading(). X360 epilogue == `addi r3, r28, 0x10` (this+16);
// reproduced by-name as &mVehicleInputInterface. Pairs with the non-const overload
// (:283, X360 0x822B4ED0) at the identical member offset (in the catch-all IO.cpp).
// The DecFIGS PS3 build inlines this accessor into
// WorldModule::BridgeEntityModulesToPhysicsModule_PreScene and its baked assert cites
// BrnRaceCarEntityModuleIO.h:282 -- the same line the declaration carries here.
// SOLE caller: that bridge (X360 0x827AADB8), which merges this interface into the
// physics module input buffer's own VehicleInputInterface.
const OutputBuffer_PreScene::VehicleInputInterface*
OutputBuffer_PreScene::GetVehicleInputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mVehicleInputInterface;
}

// X360 0x822B5950 (R, DWARF :424 / X360 baked line 433) -- const scoring-output interface
// accessor of InputBuffer_PrePhysics. Read-lock ((status>>4)&1, eStatusLockedForRead) =>
// IsBufferLockedForReading(). X360 epilogue == this + 0x33180 (209312); reproduced
// by-name as &mScoringInterface. Caller: BrnWorld::PlaceOnTrackManager::PrePhysicsUpdate.
// ⚠️ ADDRESS CORRECTED 2026-08-11: this body previously cited 0x822B5758/+163872. That
// address is the CONST GetSceneResultQueue (X360 line 424, returns +163872 ==
// mSceneResultQueue) -- the PS3-DWARF-line-vs-X360-baked-line skew this header warns about
// (+9 for this buffer) had slid the whole read-lock run one slot. The body itself was and
// is correct (it returns &member by name); only the citation was wrong. Proven: 0x822B58A8
// (line 430) -> +208976 mTakedownEventQueue, 0x822B5950 (line 433) -> +209312
// mScoringInterface, 0x822B59F8 (line 436) -> +212048 mOnlineScoringInterface, 0x822B5AA0
// (line 442) -> byte 212213 mbControllerActive.
const InputBuffer_PrePhysics::ScoringInterface*
InputBuffer_PrePhysics::GetScoringInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mScoringInterface;
}

}
}
