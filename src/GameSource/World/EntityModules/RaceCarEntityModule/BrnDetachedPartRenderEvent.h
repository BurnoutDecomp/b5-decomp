#pragma once

// BrnWorld::DetachedPartRenderEvent -- the per-frame render-side detached-part transform event
// queued by an ActiveRaceCar. The queue (EventQueue<DetachedPartRenderEvent, 20>) lives inline in
// BrnWorld::ActiveRaceCar (constructed at this+5520 by ActiveRaceCar::OnResourcesLoaded
// @0x822EB168); the X360 BaseEventQueue<DetachedPartRenderEvent>::AddEventSafe (@0x822C88B0)
// copies the leading 64-byte Matrix44Affine in four 16-byte SIMD stores (offsets +0/+16/+32/+48)
// then the two scalar dwords at +64/+68 -- i.e. a 72-byte payload at a 16-byte (0x50) stride.
// alignas(16): carries a Matrix44Affine (SIMD) and the owning queue's inline buffer is 16-byte
// aligned, so the EventQueue<...,20> base subobject pads 12->16, giving the maEvents offset 0x10
// the EventQueue<...,20>::Construct attests (@0x822E3910).
//
// Distinct from BrnPhysics::Deformation::DetachedPartRenderEvent (BrnDeformationEvents.h): same
// 72/80 layout, different namespace and owning queue (the deformation manager's output interface
// vs. the per-race-car render queue). Member names/types follow the deformation sibling, whose
// DecFIGS DWARF (BrnDeformationOutputInterface.h:56) gives the matrix+id+index trailer.
#include "BrnCommonTypes.h"   // Matrix44Affine, EntityId

namespace BrnWorld
{
    struct alignas(16) DetachedPartRenderEvent
    {
        Matrix44Affine mTransform;        // +0x00  (4x 16-byte SIMD row = 64 bytes)
        EntityId       mVehicleEntityId;  // +0x40  (dword, stw at +0x40)
        u8             muPartIndex;       // +0x44  (BYTE, stb at +0x44 -- not a dword)
        // +0x45..+0x4F: alignas(16) tail padding (untouched by the writer)
    };
}
