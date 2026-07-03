#pragma once

#include "types.hpp"
#include "GameSource/Director/MomentController/BrnMoment.h"              // BrnDirector::Moment (base)
#include "GameSource/Director/Camera/BrnBehaviourManager.h"              // Camera::BehaviourHandle<T>
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.h"   // Camera::BehaviourIceAnim

// BrnDirector::MomentPlayerStunt - the "player stunt" camera moment: while the
// player performs a stunt (jump / crash stunt), play the world-signature ICE
// take the stunt system stages, flashing the 2d post-FX on non-first frames,
// raising the Jump_Effect camera hook on a fresh stunt's opening frames, and
// swapping to the newly staged take mid-flight when the stunt chain continues.
// Class shape / member names / method set verbatim from the DecFIGS DWARF
// (BrnMomentPlayerStunt.h:46/:81-:90/:100); gated on the X360 ledger. This TU
// bodies Construct/Update/Release/SetParameters/GetName/GetInstanceType;
// Prepare/Destruct and Parameters::Construct are their own ledger functions
// (declaration-only, DWARF-gated).
//
// NOTE (mutual exclusion, the MomentHitTraffic precedent): BrnMomentSubclasses.h
// carries a layout-stubbed MomentPlayerStunt for the NewMoment TU; this real
// home and that stub define the same class and CANNOT be included into one TU.
namespace BrnDirector
{
    class MomentPlayerStunt : public Moment
    {
    public:
        // DWARF BrnMomentPlayerStunt.h:100 -- the (member-less) tuning record.
        struct Parameters : public Moment::Parameters
        {
            // DWARF h:104 -- its own ledger function (declaration-only).
            void Construct();
        };

        // @0x8225F310 (this TU, DWARF cpp:35) -- the inlined base Construct, the
        // ICE-cam handle clear, and the parameter/land-time resets.
        virtual void Construct();

        // DWARF cpp:54 -- its own ledger function (declaration-only).
        virtual bool Prepare(void* lrBehaviourController);

        // @0x82272750 (this TU, DWARF cpp:70) -- the per-frame stunt state
        // machine (see the .cpp).
        virtual void Update(f32 lfTimeStep, void* lrBehaviourController,
                            const void* lSharedInfo);

        // @0x8223AF78 (this TU, DWARF cpp:288) -- drop the ICE cam if held, clear
        // the gates, raise the searching head bit, back to SEARCHING. Returns true.
        virtual bool Release();

        // @0x821F76A8 (this TU, DWARF cpp:320) -- adopt the tuning record.
        virtual void SetParameters(const Moment::Parameters* lpParameters);

        // @0x821F7648 (this TU, DWARF cpp:346).
        virtual const char* GetName() const;

        // DWARF cpp:306 -- declaration-only (its own ledger function).
        virtual void Destruct();

    protected:
        // @0x821F7640 (this TU, DWARF cpp:332) -- E_MOMENT_PLAYER_STUNT (8).
        virtual EType GetInstanceType();

    private:
        // DWARF h:81-:90 (X360 offsets in comments; access BY NAME).
        Camera::BehaviourHandle<Camera::BehaviourIceAnim> mIceCam;   // h:81  +0x180
        const Parameters* mpParameters;                              // h:83  +0x194
        f32  mfLandTime;                                             // h:85  +0x198 (integrates while grounded)
        f32  mfTimeInState;                                          // h:86  +0x19C
        bool mbStoppedEffect;                                        // h:87  +0x1A0 (the two-phase release latch)
        bool mbFirstTimeForThisStunt;                                // h:88  +0x1A1 (the stunt-flag bit-1 latch)
        bool mbIsCrashStunt;                                         // h:89  +0x1A2 (read-only in this TU's bodies)
        bool mbHasCrashed;                                           // h:90  +0x1A3 (latched once the abort trio fires)
    };
}
