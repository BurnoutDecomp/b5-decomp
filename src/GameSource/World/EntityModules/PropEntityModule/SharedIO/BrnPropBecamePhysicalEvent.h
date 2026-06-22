#pragma once

// BrnWorld::PropEntityIO::PropBecamePhysicalEvent — published when a prop transitions
// to a physical (simulated) state; carries the prop's world position. Reconstructed
// from the DecFIGS DWARF (BrnPropEntityModuleIO.h:211-213: a single Vector3 mPosition)
// + the X360 ARTIST spine of its event queue (element stride 16 bytes, matching the
// 16-byte alignment of rw::math::vpu::Vector3). This is the minimal owning home for the
// element type; the queue itself reuses the committed BaseEventQueue<T> generic.
//
// The DWARF homes this type in BrnPropEntityModuleIO.h, but that file's current slice is
// scoped to PropVFXLocatorEvent / OutputBuffer_PreScene (owned by another group). To
// avoid editing a foreign group's file, the element type is homed here in the prop
// SharedIO area, in its canonical namespace; nothing in BrnPropEntityModuleIO.h is touched.
#include "BrnCommonTypes.h"   // Vector3 (rw::math::vpu::Vector3, 16-byte aligned)

namespace BrnWorld
{
namespace PropEntityIO
{
    // BrnPropEntityModuleIO.h:211 — single-member POD; X360 queue stride is 16 bytes
    // (`slwi r,n,4` in the Append/copy paths), == aligned sizeof(Vector3).
    struct PropBecamePhysicalEvent
    {
        Vector3 mPosition;   // :213
    };
}
}
