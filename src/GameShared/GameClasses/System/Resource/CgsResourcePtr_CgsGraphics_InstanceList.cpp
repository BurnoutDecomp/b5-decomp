#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"

// Per-instantiation TU for CgsResource::ResourcePtr<CgsGraphics::InstanceList>.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsGraphics::InstanceList>::GetMemoryR  @ 0x822CA008
//     == CgsResource::ResourcePtr<CgsGraphics::InstanceList>::GetMemoryResource()
//        (non-const).
//
// The X360 body is the generic non-const GetMemoryResource() inline in
// CgsResourcePtr.h: read offset 0 (mpResourceMemory), assert it non-null with the
// "Can not instance resource pointer - it has no main memory resource\n" message
// (baked file CgsResourcePtr.h, baked line 581 = 0x245 in the asm), and return it
// as Type*. This .cpp only forces the out-of-line emission of that one symbol the
// ARTIST build attested; the body lives in the ONE shared home (CgsResourcePtr.h).
//
// CgsGraphics::InstanceList is a COMMITTED-but-HEADERLESS type (modelled TU-local in
// CgsInstance.cpp / mirrored in CgsInstanceListResourceType.cpp). ResourcePtr<T>'s
// GetMemoryResource() only static_casts a void* to Type*, so an incomplete
// (forward) declaration is sufficient here and avoids forking the layout.
namespace CgsGraphics
{
    struct InstanceList;   // committed home: CgsInstance.cpp (headerless)
}

// GetMemoryResource() (non-const) @ 0x822CA008 (baked assert line 581, "instance" msg).
template CgsGraphics::InstanceList*
    CgsResource::ResourcePtr<CgsGraphics::InstanceList>::GetMemoryResource();
