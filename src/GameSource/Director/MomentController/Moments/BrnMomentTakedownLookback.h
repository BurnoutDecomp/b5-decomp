#pragma once

#include "types.hpp"
#include "GameSource/Director/MomentController/BrnMoment.h"              // BrnDirector::Moment (base)
#include "GameSource/Director/Camera/BrnBehaviourManager.h"             // Camera::BehaviourHandle<T> / BehaviourManager
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"         // Camera::BehaviourRig (+ its Parameters, held by value)

// BrnDirector::MomentTakedownLookback (DWARF BrnMomentTakedownLookback.h:46) -- the "takedown look-back" camera
// moment: after the player takes a rival down, a rig camera on the player's car looks back at the wreck. The moment
// holds while the victim is behind the player and within 60 m; once it draws level or falls out of range the moment
// returns to SEARCHING. Member names and the method set are the DWARF's; every body is in the .cpp
// (Construct @0x8225EDB8, Update @0x822662F0, Release @0x8223AA08, GetName @0x821F75C0 and the four vtable one-liners).
//
// FLAG (host signature): Prepare / Update take the manager and the shared record type-erased (void* / const void*),
// the moment family's convention in this tree; the DWARF declares Prepare(BehaviourManager&) and
// Update(float32_t, BehaviourManager&, const MomentSharedInfo&). The body undoes the erasure once, at its head.
namespace BrnDirector
{
    class MomentTakedownLookback : public Moment
    {
    public:
        // DWARF h:96 -- the (member-less) tuning record.
        struct Parameters : public Moment::Parameters
        {
            void Construct();   // DWARF h:100; no caller in this tree
        };

        virtual void Construct();                                   // cpp:34
        virtual bool Prepare(void* lrBehaviourController);          // cpp:55
        virtual void Update(f32 lfTimeStep, void* lrBehaviourController,
                            const void* lSharedInfo);               // cpp:72
        virtual bool Release();                                     // cpp:171
        virtual const char* GetName() const;                        // cpp:229
        virtual void SetParameters(const Moment::Parameters* lpParameters);   // cpp:203
        virtual void Destruct();                                    // cpp:189

    protected:
        virtual EType GetInstanceType();                            // cpp:215 -- E_MOMENT_TAKEDOWN_LOOKBACK (3)

    private:
        // DWARF h:81..h:86 (console offsets in comments; access BY NAME).
        const Parameters*                    mpParameters;         // +0x180
        Camera::BehaviourRig::Parameters     mLookbackRigParams;   // +0x190 (0x120 bytes)
        Camera::BehaviourHandle<Camera::BehaviourRig> mRigCameraHandle;  // +0x2B0 (5-word handle)
        Moment::VehicleRef                   mVictim;              // +0x2C4 (the taken-down car)
    };
}
