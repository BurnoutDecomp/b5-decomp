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
#include "GameSource/Director/Camera/Camera.h"   // BrnDirector::Camera::Camera (by value)

namespace BrnDirector
{
    // DWARF: BrnMoment.h:97.
    class Moment
    {
    public:
        class Parameters;
        class VehicleRef;

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

    // ----------------------------------------------------------------------------
    // MomentBystanderSeesAction -- concrete moment (DWARF namespace
    // BrnDirector::MomentBystanderSeesAction). Only Prepare is bodied in this TU; the
    // rest of its members/overrides land with the MomentBystanderSeesAction TU. We
    // derive from Moment so Prepare's SetState() resolves by name.
    // ----------------------------------------------------------------------------
    class MomentBystanderSeesAction : public Moment
    {
    public:
        bool Prepare(void* lrBehaviourController) override;   // @0x821F7560

        // Minimal stubs so the class is concrete for the embed check. DECLARATION-ONLY
        // bodies for the rest -- they land with this moment's own TU.
        void  Update(f32, void*, const void*) override {}
        bool  Release() override { return true; }
        const char* GetName() const override;
        EType GetInstanceType() override { return E_MOMENT_BYSTANDER_SEES_ACTION; }
    };
}
