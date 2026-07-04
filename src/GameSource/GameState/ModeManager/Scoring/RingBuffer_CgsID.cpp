#include "GameShared/GameClasses/Containers/CgsRingBuffer.h"  // CgsContainers::RingBuffer<T> (inline generic)
#include "BrnCommonTypes.h"                                   // typedef u64 CgsID (8-byte element)

// CgsContainers::RingBuffer<CgsID>::Push  @ 0x823193C0
// ----------------------------------------------------------------------------------------------------
// The recent-stunt ring of BrnGameState::CrashModeScoring (FixedRingBuffer<CgsID,8> mRecentStuntSet @+0x280;
// CgsID == u64, BrnCommonTypes.h:32; embed carved as maRecentStuntSet[0x58] in BrnCrashModeScoringRecentCrash.h)
// drives this RingBuffer<Type> generic Push out of line. The body is already defined inline in
// CgsRingBuffer.h; this file is the thin explicit instantiation for the 8-byte CgsID element (called from
// BrnGameState::CrashModeScoring::DealWithShowtimeStunt). The X360 body matches the generic store-for-store
// at an 8-byte (sizeof CgsID) element stride.
//
// Push (@0x823193C0): write slot = mpData[miWritePos]:
//   `lwz r11,0xC(r3)` (miWritePos), `slwi r11,r11,3` (* sizeof == 8), `lwz r9,0(r3)` (mpData @ +0),
//   `ld r10,0(r4)` + `stdx r10,r11,r9` (the element copy is one 8-byte doubleword == the CgsID u64 copy-assign).
//   Then advance miWritePos (`addi r10,r11,1`; wrap to 0 when `>= miMaxLength`, `blt`/`stw r28`), advance
//   miLength (`lwz r9,0x10(r3)`/`addi r9,r9,1`); when `miLength > miMaxLength` clamp miLength to miMaxLength
//   (`stw r11,0x10(r3)`) and advance miReadPos modulo miMaxLength (`divw`/`mullw`/`subf` => v % miMaxLength,
//   `stw r10,8(r3)`), then assert miReadPos == miWritePos (CgsRingBuffer.h, "Read pos should equal write pos
//   if buffer is full"). The de-inlined BeginAssert/Clear/StrStreamBase::operator<< /FireAssert/EndAssert
//   chain collapses to the single CGS_ASSERT already inline in CgsRingBuffer.h Push.
// The asm member offsets are exactly RingBuffer<Type>: mpData@+0, miMaxLength@+4, miReadPos@+8,
// miWritePos@+0xC, miLength@+0x10.

template void
CgsContainers::RingBuffer<CgsID>::Push(const CgsID*);
