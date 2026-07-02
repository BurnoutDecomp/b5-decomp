#pragma once

#include "types.hpp"
#include "GameSource/Director/MomentController/BrnMoment.h"              // BrnDirector::Moment (base)
#include "GameSource/Director/Camera/BrnBehaviourManager.h"              // Camera::BehaviourHandle<T> / BehaviourManager
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.h"   // Camera::BehaviourIceAnim

// BrnDirector::MomentStationaryCrash - the "stationary crash" camera moment:
// while the player wrecks at (near) standstill, play the stationary- or
// tumbling-crash ICE camera take (the tumbling variant once the integrated
// crash rotation exceeds a full turn). Class shape / member names / method set
// verbatim from the DecFIGS DWARF (BrnMomentStationaryCrash.h:46/:81-:88/:98);
// gated on the X360 ledger. This TU bodies Construct/Prepare/Update/Release/
// GetName; SetParameters/Destruct/GetInstanceType and Parameters::Construct are
// their own ledger functions (declaration-only, DWARF-gated).
//
// NOTE (mutual exclusion, the MomentHitTraffic precedent): BrnMomentSubclasses.h
// carries a layout-stubbed MomentStationaryCrash for the NewMoment TU; this real
// home and that stub define the same class and CANNOT be included into one TU.
namespace BrnDirector
{
    class MomentStationaryCrash : public Moment
    {
    public:
        // DWARF BrnMomentStationaryCrash.h:98 -- the (member-less) tuning record.
        struct Parameters : public Moment::Parameters
        {
            // DWARF h:102 (cpp) -- its own ledger function (declaration-only).
            void Construct();
        };

        // @0x8225F518 (this TU, DWARF cpp:34) -- the inlined base Construct, the
        // ICE-cam handle clear, and the take/rotation/tumbling/parameters resets.
        virtual void Construct();

        // @0x821F7688 (this TU, DWARF cpp:56) -- zero the crash timer and enter
        // SEARCHING. Always reports true.
        virtual bool Prepare(void* lrBehaviourController);

        // @0x82272EA8 (this TU, DWARF cpp:75) -- the per-frame stationary-crash
        // state machine (see the .cpp).
        virtual void Update(f32 lfTimeStep, void* lrBehaviourController,
                            const void* lSharedInfo);

        // @0x8223B148 (this TU, DWARF cpp:196) -- drop the ICE cam if held (the
        // inlined BehaviourHandle::Release) and reset to SEARCHING. Returns true.
        virtual bool Release();

        // @0x821F76B0 (this TU, DWARF cpp:252).
        virtual const char* GetName() const;

        // DWARF cpp:226/:212 -- declaration-only (their own ledger functions).
        virtual void SetParameters(const Moment::Parameters* lpParameters);
        virtual void Destruct();

    protected:
        // DWARF cpp:238 -- declaration-only (its own ledger function); the value is
        // pinned by the EType table (E_MOMENT_STATIONARY_CRASH == 11).
        virtual EType GetInstanceType();

    private:
        // DWARF h:81-:88 (X360 offsets in comments; access BY NAME).
        Camera::BehaviourHandle<Camera::BehaviourIceAnim> mIceCameraHandle;   // h:81  +0x180
        const Parameters* mpParameters;                                        // h:83  +0x194
        bool              mbIsTumblingCrash;                                   // h:85  +0x198 (a full turn integrated while crashing)
        u32               muTake;                                              // h:86  +0x19C
        f32               mfTimeCrashing;                                      // h:87  +0x1A0
        f32               mfRotationAngle;                                     // h:88  +0x1A4 (the integrated crash rotation, radians)
    };
}
