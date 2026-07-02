#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT (the helper tripwires)
#include "GameSource/Director/Camera/BrnBehaviourManager.h"  // Camera::BehaviourHandle + the BehaviourInterpolate
                                                             // minimal slice (its HasFinished decl). NOTE: the real
                                                             // Behaviours/BrnBehaviourInterpolate.h cannot ALSO be
                                                             // included -- the manager header's slice is a second
                                                             // definition of the same class (C2011); the pending
                                                             // reconcile retires the manager slice in favour of the
                                                             // real home.

// ============================================================================
// GameSource/Director/Arbitrator/States/BrnArbStateCarSelect.h
//
// BrnDirector::InterpolaterHelper -- the (offline) car-select state's little
// wrapper around a BehaviourHandle<BehaviourInterpolate>: it owns the camera
// interpolation the state runs while the player browses cars, exposing
// ready / finished / camera / release. DWARF home per the X360 asserts
// (BrnArbStateCarSelect.h:62/:65/:68); the ArbStateCarSelect state itself stays
// the container's placeholder until its own TU lands.
//
// Bodied by this TU (4 ledger functions, class:BrnDirector::InterpolaterHelper;
// all header-inline in the original -- the helper methods carry BOTH the
// helper-level tripwire and the inlined handle-level one):
//   InterpolaterHelper::IsReady     @0x82219900 (h:62 + the handle's :517 pair)
//   InterpolaterHelper::HasFinished @0x82208680 (h:65 + the handle's :600)
//   InterpolaterHelper::GetCamera   @0x82219998 (h:68 + the handle's :610)
//   InterpolaterHelper::Release     @0x822346E0 (== the handle's Release body)
// Caller in the export set: ArbStateCarSelect::Update (its own TU).
// ============================================================================

namespace BrnDirector
{
    struct InterpolaterHelper
    {
        // The helper is "prepared" once its handle owns a behaviour (the X360's
        // helper +0x00 read IS the handle's mbAllocated -- the handle is the
        // helper's leading member).
        bool IsPrepared() const { return mHandle.IsAllocated(); }

        // @0x82219900 -- h:62 tripwire, then the handle's IsReadyToPrepare
        // (whose committed body carries the X360's inlined :517 "mbIsAllocated"
        // tripwire + the manager IsBehaviourWaitingToPrepare()==0 tail).
        bool IsReady() const
        {
            CGS_ASSERT(IsPrepared(), "IsPrepared()");   // :62 (non-gating)
            return mHandle.IsReadyToPrepare();
        }

        // @0x82208680 -- h:65 tripwire + the handle-level :600 "IsAllocated()"
        // tripwire (the X360 inlines the handle's behaviour resolve: slot ->
        // *slot -> the interpolator's +0x596 finished byte; the committed
        // GetBehaviour() cache is that same pointee by Prepare's definition).
        bool HasFinished() const
        {
            CGS_ASSERT(IsPrepared(), "IsPrepared()");            // :65 (non-gating)
            CGS_ASSERT(mHandle.IsAllocated(), "IsAllocated()");   // BrnBehaviourManager.h:600 (non-gating)
            return mHandle.GetBehaviour()->HasFinished();
        }

        // @0x82219998 -- h:68 tripwire (the X360 CALLS IsReady out-of-line, so
        // its own tripwires fire too), then a BY-VALUE copy of the camera the
        // owned behaviour produced (the X360 copy-constructs from the manager
        // pool slot's +0x10 camera -- the handle's GetProducedCamera surface,
        // which carries the :610 handle tripwire).
        Camera::Camera GetCamera() const
        {
            CGS_ASSERT(IsReady(), "IsReady()");   // :68 (non-gating)
            return mHandle.GetProducedCamera();
        }

        // @0x822346E0 -- exactly the handle's Release body (drop the manager-side
        // hold and clear; a no-op when nothing is held).
        void Release()
        {
            mHandle.Release();
        }

    private:
        // The X360 helper is layout-identical to its leading handle (+0x00
        // allocated, +0x04 key, +0x08 helper index, +0x0C manager, +0x10
        // behaviour -- all four bodies read through it).
        Camera::BehaviourHandle<Camera::BehaviourInterpolate> mHandle;
    };
}
