#include "GameShared/GameClasses/Containers/CgsRingBuffer.h"   // CgsContainers::RingBuffer<T> (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"     // CgsSceneManager::EntityId (single u32 mId, 4-byte element)

// CgsContainers::RingBuffer<CgsSceneManager::EntityId>::Push       @ 0x822AD3E8
// CgsContainers::RingBuffer<CgsSceneManager::EntityId>::operator[] @ 0x822AD4E8  (non-const, mutable EntityId&)
// ----------------------------------------------------------------------------------------------------------
// A ring of scene EntityId handles used by BrnWorld::CrashPlayManager::OnCarCrash. Both methods are the
// generic CgsContainers::RingBuffer<Type> bodies (already inline in CgsRingBuffer.h); this file is the thin
// explicit instantiation for the 4-byte EntityId element (sizeof == 4, single u32 mId). The X360 bodies
// match the generic store-for-store at a 4-byte element stride.
//
// Push (@0x822AD3E8): write slot = mpData[miWritePos]:
//   `lwz r11,0xC(r3)` (miWritePos), `slwi r11,r11,2` (* sizeof == 4), `lwz r9,0(r3)` (mpData @ +0),
//   `lwz r10,0(r4)` + `stwx r10,r11,r9` (the element copy is one 4-byte word == the EntityId u32 copy-assign).
//   Then advance miWritePos (`addi r10,r11,1`; wrap to 0 when `>= miMaxLength`, `blt`/`stw r28`), advance
//   miLength (`addi r9,r9,1`); when `miLength > miMaxLength` clamp miLength to miMaxLength (`stw r11,0x10(r3)`)
//   and advance miReadPos modulo miMaxLength (`divw`/`mullw`/`subf` => v % miMaxLength, `stw r10,8(r3)`), then
//   assert miReadPos == miWritePos (CgsRingBuffer.h:137, "Read pos should equal write pos if buffer is full").
// operator[] (@0x822AD4E8): range-check assert (luIndex < miLength @ +0x10, "Attempt to access element outside
//   range", CgsRingBuffer.h:258) then return mpData[(miReadPos + luIndex) % miMaxLength]:
//   `lwz r11,8(r31)` (miReadPos), `add r11,r11,r30` (+ luIndex), `lwz r9,4(r31)` (miMaxLength),
//   `divwu`/`mullw`/`subf` (% miMaxLength), `slwi r11,r11,2` (* sizeof == 4), `add r3,r11,r10` (+ mpData).
// The asm member offsets are exactly RingBuffer<Type>: mpData@+0, miMaxLength@+4, miReadPos@+8,
// miWritePos@+0xC, miLength@+0x10.

template void
CgsContainers::RingBuffer<CgsSceneManager::EntityId>::Push(const CgsSceneManager::EntityId*);

template CgsSceneManager::EntityId&
CgsContainers::RingBuffer<CgsSceneManager::EntityId>::operator[](u32);
