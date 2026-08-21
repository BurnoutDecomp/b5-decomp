#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include "SharedClasses/Traffic/BrnTrafficGraphicsStub.h"   // BrnTraffic::GraphicsStub

// Per-instantiation TU for CgsResource::ResourcePtr<BrnTraffic::GraphicsStub>. The
// body is the generic inline non-const operator*() in CgsResourcePtr.h; this .cpp
// only forces out-of-line emission of the one symbol ARTIST attested.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::ResourcePtr<BrnTraffic::GraphicsStub>::operator*()  @ 0x8271A7E8
//
// Assert provenance: baked file CgsResourcePtr.h, baked line 563, message "Can not
// instance resource pointer - it has no main memory resource\n" -- the non-const
// operator*() (CgsResourcePtr.h:612), not the const one at :637 whose message reads
// "dereference".
template BrnTraffic::GraphicsStub&
    CgsResource::ResourcePtr<BrnTraffic::GraphicsStub>::operator*();
