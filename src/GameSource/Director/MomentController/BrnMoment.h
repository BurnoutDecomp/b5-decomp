#pragma once

// Home for BrnDirector::Moment (the camera-director "moment" base) and the
// MomentBystanderSeesAction concrete moment.
// DWARF home: GameSource/Director/MomentController/BrnMoment.h:97.
//
// Minimal OWNING slice -- first TU to home Moment, so it carries the DWARF-attested
// member set (vptr, the by-value Camera, the type/state enums, and the four bool
// flags ending in mbIsInhibited) plus enough of the virtual interface for the two
// bodied functions to resolve by name:
//   Moment::Inhibit                       @0x82208520
//       mbIsInhibited = true; Release() (vtable +0x10); SetState(searching).
//   MomentBystanderSeesAction::Prepare    @0x821F7560
//       SetState(searching); return true.
//
// FLAG (committed-type size, NOT applied): the X360 asm pins Moment's mbIsInhibited
// at this+0x17B and meState at this+0x174. With the currently committed
// BrnDirector::Camera::Camera slice (sizeof 0x150, its post-+0x140 span still
// NOMINAL) embedded by value after the 4-byte vptr, meType/meState/flags land lower
// than 0x174/0x17B -- i.e. the real X360 Camera is ~16 bytes larger than the
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

    // DWARF: BrnMoment.h:97.
    class Moment
    {
    public:
        // DWARF: BrnMoment.h:274. The per-moment tuning base. Modelled as a complete empty
        // base (its only member in the DWARF is a Construct() helper) so the concrete
        // subclass Parameters records (held by value in MomentParameterBank) can derive
        // from it. SetParameters takes a const Parameters*.
        class Parameters {};

        // DWARF: the moment-side vehicle-reference wrapper (MomentPassengerSeesAction
        // holds two by value). Extends the committed BrnDirector::VehicleRef with no
        // data; the resolve/set calls land on the base's committed surface.
        class VehicleRef : public BrnDirector::VehicleRef {};

        // DWARF: BrnMoment.h:104.
        enum EState
        {
            E_STATE_INVALID_INACTIVE       = 0,
            E_STATE_INVALID_SEARCHING      = 1,
            E_STATE_INVALID_FOUND_PREPARING = 2,
            E_STATE_VALID                  = 3
        };

        // DWARF: BrnMoment.h:117.
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

        // --- virtual interface (DWARF order pins the vtable slots) ---
        //   slot 0  Construct
        //   slot 1  Prepare
        //   slot 2  Update
        //   slot 3  SetParameters
        //   slot 4  Release        <- Inhibit calls this (vtable +0x10)
        //   slot 5  Destruct
        //   slot 6  GetName
        //   slot 7  GetInstanceType
        // DECLARATION-ONLY (no bodies here) except where a default body is attested;
        // pure-virtual where the DWARF marks the slot abstract. The per-TU `cl /c`
        // gate does not link, so undefined virtuals are fine.
        // Body recovered from the MomentFailSafe::Construct override @0x8225F190, which
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
        void Inhibit();   // @0x82208520

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

        // ADDITIVE GROW (MomentFailSafe::Update @0x8220A2B0, which clears it while
        // searching): protected setter for the switch-to gate.
        void SetCanSwitchToMeNow(bool lbCanSwitch) { mbCanSwitchToMeNow = lbCanSwitch; }

        // ADDITIVE GROW (MomentHitTraffic::Update @0x82271D90; both PS3-DWARF-named
        // Moment methods the X360 inlines): copy a produced camera into the moment's
        // embedded camera / drop the conditions-met flag.
        void SetCamera(const Camera::Camera& lrCamera) { mCamera = lrCamera; }
        void SetConditionsNotMet() { mbConditionsMet = false; }

        // ADDITIVE GROW (MomentStaticCamImpact::Update @0x82266B6C, `stb 0x179`):
        void SetCanSwitchFromMeNow(bool lbCanSwitch) { mbCanSwitchFromMeNow = lbCanSwitch; }

    public:
        // ADDITIVE GROW 2026-08-01, PUBLIC by necessity: MomentSelector::Update @0x8223A3DC
        // clears this moment's mbIsInhibited from OUTSIDE the class (`stb r21(0), 0x17B(r11)`
        // on the pointer GetMoment() just returned), immediately after Inhibit() raised it, on
        // the "valid but cannot be switched to, and the description says it may NOT be
        // inhibited" path. The console reaches the private byte directly -- either MomentSelector
        // was a friend or an inline setter folded away; a named setter is the faithful
        // de-inlining and keeps the poke off a raw offset.
        void SetInhibited(bool lbInhibited) { mbIsInhibited = lbInhibited; }

    protected:

        // DWARF member layout (BrnMoment.h:243..256). Offsets are NOMINAL beyond the
        // by-name access used here -- see the size FLAG at the top of this file.
        Camera::Camera mCamera;        // BrnMoment.h:243

    private:
        EType  meType;                 // BrnMoment.h:247
        EState meState;                // BrnMoment.h:248
        bool   mbCanSwitchToMeNow;     // BrnMoment.h:252
        bool   mbCanSwitchFromMeNow;   // BrnMoment.h:253
        bool   mbConditionsMet;        // BrnMoment.h:254
        bool   mbIsInhibited;          // BrnMoment.h:256  (X360 this+0x17B)
    };

    inline void Moment::Inhibit()
    {
        // X360 @0x82208520: stb mbIsInhibited=1; call vtable+0x10 (Release);
        //                   stw meState = E_STATE_INVALID_SEARCHING.
        mbIsInhibited = true;
        Release();
        SetState(E_STATE_INVALID_SEARCHING);
    }

    inline void Moment::Construct()
    {
        // Recovered from the inlined instance in MomentFailSafe::Construct @0x8225F190:
        // meState = INACTIVE, meType latched through the live vtable's GetInstanceType
        // (the asm's indirect call through vtbl slot 7), clear the inhibit flag, and
        // construct the embedded camera.
        meState       = E_STATE_INVALID_INACTIVE;
        meType        = GetInstanceType();
        mbIsInhibited = false;
        mCamera.Construct();
    }

    // ----------------------------------------------------------------------------
    // MomentBystanderSeesAction -- concrete moment (DWARF namespace
    // BrnDirector::MomentBystanderSeesAction). Only Prepare is bodied in this TU; the
    // rest of its members/overrides land with the MomentBystanderSeesAction TU. We
    // derive from Moment so Prepare's SetState() resolves by name.
    // ----------------------------------------------------------------------------
    class MomentBystanderSeesAction : public Moment
    {
    public:
        // DWARF: Moments/BrnMomentBystanderSeesAction.h:100. Held by value (twice) in
        // MomentParameterBank; modelled faithfully from the DWARF field list.
        struct Parameters : public Moment::Parameters
        {
            bool mbCloseCamera;     // BrnMomentBystanderSeesAction.h:102
            bool mbCrashMoment;     // BrnMomentBystanderSeesAction.h:104
            bool mbTakedownMoment;  // BrnMomentBystanderSeesAction.h:105
        };

        bool Prepare(void* lrBehaviourController) override;   // @0x821F7560

        // The rest of the override set (GROWN by this moment's own TU, batch 14 --
        // Moments/BrnMomentBystanderSeesAction.cpp replaces the earlier concrete
        // stubs with the real bodies):
        void  Construct() override;                                            // @0x8225F118
        void  Update(f32 lfTimeStep, void* lrBehaviourController,
                     const void* lSharedInfo) override;                        // @0x82266730
        void  SetParameters(const Moment::Parameters* lpParameters) override;  // @0x821F7670
        bool  Release() override;                                              // @0x8223AAF8
        const char* GetName() const override;                                  // @0x821F7608
        EType GetInstanceType() override { return E_MOMENT_BYSTANDER_SEES_ACTION; }

    private:
        // DWARF h:87/h:89 (X360 +0x180 / +0x184).
        const Parameters* mpParameters;
        Camera::BehaviourHandle<Camera::BehaviourBystanderCam> mBystander;
    };
}
