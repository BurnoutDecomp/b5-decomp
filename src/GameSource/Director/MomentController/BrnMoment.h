#pragma once

// Home for BrnDirector::Moment (the camera-director "moment" base) and the
// MomentBystanderSeesAction concrete moment.
//
// Minimal OWNING slice -- first TU to home Moment, so it carries the declared
// member set (vptr, the by-value Camera, the type/state enums, and the four bool
// flags ending in mbIsInhibited) plus enough of the virtual interface for the two
// bodied functions to resolve by name:
//   Moment::Inhibit
//       mbIsInhibited = true; Release (vtable +0x10); SetState(searching).
//   MomentBystanderSeesAction::Prepare
//       SetState(searching); return true.
//
// FLAG (committed-type size, NOT applied): the console code pins Moment's mbIsInhibited
// at this+0x17B and meState at this+0x174. With the currently committed
// BrnDirector::Camera::Camera slice (sizeof 0x150, its post-+0x140 span still
// NOMINAL) embedded by value after the 4-byte vptr, meType/meState/flags land lower
// than 0x174/0x17B -- i.e. the real console Camera is ~16 bytes larger than the
// committed nominal slice. We do NOT retype/grow Camera here (that's its own TU's
// call); both bodied functions touch their members BY NAME, so semantic parity holds
// and no absolute-offset static_assert is pinned across the nominal Camera span.

#include "types.hpp"
#include "GameSource/Director/Camera/Camera.h"               // BrnDirector::Camera::Camera (by value)
#include "GameSource/Director/Utils/BrnVehicleRef.h"          // BrnDirector::VehicleRef (Moment::VehicleRef base)
#include "GameSource/Director/Camera/BrnBehaviourManager.h"   // Camera::BehaviourHandle<T> (MomentBystanderSeesAction's mBystander)

namespace BrnDirector
{
    namespace Camera { class BehaviourBystanderCam; }   // MomentBystanderSeesAction's handle T (real home BrnBehaviourBystanderCam.h)

    class Moment
    {
    public:
        // The per-moment tuning base. Modelled as a complete empty
        // base (its only member in the declaration is a Construct() helper) so the concrete
        // subclass Parameters records (held by value in MomentParameterBank) can derive
        // from it. SetParameters takes a const Parameters*.
        class Parameters {};

        // The moment-side vehicle-reference wrapper (MomentPassengerSeesAction
        // holds two by value). Extends the committed BrnDirector::VehicleRef with no
        // data; the resolve/set calls land on the base's committed surface.
        class VehicleRef : public BrnDirector::VehicleRef {};

        enum EState
        {
            E_STATE_INVALID_INACTIVE       = 0,
            E_STATE_INVALID_SEARCHING      = 1,
            E_STATE_INVALID_FOUND_PREPARING = 2,
            E_STATE_VALID                  = 3
        };

        enum EType
        {
            E_MOMENT_HARD_STOP           = 0,
            E_MOMENT_HIT_TRAFFIC         = 1,
            E_MOMENT_TUMBLING            = 2,
            E_MOMENT_TAKEDOWN_LOOKBACK   = 3,
            E_MOMENT_PASSENGER_SEES_ACTION = 4,
            E_MOMENT_BYSTANDER_SEES_ACTION = 5,
            E_MOMENT_FAILSAFE            = 6,
            E_MOMENT_PLAYER_JUMPING      = 7,
            E_MOMENT_PLAYER_STUNT        = 8,
            E_MOMENT_STATIC_CAM_IMPACT   = 9,
            E_MOMENT_NEW_CAR_JOINED      = 10,
            E_MOMENT_STATIONARY_CRASH    = 11,

            E_MOMENT_COUNT               = 12
        };

        // --- virtual interface (declared order pins the vtable slots) ---
        //   slot 0  Construct
        //   slot 1  Prepare
        //   slot 2  Update
        //   slot 3  SetParameters
        //   slot 4  Release        <- Inhibit calls this (vtable +0x10)
        //   slot 5  Destruct
        //   slot 6  GetName
        //   slot 7  GetInstanceType
        // DECLARATION-ONLY (no bodies here) except where a default body is attested;
        // pure-virtual where the declaration marks the slot abstract. The per-TU `cl /c`
        // gate does not link, so undefined virtuals are fine.
        // Body recovered from the MomentFailSafe::Construct override, which
        // inlines it verbatim (reset the state machine, latch the concrete type through
        // the live vtable, clear the inhibit flag, construct the embedded camera) --
        // every concrete moment's Construct carries this same inlined base. Defined
        // inline below.
        virtual void  Construct();
        virtual bool  Prepare(/* Camera::BehaviourManager& */ void* lrBehaviourController) = 0;
        virtual void  Update(f32 lfTimeStep,
                             /* Camera::BehaviourManager& */ void* lrBehaviourController,
                             /* MomentSharedInfo::InParam */ const void* lSharedInfo) = 0;
        virtual void  SetParameters(const Parameters* lpParameters);
        virtual bool  Release() = 0;
        virtual void  Destruct();
        virtual const char* GetName() const = 0;

        // --- inline non-virtual interface ---
        void Inhibit();

        EState GetState() const { return meState; }
        EType  GetType()  const { return meType; }
        bool   IsValid()  const { return meState == E_STATE_VALID; }
        bool   IsInhibited() const { return mbIsInhibited; }
        bool   ConditionsAreMet() const { return mbConditionsMet; }
        bool   CanSwitchToMeNow()   const { return mbCanSwitchToMeNow; }
        bool   CanSwitchFromMeNow() const { return mbCanSwitchFromMeNow; }
        const Camera::Camera& GetCamera() const { return mCamera; }

    protected:
        virtual EType GetInstanceType() = 0;

        void SetState(EState leState) { meState = leState; }
        Camera::Camera& GetNonConstCamera() { return mCamera; }

        // ADDITIVE GROW (MomentFailSafe::Update, which clears it while
        // searching): protected setter for the switch-to gate.
        void SetCanSwitchToMeNow(bool lbCanSwitch) { mbCanSwitchToMeNow = lbCanSwitch; }

        // ADDITIVE GROW (MomentHitTraffic::Update; both declared
        // Moment methods the console build inlines): copy a produced camera into the moment's
        // embedded camera / drop the conditions-met flag.
        void SetCamera(const Camera::Camera& lrCamera) { mCamera = lrCamera; }
        void SetConditionsNotMet() { mbConditionsMet = false; }

        // ADDITIVE GROW (MomentStaticCamImpact::Update writes the +0x179 flag byte):
        void SetCanSwitchFromMeNow(bool lbCanSwitch) { mbCanSwitchFromMeNow = lbCanSwitch; }

    public:
        // ADDITIVE GROW 2026-08-01, PUBLIC by necessity: MomentSelector::Update
        // clears this moment's mbIsInhibited from OUTSIDE the class (it writes 0 into the
        //     +0x17B flag byte of the moment GetMoment() just returned), immediately after
        // Inhibit raised it, on
        // the "valid but cannot be switched to, and the description says it may NOT be
        // inhibited" path. The console reaches the private byte directly -- either MomentSelector
        // was a friend or an inline setter folded away; a named setter is the faithful
        // de-inlining and keeps the poke off a raw offset.
        void SetInhibited(bool lbInhibited) { mbIsInhibited = lbInhibited; }

    protected:

        // Member layout. Offsets are NOMINAL beyond the
        // by-name access used here -- see the size FLAG at the top of this file.
        Camera::Camera mCamera;

    private:
        EType  meType;
        EState meState;
        bool   mbCanSwitchToMeNow;
        bool   mbCanSwitchFromMeNow;
        bool   mbConditionsMet;
        bool   mbIsInhibited;          // +0x17B
    };

    inline void Moment::Inhibit()
    {
        // The console body: set mbIsInhibited, call the vtable +0x10 slot (Release),
        // then store E_STATE_INVALID_SEARCHING into meState.
        mbIsInhibited = true;
        Release();
        SetState(E_STATE_INVALID_SEARCHING);
    }

    inline void Moment::Construct()
    {
        // Recovered from the inlined instance in MomentFailSafe::Construct:
        // meState = INACTIVE, meType latched through the live vtable's GetInstanceType
        // (the console's indirect call through vtable slot 7), clear the inhibit flag, and
        // construct the embedded camera.
        meState       = E_STATE_INVALID_INACTIVE;
        meType        = GetInstanceType();
        mbIsInhibited = false;
        mCamera.Construct();
    }

    // MomentBystanderSeesAction -- concrete moment (declared namespace
    // BrnDirector::MomentBystanderSeesAction). Only Prepare is bodied in this TU; the
    // rest of its members/overrides land with the MomentBystanderSeesAction TU. We
    // derive from Moment so Prepare's SetState() resolves by name.
    class MomentBystanderSeesAction : public Moment
    {
    public:
        // Held by value (twice) in
        // MomentParameterBank; modelled faithfully from the declared field list.
        struct Parameters : public Moment::Parameters
        {
            bool mbCloseCamera;
            bool mbCrashMoment;
            bool mbTakedownMoment;
        };

        bool Prepare(void* lrBehaviourController) override;

        // The rest of the override set (GROWN by this moment's own TU, batch 14 --
        // Moments/BrnMomentBystanderSeesAction.cpp replaces the earlier concrete
        // stubs with the real bodies):
        void  Construct() override;
        void  Update(f32 lfTimeStep, void* lrBehaviourController,
                     const void* lSharedInfo) override;
        void  SetParameters(const Moment::Parameters* lpParameters) override;
        bool  Release() override;
        const char* GetName() const override;
        EType GetInstanceType() override { return E_MOMENT_BYSTANDER_SEES_ACTION; }

        // ⭐ ADDED 2026-08-29 (crash-camera wave). Squash the bystander camera's
        // perceived distance so a long crash stays readable. ArbStateCrashing::Update calls it
        // with 0.5 once this moment has held the crash for longer than kfMomentTime.
        // The console body is: assert mBystander.IsAllocated(),
        // resolve the behaviour through the handle, and -- only if the value actually changes --
        // write it and raise the behaviour's re-frame flag. Bodied in this moment's own TU.
        void SetPerceivedDistanceModificationFactor(f32 lfFactor);

    private:
        //     +0x180 / +0x184
        const Parameters* mpParameters;
        Camera::BehaviourHandle<Camera::BehaviourBystanderCam> mBystander;
    };
}
