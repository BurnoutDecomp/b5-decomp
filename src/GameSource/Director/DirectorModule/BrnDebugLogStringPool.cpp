// ============================================================================
// GameSource/Director/DirectorModule/BrnDebugLogStringPool.cpp
//
// Per-instantiation compilation home for the CgsContainers::ObjectPool<
// BrnDirector::DebugLog::LogString, 20, int> member bodies this TU owns:
//   - AllocateObject @0x82210CF8   (CgsObjectPool.h:193/0xC1 + CgsBitArray.h:222 asserts)
//   - FreeObject     @0x82210E78   (CgsObjectPool.h:205/206/209 + CgsBitArray.h:203 asserts)
//
// These are the generic inline ObjectPool<T,N,TIndex> bodies in CgsObjectPool.h forced out-of-line
// via explicit member instantiation (the X360 emits one out-of-line copy per using-TU). The
// element is the committed BrnDirector::DebugLog::LogString (72-byte timed colour string),
// reused by name; N == 20, TIndex == int. The director debug log's ActualAppend pops a slot with
// AllocateObject; Update / ActualAppend return expired slots with FreeObject.
//
// Byte-parity notes (store-for-store off the asm):
//   AllocateObject @0x82210CF8: reads miNumObjectsFree (+0x5F0); if <= 0 returns -1; else
//     --miNumObjectsFree, pops liFreeObjectIndex = maiObjectFreeQueue[miNumObjectsFree]
//     (lwzx at word offset (miNumObjectsFree + 0x168) -- i.e. the free queue sits right after the
//     20*72-byte pool, at +0x5A0), asserts the index < 20 (the "Array index out of bounds" /
//     "Index: .. Number of bits: 20" tripwires), then sets the occupancy bit
//     (BitArray::SetBit: ldx/or/stdx the 1<<(idx&0x3f) mask into the (idx>>6)-th 64-bit word at
//     +0x5F8) and returns the index. Matches ObjectPool<T,N,TIndex>::AllocateObject.
//   FreeObject @0x82210E78: asserts a2 < 20 and miNumObjectsFree < 20, pushes the index onto the
//     free queue (maiObjectFreeQueue[miNumObjectsFree] = a2; ++miNumObjectsFree), asserts the
//     occupancy bit is currently set ("The object isn't allocated"), then clears it
//     (BitArray::UnSetBit: ldx/andc-equivalent `& ~mask`/stdx). Matches
//     ObjectPool<T,N,TIndex>::FreeObject.
// ============================================================================

#include "GameShared/GameClasses/Containers/CgsObjectPool.h"
#include "GameSource/Director/DirectorModule/BrnDebugLogString.h"  // BrnDirector::DebugLog::LogString (72B element)

template int  CgsContainers::ObjectPool<BrnDirector::DebugLog::LogString, 20, int>::AllocateObject();
template void CgsContainers::ObjectPool<BrnDirector::DebugLog::LogString, 20, int>::FreeObject(int);
// operator[] @0x82211048 (class:BrnDirector::DebugLog TU; the IDA symbol is the
// truncated instantiation name "DebugLog::LogSt..."): bounds tripwire
// (CgsObjectPool.h:218), the streamed BitArray<20> index guard (CgsBitArray.h:203),
// the allocated-bit check (:219), then &pool[72 * index] -- the committed generic.
template BrnDirector::DebugLog::LogString& CgsContainers::ObjectPool<BrnDirector::DebugLog::LogString, 20, int>::operator[](int);
