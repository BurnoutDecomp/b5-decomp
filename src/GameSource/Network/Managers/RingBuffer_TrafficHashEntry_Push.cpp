#include "GameShared/GameClasses/Containers/CgsRingBuffer.h"               // CgsContainers::RingBuffer<T>::Push (inline generic)
#include "GameSource/Network/Managers/BrnNetworkTrafficManager.h"          // BrnNetwork::TrafficManager::TrafficHashEntry (4-byte element)

// CgsContainers::RingBuffer<BrnNetwork::TrafficManager::TrafficHashEntry>::Push @ 0x8254DC90
// Reconstructed from BURNOUT_X360_ARTIST.XEX. This is the generic
// CgsContainers::RingBuffer<Type>::Push (already inline in CgsRingBuffer.h); the file here is
// the thin explicit instantiation for the TrafficHash ring's 4-byte two-halfword element.
// The X360 body matches the generic store-for-store at a 4-byte stride:
//   * write slot = mpData[miWritePos]: `lwz r11,0xC(r3)` (miWritePos), `slwi r11,r11,2`
//     (* sizeof == 4), `add r11,r11,r10` (+ mpData @ +0); the element copy is two halfwords
//     (`lhz/sth` @ +0 and +2 == TrafficHashEntry's two u16 lanes);
//   * advance miWritePos (`addi r10,r11,1`; wrap to 0 when `>= miMaxLength`, `blt`/`stw r28`);
//   * advance miLength (`addi r9,r9,1`); when `miLength > miMaxLength` clamp miLength to
//     miMaxLength (`stw r11,0x10(r3)`) and advance miReadPos modulo miMaxLength
//     (`divw`/`mullw`/`subf` => `v6 % v4`, `stw r10,8(r3)`), then assert miReadPos == miWritePos
//     (CgsRingBuffer.h:137, "Read pos should equal write pos if buffer is full").
// The asm member offsets are exactly RingBuffer<Type>: mpData@+0, miMaxLength@+4, miReadPos@+8,
// miWritePos@+0xC, miLength@+0x10.
template void
CgsContainers::RingBuffer<BrnNetwork::TrafficManager::TrafficHashEntry>::Push(
    const BrnNetwork::TrafficManager::TrafficHashEntry*);
