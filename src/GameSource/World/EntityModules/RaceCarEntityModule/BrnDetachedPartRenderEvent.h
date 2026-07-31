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
//
// ---- 2026-07-31 RENDER WAVE: the trailer members were MIS-NAMED ------------
// The two trailer slots were previously named from the DEFORMATION sibling
// (mVehicleEntityId / muPartIndex). The RACE-CAR event's own DWARF
// (BrnActiveRaceCar.h:108-112) gives `int32_t miPartIndex; bool mbIsAttached;`, and
// the CONSUMER proves it: RaceCarEntityModule::RenderRaceCar @0x822CF6A0 searches the
// queue with `lwz r11, 0x40(r3); cmpw r11, r21` -- comparing +0x40 against the body
// PART INDEX -- and then reads `lbz r28, 0x44(r3)` as the boolean that decides whether
// the part is drawn with the deformation constant armed. So +0x40 is miPartIndex and
// +0x44 is mbIsAttached; the writer's stw/stb widths are unchanged.
#include "BrnCommonTypes.h"   // Matrix44Affine

namespace BrnWorld
{
    struct alignas(16) DetachedPartRenderEvent
    {
        Matrix44Affine mTransform;    // +0x00  (4x 16-byte SIMD row = 64 bytes)
        s32            miPartIndex;   // +0x40  (dword, stw at +0x40)
        bool           mbIsAttached;  // +0x44  (BYTE, stb at +0x44)
        // +0x45..+0x4F: alignas(16) tail padding (untouched by the writer)
    };
}
