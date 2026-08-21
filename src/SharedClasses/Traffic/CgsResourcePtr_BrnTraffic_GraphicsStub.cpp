#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include "SharedClasses/Traffic/BrnTrafficGraphicsStub.h"   // BrnTraffic::GraphicsStub (its own home; was the ResourceType header's fork)

// Per-instantiation TU for CgsResource::ResourcePtr<BrnTraffic::GraphicsStub>.
// The body is the generic inline non-const operator*() in CgsResourcePtr.h; this
// .cpp only forces the out-of-line emission of the one symbol the X360 ARTIST
// build attested.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnTraffic::GraphicsStub>::operat  @ 0x8271A7E8
//     == CgsResource::ResourcePtr<BrnTraffic::GraphicsStub>::operator*()  (non-const)
//
// The X360 body is the generic non-const operator*(): read offset 0
// (mpResourceMemory) as `*a1`, assert it non-null with the
// "Can not instance resource pointer - it has no main memory resource\n" message
// (baked file CgsResourcePtr.h, baked line 563 = 0x233 in the asm), then return
// that pointer (dereferenced to Type&). Called by
// BrnTraffic::TrafficCarStreamer::GetGraphicsSpec /
// BrnTraffic::TrafficCarStreamer::GetWheelGraphicsSpec.
//
// NOTE (message/line pairing): baked line 563 here carries the "instance" message
// (matching the asm rodata verbatim), which the shared home maps to the non-const
// operator*() (CgsResourcePtr.h:612). This mirrors the committed
// CgsResourcePtr_BrnStreetData_StreetData.cpp operator*() @ 0x82324DC0 (also line
// 563, "instance"). It is distinct from the const operator*() at
// CgsResourcePtr.h:637 which uses the "dereference" message.
template BrnTraffic::GraphicsStub&
    CgsResource::ResourcePtr<BrnTraffic::GraphicsStub>::operator*();
