#pragma once

#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"   // CgsResource::ResourceHandle

namespace BrnPhysics
{
    namespace Vehicle { struct VehiclePhysics; }

    namespace Deformation
    {
        // Recovered from BrnDeformationEvents.h / BrnIKBodyPart.h (DecFIGS DWARF).
        enum EBodyParts : s32
        {
            E_BODY_PART_INVALID = -1
        };

        enum DeformationResetType : s32
        {
            E_DEFORMATION_RESET_NONE = 0
        };

        // ⭐⭐ THE MODEL HANDLE IS THE REAL RESOURCE HANDLE, NOT A u32 (2026-08-11).
        // Until this date this was a 4-byte stand-in `struct ResourceHandle { u32 muValue; }`, so
        // VehicleManager::AddRaceCarDeformationModel had no honest way to pass the handle the
        // console hands it and parked a NULL instead. The console passes the WHOLE 8-byte handle
        // (`ldx r4, r20, r29` @0x825E932C, out of a `slwi ,3` stride-8 array), the AddEvent element
        // copy emits it as the TWO 32-bit pointer stores `stw +0 / stw +4`, and the consumer
        // @0x82644930 reads it back with one `ld 0(evt)`. It is CgsResource::ResourceHandle
        // {void*, Entry*} -- 8 bytes on the console, 16 on the host, exactly like the sibling
        // BrnVehicleEvents.h which has spelled it `using CgsResource::ResourceHandle;` all along
        // (and whose ValidateRaceCarEvent embed check already re-derives the widened stride).
        // ⭐ The producer side is already this type: VehicleManager::maRaceCarModelHandles is a
        // CgsResource::ResourceHandle[8], fed from CreateRaceCarEvent::mModelHandle.
        using CgsResource::ResourceHandle;

        // Input event: spawn a deformable model for a body.
        // CONSOLE LAYOUT (from the AddEvent copy and the ProcessAddDeformationModelEvents read,
        // which agree instruction for instruction): mModelHandle +0x00 (8) · mHandlingBodyID +0x08
        // (8, one `std`/`ld`) · mGlobalEntityId +0x10 (4, one `stw`/`lwz`) · pad · mCOMOffset +0x20
        // · mInitialWorldSpaceTransform +0x30 · velocity +0x70 · angular +0x80 · mpVehiclePhysics
        // +0x90 · damage +0x94 · type +0x98 · bool +0x9C == 0xA0 == 160.
        // ⭐ On the host the two pointer-pairs widen (handle 8->16, mpVehiclePhysics 4->8) and the
        // record becomes 176 -- and every SIMD member lands back on its EXACT console offset
        // (+0x20/+0x30/+0x70/+0x80), which the pre-widening 160-byte host layout did NOT.
        struct alignas(16) AddDeformationModelEvent
        {
            ResourceHandle             mModelHandle;
            RigidBodyId                mHandlingBodyID;
            EntityId                   mGlobalEntityId;
            Vector3                    mCOMOffset;
            Matrix44Affine             mInitialWorldSpaceTransform;
            Vector3                    mInitialWorldSpaceVelocity;
            Vector3                    mInitialWorldSpaceAngularVelocity;
            BrnPhysics::Vehicle::VehiclePhysics* mpVehiclePhysics;
            f32                        mfInitialDamageAmount;
            DeformationResetType       meBaseDeformationType;
            bool                       mbDoSweptSphereTests;
        };

        // Output event: a joint between two parts has broken.
        struct alignas(16) BrokenJointNotificationEvent
        {
            Vector3    mPointOnA;
            EntityId   mVehicleId;
            EBodyParts meType;
        };

        // Input event: stop simulating a deformable model. The X360 places the
        // owning queue's inline buffer 16-byte aligned.
        struct alignas(16) DeactivateDeformationModelEvent
        {
            RigidBodyId          mHandlingBodyID;
            f32                  mfInitialDamageAmount;
            DeformationResetType meDeformationResetType;
        };

        // Input event: remove a deformation model from simulation.
        struct alignas(16) RemoveDeformationModelEvent
        {
            RigidBodyId mHandlingBodyID;
        };

        // Input event: toggle a deformation model's collision.
        struct alignas(16) SetModelCollisionEvent
        {
            RigidBodyId mHandlingBodyID;
            bool        mbCollide;
        };

        // Output event: a part has detached from a vehicle.
        struct alignas(16) DetachedPartNotificationEvent
        {
            Vector3    mPointOnA;
            EntityId   mVehicleId;
            EBodyParts meType;
        };

        // ADDITIVE GROW (flagged by Deformation group, BrnIKDrivenPoint/DetachedPartRenderEvent
        // TU): the per-frame render-side detached-part transform event. Member names/types from
        // the DecFIGS DWARF (BrnDeformationOutputInterface.h:56). The X360
        // BaseEventQueue<DetachedPartRenderEvent>::AddEvent (@0x825E5C78) copies the leading
        // 64-byte Matrix44Affine in four 16-byte SIMD stores (offsets +0/+16/+32/+48), then the
        // two scalar dwords at +64/+68 and the bool at +72 — i.e. stride 80 (0x50). alignas(16):
        // carries a Matrix44Affine (SIMD) and the owning queue's inline buffer is 16-byte aligned.
        struct alignas(16) DetachedPartRenderEvent
        {
            Matrix44Affine mTransform;        // +0x00  (4x Vector3 lane = 64 bytes)
            EntityId       mVehicleEntityId;  // +0x40
            s32            miPartIndex;       // +0x44
            bool           mbIsAttached;      // +0x48
        };

        // Output event: current orientation/velocity of a hinged (jointed) part.
        // NOT 16-byte aligned (no SIMD members): DeformationOutputInterface places its
        // queue at byte +116, which is only 4-aligned — proof it is a plain 4-aligned
        // struct, so the queue's inline buffer starts at +12 (not +16).
        struct JointedPartStateEvent
        {
            EntityId   mVehicleId;
            EBodyParts meType;
            f32        mfCurrentOrientation;
            f32        mfHingeVelocity;
        };

        // Input event: set a deformation model's scene culling group.
        struct alignas(16) SetModelCullingGroupEvent
        {
            RigidBodyId  mHandlingBodyID;
            typedef u32  CullingGroup;   // InEventAddForCollision::CullingGroup (CgsSceneManagerTypes.h)
            CullingGroup mCullGroup;
        };
    }
}
