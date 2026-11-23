// CgsMemoryUnity: one of ARTIST's unity translation units (DWARF compile unit CgsMemoryUnity.cpp),
// ARTIST 0x82866680-0x8286E038.
// Each member also compiles on its own; this file holds only their includes.
// Include form from ARTIST's __FILE__ strings (0x820E6EA0: ...gameshared\gameclasses\memory\CgsHeapMalloc.cpp).
// Order: ARTIST's emission passes over the range (MSVC emits bottom-up over the call graph, each
// level in definition order: 17/21 transitions forward);
// first member: the one PS3's static constructor names (_GLOBAL__I_ of CgsMemory::HeapMalloc::Construct).

#include "CgsHeapMalloc.cpp"
#include "CgsLinearMalloc.cpp"
#include "CgsMemoryModule.cpp"
#include "CgsDistributionStream.cpp"
#include "CgsGatherStream.cpp"  // also holds X360/CgsGatherStreamX360.cpp (ARTIST 0x820E7628)
#include "CgsScatterStream.cpp"  // also holds X360/CgsScatterStreamX360.cpp (ARTIST 0x820E7688)
#include "DataStream/CgsDataStreamCommandReader.cpp"
#include "DataStream/CgsDataStreamCommandPoster.cpp"
#include "DataStream/CgsDataStreamResultPoster.cpp"
#include "DataStream/CgsDataStreamResultReader.cpp"
#include "DataStream/CgsSimpleDataStreamConsumer.cpp"
#include "../../Jobs/Relocator/CgsRelocator.cpp"
#include "DataStream/CgsSimpleDataStreamProducer.cpp"
#include "CgsMemoryModuleIO.cpp"
#include "MemoryMap/CgsMemoryMap.cpp"  // ARTIST 0x820E8160 reads memory\memorymap\CgsMemoryMap.cpp: reached through a file in MemoryMap/ (not recovered)
