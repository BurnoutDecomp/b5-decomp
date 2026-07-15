// Per-instantiation .cpp for class:CgsAlgorithms -- the thin explicit-instantiation vein over
// the committed generic algorithms (CgsShuffle.h Fisher-Yates + CgsBubbleSort.h fixed-array
// sort). The generic bodies are header-inline, so this TU just forces the out-of-line emission
// of the four specializations the X360 attests as their own ledger functions:
//
//   CgsAlgorithms::Shuffle<u8,  Stack<u8,199>>   @ 0x8271B298  (TrafficEntityModule::Reset)
//   CgsAlgorithms::Shuffle<u16, Stack<u16,1>>    @ 0x8271B420  (TrafficEntityModule::Reset)
//   CgsAlgorithms::Shuffle<u16, Stack<u16,400>>  @ 0x8271B110  (TrafficEntityModule::Reset)
//   CgsAlgorithms::BubbleSort<NearestCarInfo,8>  @ 0x8222D8F8  (AllVehicleData nearest-car queries)
//
// The Stack<T,N> element types are all committed (CgsStack.h); NearestCarInfo + its Array<,8>
// come from the committed BrnDirectorAllVehicleData home. The 1-arg BubbleSort entry tail-calls
// the un-attested 3-arg range worker (declared-only in CgsBubbleSort.h); only the attested
// 1-arg entry is forced here.
#include "GameShared/GameClasses/Algorithms/CgsShuffle.h"
#include "GameShared/GameClasses/Algorithms/CgsBubbleSort.h"
#include "GameShared/GameClasses/Containers/CgsStack.h"
#include "GameSource/Director/Utils/BrnDirectorAllVehicleData.h"

// ---- CgsAlgorithms::Shuffle instances (CgsShuffle.h inline body) --------------------------
template CgsContainers::Stack<u8, 199>&
CgsAlgorithms::Shuffle<u8, CgsContainers::Stack<u8, 199> >(
    CgsContainers::Stack<u8, 199>&, s32, s32, CgsNumeric::Random&);

template CgsContainers::Stack<u16, 1>&
CgsAlgorithms::Shuffle<u16, CgsContainers::Stack<u16, 1> >(
    CgsContainers::Stack<u16, 1>&, s32, s32, CgsNumeric::Random&);

template CgsContainers::Stack<u16, 400>&
CgsAlgorithms::Shuffle<u16, CgsContainers::Stack<u16, 400> >(
    CgsContainers::Stack<u16, 400>&, s32, s32, CgsNumeric::Random&);

// ---- CgsAlgorithms::BubbleSort 1-arg entry (CgsBubbleSort.h inline body) ------------------
template void
CgsAlgorithms::BubbleSort<BrnDirector::AllVehicleData::NearestCarInfo, 8>(
    Array<BrnDirector::AllVehicleData::NearestCarInfo, 8>&);
