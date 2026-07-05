// Explicit-instantiation ledger for CgsSceneManager::CoarseQueryResultBuffer<16384> --
// the shipping coarse-query result staging buffer of the spatial-partition module. The
// six X360 ledger symbols are all thin instantiations of the generic bodies inline in
// CgsCoarseQueryResultBuffer.h:
//
//   BeginResultsBatch     @ 0x828AD950
//   GetResultsBatch       @ 0x828AD8E0
//   EndResultsBatch       @ 0x828ADA10
//   PushResult            @ 0x828ADAC0
//   PushResults           @ 0x828ADB98
//   GetNumResultsWritten  @ 0x828ADC60
//   GetBatchByOffset      @ 0x828ADD40
//
// KU_MaxResults == 16384 (0x4000); the backing buffer is 16448 u16 slots (capacity + 64
// header/slack), honest size 32920 bytes.
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/CgsCoarseQueryResultBuffer.h"

template void       CgsSceneManager::CoarseQueryResultBuffer<16384>::BeginResultsBatch();
template u16*       CgsSceneManager::CoarseQueryResultBuffer<16384>::GetResultsBatch();
template void       CgsSceneManager::CoarseQueryResultBuffer<16384>::EndResultsBatch();
template bool       CgsSceneManager::CoarseQueryResultBuffer<16384>::PushResult(u16);
template bool       CgsSceneManager::CoarseQueryResultBuffer<16384>::PushResults(const u16*, u32);
template s32        CgsSceneManager::CoarseQueryResultBuffer<16384>::GetNumResultsWritten() const;
template const u16* CgsSceneManager::CoarseQueryResultBuffer<16384>::GetBatchByOffset(u32, u32*, u32*) const;
