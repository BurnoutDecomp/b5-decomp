#include "GameShared/GameClasses/Containers/CgsRingBuffer.h"  // CgsContainers::RingBuffer<T> (inline generic)
#include "BrnCommonTypes.h"                                   // Vector3 == rw::math::vpu::Vector3 (16-byte SIMD lane)

// CgsContainers::RingBuffer<rw::math::vpu::Vector3>::Push       @ 0x823194C0
// CgsContainers::RingBuffer<rw::math::vpu::Vector3>::operator[] @ 0x823195C0  (non-const, mutable Type&)
// ----------------------------------------------------------------------------------------------------
// The recent-jump ring of BrnGameState::StuntModeScoring (FixedRingBuffer<Vector3,256> mRecentJumpSet;
// project Vector3 == rw::math::vpu::Vector3) drives these two RingBuffer<Type> generic methods out of
// line. Both are already defined inline in CgsRingBuffer.h; this file is the thin explicit instantiation
// for the 16-byte Vector3 element (called from StuntModeScoring::UpdateAirStunts). The X360 bodies match
// the generic store-for-store at a 16-byte (sizeof Vector3) element stride.
//
// Push (@0x823194C0): write slot = mpData[miWritePos]:
//   `lwz r11,0xC(r3)` (miWritePos), `slwi r11,r11,4` (* sizeof == 16), `lwz r10,0(r3)` (mpData @ +0),
//   `lvx128 v0,r0,r4` + `stvx128 v0,r11,r10` (the element copy is one 16-byte VMX load/store == the
//   implicit Vector3 copy-assign). Then advance miWritePos (`addi r10,r11,1`; wrap to 0 when
//   `>= miMaxLength`, `blt`/`stw r28`), advance miLength (`lwz r9,0x10(r3)`/`addi r9,r9,1`); when
//   `miLength > miMaxLength` clamp miLength to miMaxLength (`stw r11,0x10(r3)`) and advance miReadPos
//   modulo miMaxLength (`divw`/`mullw`/`subf` => `v % miMaxLength`, `stw r10,8(r3)`), then assert
//   miReadPos == miWritePos (CgsRingBuffer.h, "Read pos should equal write pos if buffer is full").
// operator[] (@0x823195C0): range-check assert (luIndex < miLength @ +0x10, "Attempt to access element
//   outside range") then return mpData[(miReadPos + luIndex) % miMaxLength]:
//   `lwz r11,8(r31)` (miReadPos), `lwz r9,4(r31)` (miMaxLength), `add r11,r11,r30` (+ luIndex),
//   `divwu`/`mullw`/`subf` (% miMaxLength), `slwi r11,r11,4` (* sizeof == 16), `add r3,r11,r10` (+ mpData).
// The asm member offsets are exactly RingBuffer<Type>: mpData@+0, miMaxLength@+4, miReadPos@+8,
// miWritePos@+0xC, miLength@+0x10.

template void
CgsContainers::RingBuffer<rw::math::vpu::Vector3>::Push(const rw::math::vpu::Vector3*);

template rw::math::vpu::Vector3&
CgsContainers::RingBuffer<rw::math::vpu::Vector3>::operator[](u32);
