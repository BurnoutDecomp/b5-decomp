#pragma once

// Minimal owning home for the queue element CgsSceneManager::SceneManagerIO::
// InEventAddVolumeInstance -- the per-event payload stored in the
// EventQueue<InEventAddVolumeInstance, 1280> add-volume-instance input queue
// (InSceneUpdateInterface::mAddVolumeInstanceQueue, DWARF home
// CgsSceneManagerIO_SceneUpdate.h:276). This header exists so the explicit-
// instantiation TU for that queue's Construct (EventQueue_InEventAddVolumeInstance_1280.cpp)
// can see a COMPLETE element type (the queue embeds InEventAddVolumeInstance maEvents[1280]
// inline). The full InSceneUpdateInterface aggregate keeps its own placeholder slice and
// its own ledger TU -- this header only adds the leaf element it queues, by name.
//
// LAYOUT (DecFIGS DWARF CgsSceneManagerIO_SceneUpdate.h:137) -- derives from the empty
// SceneManagerIO::Event base:
//     VolumeInstanceId  mVolumeInstanceId  (:139)  u64, 8-aligned
//     VolumeId          mVolumeId          (:140)  u64, 8-aligned
//     Matrix44Affine    mTransform         (:141)  rw vmx 4x4, 64 bytes, 16-aligned
// Carries a Matrix44Affine (16-byte aligned), so the element is alignas(16): the two
// 8-byte handles fill +0x00..+0x0F, then the 64-byte affine matrix lives at +0x10. Total
// sizeof 0x50 (80), 16-aligned. Construct does not read the element interior, so only the
// size/alignment is load-bearing for this TU -- and the element's 16-alignment is exactly
// what makes the 12-byte BaseEventQueue header pad to +0x10 before maEvents (the Construct
// asm at 0x822E1ED0 does `addi r30, this, 0x10`).
#include "types.hpp"
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h" // CgsSceneManager::VolumeInstanceId
#include "GameShared/GameClasses/SceneManager/CgsVolumeId.h"         // CgsSceneManager::VolumeId
#include "rw/math/vpu/types.h"                                       // rw::math::vpu::Matrix44Affine

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image). Distinctly named so this leaf element home never ODR-clashes
    // with the SceneManagerIO::Event defined by the other per-element queue homes.
    struct EventBaseAddVolumeInstance {};

    // mAddVolumeInstanceQueue element. 16-byte aligned (carries an rw Matrix44Affine).
    struct alignas(16) InEventAddVolumeInstance : public EventBaseAddVolumeInstance
    {
        VolumeInstanceId             mVolumeInstanceId; // +0x00 (:139)
        VolumeId                     mVolumeId;         // +0x08 (:140)
        rw::math::vpu::Matrix44Affine mTransform;       // +0x10 (:141)
    };
}
}
