#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficAIInterfaces.h"

// ============================================================================
// GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficAIInterfaces.cpp
//
// BrnTraffic::BrnTrafficIO::TrafficAIInterface::Construct -- DWARF BrnTrafficAIInterfaces.h:137.
// The console emits NO out-of-line symbol for it (absent from names.tsv): it is inlined into
// both owners' Constructs, and the two sites agree store for store --
//   BrnTrafficIO::OutputBuffer_PostScene::Construct @0x82761830 (interface at +16416):
//       *(u16*)(+0)  = 0 ; RivalInTrafficUpdateEvent,34>::Construct(+45072) ;
//       *(u32*)(+46856) = 0 ; *(u32*)(+46996) = 0
//   BrnAI::AIModuleIO::InputBuffer::Construct @0x8278ABA8..0x8278ABCC (interface at +0x43E0):
//       sth r31, 0(r29) ; bl ...RivalInTrafficUpdateEvent,34>::Construct(r29 + 0xB010) ;
//       stwx r31, r29, 0xB708 ; stwx r31, r29, 0xB794
// Member identity (DWARF :179-:185): +0 = mu16EntityCount; +45072 (0xB010) = mUpdateRivalQueue;
// +46856 / +46996 are the miCount words of mAddedRivals / mRemovedRivals (the console's
// Array<T,N> puts its count AFTER the 34 elements: 46856 + 4 + 34*4 == 46996, and the second
// count at 46996 is the last word before the interface's 0xB7A0 end). Array<T,N>::Construct is
// `miCount = 0`. maActiveEntityList is NOT touched.
//
// Created 2026-09-03 (aiwave lane A4) because AIModuleIO::InputBuffer::Construct calls this by
// name and no body existed. The traffic post-scene output buffer's own Construct should call it
// too (see the header_request in the lane report).
// ============================================================================

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    void TrafficAIInterface::Construct()
    {
        mu16EntityCount = 0;
        mUpdateRivalQueue.Construct();
        mAddedRivals.Construct();
        mRemovedRivals.Construct();
    }
}
}
