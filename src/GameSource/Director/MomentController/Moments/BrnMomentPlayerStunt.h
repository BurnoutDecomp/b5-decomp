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
// Class shape / member names / method set verbatim from the declarations
// gated in the console build ledger. This TU
// bodies Construct/Update/Release/SetParameters/GetName/GetInstanceType;
// Prepare/Destruct and Parameters::Construct are their own ledger functions
// (declaration-only, declaration-gated).
//
namespace BrnDirector
{
    class MomentPlayerStunt : public Moment
    {
    public:
        //  The (member-less) tuning record.
        struct Parameters : public Moment::Parameters
        {
            //  Its own ledger function (declaration-only).
            void Construct();
        };

        // the inlined base Construct, the
        // ICE-cam handle clear, and the parameter/land-time resets.
        virtual void Construct();

        //  Its own ledger function (declaration-only).
        virtual bool Prepare(void* lrBehaviourController);

        // the per-frame stunt state
        // machine (see the .cpp).
        virtual void Update(f32 lfTimeStep, void* lrBehaviourController,
                            const void* lSharedInfo);

        // drop the ICE cam if held, clear
        // the gates, raise the searching head bit, back to SEARCHING. Returns true.
        virtual bool Release();

        // adopt the tuning record.
        virtual void SetParameters(const Moment::Parameters* lpParameters);

        virtual const char* GetName() const;

        //  Declaration-only (its own ledger function).
        virtual void Destruct();

    protected:
        // E_MOMENT_PLAYER_STUNT (8).
        virtual EType GetInstanceType();

    private:
        //  (console offsets in comments; access BY NAME).
        Camera::BehaviourHandle<Camera::BehaviourIceAnim> mIceCam;   // +0x180
        const Parameters* mpParameters;                              // +0x194
        f32  mfLandTime;                                             // +0x198 (integrates while grounded)
        f32  mfTimeInState;                                          // +0x19C
        bool mbStoppedEffect;                                        // +0x1A0 (the two-phase release latch)
        bool mbFirstTimeForThisStunt;                                // +0x1A1 (the stunt-flag bit-1 latch)
        bool mbIsCrashStunt;                                         // +0x1A2 (read-only in this TU's bodies)
        bool mbHasCrashed;                                           // +0x1A3 (latched once the abort trio fires)
    };
}
